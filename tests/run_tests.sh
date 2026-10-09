#!/bin/sh
# Regression tests: for every tests/NAME.bas run the interpreter and compare
# stdout+stderr with tests/NAME.out.  Optional files: NAME.in (stdin),
# NAME.flags (interpreter options).  Use "run_tests.sh --update" to rewrite
# the expected output files.
DIR=$(cd "$(dirname "$0")" && pwd)
BASIC="$DIR/../basic"
pass=0; fail=0
for src in "$DIR"/*.bas; do
  name=$(basename "$src" .bas)
  flags=""; [ -f "$DIR/$name.flags" ] && flags=$(cat "$DIR/$name.flags")
  input=/dev/null; [ -f "$DIR/$name.in" ] && input="$DIR/$name.in"
  actual=$("$BASIC" $flags "$src" < "$input" 2>&1)
  if [ "$1" = "--update" ]; then
    printf '%s\n' "$actual" > "$DIR/$name.out"; echo "updated $name"; continue
  fi
  if [ "$actual" = "$(cat "$DIR/$name.out")" ]; then
    pass=$((pass+1)); echo "ok   $name"
  else
    fail=$((fail+1)); echo "FAIL $name"
    printf '%s\n' "$actual" | diff - "$DIR/$name.out" | head -10
  fi
done
echo "passed $pass, failed $fail"
[ "$fail" -eq 0 ]
