' Sieve of Eratosthenes (FreeBASIC style)
Const N = 100
Dim sieve(2 To N) As Integer
Dim i As Integer, j As Integer, count As Integer

For i = 2 To N
  sieve(i) = 1
Next

For i = 2 To N
  If sieve(i) = 1 Then
    For j = i * i To N Step i
      sieve(j) = 0
    Next j
  End If
Next i

For i = 2 To N
  If sieve(i) = 1 Then
    Print i;
    count += 1
  End If
Next
Print
Print "There are"; count; "primes up to"; N
