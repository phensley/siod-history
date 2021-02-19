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


   Release 1.0: 24-APR-88
   Release 1.1: 25-APR-88, added: macros, predicates, load. With additions by
    Barak.Pearlmutter@DOGHEN.BOLTZ.CS.CMU.EDU: Full flonum recognizer,
    cleaned up uses of NULL/0. Now distributed with siod.scm.
   Release 1.2: 28-APR-88, name changes as requested by JAR@AI.AI.MIT.EDU,
    plus some bug fixes.
   Release 1.3: 1-MAY-88, changed env to use frames instead of alist.
    define now works properly. vms specific function edit.
   Release 1.4 20-NOV-89. Minor Cleanup and remodularization.
    Now in 3 files, siod.h, slib.c, siod.c. Makes it easier to write your
    own main loops. Some short-int changes for lightspeed C included.

  */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <signal.h>
#include <math.h>
#ifdef vms
#include <stdlib.h>
#endif

#include "siod.h"

LISP heap_1,heap_2;
LISP heap,heap_end,heap_org;

long heap_size = 5000;
long old_heap_used;
long which_heap;
long gc_status_flag = 1;
char *init_file = (char *) NULL;
char tkbuffer[TKBUFFERN];

jmp_buf errjmp;
long errjmp_ok = 0;
long nointerrupt = 1;

LISP oblist = NIL;
LISP truth = NIL;
LISP eof_val = NIL;
LISP sym_errobj = NIL;
LISP sym_progn = NIL;
LISP sym_lambda = NIL;
LISP sym_quote = NIL;
LISP open_files = NIL;
LISP unbound_marker = NIL;

process_cla(argc,argv)
 int argc; char **argv;
{int k;
 for(k=1;k<argc;++k)
   {if (strlen(argv[k])<2) continue;
    if (argv[k][0] != '-') {printf("bad arg: %s\n",argv[k]);continue;}
    switch(argv[k][1])
      {case 'h':
	 heap_size = atol(&(argv[k][2])); break;
       case 'i':
	 init_file = &(argv[k][2]); break;
       default: printf("bad arg: %s\n",argv[k]);}}}

print_welcome()
{printf("Welcome to SIOD, Scheme In One Defun, Version 1.4\n");
 printf("(C) Copyright 1988, 1989 Paradigm Associates Inc.\n");}

print_hs_1()
{printf("heap_size = %ld cells, %ld bytes\n",
        heap_size,heap_size*sizeof(struct obj));}

print_hs_2()
{printf("heap_1 at 0x%lX, heap_2 at 0x%lX\n",heap_1,heap_2);}


handle_sigfpe(sig,code,scp)
 long sig,code; struct sigcontext *scp;
{signal(SIGFPE,handle_sigfpe);
 err("floating point exception",NIL);}

handle_sigint(sig,code,scp)
 long sig,code; struct sigcontext *scp;
{signal(SIGINT,handle_sigint);
 if (nointerrupt == 0) err("control-c interrupt",NIL);
 printf("interrupts disabled\n");}

repl_driver()
{int k;
 k = setjmp(errjmp);
 if (k == 2) return;
 signal(SIGFPE,handle_sigfpe);
 signal(SIGINT,handle_sigint);
 close_open_files();
 errjmp_ok = 1;
 nointerrupt = 0;
 if (init_file && (k == 0)) vload(init_file);
 repl();}

#ifdef unix
#include <sys/types.h>
#include <sys/times.h>
struct tms time_buffer;
double myruntime()
{times(&time_buffer);
 return(time_buffer.tms_utime/60.0);}
#else
#ifdef vms
#include <time.h>
double myruntime()
{return(clock() * 1.0e-2);}
#else
double myruntime()
{long x;
 long time();
 time(&x);
 return((double) x);}
