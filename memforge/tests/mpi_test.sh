#!/usr/bin/env bash
# Distributed mode must produce exactly what a single node produces.
# Usage: mpi_test.sh BUILD_DIR [MPIEXEC]
set -euo pipefail
BIN="$1"
MPIEXEC="${2:-mpirun}"
T="$(mktemp -d)"
trap 'rm -rf "$T"' EXIT
fail() { echo "FAIL: $*" >&2; exit 1; }
export OMPI_ALLOW_RUN_AS_ROOT=1 OMPI_ALLOW_RUN_AS_ROOT_CONFIRM=1 OMPI_MCA_rmaps_base_oversubscribe=1
MPI=("$MPIEXEC" -n 3)

for k in 1 2 3 4 5; do "$BIN/mkimage" "$T/img$k.raw" --size $((4 * k + 4))M --seed $k 2>/dev/null; done
"$BIN/mkimage" "$T/big.lime" --size 48M --seed 9 --lime --hole-at 16M --hole-size 8M --rules "$T/rules.txt" 2>/dev/null
ls "$T"/img*.raw > "$T/manifest.txt"
OPTS=(--opt patscan.rules="$T/rules.txt" -j 2 -c 2M)

same_tree() {  # compare every *.jsonl under two output dirs
  (cd "$1" && find . -name '*.jsonl' | sort) > "$T/l1"
  (cd "$2" && find . -name '*.jsonl' | sort) > "$T/l2"
  cmp -s "$T/l1" "$T/l2" || fail "$3: different file sets"
  while read -r f; do cmp -s "$1/$f" "$2/$f" || fail "$3: $f differs"; done < "$T/l1"
}

"$BIN/memforge" scan -q -m "$T/manifest.txt" -i "$T/big.lime" "${OPTS[@]}" -o "$T/local"

"${MPI[@]}" "$BIN/memforge-mpi" split -q -i "$T/big.lime" "${OPTS[@]}" -o "$T/split"
same_tree "$T/local/big.lime" "$T/split/big.lime" "split"
"${MPI[@]}" "$BIN/memforge-mpi" split -q -i "$T/big.lime" "${OPTS[@]}" > "$T/split.jsonl"
"$BIN/memforge" scan -q -i "$T/big.lime" "${OPTS[@]}" > "$T/local.jsonl"
cmp -s "$T/split.jsonl" "$T/local.jsonl" || fail "split stdout differs"

"${MPI[@]}" "$BIN/memforge-mpi" farm -q -m "$T/manifest.txt" "${OPTS[@]}" -o "$T/farm"
grep -q '"images":5' "$T/farm/summary.json" || fail "farm summary"
grep -q 'mpi-farm-dynamic' "$T/farm/summary.json" || fail "farm mode"
for k in 1 2 3 4 5; do same_tree "$T/local/img$k.raw" "$T/farm/img$k.raw" "farm img$k"; done

"${MPI[@]}" "$BIN/memforge-mpi" farm -q -m "$T/manifest.txt" "${OPTS[@]}" -o "$T/farm2" --static
for k in 1 2 3 4 5; do same_tree "$T/local/img$k.raw" "$T/farm2/img$k.raw" "static farm img$k"; done

echo "mpi tests passed"
