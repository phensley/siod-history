$! VMS BUILD PROCEDURE FOR SIOD, ALTERNATIVE TO USING DESCRIP.MMS.
$! If P1 = EXTRA then P1 = name of extra module
$!
$ CFLAGS = ""
$ LFLAGS = ""
$ optarg = ",siod.opt/opt"
$ if f$getsyi("SID") .lt. 0 then optarg = ""
$ IF P1 .NES. "EXTRA" THEN GOTO TAG1
$ CFLAGS = CFLAGS + "/DEFINE=(""INIT_EXTRA=init_" + f$edit(P2,"LOWERCASE") + -
 """)"
$ OPTARG = OPTARG + "," + P2 + ".OBJ"
$ CC'CFLAGS' 'P2'.C
$TAG1:
$!
$ CC'CFLAGS' SLIB.C
$ CC'CFLAGS' SLIBA.C
$ CC'CFLAGS' SIOD.C
$ CC'CFLAGS' TRACE.C
$ IF F$EDIT(P2,"UPCASE") .EQS. "SQL_ORACLE" THEN GOTO LINK_WITH_ORACLE
$ LINK'LFLAGS' SIOD.OBJ,SLIB.OBJ,SLIBA.OBJ,TRACE.OBJ'OPTARG'
$GOTO DEF
$LINK_WITH_ORACLE:
$ @ORA_RDBMS:LNOCIC SIOD.EXE SIOD.OBJ,SLIB.OBJ,SLIBA.OBJ,-
TRACE.OBJ,SQL_ORACLE.OBJ "S"
$DEF:
$ SIOD == "$" + F$ENV("DEFAULT") + "SIOD"
