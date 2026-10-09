10 REM MS BASIC test
20 DIM A(5), N$(3)
30 FOR I = 1 TO 5: A(I) = I * 1.5: NEXT I
40 FOR I = 1 TO 5: PRINT A(I);: NEXT I: PRINT
50 READ X, Y$, Z
60 PRINT X; Y$; Z
70 DATA 42, "hello, world", 3.5
80 GOSUB 200
90 ON X - 41 GOTO 110, 120
100 PRINT "not reached"
110 PRINT "one": GOTO 130
120 PRINT "two"
130 PRINT USING "###.## ####  \   \ !"; 3.14159; 42; "abcdef"; "xyz"
140 PRINT USING "$$#,###.## +### **###"; 1234.5; 7; 12
150 A$ = "The quick brown fox"
160 PRINT INSTR(A$, "quick"); UCASE$(A$); STRING$(3, "*"); SPACE$(2); "|"
170 PRINT HEX$(255); OCT$(8); CHR$(65); ASC("a"); VAL("12.5abc"); STR$(7)
180 PRINT INT(-3.5); FIX(-3.5); SGN(-2); ABS(-4); CINT(2.5); CINT(3.5)
190 PRINT 7 MOD 3; 7 \ 2; 2 ^ 10; 10 / 4
195 PRINT 5 > 3; 5 < 3; NOT 0; 6 AND 3; 6 OR 3; 6 XOR 3
197 END
200 PRINT "in sub": RETURN
