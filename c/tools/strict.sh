#!/bin/sh
# Conformance matrix: strict ISO C89 on gcc and clang (syntax + warnings), then
# gcc with -O0 -fsanitize=undefined,address running the whole shared test suite.
# usage: sh tools/strict.sh        (from c/)
cd "$(dirname "$0")/.." || exit 1
STRICT="-std=c89 -pedantic-errors -Wall -Wextra -Wconversion -Wshadow
 -Wdeclaration-after-statement -Wvla -Wstrict-prototypes -Wundef -Wtrigraphs
 -Wlong-long -Wno-unknown-pragmas"
fail=0
for cc in gcc clang; do
  command -v $cc >/dev/null 2>&1 || { echo "skip $cc (not installed)"; continue; }
  for src in basic_all.c deck/basic_deck.c; do
    [ -f $src ] || continue
    for d in "" "-DBASIC_POSIX"; do
      out=$($cc $STRICT $d -fsyntax-only $src 2>&1)
      if [ -n "$out" ]; then echo "FAIL $cc $d $src"; echo "$out" | head -8; fail=1
      else echo "ok   $cc $d $src"; fi
    done
  done
done
# C99 library functions must not be referenced (undefined symbols of the object)
gcc -std=c89 -O0 -DBASIC_POSIX -c basic_all.c -o strict_tmp.o 2>/dev/null &&
  nm -u strict_tmp.o | awk '{print $2}' | sort | tr '\n' ' ' > strict_syms.txt
C99FUNCS='strdup|snprintf|vsnprintf|isblank|round|trunc|copysign|isnan|isinf|fmin|fmax'
C99FUNCS="$C99FUNCS|lround|cbrt|log2|log1p|expm1|hypot|erf|tgamma|lgamma|strtoll|strtoull"
C99FUNCS="$C99FUNCS|nan|nearbyint|rint|fma|asinh|acosh|atanh|exp2|remainder"
bad=$(tr ' ' '\n' < strict_syms.txt | grep -E "^($C99FUNCS)f?\$")
[ -n "$bad" ] && { echo "FAIL C99 library references: $bad"; fail=1; }
rm -f strict_tmp.o strict_syms.txt
# sanitizers
if gcc -std=c89 -pedantic -O0 -g -fsanitize=undefined,address -fno-sanitize-recover=undefined \
     -DBASIC_POSIX -o strict_san basic_all.c -lm 2>/dev/null; then
  ASAN_OPTIONS=detect_leaks=0 sh tests/run_tests.sh ./strict_san > strict_run.txt 2>&1
  tail -1 strict_run.txt
  grep -q FAIL strict_run.txt && fail=1
  rm -f strict_san strict_run.txt
else echo "skip sanitizers (not available)"; fail=1; fi
[ $fail -eq 0 ] && echo "strict: all ok"
exit $fail
