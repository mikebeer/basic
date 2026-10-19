' insertion sort of 600 pseudo-random numbers (own generator, deterministic)
N = 600
DIM A(N)
X = 12345
FOR I = 1 TO N
  X = (X * 1103 + 12345) MOD 65521
  A(I) = X
NEXT I
FOR I = 2 TO N
  V = A(I)
  J = I - 1
  WHILE J >= 1 AND A(J) > V
    A(J + 1) = A(J)
    J = J - 1
  WEND
  A(J + 1) = V
NEXT I
OK = -1
FOR I = 2 TO N
  IF A(I - 1) > A(I) THEN OK = 0
NEXT I
PRINT OK; A(1); A(N \ 2); A(N)
