' ASCII Mandelbrot
Dim As Double cr, ci, zr, zi, t
Dim As Integer x, y, k
For y = -12 To 12
  s$ = ""
  For x = -39 To 20
    cr = x / 20: ci = y / 10
    zr = 0: zi = 0
    For k = 1 To 30
      t = zr * zr - zi * zi + cr
      zi = 2 * zr * zi + ci
      zr = t
      If zr * zr + zi * zi > 4 Then Exit For
    Next
    If k > 30 Then s$ += "#" Else s$ += Mid$(" .:-=+*%@", (k Mod 9) + 1, 1)
  Next
  Print s$
Next
