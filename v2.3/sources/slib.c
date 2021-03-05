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
   Release 1.5 29-NOV-89. Added startup flag -g, select stop and copy
    or mark-and-sweep garbage collection, which assumes that the stack/register
    marking code is correct for your architecture. 
   Release 2.0 1-DEC-89. Added repl_hooks, Catch, Throw. This is significantly
    different enough (from 1.3) now that I'm calling it a major release.
   Release 2.1 4-DEC-89. Small reader features, dot, backquote, comma.
   Release 2.2 5-DEC-89. gc,read,print,eval, hooks for user defined datatypes.
   Release 2.3 6-DEC-89. save_forms, obarray intern mechanism. comment char.

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

long gc_kind_copying = 1;

long gc_cells_allocated = 0;
double gc_time_taken;
LISP *stack_start_ptr;
LISP freelist;

jmp_buf errjmp;
long errjmp_ok = 0;
long nointerrupt = 1;
long interrupt_differed = 0;

LISP oblist = NIL;
LISP truth = NIL;
LISP eof_val = NIL;
LISP sym_errobj = NIL;
LISP sym_progn = NIL;
LISP sym_lambda = NIL;
LISP sym_quote = NIL;
LISP sym_dot = NIL;
LISP open_files = NIL;
LISP unbound_marker = NIL;

LISP *obarray;
long obarray_dim = 100;

struct catch_frame
{LISP tag;
 LISP retval;
 jmp_buf cframe;
 struct catch_frame *next;};

struct gc_protected
{LISP *location;
 long length;
 struct gc_protected *next;};

struct catch_frame *catch_framep = (struct catch_frame *) NULL;


process_cla(argc,argv)
 int argc; char **argv;
{int k;
 for(k=1;k<argc;++k)
   {if (strlen(argv[k])<2) continue;
    if (argv[k][0] != '-') {printf("bad arg: %s\n",argv[k]);continue;}
    switch(argv[k][1])
      {case 'h':
	 heap_size = atol(&(argv[k][2])); break;
       case 'o':
	 obarray_dim = atol(&(argv[k][2])); break;
       case 'i':
	 init_file = &(argv[k][2]); break;
       case 'g':
	 gc_kind_copying = atol(&(argv[k][2])); break;
       default: printf("bad arg: %s\n",argv[k]);}}}

print_welcome()
{printf("Welcome to SIOD, Scheme In One Defun, Version 2.3\n");
 printf("(C) Copyright 1988, 1989 Paradigm Associates Inc.\n");}

print_hs_1()
{printf("heap_size = %ld cells, %ld bytes. GC is %s\n",
        heap_size,heap_size*sizeof(struct obj),
	(gc_kind_copying == 1) ? "stop and copy" : "mark and sweep");}

print_hs_2()
{if (gc_kind_copying == 1)
   printf("heap_1 at 0x%lX, heap_2 at 0x%lX\n",heap_1,heap_2);
 else
   printf("heap_1 at 0x%lX\n",heap_1);}

long no_interrupt(n)
     long n;
{long x;
 x = nointerrupt;
 nointerrupt = n;
 if ((nointerrupt == 0) && (interrupt_differed == 1))
   {interrupt_differed = 0;
    err_ctrl_c();}
 return(x);}



handle_sigfpe(sig,code,scp)
 long sig,code; struct sigcontext *scp;
{signal(SIGFPE,handle_sigfpe);
 err("floating point exception",NIL);}

handle_sigint(sig,code,scp)
 long sig,code; struct sigcontext *scp;
{signal(SIGINT,handle_sigint);
 if (nointerrupt == 1)
   interrupt_differed = 1;
 else
   err_ctrl_c();}

err_ctrl_c()
{err("control-c interrupt",NIL);}

LISP get_eof_val()
{return(eof_val);}

