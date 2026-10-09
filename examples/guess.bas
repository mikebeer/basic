10 REM Number guessing game (interactive; pipe numbers on stdin to try it)
20 RANDOMIZE TIMER
30 N = INT(RND * 100) + 1
40 TRIES = 0
50 INPUT "Guess a number between 1 and 100"; G
60 TRIES = TRIES + 1
70 IF G < N THEN PRINT "Too low": GOTO 50
80 IF G > N THEN PRINT "Too high": GOTO 50
90 PRINT "You got it in"; TRIES; "tries!"
