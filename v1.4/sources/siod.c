/* Scheme In One Defun, but in C this time.
 
 *                        COPYRIGHT (c) 1989 BY                             *
 *        PARADIGM ASSOCIATES INCORPORATED, CAMBRIDGE, MASSACHUSETTS.       *
 *			   ALL RIGHTS RESERVED                              *

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of Paradigm Associates Inc
not be used in advertising or publicity pertaining to distribution of
the software without specific, written prior permission.

PARADIGM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
PARADIGM BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

*/

/*

gjc@paradigm.com

Paradigm Associates Inc          Phone: 617-492-6079
29 Putnam Ave, Suite 6
Cambridge, MA 02138

  */

#include <stdio.h>

#include "siod.h"

/* This illustrates calling the main program entry points and enabling our
   own example subrs */

main(argc,argv)
 int argc; char **argv;
{print_welcome();
 process_cla(argc,argv);
 print_hs_1();
 init_storage();
 init_subrs();
 our_subrs();
 repl_driver();
 printf("EXIT\n");}

/* This is cfib, for compiled fib. Test to see what the overhead
   of interpretation actually is in a given implementation 
 */

LISP my_one;
LISP my_two;

/*   (define (standard-fib x)
       (if (< x 2)
         x
         (+ (standard-fib (- x 1))
	    (standard-fib (- x 2)))))  
*/

LISP cfib(x)
     LISP x;
{if NNULLP(lessp(x,my_two))
   return(x);
 else
   return(plus(cfib(difference(x,my_one)),
	       cfib(difference(x,my_two))));}


#ifdef vms
#include <descrip.h>
#include <ssdef.h>
LISP sys_edit(fname)
 LISP fname;
{struct dsc$descriptor_s d;
 long iflag;
 if NTYPEP(fname,tc_symbol) err("filename not a symbol",fname);
 d.dsc$b_dtype = DSC$K_DTYPE_T;
 d.dsc$b_class = DSC$K_CLASS_S;
 d.dsc$w_length = strlen(PNAME(fname));
 d.dsc$a_pointer = PNAME(fname);
 iflag = no_interrupt(1);
 edt$edit(&d);
 no_interrupt(iflag);
 return(fname);}

LISP vms_debug(v)
     LISP v;
{lib$signal(SS$_DEBUG);
 return(v);}

#endif

our_subrs()
{my_one = flocons((double) 1.0);
 my_two = flocons((double) 2.0);
 gc_protect(&my_one);
 gc_protect(&my_two);
 init_subr("cfib",tc_subr_1,cfib);
#ifdef vms
 init_subr("edit",tc_subr_1,sys_edit);
 init_subr("vms-debug",tc_subr_1,vms_debug);
#endif
}