repl_driver(want_sigint,want_init)
     long want_sigint,want_init;
{int k;
 LISP stack_start;
 stack_start_ptr = &stack_start;
 k = setjmp(errjmp);
 if (k == 2) return;
 if (want_sigint) signal(SIGFPE,handle_sigfpe);
 signal(SIGINT,handle_sigint);
 close_open_files();
 catch_framep = (struct catch_frame *) NULL;
 errjmp_ok = 1;
 interrupt_differed = 0;
 nointerrupt = 0;
 if (want_init && init_file && (k == 0)) vload(init_file,0);
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


void (*repl_puts)() = NULL;
LISP (*repl_read)() = NULL;
LISP (*repl_eval)() = NULL;
void (*repl_print)() = NULL;

void set_repl_hooks(puts_f,read_f,eval_f,print_f)
     void (*puts_f)();
     LISP (*read_f)();
     LISP (*eval_f)();
     void (*print_f)();
{repl_puts = puts_f;
 repl_read = read_f;
 repl_eval = eval_f;
 repl_print = print_f;}

grepl_puts(st)
     char *st;
{if (repl_puts == NULL)
   printf("%s",st);
 else
   (*repl_puts)(st);}
     
repl() 
{LISP x,cw;
 double rt;
 while(1)
   {if ((gc_kind_copying == 1) && ((gc_status_flag) || heap >= heap_end))
     {rt = myruntime();
      gc_stop_and_copy();
      sprintf(tkbuffer,
	      "GC took %g seconds, %ld compressed to %ld, %ld free\n",
	      myruntime()-rt,old_heap_used,heap-heap_org,heap_end-heap);
      grepl_puts(tkbuffer);}
    grepl_puts("> ");
    if (repl_read == NULL) x = lread();
    else x = (*repl_read)();
    if EQ(x,eof_val) break;
    rt = myruntime();
    if (gc_kind_copying == 1)
      cw = heap;
    else
      {gc_cells_allocated = 0;
       gc_time_taken = 0.0;}
    if (repl_eval == NULL) x = leval(x,NIL);
    else x = (*repl_eval)();
    if (gc_kind_copying == 1)
      sprintf(tkbuffer,
	      "Evaluation took %g seconds %ld cons work\n",
	      myruntime()-rt,
	      heap-cw);
    else
      sprintf(tkbuffer,
	      "Evaluation took %g seconds (%g in gc) %ld cons work\n",
	      myruntime()-rt,
	      gc_time_taken,
	      gc_cells_allocated);
    grepl_puts(tkbuffer);
    if (repl_print == NULL) lprint(x);
    else (*repl_print)(x);}}

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
{if NSYMBOLP(message) err("argument to error not a symbol",message);
 err(PNAME(message),x);
 return(NIL);}

void gc_fatal_error()
{err("ran out of storage",NIL);}

#define NEWCELL(_into,_type)          \
{if (gc_kind_copying == 1)            \
   {if ((_into = heap) >= heap_end)   \
      gc_fatal_error();               \
    heap = _into+1;}                  \
 else                                 \
   {if NULLP(freelist)                \
      gc_for_newcell();               \
    _into = freelist;                 \
    freelist = CDR(freelist);         \
    ++gc_cells_allocated;}            \
 (*_into).gc_mark = 0;                \
 (*_into).type = _type;}
	  

LISP cons(x,y)
     LISP x,y;
{LISP z;
 NEWCELL(z,tc_cons);
 CAR(z) = x;
 CDR(z) = y;
 return(z);}

LISP consp(x)
     LISP x;
{if CONSP(x) return(truth); else return(NIL);}

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
{if NCONSP(cell) err("wta to setcar",cell);
 return(CAR(cell) = value);}

LISP setcdr(cell,value)
     LISP cell, value;
{if NCONSP(cell) err("wta to setcdr",cell);
 return(CDR(cell) = value);}

LISP flocons(x)
 double x;
{LISP z;
 NEWCELL(z,tc_flonum);
 (*z).storage_as.flonum.data = x;
 return(z);}

LISP numberp(x)
     LISP x;
{if FLONUMP(x) return(truth); else return(NIL);}

LISP plus(x,y)
     LISP x,y;
{if NFLONUMP(x) err("wta(1st) to plus",x);
 if NFLONUMP(y) err("wta(2nd) to plus",y);
 return(flocons(FLONM(x)+FLONM(y)));}

LISP ltimes(x,y)
 LISP x,y;
{if NFLONUMP(x) err("wta(1st) to times",x);
 if NFLONUMP(y) err("wta(2nd) to times",y);
 return(flocons(FLONM(x)*FLONM(y)));}

LISP difference(x,y)
 LISP x,y;
{if NFLONUMP(x) err("wta(1st) to difference",x);
 if NFLONUMP(y) err("wta(2nd) to difference",y);
 return(flocons(FLONM(x)-FLONM(y)));}

LISP quotient(x,y)
 LISP x,y;
{if NFLONUMP(x) err("wta(1st) to quotient",x);
 if NFLONUMP(y) err("wta(2nd) to quotient",y);
 return(flocons(FLONM(x)/FLONM(y)));}

LISP greaterp(x,y)
 LISP x,y;
{if NFLONUMP(x) err("wta(1st) to greaterp",x);
 if NFLONUMP(y) err("wta(2nd) to greaterp",y);
 if (FLONM(x)>FLONM(y)) return(truth);
 return(NIL);}

LISP lessp(x,y)
 LISP x,y;
{if NFLONUMP(x) err("wta(1st) to lessp",x);
 if NFLONUMP(y) err("wta(2nd) to lessp",y);
 if (FLONM(x)<FLONM(y)) return(truth);
 return(NIL);}

LISP eq(x,y)
 LISP x,y;
{if EQ(x,y) return(truth); else return(NIL);}

LISP eql(x,y)
 LISP x,y;
{if EQ(x,y) return(truth); else 
 if NFLONUMP(x) return(NIL); else
 if NFLONUMP(y) return(NIL); else
 if (FLONM(x) == FLONM(y)) return(truth);
 return(NIL);}

LISP symcons(pname,vcell)
 char *pname; LISP vcell;
{LISP z;
 NEWCELL(z,tc_symbol);
 PNAME(z) = pname;
 VCELL(z) = vcell;
 return(z);}

LISP symbolp(x)
     LISP x;
{if SYMBOLP(x) return(truth); else return(NIL);}

LISP symbol_boundp(x,env)
 LISP x,env;
{LISP tmp;
 if NSYMBOLP(x) err("not a symbol",x);
 tmp = envlookup(x,env);
 if NNULLP(tmp) return(truth);
 if EQ(VCELL(x),unbound_marker) return(NIL); else return(truth);}

LISP symbol_value(x,env)
 LISP x,env;
{LISP tmp;
 if NSYMBOLP(x) err("not a symbol",x);
 tmp = envlookup(x,env);
 if NNULLP(tmp) return(CAR(tmp));
 tmp = VCELL(x);
 if EQ(tmp,unbound_marker) err("unbound variable",x);
 return(tmp);}

char * must_malloc(size)
     unsigned long size;
{char *tmp;
 tmp = (char *) malloc(size);
 if (tmp == (char *)NULL) err("failed to allocate storage from system",NIL);
 return(tmp);}

LISP gen_intern(name,copyp)
     char *name;
     long copyp;
{LISP l,sym,sl;
 char *cname;
 long hash,n,c,flag;
 flag = no_interrupt(1);
 if (obarray_dim > 1)
   {hash = 0;
    n = obarray_dim;
    cname = name;
    while(c = *cname++) hash = ((hash * 17) ^ c) % n;
    sl = obarray[hash];}
 else
   sl = oblist;
 for(l=sl;NNULLP(l);l=CDR(l))
   if (strcmp(name,PNAME(CAR(l))) == 0)
     {no_interrupt(flag);
      return(CAR(l));}
 if (copyp == 1)
   {cname = must_malloc(strlen(name)+1);
    strcpy(cname,name);}
 else
   cname = name;
 sym = symcons(cname,unbound_marker);
 if (obarray_dim > 1) obarray[hash] = cons(sym,sl);
 oblist = cons(sym,oblist);
 no_interrupt(flag);
 return(sym);}

LISP cintern(name)
 char *name;
{return(gen_intern(name,0));}

LISP rintern(name)
 char *name;
{return(gen_intern(name,1));}

LISP subrcons(type,name,f)
 long type; char *name; LISP (*f)();
{LISP z;
 NEWCELL(z,type);
 (*z).storage_as.subr.name = name;
 (*z).storage_as.subr.f = f;
 return(z);}


LISP closure(env,code)
     LISP env,code;
{LISP z;
 NEWCELL(z,tc_closure);
 (*z).storage_as.closure.env = env;
 (*z).storage_as.closure.code = code;
 return(z);}


struct gc_protected *protected_registers = NULL;

void gc_protect(location)
     LISP *location;
{gc_protect_n(location,1);}

void gc_protect_n(location,n)
     LISP *location;
     long n;
{struct gc_protected *reg;
 reg = (struct gc_protected *) must_malloc(sizeof(struct gc_protected));
 (*reg).location = location;
 (*reg).length = n;
 (*reg).next = protected_registers;
  protected_registers = reg;}

void gc_protect_sym(location,st)
     LISP *location;
     char *st;
{*location = cintern(st);
 gc_protect(location);}

scan_registers()
{struct gc_protected *reg;
 LISP *location;
 long j,n;
 for(reg = protected_registers; reg; reg = (*reg).next)
   {location = (*reg).location;
    n = (*reg).length;
    for(j=0;j<n;++j)
      location[j] = gc_relocate(location[j]);}}

init_storage()
{LISP ptr,next,end;
 long j;
 heap_1 = (LISP) must_malloc(sizeof(struct obj)*heap_size);
 heap = heap_1;
 which_heap = 1;
 heap_org = heap;
 heap_end = heap + heap_size;
 if (gc_kind_copying == 1)
   heap_2 = (LISP) must_malloc(sizeof(struct obj)*heap_size);
 else
   {ptr = heap_org;
    end = heap_end;
    while(1)
      {(*ptr).type = tc_free_cell;
       next = ptr + 1;
       if (next < end)
	 {CDR(ptr) = next;
	  ptr = next;}
       else
	 {CDR(ptr) = NIL;
	  break;}}
    freelist = heap_org;}
 gc_protect(&oblist);
 if (obarray_dim > 1)
   {obarray = (LISP *) must_malloc(sizeof(LISP) * obarray_dim);
    for(j=0;j<obarray_dim;++j)
      obarray[j] = NIL;
    gc_protect_n(obarray,obarray_dim);}
 unbound_marker = cons(cintern("**unbound-marker**"),NIL);
 gc_protect(&unbound_marker);
 eof_val = cons(cintern("eof"),NIL);
 gc_protect(&eof_val);
 gc_protect_sym(&truth,"t");
 setvar(truth,truth,NIL);
 setvar(cintern("nil"),NIL,NIL);
 setvar(cintern("let"),cintern("let-internal-macro"),NIL);
 gc_protect_sym(&sym_errobj,"errobj");
 setvar(sym_errobj,NIL,NIL);
 gc_protect_sym(&sym_progn,"begin");
 gc_protect_sym(&sym_lambda,"lambda");
 gc_protect_sym(&sym_quote,"quote");
 gc_protect_sym(&sym_dot,".");
 gc_protect(&open_files);}
 
void init_subr(name,type,fcn)
 char *name; long type; LISP (*fcn)();
{setvar(cintern(name),subrcons(type,name,fcn),NIL);}

LISP assq(x,alist)
     LISP x,alist;
{LISP l,tmp;
 for(l=alist;CONSP(l);l=CDR(l))
   {tmp = CAR(l);
    if (CONSP(tmp) && EQ(CAR(tmp),x)) return(tmp);}
 if EQ(l,NIL) return(NIL);
 err("improper list to assq",alist);}

LISP (*user_gc_relocate)() = NULL;
void (*user_gc_scan)() = NULL;
LISP (*user_gc_mark)() = NULL;
void (*user_gc_free)() = NULL;

void set_gc_hooks(rel,scan,mark,free,kind)
     LISP (*rel)(),(*mark)();
     void (*scan)(),(*free)();
     long *kind;
{user_gc_relocate = rel;
 user_gc_scan = scan;
 user_gc_mark = mark;
 user_gc_free = free;
 *kind = gc_kind_copying;}

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
    case tc_user_1:
    case tc_user_2:
    case tc_user_3:
    case tc_user_4:
    case tc_user_5:
      if (user_gc_relocate != NULL)
	{new = (*user_gc_relocate)(x);
	 break;}
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
       case tc_user_1:
       case tc_user_2:
       case tc_user_3:
       case tc_user_4:
       case tc_user_5:
	 if (user_gc_scan != NULL) (*user_gc_scan)(ptr);
	 break;
       default:
	 break;}}}
      
