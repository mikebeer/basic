# BASIC in CREXX

A BASIC interpreter written in [CREXX](https://github.com/adesutherland/CREXX)
(Level B, `1.0.0-beta.3`).  It runs classic Microsoft BASIC programs
(GW-BASIC / QuickBASIC style, with or without line numbers) and a large part of
the FreeBASIC dialect: `SUB` / `FUNCTION`, `SELECT CASE`, `DO ... LOOP`,
block `IF`, `CONST`, typed `DIM ... AS`, `EXIT` / `CONTINUE`, compound
assignment (`+=`), `PRINT USING` and file I/O.

The whole interpreter is the single file [`basic.crexx`](basic.crexx).  A
program is tokenised once, blocks / labels / procedures / `DATA` are resolved in
a link pass, and execution then walks the token array - so no text is scanned
while the program runs.

The full manual, in the style of a classic IBM publication, is in
[`docs/BASIC-CREXX-Guide.docx`](docs/BASIC-CREXX-Guide.docx) (PDF copy:
[`docs/BASIC-CREXX-Guide.pdf`](docs/BASIC-CREXX-Guide.pdf)).

## Running

You need the `crexx` driver (`rxc`, `rxas`, `rxvm`) on your `PATH`, or point
`CREXX` at it.

```sh
./basic examples/hello.bas          # run a program
./basic -fb examples/subs.bas       # FreeBASIC style number printing
./basic                             # interactive prompt (RUN, LIST, NEW, LOAD "f", SYSTEM)
./tests/run_tests.sh                # regression tests
```

The first run compiles the interpreter to bytecode (about half a minute);
afterwards start-up is well under a second.  Without the launcher:

```sh
crexx basic.crexx -args examples/hello.bas
```

Options (before the program name): `-ms` (default) / `-qb`, `-fb`, `-ansi`,
`-dump` (list the token stream).  Anything after the program name is passed to
the program (`COMMAND$`).

* `-ms` - numbers print as ` 5 ` (leading sign space, trailing space) like
  GW-BASIC / QuickBASIC.
* `-fb` - numbers print as ` 5` like FreeBASIC; `STR$(5)` has no leading space.
* `-ansi` - `CLS`, `LOCATE` and `COLOR` emit ANSI escape codes (they are
  silent no-ops otherwise, so redirected output stays clean).

## Supported language

**Statements** - `PRINT` / `?` (`;` `,` `TAB` `SPC`, `#n,` file output),
`PRINT USING` (`# . , + - $$ ** ^^^^ \ \ ! &`), `LET`, `IF/THEN/ELSE`
(single-line and block, `ELSEIF`, `IF x GOTO n`), `FOR/NEXT` (`STEP`, `NEXT i, j`),
`WHILE/WEND`, `DO/LOOP` (`WHILE`/`UNTIL` at either end), `SELECT CASE`
(values, lists, `a TO b`, `IS <op>`), `GOTO`, `GOSUB/RETURN`, `ON x GOTO/GOSUB`,
labels and line numbers, `EXIT FOR/DO/WHILE/SUB/FUNCTION/SELECT`,
`CONTINUE FOR/DO/WHILE`, `END`, `STOP`, `SYSTEM`, `DIM` (`DIM SHARED`,
`DIM a(1 TO 5)`, `DIM AS type`, initialisers), `REDIM [PRESERVE]`, `ERASE`,
`CONST`, `STATIC`, `OPTION BASE`, `DEFINT/DEFSNG/DEFDBL/DEFSTR/DEFLNG`,
`DATA/READ/RESTORE [label]`, `SWAP`, `INPUT` / `LINE INPUT` (console or `#n`),
`WRITE`, `OPEN ... FOR INPUT/OUTPUT/APPEND AS #n` (and the old
`OPEN "I", #1, f$` form), `CLOSE`, `ON ERROR GOTO/RESUME NEXT`, `RESUME [NEXT|label]`,
`ERROR n`, `DEF FNx(..) = ..`, `SUB` / `FUNCTION` (recursion, `BYREF`/`BYVAL`,
array parameters, `FUNCTION = x`, `RETURN x`), `CALL`, `MID$(s,i,n) = ..`,
`RANDOMIZE`, `CLS`, `LOCATE`, `COLOR`, `SLEEP`, `BEEP`, `CLEAR`.

**Operators** - `+ - * / \ MOD ^`, comparison, `AND OR XOR EQV IMP NOT`,
`SHL SHR`, `&` (string concatenation), `+=  -=  *=  /=  \=  ^=  &=`.

**Functions** - `ABS ASC ATN ATAN2 ACOS ASIN COS SIN TAN SINH COSH TANH EXP LOG
LOG10 LOG2 SQR SGN INT FIX FRAC CINT CLNG CSNG CDBL CBOOL RND TIMER DATE$ TIME$
LEN LEFT$ RIGHT$ MID$ INSTR INSTRREV LTRIM$ RTRIM$ TRIM$ UCASE$ LCASE$ STR$
VAL VALINT CHR$ HEX$ OCT$ BIN$ SPACE$ STRING$ TAB SPC IIF MAX MIN POW ISNUMERIC
EOF LOF FREEFILE INPUT$ INKEY$ ERR ERL FRE POS CSRLIN LBOUND UBOUND COMMAND$
ENVIRON$`. A `SUB`/`FUNCTION` with the same name as a builtin overrides it.

Numbers are 64-bit floats; variables with an integer suffix/type (`%`, `&`,
`AS INTEGER`, `LONG`, ...) round on assignment.  Strings are unlimited length.
Names are case-insensitive.  Arrays are 0-based unless `OPTION BASE 1` or an
explicit `lo TO hi` is used, and an undimensioned array auto-dimensions to 10.

## Not supported

`TYPE ... END TYPE`, pointers, `UNION`, `PUT/GET`/random-access files, `SHELL`,
graphics (`SCREEN`, `PSET`, `LINE`, ...), sound, `#include` / preprocessor,
`ENUM`, and object-oriented FreeBASIC features.  `SCREEN`, `WIDTH`, `VIEW` and
`PALETTE` are accepted and ignored.  `PRINT` shows numbers with up to 15
significant digits (no single-precision rounding).

## Layout

| Path | |
|---|---|
| `basic.crexx` | the interpreter |
| `basic` | launcher script (compiles on first use) |
| `docs/` | the Language Reference and User's Guide (Word and PDF) |
| `examples/` | sample programs in both dialects |
| `tests/` | regression programs with expected output (`tests/run_tests.sh`) |
