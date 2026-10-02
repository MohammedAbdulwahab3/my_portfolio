// Findings produced by plugins, plus JSON and binary (MPI wire) encodings.
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace mf {

struct Field {
  enum class Type : uint8_t { Str, UInt, Hex, Float, Bool };
  std::string key;
  Type type = Type::Str;
  std::string s;
  uint64_t u = 0;
  double d = 0;
};

struct Finding {
  uint64_t offset = 0;  // physical address of the first byte
  uint64_t length = 0;
  uint16_t plugin = 0;  // index into the active plugin list
  std::string kind;
  std::vector<Field> fields;

  Finding& str(std::string key, std::string v);
  Finding& num(std::string key, uint64_t v);
  Finding& hex(std::string key, uint64_t v);
  Finding& real(std::string key, double v);
  Finding& flag(std::string key, bool v);
  const Field* get(const std::string& key) const;
  Field* get(const std::string& key);
};

// Total order used to make output independent of scheduling.
bool finding_less(const Finding& a, const Finding& b);

// Named counters, aggregated by summation. Keys are "<plugin>.<counter>".
using Counters = std::map<std::string, uint64_t>;
void merge_counters(Counters& into, const Counters& from);

// JSON helpers -------------------------------------------------------------
// Escapes arbitrary bytes into a JSON string body (bytes >= 0x80 -> \u00XX),
// so output is always valid JSON even for garbage pulled out of memory.
void json_escape(std::string& out, const std::string& s);
std::string json_quote(const std::string& s);
std::string hex_str(uint64_t v);
// One JSON object (no trailing newline). image may be empty.
std::string finding_to_json(const Finding& f, const std::string& plugin_name,
                            const std::string& image);

// Binary wire format (used to ship results between MPI ranks) ---------------
class ByteWriter {
 public:
  void u8(uint8_t v) { buf_.push_back(v); }
  void u64(uint64_t v);
  void f64(double v);
  void str(const std::string& s);
  std::vector<uint8_t>& bytes() { return buf_; }

 private:
  std::vector<uint8_t> buf_;
};

class ByteReader {
 public:
  ByteReader(const uint8_t* p, size_t n) : p_(p), end_(p + n) {}
  uint8_t u8();
  uint64_t u64();
  double f64();
  std::string str();
  bool done() const { return p_ == end_; }

 private:
  void need(size_t n);
  const uint8_t* p_;
  const uint8_t* end_;
};

void encode_finding(ByteWriter& w, const Finding& f);
Finding decode_finding(ByteReader& r);
void encode_counters(ByteWriter& w, const Counters& c);
Counters decode_counters(ByteReader& r);

}  // namespace mf
