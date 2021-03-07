/* Scheme In One Defun, but in C this time.
 
 * COPYRIGHT (c) 1988-1992 BY *
 * PARADIGM ASSOCIATES INCORPORATED, CAMBRIDGE, MASSACHUSETTS. *
 * See the source file SLIB.C for more information. *

*/

/*

g...@paradigm.com

Paradigm Associates Inc Phone: 617-492-6079
29 Putnam Ave, Suite 6
Cambridge, MA 02138

An example main-program call with some customized subrs.

 */

#include <stdio.h>
#ifdef THINK_C
#include <console.h>
#endif

#include "siod.h"

LISP my_one;
LISP my_two;

LISP cfib(LISP x);

#ifdef VMS
LISP vms_debug(LISP cmd);
#endif

int main(int argc,char **argv)
{print_welcome();
#ifdef THINK_C
 argc = ccommand(&argv);
#endif
 process_cla(argc,argv,1);
 print_hs_1();
 init_storage();
 init_subrs();
 my_one = flocons((double) 1.0);
 my_two = flocons((double) 2.0);
 gc_protect(&my_one);
 gc_protect(&my_two);
 init_subr("cfib",tc_subr_1,cfib);
#ifdef VMS
 init_subr("vms-debug",tc_subr_1,vms_debug);
#endif
 repl_driver(1,1);
 printf("EXIT\n");}

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

#ifdef VMS

#include <ssdef.h>
#include <descrip.h>

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
