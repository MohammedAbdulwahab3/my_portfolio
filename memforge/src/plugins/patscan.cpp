// Multi-pattern IOC scanner (Aho-Corasick). Rules come from a file
// (patscan.rules=FILE) and/or inline literals (patscan.patterns=a,b,c).
//
// Rule file, one rule per line:
//   <name> text "<literal>" [nocase] [wide] [ascii]
//   <name> hex  "4d 5a 90 00"
// `wide` adds a UTF-16LE variant; `ascii` keeps the narrow one (default
// unless only `wide` is given). Lines starting with # are comments.
#include <cctype>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>

#include "memforge/aho_corasick.hpp"
#include "memforge/plugin.hpp"
#include "plugins.hpp"

namespace mf {
namespace {

struct Variant {
  std::string rule;
  std::string encoding;
};

std::vector<std::string> tokenize(const std::string& line, int lineno) {
  std::vector<std::string> t;
  size_t i = 0;
  while (i < line.size()) {
    if (std::isspace(static_cast<unsigned char>(line[i]))) {
      ++i;
    } else if (line[i] == '"') {
      std::string s;
      ++i;
      while (i < line.size() && line[i] != '"') {
        if (line[i] == '\\' && i + 1 < line.size()) {
          char e = line[++i];
          if (e == 'n') s += '\n';
          else if (e == 't') s += '\t';
          else if (e == 'x' && i + 2 < line.size()) {
            s += static_cast<char>(std::stoi(line.substr(i + 1, 2), nullptr, 16));
            i += 2;
          } else s += e;
        } else {
          s += line[i];
        }
        ++i;
      }
      if (i >= line.size()) throw std::runtime_error("patscan rules line " + std::to_string(lineno) + ": unterminated string");
      ++i;
      t.push_back(s);
    } else {
      size_t j = i;
      while (j < line.size() && !std::isspace(static_cast<unsigned char>(line[j]))) ++j;
      t.push_back(line.substr(i, j - i));
      i = j;
    }
  }
  return t;
}

std::string parse_hex(const std::string& h, int lineno) {
  std::string out, digits;
  for (char c : h)
    if (!std::isspace(static_cast<unsigned char>(c))) digits += c;
  if (digits.empty() || digits.size() % 2)
    throw std::runtime_error("patscan rules line " + std::to_string(lineno) + ": bad hex string");
  for (size_t i = 0; i < digits.size(); i += 2) {
    if (!std::isxdigit(static_cast<unsigned char>(digits[i])) || !std::isxdigit(static_cast<unsigned char>(digits[i + 1])))
      throw std::runtime_error("patscan rules line " + std::to_string(lineno) + ": bad hex digit");
    out += static_cast<char>(std::stoi(digits.substr(i, 2), nullptr, 16));
  }
  return out;
}

std::string widen(const std::string& s) {
  std::string w;
  for (char c : s) {
    w += c;
    w += '\0';
  }
  return w;
}

class PatScanPlugin : public Plugin {
 public:
  const char* name() const override { return "patscan"; }
  const char* description() const override {
    return "Aho-Corasick multi-pattern IOC scan (patscan.rules=FILE, patscan.patterns=a,b)";
  }

  void configure(const Options& o) override {
    std::string rules = o.get("patscan.rules", "");
    if (!rules.empty()) load_rules(rules);
    for (const auto& p : o.get_list("patscan.patterns")) {
      add(exact_, exact_v_, p, {p, "ascii"});
      add(exact_, exact_v_, widen(p), {p, "utf16le"});
    }
    exact_.build();
    nocase_.build();
    halo_ = std::max(exact_.max_len(), nocase_.max_len());
  }

  uint64_t halo() const override { return halo_; }

  void scan(const ScanContext&, const Chunk& c, Sink& sink) const override {
    uint64_t hits = 0;
    auto run = [&](const AhoCorasick& ac, const std::vector<Variant>& vars) {
      ac.scan(c.data, c.avail, c.len, [&](uint32_t pid, size_t start) {
        sink.emit(c.start + start, ac.pattern(pid).size(), "match")
            .str("rule", vars[pid].rule)
            .str("encoding", vars[pid].encoding);
        ++hits;
      });
    };
    run(exact_, exact_v_);
    run(nocase_, nocase_v_);
    sink.count("matches", hits);
  }

 private:
  static void add(AhoCorasick& ac, std::vector<Variant>& v, const std::string& p, Variant var) {
    ac.add(p);
    v.push_back(std::move(var));
  }

  void load_rules(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open patscan rules: " + path);
    std::string line;
    int lineno = 0;
    while (std::getline(in, line)) {
      ++lineno;
      auto t = tokenize(line, lineno);
      if (t.empty() || t[0][0] == '#') continue;
      if (t.size() < 3)
        throw std::runtime_error("patscan rules line " + std::to_string(lineno) + ": expected <name> <text|hex> \"<value>\"");
      const std::string& rule = t[0];
      bool nocase = false, wide = false, ascii = false;
      for (size_t k = 3; k < t.size(); ++k) {
        if (t[k] == "nocase") nocase = true;
        else if (t[k] == "wide") wide = true;
        else if (t[k] == "ascii") ascii = true;
        else throw std::runtime_error("patscan rules line " + std::to_string(lineno) + ": unknown modifier " + t[k]);
      }
      if (!wide) ascii = true;
      if (t[1] == "hex") {
        add(exact_, exact_v_, parse_hex(t[2], lineno), {rule, "hex"});
      } else if (t[1] == "text") {
        if (t[2].empty()) throw std::runtime_error("patscan rules line " + std::to_string(lineno) + ": empty literal");
        AhoCorasick& ac = nocase ? nocase_ : exact_;
        auto& vars = nocase ? nocase_v_ : exact_v_;
        if (ascii) add(ac, vars, t[2], {rule, "ascii"});
        if (wide) add(ac, vars, widen(t[2]), {rule, "utf16le"});
      } else {
        throw std::runtime_error("patscan rules line " + std::to_string(lineno) + ": type must be text or hex");
      }
    }
  }

  AhoCorasick exact_{false}, nocase_{true};
  std::vector<Variant> exact_v_, nocase_v_;
  uint64_t halo_ = 0;
};

}  // namespace

std::unique_ptr<Plugin> make_patscan_plugin() { return std::make_unique<PatScanPlugin>(); }

}  // namespace mf