gc_stop_and_copy()
{LISP newspace;
 long flag;
 flag = no_interrupt(1);
 errjmp_ok = 0;
 old_heap_used = heap - heap_org;
 newspace = get_newspace();
 scan_registers();
 scan_newspace(newspace);
 errjmp_ok = 1;
 no_interrupt(flag);}

gc_for_newcell()
{long flag;
 if (errjmp_ok == 0) gc_fatal_error();
 flag = no_interrupt(1);
 errjmp_ok = 0;
 gc_mark_and_sweep();
 errjmp_ok = 1;
 no_interrupt(flag);
 if NULLP(freelist) gc_fatal_error();}

jmp_buf save_regs_gc_mark;

gc_mark_and_sweep()
{LISP stack_end;
 gc_ms_stats_start();
 /* This assumes that all registers are saved into the jmp_buff */
 setjmp(save_regs_gc_mark);
 mark_locations(save_regs_gc_mark,
		((char *) save_regs_gc_mark) + sizeof(save_regs_gc_mark));
 mark_protected_registers();
 mark_locations(stack_start_ptr,&stack_end);
 gc_sweep();
 gc_ms_stats_end();}

double gc_rt;
long gc_cells_collected;

gc_ms_stats_start()
{gc_rt = myruntime();
 gc_cells_collected = 0;
 if (gc_status_flag)
   printf("[starting GC]\n");}

