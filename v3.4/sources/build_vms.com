$! NAME:    BUILD_VMS.COM 
$! PURPOSE: BUILD PROCEDURE FOR SIOD IN OPENVMS.
$! CREATED: 25-NOV-1997, George J. Carrette 
$! USAGE:   Create a directory structure, [SIOD], [SIOD.SRC]
$!          and move this file to the parent
$!          directory, and all other sources to the .SRC subdirectory.
$!          Running this procedure will result in [SIOD.OBJ...] and
$!          [SIOD.LIB]. The only directory needed at runtime is [SIOD.LIB],
$!          where the procedure @[SIOD.LIB]SETUP_VMS is used to define
$!          the symbols and logical names required.
$!          Alternatively this procedure takes P1...P8 arguments
$!          to allow a different directory organization and perform
$!          other actions. For example, build in-place:
$!          @BUILD_VMS BUILD SYS$DISK:[]
$!
$! NOTE:    It was just getting too difficult to use MMS to encode
$!          all that was needed here, without wrapping a COM file
$!          which was nearly as complex as this. The 20-line MAKE
$!          subroutine is good enough to allow for efficient development,
$!          to minimize repeated recompilation and relinking.
$!
$ SELF = F$ENV("PROCEDURE")
$ SELF_DIR = F$PARSE(SELF,,,"DEVICE") + -
             F$PARSE(SELF,,,"DIRECTORY")
$ ACTION = P1
$ IF ACTION .EQS. "" THEN ACTION = "BUILD"
$ IF ACTION .EQS. "BUILD" THEN GOTO BUILD
$ IF ACTION .EQS. "EXTRACT" THEN GOTO EXTRACT
$ WRITE SYS$OUTPUT "Unknown SIOD VMS_BUILD.COM action: ''ACTION'"
$ EXIT 44
$BUILD:
$ SRC_DIR = F$EXTRACT(0,F$LENGTH(SELF_DIR) -1,SELF_DIR) +-
             ".SRC]"
$ IF P2 .NES. "" THEN SRC_DIR = P2
$ ARCH_NAME = f$getsyi("ARCH_NAME")
$ ARCH_DIR = F$EXTRACT(0,F$LENGTH(SELF_DIR) -1,SELF_DIR) +-
             ".LIB." + ARCH_NAME + "]"
$ ARCH_SET = F$EXTRACT(0,F$LENGTH(SELF_DIR) -1,SELF_DIR) + ".LIB]"
$ IF P3 .NES. "" THEN ARCH_DIR = P4
$ OBJ_DIR = F$EXTRACT(0,F$LENGTH(SELF_DIR) -1,SELF_DIR) +-
             ".OBJ." + ARCH_NAME + "]"
$ IF P4 .NES. "" THEN OBJ_DIR = P4
$ CREATE/DIR 'ARCH_DIR','OBJ_DIR'
$ DEFINE/JOB SIODSHR     SIOD_LIB:SIODSHR.EXE
$ DEFINE/JOB HS_REGEXSHR SIOD_LIB:HS_REGEXSHR.EXE
$ DEFINE/JOB SIOD_LIB 'ARCH_DIR'
$ DEFINE/JOB SIOD_SRC 'SRC_DIR'
$ DEFINE/JOB SIOD_OBJ 'OBJ_DIR'
$ SIOD == "$SIOD_LIB:SIOD"
$ CSIOD == "$SIOD_LIB:CSIOD"
$!
$ IGNORE_REASONABLE = "IGNORECALLVAL"
$ IGNORE_EXTRA = ",IMPLICITFUNC,STRCTPADDING,CONTROLASSIGN,CXXKEYWORD," +-
                 "FALLOFFEND,KNRFUNC,MACROREDEF,MAINPARM,NESTEDTYPE," +-
                 "NOPARMLIST,PTRMISMATCH,SWITCHLONG,VALUEPRES"