#endif
#endif

 
repl() 
{LISP x,cw;
 double rt;
 while(1)
   {if ((gc_status_flag) || heap >= heap_end)
     {rt = myruntime();
      gc();
      printf("GC took %g seconds, %ld compressed to %ld, %ld free\n",
             myruntime()-rt,old_heap_used,heap-heap_org,heap_end-heap);}
    printf("> ");
    x = lread();
    if EQ(x,eof_val) break;
    rt = myruntime();
    cw = heap;
    x = leval(x,NIL);
    printf("Evaluation took %g seconds %ld cons work\n",
	   myruntime()-rt,heap-cw);
    lprint(x);}}

err(message,x)
 char *message; LISP x;
{nointerrupt = 1;
 if NNULLP(x) 
    printf("ERROR: %s (see errobj)\n",message);
  else printf("ERROR: %s\n",message);
 if (errjmp_ok == 1) {setvar(sym_errobj,x,NIL); longjmp(errjmp,1);}
 printf("FATAL ERROR DURING STARTUP OR CRITICAL CODE SECTION\n");
 exit(1);}


LISP lerr(message,x)
     LISP message,x;
{if NTYPEP(message,tc_symbol) err("argument to error not a symbol",message);
 err(PNAME(message),x);
 return(NIL);}


LISP cons(x,y)
     LISP x,y;
{LISP z;
 if ((z = heap) >= heap_end) err("ran out of storage",NIL);
 heap = z+1;
 (*z).gc_mark = 0;
 (*z).type = tc_cons;
 CAR(z) = x;
 CDR(z) = y;
 return(z);}

LISP consp(x)
     LISP x;
{if TYPEP(x,tc_cons) return(truth); else return(NIL);}

LISP car(x)
     LISP x;
{switch TYPE(x)
   {case tc_nil:
      return(NIL);
    case tc_cons:
      return(CAR(x));
    default:
      err("wta to car",x);}}

LISP cdr(x)
     LISP x;
{switch TYPE(x)
   {case tc_nil:
      return(NIL);
    case tc_cons:
      return(CDR(x));
    default:
      err("wta to cdr",x);}}


LISP setcar(cell,value)
     LISP cell, value;
{if NTYPEP(cell,tc_cons) err("wta to setcar",cell);
 return(CAR(cell) = value);}

LISP setcdr(cell,value)
     LISP cell, value;
{if NTYPEP(cell,tc_cons) err("wta to setcdr",cell);
 return(CDR(cell) = value);}

LISP flocons(x)
 double x;
{LISP z;
 if ((z = heap) >= heap_end) err("ran out of storage",NIL);
 heap = z+1;
 (*z).gc_mark = 0;
 (*z).type = tc_flonum;
 (*z).storage_as.flonum.data = x;
 return(z);}

LISP numberp(x)
     LISP x;
{if TYPEP(x,tc_flonum) return(truth); else return(NIL);}

LISP plus(x,y)
     LISP x,y;
{if NTYPEP(x,tc_flonum) err("wta(1st) to plus",x);
 if NTYPEP(y,tc_flonum) err("wta(2nd) to plus",y);
 return(flocons(FLONM(x)+FLONM(y)));}

LISP ltimes(x,y)
 LISP x,y;
{if NTYPEP(x,tc_flonum) err("wta(1st) to times",x);
 if NTYPEP(y,tc_flonum) err("wta(2nd) to times",y);
 return(flocons(FLONM(x)*FLONM(y)));}

LISP difference(x,y)
 LISP x,y;
{if NTYPEP(x,tc_flonum) err("wta(1st) to difference",x);
 if NTYPEP(y,tc_flonum) err("wta(2nd) to difference",y);
 return(flocons(FLONM(x)-FLONM(y)));}

LISP quotient(x,y)
 LISP x,y;
{if NTYPEP(x,tc_flonum) err("wta(1st) to quotient",x);
 if NTYPEP(y,tc_flonum) err("wta(2nd) to quotient",y);
 return(flocons(FLONM(x)/FLONM(y)));}

LISP greaterp(x,y)
 LISP x,y;
{if NTYPEP(x,tc_flonum) err("wta(1st) to greaterp",x);
 if NTYPEP(y,tc_flonum) err("wta(2nd) to greaterp",y);
 if (FLONM(x)>FLONM(y)) return(truth);
 return(NIL);}

