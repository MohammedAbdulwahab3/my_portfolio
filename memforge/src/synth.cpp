#include "memforge/synth.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>

#include "memforge/addrspace.hpp"
#include "memforge/finding.hpp"

namespace mf {

namespace {

constexpr uint64_t kPage = 4096;

struct Rng {
  uint64_t s;
  uint64_t next() {
    s ^= s << 13;
    s ^= s >> 7;
    s ^= s << 17;
    return s;
  }
  double unit() { return (next() >> 11) * (1.0 / 9007199254740992.0); }
  uint64_t below(uint64_t n) { return next() % n; }
};

const char* kWords[] = {"kernel", "process", "thread", "memory", "handle", "registry", "service",
                        "session", "desktop", "window",  "buffer", "object", "driver", "module",
                        "system",  "network", "config",  "update", "status", "record"};

void put(std::vector<uint8_t>& m, uint64_t off, const void* p, size_t n) {
  std::memcpy(m.data() + off, p, n);
}
void put_str(std::vector<uint8_t>& m, uint64_t off, const std::string& s) { put(m, off, s.data(), s.size()); }
void put_wide(std::vector<uint8_t>& m, uint64_t off, const std::string& s) {
  for (size_t i = 0; i < s.size(); ++i) {
    m[off + 2 * i] = static_cast<uint8_t>(s[i]);
    m[off + 2 * i + 1] = 0;
  }
}
template <typename T>
void put_le(std::vector<uint8_t>& m, uint64_t off, T v) {
  put(m, off, &v, sizeof v);
}

class Planter {
 public:
  Planter(SynthImage& img, Rng& rng) : img_(img), rng_(rng), used_(img.phys.size() / kPage, false) {
    const auto& sp = img.spec;
    for (uint64_t p = sp.hole_start / kPage; p < (sp.hole_start + sp.hole_size) / kPage && p < used_.size(); ++p)
      used_[p] = true;
  }

  // Reserves (and zeroes) the pages under [off, off+len). False if taken.
  bool reserve(uint64_t off, uint64_t len) {
    if (off + len > img_.phys.size()) return false;
    for (uint64_t p = off / kPage; p <= (off + len - 1) / kPage; ++p)
      if (used_[p]) return false;
    for (uint64_t p = off / kPage; p <= (off + len - 1) / kPage; ++p) {
      used_[p] = true;
      std::memset(img_.phys.data() + p * kPage, 0, kPage);
    }
    return true;
  }

  // An offset where [off, off+len) is free: straddling a boundary when asked.
  uint64_t slot(uint64_t len, bool straddle, uint64_t align = 1) {
    const uint64_t size = img_.phys.size();
    for (int tries = 0; tries < 100000; ++tries) {
      // Prefer the configured boundary; once those are taken, use 64 KiB
      // multiples (still chunk edges for small chunk sizes).
      const uint64_t b = tries < 200 ? img_.spec.boundary : (64ull << 10);
      uint64_t off;
      if (straddle && size / b > 1) {
        uint64_t k = 1 + rng_.below(size / b - 1);
        off = k * b - std::max<uint64_t>(1, len / 2);
        off -= off % align;
      } else {
        off = rng_.below(size - len - kPage) / align * align;
      }
      if (reserve(off, len)) return off;
    }
    throw std::runtime_error("synthetic image too small for planted artifacts");
  }

  void truth(std::string plugin, std::string kind, uint64_t off, std::string key, std::string value) {
    img_.truth.push_back({std::move(plugin), std::move(kind), off, std::move(key), std::move(value)});
  }

