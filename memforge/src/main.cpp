// memforge: parallel physical-memory forensics.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <stdexcept>

#include "cli_common.hpp"
#include "memforge/addrspace.hpp"
#include "memforge/batch.hpp"
#include "memforge/engine.hpp"

using namespace mf;
using namespace mf::cli;

namespace {

const char* kUsage = R"(memforge - parallel memory forensics engine

usage: memforge <command> [options]

commands:
  scan      scan memory images (raw or LiME) with one or more plugins
  bench     measure thread scaling on one image
  info      show an image's format and physical ranges
  vtop      translate an x64 virtual address (--dtb, --va)
  plugins   list plugins and their options

scan options:
  -i, --image PATH       image to scan (repeatable; bare paths work too)
  -m, --manifest FILE    file with one image path per line
  -p, --plugins LIST     comma-separated plugin names or "all" (default)
  -j, --threads N        worker threads (default: all cores)
  -c, --chunk-size SIZE  work unit size, e.g. 4M, 16M (default 16M)
  -o, --out DIR          write DIR/<image>/<plugin>.jsonl, report.json and
                         DIR/summary.json (default: JSONL on stdout)
      --opt KEY=VALUE    plugin option, e.g. strings.min=8 (repeatable)
      --shard I/N        only process shard I of N (size-balanced), for
                         Slurm array jobs / k8s indexed jobs without MPI
      --window N         images scheduled together (default 16)
      --no-output        scan and count only
      --summary FILE     write the run summary JSON here
  -q, --quiet            no progress on stderr

bench options: -i IMAGE [-p LIST] [--threads-list 1,2,4,8] [--repeat N]
               [-c SIZE] [--opt K=V] [--no-render]
)";

int cmd_plugins() {
  for (const auto& p : available_plugins())
    std::printf("%-9s %s\n", p.name.c_str(), p.description.c_str());
  return 0;
}

int cmd_info(const ScanArgs& a) {
  for (const auto& path : all_images(a)) {
    auto img = MemoryImage::open(path);
    std::printf("%s\n  format: %s\n  file size: %s\n  physical memory: %s in %zu range(s)\n",
                path.c_str(), format_name(img->format()), human_bytes(img->file_size()).c_str(),
                human_bytes(img->total_bytes()).c_str(), img->ranges().size());
    for (const auto& r : img->ranges())
      std::printf("    [%#014llx - %#014llx)  %s  @file %#llx\n",
                  static_cast<unsigned long long>(r.phys_start),
                  static_cast<unsigned long long>(r.phys_end()), human_bytes(r.size).c_str(),
                  static_cast<unsigned long long>(r.file_offset));
  }
  return 0;
}

int cmd_vtop(const ScanArgs& a) {
  auto paths = all_images(a);
  if (paths.size() != 1 || !a.have_dtb || !a.have_va)
    throw std::invalid_argument("vtop needs exactly one image, --dtb and --va");
  auto img = MemoryImage::open(paths[0]);
  X64AddressSpace as(*img, a.dtb);
  auto pa = as.translate(a.va);
  if (!pa) {
    std::printf("%#llx -> not mapped\n", static_cast<unsigned long long>(a.va));
    return 1;
  }
  std::printf("%#llx -> %#llx\n", static_cast<unsigned long long>(a.va), static_cast<unsigned long long>(*pa));
  uint8_t buf[32];
  as.read(a.va, buf, sizeof buf);
  for (int row = 0; row < 2; ++row) {
    std::printf("  %016llx  ", static_cast<unsigned long long>(a.va + row * 16));
    for (int k = 0; k < 16; ++k) std::printf("%02x ", buf[row * 16 + k]);
    std::printf(" |");
    for (int k = 0; k < 16; ++k) {
      uint8_t c = buf[row * 16 + k];
      std::putchar(c >= 0x20 && c < 0x7f ? c : '.');
    }
    std::printf("|\n");
  }
  return 0;
}

