#include "memforge/engine.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <exception>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>

#include <sys/stat.h>

#ifdef MEMFORGE_HAVE_OPENMP
#include <omp.h>
#endif

namespace mf {

namespace {
using Clock = std::chrono::steady_clock;
double seconds_since(Clock::time_point t0) {
  return std::chrono::duration<double>(Clock::now() - t0).count();
}
constexpr uint64_t kPage = 4096;
}  // namespace

unsigned resolve_threads(unsigned requested) {
  if (requested) return requested;
  unsigned hw = std::thread::hardware_concurrency();
  return hw ? hw : 1;
}

const char* parallel_backend() {
#ifdef MEMFORGE_HAVE_OPENMP
  return "openmp";
#else
  return "std::thread";
#endif
}

void parallel_for(size_t n, unsigned threads, const std::function<void(size_t)>& fn) {
  threads = resolve_threads(threads);
  if (n == 0) return;
  std::exception_ptr err;
  std::mutex err_mu;
  auto guarded = [&](size_t i) {
    try {
      fn(i);
    } catch (...) {
      std::lock_guard<std::mutex> lk(err_mu);
      if (!err) err = std::current_exception();
    }
  };
  if (threads == 1 || n == 1) {
    for (size_t i = 0; i < n; ++i) guarded(i);
  } else {
#ifdef MEMFORGE_HAVE_OPENMP
    const long long nn = static_cast<long long>(n);
#pragma omp parallel for schedule(dynamic, 1) num_threads(threads)
    for (long long i = 0; i < nn; ++i) guarded(static_cast<size_t>(i));
#else
    std::atomic<size_t> next{0};
    std::vector<std::thread> pool;
    unsigned t = static_cast<unsigned>(std::min<size_t>(threads, n));
    pool.reserve(t);
    for (unsigned k = 0; k < t; ++k)
      pool.emplace_back([&] {
        for (size_t i; (i = next.fetch_add(1, std::memory_order_relaxed)) < n;) guarded(i);
      });
    for (auto& th : pool) th.join();
#endif
  }
  if (err) std::rethrow_exception(err);
}

uint64_t max_halo(const PluginSet& plugins) {
  uint64_t h = 0;
  for (const Plugin* p : plugins) h = std::max(h, p->halo());
  return h;
}

std::vector<ChunkSpec> plan_chunks(const MemoryImage& image, uint64_t chunk_size, uint64_t halo) {
  if (chunk_size == 0) throw std::invalid_argument("chunk size must be > 0");
  chunk_size = (chunk_size + kPage - 1) / kPage * kPage;
  std::vector<ChunkSpec> out;
  for (const MemRange& r : image.ranges()) {
    for (uint64_t off = 0; off < r.size; off += chunk_size) {
      ChunkSpec c;
      c.start = r.phys_start + off;
      c.len = std::min(chunk_size, r.size - off);
      c.avail = std::min(c.len + halo, r.size - off);
      c.behind = off;
      out.push_back(c);
    }
  }
  return out;
}

ChunkResult scan_chunk(const MemoryImage& image, const PluginSet& plugins, const ChunkSpec& spec,
                       uint64_t index) {
  ChunkResult res;
  res.plugin_seconds.assign(plugins.size(), 0.0);
  res.bytes = spec.len;
  Chunk c;
  c.index = index;
  c.start = spec.start;
  c.len = spec.len;
  c.avail = spec.avail;
  c.behind = spec.behind;
  c.data = image.ptr(spec.start, spec.avail);
  if (!c.data) throw std::logic_error("chunk not backed by a single range");
  ScanContext ctx{image};
  for (size_t i = 0; i < plugins.size(); ++i) {
    std::string prefix = std::string(plugins[i]->name()) + ".";
    Sink sink(res.findings, res.counters, static_cast<uint16_t>(i), prefix);
    auto t0 = Clock::now();
    plugins[i]->scan(ctx, c, sink);
    res.plugin_seconds[i] = seconds_since(t0);
  }
  std::sort(res.findings.begin(), res.findings.end(), finding_less);
  return res;
}

// Wire format ---------------------------------------------------------------
void encode_chunk_result(ByteWriter& w, const ChunkResult& r) {
  w.u64(r.findings.size());
  for (const auto& f : r.findings) encode_finding(w, f);
  encode_counters(w, r.counters);
  w.u64(r.plugin_seconds.size());
  for (double d : r.plugin_seconds) w.f64(d);
  w.u64(r.bytes);
  w.u64(r.blocks.size());
  for (const auto& b : r.blocks) w.str(b);
  w.u64(r.rendered.size());
  for (uint64_t n : r.rendered) w.u64(n);
}

ChunkResult decode_chunk_result(ByteReader& rd) {
  ChunkResult r;
  r.findings.resize(rd.u64());
  for (auto& f : r.findings) f = decode_finding(rd);
  r.counters = decode_counters(rd);
  r.plugin_seconds.resize(rd.u64());
  for (auto& d : r.plugin_seconds) d = rd.f64();
  r.bytes = rd.u64();
  r.blocks.resize(rd.u64());
  for (auto& b : r.blocks) b = rd.str();
  r.rendered.resize(rd.u64());
  for (auto& n : r.rendered) n = rd.u64();
  return r;
}

// Rendering and writers -------------------------------------------------------
Renderer::Renderer(std::vector<std::string> plugin_names, std::string image_label,
                   bool per_plugin_blocks)
    : names_(std::move(plugin_names)), image_(std::move(image_label)), per_plugin_(per_plugin_blocks) {}

void Renderer::render(const Finding& f, std::string& out) const {
  out += finding_to_json(f, names_.at(f.plugin), image_);
  out += '\n';
}

std::vector<bool> reducer_mask(const PluginSet& plugins) {
  std::vector<bool> m;
  for (const Plugin* p : plugins) m.push_back(p->make_reducer() != nullptr);
  return m;
}

void prerender(ChunkResult& r, const Renderer* renderer, const std::vector<bool>& has_reducer) {
  r.blocks.assign(renderer ? renderer->nblocks() : 0, std::string());
  r.rendered.assign(has_reducer.size(), 0);
  std::vector<Finding> keep;
  for (auto& f : r.findings) {
    if (has_reducer[f.plugin]) {
      keep.push_back(std::move(f));
    } else {
      if (renderer) renderer->render(f, r.blocks[renderer->block_of(f)]);
      ++r.rendered[f.plugin];
    }
  }
  r.findings = std::move(keep);
}

JsonlWriter::JsonlWriter(FILE* out, std::vector<std::string> plugin_names, std::string image_label,
                         bool owns_file)
    : out_(out), renderer_(std::move(plugin_names), std::move(image_label), false), owns_(owns_file) {}

JsonlWriter::~JsonlWriter() {
  flush();
  if (owns_ && out_) std::fclose(out_);
}

void JsonlWriter::write(const Finding& f) {
  renderer_.render(f, buf_);
  if (buf_.size() >= (1u << 20)) flush();
}

void JsonlWriter::write_block(size_t, const std::string& bytes) {
  if (bytes.size() >= (1u << 20)) {
    flush();
    if (out_) std::fwrite(bytes.data(), 1, bytes.size(), out_);
    return;
  }
  buf_ += bytes;
  if (buf_.size() >= (1u << 20)) flush();
}

void JsonlWriter::flush() {
  if (!buf_.empty() && out_) std::fwrite(buf_.data(), 1, buf_.size(), out_);
  buf_.clear();
}

void JsonlWriter::finish() {
  flush();
  if (out_) std::fflush(out_);
}

PerPluginWriter::PerPluginWriter(const std::string& dir, const std::vector<std::string>& names)
    : renderer_(names, "", true) {
  for (const auto& n : names) {
    std::string path = dir + "/" + n + ".jsonl";
    FILE* f = std::fopen(path.c_str(), "w");
    if (!f) throw std::runtime_error("cannot create " + path + ": " + std::strerror(errno));
    writers_.push_back(std::make_unique<JsonlWriter>(f, names, "", true));
  }
}

void PerPluginWriter::write(const Finding& f) { writers_.at(f.plugin)->write(f); }

void PerPluginWriter::write_block(size_t block, const std::string& bytes) {
  writers_.at(block)->write_block(0, bytes);
}

void PerPluginWriter::finish() {
  for (auto& w : writers_) w->finish();
}

// Pipeline ------------------------------------------------------------------
Pipeline::Pipeline(const PluginSet& plugins, FindingWriter* writer)
    : plugins_(plugins), writer_(writer) {
  findings.assign(plugins.size(), 0);
  plugin_seconds.assign(plugins.size(), 0.0);
  for (const Plugin* p : plugins) reducers_.push_back(p->make_reducer());
  emit_ = [this](Finding&& f) { write(std::move(f)); };
}

void Pipeline::write(Finding&& f) {
  ++findings[f.plugin];
  if (writer_) writer_->write(f);
}

void Pipeline::consume(ChunkResult&& r) {
  for (size_t b = 0; b < r.blocks.size(); ++b)
    if (!r.blocks[b].empty() && writer_) writer_->write_block(b, r.blocks[b]);
  for (size_t p = 0; p < r.rendered.size() && p < findings.size(); ++p) findings[p] += r.rendered[p];
  for (auto& f : r.findings) {
    Reducer* red = reducers_[f.plugin].get();
    if (red)
      red->push(std::move(f), emit_);
    else
      write(std::move(f));
  }
  merge_counters(counters, r.counters);
  for (size_t i = 0; i < plugin_seconds.size() && i < r.plugin_seconds.size(); ++i)
    plugin_seconds[i] += r.plugin_seconds[i];
  bytes += r.bytes;
  ++chunks;
}

void Pipeline::finish() {
  for (auto& red : reducers_)
    if (red) red->flush(emit_);
  if (writer_) writer_->finish();
}

// Reports -------------------------------------------------------------------
std::string report_to_json(const ImageReport& r) {
  std::string o = "{\"image\":" + json_quote(r.image);
  o += ",\"format\":" + json_quote(r.format);
  if (!r.error.empty()) o += ",\"error\":" + json_quote(r.error);
  o += ",\"file_size\":" + std::to_string(r.file_size);
  o += ",\"phys_bytes\":" + std::to_string(r.phys_bytes);
  o += ",\"chunks\":" + std::to_string(r.chunks);
  o += ",\"threads\":" + std::to_string(r.threads);
  if (r.rank >= 0) o += ",\"rank\":" + std::to_string(r.rank);
  char b[64];
  std::snprintf(b, sizeof b, "%.6f", r.wall_seconds);
  o += ",\"wall_seconds\":";
  o += b;
  double gbps = r.wall_seconds > 0 ? r.phys_bytes / r.wall_seconds / 1e9 : 0;
  std::snprintf(b, sizeof b, "%.4f", gbps);
  o += ",\"throughput_gbps\":";
  o += b;
  o += ",\"plugins\":{";
  for (size_t i = 0; i < r.plugins.size(); ++i) {
    if (i) o += ',';
    std::snprintf(b, sizeof b, "%.6f", i < r.plugin_seconds.size() ? r.plugin_seconds[i] : 0.0);
    o += json_quote(r.plugins[i]) + ":{\"findings\":" +
         std::to_string(i < r.findings.size() ? r.findings[i] : 0) + ",\"cpu_seconds\":" + b + "}";
  }
  o += "},\"counters\":{";
  bool first = true;
  for (const auto& [k, v] : r.counters) {
    if (!first) o += ',';
    first = false;
    o += json_quote(k) + ":" + std::to_string(v);
  }
  o += "}}";
  return o;
}

void encode_report(ByteWriter& w, const ImageReport& r) {
  w.str(r.image);
  w.str(r.format);
  w.str(r.error);
  w.u64(r.file_size);
  w.u64(r.phys_bytes);
  w.u64(r.chunks);
  w.u64(r.threads);
  w.u64(static_cast<uint64_t>(static_cast<int64_t>(r.rank)));
  w.f64(r.wall_seconds);
  w.u64(r.plugins.size());
  for (size_t i = 0; i < r.plugins.size(); ++i) {
    w.str(r.plugins[i]);
    w.u64(i < r.findings.size() ? r.findings[i] : 0);
    w.f64(i < r.plugin_seconds.size() ? r.plugin_seconds[i] : 0);
  }
  encode_counters(w, r.counters);
}

ImageReport decode_report(ByteReader& rd) {
  ImageReport r;
  r.image = rd.str();
  r.format = rd.str();
  r.error = rd.str();
  r.file_size = rd.u64();
  r.phys_bytes = rd.u64();
  r.chunks = rd.u64();
  r.threads = static_cast<unsigned>(rd.u64());
  r.rank = static_cast<int>(static_cast<int64_t>(rd.u64()));
  r.wall_seconds = rd.f64();
  uint64_t n = rd.u64();
  for (uint64_t i = 0; i < n; ++i) {
    r.plugins.push_back(rd.str());
    r.findings.push_back(rd.u64());
    r.plugin_seconds.push_back(rd.f64());
  }
  r.counters = decode_counters(rd);
  return r;
}

// Scheduling ----------------------------------------------------------------
namespace {

// Releases chunk results to a consumer strictly in chunk order.
class ReorderBuffer {
 public:
  ReorderBuffer(size_t n, std::function<void(ChunkResult&&)> sink)
      : slots_(n), sink_(std::move(sink)) {}