LISP lessp(x,y)
 LISP x,y;
{if NTYPEP(x,tc_flonum) err("wta(1st) to lessp",x);
 if NTYPEP(y,tc_flonum) err("wta(2nd) to lessp",y);
 if (FLONM(x)<FLONM(y)) return(truth);
 return(NIL);}

LISP eq(x,y)
 LISP x,y;
{if EQ(x,y) return(truth); else return(NIL);}

LISP eql(x,y)
 LISP x,y;
{if EQ(x,y) return(truth); else 
 if NTYPEP(x,tc_flonum) return(NIL); else
 if NTYPEP(y,tc_flonum) return(NIL); else
 if (FLONM(x) == FLONM(y)) return(truth);
 return(NIL);}

LISP symcons(pname,vcell)
 char *pname; LISP vcell;
{LISP z;
 if ((z = heap) >= heap_end) err("ran out of storage",NIL);
 heap = z+1;
 (*z).gc_mark = 0;
 (*z).type = tc_symbol;
 PNAME(z) = pname;
 VCELL(z) = vcell;
 return(z);}

LISP symbolp(x)
     LISP x;
{if TYPEP(x,tc_symbol) return(truth); else return(NIL);}

LISP symbol_boundp(x,env)
 LISP x,env;
{LISP tmp;
 if NTYPEP(x,tc_symbol) err("not a symbol",x);
 tmp = envlookup(x,env);
 if NNULLP(tmp) return(truth);
 if EQ(VCELL(x),unbound_marker) return(NIL); else return(truth);}

LISP symbol_value(x,env)
 LISP x,env;
{LISP tmp;
 if NTYPEP(x,tc_symbol) err("not a symbol",x);
 tmp = envlookup(x,env);
 if NNULLP(tmp) return(CAR(tmp));
 tmp = VCELL(x);
 if EQ(tmp,unbound_marker) err("unbound variable",x);
 return(tmp);}

LISP cintern_soft(name)
 char *name;
{LISP l;
 for(l=oblist;NNULLP(l);l=CDR(l))
   if (strcmp(name,PNAME(CAR(l))) == 0) return(CAR(l));
 return(NIL);}

LISP cintern(name)
 char *name;
{LISP sym;
 sym = cintern_soft(name);
 if(sym) return(sym);
 sym = symcons(name,unbound_marker);
 oblist = cons(sym,oblist);
 return(sym);}

char * must_malloc(size)
     unsigned long size;
{char *tmp;
 tmp = (char *) malloc(size);
 if (tmp == (char *)NULL) err("failed to allocate storage from system",NIL);
 return(tmp);}

LISP rintern(name)
 char *name;
{LISP sym;
 char *newname;
 sym = cintern_soft(name);
 if(sym) return(sym);
 newname = must_malloc(strlen(name)+1);
 strcpy(newname,name);
 sym = symcons(newname,unbound_marker);
 oblist = cons(sym,oblist);
 return(sym);}

LISP subrcons(type,name,f)
 long type; char *name; LISP (*f)();
{LISP z;
 if ((z = heap) >= heap_end) err("ran out of storage",NIL);
 heap = z+1;
 (*z).gc_mark = 0;
 (*z).type = type;
 (*z).storage_as.subr.name = name;
 (*z).storage_as.subr.f = f;
 return(z);}


LISP closure(env,code)
     LISP env,code;
{LISP z;
 if ((z = heap) >= heap_end) err("ran out of storage",NIL);
 heap = z+1;
 (*z).gc_mark = 0;
 (*z).type = tc_closure;
 (*z).storage_as.closure.env = env;
 (*z).storage_as.closure.code = code;
 return(z);}


struct gc_protected *protected_registers = NULL;

void gc_protect(location)
 LISP *location;
{struct gc_protected *reg;
 reg = (struct gc_protected *) must_malloc(sizeof(struct gc_protected));
 (*reg).location = location;
 (*reg).next = protected_registers;
  protected_registers = reg;}

scan_registers()
{struct gc_protected *reg;
 for(reg = protected_registers; reg; reg = (*reg).next)
   *((*reg).location) = gc_relocate(*((*reg).location));}

