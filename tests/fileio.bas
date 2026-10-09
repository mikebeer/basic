' Write a file, read it back
Dim f As Integer = FreeFile
Dim fname As String = "/tmp/crexx_basic_demo.txt"
Open fname For Output As #1
For i = 1 To 5
  Print #1, "Line number"; i
Next
Close #1

Open fname For Input As #2
Dim ln As String
n = 0
Do While Not Eof(2)
  Line Input #2, ln
  n += 1
  Print n; ": "; ln
Loop
Close #2
