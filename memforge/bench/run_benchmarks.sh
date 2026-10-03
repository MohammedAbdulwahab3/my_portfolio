#!/usr/bin/env bash
# Reproduces bench/RESULTS.md. Usage: bench/run_benchmarks.sh BUILD_DIR WORKDIR [SIZE]
set -euo pipefail
BIN="$1"; W="$2"; SIZE="${3:-2G}"
mkdir -p "$W"
export OMPI_ALLOW_RUN_AS_ROOT=1 OMPI_ALLOW_RUN_AS_ROOT_CONFIRM=1 OMPI_MCA_rmaps_base_oversubscribe=1
NP=$(nproc)
TL=1; t=2; while [ $t -le "$NP" ]; do TL="$TL,$t"; t=$((t * 2)); done
[[ ",$TL," == *",$NP,"* ]] || TL="$TL,$NP"

echo "## Machine"; echo; echo '```'
lscpu | grep -E "Model name|^CPU\(s\)|Thread|L3" | sed 's/  */ /g'
"$BIN/memforge" version; echo '```'; echo

[ -f "$W/big.raw" ] || "$BIN/mkimage" "$W/big.raw" --size "$SIZE" --seed 11 --copies 64 --rules "$W/rules.txt" 2>/dev/null
OPT=(--opt patscan.rules="$W/rules.txt")

echo "## 1. Thread scaling, one $SIZE image, all 7 plugins (JSON rendered to /dev/null)"; echo; echo '```'
"$BIN/memforge" bench -i "$W/big.raw" -p all "${OPT[@]}" --threads-list "$TL" --repeat 3 2>/dev/null
echo '```'; echo

echo "## 2. Per-plugin throughput (single thread -> all threads)"; echo; echo '```'
printf "%-9s %-12s %-12s %s\n" plugin "1T GB/s" "${NP}T GB/s" speedup
for p in strings netioc patscan pescan pooltag dtbscan entropy; do
  out=$("$BIN/memforge" bench -i "$W/big.raw" -p "$p" "${OPT[@]}" --threads-list "1,$NP" --repeat 2 2>/dev/null | tail -2)
  g1=$(echo "$out" | head -1 | awk '{print $3}'); gn=$(echo "$out" | tail -1 | awk '{print $3}')
  printf "%-9s %-12s %-12s %.2fx\n" "$p" "$g1" "$gn" "$(echo "$gn / $g1" | bc -l)"
done
echo '```'; echo

echo "## 3. Versus a single-threaded Python baseline (strings + URLs + IPv4)"; echo; echo '```'
python3 "$(dirname "$0")/py_baseline.py" "$W/big.raw"
for t in 1 "$NP"; do
  "$BIN/memforge" scan -q -i "$W/big.raw" -p strings,netioc -j "$t" -o "$W/out-cmp" --summary "$W/cmp.json"
  echo "memforge -j $t: $(grep -o '"wall_seconds":[0-9.]*' "$W/cmp.json" | head -1 | cut -d: -f2) s ($(grep -o '"aggregate_gbps":[0-9.]*' "$W/cmp.json" | cut -d: -f2) GB/s, JSONL written to disk)"
done
echo '```'; echo

echo "## 4. Many small images: 256 x 16 MiB (pescan,pooltag,dtbscan,netioc)"; echo; echo '```'
mkdir -p "$W/fleet"
for k in $(seq 1 256); do [ -f "$W/fleet/m$k.raw" ] || "$BIN/mkimage" "$W/fleet/m$k.raw" --size 16M --seed $((k + 100)) 2>/dev/null; done
ls "$W"/fleet/*.raw > "$W/fleet.txt"
P=pescan,pooltag,dtbscan,netioc
cat "$W"/fleet/*.raw > /dev/null
for win in 1 16; do
  "$BIN/memforge" scan -q -m "$W/fleet.txt" -p $P -j "$NP" --window $win --no-output --summary "$W/fleet.json"
  echo "1 process, $NP threads, window=$win : $(grep -o '"wall_seconds":[0-9.]*' "$W/fleet.json" | head -1 | cut -d: -f2) s  ($(grep -o '"aggregate_gbps":[0-9.]*' "$W/fleet.json" | cut -d: -f2) GB/s)"
done
if [ -x "$BIN/memforge-mpi" ]; then
  for ranks in 2 "$NP"; do
    th=$((NP / ranks)); [ $th -ge 1 ] || th=1
    mpirun --bind-to none -n $((ranks + 1)) "$BIN/memforge-mpi" farm -q -m "$W/fleet.txt" -p $P -j $th --no-output --summary "$W/farm.json"
    echo "mpi farm dynamic, $ranks workers x $th threads : $(grep -o '"wall_seconds":[0-9.]*' "$W/farm.json" | head -1 | cut -d: -f2) s  ($(grep -o '"aggregate_gbps":[0-9.]*' "$W/farm.json" | cut -d: -f2) GB/s)"
    mpirun --bind-to none -n $ranks "$BIN/memforge-mpi" farm -q -m "$W/fleet.txt" -p $P -j $th --no-output --static --summary "$W/farm.json"
    echo "mpi farm static,  $ranks ranks   x $th threads : $(grep -o '"wall_seconds":[0-9.]*' "$W/farm.json" | head -1 | cut -d: -f2) s  ($(grep -o '"aggregate_gbps":[0-9.]*' "$W/farm.json" | cut -d: -f2) GB/s)"
  done
  echo
  "$BIN/memforge" scan -q -i "$W/big.raw" -p all "${OPT[@]}" -j "$NP" --no-output --summary "$W/one.json"
  echo "split, 1 process x $NP threads      : $(grep -o '"wall_seconds":[0-9.]*' "$W/one.json" | head -1 | cut -d: -f2) s"
  mpirun --bind-to none -n 2 "$BIN/memforge-mpi" split -q -i "$W/big.raw" -p all "${OPT[@]}" -j $((NP / 2)) --no-output --summary "$W/split.json"
  echo "split, 2 ranks x $((NP / 2)) threads (1 node)  : $(grep -o '"wall_seconds":[0-9.]*' "$W/split.json" | head -1 | cut -d: -f2) s"
fi
echo '```'
