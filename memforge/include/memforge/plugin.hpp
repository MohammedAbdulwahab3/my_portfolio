// Plugin interface.
//
// The engine cuts physical memory into chunks and hands each one to every
// active plugin, on many threads at once. To get results that are *exactly*
// those of a single serial pass, plugins follow one ownership rule:
//
//   A finding belongs to the chunk that contains its first byte.
//
// So a plugin reports only findings whose start lies in [0, chunk.len). It
// may read up to chunk.avail bytes forward (the "halo", sized from halo())
// to finish a match that crosses the chunk end, and up to chunk.behind bytes
// backward to check whether a match really starts here or is the tail of one
// owned by the previous chunk. Chunks never cross MemRange boundaries, since
// bytes on either side of a gap in physical memory are not adjacent.
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "memforge/finding.hpp"
#include "memforge/image.hpp"
#include "memforge/options.hpp"

namespace mf {

struct Chunk {
  uint64_t index = 0;
  uint64_t start = 0;   // physical address of data[0]
  uint64_t len = 0;     // owned bytes
  uint64_t avail = 0;   // readable bytes from data[0] (len + halo, clipped to range end)
  uint64_t behind = 0;  // readable bytes before data[0] (to the range start)
  const uint8_t* data = nullptr;

  // Byte at a chunk-relative position, -1 when outside the readable window.
  int at(int64_t rel) const {
    if (rel < 0 ? static_cast<uint64_t>(-rel) > behind : static_cast<uint64_t>(rel) >= avail)
      return -1;
    return data[rel];
  }
};

class Sink {
 public:
  Sink(std::vector<Finding>& out, Counters& counters, uint16_t plugin, const std::string& prefix)
      : out_(out), counters_(counters), plugin_(plugin), prefix_(prefix) {}

  Finding& emit(uint64_t offset, uint64_t length, std::string kind) {
    out_.emplace_back();
    Finding& f = out_.back();
    f.offset = offset;
    f.length = length;
    f.plugin = plugin_;
    f.kind = std::move(kind);
    return f;
  }
  void count(const std::string& key, uint64_t n = 1) { counters_[prefix_ + key] += n; }

 private:
  std::vector<Finding>& out_;
  Counters& counters_;
  uint16_t plugin_;
  const std::string& prefix_;
};

struct ScanContext {
  const MemoryImage& image;
};

using Emit = std::function<void(Finding&&)>;

// Optional ordered post-pass that sees one plugin's findings in address
// order (e.g. to merge adjacent per-page hits into regions). Runs serially.
class Reducer {
 public:
  virtual ~Reducer() = default;
  virtual void push(Finding&& f, const Emit& emit) = 0;
  virtual void flush(const Emit& emit) = 0;
};

class Plugin {
 public:
  virtual ~Plugin() = default;
  virtual const char* name() const = 0;
  virtual const char* description() const = 0;
  // Reads "<name>.*" options. Called once, before any scan.
  virtual void configure(const Options&) {}
  // Max bytes a finding may extend past its first byte.
  virtual uint64_t halo() const = 0;
  // Must be thread-safe: called concurrently on different chunks.
  virtual void scan(const ScanContext& ctx, const Chunk& chunk, Sink& sink) const = 0;
  virtual std::unique_ptr<Reducer> make_reducer() const { return nullptr; }
};

struct PluginInfo {
  std::string name;
  std::string description;
};

std::vector<PluginInfo> available_plugins();
// names may contain "all". Throws on unknown names.
std::vector<std::unique_ptr<Plugin>> create_plugins(const std::vector<std::string>& names,
                                                    const Options& opts);

}  // namespace mf