$! The above EXTRA list is too long for the DCL command buffer.
$! Hence, just turning off warnings.
$ IG = IGNORE_REASONABLE ! + IGNORE_EXTRA
$! CWARN = "/WARNINGS=(ENABLE=ALL,DISABLE=(''IG')"
$ CWARN = "/NOWARNINGS"
$!
$ CFLAGS = "/DECC''CWARN'/LIST=SIOD_OBJ:"
$!
$ LFLAGS="/MAP=SIOD_OBJ:/FULL"
$!
$ LIB_MODULES = "SLIB,SLIBA,TRACE,SLIBU,MD5"
$!
$ J = 0
$ OBJ_FILES = ""
$ LOOP_LIB:
$ M = F$ELEMENT(J,",",LIB_MODULES)
$ IF M .EQS. "," THEN GOTO ENDLOOP_LIB
$ CALL MAKE "SIOD_OBJ:''M'.OBJ" - 
            "SIOD_SRC:''M'.C" -
            "CC''CFLAGS'/OBJ=SIOD_OBJ:''M'.OBJ SIOD_SRC:''M'.C"
$ J = J + 1
$ OBJ_FILES = OBJ_FILES + "SIOD_OBJ:''M'.OBJ,"
$ GOTO LOOP_LIB
$ENDLOOP_LIB:
$ OPT_FILE  = "SIOD_OBJ:SIODSHR_''ARCH_NAME'.OPT"
$ CALL MAKE "''OPT_FILE'" "SIOD_SRC:VMS_OPT_FILES.TXT" -
            "@''SELF' EXTRACT ''OPT_FILE' SIOD_SRC:VMS_OPT_FILES.TXT"
$ CALL MAKE "SIOD_LIB:SIODSHR.EXE" -
            "''OBJ_FILES'''OPT_FILE'" -
            "LINK''LFLAGS'/SHARE=SIOD_LIB:SIODSHR.EXE ''OPT_FILE'/OPT"
$!
$ CALL MAKE "SIOD_OBJ:SIOD.OBJ" -
            "SIOD_SRC:SIOD.C" -
            "CC''CFLAGS'/OBJ=SIOD_OBJ:SIOD.OBJ SIOD_SRC:SIOD.C"
$ OPT_FILE = "SIOD_OBJ:SIOD_''ARCH_NAME'.OPT"
$ CALL MAKE "''OPT_FILE'" "SIOD_SRC:VMS_OPT_FILES.TXT" -
            "@''SELF' EXTRACT ''OPT_FILE' SIOD_SRC:VMS_OPT_FILES.TXT"
$ CALL MAKE "SIOD_LIB:SIOD.EXE" -
            "SIOD_OBJ:SIOD.OBJ,SIOD_LIB:SIODSHR.EXE,''OPT_FILE'" -
  "LINK''LFLAGS'/EXE=SIOD_LIB:SIOD.EXE SIOD_OBJ:SIOD.OBJ,''OPT_FILE'/OPT"
$!
$ CALL MAKE "SIOD_OBJ:PARSER_PRATT.OBJ" -
            "SIOD_SRC:PARSER_PRATT.C" -
            "CC''CFLAGS'/OBJ=SIOD_OBJ:PARSER_PRATT.OBJ SIOD_SRC:PARSER_PRATT.C"
$ OPT_FILE = "SIOD_OBJ:PARSER_PRATT_''ARCH_NAME'.OPT"
$ CALL MAKE "''OPT_FILE'" "SIOD_SRC:VMS_OPT_FILES.TXT" -
            "@''SELF' EXTRACT ''OPT_FILE' SIOD_SRC:VMS_OPT_FILES.TXT"
$ CALL MAKE "SIOD_LIB:PARSER_PRATT.EXE" -
            "SIOD_OBJ:PARSER_PRATT.OBJ,SIOD_LIB:SIODSHR.EXE,''OPT_FILE'" -
  "LINK''LFLAGS'/SHARE=SIOD_LIB:PARSER_PRATT.EXE ''OPT_FILE'/OPT"
$!
$ CALL MAKE "SIOD_OBJ:TAR.OBJ" -
            "SIOD_SRC:TAR.C" -
            "CC''CFLAGS'/OBJ=SIOD_OBJ:TAR.OBJ SIOD_SRC:TAR.C"
