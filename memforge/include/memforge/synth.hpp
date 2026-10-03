// Synthetic physical-memory images with planted artifacts and ground truth.
// Used by tests, benchmarks and `mkimage`. Artifacts are deliberately placed
// across chunk boundaries to exercise the engine's exactness guarantee.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "memforge/image.hpp"

namespace mf {

struct SynthSpec {
  uint64_t size = 64ull << 20;      // bytes of physical memory (page multiple)
  uint64_t seed = 1;
  double zero_fraction = 0.30;      // share of all-zero pages
  double text_fraction = 0.20;      // share of prose-like pages
  uint64_t boundary = 1ull << 20;   // straddle multiples of this
  uint64_t hole_start = 0;          // physical gap (omitted in LiME output)
  uint64_t hole_size = 0;
  int copies = 1;                   // how many times each artifact set is planted
};

struct Planted {
  std::string plugin;
  std::string kind;
  uint64_t offset = 0;
  std::string key;    // field to check, e.g. "url"
  std::string value;  // expected field value
};

struct SynthImage {
  std::vector<uint8_t> phys;  // physical memory [0, size)
  std::vector<Planted> truth;
  SynthSpec spec;
};

SynthImage make_synthetic(const SynthSpec& spec);
// Raw dump: physical memory verbatim (the hole reads as zeros).
std::vector<uint8_t> to_raw(const SynthImage& s);
// LiME capture with the hole omitted (two ranges when a hole is set).
std::vector<uint8_t> to_lime(const SynthImage& s);
std::unique_ptr<MemoryImage> to_memory_image(const SynthImage& s, bool with_hole);
std::string truth_to_json(const SynthImage& s);
// Rules file matching the planted patscan artifacts.
std::string synth_patscan_rules();

}  // namespace mf