gc_ms_stats_end()
{gc_rt = myruntime() - gc_rt;
 gc_time_taken = gc_time_taken + gc_rt;
 if (gc_status_flag)
   printf("[GC took %g cpu seconds, %ld cells collected]\n",
	  gc_rt,
	  gc_cells_collected);}


void gc_mark(ptr)
     LISP ptr;
{gc_mark_loop:
 if NULLP(ptr) return;
 if ((*ptr).gc_mark) return;
 (*ptr).gc_mark = 1;
 switch ((*ptr).type)
   {case tc_flonum:
      break;
    case tc_cons:
      gc_mark(CAR(ptr));
      ptr = CDR(ptr);
      goto gc_mark_loop;
    case tc_symbol:
      ptr = VCELL(ptr);
      goto gc_mark_loop;
    case tc_closure:
      gc_mark((*ptr).storage_as.closure.code);
      ptr = (*ptr).storage_as.closure.env;
      goto gc_mark_loop;
    case tc_subr_0:
    case tc_subr_1:
    case tc_subr_2:
    case tc_subr_3:
    case tc_lsubr:
    case tc_fsubr:
    case tc_msubr:
      return;
    case tc_user_1:
    case tc_user_2:
    case tc_user_3:
    case tc_user_4:
    case tc_user_5:
      if (user_gc_mark != NULL)
	{ptr = (*user_gc_mark)(ptr);
	 goto gc_mark_loop;}
    default:
      err("BUG IN GARBAGE COLLECTOR gc_mark",NIL);}}

