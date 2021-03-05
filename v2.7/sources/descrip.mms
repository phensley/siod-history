! VMS MAKEFILE (using MMS)!

CFLAGS = /DEBUG/LIST/SHOW=(NOSOURCE)/OPTIMIZE=(NOINLINE)/STANDARD=PORTABLE

OBJS = siod.obj,slib.obj,sliba.obj

siod.exe depends_on $(OBJS),siod.opt
 dflag = ""
 if f$type(setdebug) .nes. "" then dflag = "/DEBUG"
 link'dflag'/exe=siod.exe $(OBJS),siod.opt/opt
 if f$type(setdebug) .nes. "" then setdebug siod.exe 0
 ! re-execute the next line in your superior process:
 siod == "$" + f$env("DEFAULT") + "SIOD"

DISTRIB depends_on siod.shar,siod.1_of_1
 !(ALL DONE)

siod.obj depends_on siod.c,siod.h,

slib.obj depends_on slib.c,siod.h,siodp.h
sliba,obj depends_on sliba.c,siod.h,siodp.h

DISTRIB_FILES = MAKEFILE.,README.,SIOD.1,SIOD.C,SIOD.DOC,SIOD.H,SIOD.SCM,\
                SLIB.C,SIOD.TIM,MAKEFILE.COM,PRATT.SCM,DESCRIP.MMS,SIOD.OPT,\
                SHAR.DB,SIODP.H,SLIBA.C,SIODM.C

siod.shar depends_on $(DISTRIB_FILES)
 minishar siod.shar shar.db

SIOD.1_OF_1  depends_on $(DISTRIB_FILES)
 DEFINE share_max_part_size 300
 @NTOOLS_DIR:VMS_SHARE "$(DISTRIB_FILES)" SIOD
