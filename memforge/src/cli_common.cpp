#include "cli_common.hpp"

#include <cstdio>
#include <stdexcept>

namespace mf::cli {

namespace {
uint64_t parse_u64(const std::string& s) {
  size_t pos = 0;
  uint64_t v = std::stoull(s, &pos, 0);
  if (pos != s.size()) throw std::invalid_argument("not a number: " + s);
  return v;
}
}  // namespace

ScanArgs parse_args(int argc, char** argv, int first) {
  ScanArgs a;
  auto need = [&](int& i) -> std::string {
    if (i + 1 >= argc) throw std::invalid_argument(std::string("missing value for ") + argv[i]);
    return argv[++i];
  };
  for (int i = first; i < argc; ++i) {
    std::string k = argv[i];
    if (k == "-i" || k == "--image") a.images.push_back(need(i));
    else if (k == "-m" || k == "--manifest") a.manifest = need(i);
    else if (k == "-p" || k == "--plugins") a.plugins = split(need(i), ',');
    else if (k == "-j" || k == "--threads") a.batch.engine.threads = static_cast<unsigned>(parse_u64(need(i)));
    else if (k == "-c" || k == "--chunk-size") a.batch.engine.chunk_size = parse_size(need(i));
    else if (k == "-o" || k == "--out") a.batch.out_dir = need(i);
    else if (k == "--opt") a.opts.parse(need(i));
    else if (k == "--window") a.batch.window = parse_u64(need(i));
    else if (k == "--no-output") a.batch.discard = true;
    else if (k == "--summary") a.summary = need(i);
    else if (k == "-q" || k == "--quiet") a.quiet = true;
    else if (k == "--static") a.static_schedule = true;
    else if (k == "--repeat") a.repeat = static_cast<int>(parse_u64(need(i)));
    else if (k == "--no-render") a.render = false;
    else if (k == "--dtb") { a.dtb = parse_u64(need(i)); a.have_dtb = true; }
    else if (k == "--va") { a.va = parse_u64(need(i)); a.have_va = true; }
    else if (k == "--threads-list") {
      for (const auto& t : split(need(i), ',')) a.thread_list.push_back(static_cast<unsigned>(parse_u64(t)));
    } else if (k == "--shard") {
      std::string v = need(i);
      auto slash = v.find('/');
      if (slash == std::string::npos) throw std::invalid_argument("--shard expects I/N");
      a.shard = static_cast<int>(parse_u64(v.substr(0, slash)));
      a.shards = static_cast<int>(parse_u64(v.substr(slash + 1)));
      if (a.shards < 1 || a.shard < 0 || a.shard >= a.shards) throw std::invalid_argument("bad --shard " + v);
    } else if (!k.empty() && k[0] != '-') {
      a.images.push_back(k);
    } else {
      throw std::invalid_argument("unknown option " + k);
    }
  }
  return a;
}

std::vector<std::string> all_images(const ScanArgs& a) {
  std::vector<std::string> v = a.images;
  if (!a.manifest.empty())
    for (auto& p : read_manifest(a.manifest)) v.push_back(p);
  return v;
}

std::vector<std::unique_ptr<Plugin>> make_plugins(const ScanArgs& a) {
  return create_plugins(a.plugins, a.opts);
}

PluginSet raw(const std::vector<std::unique_ptr<Plugin>>& v) {
  PluginSet s;
  for (const auto& p : v) s.push_back(p.get());
  return s;
}

std::string human_bytes(uint64_t n) {
  const char* u[] = {"B", "KiB", "MiB", "GiB", "TiB"};
  double d = static_cast<double>(n);
  int k = 0;
  while (d >= 1024 && k < 4) {
    d /= 1024;
    ++k;
  }
  char b[32];
  std::snprintf(b, sizeof b, "%.2f %s", d, u[k]);
  return b;
}

void print_progress(const ImageReport& r, size_t done, size_t total) {
  if (!r.error.empty()) {
    std::fprintf(stderr, "[%zu/%zu] %s: ERROR %s\n", done, total, r.image.c_str(), r.error.c_str());
    return;
  }
  uint64_t f = 0;
  for (auto n : r.findings) f += n;
  std::fprintf(stderr, "[%zu/%zu] %s  %s  %.3f s  %.2f GB/s  %llu findings%s\n", done, total,
               r.image.c_str(), human_bytes(r.phys_bytes).c_str(), r.wall_seconds,
               r.wall_seconds > 0 ? r.phys_bytes / r.wall_seconds / 1e9 : 0.0,
               static_cast<unsigned long long>(f),
               r.rank >= 0 ? (" (rank " + std::to_string(r.rank) + ")").c_str() : "");
}

}  // namespace mf::cli