$ OPT_FILE = "SIOD_OBJ:TAR_''ARCH_NAME'.OPT"
$ CALL MAKE "''OPT_FILE'" "SIOD_SRC:VMS_OPT_FILES.TXT" -
            "@''SELF' EXTRACT ''OPT_FILE' SIOD_SRC:VMS_OPT_FILES.TXT"
$ CALL MAKE "SIOD_LIB:TAR.EXE" -
            "SIOD_OBJ:TAR.OBJ,SIOD_LIB:SIODSHR.EXE,''OPT_FILE'" -
  "LINK''LFLAGS'/SHARE=SIOD_LIB:TAR.EXE ''OPT_FILE'/OPT"
$!
$ IF F$TRN("MULTINET_ROOT") .EQS. "" THEN GOTO SKIP_TCP
$ CALL MAKE "SIOD_OBJ:SS.OBJ" -
            "SIOD_SRC:SS.C" -
            "CC''CFLAGS'/DEFINE=MULTINET/OBJ=SIOD_OBJ:SS.OBJ SIOD_SRC:SS.C"
$ OPT_FILE = "SIOD_OBJ:SS_''ARCH_NAME'.OPT"
$ CALL MAKE "''OPT_FILE'" "SIOD_SRC:VMS_OPT_FILES.TXT" -
            "@''SELF' EXTRACT ''OPT_FILE' SIOD_SRC:VMS_OPT_FILES.TXT"
$ CALL MAKE "SIOD_LIB:SS.EXE" -
            "SIOD_OBJ:SS.OBJ,SIOD_LIB:SIODSHR.EXE,''OPT_FILE'" -
  "LINK''LFLAGS'/SHARE=SIOD_LIB:SS.EXE ''OPT_FILE'/OPT"
$SKIP_TCP:
$!
$ SYBASE = F$TRN("SYBASE")
$ IF SYBASE .EQS. "" THEN GOTO SKIP_SYBASE
$ INC = F$EXTRACT(0,F$LENGTH(SYBASE) - 1,SYBASE) + ".INCLUDE]"
$ I = "/INCLUDE=(''INC')"
$ L = F$EXTRACT(0,F$LENGTH(SYBASE) - 1,SYBASE) + ".LIB]"
$ CALL MAKE "SIOD_OBJ:SQL_SYBASE.OBJ" -
            "SIOD_SRC:SQL_SYBASE.C" -
            "CC''CFLAGS'''I'/OBJ=SIOD_OBJ:SQL_SYBASE.OBJ SIOD_SRC:SQL_SYBASE.C"
$ CALL MAKE "SIOD_OBJ:SQL_SYBASEC.OBJ" -
            "SIOD_SRC:SQL_SYBASEC.C" -
            "CC''CFLAGS'''I'/OBJ=SIOD_OBJ:SQL_SYBASEC.OBJ SIOD_SRC:SQL_SYBASEC.C"
$ OPT_FILE = "SIOD_OBJ:SQL_SYBASE_''ARCH_NAME'.OPT"
$ CALL MAKE "''OPT_FILE'" "SIOD_SRC:VMS_OPT_FILES.TXT" -
            "@''SELF' EXTRACT ''OPT_FILE' SIOD_SRC:VMS_OPT_FILES.TXT"
$ CALL MAKE "SIOD_LIB:SQL_SYBASE.EXE" -
            "SIOD_OBJ:SQL_SYBASE.OBJ,SIOD_OBJ:SQL_SYBASEC.OBJ,SIOD_LIB:SIODSHR.EXE,''OPT_FILE'" -
  "LINK''LFLAGS'/SHARE=SIOD_LIB:SQL_SYBASE.EXE ''OPT_FILE'/OPT"
$SKIP_SYBASE:
$!
$ CALL MAKE "SIOD_OBJ:REGCOMP.OBJ" - 
            "SIOD_SRC:REGCOMP.C" -
            "CC''CFLAGS'/OBJ=SIOD_OBJ:REGCOMP.OBJ SIOD_SRC:REGCOMP.C"