init_storage()
{heap_1 = (LISP) must_malloc(sizeof(struct obj)*heap_size);
 heap_2 = (LISP) must_malloc(sizeof(struct obj)*heap_size);
 heap = heap_1;
 which_heap = 1;
 heap_org = heap;
 heap_end = heap + heap_size;
 unbound_marker = cons(cintern("**unbound-marker**"),NIL);
 gc_protect(&unbound_marker);
 eof_val = cons(cintern("eof"),NIL);
 gc_protect(&eof_val);
 truth = cintern("t");
 gc_protect(&truth);
 setvar(truth,truth,NIL);
 setvar(cintern("nil"),NIL,NIL);
 setvar(cintern("let"),cintern("let-internal-macro"),NIL);
 sym_errobj = cintern("errobj");
 gc_protect(&sym_errobj);
 setvar(sym_errobj,NIL,NIL);
 sym_progn = cintern("begin");
 gc_protect(&sym_progn);
 sym_lambda = cintern("lambda");
 gc_protect(&sym_lambda);
 sym_quote = cintern("quote");
 gc_protect(&sym_quote);
 gc_protect(&oblist);
 gc_protect(&open_files);}
 
void init_subr(name,type,fcn)
 char *name; long type; LISP (*fcn)();
{setvar(cintern(name),subrcons(type,name,fcn),NIL);}

LISP assq(x,alist)
     LISP x,alist;
{LISP l,tmp;
 for(l=alist;TYPEP(l,tc_cons);l=CDR(l))
   {tmp = CAR(l);
    if (TYPEP(tmp,tc_cons) && EQ(CAR(tmp),x)) return(tmp);}
 if EQ(l,NIL) return(NIL);
 err("improper list to assq",alist);}

LISP gc_relocate(x)
     LISP x;
{LISP new;
 if EQ(x,NIL) return(NIL);
 if ((*x).gc_mark == 1) return(CAR(x));
 switch TYPE(x)
   {case tc_flonum:
      new = flocons(FLONM(x));
      break;
    case tc_cons:
      new = cons(CAR(x),CDR(x));
      break;
    case tc_symbol:
      new = symcons(PNAME(x),VCELL(x));
      break;
    case tc_closure:
      new = closure((*x).storage_as.closure.env,
		    (*x).storage_as.closure.code);
      break;
    case tc_subr_0:
    case tc_subr_1:
    case tc_subr_2:
    case tc_subr_3:
    case tc_lsubr:
    case tc_fsubr:
    case tc_msubr:
      new = subrcons(TYPE(x),
		     (*x).storage_as.subr.name,
		     (*x).storage_as.subr.f);
      break;
    default: err("BUG IN GARBAGE COLLECTOR gc_relocate",NIL);}
 (*x).gc_mark = 1;
 CAR(x) = new;
 return(new);}

LISP get_newspace()
{LISP newspace;
 if (which_heap == 1)
   {newspace = heap_2;
    which_heap = 2;}
 else
   {newspace = heap_1;
    which_heap = 1;}
 heap = newspace;
 heap_org = heap;
 heap_end = heap + heap_size;
 return(newspace);}

scan_newspace(newspace)
     LISP newspace;
{LISP ptr;
 for(ptr=newspace; ptr < heap; ++ptr)
   {switch TYPE(ptr)
      {case tc_cons:
       case tc_closure:
	 CAR(ptr) = gc_relocate(CAR(ptr));
	 CDR(ptr) = gc_relocate(CDR(ptr));
	 break;
       case tc_symbol:
	 VCELL(ptr) = gc_relocate(VCELL(ptr));
	 break;
       default:
	 break;}}}
      
gc()
{LISP newspace;
 errjmp_ok = 0;
 nointerrupt = 1;
 old_heap_used = heap - heap_org;
 newspace = get_newspace();
 scan_registers();
 scan_newspace(newspace);
 errjmp_ok = 1;
 nointerrupt = 0;}

