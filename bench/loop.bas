' simple FOR loop with integer and float accumulation
S = 0
T# = 0
FOR I = 1 TO 200000
  S = S + I MOD 7
  T# = T# + I * 0.5
NEXT I
PRINT S; T#
N = 0
I = 0
WHILE I < 100000
  I = I + 1
  IF I MOD 3 = 0 OR I MOD 5 = 0 THEN N = N + I
WEND
PRINT N