$ CALL MAKE "SIOD_OBJ:REGERROR.OBJ" - 
            "SIOD_SRC:REGERROR.C" -
            "CC''CFLAGS'/OBJ=SIOD_OBJ:REGERROR.OBJ SIOD_SRC:REGERROR.C"
$ CALL MAKE "SIOD_OBJ:REGEXEC.OBJ" - 
            "SIOD_SRC:REGEXEC.C" -
            "CC''CFLAGS'/OBJ=SIOD_OBJ:REGEXEC.OBJ SIOD_SRC:REGEXEC.C"
$ CALL MAKE "SIOD_OBJ:REGFREE.OBJ" - 
            "SIOD_SRC:REGFREE.C" -
            "CC''CFLAGS'/OBJ=SIOD_OBJ:REGFREE.OBJ SIOD_SRC:REGFREE.C"
$!
$ OPT_FILE = "SIOD_OBJ:HS_REGEX_''ARCH_NAME'.OPT"
$ CALL MAKE "''OPT_FILE'" "SIOD_SRC:VMS_OPT_FILES.TXT" -
            "@''SELF' EXTRACT ''OPT_FILE' SIOD_SRC:VMS_OPT_FILES.TXT"
$ CALL MAKE "SIOD_LIB:HS_REGEXSHR.EXE" -
            "siod_obj:regcomp.obj,siod_obj:regerror.obj,siod_obj:regexec.obj,siod_obj:regfree.obj,''OPT_FILE'" -
  "LINK''LFLAGS'/SHARE=SIOD_LIB:HS_REGEXSHR.EXE ''OPT_FILE'/OPT"
$!
$ CALL MAKE "SIOD_OBJ:REGEX.OBJ" -
            "SIOD_SRC:REGEX.C" -
            "CC''CFLAGS'/INCLUDE=SIOD_SRC:/OBJ=SIOD_OBJ:REGEX.OBJ SIOD_SRC:REGEX.C"
$ OPT_FILE = "SIOD_OBJ:REGEX_''ARCH_NAME'.OPT"
$ CALL MAKE "''OPT_FILE'" "SIOD_SRC:VMS_OPT_FILES.TXT" -
            "@''SELF' EXTRACT ''OPT_FILE' SIOD_SRC:VMS_OPT_FILES.TXT"
$ CALL MAKE "SIOD_LIB:REGEX.EXE" -
            "SIOD_OBJ:REGEX.OBJ,SIOD_LIB:SIODSHR.EXE,SIOD_LIB:HS_REGEXSHR.EXE,''OPT_FILE'" -
  "LINK''LFLAGS'/SHARE=SIOD_LIB:REGEX.EXE ''OPT_FILE'/OPT"
$!
$CMDFILES = "csiod,snapshot-dir,snapshot-compare,http-get,cp-build," +-
            "ftp-cp,ftp-put,ftp-test,ftp-get,http-stress,proxy-server"
$!
$LIBFILES = "fork-test.scm,http-server.scm,http-stress.scm,http.scm," +-
            "maze-support.scm,pratt.scm,siod.scm,smtp.scm,sql_oracle.scm," +-
            "sql_rdb.scm,sql_sybase.scm,cgi-echo.scm,find-files.scm," +-
            "parser_pratt.scm,pop3.scm,selfdoc.scm," +-
            "piechart.scm,cgi.scm,ftp.scm," +-
            "sql_msql.scm"
$!
$ DOCFILES = "CP-BUILD.TXT,CSIOD.TXT,FTP-CP.TXT,FTP-GET.TXT," +-
             "FTP-PUT.TXT,FTP-TEST.TXT,HTTP-GET.TXT,HTTP-STRESS.TXT," +-
             "PROXY-SERVER.TXT,SIOD.TXT,SNAPSHOT-COMPARE.TXT," +-
             "SNAPSHOT-DIR.TXT,SIOD.HTML,SIOD_REGEX.HTML,hello.scm"
$!
$ CALL MAKE "''ARCH_SET'SETUP_VMS.COM" "SIOD_SRC:SETUP_VMS.COM" -
            "COPY  SIOD_SRC:SETUP_VMS.COM ''ARCH_SET'"
