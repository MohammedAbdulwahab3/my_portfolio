// memforge test suite (no external framework).
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "memforge/addrspace.hpp"
#include "memforge/aho_corasick.hpp"
#include "memforge/batch.hpp"
#include "memforge/engine.hpp"
#include "memforge/synth.hpp"

using namespace mf;

namespace {

int g_failed = 0, g_checks = 0;

#define CHECK(cond)                                                              \
  do {                                                                           \
    ++g_checks;                                                                  \
    if (!(cond)) {                                                               \
      ++g_failed;                                                                \
      std::fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);    \
    }                                                                            \
  } while (0)

#define CHECK_EQ(a, b)                                                                     \
  do {                                                                                     \
    ++g_checks;                                                                            \
    auto va_ = (a);                                                                        \
    auto vb_ = (b);                                                                        \
    if (!(va_ == vb_)) {                                                                   \
      ++g_failed;                                                                          \
      std::ostringstream os_;                                                              \
      os_ << va_ << " != " << vb_;                                                         \
      std::fprintf(stderr, "  FAIL %s:%d: %s == %s (%s)\n", __FILE__, __LINE__, #a, #b,   \
                   os_.str().c_str());                                                     \
    }                                                                                      \
  } while (0)

std::string tmp_path(const std::string& name) {
  const char* d = std::getenv("TMPDIR");
  return std::string(d ? d : "/tmp") + "/memforge-test-" + std::to_string(::getpid()) + "-" + name;
}

void write_file(const std::string& p, const std::vector<uint8_t>& b) {
  std::ofstream o(p, std::ios::binary);
  o.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
}

