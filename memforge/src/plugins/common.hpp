#pragma once

#include <cstdint>
#include <cstring>

namespace mf::detail {

inline bool printable(int c) { return (c >= 0x20 && c < 0x7f) || c == '\t'; }

template <typename T>
inline T load(const uint8_t* p) {
  T v;
  std::memcpy(&v, p, sizeof v);
  return v;
}

}  // namespace mf::detail