mark_protected_registers()
{struct gc_protected *reg;
 LISP *location;
 long j,n;
 for(reg = protected_registers; reg; reg = (*reg).next)
   {location = (*reg).location;
    n = (*reg).length;
    for(j=0;j<n;++j)
      gc_mark(location[j]);}}

mark_locations(start,end)
     LISP *start,*end;
{LISP *tmp;
 long n;
 if (start > end)
   {tmp = start;
    start = end;
    end = tmp;}
 n = end - start;
 mark_locations_array(start,n);}

mark_locations_array(x,n)
     LISP x[];
     long n;
{int j;
 LISP p;
 for(j=0;j<n;++j)
   {p = x[j];
    if ((p >= heap_org) &&
	(p < heap_end) &&
	(((((char *)p) - ((char *)heap_org)) % sizeof(struct obj)) == 0) &&
	NTYPEP(p,tc_free_cell))
      gc_mark(p);}}


gc_sweep()
{LISP ptr,end,nfreelist;
 long n;
 end = heap_end;
 n = 0;
 nfreelist = freelist;
 for(ptr=heap_org; ptr < end; ++ptr)
   if (((*ptr).gc_mark == 0))
     switch((*ptr).type)
       {case tc_free_cell:
	  break;
	case tc_user_1:
	case tc_user_2:
	case tc_user_3:
	case tc_user_4:
	case tc_user_5:
	  if (user_gc_free != NULL) (*user_gc_free)(ptr);
	default:
	  ++n;
	  (*ptr).type = tc_free_cell;
	  CDR(ptr) = nfreelist;
	  nfreelist = ptr;}
   else
     (*ptr).gc_mark = 0;
 gc_cells_collected = n;
 freelist = nfreelist;}

LISP user_gc(args)
     LISP args;
{long old_status_flag,flag;
 if (gc_kind_copying == 1)
   err("implementation cannot GC at will with stop-and-copy\n",
       NIL);
 flag = no_interrupt(1);
 errjmp_ok = 0;
 old_status_flag = gc_status_flag;
 if NNULLP(args)
   if NULLP(car(args)) gc_status_flag = 0; else gc_status_flag = 1;
 gc_mark_and_sweep();
 gc_status_flag = old_status_flag;
 errjmp_ok = 1;
 no_interrupt(flag);
 return(NIL);}
 
