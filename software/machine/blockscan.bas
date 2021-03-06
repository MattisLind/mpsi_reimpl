
10 REM**************************
20 REM** BLOCK SCAN
30 REM** 2013 NOV / BH
40 REM** REQUIRES MPEEK() & MBAND() FUNCTIONS
50 REM**************************
60 REM** BLOCK ADDRESSES & LABELS
70 REM**************************
80 DATA 6144,"ROM (BASIC)"
90 DATA 7168,"ROM (TAPE) "
100 DATA 8192,"CARD A     "
110 DATA 9216,"CARTRIDGE C"
120 DATA 10240,"CARTRIDGE B"
130 DATA 11264,"CARD B     "
140 DATA 12288,"CARTRIDGE A"
150 DATA 13312,"CARD C     "
160 DATA 14336,"CARD D     "
170 DATA 15360,"CARTRIDGE D"
180 DATA 16384,"CARTRIDGE E"
190 DATA 24576,"RAM        "
200 DATA 0,""
210 REM************************
220 REM** INIT
230 REM************************
240 PRINT "*** HP9830 BLOCK SCAN ***"
250 DIM C$[64],B$[20],S$[20]
260 C$=" !??#$%&'()*+,-./0123456789:;<=>?ABCDEFGHIJKLMNOPQRSTUVWXYZ???^?"
270 FORMAT F6.0,": ID=",F6.0
280 FORMAT F6.0,": EMPTY"
290 FORMAT 10X,"ENTRY",F3.0,": CODE=",F3.0,"  EXEC=",F7.0,2X,F1.1
300 REM************************
310 REM** SCAN BLOCKS
320 READ B,B$
330 IF B=0 THEN 360
340 GOSUB 390
350 GOTO 320
360 PRINT 
370 PRINT "*** BLOCK SCAN COMPLETE ***"
380 END 
390 REM************************
400 REM*** EXAMINE BLOCK SUBROUTINE
410 REM************************
420 B=B-1
430 PRINT 
440 PRINT "** ";B$;" / BLOCK ";
450 D=MPEEK(B)
460 IF D#0 THEN 500
470 REM** BLOCK EMPTY
480 WRITE (15,280)FNO(B)
490 RETURN 
500 REM** LIST BLOCK TABLES
510 WRITE (15,270)FNO(B),FNO(D)
520 T=FNT(1)
530 PRINT "      STATEMENTS:";FNO(T)
540 GOSUB 650
550 T=FNT(2)
560 PRINT "      COMMANDS:";FNO(T)
570 GOSUB 650
580 T=FNT(3)
590 PRINT "      FUNCTIONS:";FNO(T)
600 GOSUB 650
610 T=FNT(4)
620 PRINT "      OTHER OPS:";FNO(T)
630 GOSUB 650
640 RETURN 
650 REM************************
660 REM** EXAMINE TABLE SUBROUTINE
670 REM************************
680 A=T
690 E=0
700 D=0
710 REM*************
720 REM** ENTRY LOOP
730 S$=""
740 REM*************
750 REM** BYTE LOOP
760 IF D#0 THEN 810
770 REM** HI BYTE
780 Z=MPEEK(A)
790 Y=MBAND(INT(Z/256),255)
800 GOTO 850
810 REM** LO BYTE
820 Y=MBAND(Z,255)
830 A=A+1
840 REM*************
850 REM** PROCESS BYTE
860 D=1-D
870 IF MBAND(Y,128)#0 THEN 950
880 IF Y >= 32 AND Y<96 THEN 900
890 Y=63
900 Y=Y-32+1
910 S$[LEN(S$)+1]=C$[Y,Y]
920 GOTO 750
930 REM** END BYTE LOOP
940 REM*************
950 E=E+1
960 C=MBAND(Y,63)
970 F=T-C+MPEEK(T-C)
980 IF F >= 0 THEN 1000
990 F=F+65536
1000 WRITE (15,290)E,C,FNO(F),S$
1010 IF MBAND(Y,64)=0 THEN 730
1020 REM** END ENTRY LOOP
1030 REM*************
1040 RETURN 
1050 REM************************
1060 REM** TABLE ADDRESS FUNCTION
1070 REM************************
1080 DEF FNT(I)=B-I+MPEEK(B-I)
1090 REM************************
1100 REM** DEC TO OCTAL FUNCTION
1110 REM************************
1120 DEF FNO(D)
1130 X=D
1140 O=0
1150 P=1
1160 N=INT(X/8)
1170 O=(X-N*8)*P+O
1180 X=N
1190 P=P*10
1200 IF X#0 THEN 1160
1210 RETURN O
1220 REM************************
1230 REM** EOP
1240 REM************************
