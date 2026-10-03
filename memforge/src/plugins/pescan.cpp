// Carves PE (Windows executable / DLL / driver) headers. Mapped images start
// on a page boundary, so by default only page-aligned offsets are tested.
#include <memory>
#include <stdexcept>
#include <string>

#include "common.hpp"
#include "memforge/plugin.hpp"
#include "plugins.hpp"

namespace mf {
namespace {

using detail::load;

constexpr uint64_t kMaxLfanew = 0x1000;
constexpr uint64_t kMaxSections = 96;
constexpr uint64_t kMaxOptHeader = 0xF0;

const char* machine_name(uint16_t m) {
  switch (m) {
    case 0x014c: return "x86";
    case 0x8664: return "x64";
    case 0xaa64: return "arm64";
    case 0x01c4: return "arm";
    default: return nullptr;
  }
}

class PeScanPlugin : public Plugin {
 public:
  const char* name() const override { return "pescan"; }
  const char* description() const override {
    return "PE headers of in-memory executables, DLLs and drivers (pescan.align=4096)";
  }
  void configure(const Options& o) override {
    align_ = static_cast<uint64_t>(o.get_int("pescan.align", 4096));
    if (align_ == 0 || (align_ & (align_ - 1))) throw std::invalid_argument("pescan.align must be a power of two");
  }
  uint64_t halo() const override { return kMaxLfanew + 24 + kMaxOptHeader + kMaxSections * 40; }

  void scan(const ScanContext&, const Chunk& c, Sink& sink) const override {
    uint64_t first = (align_ - (c.start & (align_ - 1))) & (align_ - 1);
    uint64_t hits = 0;
    for (uint64_t i = first; i < c.len; i += align_) {
      const uint8_t* d = c.data + i;
      uint64_t avail = c.avail - i;
      if (avail < 0x40 || d[0] != 'M' || d[1] != 'Z') continue;
      uint32_t lfanew = load<uint32_t>(d + 0x3c);
      if (lfanew < 0x40 || lfanew > kMaxLfanew || lfanew + 24 + 2 > avail) continue;
      const uint8_t* nt = d + lfanew;
      if (load<uint32_t>(nt) != 0x00004550) continue;  // "PE\0\0"
      uint16_t machine = load<uint16_t>(nt + 4);
      const char* mname = machine_name(machine);
      uint16_t nsec = load<uint16_t>(nt + 6);
      uint32_t timestamp = load<uint32_t>(nt + 8);
      uint16_t opt_size = load<uint16_t>(nt + 20);
      uint16_t chars = load<uint16_t>(nt + 22);
      if (!mname || nsec == 0 || nsec > kMaxSections) continue;
      const uint8_t* opt = nt + 24;
      uint16_t magic = load<uint16_t>(opt);
      bool pe64 = magic == 0x20b;
      if (magic != 0x10b && !pe64) continue;
      // Through Subsystem; reject oversized optional headers so every read
      // stays inside the halo (keeps results independent of chunking).
      constexpr uint64_t kNeedOpt = 0x46;
      if (opt_size < kNeedOpt || opt_size > kMaxOptHeader || lfanew + 24 + kNeedOpt > avail) continue;
      uint32_t entry = load<uint32_t>(opt + 16);
      uint64_t image_base = pe64 ? load<uint64_t>(opt + 24) : load<uint32_t>(opt + 28);
      uint32_t size_of_image = load<uint32_t>(opt + 56);
      uint16_t subsystem = load<uint16_t>(opt + 68);
      if (size_of_image == 0) continue;

      const char* kind = (chars & 0x2000) ? "dll" : (subsystem == 1 ? "driver" : "exe");
      Finding& f = sink.emit(c.start + i, lfanew + 24 + opt_size, kind);
      f.str("format", pe64 ? "PE32+" : "PE32")
          .str("machine", mname)
          .num("sections", nsec)
          .hex("timestamp", timestamp)
          .hex("entry_rva", entry)
          .hex("image_base", image_base)
          .hex("size_of_image", size_of_image)
          .num("subsystem", subsystem);
      // Section table, if it is within reach.
      uint64_t sec_off = lfanew + 24 + opt_size;
      if (sec_off + nsec * 40ull <= avail) {
        std::string names;
        for (uint16_t s = 0; s < nsec; ++s) {
          const uint8_t* sh = d + sec_off + s * 40ull;
          std::string n;
          for (int k = 0; k < 8 && sh[k]; ++k) n += static_cast<char>(sh[k]);
          if (s) names += ',';
          names += n;
        }
        f.str("section_names", names);
        f.length = sec_off + nsec * 40ull;
      }
      ++hits;
    }
    sink.count("headers", hits);
  }

 private:
  uint64_t align_ = 4096;
};

}  // namespace

std::unique_ptr<Plugin> make_pescan_plugin() { return std::make_unique<PeScanPlugin>(); }

}  // namespace mf