  // Returns true when this call released the final chunk.
  bool complete(size_t idx, ChunkResult&& r) {
    std::lock_guard<std::mutex> lk(mu_);
    slots_[idx] = std::move(r);
    while (next_ < slots_.size() && slots_[next_]) {
      sink_(std::move(*slots_[next_]));
      slots_[next_].reset();
      ++next_;
    }
    return next_ == slots_.size();
  }

 private:
  std::mutex mu_;
  std::vector<std::optional<ChunkResult>> slots_;
  size_t next_ = 0;
  std::function<void(ChunkResult&&)> sink_;
};

std::vector<std::string> names_of(const PluginSet& plugins) {
  std::vector<std::string> n;
  for (const Plugin* p : plugins) n.emplace_back(p->name());
  return n;
}

}  // namespace

std::vector<ImageReport> scan_images(const std::vector<ImageTask>& tasks, const PluginSet& plugins,
                                     const EngineConfig& cfg) {
  const unsigned threads = resolve_threads(cfg.threads);
  const uint64_t halo = max_halo(plugins);
  const auto t0 = Clock::now();

  struct State {
    std::vector<ChunkSpec> chunks;
    std::unique_ptr<Pipeline> pipeline;
    std::unique_ptr<ReorderBuffer> reorder;
    double done_at = 0;
  };
  std::vector<State> st(tasks.size());
  struct Task {
    uint32_t image;
    uint32_t chunk;
  };
  std::vector<Task> flat;
  const std::vector<bool> has_reducer = reducer_mask(plugins);

  for (size_t i = 0; i < tasks.size(); ++i) {
    State& s = st[i];
    tasks[i].image->advise_sequential();
    s.chunks = plan_chunks(*tasks[i].image, cfg.chunk_size, halo);
    s.pipeline = std::make_unique<Pipeline>(plugins, tasks[i].writer);
    Pipeline* pl = s.pipeline.get();
    s.reorder = std::make_unique<ReorderBuffer>(
        s.chunks.size(), [pl](ChunkResult&& r) { pl->consume(std::move(r)); });
    for (size_t c = 0; c < s.chunks.size(); ++c)
      flat.push_back({static_cast<uint32_t>(i), static_cast<uint32_t>(c)});
  }

  parallel_for(flat.size(), threads, [&](size_t k) {
    const Task& t = flat[k];
    State& s = st[t.image];
    ChunkResult r = scan_chunk(*tasks[t.image].image, plugins, s.chunks[t.chunk], t.chunk);
    if (FindingWriter* w = tasks[t.image].writer)
      if (const Renderer* rd = w->renderer()) prerender(r, rd, has_reducer);
    if (s.reorder->complete(t.chunk, std::move(r))) s.done_at = seconds_since(t0);
  });

  std::vector<ImageReport> reports;
  for (size_t i = 0; i < tasks.size(); ++i) {
    State& s = st[i];
    s.pipeline->finish();
    const MemoryImage& img = *tasks[i].image;
    ImageReport r;
    r.image = img.path();
    r.format = format_name(img.format());
    r.file_size = img.file_size();
    r.phys_bytes = img.total_bytes();
    r.chunks = s.chunks.size();
    r.threads = threads;
    r.wall_seconds = s.chunks.empty() ? 0 : s.done_at;
    r.plugins = names_of(plugins);
    r.findings = s.pipeline->findings;
    r.plugin_seconds = s.pipeline->plugin_seconds;
    r.counters = s.pipeline->counters;
    reports.push_back(std::move(r));
  }
  return reports;
}

std::vector<ChunkResult> scan_chunk_range(const MemoryImage& image, const PluginSet& plugins,
                                          const std::vector<ChunkSpec>& chunks, size_t first,
                                          size_t last, unsigned threads,
                                          const Renderer* renderer, bool count_only) {
  last = std::min(last, chunks.size());
  if (first >= last) return {};
  std::vector<ChunkResult> out(last - first);
  const std::vector<bool> has_reducer = reducer_mask(plugins);
  parallel_for(last - first, threads, [&](size_t k) {
    out[k] = scan_chunk(image, plugins, chunks[first + k], first + k);
    if (renderer || count_only) prerender(out[k], count_only ? nullptr : renderer, has_reducer);
  });
  return out;
}

}  // namespace mf
