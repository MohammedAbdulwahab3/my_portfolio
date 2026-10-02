#include "memforge/batch.hpp"

#include <sys/stat.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <stdexcept>

namespace mf {

std::vector<std::string> read_manifest(const std::string& path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("cannot open manifest " + path);
  std::vector<std::string> out;
  std::string line;
  while (std::getline(in, line)) {
    size_t a = line.find_first_not_of(" \t\r");
    if (a == std::string::npos || line[a] == '#') continue;
    size_t b = line.find_last_not_of(" \t\r");
    out.push_back(line.substr(a, b - a + 1));
  }
  return out;
}

uint64_t file_size_or_zero(const std::string& path) {
  struct stat st {};
  return ::stat(path.c_str(), &st) == 0 ? static_cast<uint64_t>(st.st_size) : 0;
}

void make_dirs(const std::string& path) {
  std::string cur;
  for (size_t i = 0; i <= path.size(); ++i) {
    if (i == path.size() || path[i] == '/') {
      if (!cur.empty() && ::mkdir(cur.c_str(), 0755) != 0 && errno != EEXIST)
        throw std::runtime_error("mkdir " + cur + ": " + std::strerror(errno));
    }
    if (i < path.size()) cur += path[i];
  }
}

std::vector<std::string> output_names(const std::vector<std::string>& paths) {
  std::vector<std::string> out;
  std::set<std::string> used;
  for (const auto& p : paths) {
    std::string base = p.substr(p.find_last_of('/') == std::string::npos ? 0 : p.find_last_of('/') + 1);
    std::string clean;
    for (char c : base)
      clean += (std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '-' || c == '_') ? c : '_';
    if (clean.empty() || clean == "." || clean == "..") clean = "image";
    std::string name = clean;
    for (int k = 2; used.count(name); ++k) name = clean + "-" + std::to_string(k);
    used.insert(name);
    out.push_back(name);
  }
  return out;
}

std::vector<int> lpt_assign(const std::vector<uint64_t>& weights, int parts) {
  if (parts < 1) throw std::invalid_argument("parts must be >= 1");
  std::vector<size_t> order(weights.size());
  for (size_t i = 0; i < order.size(); ++i) order[i] = i;
  std::stable_sort(order.begin(), order.end(),
                   [&](size_t a, size_t b) { return weights[a] > weights[b]; });
  using Bin = std::pair<uint64_t, int>;  // (load, bin) - min-heap, ties to lowest bin
  std::priority_queue<Bin, std::vector<Bin>, std::greater<Bin>> heap;
  for (int b = 0; b < parts; ++b) heap.push({0, b});
  std::vector<int> out(weights.size(), 0);
  for (size_t j : order) {
    Bin b = heap.top();
    heap.pop();
    out[j] = b.second;
    heap.push({b.first + std::max<uint64_t>(weights[j], 1), b.second});
  }
  return out;
}

void write_text_file(const std::string& path, const std::string& text) {
  FILE* f = std::fopen(path.c_str(), "w");
  if (!f) throw std::runtime_error("cannot write " + path + ": " + std::strerror(errno));
  std::fwrite(text.data(), 1, text.size(), f);
  std::fputc('\n', f);
  std::fclose(f);
}

std::vector<ImageReport> run_batch(const std::vector<std::string>& paths,
                                   const std::vector<std::string>& names,
                                   const std::vector<size_t>& which, const PluginSet& plugins,
                                   const BatchConfig& cfg,
                                   const std::function<void(const ImageReport&)>& on_report) {
  std::vector<std::string> pnames;
  for (const Plugin* p : plugins) pnames.emplace_back(p->name());
  std::vector<ImageReport> all;
  const size_t window = std::max<size_t>(1, cfg.window);

  for (size_t w0 = 0; w0 < which.size(); w0 += window) {
    const size_t w1 = std::min(which.size(), w0 + window);
    std::vector<std::unique_ptr<MemoryImage>> images;
    std::vector<std::unique_ptr<FindingWriter>> writers;
    std::vector<ImageTask> tasks;
    std::vector<size_t> task_job;  // which[] position for each task
    std::vector<ImageReport> failed;

    for (size_t k = w0; k < w1; ++k) {
      const size_t j = which[k];
      try {
        auto img = MemoryImage::open(paths[j]);
        std::unique_ptr<FindingWriter> wr;
        if (cfg.discard) {
          wr = std::make_unique<NullWriter>();
        } else if (cfg.out_dir.empty()) {
          wr = std::make_unique<JsonlWriter>(stdout, pnames, paths[j]);
        } else {
          std::string dir = cfg.out_dir + "/" + names[j];
          make_dirs(dir);
          wr = std::make_unique<PerPluginWriter>(dir, pnames);
        }
        tasks.push_back({img.get(), wr.get()});
        task_job.push_back(j);
        images.push_back(std::move(img));
        writers.push_back(std::move(wr));
      } catch (const std::exception& e) {
        ImageReport r;
        r.image = paths[j];
        r.error = e.what();
        r.plugins = pnames;
        r.findings.assign(pnames.size(), 0);
        r.plugin_seconds.assign(pnames.size(), 0);
        failed.push_back(std::move(r));
      }
    }

    auto reports = scan_images(tasks, plugins, cfg.engine);
    writers.clear();  // flush + close output before reporting completion
    for (size_t t = 0; t < reports.size(); ++t) {
      if (!cfg.out_dir.empty() && !cfg.discard)
        write_text_file(cfg.out_dir + "/" + names[task_job[t]] + "/report.json",
                        report_to_json(reports[t]));
      if (on_report) on_report(reports[t]);
      all.push_back(std::move(reports[t]));
    }
    for (auto& r : failed) {
      if (on_report) on_report(r);
      all.push_back(std::move(r));
    }
  }
  return all;
}

std::string run_summary_json(const std::vector<ImageReport>& reports, double wall_seconds,
                             const std::string& mode, unsigned threads, int ranks) {
  uint64_t bytes = 0, failed = 0;
  std::map<std::string, uint64_t> by_plugin;
  Counters counters;
  for (const auto& r : reports) {
    if (!r.error.empty()) {
      ++failed;
      continue;
    }
    bytes += r.phys_bytes;
    for (size_t i = 0; i < r.plugins.size(); ++i) by_plugin[r.plugins[i]] += r.findings[i];
    merge_counters(counters, r.counters);
  }
  char b[64];
  std::string o = "{\"tool\":\"memforge\",\"version\":\"" + std::string(kVersion) + "\"";
  o += ",\"mode\":" + json_quote(mode);
  o += ",\"backend\":" + json_quote(parallel_backend());
  o += ",\"threads_per_rank\":" + std::to_string(threads);
  o += ",\"ranks\":" + std::to_string(ranks);
  o += ",\"images\":" + std::to_string(reports.size());
  o += ",\"failed\":" + std::to_string(failed);
  o += ",\"total_bytes\":" + std::to_string(bytes);
  std::snprintf(b, sizeof b, "%.6f", wall_seconds);
  o += ",\"wall_seconds\":";
  o += b;
  std::snprintf(b, sizeof b, "%.4f", wall_seconds > 0 ? bytes / wall_seconds / 1e9 : 0.0);
  o += ",\"aggregate_gbps\":";
  o += b;
  o += ",\"findings\":{";
  bool first = true;
  for (const auto& [k, v] : by_plugin) {
    if (!first) o += ',';
    first = false;
    o += json_quote(k) + ":" + std::to_string(v);
  }
  o += "},\"counters\":{";
  first = true;
  for (const auto& [k, v] : counters) {
    if (!first) o += ',';
    first = false;
    o += json_quote(k) + ":" + std::to_string(v);
  }
  o += "},\"reports\":[";
  for (size_t i = 0; i < reports.size(); ++i) {
    if (i) o += ',';
    o += report_to_json(reports[i]);
  }
  o += "]}";
  return o;
}

}  // namespace mf