  SynthImage& img_;
  Rng& rng_;
  std::vector<bool> used_;
};

void plant_pe(Planter& pl, bool straddle_unused, bool pe64, bool dll, uint16_t subsystem,
              const std::string& kind) {
  (void)straddle_unused;
  auto& m = pl.img_.phys;
  uint64_t off = pl.slot(kPage, false, kPage);
  m[off] = 'M';
  m[off + 1] = 'Z';
  put_le<uint32_t>(m, off + 0x3c, 0x80);
  uint64_t nt = off + 0x80;
  put_le<uint32_t>(m, nt, 0x00004550);
  put_le<uint16_t>(m, nt + 4, pe64 ? 0x8664 : 0x014c);
  put_le<uint16_t>(m, nt + 6, 3);
  put_le<uint32_t>(m, nt + 8, 0x5f3e2a10);
  uint16_t opt_size = pe64 ? 0xF0 : 0xE0;
  put_le<uint16_t>(m, nt + 20, opt_size);
  put_le<uint16_t>(m, nt + 22, static_cast<uint16_t>(0x0022 | (dll ? 0x2000 : 0)));
  uint64_t opt = nt + 24;
  put_le<uint16_t>(m, opt, pe64 ? 0x20b : 0x10b);
  put_le<uint32_t>(m, opt + 16, 0x1230);
  if (pe64)
    put_le<uint64_t>(m, opt + 24, 0x180000000ull);
  else
    put_le<uint32_t>(m, opt + 28, 0x400000);
  put_le<uint32_t>(m, opt + 56, 0x5000);
  put_le<uint16_t>(m, opt + 68, subsystem);
  const char* secs[] = {".text", ".rdata", ".data"};
  for (int s = 0; s < 3; ++s) put_str(m, opt + opt_size + s * 40, secs[s]);
  pl.truth("pescan", kind, off, "format", pe64 ? "PE32+" : "PE32");
}

}  // namespace

SynthImage make_synthetic(const SynthSpec& spec_in) {
  SynthSpec spec = spec_in;
  spec.size = spec.size / kPage * kPage;
  if (spec.size < (4ull << 20)) throw std::invalid_argument("synthetic image must be >= 4 MiB");
  if (spec.hole_size && (spec.hole_start % kPage || spec.hole_size % kPage ||
                         spec.hole_start + spec.hole_size > spec.size))
    throw std::invalid_argument("hole must be page aligned and inside the image");
  SynthImage img;
  img.spec = spec;
  img.phys.assign(spec.size, 0);
  Rng rng{spec.seed * 0x9E3779B97F4A7C15ull + 1};

  // Background: zero pages, prose pages, and "code-like" pages drawn from a
  // 48-symbol alphabet (entropy <= 5.6 bits, no '.' so no stray IPs).
  static const char kCode[] = "\x00\x01\x02\x03\x04\x08\x0f\x10\x20\x24\x28\x30\x40\x41\x44\x45"
                              "\x48\x4c\x50\x55\x57\x5d\x5e\x66\x74\x75\x83\x85\x89\x8b\x8d\x90"
                              "\xb8\xc0\xc3\xc7\xcc\xd0\xe8\xe9\xeb\xf0\xf6\xff\x0a\x7f\x14\x18";
  const uint64_t pages = spec.size / kPage;
  for (uint64_t p = 0; p < pages; ++p) {
    if (spec.hole_size && p * kPage >= spec.hole_start && p * kPage < spec.hole_start + spec.hole_size)
      continue;
    uint8_t* d = img.phys.data() + p * kPage;
    double u = rng.unit();
    if (u < spec.zero_fraction) continue;
    if (u < spec.zero_fraction + spec.text_fraction) {
      size_t i = 0;
      while (i < kPage) {
        const char* w = kWords[rng.below(20)];
        size_t n = std::strlen(w);
        for (size_t k = 0; k < n && i < kPage; ++k) d[i++] = static_cast<uint8_t>(w[k]);
        if (i < kPage) d[i++] = (rng.below(8) == 0) ? 0 : ' ';
      }
    } else {
      for (size_t i = 0; i < kPage; i += 8) {
        uint64_t r = rng.next();
        for (int k = 0; k < 8; ++k) d[i + k] = static_cast<uint8_t>(kCode[((r >> (k * 8)) & 0xff) % 48]);
      }
    }
  }

  Planter pl(img, rng);
  auto& m = img.phys;
  for (int copy = 0; copy < std::max(1, spec.copies); ++copy) {
    // Network IOCs (ASCII and UTF-16LE), half of them straddling boundaries.
    const std::string urls[] = {"http://evil-c2.example.com/gate.php?id=1337",
                                "https://update.badcdn.net/payload.bin",
                                "ftp://203.0.113.77/drop/stage2.dll"};
    for (int k = 0; k < 3; ++k) {
      uint64_t off = pl.slot(urls[k].size() + 2, k != 1);
      put_str(m, off + 1, urls[k]);
      pl.truth("netioc", "url", off + 1, "url", urls[k]);
    }
    const std::string wurl = "https://exfil.example.org/upload?k=42";
    uint64_t off = pl.slot(2 * wurl.size() + 4, true, 2);
    put_wide(m, off + 2, wurl);
    pl.truth("netioc", "url", off + 2, "url", wurl);

    const std::string ips[] = {"185.220.101.42", "10.13.37.1"};
    for (int k = 0; k < 2; ++k) {
      off = pl.slot(ips[k].size() + 2, k == 0);
      put_str(m, off + 1, ips[k]);
      pl.truth("netioc", "ipv4", off + 1, "ip", ips[k]);
    }
    off = pl.slot(40, true, 2);
    put_wide(m, off + 2, "45.33.32.156");
    pl.truth("netioc", "ipv4", off + 2, "ip", "45.33.32.156");

    // Strings.
    for (int k = 0; k < 4; ++k) {
      std::string s = "MEMFORGE_MARKER_STRING_" + std::to_string(copy) + "_" + std::to_string(k);
      off = pl.slot(s.size() + 2, k % 2 == 0);
      put_str(m, off + 1, s);
      pl.truth("strings", "ascii", off + 1, "value", s);
    }
    std::string ws = "C:\\Windows\\Temp\\svch0st.exe";
    off = pl.slot(2 * ws.size() + 4, true, 2);
    put_wide(m, off + 2, ws);
    pl.truth("strings", "utf16le", off + 2, "value", ws);

    // patscan IOCs (rules in synth_patscan_rules()).
    off = pl.slot(16, true);
    put_str(m, off + 1, "MiMiKaTz");
    pl.truth("patscan", "match", off + 1, "rule", "mimikatz");
    off = pl.slot(64, false);
    put_wide(m, off + 2, "Invoke-ReflectivePEInjection");
    pl.truth("patscan", "match", off + 2, "rule", "reflective_loader");
    off = pl.slot(16, true);
    const uint8_t sc[] = {0xfc, 0x48, 0x83, 0xe4, 0xf0, 0xe8, 0xc0, 0x00, 0x00, 0x00};
    put(m, off + 1, sc, sizeof sc);
    pl.truth("patscan", "match", off + 1, "rule", "msf_x64_shellcode");

    // PE images.
    plant_pe(pl, false, true, true, 2, "dll");
    plant_pe(pl, false, true, false, 1, "driver");
    plant_pe(pl, false, false, false, 2, "exe");

    // Pool allocations: a process object (with its image name) and a socket.
    off = pl.slot(0x600, true, 16);
    m[off + 2] = 0x60;  // BlockSize: 0x600 bytes
    m[off + 3] = 0x02;
    put_str(m, off + 4, "Pro\xe3");  // tag "Proc" with the protected bit set
    put_str(m, off + 0x450, "evil.exe");
    pl.truth("pooltag", "process", off, "name_guess", "evil.exe");
    off = pl.slot(0x200, false, 16);
    m[off + 2] = 0x20;
    m[off + 3] = 0x02;
    put_str(m, off + 4, "TcpE");
    pl.truth("pooltag", "tcp-endpoint", off, "tag", "TcpE");

    // A 40-page block of random bytes straddling a boundary: one region.
    const uint64_t hp = 40;
    off = pl.slot(hp * kPage, true, kPage);
    for (uint64_t i = 0; i < hp * kPage; i += 8) put_le<uint64_t>(m, off + i, rng.next());
    pl.truth("entropy", "high-entropy-region", off, "pages", std::to_string(hp));

    // x64 page tables: a real PML4 with a self-map at a random kernel slot.
    uint64_t pml4 = pl.slot(kPage, false, kPage);
    uint64_t self = 256 + rng.below(256);
    put_le<uint64_t>(m, pml4 + self * 8, pml4 | 0x63);
    for (int k = 0; k < 5; ++k) {
      uint64_t idx = 256 + (self - 256 + 1 + k * 37) % 256;
      uint64_t tbl = pl.slot(kPage, false, kPage);
      put_le<uint64_t>(m, pml4 + idx * 8, tbl | 0x63);
    }
    for (int k = 0; k < 2; ++k) {
      uint64_t tbl = pl.slot(kPage, false, kPage);
      put_le<uint64_t>(m, pml4 + k * 8, tbl | 0x67);
    }
    pl.truth("dtbscan", "x64-dtb", pml4, "dtb", hex_str(pml4));
    // Decoy: self-map entry, but a present entry has the (reserved) PS bit.
    uint64_t decoy = pl.slot(kPage, false, kPage);
    put_le<uint64_t>(m, decoy + 0x1ED * 8, decoy | 0x63);
    for (int k = 0; k < 5; ++k) put_le<uint64_t>(m, decoy + (0x100 + k) * 8, pml4 | 0x63);
    put_le<uint64_t>(m, decoy + 0x1F0 * 8, pml4 | 0xE3);
  }
  return img;
}

std::vector<uint8_t> to_raw(const SynthImage& s) { return s.phys; }

std::vector<uint8_t> to_lime(const SynthImage& s) {
  std::vector<std::pair<uint64_t, uint64_t>> ranges;  // [start, end)
  const auto& sp = s.spec;
  if (sp.hole_size) {
    if (sp.hole_start) ranges.push_back({0, sp.hole_start});
    if (sp.hole_start + sp.hole_size < sp.size) ranges.push_back({sp.hole_start + sp.hole_size, sp.size});
  } else {
    ranges.push_back({0, sp.size});
  }
  std::vector<uint8_t> out;
  for (auto [a, b] : ranges) {
    uint8_t h[32] = {};
    uint32_t magic = 0x4C694D45, version = 1;
    uint64_t e = b - 1;
    std::memcpy(h, &magic, 4);
    std::memcpy(h + 4, &version, 4);
    std::memcpy(h + 8, &a, 8);
    std::memcpy(h + 16, &e, 8);
    out.insert(out.end(), h, h + 32);
    out.insert(out.end(), s.phys.begin() + a, s.phys.begin() + b);
  }
  return out;
}

std::unique_ptr<MemoryImage> to_memory_image(const SynthImage& s, bool with_hole) {
  if (!with_hole || !s.spec.hole_size) return MemoryImage::from_buffer(s.phys, {}, "synthetic");
  const auto& sp = s.spec;
  std::vector<uint8_t> buf;
  std::vector<MemRange> ranges;
  if (sp.hole_start) {
    ranges.push_back({0, sp.hole_start, 0});
    buf.insert(buf.end(), s.phys.begin(), s.phys.begin() + sp.hole_start);
  }
  uint64_t tail = sp.hole_start + sp.hole_size;
  if (tail < sp.size) {
    ranges.push_back({tail, sp.size - tail, buf.size()});
    buf.insert(buf.end(), s.phys.begin() + tail, s.phys.end());
  }
  return MemoryImage::from_buffer(std::move(buf), std::move(ranges), "synthetic-lime");
}

std::string truth_to_json(const SynthImage& s) {
  std::string o = "[";
  for (size_t i = 0; i < s.truth.size(); ++i) {
    const auto& t = s.truth[i];
    if (i) o += ",\n ";
    o += "{\"plugin\":" + json_quote(t.plugin) + ",\"kind\":" + json_quote(t.kind) +
         ",\"offset\":" + std::to_string(t.offset) + ",\"key\":" + json_quote(t.key) +
         ",\"value\":" + json_quote(t.value) + "}";
  }
  return o + "]";
}

std::string synth_patscan_rules() {
  return "# planted by memforge synth\n"
         "mimikatz          text \"mimikatz\" nocase\n"
         "reflective_loader text \"Invoke-ReflectivePEInjection\" wide\n"
         "msf_x64_shellcode hex  \"fc 48 83 e4 f0 e8 c0 00 00 00\"\n";
}

}  // namespace mf