LISP gc_status(args)
     LISP args;
{if NNULLP(args) 
  if NULLP(car(args)) gc_status_flag = 0; else gc_status_flag = 1;
 if (gc_status_flag)
  printf("garbage collection is on\n"); else
  printf("garbage collection is off\n");
 printf("%ld allocated %ld free\n",heap - heap_org, heap_end - heap);
 return(NIL);}

LISP leval_args(l,env)
     LISP l,env;
{LISP result,v1,v2,tmp;
 if NULLP(l) return(NIL);
 if NTYPEP(l,tc_cons) err("bad syntax argument list",l);
 result = cons(leval(CAR(l),env),NIL);
 for(v1=result,v2=CDR(l);
     TYPEP(v2,tc_cons);
     v1 = tmp, v2 = CDR(v2))
  {tmp = cons(leval(CAR(v2),env),NIL);
   CDR(v1) = tmp;}
 if NNULLP(v2) err("bad syntax argument list",l);
 return(result);}

LISP extend_env(actuals,formals,env)
 LISP actuals,formals,env;
{if TYPEP(formals,tc_symbol)
    return(cons(cons(cons(formals,NIL),cons(actuals,NIL)),env));
 return(cons(cons(formals,actuals),env));}

LISP envlookup(var,env)
 LISP var,env;
{LISP frame,al,fl,tmp;
 for(frame=env;TYPEP(frame,tc_cons);frame=CDR(frame))
   {tmp = CAR(frame);
    if NTYPEP(tmp,tc_cons) err("damaged frame",tmp);
    for(fl=CAR(tmp),al=CDR(tmp);
	TYPEP(fl,tc_cons);
	fl=CDR(fl),al=CDR(al))
      {if NTYPEP(al,tc_cons) err("too few arguments",tmp);
       if EQ(CAR(fl),var) return(al);}}
 if NNULLP(frame) err("damaged env",env);
 return(NIL);}

LISP leval(x,env)
 LISP x,env;
{LISP tmp;
 loop:
 switch TYPE(x)
   {case tc_symbol:
      tmp = envlookup(x,env);
      if (tmp) return(CAR(tmp));
      tmp = VCELL(x);
      if EQ(tmp,unbound_marker) err("unbound variable",x);
      return(tmp);
    case tc_cons:
      tmp = leval(CAR(x),env);
      switch TYPE(tmp)
	{case tc_subr_0:
	   return(SUBRF(tmp)());
	 case tc_subr_1:
	   return(SUBRF(tmp)(leval(car(CDR(x)),env)));
	 case tc_subr_2:
	   return(SUBRF(tmp)(leval(car(CDR(x)),env),
			     leval(car(cdr(CDR(x))),env)));
	 case tc_subr_3:
	   return(SUBRF(tmp)(leval(car(CDR(x)),env),
			     leval(car(cdr(CDR(x))),env),
			     leval(car(cdr(cdr(CDR(x)))),env)));
	 case tc_lsubr:
	   return(SUBRF(tmp)(leval_args(CDR(x),env)));
	 case tc_fsubr:
	   return(SUBRF(tmp)(CDR(x),env));
	 case tc_msubr:
	   if NULLP(SUBRF(tmp)(&x,&env)) return(x);
	   goto loop;
	 case tc_closure:
	   env = extend_env(leval_args(CDR(x),env),
			    car((*tmp).storage_as.closure.code),
			    (*tmp).storage_as.closure.env);
	   x = cdr((*tmp).storage_as.closure.code);
	   goto loop;
	 case tc_symbol:
	   x = cons(tmp,cons(cons(sym_quote,cons(x,NIL)),NIL));
	   x = leval(x,NIL);
	   goto loop;
	 default:
	   err("bad function",tmp);}
    default:
      return(x);}}

LISP setvar(var,val,env)
 LISP var,val,env;
{LISP tmp;
 if NTYPEP(var,tc_symbol) err("wta(non-symbol) to setvar",var);
 tmp = envlookup(var,env);
 if NULLP(tmp) return(VCELL(var) = val);
 return(CAR(tmp)=val);}
 

LISP leval_setq(args,env)
 LISP args,env;
{return(setvar(car(args),leval(car(cdr(args)),env),env));}

