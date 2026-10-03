#include "memforge/options.hpp"

#include <cctype>
#include <stdexcept>

namespace mf {

void Options::parse(const std::string& a) {
  auto eq = a.find('=');
  if (eq == std::string::npos || eq == 0) throw std::invalid_argument("expected key=value, got '" + a + "'");
  set(a.substr(0, eq), a.substr(eq + 1));
}

std::string Options::get(const std::string& key, const std::string& def) const {
  auto it = kv_.find(key);
  return it == kv_.end() ? def : it->second;
}

int64_t Options::get_int(const std::string& key, int64_t def) const {
  auto it = kv_.find(key);
  if (it == kv_.end()) return def;
  try {
    size_t pos = 0;
    int64_t v = std::stoll(it->second, &pos, 0);
    if (pos != it->second.size()) throw std::invalid_argument("");
    return v;
  } catch (...) {
    throw std::invalid_argument("option " + key + ": not an integer: " + it->second);
  }
}

double Options::get_double(const std::string& key, double def) const {
  auto it = kv_.find(key);
  if (it == kv_.end()) return def;
  try {
    return std::stod(it->second);
  } catch (...) {
    throw std::invalid_argument("option " + key + ": not a number: " + it->second);
  }
}

bool Options::get_bool(const std::string& key, bool def) const {
  auto it = kv_.find(key);
  if (it == kv_.end()) return def;
  const std::string& v = it->second;
  if (v == "1" || v == "true" || v == "yes" || v == "on") return true;
  if (v == "0" || v == "false" || v == "no" || v == "off") return false;
  throw std::invalid_argument("option " + key + ": not a boolean: " + v);
}

std::vector<std::string> Options::get_list(const std::string& key) const {
  auto it = kv_.find(key);
  if (it == kv_.end()) return {};
  return split(it->second, ',');
}

std::vector<std::string> split(const std::string& s, char sep) {
  std::vector<std::string> out;
  std::string cur;
  for (char c : s) {
    if (c == sep) {
      if (!cur.empty()) out.push_back(cur);
      cur.clear();
    } else {
      cur += c;
    }
  }
  if (!cur.empty()) out.push_back(cur);
  return out;
}

uint64_t parse_size(const std::string& s) {
  if (s.empty()) throw std::invalid_argument("empty size");
  size_t pos = 0;
  unsigned long long v = std::stoull(s, &pos, 0);
  uint64_t mul = 1;
  if (pos < s.size()) {
    char c = static_cast<char>(std::toupper(static_cast<unsigned char>(s[pos])));
    if (c == 'K') mul = 1ull << 10;
    else if (c == 'M') mul = 1ull << 20;
    else if (c == 'G') mul = 1ull << 30;
    else if (c == 'T') mul = 1ull << 40;
    else throw std::invalid_argument("bad size suffix in '" + s + "'");
    ++pos;
    if (pos < s.size() && (s[pos] == 'i' || s[pos] == 'I')) ++pos;
    if (pos < s.size() && (s[pos] == 'b' || s[pos] == 'B')) ++pos;
    if (pos != s.size()) throw std::invalid_argument("bad size '" + s + "'");
  }
  return v * mul;
}

}  // namespace mf
