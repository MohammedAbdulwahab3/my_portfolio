// Aho-Corasick multi-pattern matcher compiled to a dense DFA over a
// compressed alphabet (only bytes that occur in patterns get their own
// class), so scanning is one table lookup per input byte.
#pragma once

#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace mf {

class AhoCorasick {
 public:
  explicit AhoCorasick(bool nocase = false) : nocase_(nocase) {}
  // Returns the pattern id.
  uint32_t add(const std::string& pattern);
  void build();
  bool empty() const { return patterns_.empty(); }
  size_t size() const { return patterns_.size(); }
  size_t max_len() const { return max_len_; }
  size_t states() const { return out_head_.size(); }
  const std::string& pattern(uint32_t id) const { return patterns_[id]; }

  // Calls cb(pattern_id, start_pos) for every match whose start lies in
  // [0, own_len); text may extend past own_len so matches can finish.
  template <typename CB>
  void scan(const uint8_t* text, size_t text_len, size_t own_len, CB&& cb) const {
    if (patterns_.empty() || own_len == 0) return;
    // A table DFA is bound by the latency of its dependent loads, so the
    // owned range is split into kLanes independent sub-ranges whose state
    // machines advance in lockstep (ILP). Each lane follows the same
    // ownership rule as chunks: it reports matches starting in its own part
    // and reads up to max_len-1 bytes past it, so results are exact.
    constexpr int kLanes = 4;
    const uint32_t* delta = delta_.data();
    const uint8_t* cls = cls_;
    const size_t step = (own_len + kLanes - 1) / kLanes;
    size_t pos[kLanes], end[kLanes], own_end[kLanes];
    uint32_t st[kLanes];
    size_t common = SIZE_MAX;
    for (int k = 0; k < kLanes; ++k) {
      pos[k] = std::min(own_len, k * step);
      own_end[k] = std::min(own_len, (k + 1) * step);
      end[k] = pos[k] < own_end[k] ? std::min(text_len, own_end[k] + max_len_ - 1) : pos[k];
      st[k] = 0;
      common = std::min(common, end[k] - pos[k]);
    }
    auto report = [&](int k, size_t i) {
      for (int32_t o = out_head_[st[k] / nclasses_]; o >= 0; o = out_next_[o]) {
        uint32_t pid = out_pid_[o];
        size_t start = i + 1 - patterns_[pid].size();
        if (start < own_end[k]) cb(pid, start);
      }
    };
    for (size_t i = 0; i < common; ++i) {
      for (int k = 0; k < kLanes; ++k) st[k] = delta[st[k] + cls[text[pos[k] + i]]];
      for (int k = 0; k < kLanes; ++k)
        if (__builtin_expect(st[k] >= out_min_, 0)) report(k, pos[k] + i);
    }
    for (int k = 0; k < kLanes; ++k)
      for (size_t i = pos[k] + common; i < end[k]; ++i) {
        st[k] = delta[st[k] + cls[text[i]]];
        if (st[k] >= out_min_) report(k, i);
      }
  }

 private:
  bool nocase_;
  bool built_ = false;
  std::vector<std::string> patterns_;
  size_t max_len_ = 0;
  uint8_t cls_[256] = {};
  uint32_t nclasses_ = 1;
  // [state*nclasses + cls] -> next*nclasses. States are numbered so that
  // exactly those >= out_min_ (premultiplied) have outputs.
  std::vector<uint32_t> delta_;
  uint32_t out_min_ = 0;
  std::vector<int32_t> out_head_;   // per state, index into out_* or -1
  std::vector<int32_t> out_next_;
  std::vector<uint32_t> out_pid_;
};

}  // namespace mf
