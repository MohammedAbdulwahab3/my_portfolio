// key=value plugin options, e.g. --opt strings.min=8
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace mf {

class Options {
 public:
  void set(const std::string& key, const std::string& value) { kv_[key] = value; }
  // Parses "key=value"; throws on malformed input.
  void parse(const std::string& assignment);
  bool has(const std::string& key) const { return kv_.count(key) != 0; }
  std::string get(const std::string& key, const std::string& def) const;
  int64_t get_int(const std::string& key, int64_t def) const;
  double get_double(const std::string& key, double def) const;
  bool get_bool(const std::string& key, bool def) const;
  // Comma-separated list.
  std::vector<std::string> get_list(const std::string& key) const;
  const std::map<std::string, std::string>& all() const { return kv_; }

 private:
  std::map<std::string, std::string> kv_;
};

// Parses sizes like "16M", "4k", "1G", "4096".
uint64_t parse_size(const std::string& s);
std::vector<std::string> split(const std::string& s, char sep);

}  // namespace mf
