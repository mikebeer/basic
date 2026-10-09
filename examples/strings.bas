10 REM String functions (MS BASIC)
20 A$ = "The quick brown fox jumps over the lazy dog"
30 PRINT LEN(A$); " characters"
40 PRINT LEFT$(A$, 9); "|"; RIGHT$(A$, 8); "|"; MID$(A$, 11, 5)
50 PRINT UCASE$(A$)
60 PRINT INSTR(A$, "fox"); INSTR(A$, "cat")
70 FOR I = 1 TO LEN(A$)
80   C$ = MID$(A$, I, 1)
90   IF C$ = " " THEN W = W + 1
100 NEXT I
110 PRINT "words:"; W + 1
120 R$ = ""
130 FOR I = LEN(A$) TO 1 STEP -1: R$ = R$ + MID$(A$, I, 1): NEXT I
140 PRINT R$
150 PRINT ASC("A"); CHR$(72); CHR$(105); STR$(3.5); VAL("42abc")
160 PRINT HEX$(255); " "; OCT$(64); " "; STRING$(5, "-"); "|"; SPACE$(3); "|"
