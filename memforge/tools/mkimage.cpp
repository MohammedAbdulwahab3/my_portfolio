// mkimage: writes a synthetic memory image plus its ground truth.
//   mkimage OUT [--size 256M] [--seed N] [--lime] [--hole-at OFF --hole-size N]
//               [--copies N] [--truth FILE] [--rules FILE]
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include "memforge/batch.hpp"
#include "memforge/options.hpp"
#include "memforge/synth.hpp"

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr,
                 "usage: mkimage OUT [--size 256M] [--seed N] [--lime] [--hole-at OFF --hole-size N]\n"
                 "               [--copies N] [--truth FILE] [--rules FILE]\n");
    return 1;
  }
  try {
    mf::SynthSpec spec;
    std::string out = argv[1], truth, rules;
    bool lime = false;
    for (int i = 2; i < argc; ++i) {
      std::string k = argv[i];
      auto val = [&]() -> std::string {
        if (i + 1 >= argc) throw std::invalid_argument("missing value for " + k);
        return argv[++i];
      };
      if (k == "--size") spec.size = mf::parse_size(val());
      else if (k == "--seed") spec.seed = std::stoull(val());
      else if (k == "--lime") lime = true;
      else if (k == "--hole-at") spec.hole_start = mf::parse_size(val());
      else if (k == "--hole-size") spec.hole_size = mf::parse_size(val());
      else if (k == "--copies") spec.copies = std::stoi(val());
      else if (k == "--truth") truth = val();
      else if (k == "--rules") rules = val();
      else throw std::invalid_argument("unknown option " + k);
    }
    auto img = mf::make_synthetic(spec);
    std::vector<uint8_t> lime_bytes;
    if (lime) lime_bytes = mf::to_lime(img);
    const std::vector<uint8_t>& bytes = lime ? lime_bytes : img.phys;  // raw: no copy
    FILE* f = std::fopen(out.c_str(), "wb");
    if (!f) throw std::runtime_error("cannot write " + out);
    if (std::fwrite(bytes.data(), 1, bytes.size(), f) != bytes.size() || std::fclose(f) != 0)
      throw std::runtime_error("short write to " + out);
    if (!truth.empty()) mf::write_text_file(truth, mf::truth_to_json(img));
    if (!rules.empty()) mf::write_text_file(rules, mf::synth_patscan_rules());
    std::fprintf(stderr, "wrote %s (%s, %zu bytes, %zu planted artifacts)\n", out.c_str(),
                 lime ? "lime" : "raw", bytes.size(), img.truth.size());
    return 0;
  } catch (const std::exception& e) {
    std::fprintf(stderr, "mkimage: %s\n", e.what());
    return 1;
  }
}
