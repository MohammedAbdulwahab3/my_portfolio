// memforge-mpi: distributed memory forensics.
//
//   farm   Many images across nodes. Default is dynamic master/worker: rank 0
//          hands out images largest-first as workers become free (robust to
//          uneven image sizes and node speeds). --static instead assigns
//          images up front with LPT so every rank, including 0, scans.
//   split  One (huge) image across nodes. Every rank plans the same chunk
//          list, scans a contiguous block of it with all its cores, renders
//          output locally, and ships it to rank 0, which applies reducers and
//          writes in address order: output is byte-identical to a 1-node run.
//
// Images must be readable at the same path on every node (shared FS).
#include <mpi.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>

#include "cli_common.hpp"
#include "memforge/batch.hpp"
#include "memforge/engine.hpp"

using namespace mf;
using namespace mf::cli;

namespace {

constexpr int kTagReady = 1;
constexpr int kTagJob = 2;
constexpr int kTagData = 3;
constexpr uint64_t kStop = std::numeric_limits<uint64_t>::max();
constexpr uint64_t kPiece = 1ull << 30;

const char* kUsage = R"(memforge-mpi - distributed memory forensics

usage: mpirun -n N memforge-mpi farm  (-m MANIFEST | -i IMG...) -o DIR [scan options] [--static]
       mpirun -n N memforge-mpi split -i IMG [-o DIR] [scan options]

scan options are those of `memforge scan` (-p, -j, -c, --opt, --no-output, ...).
-j sets threads per rank; run one rank per node (or per NUMA domain) and
let its threads use the whole node: Open MPI binds each rank to ONE core by
default, so pass `--bind-to none` (or `--map-by node:PE=<cores>`).
)";

void send_bytes(const std::vector<uint8_t>& b, int dest, int tag) {
  uint64_t n = b.size();
  MPI_Send(&n, 1, MPI_UINT64_T, dest, tag, MPI_COMM_WORLD);
  for (uint64_t off = 0; off < n; off += kPiece) {
    int len = static_cast<int>(std::min(kPiece, n - off));
    MPI_Send(b.data() + off, len, MPI_BYTE, dest, tag, MPI_COMM_WORLD);
  }
}