LISP gc_status(args)
     LISP args;
{LISP l;
 int n;
 if NNULLP(args) 
   if NULLP(car(args)) gc_status_flag = 0; else gc_status_flag = 1;
 if (gc_kind_copying == 1)
   {if (gc_status_flag)
      printf("garbage collection is on\n");
   else
     printf("garbage collection is off\n");
    printf("%ld allocated %ld free\n",heap - heap_org, heap_end - heap);}
 else
   {if (gc_status_flag)
      printf("garbage collection verbose\n");
    else
      printf("garbage collection silent\n");
    {for(n=0,l=freelist;NNULLP(l); ++n) l = CDR(l);
     printf("%ld allocated %ld free\n",(heap_end - heap_org) - n,n);}}
 return(NIL);}

LISP leval_args(l,env)
     LISP l,env;
{LISP result,v1,v2,tmp;
 if NULLP(l) return(NIL);
 if NCONSP(l) err("bad syntax argument list",l);
 result = cons(leval(CAR(l),env),NIL);
 for(v1=result,v2=CDR(l);
     CONSP(v2);
     v1 = tmp, v2 = CDR(v2))
  {tmp = cons(leval(CAR(v2),env),NIL);
   CDR(v1) = tmp;}
 if NNULLP(v2) err("bad syntax argument list",l);
 return(result);}

LISP extend_env(actuals,formals,env)
 LISP actuals,formals,env;
{if SYMBOLP(formals)
   return(cons(cons(cons(formals,NIL),cons(actuals,NIL)),env));
 return(cons(cons(formals,actuals),env));}

LISP envlookup(var,env)
 LISP var,env;
{LISP frame,al,fl,tmp;
 for(frame=env;CONSP(frame);frame=CDR(frame))
   {tmp = CAR(frame);
    if NCONSP(tmp) err("damaged frame",tmp);
    for(fl=CAR(tmp),al=CDR(tmp);
	CONSP(fl);
	fl=CDR(fl),al=CDR(al))
      {if NCONSP(al) err("too few arguments",tmp);
       if EQ(CAR(fl),var) return(al);}}
 if NNULLP(frame) err("damaged env",env);
 return(NIL);}

LISP (*user_leval)() = NULL;

void set_eval_hooks(fcn)
     LISP (*fcn)();
{user_leval = fcn;}

LISP leval(x,env)
 LISP x,env;
{LISP tmp,arg1;
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
	   x = CDR(x);
	   arg1 = leval(car(x),env);
	   x = NULLP(x) ? NIL : CDR(x);
	   return(SUBRF(tmp)(arg1,
			     leval(car(x),env)));
	 case tc_subr_3:
	   x = CDR(x);
	   arg1 = leval(car(x),env);
	   x = NULLP(x) ? NIL : CDR(x);
	   return(SUBRF(tmp)(arg1,
			     leval(car(x),env),
			     leval(car(cdr(x)),env)));
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
	 case tc_user_1:
	 case tc_user_2:
	 case tc_user_3:
	 case tc_user_4:
	 case tc_user_5:
	   if (user_leval != NULL)
	     {if NULLP((*user_leval)(tmp,&x,&env)) return(x); else goto loop;}
	 default:
	   err("bad function",tmp);}
    default:
      return(x);}}

LISP setvar(var,val,env)
 LISP var,val,env;
{LISP tmp;
 if NSYMBOLP(var) err("wta(non-symbol) to setvar",var);
 tmp = envlookup(var,env);
 if NULLP(tmp) return(VCELL(var) = val);
 return(CAR(tmp)=val);}
 

LISP leval_setq(args,env)
 LISP args,env;
{return(setvar(car(args),leval(car(cdr(args)),env),env));}

