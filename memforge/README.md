# memforge — parallel & distributed memory forensics

Volatility, the standard memory-forensics framework, runs single-threaded Python
plugins over one image at a time. That is fine for one laptop dump. It does not
hold up when the image is 256 GB, or when a DFIR pipeline has to triage
thousands of captures after a fleet-wide incident.

**memforge** is a C++17 engine that exploits that gap:

* **Intra-image parallelism (OpenMP).** Physical memory is cut into chunks and
  every plugin runs on every chunk across all cores. The result is *provably
  identical* to one serial pass.
* **Fleet parallelism.** Tasks are flattened across images, so a node triaging
  thousands of small dumps keeps every core busy.
* **Distribution (MPI).** `farm` mode spreads images across nodes with a
  dynamic master/worker or static LPT schedule. `split` mode spreads *one*
  huge image across nodes, and its output is byte-identical to a single-node
  run.
* **Scheduler-agnostic sharding.** `--shard I/N` lets Slurm array jobs or
  Kubernetes indexed jobs split a manifest without MPI.

```
                ┌──────────── memforge-mpi farm / --shard I/N ────────────┐
 manifest ──►   │ rank 0: LPT / dynamic queue of images (largest first)   │
 (1000s of      └──────┬──────────────────┬──────────────────┬────────────┘
  images)              ▼                  ▼                  ▼
                   node 1             node 2      …      node N
               ┌─────────────────────────────────────────────────────┐
               │ flat task list: (image, 16 MiB chunk) × all plugins │
               │ OpenMP dynamic schedule ─► scan_chunk() on T cores  │
               │   • every plugin runs while the chunk is hot (fused)│
               │   • JSON rendered on the worker thread              │
               │ per-image reorder buffer ─► reducers ─► JSONL files │
               └─────────────────────────────────────────────────────┘
```

## Exactness: the chunk-ownership rule

Splitting memory naively breaks forensics. A string, URL, or PE header that
crosses a chunk edge gets missed or reported twice. The test suite shows this:
removing one look-behind check produces 36 duplicate strings at 64 KiB chunk
edges. memforge enforces a single rule (see `include/memforge/plugin.hpp`):

> **A finding belongs to the chunk that contains its first byte.**

* Each chunk exposes a **halo**: it may read `halo()` bytes past its end to
  finish a match that started inside it.
* Each chunk also exposes a **look-behind** window back to the start of its
  physical range. A match that *continues* from the previous chunk is
  recognised as not owned. Chunks never span a gap in physical memory, such as
  between LiME ranges.
* Per-image **reorder buffers** release chunk results strictly in address
  order. An optional per-plugin **reducer** then runs serially; for example,
  `entropy` merges per-page hits into regions, so a region that spans 50
  chunks is reported once.
* **Plugins must bound every read by their halo.** For example, `pescan`
  rejects optional headers larger than `0xF0`, so results never depend on
  chunk size.

The property is tested directly. Output for chunk sizes {64 KiB, 300 KiB,
1 MiB, 5 MiB} × threads {1, 3, 8} is compared against one monolithic serial
scan. The MPI split path (partition → wire format → ordered merge) and real
multi-rank MPI runs are compared the same way.

## Plugins

| plugin    | finds | how it parallelises |
|-----------|-------|---------------------|
| `strings` | ASCII + UTF-16LE strings | SSE2 bitmasks per 64 B block; run starts by shifts, `ctz` to run ends |
| `netioc`  | URLs, IPv4 (ASCII + UTF-16LE), IP class | `memchr` anchors on `:` / `.`, then back-tracks |
| `patscan` | IOC rules: text (`nocase`, `wide`) and hex | Aho-Corasick DFA, compressed alphabet, 4 interleaved lanes |
| `pescan`  | in-memory EXE/DLL/driver headers, sections | page-aligned probe + header validation |
| `pooltag` | Windows x64 pool objects: processes (with a name guess), threads, files, drivers, TCP/UDP endpoints | `_POOL_HEADER` validation, the psscan technique |
| `dtbscan` | x64 page-table roots (CR3/DTB) without symbols | PML4 self-map candidate, verified by a full 4-level walk |
| `entropy` | high-entropy regions (packed / encrypted payloads), zero-page census | per-page Shannon entropy plus an ordered region-merge reducer |

Options are passed as `--opt plugin.key=value`, for example `strings.min=8`,
`entropy.threshold=7.5`, `patscan.rules=iocs.txt`, `netioc.public_only=true`,
or `pooltag.tags=Toke,Sema`. Run `memforge plugins` to list them.

Example `patscan` rule file:

```
mimikatz          text "mimikatz" nocase wide ascii
reflective_loader text "Invoke-ReflectivePEInjection" wide
msf_x64_shellcode hex  "fc 48 83 e4 f0 e8 c0 00 00 00"
```

Plugins implement one small interface (`name`, `halo`, a thread-safe `scan`,
and optionally `make_reducer`). Adding one means adding one file under
`src/plugins/` and one line in `src/registry.cpp`.

## Build

```sh
cmake -S memforge -B memforge/build -DCMAKE_BUILD_TYPE=Release
cmake --build memforge/build -j
ctest --test-dir memforge/build --output-on-failure   # unit + CLI + MPI end-to-end
```

The build requires a C++17 compiler and CMake ≥ 3.16. OpenMP is optional and
falls back to a `std::thread` pool (`-DMEMFORGE_OPENMP=OFF`). MPI is optional
and only needed for `memforge-mpi`. CI builds and tests both configurations.

## Usage