LISP syntax_define(args)
 LISP args;
{if TYPEP(car(args),tc_symbol) return(args);
 return(syntax_define(
        cons(car(car(args)),
	cons(cons(sym_lambda,
	     cons(cdr(car(args)),
		  cdr(args))),
	     NIL))));}
      
LISP leval_define(args,env)
 LISP args,env;
{LISP tmp,var,val;
 tmp = syntax_define(args);
 var = car(tmp);
 if NTYPEP(var,tc_symbol) err("wta(non-symbol) to define",var);
 val = leval(car(cdr(tmp)),env);
 tmp = envlookup(var,env);
 if NNULLP(tmp) return(CAR(tmp) = val);
 if NULLP(env) return(VCELL(var) = val);
 tmp = car(env);
 setcar(tmp,cons(var,car(tmp)));
 setcdr(tmp,cons(val,cdr(tmp)));
 return(val);}
 
LISP leval_if(pform,penv)
 LISP *pform,*penv;
{LISP args,env;
 args = cdr(*pform);
 env = *penv;
 if NNULLP(leval(car(args),env)) 
    *pform = car(cdr(args)); else *pform = car(cdr(cdr(args)));
 return(truth);}

LISP leval_lambda(args,env)
 LISP args,env;
{LISP body;
 if NULLP(cdr(cdr(args)))
   body = car(cdr(args));
  else body = cons(sym_progn,cdr(args));
 return(closure(env,cons(arglchk(car(args)),body)));}
                         
LISP leval_progn(pform,penv)
 LISP *pform,*penv;
{LISP env,l,next;
 env = *penv;
 l = cdr(*pform);
 next = cdr(l);
 while(NNULLP(next)) {leval(car(l),env);l=next;next=cdr(next);}
 *pform = car(l); 
 return(truth);}

LISP leval_or(pform,penv)
 LISP *pform,*penv;
{LISP env,l,next,val;
 env = *penv;
 l = cdr(*pform);
 next = cdr(l);
 while(NNULLP(next))
   {val = leval(car(l),env);
    if NNULLP(val) {*pform = val; return(NIL);}
    l=next;next=cdr(next);}
 *pform = car(l); 
 return(truth);}

LISP leval_and(pform,penv)
 LISP *pform,*penv;
{LISP env,l,next;
 env = *penv;
 l = cdr(*pform);
 if NULLP(l) {*pform = truth; return(NIL);}
 next = cdr(l);
 while(NNULLP(next))
   {if NULLP(leval(car(l),env)) {*pform = NIL; return(NIL);}
    l=next;next=cdr(next);}
 *pform = car(l); 
 return(truth);}

LISP leval_let(pform,penv)
 LISP *pform,*penv;
{LISP env,l;
 l = cdr(*pform);
 env = *penv;
 *penv = extend_env(leval_args(car(cdr(l)),env),car(l),env);
 *pform = car(cdr(cdr(l)));
 return(truth);}

LISP reverse(l)
 LISP l;
{LISP n,p;
 n = NIL;
 for(p=l;NNULLP(p);p=cdr(p)) n = cons(car(p),n);
 return(n);}

LISP let_macro(form)
 LISP form;
{LISP p,fl,al,tmp;
 fl = NIL;
 al = NIL;
 for(p=car(cdr(form));NNULLP(p);p=cdr(p))
  {tmp = car(p);
   if TYPEP(tmp,tc_symbol) {fl = cons(tmp,fl); al = cons(NIL,al);}
   else {fl = cons(car(tmp),fl); al = cons(car(cdr(tmp)),al);}}
 p = cdr(cdr(form));
 if NULLP(cdr(p)) p = car(p); else p = cons(sym_progn,p);
 setcdr(form,cons(reverse(fl),cons(reverse(al),cons(p,NIL))));
 setcar(form,cintern("let-internal"));
 return(form);}
   
 LISP leval_quote(args,env)
 LISP args,env;
{return(car(args));}

LISP leval_tenv(args,env)
 LISP args,env;
{return(env);}

