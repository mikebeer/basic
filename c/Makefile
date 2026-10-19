# BASIC interpreter in portable C89
CC ?= cc
CFLAGS ?= -O2
STD = -std=c89 -pedantic -Wall -Wextra
POSIX = -DBASIC_POSIX
SRC = src/basic.c src/b0_base.c src/b_proto.h src/b1_util.c src/b2_lex.c src/b3_link.c \
      src/b4_expr.c src/b5_stmt.c src/b6_main.c

basic: basic_all.c
	$(CC) $(STD) $(CFLAGS) $(POSIX) -o basic basic_all.c -lm

src/b_proto.h: src/b1_util.c src/b2_lex.c src/b3_link.c src/b4_expr.c src/b5_stmt.c src/b6_main.c tools/protos.sh
	sh tools/protos.sh > src/b_proto.h

basic_all.c: $(SRC) tools/amalg.sh
	sh tools/amalg.sh > basic_all.c

test: basic
	sh tests/run_tests.sh

strict: basic_all.c
	sh tools/strict.sh

bench: basic
	sh ../bench/run_bench.sh ./basic

clean:
	rm -f basic strict_san

PYTHON ?= python3
DECKCOLS ?= 71

# the card deck for the VM/370 build: every line at most $(DECKCOLS) columns
deck: deck/basic_deck.c

deck/basic_deck.c: basic_all.c tools/deck.py
	mkdir -p deck
	$(PYTHON) tools/deck.py --limit $(DECKCOLS) basic_all.c > deck/basic_deck.c
	$(PYTHON) tools/deck.py --verify basic_all.c deck/basic_deck.c

check: basic_all.c deck
	$(PYTHON) tools/vm370_check.py

.PHONY: test strict bench deck check clean