$!
$ WRITE SYS$OUTPUT "CMDFILES" 
$ J = 0
$LOOP2:
$ F = F$ELEMENT(J,",",CMDFILES)
$ IF F .EQS. "," THEN GOTO ENDLOOP2
$ CALL MAKE "SIOD_LIB:''F'.EXE" -
            "SIOD_SRC:''F'.SMD,SIOD_LIB:SIOD.EXE" -
  "SIOD -v01,-m2 SIOD_SRC:CSIOD.SMD :o=SIOD_LIB:''F'.EXE SIOD_SRC:''F'.SMD"
$ J = J + 1
$ GOTO LOOP2
$ENDLOOP2:
$!
$ WRITE SYS$OUTPUT "LIBFILES" 
$ J = 0 
$LOOP1:
$ F = F$ELEMENT(J,",",LIBFILES)
$ IF F .EQS. "," THEN GOTO ENDLOOP1
$ CALL MAKE "SIOD_LIB:''F'" "SIOD_SRC:''F'" -
  "CSIOD SIOD_SRC:''F' :o=SIOD_LIB:''F' :m=0 :v=3 :i=/bin/false"
$ J = J + 1
$ GOTO LOOP1
$ENDLOOP1:
$!
$ WRITE SYS$OUTPUT "DOCFILES"
$ J = 0 
$LOOP3:
$ F = F$ELEMENT(J,",",DOCFILES)
$ IF F .EQS. "," THEN GOTO ENDLOOP3
$ CALL MAKE "SIOD_LIB:''F'" "SIOD_SRC:''F'" -
            "COPY SIOD_SRC:''F' SIOD_LIB:''F'"
$ J = J + 1
$ GOTO LOOP3
$ENDLOOP3:
$EXIT
$!
$ MAKE:SUBROUTINE
$ RETVAL = 1
$ T1 = ""
$ T1 = F$FILE(P1,"RDT")
$ IF T1 .EQS. "" THEN GOTO DOIT
$ T1 = F$CVTIME(T1)
$! WRITE SYS$OUTPUT "''T1' -> ''P1'"
$ J = 0
$LOOP:
$ E = F$ELEMENT(J,",",P2)
$ IF E .EQS. "," THEN GOTO END
$ T2 = F$CVTIME(F$FILE(E,"RDT"))
$! WRITE SYS$OUTPUT "''T2' <- ''E'"
$ IF T2 .GTS. T1 THEN GOTO DOIT
$ J = J + 1
$ GOTO LOOP
$DOIT:
$ WRITE SYS$OUTPUT P3
$ 'P3'
$ RETVAL = $STATUS
$END:
$EXIT RETVAL
$ENDSUBROUTINE
$!
$EXTRACT:
$ ELEMENT = F$edit(p2,"UPCASE")
$ ARCHIVE = P3
$ MARKER = "START ''ELEMENT'"
$ OPEN/READ EXTRACT_IN 'ARCHIVE'
$ OPEN/WRITE EXTRACT_OUT 'ELEMENT'
$ FOUND_MARKER = "FALSE"
$LOOP_EXTRACT:
$ READ/END=EXTRACT_RAW_EOF EXTRACT_IN REC
$ IF FOUND_MARKER THEN GOTO EXTRACT_FOUND
$ IF REC .NES. MARKER THEN GOTO LOOP_EXTRACT
$ FOUND_MARKER = "TRUE"
$ GOTO LOOP_EXTRACT
$EXTRACT_FOUND:
$ IF REC .EQS. "EOF" THEN GOTO EXTRACT_EOF
$ WRITE EXTRACT_OUT REC
$ GOTO LOOP_EXTRACT
$EXTRACT_EOF:
$ CLOSE EXTRACT_IN
$ CLOSE EXTRACT_OUT
$ EXIT
$!
$EXTRACT_RAW_EOF:
$ CLOSE EXTRACT_IN
$ CLOSE EXTRACT_OUT
$ WRITE SYS$OUTPUT "Error: got raw EOF in state ''FOUND_MARKER'"
$ EXIT 44