LISP lprint(exp)
 LISP exp;
{lprin1(exp);
 printf("\n");
 return(NIL);}

LISP lprin1(exp)
 LISP exp;
{LISP tmp;
 switch TYPE(exp)
   {case tc_nil:
      printf("()");
      break;
   case tc_cons:
      printf("(");
      lprin1(car(exp));
      for(tmp=cdr(exp);TYPEP(tmp,tc_cons);tmp=cdr(tmp))
	{printf(" ");lprin1(car(tmp));}
      if NNULLP(tmp) {printf(" . ");lprin1(tmp);}
      printf(")");
      break;
    case tc_flonum:
      printf("%g",FLONM(exp));
      break;
    case tc_symbol:
      printf("%s",PNAME(exp));
      break;
    case tc_subr_0:
    case tc_subr_1:
    case tc_subr_2:
    case tc_subr_3:
    case tc_lsubr:
    case tc_fsubr:
    case tc_msubr:
      printf("#<SUBR(%d) %s>",TYPE(exp),(*exp).storage_as.subr.name);
      break;
    case tc_closure:
      printf("#<CLOSURE ");
      lprin1(car((*exp).storage_as.closure.code));
      printf(" ");
      lprin1(cdr((*exp).storage_as.closure.code));
      printf(">");
      break;}
 return(NIL);}

LISP lreadr(),lreadparen(),lreadtk(),lreadf();

LISP lread()
{return(lreadf(stdin));}

 int
flush_ws(f,eoferr)
 FILE *f;
 char *eoferr;
{int c;
 while(1)
   {c = getc(f);
    if (c == EOF) if (eoferr) err(eoferr,NIL); else return(c);
    if (isspace(c)) continue;
    return(c);}}

LISP lreadf(f)
 FILE *f;
{int c;
 c = flush_ws(f,(char *)NULL);
 if (c == EOF) return(eof_val);
 ungetc(c,f);
 return(lreadr(f));}

LISP lreadr(f)
 FILE *f;
{int c,j;
 char *p;
 c = flush_ws(f,"end of file inside read");
 switch (c)
   {case '(':
      return(lreadparen(f));
    case ')':
      err("unexpected close paren",NIL);
    case '\'':
      return(cons(sym_quote,cons(lreadr(f),NIL)));}
 p = tkbuffer;
 *p++ = c;
 for(j = 1; j<TKBUFFERN; ++j)
   {c = getc(f);
    if (c == EOF) return(lreadtk(j));
    if (isspace(c)) return(lreadtk(j));
    if (strchr("()'",c)) {ungetc(c,f);return(lreadtk(j));}
    *p++ = c;}
 err("token larger than TKBUFFERN",NIL);}

LISP lreadparen(f)
 FILE *f;
{int c;
 LISP tmp;
 c = flush_ws(f,"end of file inside list");
 if (c == ')') return(NIL);
 ungetc(c,f);
 tmp = lreadr(f);
 return(cons(tmp,lreadparen(f)));}

LISP lreadtk(j)
     long j;
{int k;
 char c,*p;
 p = tkbuffer;
 p[j] = 0;
 if (*p == '-') p+=1;
 { int adigit = 0;
   while(isdigit(*p)) {p+=1; adigit=1;}
   if (*p=='.') {
     p += 1;
     while(isdigit(*p)) {p+=1; adigit=1;}}
   if (!adigit) goto a_symbol; }
 if (*p=='e') {
   p+=1;
   if (*p=='-'||*p=='+') p+=1;
   if (!isdigit(*p)) goto a_symbol; else p+=1;
   while(isdigit(*p)) p+=1; }
 if (*p) goto a_symbol;
 return(flocons(atof(tkbuffer)));
 a_symbol:
 return(rintern(tkbuffer));}
      
LISP copy_list(x)
 LISP x;
{if NULLP(x) return(NIL);
 return(cons(car(x),copy_list(cdr(x))));}

LISP oblistfn()
{return(copy_list(oblist));}

close_open_files()
{LISP l;
 FILE *p;
 for(l=open_files;NNULLP(l);l=cdr(l))
   {p = (FILE *) PNAME(car(l));
    if (p)
      {printf("closing a file left open\n");
       fclose(p);}}
 open_files = NIL;}


