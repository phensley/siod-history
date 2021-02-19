! VMS MAKEFILE (using MMS) for SIOD.
!
! use MMS/MACRO=("LINK_PCA=1") for PCA
! use MMS/MACRO=("RELEASE=1") for no debugging.
! use MMS/MACRO=("STRIP=1") for no debugging, no symbols.
! use MMS/MACRO=("EXTRA=xxx") for extra modules. (sql_rdb for example)

.ifdef EXTRA
EOBJ = ,$(EXTRA).OBJ
CFLAGD = /DEFINE=("INIT_EXTRA=init_$(EXTRA)")
.endif

.ifdef STRIP
CFLAGS = /NODEBUG/OPTIMIZE/LIST/SHOW=(NOSOURCE)$(CFLAGD)
LINKFLAGS = /notraceback/exe=$(mms$target_name).exe
.else
.ifdef RELEASE
CFLAGS = /DEBUG=TRACEBACK/OPTIMIZE/LIST/SHOW=(NOSOURCE)$(CFLAGD)
LINKFLAGS = /traceback/exe=$(mms$target_name).exe
.else
.ifdef LINK_PCA
CFLAGS = /DEBUG/OPTIMIZE=NOINLINE/LIST/SHOW=(NOSOURCE)$(CFLAGD)
LINKFLAGS = /debug=SYS$LIBRARY:PCA$OBJ.OBJ/exe=$(mms$target_name).exe
.else
CFLAGS = /DEBUG/NOOPTIMIZE/LIST/SHOW=(NOSOURCE)$(CFLAGD)
LINKFLAGS = /debug/exe=$(mms$target_name).exe
.endif
.endif
.endif

OBJS = siod.obj,slib.obj,sliba.obj,trace.obj

siod.exe depends_on $(OBJS),siod.opt$(EOBJ)
 optarg = ",siod.opt/opt"
 if f$getsyi("SID") .lt. 0 then optarg = ""
 link$(LINKFLAGS) $(OBJS)'optarg'$(EOBJ)
 ! re-execute the next line in your superior process:
 siod == "$" + f$env("DEFAULT") + "SIOD"

DISTRIB depends_on siod.shar,siod.1_of_1
 !(ALL DONE)

siod.obj depends_on siod.c,siod.h,

slib.obj depends_on slib.c,siod.h,siodp.h
sliba.obj depends_on sliba.c,siod.h,siodp.h
trace.obj depends_on trace.c,siod.h,siodp.h

.ifdef EXTRA
$(EXTRA).obj depends_on $(EXTRA).c,siod.h
.endif

DISTRIB_FILES = MAKEFILE.,README.,SIOD.1,SIOD.C,SIOD.DOC,SIOD.H,SIOD.SCM,\
SLIB.C,SIOD.TIM,MAKEFILE.COM,PRATT.SCM,DESCRIP.MMS,SIOD.OPT,\
SHAR.DB,SIODP.H,SLIBA.C,SIODM.C,TRACE.C,\
makefile.wnt,make.bat,sql_oracle.c,sql_oracle.scm,\
sql_rdb.c,sql_rdb.scm

siod.shar depends_on $(DISTRIB_FILES)
 minishar siod.shar shar.db

SIOD.1_OF_1 depends_on $(DISTRIB_FILES)
 DEFINE share_max_part_size 500
 @NTOOLS_DIR:VMS_SHARE "$(DISTRIB_FILES)" SIOD

put_exp depends_on descrip.mms
 make_put_exp readme,siod.h,siodp.h,siod.c,slib.c,sliba.c,trace.c,\
siod.doc,siod.tim,pratt.scm,siod.scm,makefile.wnt,make.bat
