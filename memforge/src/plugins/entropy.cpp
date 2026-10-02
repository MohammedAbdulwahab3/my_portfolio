// Per-page Shannon entropy. Pages above the threshold (packed, compressed or
// encrypted content - e.g. injected payloads, ransomware key material) are
// merged into contiguous regions by an ordered reducer, so a region that
// spans many chunks is still reported once.
#include <array>
#include <cmath>
#include <memory>

#include "memforge/plugin.hpp"
#include "plugins.hpp"

namespace mf {
namespace {

constexpr uint64_t kPage = 4096;

// c*log2(c) for c in [0, 4096].
const std::array<double, kPage + 1>& xlogx() {
  static const auto table = [] {
    std::array<double, kPage + 1> t{};
    for (size_t c = 1; c <= kPage; ++c) t[c] = c * std::log2(static_cast<double>(c));
    return t;
  }();
  return table;
}

class EntropyReducer : public Reducer {
 public:
  void push(Finding&& f, const Emit& emit) override {
    if (have_ && f.offset == cur_.offset + cur_.length) {
      cur_.length += f.length;
      sum_ += f.get("entropy")->d;
      max_ = std::max(max_, f.get("entropy")->d);
      ++pages_;
      return;
    }
    flush(emit);
    cur_ = std::move(f);
    sum_ = max_ = cur_.get("entropy")->d;
    pages_ = 1;
    have_ = true;
  }
  void flush(const Emit& emit) override {
    if (!have_) return;
    Finding out;
    out.offset = cur_.offset;
    out.length = cur_.length;
    out.plugin = cur_.plugin;
    out.kind = "high-entropy-region";
    out.num("pages", pages_).real("mean_entropy", sum_ / pages_).real("max_entropy", max_);
    emit(std::move(out));
    have_ = false;
  }

 private:
  bool have_ = false;
  Finding cur_;
  double sum_ = 0, max_ = 0;
  uint64_t pages_ = 0;
};

class EntropyPlugin : public Plugin {
 public:
  const char* name() const override { return "entropy"; }
  const char* description() const override {
    return "High-entropy page regions + zero-page census (entropy.threshold=7.2)";
  }
  void configure(const Options& o) override {
    threshold_ = o.get_double("entropy.threshold", 7.2);
    xlogx();
  }
  uint64_t halo() const override { return kPage; }

  void scan(const ScanContext&, const Chunk& c, Sink& sink) const override {
    const auto& tbl = xlogx();
    uint64_t first = (kPage - (c.start & (kPage - 1))) & (kPage - 1);
    uint64_t zero = 0, high = 0, pages = 0;
    for (uint64_t i = first; i < c.len && i + kPage <= c.avail; i += kPage) {
      const uint8_t* d = c.data + i;
      uint32_t h[256] = {};
      for (uint64_t k = 0; k < kPage; k += 4) {
        ++h[d[k]];
        ++h[d[k + 1]];
        ++h[d[k + 2]];
        ++h[d[k + 3]];
      }
      ++pages;
      if (h[0] == kPage) {
        ++zero;
        continue;
      }
      double s = 0;
      for (uint32_t v : h) s += tbl[v];
      double e = 12.0 - s / kPage;  // log2(4096) - (1/N) sum c log2 c
      if (e >= threshold_) {
        sink.emit(c.start + i, kPage, "high-entropy-page").real("entropy", e);
        ++high;
      }
    }
    sink.count("pages", pages);
    sink.count("zero_pages", zero);
    sink.count("high_entropy_pages", high);
  }

  std::unique_ptr<Reducer> make_reducer() const override { return std::make_unique<EntropyReducer>(); }

 private:
  double threshold_ = 7.2;
};

}  // namespace

std::unique_ptr<Plugin> make_entropy_plugin() { return std::make_unique<EntropyPlugin>(); }

}  // namespace mf