int cmd_scan(const ScanArgs& a) {
  auto paths = all_images(a);
  if (paths.empty()) throw std::invalid_argument("no images given (use -i or -m)");
  auto owned = make_plugins(a);
  PluginSet plugins = raw(owned);
  auto names = output_names(paths);

  std::vector<size_t> which;
  if (a.shards > 1) {
    std::vector<uint64_t> sizes;
    for (const auto& p : paths) sizes.push_back(file_size_or_zero(p));
    auto bins = lpt_assign(sizes, a.shards);
    for (size_t i = 0; i < paths.size(); ++i)
      if (bins[i] == a.shard) which.push_back(i);
  } else {
    for (size_t i = 0; i < paths.size(); ++i) which.push_back(i);
  }

  if (!a.batch.out_dir.empty()) make_dirs(a.batch.out_dir);
  const unsigned threads = resolve_threads(a.batch.engine.threads);
  if (!a.quiet)
    std::fprintf(stderr, "memforge %s: %zu image(s), %zu plugin(s), %u %s thread(s), chunk %s\n",
                 kVersion, which.size(), plugins.size(), threads, parallel_backend(),
                 human_bytes(a.batch.engine.chunk_size).c_str());

  size_t done = 0;
  auto t0 = std::chrono::steady_clock::now();
  auto reports = run_batch(paths, names, which, plugins, a.batch, [&](const ImageReport& r) {
    ++done;
    if (!a.quiet) print_progress(r, done, which.size());
  });
  double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  std::string mode = a.shards > 1 ? "shard " + std::to_string(a.shard) + "/" + std::to_string(a.shards) : "local";
  std::string summary = run_summary_json(reports, wall, mode, threads, 1);

  std::string summary_path = a.summary;
  if (summary_path.empty() && !a.batch.out_dir.empty())
    summary_path = a.batch.out_dir + "/" +
                   (a.shards > 1 ? "summary.shard-" + std::to_string(a.shard) + ".json" : "summary.json");
  if (!summary_path.empty()) write_text_file(summary_path, summary);

  uint64_t bytes = 0;
  size_t failed = 0;
  for (const auto& r : reports) {
    bytes += r.phys_bytes;
    failed += !r.error.empty();
  }
  if (!a.quiet)
    std::fprintf(stderr, "done: %s in %.3f s (%.2f GB/s)%s%s\n", human_bytes(bytes).c_str(), wall,
                 wall > 0 ? bytes / wall / 1e9 : 0.0,
                 failed ? (", " + std::to_string(failed) + " image(s) failed").c_str() : "",
                 summary_path.empty() ? "" : (", summary: " + summary_path).c_str());
  return failed ? 2 : 0;
}

int cmd_bench(const ScanArgs& a) {
  auto paths = all_images(a);
  if (paths.size() != 1) throw std::invalid_argument("bench needs exactly one image");
  auto owned = make_plugins(a);
  PluginSet plugins = raw(owned);
  std::vector<std::string> pnames;
  for (auto* p : plugins) pnames.emplace_back(p->name());
  auto img = MemoryImage::open(paths[0]);
  std::vector<unsigned> tl = a.thread_list;
  if (tl.empty()) {
    unsigned hw = resolve_threads(0);
    for (unsigned t = 1; t < hw; t *= 2) tl.push_back(t);
    tl.push_back(hw);
  }
  FILE* devnull = std::fopen("/dev/null", "w");
  auto run_once = [&](unsigned t) {
    EngineConfig cfg = a.batch.engine;
    cfg.threads = t;
    std::unique_ptr<FindingWriter> w;
    if (a.render)
      w = std::make_unique<JsonlWriter>(devnull, pnames, "");
    else
      w = std::make_unique<NullWriter>();
    auto t0 = std::chrono::steady_clock::now();
    auto reps = scan_images({{img.get(), w.get()}}, plugins, cfg);
    w->finish();
    double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    uint64_t f = 0;
    for (auto n : reps[0].findings) f += n;
    return std::make_pair(s, f);
  };
  std::fprintf(stderr, "bench: %s (%s), plugins: %s, backend %s, warming up...\n", paths[0].c_str(),
               human_bytes(img->total_bytes()).c_str(), a.plugins.size() == 1 ? a.plugins[0].c_str() : "multiple",
               parallel_backend());
  run_once(tl.back());  // warm the page cache
  std::printf("%-8s %-10s %-10s %-9s %-10s %s\n", "threads", "best_s", "GB/s", "speedup", "efficiency", "findings");
  double base = 0;
  for (unsigned t : tl) {
    double best = 1e300;
    uint64_t f = 0;
    for (int r = 0; r < std::max(1, a.repeat); ++r) {
      auto [s, n] = run_once(t);
      best = std::min(best, s);
      f = n;
    }
    if (base == 0) base = best * tl.front();
    double speedup = base / best;
    std::printf("%-8u %-10.4f %-10.3f %-9.2f %-10.2f %llu\n", t, best, img->total_bytes() / best / 1e9,
                speedup, speedup / t, static_cast<unsigned long long>(f));
    std::fflush(stdout);
  }
  std::fclose(devnull);
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2 || !std::strcmp(argv[1], "-h") || !std::strcmp(argv[1], "--help")) {
    std::fputs(kUsage, argc < 2 ? stderr : stdout);
    return argc < 2 ? 1 : 0;
  }
  std::string cmd = argv[1];
  try {
    if (cmd == "plugins") return cmd_plugins();
    if (cmd == "--version" || cmd == "version") {
      std::printf("memforge %s (%s)\n", kVersion, parallel_backend());
      return 0;
    }
    ScanArgs a = parse_args(argc, argv, 2);
    if (cmd == "scan") return cmd_scan(a);
    if (cmd == "bench") return cmd_bench(a);
    if (cmd == "info") return cmd_info(a);
    if (cmd == "vtop") return cmd_vtop(a);
    std::fprintf(stderr, "unknown command '%s'\n\n%s", cmd.c_str(), kUsage);
    return 1;
  } catch (const std::invalid_argument& e) {
    std::fprintf(stderr, "memforge: %s\n", e.what());
    return 1;
  } catch (const std::exception& e) {
    std::fprintf(stderr, "memforge: error: %s\n", e.what());
    return 3;
  }
}
