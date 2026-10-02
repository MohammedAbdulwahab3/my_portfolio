// Finds x64 page-table roots (DTB / CR3 values) without symbols, using the
// self-referencing PML4 entry that Windows installs in every address space
// (randomized slot since Windows 10 1607). Each candidate is verified by a
// full 4-level walk: the self-map VA must translate back to the PML4 itself.
// The DTB is the key that unlocks virtual-address analysis of the image.
#include <memory>

#include "common.hpp"
#include "memforge/addrspace.hpp"
#include "memforge/plugin.hpp"
#include "plugins.hpp"

namespace mf {
namespace {

using detail::load;
constexpr uint64_t kPage = 4096;

class DtbScanPlugin : public Plugin {
 public:
  const char* name() const override { return "dtbscan"; }
  const char* description() const override {
    return "x64 page-table roots (DTB/CR3) via the verified PML4 self-map (dtbscan.min_kernel=4)";
  }
  void configure(const Options& o) override {
    min_kernel_ = static_cast<int>(o.get_int("dtbscan.min_kernel", 4));
  }
  uint64_t halo() const override { return kPage; }

  void scan(const ScanContext& ctx, const Chunk& c, Sink& sink) const override {
    const uint64_t max_phys = ctx.image.max_phys();
    uint64_t first = (kPage - (c.start & (kPage - 1))) & (kPage - 1);
    uint64_t hits = 0;
    for (uint64_t i = first; i < c.len && i + kPage <= c.avail; i += kPage) {
      const uint8_t* d = c.data + i;
      const uint64_t pa = c.start + i;
      // Cheap filter first: the self-map candidate must exist.
      int self = -1;
      for (int k = 256; k < 512; ++k) {
        uint64_t e = load<uint64_t>(d + k * 8);
        if ((e & (kPtePresent | kPteWrite)) == (kPtePresent | kPteWrite) && !(e & kPteUser) &&
            (e & kPteAddrMask) == pa) {
          self = k;
          break;
        }
      }
      if (self < 0) continue;
      // Every present entry must look like a PML4E: PS bit clear, target in RAM.
      int user = 0, kernel = 0;
      bool sane = true;
      for (int k = 0; k < 512 && sane; ++k) {
        uint64_t e = load<uint64_t>(d + k * 8);
        if (!(e & kPtePresent)) continue;
        if ((e & kPteLarge) || (e & kPteAddrMask) >= max_phys) sane = false;
        (k < 256 ? user : kernel)++;
      }
      if (!sane || kernel < min_kernel_) continue;
      X64AddressSpace as(ctx.image, pa);
      uint64_t va = selfmap_pml4_va(static_cast<uint64_t>(self));
      auto back = as.translate(va);
      if (!back || *back != pa) continue;
      sink.emit(pa, kPage, "x64-dtb")
          .hex("dtb", pa)
          .hex("selfmap_index", static_cast<uint64_t>(self))
          .hex("pte_base", canonical(static_cast<uint64_t>(self) << 39))
          .num("user_entries", static_cast<uint64_t>(user))
          .num("kernel_entries", static_cast<uint64_t>(kernel))
          .flag("verified", true);
      ++hits;
    }
    sink.count("candidates", hits);
  }

 private:
  int min_kernel_ = 4;
};

}  // namespace

std::unique_ptr<Plugin> make_dtbscan_plugin() { return std::make_unique<DtbScanPlugin>(); }

}  // namespace mf
