#include "memforge/finding.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <tuple>

namespace mf {

namespace {
Field& add(std::vector<Field>& fs, std::string key, Field::Type t) {
  fs.emplace_back();
  fs.back().key = std::move(key);
  fs.back().type = t;
  return fs.back();
}
}  // namespace

Finding& Finding::str(std::string key, std::string v) {
  add(fields, std::move(key), Field::Type::Str).s = std::move(v);
  return *this;
}
Finding& Finding::num(std::string key, uint64_t v) {
  add(fields, std::move(key), Field::Type::UInt).u = v;
  return *this;
}
Finding& Finding::hex(std::string key, uint64_t v) {
  add(fields, std::move(key), Field::Type::Hex).u = v;
  return *this;
}
Finding& Finding::real(std::string key, double v) {
  add(fields, std::move(key), Field::Type::Float).d = v;
  return *this;
}
Finding& Finding::flag(std::string key, bool v) {
  add(fields, std::move(key), Field::Type::Bool).u = v ? 1 : 0;
  return *this;
}
const Field* Finding::get(const std::string& key) const {
  for (const auto& f : fields)
    if (f.key == key) return &f;
  return nullptr;
}
Field* Finding::get(const std::string& key) {
  for (auto& f : fields)
    if (f.key == key) return &f;
  return nullptr;
}

bool finding_less(const Finding& a, const Finding& b) {
  return std::tie(a.offset, a.plugin, a.kind, a.length) <
         std::tie(b.offset, b.plugin, b.kind, b.length);
}

void merge_counters(Counters& into, const Counters& from) {
  for (const auto& [k, v] : from) into[k] += v;
}

void json_escape(std::string& out, const std::string& s) {
  static const char* hexd = "0123456789abcdef";
  for (unsigned char c : s) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (c < 0x20 || c >= 0x7f) {
          out += "\\u00";
          out += hexd[c >> 4];
          out += hexd[c & 15];
        } else {
          out += static_cast<char>(c);
        }
    }
  }
}

std::string json_quote(const std::string& s) {
  std::string out = "\"";
  json_escape(out, s);
  out += '"';
  return out;
}

std::string hex_str(uint64_t v) {
  char b[24];
  std::snprintf(b, sizeof b, "0x%llx", static_cast<unsigned long long>(v));
  return b;
}

std::string finding_to_json(const Finding& f, const std::string& plugin_name,
                            const std::string& image) {
  std::string o;
  o.reserve(128);
  o += '{';
  if (!image.empty()) {
    o += "\"image\":";
    o += json_quote(image);
    o += ',';
  }
  o += "\"plugin\":";
  o += json_quote(plugin_name);
  o += ",\"offset\":";
  o += std::to_string(f.offset);
  o += ",\"offset_hex\":\"";
  o += hex_str(f.offset);
  o += "\",\"length\":";
  o += std::to_string(f.length);
  o += ",\"kind\":";
  o += json_quote(f.kind);
  for (const auto& fld : f.fields) {
    o += ',';
    o += json_quote(fld.key);
    o += ':';
    switch (fld.type) {
      case Field::Type::Str: o += json_quote(fld.s); break;
      case Field::Type::UInt: o += std::to_string(fld.u); break;
      case Field::Type::Hex: o += '"' + hex_str(fld.u) + '"'; break;
      case Field::Type::Bool: o += fld.u ? "true" : "false"; break;
      case Field::Type::Float: {
        char b[32];
        if (std::isfinite(fld.d))
          std::snprintf(b, sizeof b, "%.4f", fld.d);
        else
          std::snprintf(b, sizeof b, "null");
        o += b;
        break;
      }
    }
  }
  o += '}';
  return o;
}

void ByteWriter::u64(uint64_t v) {
  uint8_t b[8];
  std::memcpy(b, &v, 8);
  buf_.insert(buf_.end(), b, b + 8);
}
void ByteWriter::f64(double v) {
  uint64_t u;
  std::memcpy(&u, &v, 8);
  u64(u);
}
void ByteWriter::str(const std::string& s) {
  u64(s.size());
  buf_.insert(buf_.end(), s.begin(), s.end());
}

void ByteReader::need(size_t n) {
  if (static_cast<size_t>(end_ - p_) < n) throw std::runtime_error("truncated message");
}
uint8_t ByteReader::u8() {
  need(1);
  return *p_++;
}
uint64_t ByteReader::u64() {
  need(8);
  uint64_t v;
  std::memcpy(&v, p_, 8);
  p_ += 8;
  return v;
}
double ByteReader::f64() {
  uint64_t u = u64();
  double d;
  std::memcpy(&d, &u, 8);
  return d;
}
std::string ByteReader::str() {
  uint64_t n = u64();
  need(n);
  std::string s(reinterpret_cast<const char*>(p_), n);
  p_ += n;
  return s;
}

void encode_finding(ByteWriter& w, const Finding& f) {
  w.u64(f.offset);
  w.u64(f.length);
  w.u64(f.plugin);
  w.str(f.kind);
  w.u64(f.fields.size());
  for (const auto& fld : f.fields) {
    w.str(fld.key);
    w.u8(static_cast<uint8_t>(fld.type));
    switch (fld.type) {
      case Field::Type::Str: w.str(fld.s); break;
      case Field::Type::Float: w.f64(fld.d); break;
      default: w.u64(fld.u);
    }
  }
}

Finding decode_finding(ByteReader& r) {
  Finding f;
  f.offset = r.u64();
  f.length = r.u64();
  f.plugin = static_cast<uint16_t>(r.u64());
  f.kind = r.str();
  uint64_t n = r.u64();
  f.fields.resize(n);
  for (auto& fld : f.fields) {
    fld.key = r.str();
    fld.type = static_cast<Field::Type>(r.u8());
    switch (fld.type) {
      case Field::Type::Str: fld.s = r.str(); break;
      case Field::Type::Float: fld.d = r.f64(); break;
      default: fld.u = r.u64();
    }
  }
  return f;
}

void encode_counters(ByteWriter& w, const Counters& c) {
  w.u64(c.size());
  for (const auto& [k, v] : c) {
    w.str(k);
    w.u64(v);
  }
}

Counters decode_counters(ByteReader& r) {
  Counters c;
  uint64_t n = r.u64();
  for (uint64_t i = 0; i < n; ++i) {
    std::string k = r.str();
    c[k] = r.u64();
  }
  return c;
}

}  // namespace mf
