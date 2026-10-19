' sieve of Eratosthenes up to 20000
N = 20000
DIM P(N)
FOR I = 2 TO N: P(I) = 1: NEXT I
FOR I = 2 TO INT(SQR(N))
  IF P(I) = 1 THEN
    FOR J = I * I TO N STEP I
      P(J) = 0
    NEXT J
  END IF
NEXT I
C = 0
LAST = 0
FOR I = 2 TO N
  IF P(I) = 1 THEN C = C + 1: LAST = I
NEXT I
PRINT C; LAST
