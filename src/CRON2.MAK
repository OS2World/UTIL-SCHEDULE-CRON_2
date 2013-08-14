.SUFFIXES:

.SUFFIXES: .c

!IFNDEF TCPIP
OBJS = CRON2.OBJ CRONAT.OBJ
TCP =
!ELSE
OBJS = CRON2.OBJ CRONTCP.OBJ CRONAT.OBJ
TCP = TCPIPDLL.LIB TCPIP.LIB
!ENDIF

CRON2.EXE:  $(OBJS)
   @REM @<<CRON2.@0
     /NOI /NOE /PM:VIO /ALIGN:16 /EXEPACK /BASE:65536 /STACK:16384 +
     $(OBJS)
     CRON2.EXE

     $(TCP)
     ;
<<
   LINK386.EXE @CRON2.@0

{.}.c.obj:
!IFDEF TCPIP
   ICC.EXE /DTCPIP /ss /Gm+ /C .\$*.c
!ELSE
   ICC.EXE /ss /Gm+ /C .\$*.c
!ENDIF

#===================================================================
#
#   Dependencies
#
#===================================================================

all: CRON2.EXE

CRON2.OBJ   :  CRON2.C CRON2.H CRON2.MAK
CRONAT.OBJ  :  CRONAT.C CRON2.H
!IFDEF TCPIP
CRONTCP.OBJ :  CRONTCP.C CRON2.H CRON2.MAK
!ENDIF
