#include "memforge/addrspace.hpp"

#include <algorithm>
#include <cstring>

namespace mf {

std::optional<uint64_t> X64AddressSpace::translate(uint64_t va) const {
  if (canonical(va) != va) return std::nullopt;
  uint64_t e;
  if (!image_.read_u64(dtb_ + ((va >> 39) & 0x1FF) * 8, e) || !(e & kPtePresent)) return std::nullopt;
  if (!image_.read_u64((e & kPteAddrMask) + ((va >> 30) & 0x1FF) * 8, e) || !(e & kPtePresent))
    return std::nullopt;
  if (e & kPteLarge) return (e & 0x000FFFFFC0000000ull) | (va & 0x3FFFFFFFull);
  if (!image_.read_u64((e & kPteAddrMask) + ((va >> 21) & 0x1FF) * 8, e) || !(e & kPtePresent))
    return std::nullopt;
  if (e & kPteLarge) return (e & 0x000FFFFFFFE00000ull) | (va & 0x1FFFFFull);
  if (!image_.read_u64((e & kPteAddrMask) + ((va >> 12) & 0x1FF) * 8, e) || !(e & kPtePresent))
    return std::nullopt;
  return (e & kPteAddrMask) | (va & 0xFFFull);
}

uint64_t X64AddressSpace::read(uint64_t va, void* dst, uint64_t len) const {
  auto* out = static_cast<uint8_t*>(dst);
  uint64_t mapped = 0;
  while (len > 0) {
    uint64_t n = std::min<uint64_t>(len, 4096 - (va & 0xFFF));
    auto pa = translate(va);
    if (pa)
      mapped += image_.read(*pa, out, n);
    else
      std::memset(out, 0, n);
    out += n;
    va += n;
    len -= n;
  }
  return mapped;
}

}  // namespace mf
