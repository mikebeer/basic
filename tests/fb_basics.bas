' FreeBASIC-style test
Const PI = 3.14159265
Dim a(10) As Integer
Dim s As String
Declare Function fact(n As Integer) As Integer

Function fact(n As Integer) As Integer
  If n <= 1 Then
    Return 1
  Else
    Return n * fact(n - 1)
  End If
End Function

Sub greet(who As String)
  Print "Hello, "; who; "!"
End Sub

Function add(a As Integer, b As Integer) As Integer
  add = a + b
End Function

For i = 1 To 10
  a(i) = i * i
Next
total = 0
For i = 1 To 10
  total += a(i)
Next
Print "total ="; total
Print "fact(10) ="; fact(10)
greet "world"
Call greet("again")
Print add(2, 3)
Select Case total
  Case 1 To 100
    Print "small"
  Case 385
    Print "exactly 385"
  Case Else
    Print "other"
End Select
Do
  i -= 1
Loop While i > 5
Print i
Do While i < 8
  i += 1
  If i = 7 Then Exit Do
Loop
Print i
s = "abc"
s = s & "def"
Print s, Len(s), UCase$(s), InStr(s, "cd")
For i = 1 To 3
  For j = 1 To 3
    If j = 2 Then Continue For
    Print i; ","; j;
  Next j
  Print
Next i
Print PI