LISP syntax_define(args)
 LISP args;
{if SYMBOLP(car(args)) return(args);
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
 if NSYMBOLP(var) err("wta(non-symbol) to define",var);
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

LISP leval_catch(args,env)
 LISP args,env;
{struct catch_frame frame;
 int k;
 LISP l,val;
 frame.tag = leval(car(args),env);
 frame.next = catch_framep;
 k = setjmp(frame.cframe);
 catch_framep = &frame;
 if (k == 2)
   {catch_framep = frame.next;
    return(frame.retval);}
 for(l=cdr(args); NNULLP(l); l = cdr(l))
   val = leval(car(l),env);
 catch_framep = frame.next;
 return(val);}

LISP lthrow(tag,value)
     LISP tag,value;
{struct catch_frame *l;
 for(l=catch_framep; l; l = (*l).next)
   if EQ((*l).tag,tag)
     {(*l).retval = value;
      longjmp((*l).cframe,2);}
 err("no *catch found with this tag",tag);
 return(NIL);}

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
   if SYMBOLP(tmp) {fl = cons(tmp,fl); al = cons(NIL,al);}
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

LISP symbolconc(args)
     LISP args;
{long size;
 LISP l,s;
 size = 0;
 tkbuffer[0] = 0;
 for(l=args;NNULLP(l);l=cdr(l))
   {s = car(l);
    if NSYMBOLP(s) err("wta(non-symbol) to symbolconc",s);
    size = size + strlen(PNAME(s));
    if (size >  TKBUFFERN) err("symbolconc buffer overflow",NIL);
    strcat(tkbuffer,PNAME(s));}
 return(rintern(tkbuffer));}


LISP (*user_prin1)() = NULL;

void set_print_hooks(fcn)
     LISP (*fcn)();
{user_prin1 = fcn;}

LISP lprin1f(exp,f)
     LISP exp;
     FILE *f;
{LISP tmp;
 switch TYPE(exp)
   {case tc_nil:
      fprintf(f,"()");
      break;
   case tc_cons:
      fprintf(f,"(");
      lprin1f(car(exp),f);
      for(tmp=cdr(exp);CONSP(tmp);tmp=cdr(tmp))
	{fprintf(f," ");lprin1f(car(tmp),f);}
      if NNULLP(tmp) {fprintf(f," . ");lprin1f(tmp,f);}
      fprintf(f,")");
      break;
    case tc_flonum:
      fprintf(f,"%g",FLONM(exp));
      break;
    case tc_symbol:
      fprintf(f,"%s",PNAME(exp));
      break;
    case tc_subr_0:
    case tc_subr_1:
    case tc_subr_2:
    case tc_subr_3:
    case tc_lsubr:
    case tc_fsubr:
    case tc_msubr:
      fprintf(f,"#<SUBR(%d) %s>",TYPE(exp),(*exp).storage_as.subr.name);
      break;
    case tc_closure:
      fprintf(f,"#<CLOSURE ");
      lprin1f(car((*exp).storage_as.closure.code),f);
      fprintf(f," ");
      lprin1f(cdr((*exp).storage_as.closure.code),f);
      fprintf(f,">");
      break;
    case tc_user_1:
    case tc_user_2:
    case tc_user_3:
    case tc_user_4:
    case tc_user_5:
      if (user_prin1 != NULL)
	{(*user_prin1)(exp,f);
	 break;}
    default:
      fprintf(f,"#<UNKNOWN %d %lX>",TYPE(exp),exp);}
 return(NIL);}

LISP lprint(exp)
 LISP exp;
{lprin1f(exp,stdout);
 printf("\n");
 return(NIL);}

LISP lreadr(),lreadparen(),lreadtk(),lreadf();

LISP lread()
{return(lreadf(stdin));}

 int
flush_ws(f,eoferr)
 FILE *f;
 char *eoferr;
{int c,commentp;
 commentp = 0;
 while(1)
   {c = getc(f);
    if (c == EOF) if (eoferr) err(eoferr,NIL); else return(c);
    if (commentp) {if (c == '\n') commentp = 0;}
    else if (c == ';') commentp = 1;
    else if (!isspace(c)) return(c);}}

LISP lreadf(f)
 FILE *f;
{int c;
 c = flush_ws(f,(char *)NULL);
 if (c == EOF) return(eof_val);
 ungetc(c,f);
 return(lreadr(f));}

char *user_ch_readm = "";
char *user_te_readm = "";

LISP (*user_readm)() = NULL;

void set_read_hooks(all_set,end_set,fcn)
     char *all_set,*end_set;
     LISP (*fcn)();
{user_ch_readm = all_set;
 user_te_readm = end_set;
 user_readm = fcn;}

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
      return(cons(sym_quote,cons(lreadr(f),NIL)));
    case '`':
      return(cons(cintern("+internal-backquote"),lreadr(f)));
    case ',':
      c = getc(f);
      switch(c)
	{case '@':
	   p = "+internal-comma-atsign";
	   break;
	 case '.':
	   p = "+internal-comma-dot";
	   break;
	 default:
	   p = "+internal-comma";
	   ungetc(c,f);}
      return(cons(cintern(p),lreadr(f)));
    default:
      if ((user_readm != NULL) && strchr(user_ch_readm,c))
	return((*user_readm)(c,f));}
 p = tkbuffer;
 *p++ = c;
 for(j = 1; j<TKBUFFERN; ++j)
   {c = getc(f);
    if (c == EOF) return(lreadtk(j));
    if (isspace(c)) return(lreadtk(j));
    if (strchr("()'`,;",c) || strchr(user_te_readm,c))
      {ungetc(c,f);return(lreadtk(j));}
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
 if EQ(tmp,sym_dot)
   {tmp = lreadr(f);
    c = flush_ws(f,"end of file inside list");
    if (c != ')') err("missing close paren");
    return(tmp);}
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

FILE *fopen_care(name,how)
     char *name,*how;
{FILE *f;
 LISP sym;
 long flag;
 sym = symcons(0,NIL);
 open_files = cons(sym,open_files);
 flag = no_interrupt(1);
 f = fopen(name,how);
 if (!f)
   {perror(name);
    err("could not open file",NIL);}
 PNAME(sym) = (char *) f;
 no_interrupt(flag);
 return(f);}

LISP fclose_dq(f,l)
     FILE *f;
     LISP l;
{FILE *p;
 if NULLP(l) return(l);
 if (PNAME(CAR(l)) == (char *) f) return(CDR(l));
 CDR(l) = fclose_dq(f,CDR(l));
 return(l);}


fclose_care(f)
     FILE *f;
{long flag;
 LISP l;
 flag = no_interrupt(1);
 fclose(f);
 open_files = fclose_dq(f,open_files);
 no_interrupt(flag);}

LISP vload(fname,cflag)
     char *fname;
     long cflag;
{LISP form,result,tail;
 FILE *f;
 printf("loading %s\n",fname);
 f = fopen_care(fname,"r");
 result = NIL;
 tail = NIL;
 while(1)
   {form = lreadf(f);
    if EQ(form,eof_val) break;
    if (cflag)
      {form = cons(form,NIL);
       if NULLP(result)
	 result = tail = form;
       else
	 tail = setcdr(tail,form);}
    else
      leval(form,NIL);}
 fclose_care(f);
 printf("done.\n");
 return(result);}

LISP load(fname,cflag)
 LISP fname,cflag;
{if NSYMBOLP(fname) err("filename not a symbol",fname);
 return(vload(PNAME(fname),NULLP(cflag) ? 0 : 1));}

LISP save_forms(fname,forms,how)
     LISP fname,forms,how;
{char *cname,*chow;
 LISP l;
 FILE *f;
 if NSYMBOLP(fname) err("filename not a symbol",fname);
 cname = PNAME(fname);
 if EQ(how,NIL) chow = "w";
 else if EQ(how,cintern("a")) chow = "a";
 else err("bad argument to save-forms",how);
 printf("%s forms to %s\n",
	(*chow == 'a') ? "appending" : "saving",
	cname);
 f = fopen_care(cname,chow);
 for(l=forms;NNULLP(l);l=cdr(l))
   {lprin1f(car(l),f);
    putc('\n',f);}
 fclose_care(f);
 printf("done.\n");
 return(truth);}

LISP quit()
{longjmp(errjmp,2);
 return(NIL);}

LISP nullp(x)
 LISP x;
{if EQ(x,NIL) return(truth); else return(NIL);}

LISP arglchk(x)
 LISP x;
{LISP l;
 if SYMBOLP(x) return(x);
 for(l=x;CONSP(l);l=CDR(l));
 if NNULLP(l) err("improper formal argument list",x);
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
 init_subr("*catch",tc_fsubr,leval_catch);
 init_subr("*throw",tc_subr_2,lthrow);
 init_subr("quote",tc_fsubr,leval_quote);
 init_subr("oblist",tc_subr_0,oblistfn);
 init_subr("copy-list",tc_subr_1,copy_list);
 init_subr("gc-status",tc_lsubr,gc_status);
 init_subr("gc",tc_lsubr,user_gc);
 init_subr("load",tc_subr_2,load);
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
 init_subr("reverse",tc_subr_1,reverse);
 init_subr("symbolconc",tc_lsubr,symbolconc);
 init_subr("save-forms",tc_subr_3,save_forms);}

