.SUFFIXES:

.SUFFIXES: .c

AT.EXE:  AT.OBJ
   @REM @<<AT.@0
     /NOI /NOE /PM:VIO /ALIGN:16 /EXEPACK /BASE:65536 /STACK:16384 +
     AT.OBJ
     AT.EXE

     TCPIPDLL.LIB +
     TCPIP.LIB
     ;
<<
   LINK386.EXE @AT.@0

{.}.c.obj:
   ICC.EXE /ss /Gm+ /C .\$*.c

!include AT.DEP
