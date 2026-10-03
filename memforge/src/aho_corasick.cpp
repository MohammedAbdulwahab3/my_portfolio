#include "memforge/aho_corasick.hpp"

#include <cctype>
#include <queue>
#include <stdexcept>

namespace mf {

uint32_t AhoCorasick::add(const std::string& pattern) {
  if (built_) throw std::logic_error("AhoCorasick::add after build");
  if (pattern.empty()) throw std::invalid_argument("empty pattern");
  std::string p = pattern;
  if (nocase_)
    for (auto& c : p) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  patterns_.push_back(p);
  max_len_ = std::max(max_len_, p.size());
  return static_cast<uint32_t>(patterns_.size() - 1);
}

void AhoCorasick::build() {
  // Alphabet compression. Class 0 = "byte never appears in a pattern".
  bool used[256] = {};
  for (const auto& p : patterns_)
    for (unsigned char c : p) used[c] = true;
  nclasses_ = 1;
  for (int b = 0; b < 256; ++b)
    if (used[b]) cls_[b] = static_cast<uint8_t>(nclasses_++);
  if (nocase_)
    for (int b = 'A'; b <= 'Z'; ++b) cls_[b] = cls_[b - 'A' + 'a'];
  if (nclasses_ > 256) throw std::logic_error("alphabet overflow");

  // Trie (goto function) with -1 = missing edge.
  std::vector<std::vector<int32_t>> go(1, std::vector<int32_t>(nclasses_, -1));
  std::vector<std::vector<uint32_t>> outs(1);
  for (uint32_t pid = 0; pid < patterns_.size(); ++pid) {
    int32_t s = 0;
    for (unsigned char c : patterns_[pid]) {
      uint8_t k = cls_[c];
      if (go[s][k] < 0) {
        go[s][k] = static_cast<int32_t>(go.size());
        go.emplace_back(nclasses_, -1);
        outs.emplace_back();
      }
      s = go[s][k];
    }
    outs[s].push_back(pid);
  }

  // BFS: failure links folded into a full DFA transition table; outputs
  // are chained through dictionary-suffix links.
  const size_t n = go.size();
  std::vector<int32_t> fail(n, 0);
  delta_.assign(n * nclasses_, 0);
  out_head_.assign(n, -1);
  std::vector<int32_t> dict(n, -1);  // nearest proper suffix state with output
  std::queue<int32_t> q;
  for (uint32_t k = 0; k < nclasses_; ++k) {
    int32_t t = go[0][k];
    if (t > 0) {
      fail[t] = 0;
      delta_[k] = static_cast<uint32_t>(t);
      q.push(t);
    }
  }
  std::vector<int32_t> order;
  while (!q.empty()) {
    int32_t s = q.front();
    q.pop();
    order.push_back(s);
    int32_t f = fail[s];
    dict[s] = outs[f].empty() ? dict[f] : f;
    for (uint32_t k = 0; k < nclasses_; ++k) {
      int32_t t = go[s][k];
      if (t >= 0) {
        fail[t] = static_cast<int32_t>(delta_[static_cast<size_t>(f) * nclasses_ + k]);
        delta_[static_cast<size_t>(s) * nclasses_ + k] = static_cast<uint32_t>(t);
        q.push(t);
      } else {
        delta_[static_cast<size_t>(s) * nclasses_ + k] = delta_[static_cast<size_t>(f) * nclasses_ + k];
      }
    }
  }
  // Materialize output lists: own outputs followed by those of dict links.
  for (size_t s = 1; s < n; ++s) {
    int32_t head = -1;
    std::vector<uint32_t> all;
    for (int32_t t = static_cast<int32_t>(s); t > 0; t = dict[t])
      for (uint32_t pid : outs[t]) all.push_back(pid);
    for (auto it = all.rbegin(); it != all.rend(); ++it) {
      out_pid_.push_back(*it);
      out_next_.push_back(head);
      head = static_cast<int32_t>(out_pid_.size() - 1);
    }
    out_head_[s] = head;
  }
  // Renumber: states without output first (root stays 0), then output
  // states, so "has output" is one compare off the critical path. Then
  // premultiply targets by the row width.
  std::vector<uint32_t> id(n);
  uint32_t next = 0;
  for (size_t s = 0; s < n; ++s)
    if (out_head_[s] < 0) id[s] = next++;
  out_min_ = next;
  for (size_t s = 0; s < n; ++s)
    if (out_head_[s] >= 0) id[s] = next++;
  if (static_cast<uint64_t>(n) * nclasses_ >= (1ull << 32)) throw std::length_error("pattern set too large");
  std::vector<uint32_t> nd(n * nclasses_);
  std::vector<int32_t> nh(n);
  for (size_t s = 0; s < n; ++s) {
    for (uint32_t k = 0; k < nclasses_; ++k)
      nd[static_cast<size_t>(id[s]) * nclasses_ + k] = id[delta_[s * nclasses_ + k]] * nclasses_;
    nh[id[s]] = out_head_[s];
  }
  delta_ = std::move(nd);
  out_head_ = std::move(nh);
  out_min_ *= nclasses_;
  built_ = true;
}

}  // namespace mf
