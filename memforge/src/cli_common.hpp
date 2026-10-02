// Argument parsing shared by memforge and memforge-mpi.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "memforge/batch.hpp"
#include "memforge/options.hpp"
#include "memforge/plugin.hpp"

namespace mf::cli {

struct ScanArgs {
  std::vector<std::string> images;
  std::string manifest;
  std::vector<std::string> plugins{"all"};
  Options opts;
  BatchConfig batch;
  int shard = 0, shards = 1;
  std::string summary;
  bool quiet = false;
  bool static_schedule = false;  // MPI farm: LPT up front instead of master/worker
  std::vector<unsigned> thread_list;  // bench
  int repeat = 3;                     // bench
  bool render = true;                 // bench
  uint64_t dtb = 0, va = 0;           // vtop
  bool have_dtb = false, have_va = false;
};

// Parses argv[first..]; throws std::invalid_argument on bad usage.
ScanArgs parse_args(int argc, char** argv, int first);
std::vector<std::string> all_images(const ScanArgs& a);
std::vector<std::unique_ptr<Plugin>> make_plugins(const ScanArgs& a);
PluginSet raw(const std::vector<std::unique_ptr<Plugin>>& v);
std::string human_bytes(uint64_t n);
void print_progress(const ImageReport& r, size_t done, size_t total);

}  // namespace mf::cli
