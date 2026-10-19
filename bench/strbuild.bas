' string building and scanning
S$ = ""
FOR I = 1 TO 3000
  S$ = S$ + CHR$(65 + I MOD 26)
NEXT I
PRINT LEN(S$); LEFT$(S$, 20); RIGHT$(S$, 10)
C = 0
FOR I = 1 TO LEN(S$)
  IF MID$(S$, I, 1) = "E" THEN C = C + 1
NEXT I
PRINT C
W$ = ""
FOR I = 1 TO 400
  W$ = W$ + STR$(I) + ","
NEXT I
PRINT LEN(W$); INSTR(W$, "399"); UCASE$(LEFT$(W$, 12))
