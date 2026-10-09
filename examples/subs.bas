' SUB / FUNCTION, recursion, BYREF, SELECT CASE
Declare Function Fib(n As Integer) As Integer
Declare Sub Swap2(ByRef a As Integer, ByRef b As Integer)

Function Fib(n As Integer) As Integer
  If n < 2 Then Return n
  Return Fib(n - 1) + Fib(n - 2)
End Function

Sub Swap2(ByRef a As Integer, ByRef b As Integer)
  Dim t As Integer = a
  a = b
  b = t
End Sub

Function Grade$(score As Integer)
  Select Case score
    Case Is >= 90
      Grade$ = "A"
    Case 80 To 89
      Grade$ = "B"
    Case 70 To 79
      Grade$ = "C"
    Case Else
      Grade$ = "F"
  End Select
End Function

Dim x As Integer = 1, y As Integer = 2
Swap2 x, y
Print "x ="; x; " y ="; y
For i = 0 To 15
  Print Fib(i);
Next
Print
For Each_s = 55 To 95 Step 10
  Print Each_s; " -> "; Grade$(Each_s)
Next
