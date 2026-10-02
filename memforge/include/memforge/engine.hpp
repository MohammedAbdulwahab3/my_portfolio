// Parallel scan engine.
//
// Work decomposition: every image is cut into chunks (default 16 MiB) and a
// flat task list of (image, chunk) pairs is executed by a thread team with
// dynamic scheduling. Each task runs *all* active plugins over its chunk while
// it is hot in cache (one pass over memory regardless of plugin count).
//
// Results flow through a per-image reorder buffer that releases chunk results
// strictly in address order, so output is byte-identical for any thread
// count, chunk size, or rank count, and memory stays bounded by the number of
// in-flight chunks rather than the size of the image.
#pragma once

#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "memforge/finding.hpp"
#include "memforge/image.hpp"
#include "memforge/plugin.hpp"

namespace mf {

using PluginSet = std::vector<const Plugin*>;

struct EngineConfig {
  unsigned threads = 0;              // 0 = hardware concurrency
  uint64_t chunk_size = 16ull << 20;  // rounded up to a page multiple
};

unsigned resolve_threads(unsigned requested);
const char* parallel_backend();  // "openmp" or "std::thread"

// Runs fn(i) for i in [0, n) on `threads` workers, dynamically scheduled in
// index order. The first exception thrown by fn is rethrown after the join.
void parallel_for(size_t n, unsigned threads, const std::function<void(size_t)>& fn);

struct ChunkSpec {
  uint64_t start = 0;
  uint64_t len = 0;
  uint64_t avail = 0;
  uint64_t behind = 0;
};

uint64_t max_halo(const PluginSet& plugins);
std::vector<ChunkSpec> plan_chunks(const MemoryImage& image, uint64_t chunk_size, uint64_t halo);

struct ChunkResult {
  std::vector<Finding> findings;  // sorted with finding_less
  Counters counters;
  std::vector<double> plugin_seconds;
  uint64_t bytes = 0;
  // Filled by prerender(): output bytes per writer block, rendered on the
  // worker thread, and how many findings of each plugin they contain.
  std::vector<std::string> blocks;
  std::vector<uint64_t> rendered;
};

ChunkResult scan_chunk(const MemoryImage& image, const PluginSet& plugins, const ChunkSpec& spec,
                       uint64_t index);

void encode_chunk_result(ByteWriter& w, const ChunkResult& r);
ChunkResult decode_chunk_result(ByteReader& rd);

// Output -------------------------------------------------------------------
// Turns findings into JSONL text. Thread-safe (const), so formatting runs on
// the scan workers instead of serializing on the ordered output path.
class Renderer {
 public:
  Renderer(std::vector<std::string> plugin_names, std::string image_label, bool per_plugin_blocks);
  size_t nblocks() const { return per_plugin_ ? names_.size() : 1; }
  size_t block_of(const Finding& f) const { return per_plugin_ ? f.plugin : 0; }
  void render(const Finding& f, std::string& out) const;
  const std::vector<std::string>& names() const { return names_; }

 private:
  std::vector<std::string> names_;
  std::string image_;
  bool per_plugin_;
};

class FindingWriter {
 public:
  virtual ~FindingWriter() = default;
  virtual void write(const Finding& f) = 0;
  // Appends pre-rendered bytes for one block (see Renderer).
  virtual void write_block(size_t /*block*/, const std::string& /*bytes*/) {}
  // Non-null enables parallel pre-rendering on the workers.
  virtual const Renderer* renderer() const { return nullptr; }
  virtual void finish() {}
};

// Moves findings of plugins without a reducer into rendered blocks. With a
// null renderer they are only counted and dropped (count-only runs).
void prerender(ChunkResult& r, const Renderer* renderer, const std::vector<bool>& has_reducer);
std::vector<bool> reducer_mask(const PluginSet& plugins);

// One JSON object per line, all plugins interleaved in address order.
// Buffered; whole lines go out per fwrite, so several writers may share one
// FILE* (e.g. stdout).
class JsonlWriter : public FindingWriter {
 public:
  JsonlWriter(FILE* out, std::vector<std::string> plugin_names, std::string image_label,
              bool owns_file = false);
  ~JsonlWriter() override;
  void write(const Finding& f) override;
  void write_block(size_t block, const std::string& bytes) override;
  const Renderer* renderer() const override { return &renderer_; }
  void finish() override;

 private:
  void flush();
  FILE* out_;
  Renderer renderer_;
  bool owns_;
  std::string buf_;
};

// <dir>/<plugin>.jsonl for each plugin.
class PerPluginWriter : public FindingWriter {
 public:
  PerPluginWriter(const std::string& dir, const std::vector<std::string>& plugin_names);
  void write(const Finding& f) override;
  void write_block(size_t block, const std::string& bytes) override;
  const Renderer* renderer() const override { return &renderer_; }
  void finish() override;

 private:
  Renderer renderer_;
  std::vector<std::unique_ptr<JsonlWriter>> writers_;
};

class VectorWriter : public FindingWriter {
 public:
  void write(const Finding& f) override { findings.push_back(f); }
  std::vector<Finding> findings;
};

class NullWriter : public FindingWriter {
 public:
  void write(const Finding&) override {}
};

// Ordered consumer: applies plugin reducers, forwards to a writer, and keeps
// statistics. Not thread-safe; the engine feeds it serially in chunk order.
class Pipeline {
 public:
  Pipeline(const PluginSet& plugins, FindingWriter* writer);
  void consume(ChunkResult&& r);
  void finish();

  std::vector<uint64_t> findings;  // per plugin, after reduction
  std::vector<double> plugin_seconds;
  Counters counters;
  uint64_t bytes = 0;
  uint64_t chunks = 0;

 private:
  void write(Finding&& f);
  const PluginSet& plugins_;
  FindingWriter* writer_;
  std::vector<std::unique_ptr<Reducer>> reducers_;
  Emit emit_;
};

struct ImageReport {
  std::string image;
  std::string format;
  std::string error;  // non-empty when the image could not be scanned
  uint64_t file_size = 0;
  uint64_t phys_bytes = 0;
  uint64_t chunks = 0;
  unsigned threads = 0;
  int rank = -1;
  double wall_seconds = 0;
  std::vector<std::string> plugins;
  std::vector<uint64_t> findings;
  std::vector<double> plugin_seconds;
  Counters counters;
};

std::string report_to_json(const ImageReport& r);
ImageReport decode_report(ByteReader& rd);
void encode_report(ByteWriter& w, const ImageReport& r);

struct ImageTask {
  const MemoryImage* image;
  FindingWriter* writer;
};

// Scans several images with one shared thread team (tasks are flattened
// across images, so thousands of small images keep every core busy).
std::vector<ImageReport> scan_images(const std::vector<ImageTask>& tasks, const PluginSet& plugins,
                                     const EngineConfig& cfg);

// Scans a subset of one image's chunks [first, last) and returns their
// results in order (used by the MPI split mode).
std::vector<ChunkResult> scan_chunk_range(const MemoryImage& image, const PluginSet& plugins,
                                          const std::vector<ChunkSpec>& chunks, size_t first,
                                          size_t last, unsigned threads,
                                          const Renderer* renderer = nullptr,
                                          bool count_only = false);

}  // namespace mf
