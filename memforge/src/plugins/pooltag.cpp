// Windows x64 kernel pool scanner (the technique behind Volatility's psscan,
// filescan, netscan...). Finds _POOL_HEADERs carrying object tags, which also
// reveals objects that were unlinked from kernel lists (DKOM-hidden processes)
// or already freed.
//
// x64 _POOL_HEADER (16 bytes, 16-byte aligned):
//   +0 PreviousSize:8  +1 PoolIndex:8  +2 BlockSize:8 (x16 bytes)  +3 PoolType:8
//   +4 PoolTag[4]      +8 ProcessBilled / AllocatorBackTraceIndex
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "common.hpp"
#include "memforge/plugin.hpp"
#include "plugins.hpp"

namespace mf {
namespace {

using detail::load;

struct Tag {
  uint32_t value;
  std::string tag;
  std::string kind;
  uint32_t min_size;
};

uint32_t tag_value(const std::string& t) {
  uint32_t v = 0;
  std::memcpy(&v, t.data(), 4);
  return v;
}

class PoolTagPlugin : public Plugin {
 public:
  const char* name() const override { return "pooltag"; }
  const char* description() const override {
    return "Windows x64 pool allocations by object tag: processes, threads, files, sockets... "
           "(pooltag.tags=ABCD,EFGH)";
  }
  void configure(const Options& o) override {
    tags_ = {
        {0, "Proc", "process", 0x300},     {0, "Thre", "thread", 0x300},
        {0, "File", "file", 0x90},         {0, "Driv", "driver", 0xf0},
        {0, "Muta", "mutant", 0x40},       {0, "TcpE", "tcp-endpoint", 0x100},
        {0, "TcpL", "tcp-listener", 0x80}, {0, "UdpA", "udp-endpoint", 0x80},
    };
    for (const auto& t : o.get_list("pooltag.tags")) {
      if (t.size() != 4) throw std::invalid_argument("pooltag.tags entries must be 4 characters: " + t);
      tags_.push_back({0, t, "custom", 0x20});
    }
    for (auto& t : tags_) t.value = tag_value(t.tag);
  }
  uint64_t halo() const override { return 255 * 16; }

  void scan(const ScanContext&, const Chunk& c, Sink& sink) const override {
    uint64_t first = (16 - (c.start & 15)) & 15;
    uint64_t hits = 0;
    for (uint64_t i = first; i + 16 <= c.avail && i < c.len; i += 16) {
      const uint8_t* h = c.data + i;
      uint32_t tag = load<uint32_t>(h + 4) & 0x7fffffffu;  // drop "protected" bit
      const Tag* t = nullptr;
      for (const auto& cand : tags_)
        if (cand.value == tag) {
          t = &cand;
          break;
        }
      if (!t) continue;
      uint32_t block = h[2] * 16u;
      uint8_t pool_type = h[3];
      if (block < t->min_size || pool_type == 0 || pool_type > 0x7f) continue;
      Finding& f = sink.emit(c.start + i, block, t->kind);
      f.str("tag", t->tag).hex("block_size", block).hex("pool_type", pool_type);
      if (t->kind == "process") {
        std::string name = guess_image_name(c, i, block);
        if (!name.empty()) f.str("name_guess", name);
      }
      ++hits;
    }
    sink.count("allocations", hits);
  }

 private:
  // _EPROCESS.ImageFileName is a 15-byte NUL-padded ANSI name whose offset
  // differs per build. Without symbols, take the first NUL-delimited run of
  // 3..15 filename characters ending in ".exe" inside the allocation.
  static std::string guess_image_name(const Chunk& c, uint64_t i, uint32_t block) {
    uint64_t end = std::min<uint64_t>(c.avail, i + block);
    for (uint64_t p = i + 16; p + 4 < end; ++p) {
      if (std::memcmp(c.data + p, ".exe", 4) != 0) continue;
      uint64_t s = p;
      while (s > i + 16 && p + 4 - s < 15) {
        unsigned char ch = c.data[s - 1];
        if (ch < 0x21 || ch >= 0x7f) break;
        --s;
      }
      if (c.data[s - 1] != 0 || p + 4 >= end || c.data[p + 4] != 0) continue;
      if (p + 4 - s < 5) continue;  // need at least one char before ".exe"
      return std::string(reinterpret_cast<const char*>(c.data + s), p + 4 - s);
    }
    return {};
  }

  std::vector<Tag> tags_;
};

}  // namespace

std::unique_ptr<Plugin> make_pooltag_plugin() { return std::make_unique<PoolTagPlugin>(); }

}  // namespace mf
