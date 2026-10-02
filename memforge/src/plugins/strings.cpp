// ASCII and UTF-16LE string extraction (like `strings -a` + `strings -el`).
#include <algorithm>
#include <array>
#include <memory>

#if defined(__SSE2__)
#include <emmintrin.h>
#endif

#include "common.hpp"
#include "memforge/plugin.hpp"
#include "plugins.hpp"

namespace mf {
namespace {

using detail::printable;

struct Masks {
  uint64_t print;
  uint64_t zero;
};

const std::array<bool, 256>& table();

Masks masks_scalar(const uint8_t* p, uint64_t n) {
  const auto& P = table();
  Masks m{0, 0};
  for (uint64_t k = 0; k < n; ++k) {
    m.print |= static_cast<uint64_t>(P[p[k]]) << k;
    m.zero |= static_cast<uint64_t>(p[k] == 0) << k;
  }
  return m;
}

inline Masks masks64(const uint8_t* p) {
#if defined(__SSE2__)
  // printable: c in [0x20, 0x7e] <=> int8(c + 0x60) < -33, or c == '\t'
  const __m128i off = _mm_set1_epi8(0x60), lim = _mm_set1_epi8(-33);
  const __m128i tab = _mm_set1_epi8('\t'), z = _mm_setzero_si128();
  Masks m{0, 0};
  for (int k = 0; k < 4; ++k) {
    __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(p + 16 * k));
    __m128i pr = _mm_or_si128(_mm_cmplt_epi8(_mm_add_epi8(v, off), lim), _mm_cmpeq_epi8(v, tab));
    m.print |= static_cast<uint64_t>(static_cast<uint16_t>(_mm_movemask_epi8(pr))) << (16 * k);
    m.zero |= static_cast<uint64_t>(static_cast<uint16_t>(_mm_movemask_epi8(_mm_cmpeq_epi8(v, z)))) << (16 * k);
  }
  return m;
#else
  return masks_scalar(p, 64);
#endif
}

const std::array<bool, 256>& table() {
  static const auto t = [] {
    std::array<bool, 256> a{};
    for (int c = 0; c < 256; ++c) a[c] = printable(c);
    return a;
  }();
  return t;
}

class StringsPlugin : public Plugin {
 public:
  const char* name() const override { return "strings"; }
  const char* description() const override {
    return "ASCII and UTF-16LE printable strings (strings.min, strings.max, strings.wide)";
  }
  void configure(const Options& o) override {
    min_ = static_cast<uint64_t>(std::max<int64_t>(1, o.get_int("strings.min", 6)));
    max_ = static_cast<uint64_t>(std::max<int64_t>(static_cast<int64_t>(min_), o.get_int("strings.max", 1024)));
    wide_ = o.get_bool("strings.wide", true);
  }
  // A string is reported once, at its first byte, truncated to max_ chars.
  uint64_t halo() const override { return 2 * max_; }

  // Bit-parallel: per 64-byte block build "printable" and "zero" masks
  // (SSE2 when available), derive run starts with shifts, and visit only the
  // set bits. Cost is independent of how often bytes flip between classes,
  // which is what makes a byte-at-a-time loop mispredict on real memory.
  void scan(const ScanContext&, const Chunk& c, Sink& sink) const override {
    const auto& P = table();
    const uint8_t* d = c.data;
    const uint64_t len = c.len, avail = c.avail;
    uint64_t ascii = 0, wide = 0;
    auto wide_at = [&](int64_t j) {  // a UTF-16LE printable char starts at j
      int lo = c.at(j), hi = c.at(j + 1);
      return lo >= 0 && P[lo] && hi == 0;
    };
    uint64_t acarry = (c.behind > 0 && P[d[-1]]) ? 1 : 0;
    uint64_t wcarry = (wide_at(-2) ? 1 : 0) | (wide_at(-1) ? 2 : 0);

    for (uint64_t base = 0; base < len; base += 64) {
      const uint64_t n = std::min<uint64_t>(64, avail - base);  // n < 64 only at the range end
      const Masks m = n == 64 ? masks64(d + base) : masks_scalar(d + base, n);
      const uint64_t own = len - base >= 64 ? ~0ull : ((1ull << (len - base)) - 1);

      uint64_t starts = m.print & ~((m.print << 1) | acarry) & own;
      acarry = m.print >> 63;
      while (starts) {
        const unsigned bit = static_cast<unsigned>(__builtin_ctzll(starts));
        starts &= starts - 1;
        const uint64_t i = base + bit;
        const uint64_t rest = ~m.print >> bit;
        uint64_t e;
        if (rest) {
          e = i + static_cast<uint64_t>(__builtin_ctzll(rest));
        } else {  // run continues into the next block
          e = base + 64;
          while (e < avail && e - i < max_ && P[d[e]]) ++e;
        }
        const uint64_t n_chars = std::min(e - i, max_);
        if (n_chars >= min_) {
          sink.emit(c.start + i, n_chars, "ascii")
              .str("value", std::string(reinterpret_cast<const char*>(d + i), n_chars));
          ++ascii;
        }
      }

      if (!wide_) continue;
      uint64_t zero_next = m.zero >> 1;
      if (n == 64 && base + 64 < avail && d[base + 64] == 0) zero_next |= 1ull << 63;
      const uint64_t w = m.print & zero_next;  // bit j: wide char starts at base+j
      uint64_t wstarts = w & ~((w << 2) | wcarry) & own;
      wcarry = (w >> 62) & 3;
      while (wstarts) {
        const uint64_t i = base + static_cast<unsigned>(__builtin_ctzll(wstarts));
        wstarts &= wstarts - 1;
        uint64_t k = 1;
        while (k < max_ && i + 2 * k + 1 < avail && P[d[i + 2 * k]] && d[i + 2 * k + 1] == 0) ++k;
        if (k >= min_) {
          std::string v(k, ' ');
          for (uint64_t q = 0; q < k; ++q) v[q] = static_cast<char>(d[i + 2 * q]);
          sink.emit(c.start + i, 2 * k, "utf16le").str("value", std::move(v));
          ++wide;
        }
      }
    }
    sink.count("ascii", ascii);
    sink.count("utf16le", wide);
  }

 private:
  uint64_t min_ = 6, max_ = 1024;
  bool wide_ = true;
};

}  // namespace

std::unique_ptr<Plugin> make_strings_plugin() { return std::make_unique<StringsPlugin>(); }

}  // namespace mf