```sh
# Synthetic test image with planted artifacts + ground truth + matching rules
memforge/build/mkimage mem.raw --size 1G --truth truth.json --rules iocs.txt

memforge scan mem.raw --opt patscan.rules=iocs.txt > findings.jsonl     # all cores
memforge scan -m manifest.txt -p pescan,pooltag,dtbscan -o out/           # a fleet
memforge scan -m manifest.txt -o out/ --shard $SLURM_ARRAY_TASK_ID/64     # no MPI
memforge info capture.lime                                                # LiME ranges
memforge vtop -i mem.raw --dtb 0x1aa000 --va 0xfffff6fb7dbed000           # x64 paging
memforge bench -i mem.raw -p all --threads-list 1,2,4,8                   # scaling

# Distributed (images on a shared filesystem; one rank per node)
mpirun --bind-to none -n 65 memforge-mpi farm  -m manifest.txt -o out/ -j 32
mpirun --bind-to none -n 8  memforge-mpi split -i huge-256G.raw -o out/ -j 32
```

`-o DIR` writes `DIR/<image>/<plugin>.jsonl`, `DIR/<image>/report.json`
(timings, counters, finding counts), and `DIR/summary.json`. A failed or
corrupt image is recorded in the summary and does not abort the batch (exit
code 2).

Each finding is one JSON line:

```json
{"plugin":"pooltag","offset":66059520,"offset_hex":"0x3effd00","length":1536,"kind":"process","tag":"Proc","block_size":"0x600","pool_type":"0x2","name_guess":"evil.exe"}
{"plugin":"dtbscan","offset":48259072,"offset_hex":"0x2e06000","length":4096,"kind":"x64-dtb","dtb":"0x2e06000","selfmap_index":"0x11d","pte_base":"0xffff8e8000000000","user_entries":2,"kernel_entries":6,"verified":true}
{"plugin":"entropy","offset":64929792,"offset_hex":"0x3dec000","length":163840,"kind":"high-entropy-region","pages":40,"mean_entropy":7.9545,"max_entropy":7.9603}
```

> **Open MPI note:** by default Open MPI binds each rank to a *single core*,
> which silently serialises the OpenMP team inside the rank. In our runs this
> made 2 ranks × 2 threads 3.5× slower. Use `--bind-to none` or
> `--map-by node:PE=<cores>`.

## Results

These numbers come from a 4-core Xeon VM. The full output is in
[`bench/RESULTS.md`](bench/RESULTS.md), and `bench/run_benchmarks.sh`
reproduces it. Highlights:

* **Thread scaling:** all 7 plugins on a 2 GiB image reach **3.99× on 4
  cores**, with the same 9.37 M findings at every thread count.
* **Versus Python:** a single-threaded pure-Python scan doing strings, URLs and
  IPv4 takes **330 s** on the 2 GiB image. It uses the C-implemented `re`
  module, which favours Python, and real Volatility plugins add more overhead.
  memforge takes **15.3 s on 1 thread (22×)** and **4.6 s on 4 threads
  (73×)**, while writing full JSONL to disk.
* **Fleets:** for 256 × 16 MiB images, flattening tasks across images
  (`--window 16`) is **3.6× faster** than scanning images one at a time on the
  same 4 cores. The static MPI farm reaches the same aggregate rate (4.4 GB/s).
* **Split mode:** 2 ranks × 2 threads match 1 process × 4 threads on one node
  (3.40 s vs 3.46 s), so the distributed merge is effectively free.
* **Hot loops:** SIMD bitmasks made the `strings` scan core 6.7× faster.
  Anchoring `netioc` on `memchr` made it 23× faster. Interleaving the
  Aho-Corasick lanes gave 1.75×.

Multi-node numbers are not included because only one machine was available.
Multi-rank correctness is tested (3 ranks on one host, byte-identical
output), but cross-node scaling still needs to be measured on a real cluster.

## Limitations (honest scope)

* **Scanning, not structure walking.** memforge parallelises the scanning
  workloads that dominate DFIR triage time. It does **not** yet replace
  symbol-driven plugins such as Volatility's `pslist`, `handles`, or `malfind`,
  which need ISF/PDB type information. `dtbscan` and `X64AddressSpace` are the
  foundation for that.
* `pooltag` uses the classic `_POOL_HEADER` heuristic. Windows 10 19H1+
  segment-heap allocations and pool-header encoding reduce its recall, just as
  they do for Volatility's `psscan`. `name_guess` is a heuristic.
* Input formats are raw/padded dumps and LiME. Windows crash dumps, hiberfil,
  AFF4, and compressed formats are not yet supported.
* The evaluation uses synthetic images with ground truth, so it can test exact
  recall. Real-world validation against reference dumps is the next step.

## Roadmap

1. ISF symbol loading → parallel `pslist`/`psscan` cross-view (DKOM detection)
   and per-process VAD scans fanned out over processes.
2. Crash-dump / hiberfil / AFF4 readers behind `MemoryImage`.
3. A Volatility 3 bridge: run memforge as a high-speed pre-filter and hand
   offsets to Volatility for deep analysis.
4. Cross-image analytics in `farm` mode, such as page-hash deduplication and
   "which hosts share this IOC?" queries.

## Layout

```
include/memforge/   public headers (image, plugin, engine, batch, addrspace, aho_corasick, synth)
src/                engine, image I/O, batch orchestration, CLI, MPI driver
src/plugins/        the seven plugins
tools/mkimage.cpp   synthetic image + ground-truth generator
tests/              unit tests, CLI end-to-end, MPI end-to-end
bench/              benchmark harness, Python baseline, RESULTS.md
```
