/* Scheme In One Defun, but in C this time.

 * COPYRIGHT (c) 1988-1994 BY *
 * PARADIGM ASSOCIATES INCORPORATED, CAMBRIDGE, MASSACHUSETTS. *
 * See the source file SLIB.C for more information. *

*/

/*

g...@paradigm.com or g...@mitech.com or g...@world.std.com

Paradigm Associates Inc Phone: 617-492-6079
29 Putnam Ave, Suite 6
Cambridge, MA 02138

An example main-program call with some customized subrs.

 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef THINK_C
#include <console.h>
#endif

#include "siod.h"

LISP my_one;
LISP my_two;
LISP my_99;
LISP my_0;


LISP cfib(LISP x);
LISP clooptest(LISP x,LISP f);

#ifdef VMS
LISP vms_debug(LISP cmd);
#endif

#ifdef WIN32
#include <windows.h>
LISP win32_debug(void)
{DebugBreak();
 return(NIL);}
#endif

int main(int argc,char **argv)
{int j,xflag = 0,retval = 0;
 char *linebuffer = NULL,*ptr;
 print_welcome();
#ifdef THINK_C
 argc = ccommand(&argv);
#endif
 for(j=1;j<argc;++j)
 if (strcmp(argv[j],"x") == 0)
 xflag = 1;
 else if (strncmp(argv[j],"-e",2) == 0)
 {xflag = 2;
 linebuffer = &argv[j][2];}
 process_cla(argc,argv,(xflag) ? 0 : 1);
 print_hs_1();
 init_storage();
 init_subrs();
 init_trace();
 my_one = flocons((double) 1.0);
 my_two = flocons((double) 2.0);
 my_99 = flocons((double) 99.0);
 my_0 = flocons((double) 0.0);
 gc_protect(&my_one);
 gc_protect(&my_two);
 gc_protect(&my_99);
 gc_protect(&my_0);
 init_subr_1("cfib",cfib);
 init_subr_2("cloop-test",clooptest);
#ifdef VMS
 init_subr_1("vms-debug",vms_debug);
#endif
#ifdef WIN32
 init_subr_0("win32-debug",win32_debug);
#endif
#ifdef INIT_EXTRA
 INIT_EXTRA();
#endif
 switch(xflag)
 {case 0:
 retval = repl_driver(1,1,NULL);
 break;
 case 1:
 printf("Using repl_c_string\n");
 linebuffer = (char *) malloc(256);
 while(fgets(linebuffer,256,stdin))
 {if ((ptr = strchr(linebuffer,'\n'))) *ptr = 0;
 retval = repl_c_string(linebuffer,1,xflag,0);
 xflag = 0;}
 break;
 case 2:
 retval = repl_c_string(linebuffer,1,xflag,1);
 break;}
 printf("EXIT\n");
 exit(retval);
 return(0);}

/* This is cfib, (compiled fib). Test to see what the overhead
 of interpretation actually is in a given implementation benchmark
 standard-fib against cfib.

 (define (standard-fib x)
 (if (< x 2)
 x
 (+ (standard-fib (- x 1))
 (standard-fib (- x 2)))))

*/

LISP cfib(LISP x)
{if NNULLP(lessp(x,my_two))
 return(x);
 else
 return(plus(cfib(difference(x,my_one)),
 cfib(difference(x,my_two))));}

/* compiled version of loop-test from siod.scm
 This won't number-cons for n up to 99 (with default arguments).
 Another test of overhead of interpretation */

LISP clooptest(LISP n,LISP f)
{LISP j,k,m,result;
 j = my_0;
 result = NIL;
 while(NNULLP(lessp(j,n)))
 {j = plus(j,my_one);
 k = my_0;
 while(NNULLP(lessp(k,my_99)))
 {k = plus(k,my_one);
 m = my_0;
 while(NNULLP(lessp(m,my_99)))
 {m = plus(m,my_one);
 if NNULLP(f) result = cons(NIL,result);}}}
 return(result);}

#ifdef VMS

#include <ssdef.h>
#include <descrip.h>
#include <lib$routines.h>

LISP vms_debug(arg)
 LISP arg;
{unsigned char arg1[257];
 char *data;
 if NULLP(arg)
 lib$signal(SS$_DEBUG,0);
 else
 {data = get_c_string(arg);
 arg1[0] = strlen(data);
 memcpy(&arg1[1],data,arg1[0]);
 lib$signal(SS$_DEBUG,1,arg1);}
 return(NIL);}

#endif
