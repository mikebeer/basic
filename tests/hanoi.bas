10 REM Towers of Hanoi with GOSUB-free recursion via SUB
20 DECLARE SUB Hanoi (N AS INTEGER, F$, T$, V$)
30 DIM SHARED MOVES AS INTEGER
40 Hanoi 4, "A", "C", "B"
50 PRINT "Total moves:"; MOVES
60 END
70 SUB Hanoi (N AS INTEGER, F$, T$, V$)
80   IF N = 0 THEN EXIT SUB
90   Hanoi N - 1, F$, V$, T$
100  MOVES = MOVES + 1
110  PRINT "Move disc"; N; "from "; F$; " to "; T$
120  Hanoi N - 1, V$, T$, F$
130 END SUB
