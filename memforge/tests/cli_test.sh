#!/usr/bin/env bash
# End-to-end tests for the memforge CLI. Usage: cli_test.sh BUILD_DIR
set -euo pipefail
BIN="$1"
T="$(mktemp -d)"
trap 'rm -rf "$T"' EXIT
fail() { echo "FAIL: $*" >&2; exit 1; }

"$BIN/mkimage" "$T/a.raw" --size 32M --seed 1 --truth "$T/a.truth.json" --rules "$T/rules.txt" 2>/dev/null
"$BIN/mkimage" "$T/b.lime" --size 24M --seed 2 --lime --hole-at 8M --hole-size 4M 2>/dev/null
"$BIN/mkimage" "$T/c.raw" --size 8M --seed 3 2>/dev/null

# plugins / info
[ "$("$BIN/memforge" plugins | wc -l)" -eq 7 ] || fail "expected 7 plugins"
"$BIN/memforge" info "$T/b.lime" | grep -q "format: lime" || fail "info: LiME not detected"
"$BIN/memforge" info "$T/b.lime" | grep -q "2 range(s)" || fail "info: expected 2 ranges"

# Output must not depend on threads or chunk size.
"$BIN/memforge" scan -q -i "$T/a.raw" --opt patscan.rules="$T/rules.txt" -j 1 -c 1T > "$T/serial.jsonl"
"$BIN/memforge" scan -q -i "$T/a.raw" --opt patscan.rules="$T/rules.txt" -j 4 -c 256K > "$T/par.jsonl"
cmp -s "$T/serial.jsonl" "$T/par.jsonl" || fail "parallel output differs from serial"
[ "$(wc -l < "$T/serial.jsonl")" -gt 1000 ] || fail "too few findings"

# Directory output, summary, and every planted artifact present.
"$BIN/memforge" scan -q -m <(printf '%s\n' "$T/a.raw" "$T/b.lime" "$T/c.raw") \
  --opt patscan.rules="$T/rules.txt" -o "$T/out" -j 4
for f in summary.json a.raw/report.json a.raw/strings.jsonl b.lime/netioc.jsonl c.raw/pescan.jsonl; do
  [ -s "$T/out/$f" ] || fail "missing $f"
done
grep -q '"images":3' "$T/out/summary.json" || fail "summary image count"
grep -q '"failed":0' "$T/out/summary.json" || fail "summary failures"
grep -o '"offset":[0-9]*,"key":"[a-z_]*","value":"[^"]*"' "$T/a.truth.json" | while read -r line; do
  off=$(sed 's/"offset":\([0-9]*\).*/\1/' <<<"$line")
  val=$(sed 's/.*"value":"\(.*\)"/\1/' <<<"$line")
  grep -h "\"offset\":$off," "$T"/out/a.raw/*.jsonl | grep -qF -- "$val" || fail "planted artifact at $off ($val) not found"
done

# Sharding: shards partition the input exactly.
for s in 0 1; do
  "$BIN/memforge" scan -q -m <(printf '%s\n' "$T/a.raw" "$T/b.lime" "$T/c.raw") --shard $s/2 \
    -p pescan --no-output --summary "$T/shard$s.json"
done
n0=$(grep -o '"images":[0-9]*' "$T/shard0.json" | cut -d: -f2)
n1=$(grep -o '"images":[0-9]*' "$T/shard1.json" | cut -d: -f2)
[ $((n0 + n1)) -eq 3 ] || fail "shards cover $((n0 + n1)) images, expected 3"

# A missing image is reported, not fatal to the batch.
set +e
"$BIN/memforge" scan -q -i "$T/c.raw" -i "$T/nope.raw" -p pescan --no-output --summary "$T/f.json"
rc=$?
set -e
[ $rc -eq 2 ] || fail "expected exit 2 for a partially failed batch, got $rc"
grep -q '"failed":1' "$T/f.json" || fail "failure not in summary"

# dtbscan -> vtop: the self-map VA must translate back to the DTB.
dtb=$(grep -o '"dtb":"0x[0-9a-f]*"' "$T/out/a.raw/dtbscan.jsonl" | head -1 | cut -d'"' -f4)
idx=$(grep -o '"selfmap_index":"0x[0-9a-f]*"' "$T/out/a.raw/dtbscan.jsonl" | head -1 | cut -d'"' -f4)
[ -n "$dtb" ] || fail "no DTB found"
va=$(python3 -c "i=$idx; v=(i<<39)|(i<<30)|(i<<21)|(i<<12); v|=(0xffff<<48) if v>>47&1 else 0; print(hex(v))")
"$BIN/memforge" vtop -i "$T/a.raw" --dtb "$dtb" --va "$va" | grep -q -- "-> $dtb" || fail "vtop self-map"

echo "cli tests passed"
