#!/bin/sh
# usage: bench/run_bench.sh [interpreter]   - runs every benchmark, prints the time
# The interpreter defaults to the C port (c/basic); pass ./basic for the CREXX
# version (much slower: it is interpreted by the CREXX virtual machine).
# The output of each program is deterministic, so the two versions can be compared.
DIR=$(cd "$(dirname "$0")" && pwd)
BASIC="${1:-$DIR/../c/basic}"
now() {
  t=$(date +%s%N 2>/dev/null)
  case "$t" in *N*|"") t=$(( $(date +%s) * 1000000000 )) ;; esac
  echo "$t"
}
for f in loop nested strbuild sort sieve; do
  t0=$(now)
  out=$("$BASIC" "$DIR/$f.bas" 2>&1 | tr '\n' '|')
  t1=$(now)
  printf '%-9s %6s ms  %s\n' "$f" "$(( (t1 - t0) / 1000000 ))" "$out"
done
