/* Scheme In One Defun, but in C this time.
 
 *                        COPYRIGHT (c) 1989 BY                             *
 *        PARADIGM ASSOCIATES INCORPORATED, CAMBRIDGE, MASSACHUSETTS.       *
 *        See the source file SLIB.C for more information.                  *

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

LISP our_gc_mark();
void our_gc_free();
void our_print();
LISP our_readm();
LISP our_eval();

main(argc,argv)
 int argc; char **argv;
{long gc_kind;
 print_welcome();
 process_cla(argc,argv,1);
 set_gc_hooks(NULL,NULL,NULL,NULL,&gc_kind);
 print_hs_1();
 init_storage();
 init_subrs();
 if (gc_kind == 0)
   {set_gc_hooks(NULL,NULL,our_gc_mark,our_gc_free,&gc_kind);
    set_read_hooks("\"","\"",our_readm,NULL);
    set_eval_hooks(our_eval);
    set_print_hooks(our_print);}
 our_subrs((gc_kind == 0) ? 1 : 0);
 repl_driver(1,1);
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

LISP our_gc_mark(ptr)
     LISP ptr;
{return(NIL);}

void our_gc_free(ptr)
     LISP ptr;
{free(PNAME(ptr));
 PNAME(ptr) = 0;}

void our_print(ptr,f)
     LISP ptr;
     FILE *f;
{fput_st(f,"\"");
 fput_st(f,PNAME(ptr));
 fput_st(f,"\"");}

#define tc_string tc_user_1

LISP strcons(length)
     long length;
{long flag;
 LISP s;
 s = symcons("",NIL);
 flag = no_interrupt(1);
 PNAME(s) = must_malloc(length);
 no_interrupt(flag);
 (*s).type = tc_string;
 return(s);}

LISP string_append(args)
     LISP args;
{long size;
 LISP l,s;
 char *data;
 size = 0;
 for(l=args;NNULLP(l);l=cdr(l))
   {s = car(l);
    if (NTYPEP(s,tc_symbol) && NTYPEP(s,tc_string))
      err("wta to string-append",s);
    size = size + strlen(PNAME(s));}
 s = strcons(size+1);
 data = PNAME(s);
 data[0] = 0;
 for(l=args;NNULLP(l);l=cdr(l))
   strcat(data,PNAME(car(l)));
 return(s);}

LISP our_readm(tc,f)
     int tc;
     struct gen_readio *f;
{char temp[100];
 int c;
 long j;
 LISP s;
 j = 0;
 while(((c = GETC_FCN(f)) != tc) && (c != EOF))
   {if ((j + 2) > sizeof(temp)) err("read string overflow",NIL);
    temp[j] = c;
    ++j;}
 s = strcons(j+1);
 temp[j] = 0;
 strcpy(PNAME(s),temp);
 return(s);}

LISP our_eval(obj,formp,envp)
     LISP obj,*formp,*envp;
{LISP ind;
 char buff[2];
 long n,j;
 if NTYPEP(obj,tc_string) err("eval bug",obj);
 n = strlen(PNAME(obj));
 ind = leval(car(cdr(*formp)),*envp);
 if NFLONUMP(ind) err("non numeric string index",ind);
 j = (long) FLONM(ind);
 if ((j < 0) || (j >= n)) err("string index out of range",ind);
 buff[0] = PNAME(obj)[j];
 buff[1] = 0;
 *formp = rintern(buff);
 return(NIL);}

int rfs_getc(p)
     unsigned char **p;
{int i;
 i = **p;
 if (!i) return(EOF);
 *p = *p + 1;
 return(i);}

void rfs_putc(c,p)
     unsigned char c,**p;
{*p = *p - 1;}

LISP read_from_string(x)
     LISP x;
{char *p;
 if NTYPEP(x,tc_string) err("not a string",x);
 p = PNAME(x);
 return(gen_read(rfs_getc,rfs_putc,&p));}

our_subrs(flag)
     int flag;
{my_one = flocons((double) 1.0);
 my_two = flocons((double) 2.0);
 gc_protect(&my_one);
 gc_protect(&my_two);
 init_subr("cfib",tc_subr_1,cfib);
#ifdef vms
 init_subr("edit",tc_subr_1,sys_edit);
 init_subr("vms-debug",tc_subr_1,vms_debug);
#endif
 if (flag)
   {init_subr("string-append",tc_lsubr,string_append);
    init_subr("read-from-string",tc_subr_1,read_from_string);}}
