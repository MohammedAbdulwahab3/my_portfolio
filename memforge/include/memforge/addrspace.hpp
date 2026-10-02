// x86-64 4-level paging (4 KiB / 2 MiB / 1 GiB pages) over a physical image.
#pragma once

#include <cstdint>
#include <optional>

#include "memforge/image.hpp"

namespace mf {

constexpr uint64_t kPteAddrMask = 0x000FFFFFFFFFF000ull;
constexpr uint64_t kPtePresent = 1ull << 0;
constexpr uint64_t kPteWrite = 1ull << 1;
constexpr uint64_t kPteUser = 1ull << 2;
constexpr uint64_t kPteLarge = 1ull << 7;

// Sign-extends a 48-bit virtual address to canonical form.
inline uint64_t canonical(uint64_t va) {
  return (va & (1ull << 47)) ? (va | 0xFFFF000000000000ull) : (va & 0x0000FFFFFFFFFFFFull);
}

// Virtual address of the page-table self-map whose PML4 slot is `idx`, i.e.
// the VA at which the PML4 page itself is visible.
inline uint64_t selfmap_pml4_va(uint64_t idx) {
  return canonical((idx << 39) | (idx << 30) | (idx << 21) | (idx << 12));
}

class X64AddressSpace {
 public:
  X64AddressSpace(const MemoryImage& image, uint64_t dtb)
      : image_(image), dtb_(dtb & kPteAddrMask) {}
  uint64_t dtb() const { return dtb_; }
  std::optional<uint64_t> translate(uint64_t va) const;
  // Reads through the page tables; unmapped bytes are zero-filled.
  // Returns the number of bytes that were mapped.
  uint64_t read(uint64_t va, void* dst, uint64_t len) const;

 private:
  const MemoryImage& image_;
  uint64_t dtb_;
};

}  // namespace mf