LISP vload(fname)
 char *fname;
{LISP sym,form;
 FILE *f;
 printf("loading %s\n",fname);
 sym = symcons(0,NIL);
 open_files = cons(sym,open_files);
 PNAME(sym) = (char *) fopen(fname,"r");
 f = (FILE *) PNAME(sym);
 if (!f) {open_files = cdr(open_files);
	  printf("Could not open file\n");
	  return(NIL);}
 while(1)
   {form = lreadf(f);
    if EQ(form,eof_val) break;
    leval(form,NIL);}
 fclose(f);
 open_files = cdr(open_files);
 printf("done.\n");
 return(truth);}

LISP load(fname)
 LISP fname;
{if NTYPEP(fname,tc_symbol) err("filename not a symbol",fname);
 return(vload(PNAME(fname)));}

LISP quit()
{longjmp(errjmp,2);
 return(NIL);}

LISP nullp(x)
 LISP x;
{if EQ(x,NIL) return(truth); else return(NIL);}

LISP arglchk(x)
 LISP x;
{LISP l;
 if TYPEP(x,tc_symbol) return(x);
 for(l=x;TYPEP(l,tc_cons);l=CDR(l));
 if NNULLP(l) err("improper formal argument list",x);
 return(x);}

long no_interrupt(n)
     long n;
{long x;
 x = nointerrupt;
 nointerrupt = n;
 return(x);}

init_subrs()
{init_subr("cons",tc_subr_2,cons);
 init_subr("car",tc_subr_1,car);
 init_subr("cdr",tc_subr_1,cdr);
 init_subr("set-car!",tc_subr_2,setcar);
 init_subr("set-cdr!",tc_subr_2,setcdr);
 init_subr("+",tc_subr_2,plus);
 init_subr("-",tc_subr_2,difference);
 init_subr("*",tc_subr_2,ltimes);
 init_subr("/",tc_subr_2,quotient);
 init_subr(">",tc_subr_2,greaterp);
 init_subr("<",tc_subr_2,lessp);
 init_subr("eq?",tc_subr_2,eq);
 init_subr("eqv?",tc_subr_2,eql);
 init_subr("assq",tc_subr_2,assq);
 init_subr("read",tc_subr_0,lread);
 init_subr("print",tc_subr_1,lprint);
 init_subr("eval",tc_subr_2,leval);
 init_subr("define",tc_fsubr,leval_define);
 init_subr("lambda",tc_fsubr,leval_lambda);
 init_subr("if",tc_msubr,leval_if);
 init_subr("begin",tc_msubr,leval_progn);
 init_subr("set!",tc_fsubr,leval_setq);
 init_subr("or",tc_msubr,leval_or);
 init_subr("and",tc_msubr,leval_and);
 init_subr("quote",tc_fsubr,leval_quote);
 init_subr("oblist",tc_subr_0,oblistfn);
 init_subr("copy-list",tc_subr_1,copy_list);
 init_subr("gc-status",tc_lsubr,gc_status);
 init_subr("load",tc_subr_1,load);
 init_subr("pair?",tc_subr_1,consp);
 init_subr("symbol?",tc_subr_1,symbolp);
 init_subr("number?",tc_subr_1,numberp);
 init_subr("let-internal",tc_msubr,leval_let);
 init_subr("let-internal-macro",tc_subr_1,let_macro);
 init_subr("symbol-bound?",tc_subr_2,symbol_boundp);
 init_subr("symbol-value",tc_subr_2,symbol_value);
 init_subr("set-symbol-value!",tc_subr_3,setvar);
 init_subr("the-environment",tc_fsubr,leval_tenv);
 init_subr("error",tc_subr_2,lerr);
 init_subr("quit",tc_subr_0,quit);
 init_subr("not",tc_subr_1,nullp);
 init_subr("null?",tc_subr_1,nullp);
 init_subr("env-lookup",tc_subr_2,envlookup);
 init_subr("reverse",tc_subr_1,reverse);}


