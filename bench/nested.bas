' nested loops with arithmetic and a 2-D array
DIM M(60, 60)
FOR I = 0 TO 60
  FOR J = 0 TO 60
    M(I, J) = (I * J) MOD 11 + SQR(I + J)
  NEXT J
NEXT I
T = 0
FOR K = 1 TO 5
  FOR I = 0 TO 60
    FOR J = 0 TO 60
      T = T + M(I, J) * K - INT(M(J, I) / 3)
    NEXT J
  NEXT I
NEXT K
PRINT T
