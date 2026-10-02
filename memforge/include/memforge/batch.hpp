// Multi-image orchestration: manifests, sharding, output layout, summaries.
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "memforge/engine.hpp"

namespace mf {

constexpr const char* kVersion = "0.1.0";

struct BatchConfig {
  EngineConfig engine;
  std::string out_dir;  // empty: JSONL on stdout
  bool discard = false;  // scan + count only, no findings written
  size_t window = 16;    // images open (and scheduled together) at once
};

// One path per line; blank lines and #-comments ignored.
std::vector<std::string> read_manifest(const std::string& path);
uint64_t file_size_or_zero(const std::string& path);
void make_dirs(const std::string& path);

// Stable, collision-free directory names derived from the image basenames
// (a pure function of the manifest, so every rank derives the same names).
std::vector<std::string> output_names(const std::vector<std::string>& paths);

// Longest-processing-time-first assignment of weighted jobs to `parts`
// bins; deterministic. Returns the bin of each job.
std::vector<int> lpt_assign(const std::vector<uint64_t>& weights, int parts);

// Scans paths[which[k]] for each k. Images that fail to open are reported
// (ImageReport::error) rather than aborting the batch. Writes
// <out_dir>/<name>/<plugin>.jsonl and report.json when out_dir is set.
std::vector<ImageReport> run_batch(const std::vector<std::string>& paths,
                                   const std::vector<std::string>& names,
                                   const std::vector<size_t>& which, const PluginSet& plugins,
                                   const BatchConfig& cfg,
                                   const std::function<void(const ImageReport&)>& on_report = {});

std::string run_summary_json(const std::vector<ImageReport>& reports, double wall_seconds,
                             const std::string& mode, unsigned threads, int ranks);

void write_text_file(const std::string& path, const std::string& text);

}  // namespace mf
