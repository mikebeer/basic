Dim Shared total As Integer
Dim words(1 To 3) As String
words(1) = "pear": words(2) = "apple": words(3) = "fig"

Sub swapit(a As Integer, b As Integer)
  Dim t As Integer
  t = a: a = b: b = t
End Sub

Sub bump(ByRef n As Integer)
  n += 1
  total += n
End Sub

Function sumarr(arr() As Integer, n As Integer) As Integer
  Dim s As Integer, i As Integer
  For i = 1 To n
    s += arr(i)
  Next
  Return s
End Function

Function counter() As Integer
  Static c As Integer
  c += 1
  Return c
End Function

Dim v(1 To 5) As Integer
For i = 1 To 5: v(i) = i * 10: Next
x = 1: y = 2
swapit x, y
Print x; y
k = 5
bump k: bump k
Print k; total
Print sumarr(v(), 5)
Print counter(); counter(); counter()

' bubble sort
n = 3
For i = 1 To n - 1
  For j = 1 To n - i
    If words(j) > words(j + 1) Then Swap words(j), words(j + 1)
  Next j
Next i
For i = 1 To n: Print words(i); " ";: Next: Print

Select Case words(1)
  Case "apple", "banana"
    Print "fruit A"
  Case Is > "m"
    Print "late"
  Case Else
    Print "else"
End Select

s$ = "x"
Do Until Len(s$) >= 5
  s$ = s$ + "y"
Loop
Print s$
If 1 = 1 Then If 2 = 3 Then Print "no" Else Print "inner else"
For i = 10 To 1 Step -3: Print i;: Next: Print
GoTo skip
Print "skipped"
skip:
Print "done"
Redim v(1 To 8) As Integer
Print UBound(v); LBound(v)
Dim g(2, 3) As Double
g(2, 3) = 7.5
Print g(2, 3), UBound(g, 1), UBound(g, 2)
Print IIf(1 > 2, "yes", "no")
Print Mid("hello world", 7), Left("abc", 2), Right("abc", 2), Trim("  pad  ") & "|"
Print String(3, "ab"), Space(2) & "|", LCase("ABC")
Print Hex(255), Oct(8), Bin(5)
Print CInt(2.7), CDbl(3), Abs(-2.5), Sgn(0), Int(2.9), Fix(-2.9)
Print 5 \ 2, 5 Mod 2, 2 ^ 3, 1 Shl 4, 256 Shr 2