std::string read_file(const std::string& p) {
  std::ifstream in(p, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

std::vector<std::string> plugin_names(const PluginSet& ps) {
  std::vector<std::string> n;
  for (auto* p : ps) n.emplace_back(p->name());
  return n;
}

std::string field_text(const Field& f) {
  switch (f.type) {
    case Field::Type::Str: return f.s;
    case Field::Type::Hex: return hex_str(f.u);
    case Field::Type::Bool: return f.u ? "true" : "false";
    case Field::Type::Float: return std::to_string(f.d);
    default: return std::to_string(f.u);
  }
}

// Scan into structured findings.
std::vector<Finding> scan_vec(const MemoryImage& img, const PluginSet& ps, uint64_t chunk, unsigned threads) {
  VectorWriter w;
  EngineConfig cfg;
  cfg.chunk_size = chunk;
  cfg.threads = threads;
  scan_images({{&img, &w}}, ps, cfg);
  return w.findings;
}

std::vector<std::string> as_lines(const std::vector<Finding>& fs, const PluginSet& ps) {
  auto names = plugin_names(ps);
  std::vector<std::string> out;
  for (const auto& f : fs) out.push_back(finding_to_json(f, names[f.plugin], ""));
  return out;
}

struct Fixture {
  SynthImage synth;
  std::string rules_path;
  Options opts;
  std::vector<std::unique_ptr<Plugin>> owned;
  PluginSet ps;

  explicit Fixture(uint64_t size = 24ull << 20, int copies = 2, uint64_t hole_at = 0, uint64_t hole = 0) {
    SynthSpec spec;
    spec.size = size;
    spec.seed = 7;
    spec.copies = copies;
    spec.hole_start = hole_at;
    spec.hole_size = hole;
    synth = make_synthetic(spec);
    rules_path = tmp_path("rules.txt");
    write_text_file(rules_path, synth_patscan_rules());
    opts.set("patscan.rules", rules_path);
    owned = create_plugins({"all"}, opts);
    for (auto& p : owned) ps.push_back(p.get());
  }
  ~Fixture() { std::remove(rules_path.c_str()); }
};

// ---------------------------------------------------------------------------

void test_options() {
  CHECK_EQ(parse_size("4096"), 4096ull);
  CHECK_EQ(parse_size("16M"), 16ull << 20);
  CHECK_EQ(parse_size("2GiB"), 2ull << 30);
  CHECK_EQ(parse_size("0x1000"), 4096ull);
  Options o;
  o.parse("strings.min=8");
  CHECK_EQ(o.get_int("strings.min", 0), 8);
  CHECK(o.get_bool("missing", true));
  bool threw = false;
  try {
    o.parse("novalue");
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  CHECK(threw);
}

void test_aho_corasick() {
  std::mt19937_64 rng(42);
  for (int round = 0; round < 40; ++round) {
    bool nocase = round % 2;
    AhoCorasick ac(nocase);
    std::vector<std::string> pats;
    int np = 1 + static_cast<int>(rng() % 12);
    for (int k = 0; k < np; ++k) {
      std::string p;
      int len = 1 + static_cast<int>(rng() % 5);
      for (int i = 0; i < len; ++i) p += "abAB\x00\xff"[rng() % 6];
      pats.push_back(p);
      ac.add(p);
    }
    ac.build();
    std::string text;
    for (int i = 0; i < 3000; ++i) text += "abAB\x00\xffz"[rng() % 7];
    auto fold = [&](char c) {
      return nocase ? static_cast<char>(std::tolower(static_cast<unsigned char>(c))) : c;
    };
    std::vector<std::pair<uint32_t, size_t>> want, got;
    size_t own = 2000;
    for (uint32_t pid = 0; pid < pats.size(); ++pid)
      for (size_t s = 0; s < own && s + pats[pid].size() <= text.size(); ++s) {
        bool m = true;
        for (size_t k = 0; k < pats[pid].size() && m; ++k) m = fold(text[s + k]) == fold(pats[pid][k]);
        if (m) want.push_back({pid, s});
      }
    ac.scan(reinterpret_cast<const uint8_t*>(text.data()), text.size(), own,
            [&](uint32_t pid, size_t s) { got.push_back({pid, s}); });
    std::sort(want.begin(), want.end());
    std::sort(got.begin(), got.end());
    CHECK(want == got);
  }
}

void test_lime_and_ranges() {
  SynthSpec spec;
  spec.size = 8ull << 20;
  spec.hole_start = 3ull << 20;
  spec.hole_size = 1ull << 20;
  auto s = make_synthetic(spec);
  auto path = tmp_path("img.lime");
  write_file(path, to_lime(s));
  auto img = MemoryImage::open(path);
  CHECK(img->format() == ImageFormat::LiME);
  CHECK_EQ(img->ranges().size(), size_t(2));
  CHECK_EQ(img->total_bytes(), uint64_t(7ull << 20));
  CHECK_EQ(img->max_phys(), uint64_t(8ull << 20));
  CHECK(img->ptr((3ull << 20) + 5, 1) == nullptr);       // in the hole
  CHECK(img->ptr((3ull << 20) - 2, 4) == nullptr);       // crosses into the hole
  const uint8_t* p = img->ptr(5ull << 20, 4096);
  CHECK(p && std::memcmp(p, s.phys.data() + (5ull << 20), 4096) == 0);
  std::vector<uint8_t> buf(8192);
  img->read((3ull << 20) - 4096, buf.data(), buf.size());
  CHECK(std::memcmp(buf.data(), s.phys.data() + (3ull << 20) - 4096, 4096) == 0);
  CHECK(std::all_of(buf.begin() + 4096, buf.end(), [](uint8_t b) { return b == 0; }));
  std::remove(path.c_str());

  // Truncated LiME must be rejected, not misread.
  auto bytes = to_lime(s);
  bytes.resize(bytes.size() - 100);
  write_file(path, bytes);
  bool threw = false;
  try {
    MemoryImage::open(path);
  } catch (const std::runtime_error&) {
    threw = true;
  }
  CHECK(threw);
  std::remove(path.c_str());
}

void test_address_translation() {
  std::vector<uint8_t> m(64ull << 20, 0);
  auto put = [&](uint64_t off, uint64_t v) { std::memcpy(m.data() + off, &v, 8); };
  const uint64_t pml4 = 0x1000, pdpt = 0x2000, pd = 0x3000, pt = 0x4000;
  // 4 KiB page: VA 0xFFFFF80000123000 -> 0x9000
  uint64_t va = 0xFFFFF80000123000ull;
  put(pml4 + ((va >> 39) & 511) * 8, pdpt | 3);
  put(pdpt + ((va >> 30) & 511) * 8, pd | 3);
  put(pd + ((va >> 21) & 511) * 8, pt | 3);
  put(pt + ((va >> 12) & 511) * 8, 0x9000 | 3);
  std::memcpy(m.data() + 0x9000 + 0x45, "hello", 5);
  // 2 MiB page: VA 0x40000000 -> 0x600000
  put(pml4 + 0, 0x5000 | 7);
  put(0x5000 + 1 * 8, 0x6000 | 7);
  put(0x6000 + 0, 0x600000 | 0x87);
  // 1 GiB page: VA 0x8000000000 (pml4 slot 1) -> 0x0
  put(pml4 + 8, 0x7000 | 7);
  put(0x7000, 0x0 | 0x87);
  auto img = MemoryImage::from_buffer(std::move(m));
  X64AddressSpace as(*img, pml4);
  CHECK(as.translate(va + 0x45) == std::optional<uint64_t>(0x9045));
  char buf[6] = {};
  as.read(va + 0x45, buf, 5);
  CHECK(std::string(buf) == "hello");
  CHECK(as.translate(0x40000000ull + 0x12345) == std::optional<uint64_t>(0x612345));
  CHECK(as.translate(0x8000000000ull + 0x1234) == std::optional<uint64_t>(0x1234));
  CHECK(!as.translate(0x0000123400000000ull).has_value());   // not present
  CHECK(!as.translate(0x0000900000000000ull).has_value());   // non-canonical? no: unmapped
  CHECK(!as.translate(0x0001000000000000ull).has_value());   // non-canonical
  CHECK_EQ(selfmap_pml4_va(0x1ED), 0xFFFFF6FB7DBED000ull);   // classic Windows value
}

bool find_truth(const std::vector<Finding>& fs, const PluginSet& ps, const Planted& t) {
  auto names = plugin_names(ps);
  for (const auto& f : fs) {
    if (f.offset != t.offset || names[f.plugin] != t.plugin || f.kind != t.kind) continue;
    const Field* fld = f.get(t.key);
    if (fld && field_text(*fld) == t.value) return true;
  }
  return false;
}

void test_ground_truth(bool lime_hole) {
  Fixture fx(24ull << 20, 2, lime_hole ? (7ull << 20) : 0, lime_hole ? (2ull << 20) : 0);
  auto img = to_memory_image(fx.synth, lime_hole);
  auto fs = scan_vec(*img, fx.ps, 1ull << 20, 4);
  size_t found = 0;
  for (const auto& t : fx.synth.truth) {
    bool ok = find_truth(fs, fx.ps, t);
    if (!ok)
      std::fprintf(stderr, "  missing %s/%s at %#llx (%s=%s)\n", t.plugin.c_str(), t.kind.c_str(),
                   static_cast<unsigned long long>(t.offset), t.key.c_str(), t.value.c_str());
    found += ok;
  }
  CHECK_EQ(found, fx.synth.truth.size());
  // No false positives for the structural plugins: every dtbscan / pescan /
  // pooltag / entropy finding must be a planted one.
  auto names = plugin_names(fx.ps);
  for (const auto& f : fs) {
    const std::string& n = names[f.plugin];
    if (n != "dtbscan" && n != "pescan" && n != "pooltag" && n != "entropy") continue;
    bool planted = false;
    for (const auto& t : fx.synth.truth) planted |= t.plugin == n && t.offset == f.offset && t.kind == f.kind;
    if (!planted)
      std::fprintf(stderr, "  unexpected %s/%s at %#llx\n", n.c_str(), f.kind.c_str(),
                   static_cast<unsigned long long>(f.offset));
    CHECK(planted);
  }
}

// The central property: chunked, parallel scanning gives exactly the result
// of one serial pass over the whole image.
void test_exactness() {
  Fixture fx(24ull << 20, 2);
  auto img = to_memory_image(fx.synth, false);
  auto ref = as_lines(scan_vec(*img, fx.ps, 1ull << 40, 1), fx.ps);
  CHECK(ref.size() > 1000);
  for (uint64_t chunk : {64ull << 10, 300ull << 10, 1ull << 20, 5ull << 20})
    for (unsigned t : {1u, 3u, 8u}) {
      auto got = as_lines(scan_vec(*img, fx.ps, chunk, t), fx.ps);
      if (got != ref)
        std::fprintf(stderr, "  mismatch chunk=%llu threads=%u (%zu vs %zu findings)\n",
                     static_cast<unsigned long long>(chunk), t, got.size(), ref.size());
      CHECK(got == ref);
    }
}

// Same, through the parallel pre-rendering path and real files.
void test_rendered_output_identical() {
  Fixture fx(16ull << 20, 1);
  auto img = to_memory_image(fx.synth, false);
  auto names = plugin_names(fx.ps);
  auto run = [&](uint64_t chunk, unsigned threads, bool per_plugin) {
    std::string dir = tmp_path("out-" + std::to_string(chunk) + "-" + std::to_string(threads));
    make_dirs(dir);
    std::string all;
    {
      std::unique_ptr<FindingWriter> w;
      FILE* f = nullptr;
      if (per_plugin) {
        w = std::make_unique<PerPluginWriter>(dir, names);
      } else {
        f = std::fopen((dir + "/all.jsonl").c_str(), "w");
        w = std::make_unique<JsonlWriter>(f, names, "img", true);
      }
      EngineConfig cfg;
      cfg.chunk_size = chunk;
      cfg.threads = threads;
      scan_images({{img.get(), w.get()}}, fx.ps, cfg);
    }
    if (per_plugin) {
      for (const auto& n : names) {
        all += read_file(dir + "/" + n + ".jsonl");
        std::remove((dir + "/" + n + ".jsonl").c_str());
      }
    } else {
      all = read_file(dir + "/all.jsonl");
      std::remove((dir + "/all.jsonl").c_str());
    }
    ::rmdir(dir.c_str());
    return all;
  };
  for (bool pp : {false, true}) {
    std::string ref = run(1ull << 40, 1, pp);
    CHECK(ref.size() > 10000);
    CHECK(run(256ull << 10, 4, pp) == ref);
    CHECK(run(1ull << 20, 8, pp) == ref);
  }
}

// What memforge-mpi split does, minus MPI: partition the chunk list, scan the
// parts independently, ship results through the wire format, merge in order.
void test_split_merge() {
  Fixture fx(16ull << 20, 1);
  auto img = to_memory_image(fx.synth, false);
  auto ref = as_lines(scan_vec(*img, fx.ps, 1ull << 40, 1), fx.ps);
  auto chunks = plan_chunks(*img, 1ull << 20, max_halo(fx.ps));
  for (int ranks : {2, 3, 5}) {
    VectorWriter w;
    Pipeline pipe(fx.ps, &w);
    for (int r = 0; r < ranks; ++r) {
      size_t a = chunks.size() * r / ranks, b = chunks.size() * (r + 1) / ranks;
      auto part = scan_chunk_range(*img, fx.ps, chunks, a, b, 2);
      ByteWriter bw;
      bw.u64(part.size());
      for (const auto& c : part) encode_chunk_result(bw, c);
      ByteReader rd(bw.bytes().data(), bw.bytes().size());
      uint64_t n = rd.u64();
      for (uint64_t k = 0; k < n; ++k) pipe.consume(decode_chunk_result(rd));
      CHECK(rd.done());
    }
    pipe.finish();
    CHECK(as_lines(w.findings, fx.ps) == ref);
  }
}

void test_string_boundaries() {
  // A string straddling a chunk edge is reported exactly once, at its start;
  // a string longer than a whole chunk is still reported once.
  std::vector<uint8_t> m(64 * 1024, 0);
  std::string s1 = "BOUNDARY_STRADDLER";
  std::memcpy(m.data() + 4096 - 7, s1.data(), s1.size());
  std::string s2(9000, 'Q');
  std::memcpy(m.data() + 20000, s2.data(), s2.size());
  auto img = MemoryImage::from_buffer(std::move(m));
  Options o;
  o.set("strings.max", "16384");
  o.set("strings.wide", "false");
  auto owned = create_plugins({"strings"}, o);
  PluginSet ps{owned[0].get()};
  for (uint64_t chunk : {4096ull, 8192ull, 1ull << 20}) {
    auto fs = scan_vec(*img, ps, chunk, 4);
    CHECK_EQ(fs.size(), size_t(2));
    if (fs.size() == 2) {
      CHECK_EQ(fs[0].offset, uint64_t(4096 - 7));
      CHECK(fs[0].get("value")->s == s1);
      CHECK_EQ(fs[1].offset, uint64_t(20000));
      CHECK_EQ(fs[1].length, uint64_t(9000));
    }
  }
}

void test_wire_format() {
  Finding f;
  f.offset = 0x1234;
  f.length = 7;
  f.plugin = 3;
  f.kind = "k";
  f.str("s", std::string("a\0b", 3)).num("n", 42).hex("h", 0xdead).real("r", 1.5).flag("b", true);
  ByteWriter w;
  encode_finding(w, f);
  ByteReader r(w.bytes().data(), w.bytes().size());
  Finding g = decode_finding(r);
  CHECK(r.done());
  CHECK(finding_to_json(f, "p", "i") == finding_to_json(g, "p", "i"));
  // JSON escaping keeps garbage bytes valid.
  CHECK(json_quote(std::string("\x01\xff\"", 3)) == "\"\\u0001\\u00ff\\\"\"");

  ImageReport rep;
  rep.image = "x";
  rep.plugins = {"a", "b"};
  rep.findings = {1, 2};
  rep.plugin_seconds = {0.5, 0.25};
  rep.counters["a.c"] = 9;
  rep.rank = 3;
  ByteWriter w2;
  encode_report(w2, rep);
  ByteReader r2(w2.bytes().data(), w2.bytes().size());
  CHECK(report_to_json(decode_report(r2)) == report_to_json(rep));
}

void test_lpt_and_names() {
  std::vector<uint64_t> w = {10, 9, 8, 7, 6, 5, 4, 3, 2, 1};
  auto bins = lpt_assign(w, 3);
  std::vector<uint64_t> load(3, 0);
  for (size_t i = 0; i < w.size(); ++i) load[bins[i]] += w[i];
  CHECK(*std::max_element(load.begin(), load.end()) - *std::min_element(load.begin(), load.end()) <= 1);
  CHECK(lpt_assign(w, 3) == bins);
  auto names = output_names({"/a/mem.raw", "/b/mem.raw", "/c/mem.raw", "weird name?.lime"});
  CHECK(names[0] == "mem.raw" && names[1] == "mem.raw-2" && names[2] == "mem.raw-3");
  CHECK(names[3] == "weird_name_.lime");
}

void test_batch_with_failures() {
  SynthSpec spec;
  spec.size = 4ull << 20;
  std::vector<std::string> paths;
  for (int k = 0; k < 5; ++k) {
    spec.seed = 100 + k;
    paths.push_back(tmp_path("batch" + std::to_string(k) + ".raw"));
    write_file(paths.back(), to_raw(make_synthetic(spec)));
  }
  paths.insert(paths.begin() + 2, tmp_path("does-not-exist.raw"));
  auto owned = create_plugins({"netioc", "pescan"}, Options());
  PluginSet ps{owned[0].get(), owned[1].get()};
  BatchConfig cfg;
  cfg.discard = true;
  cfg.window = 2;
  cfg.engine.threads = 4;
  std::vector<size_t> which;
  for (size_t i = 0; i < paths.size(); ++i) which.push_back(i);
  auto reps = run_batch(paths, output_names(paths), which, ps, cfg);
  CHECK_EQ(reps.size(), paths.size());
  int failed = 0;
  for (const auto& r : reps) {
    if (!r.error.empty()) {
      ++failed;
      continue;
    }
    CHECK_EQ(r.findings[1], uint64_t(3));  // three planted PE headers per image
  }
  CHECK_EQ(failed, 1);
  for (const auto& p : paths) std::remove(p.c_str());
}

}  // namespace

int main() {
  struct T {
    const char* name;
    std::function<void()> fn;
  } tests[] = {
      {"options", test_options},
      {"aho_corasick_vs_bruteforce", test_aho_corasick},
      {"lime_and_ranges", test_lime_and_ranges},
      {"x64_address_translation", test_address_translation},
      {"ground_truth_raw", [] { test_ground_truth(false); }},
      {"ground_truth_lime_with_hole", [] { test_ground_truth(true); }},
      {"exactness_chunks_x_threads", test_exactness},
      {"rendered_output_identical", test_rendered_output_identical},
      {"split_merge_like_mpi", test_split_merge},
      {"string_boundaries", test_string_boundaries},
      {"wire_format", test_wire_format},
      {"lpt_and_output_names", test_lpt_and_names},
      {"batch_with_failures", test_batch_with_failures},
  };
  for (auto& t : tests) {
    int before = g_failed;
    try {
      t.fn();
    } catch (const std::exception& e) {
      ++g_failed;
      std::fprintf(stderr, "  EXCEPTION: %s\n", e.what());
    }
    std::printf("%s %s\n", g_failed == before ? "[ OK ]" : "[FAIL]", t.name);
  }
  std::printf("%d checks, %d failed (backend: %s)\n", g_checks, g_failed, parallel_backend());
  return g_failed ? 1 : 0;
}