std::vector<uint8_t> recv_bytes(int src, int tag, int* actual_src = nullptr) {
  uint64_t n = 0;
  MPI_Status st;
  MPI_Recv(&n, 1, MPI_UINT64_T, src, tag, MPI_COMM_WORLD, &st);
  if (actual_src) *actual_src = st.MPI_SOURCE;
  std::vector<uint8_t> b(n);
  for (uint64_t off = 0; off < n; off += kPiece) {
    int len = static_cast<int>(std::min(kPiece, n - off));
    MPI_Recv(b.data() + off, len, MPI_BYTE, st.MPI_SOURCE, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
  }
  return b;
}

std::vector<uint8_t> encode_reports(const std::vector<ImageReport>& rs) {
  ByteWriter w;
  w.u64(rs.size());
  for (const auto& r : rs) encode_report(w, r);
  return std::move(w.bytes());
}

std::vector<ImageReport> decode_reports(const std::vector<uint8_t>& b) {
  ByteReader rd(b.data(), b.size());
  std::vector<ImageReport> rs(rd.u64());
  for (auto& r : rs) r = decode_report(rd);
  return rs;
}

void finish_summary(const ScanArgs& a, std::vector<ImageReport>& reports, double wall,
                    const std::string& mode, int ranks) {
  unsigned threads = resolve_threads(a.batch.engine.threads);
  std::string s = run_summary_json(reports, wall, mode, threads, ranks);
  std::string path = a.summary;
  if (path.empty() && !a.batch.out_dir.empty()) path = a.batch.out_dir + "/summary.json";
  if (!path.empty()) write_text_file(path, s);
  uint64_t bytes = 0;
  size_t failed = 0;
  for (const auto& r : reports) {
    bytes += r.phys_bytes;
    failed += !r.error.empty();
  }
  if (!a.quiet)
    std::fprintf(stderr, "done: %zu image(s), %s on %d rank(s) x %u thread(s) in %.3f s (%.2f GB/s)%s\n",
                 reports.size(), human_bytes(bytes).c_str(), ranks, threads, wall,
                 wall > 0 ? bytes / wall / 1e9 : 0.0, failed ? ", with failures" : "");
}

int run_farm(const ScanArgs& a, int rank, int size) {
  auto paths = all_images(a);
  if (paths.empty()) throw std::invalid_argument("farm: no images (use -m or -i)");
  if (size > 1 && a.batch.out_dir.empty() && !a.batch.discard)
    throw std::invalid_argument("farm with several ranks needs -o DIR (or --no-output)");
  auto owned = make_plugins(a);
  PluginSet plugins = raw(owned);
  auto names = output_names(paths);
  if (rank == 0 && !a.batch.out_dir.empty()) make_dirs(a.batch.out_dir);
  MPI_Barrier(MPI_COMM_WORLD);
  auto t0 = std::chrono::steady_clock::now();
  std::vector<uint64_t> sizes;
  for (const auto& p : paths) sizes.push_back(file_size_or_zero(p));

  std::vector<ImageReport> reports;
  size_t done = 0;
  auto progress = [&](const ImageReport& r) {
    ++done;
    if (rank == 0 && !a.quiet) print_progress(r, done, paths.size());
  };

  if (size == 1 || a.static_schedule) {
    auto bins = lpt_assign(sizes, size);
    std::vector<size_t> mine;
    for (size_t i = 0; i < paths.size(); ++i)
      if (bins[i] == rank) mine.push_back(i);
    auto local = run_batch(paths, names, mine, plugins, a.batch, [&](const ImageReport& r) {
      if (rank == 0) progress(r);
    });
    for (auto& r : local) r.rank = rank;
    if (rank == 0) {
      reports = std::move(local);
      for (int src = 1; src < size; ++src) {
        auto rs = decode_reports(recv_bytes(src, kTagData));
        for (auto& r : rs) {
          progress(r);
          reports.push_back(std::move(r));
        }
      }
    } else {
      send_bytes(encode_reports(local), 0, kTagData);
    }
  } else if (rank == 0) {
    // Master: largest images first, to whichever worker asks next.
    std::vector<size_t> queue(paths.size());
    for (size_t i = 0; i < queue.size(); ++i) queue[i] = i;
    std::stable_sort(queue.begin(), queue.end(), [&](size_t x, size_t y) { return sizes[x] > sizes[y]; });
    size_t next = 0;
    int active = size - 1;
    while (active > 0) {
      int src = 0;
      auto msg = recv_bytes(MPI_ANY_SOURCE, kTagReady, &src);
      if (!msg.empty())
        for (auto& r : decode_reports(msg)) {
          progress(r);
          reports.push_back(std::move(r));
        }
      uint64_t job = next < queue.size() ? queue[next++] : kStop;
      MPI_Send(&job, 1, MPI_UINT64_T, src, kTagJob, MPI_COMM_WORLD);
      if (job == kStop) --active;
    }
  } else {
    std::vector<ImageReport> last;
    for (;;) {
      send_bytes(last.empty() ? std::vector<uint8_t>() : encode_reports(last), 0, kTagReady);
      uint64_t job = 0;
      MPI_Recv(&job, 1, MPI_UINT64_T, 0, kTagJob, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
      if (job == kStop) break;
      last = run_batch(paths, names, {static_cast<size_t>(job)}, plugins, a.batch);
      for (auto& r : last) r.rank = rank;
    }
  }

  MPI_Barrier(MPI_COMM_WORLD);
  double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  if (rank == 0) {
    std::sort(reports.begin(), reports.end(),
              [](const ImageReport& x, const ImageReport& y) { return x.image < y.image; });
    finish_summary(a, reports, wall, a.static_schedule || size == 1 ? "mpi-farm-static" : "mpi-farm-dynamic",
                   size);
    for (const auto& r : reports)
      if (!r.error.empty()) return 2;
  }
  return 0;
}

int run_split(const ScanArgs& a, int rank, int size) {
  auto paths = all_images(a);
  if (paths.empty()) throw std::invalid_argument("split: no image (use -i)");
  auto owned = make_plugins(a);
  PluginSet plugins = raw(owned);
  std::vector<std::string> pnames;
  for (auto* p : plugins) pnames.emplace_back(p->name());
  auto names = output_names(paths);
  if (rank == 0 && !a.batch.out_dir.empty()) make_dirs(a.batch.out_dir);
  const unsigned threads = resolve_threads(a.batch.engine.threads);
  std::vector<ImageReport> reports;
  MPI_Barrier(MPI_COMM_WORLD);
  auto t_all = std::chrono::steady_clock::now();

  for (size_t j = 0; j < paths.size(); ++j) {
    auto img = MemoryImage::open(paths[j]);
    img->advise_sequential();
    auto chunks = plan_chunks(*img, a.batch.engine.chunk_size, max_halo(plugins));
    const size_t n = chunks.size();
    const size_t first = n * rank / size, last = n * (rank + 1) / size;
    // Render exactly as rank 0's writer will, so workers ship ready bytes.
    const bool per_plugin = !a.batch.out_dir.empty();
    Renderer renderer(pnames, per_plugin ? "" : paths[j], per_plugin);
    MPI_Barrier(MPI_COMM_WORLD);
    auto t0 = std::chrono::steady_clock::now();
    auto local = scan_chunk_range(*img, plugins, chunks, first, last, threads, &renderer, a.batch.discard);

    if (rank != 0) {
      ByteWriter w;
      w.u64(local.size());
      for (const auto& r : local) encode_chunk_result(w, r);
      send_bytes(w.bytes(), 0, kTagData);
      continue;
    }
    std::unique_ptr<FindingWriter> writer;
    if (a.batch.discard) {
      writer = std::make_unique<NullWriter>();
    } else if (per_plugin) {
      std::string dir = a.batch.out_dir + "/" + names[j];
      make_dirs(dir);
      writer = std::make_unique<PerPluginWriter>(dir, pnames);
    } else {
      writer = std::make_unique<JsonlWriter>(stdout, pnames, paths[j]);
    }
    Pipeline pipe(plugins, writer.get());
    for (auto& r : local) pipe.consume(std::move(r));
    local.clear();
    for (int src = 1; src < size; ++src) {
      auto b = recv_bytes(src, kTagData);
      ByteReader rd(b.data(), b.size());
      uint64_t cnt = rd.u64();
      for (uint64_t k = 0; k < cnt; ++k) pipe.consume(decode_chunk_result(rd));
    }
    pipe.finish();
    writer.reset();
    ImageReport r;
    r.image = paths[j];
    r.format = format_name(img->format());
    r.file_size = img->file_size();
    r.phys_bytes = img->total_bytes();
    r.chunks = n;
    r.threads = threads;
    r.wall_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    r.plugins = pnames;
    r.findings = pipe.findings;
    r.plugin_seconds = pipe.plugin_seconds;
    r.counters = pipe.counters;
    if (per_plugin) write_text_file(a.batch.out_dir + "/" + names[j] + "/report.json", report_to_json(r));
    if (!a.quiet) print_progress(r, j + 1, paths.size());
    reports.push_back(std::move(r));
  }
  MPI_Barrier(MPI_COMM_WORLD);
  double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - t_all).count();
  if (rank == 0) finish_summary(a, reports, wall, "mpi-split", size);
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  int provided = 0;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);
  int rank = 0, size = 1;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  int rc = 0;
  if (argc < 2 || (std::strcmp(argv[1], "farm") && std::strcmp(argv[1], "split"))) {
    if (rank == 0) std::fputs(kUsage, stderr);
    MPI_Finalize();
    return 1;
  }
  try {
    ScanArgs a = parse_args(argc, argv, 2);
    if (rank != 0) a.quiet = true;
    rc = !std::strcmp(argv[1], "farm") ? run_farm(a, rank, size) : run_split(a, rank, size);
  } catch (const std::exception& e) {
    std::fprintf(stderr, "memforge-mpi[rank %d]: %s\n", rank, e.what());
    MPI_Abort(MPI_COMM_WORLD, 3);
  }
  MPI_Finalize();
  return rc;
}
