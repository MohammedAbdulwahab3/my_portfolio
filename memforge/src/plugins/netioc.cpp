// Network indicators: URLs and dotted-quad IPv4 addresses, in both ASCII and
// UTF-16LE (Windows keeps most strings wide).
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

#include "memforge/plugin.hpp"
#include "plugins.hpp"

namespace mf {
namespace {

constexpr uint64_t kMaxUrl = 2048;

bool is_alpha(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
bool is_digit(int c) { return c >= '0' && c <= '9'; }
bool is_alnum(int c) { return is_alpha(c) || is_digit(c); }
bool is_url_char(int c) {
  if (is_alnum(c)) return true;
  switch (c) {
    case '-': case '.': case '_': case '~': case ':': case '/': case '?': case '#':
    case '[': case ']': case '@': case '!': case '$': case '&': case '\'': case '(':
    case ')': case '*': case '+': case ',': case ';': case '=': case '%':
      return true;
    default:
      return false;
  }
}

// Character view over a chunk with stride 1 (ASCII) or 2 (UTF-16LE, high
// byte must be zero). Positions are chunk-relative byte offsets.
struct View {
  const Chunk& c;
  int stride;
  int ch(int64_t pos) const {
    int lo = c.at(pos);
    if (lo < 0) return -1;
    if (stride == 2) {
      int hi = c.at(pos + 1);
      if (hi != 0) return -1;
    }
    return lo;
  }
};

std::string ip_class(int a, int b) {
  if (a == 10 || (a == 172 && b >= 16 && b <= 31) || (a == 192 && b == 168)) return "private";
  if (a == 127) return "loopback";
  if (a == 169 && b == 254) return "link-local";
  if (a >= 224 && a <= 239) return "multicast";
  if (a == 0 || a >= 240) return "reserved";
  return "public";
}

class NetIocPlugin : public Plugin {
 public:
  const char* name() const override { return "netioc"; }
  const char* description() const override {
    return "URLs and IPv4 addresses in ASCII and UTF-16LE (netioc.wide, netioc.public_only)";
  }
  void configure(const Options& o) override {
    wide_ = o.get_bool("netioc.wide", true);
    public_only_ = o.get_bool("netioc.public_only", false);
  }
  uint64_t halo() const override { return 2 * (kMaxUrl + 16); }

  void scan(const ScanContext&, const Chunk& c, Sink& sink) const override {
    uint64_t urls = 0, ips = 0;
    for (int stride = 1; stride <= (wide_ ? 2 : 1); ++stride) {
      View v{c, stride};
      const char* enc = stride == 1 ? "ascii" : "utf16le";
      // URLs: anchor on "://", walk back over the scheme. The URL is owned by
      // the chunk holding the scheme's first byte, so look a few chars past
      // the end of the owned region for anchors.
      const int64_t limit = std::min<int64_t>(static_cast<int64_t>(c.len) + 8 * stride,
                                              static_cast<int64_t>(c.avail) - 3 * stride);
      for (int64_t i = 0; i < limit; ++i) {
        const void* hit = std::memchr(c.data + i, ':', static_cast<size_t>(limit - i));
        if (!hit) break;
        i = static_cast<const uint8_t*>(hit) - c.data;
        if (v.ch(i) != ':' || v.ch(i + stride) != '/' || v.ch(i + 2 * stride) != '/') continue;
        int64_t s = i;
        int k = 0;
        while (k < 6 && is_alpha(v.ch(s - stride))) {
          s -= stride;
          ++k;
        }
        if (k < 3 || is_alnum(v.ch(s - stride))) continue;
        if (s < 0 || s >= static_cast<int64_t>(c.len)) continue;  // not ours
        std::string scheme;
        for (int64_t p = s; p < i; p += stride) scheme += static_cast<char>(v.ch(p) | 0x20);
        if (scheme != "http" && scheme != "https" && scheme != "ftp" && scheme != "hxxp" && scheme != "hxxps")
          continue;
        std::string url = scheme + "://";
        int64_t p = i + 3 * stride;
        while (url.size() < kMaxUrl && is_url_char(v.ch(p))) {
          url += static_cast<char>(v.ch(p));
          p += stride;
        }
        // Host validation: [user@]host[:port], host needs a dot or "localhost".
        size_t hs = scheme.size() + 3;
        size_t he = url.find_first_of("/?#", hs);
        std::string auth = url.substr(hs, he == std::string::npos ? std::string::npos : he - hs);
        auto at = auth.rfind('@');
        std::string host = at == std::string::npos ? auth : auth.substr(at + 1);
        auto colon = host.find(':');
        if (colon != std::string::npos) host.resize(colon);
        bool ok = host.size() >= 3 && (host.find('.') != std::string::npos || host == "localhost");
        for (char ch : host)
          if (!is_alnum(ch) && ch != '-' && ch != '.') ok = false;
        if (!ok || host.front() == '.' || host.back() == '.') continue;
        while (!url.empty() && (url.back() == '.' || url.back() == ',' || url.back() == ')' || url.back() == '\''))
          url.pop_back();  // trailing punctuation from surrounding prose
        sink.emit(c.start + s, (url.size()) * stride, "url")
            .str("url", url)
            .str("host", host)
            .str("encoding", enc);
        ++urls;
      }
      // IPv4: owned by the chunk holding the first digit. Anchor on the
      // first '.', which is far rarer in memory than digits, then step back
      // over at most three digits to the candidate start.
      const int64_t dlimit = std::min<int64_t>(static_cast<int64_t>(c.len) + 3 * stride,
                                               static_cast<int64_t>(c.avail));
      for (int64_t dot = 0; dot < dlimit; ++dot) {
        const void* hit = std::memchr(c.data + dot, '.', static_cast<size_t>(dlimit - dot));
        if (!hit) break;
        dot = static_cast<const uint8_t*>(hit) - c.data;
        if (v.ch(dot) != '.') continue;
        int64_t i = dot;
        int nd = 0;
        while (nd < 3 && is_digit(v.ch(i - stride))) {
          i -= stride;
          ++nd;
        }
        if (nd == 0 || i < 0 || i >= static_cast<int64_t>(c.len)) continue;
        int prev = v.ch(i - stride);
        if (is_digit(prev) || prev == '.' || is_alpha(prev)) continue;
        int oct[4];
        int64_t p = i;
        bool ok = true;
        for (int k = 0; k < 4 && ok; ++k) {
          int n = 0, val = 0;
          while (n < 4 && is_digit(v.ch(p))) {
            val = val * 10 + (v.ch(p) - '0');
            p += stride;
            ++n;
          }
          if (n == 0 || n > 3 || val > 255) ok = false;
          oct[k] = val;
          if (ok && k < 3) {
            if (v.ch(p) != '.') ok = false;
            p += stride;
          }
        }
        if (!ok) continue;
        int next = v.ch(p);
        if (is_digit(next) || is_alpha(next) || (next == '.' && is_digit(v.ch(p + stride)))) continue;
        if (oct[0] == 0) continue;  // 0.x.x.x is almost always a version number
        std::string cls = ip_class(oct[0], oct[1]);
        if (public_only_ && cls != "public") continue;
        char buf[20];
        std::snprintf(buf, sizeof buf, "%d.%d.%d.%d", oct[0], oct[1], oct[2], oct[3]);
        sink.emit(c.start + i, static_cast<uint64_t>(p - i), "ipv4")
            .str("ip", buf)
            .str("class", cls)
            .str("encoding", enc);
        ++ips;
      }
    }
    sink.count("urls", urls);
    sink.count("ipv4", ips);
  }

 private:
  bool wide_ = true;
  bool public_only_ = false;
};

}  // namespace

std::unique_ptr<Plugin> make_netioc_plugin() { return std::make_unique<NetIocPlugin>(); }

}  // namespace mf
