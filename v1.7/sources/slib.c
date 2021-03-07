/* Scheme In One Defun, but in C this time.
 
 * COPYRIGHT (c) 1988-1992 BY *
 * PARADIGM ASSOCIATES INCORPORATED, CAMBRIDGE, MASSACHUSETTS. *
 *			 ALL RIGHTS RESERVED *

Permission to use, copy, modify, distribute and sell this software
and its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appear in all copies
and that both that copyright notice and this permission notice appear
in supporting documentation, and that the name of Paradigm Associates
Inc not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.

PARADIGM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
PARADIGM BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

*/

/*

g...@paradigm.com

Paradigm Associates Inc Phone: 617-492-6079
29 Putnam Ave, Suite 6
Cambridge, MA 02138


 Release 1.0: 24-APR-88
 Release 1.1: 25-APR-88, added: macros, predicates, load. With additions by
 Barak.Pe...@DOGHEN.BOLTZ.CS.CMU.EDU: Full flonum recognizer,
 cleaned up uses of NULL/0. Now distributed with siod.scm.
 Release 1.2: 28-APR-88, name changes as requested by J...@AI.AI.MIT.EDU,
 plus some bug fixes.
 Release 1.3: 1-MAY-88, changed env to use frames instead of alist.
 define now works properly. vms specific function edit.
 Release 1.4 20-NOV-89. Minor Cleanup and remodularization.
 Now in 3 files, siod.h, slib.c, siod.c. Makes it easier to write your
 own main loops. Some short-int changes for lightspeed C included.
 Release 1.5 29-NOV-89. Added startup flag -g, select stop and copy
er
 marking code is correct for your architecture. 
y
 different enough (from 1.3) now that I'm calling it a major release.
 Release 2.1 4-DEC-89. Small reader features, dot, backquote, comma.
.
 Release 2.3 6-DEC-89. save_forms, obarray intern mechanism. comment char.
 Release 2.3a......... minor speed-ups. i/o interrupt considerations.
 Release 2.4 27-APR-90 gen_readr, for read-from-string.
 Release 2.5 18-SEP-90 arrays added to SIOD.C by popular demand. inums.
 Release 2.6 11-MAR-92 function prototypes, some remodularization.
 Release 2.7 20-MAR-92 hash tables, fasload. Stack check.

 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <signal.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>

#include "siod.h"
#include "siodp.h"

LISP heap_1,heap_2;
LISP heap,heap_end,heap_org;
long heap_size = 5000;
long old_heap_used;
long which_heap;
long gc_status_flag = 1;
char *init_file = (char *) NULL;
char *tkbuffer = NULL;
long gc_kind_copying = 1;
long gc_cells_allocated = 0;
double gc_time_taken;
LISP *stack_start_ptr;
LISP freelist;
jmp_buf errjmp;
long errjmp_ok = 0;
long nointerrupt = 1;
long interrupt_differed = 0;
LISP oblistvar = NIL;
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
struct catch_frame *catch_framep = (struct catch_frame *) NULL;
void (*repl_puts)(char *) = NULL;
LISP (*repl_read)(void) = NULL;
LISP (*repl_eval)(LISP) = NULL;
void (*repl_print)(LISP) = NULL;
LISP *inums;
long inums_dim = 100;
struct user_type_hooks *user_type_hooks = NULL;
struct gc_protected *protected_registers = NULL;
jmp_buf save_regs_gc_mark;
double gc_rt;
long gc_cells_collected;
char *user_ch_readm = "";
char *user_te_readm = "";
LISP (*user_readm)(int, struct gen_readio *) = NULL;
LISP (*user_readt)(char *,long, int *) = NULL;
#ifdef THINK_C
int ipoll_counter = 0;
#endif

char *stack_limit_ptr = NULL;
long stack_size = 
#ifdef THINK_C
 10000;
#else
 50000;
#endif

void process_cla(int argc,char **argv,int warnflag)
{int k;
 for(k=1;k<argc;++k)
 {if (strlen(argv[k])<2) continue;
 if (argv[k][0] != '-')
 {if (warnflag) printf("bad arg: %s\n",argv[k]);continue;}
 switch(argv[k][1])
 {case 'h':
	 heap_size = atol(&(argv[k][2]));
	 break;
 case 'o':
	 obarray_dim = atol(&(argv[k][2]));
	 break;
 case 'i':
	 init_file = &(argv[k][2]);
	 break;
 case 'n':
	 inums_dim = atol(&(argv[k][2]));
	 break;
 case 'g':
	 gc_kind_copying = atol(&(argv[k][2]));
	 break;
 case 's':
	 stack_size = atol(&(argv[k][2]));
	 break;
 default:
	 if (warnflag) printf("bad arg: %s\n",argv[k]);}}}

void print_welcome(void)
{printf("Welcome to SIOD, Scheme In One Defun, Version 2.7\n");
 printf("(C) Copyright 1988-1992 Paradigm Associates Inc.\n");}

void print_hs_1(void)
{printf("heap_size = %ld cells, %ld bytes. %ld inums. GC is %s\n",
 heap_size,heap_size*sizeof(struct obj),
	inums_dim,
	(gc_kind_copying == 1) ? "stop and copy" : "mark and sweep");}

void print_hs_2(void)
{if (gc_kind_copying == 1)
 printf("heap_1 at 0x%lX, heap_2 at 0x%lX\n",heap_1,heap_2);
 else
 printf("heap_1 at 0x%lX\n",heap_1);}

long no_interrupt(long n)
{long x;
 x = nointerrupt;
 nointerrupt = n;
 if ((nointerrupt == 0) && (interrupt_differed == 1))
 {interrupt_differed = 0;
 err_ctrl_c();}
 return(x);}

void handle_sigfpe(int sig SIG_restargs)
{signal(SIGFPE,handle_sigfpe);
 err("floating point exception",NIL);}

void handle_sigint(int sig SIG_restargs)
{signal(SIGINT,handle_sigint);
 if (nointerrupt == 1)
 interrupt_differed = 1;
 else
 err_ctrl_c();}

void err_ctrl_c(void)
{err("control-c interrupt",NIL);}

LISP get_eof_val(void)
{return(eof_val);}

void repl_driver(long want_sigint,long want_init)
{int k;
 LISP stack_start;
 stack_start_ptr = &stack_start;
 stack_limit_ptr = STACK_LIMIT(stack_start_ptr,stack_size);
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

#ifdef vms
double myruntime(void)
{double total;
 struct tbuffer b;
 times(&b);
 total = b.proc_user_time;
 total += b.proc_system_time;
 return(total / CLK_TCK);}
#else
#ifdef unix
#include <sys/types.h>
#include <sys/times.h>
double myruntime(void)
{double total;
 struct tms b;
 times(&b);
 total = b.tms_utime;
 total += b.tms_stime;
 return(total / 60.0);}
#else
#ifdef THINK_C
double myruntime(void)
{return(((double) clock()) / ((double) CLOCKS_PER_SEC));}
#else
double myruntime(void)
{time_t x;
 time(&x);
 return((double) x);}
#endif
#endif
#endif

void set_repl_hooks(void (*puts_f)(),
		 LISP (*read_f)(),
		 LISP (*eval_f)(),
		 void (*print_f)())
{repl_puts = puts_f;
 repl_read = read_f;
 repl_eval = eval_f;
 repl_print = print_f;}

void fput_st(FILE *f,char *st)
{long flag;
 flag = no_interrupt(1);
 fprintf(f,"%s",st);
 no_interrupt(flag);}

void put_st(char *st)
{fput_st(stdout,st);}
  
void grepl_puts(char *st)
{if (repl_puts == NULL)
 put_st(st);
 else
 (*repl_puts)(st);}
  
void repl(void) 
{LISP x,cw;
 double rt;
 while(1)
end))
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
 else x = (*repl_eval)(x);
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

void err(char *message, LISP x)
{nointerrupt = 1;
 if NNULLP(x) 
 printf("ERROR: %s (see errobj)\n",message);
 else printf("ERROR: %s\n",message);
 if (errjmp_ok == 1) {setvar(sym_errobj,x,NIL); longjmp(errjmp,1);}
 printf("FATAL ERROR DURING STARTUP OR CRITICAL CODE SECTION\n");
 exit(1);}

void err_stack(char *ptr)
 /* The user could be given an option to continue here */
{err("the currently assigned stack limit has been exceded",NIL);}

LISP stack_limit(LISP amount,LISP silent)
{if NNULLP(amount)
 {stack_size = get_c_long(amount);
 stack_limit_ptr = STACK_LIMIT(stack_start_ptr,stack_size);}
 if NULLP(silent)
 {sprintf(tkbuffer,"Stack_size = %ld bytes, [%08lX,0%08lX]\n",
	 stack_size,stack_start_ptr,stack_limit_ptr);
 put_st(tkbuffer);
 return(NIL);}
 else
 return(flocons(stack_size));}

char *get_c_string(LISP x)
{if TYPEP(x,tc_symbol)
 return(PNAME(x));
 else if TYPEP(x,tc_string)
 return(x->storage_as.string.data);
 else
 err("not a symbol or string",x);}

LISP lerr(LISP message, LISP x)
{err(get_c_string(message),x);
 return(NIL);}

void gc_fatal_error(void)
{err("ran out of storage",NIL);}

LISP newcell(long type)
{LISP z;
 NEWCELL(z,type);
 return(z);}

LISP cons(LISP x,LISP y)
{LISP z;
 NEWCELL(z,tc_cons);
 CAR(z) = x;
 CDR(z) = y;
 return(z);}

LISP consp(LISP x)
{if CONSP(x) return(truth); else return(NIL);}

LISP car(LISP x)
{switch TYPE(x)
 {case tc_nil:
 return(NIL);
 case tc_cons:
 return(CAR(x));
 default:
 err("wta to car",x);}}

LISP cdr(LISP x)
{switch TYPE(x)
 {case tc_nil:
 return(NIL);
 case tc_cons:
 return(CDR(x));
 default:
 err("wta to cdr",x);}}

LISP setcar(LISP cell, LISP value)
{if NCONSP(cell) err("wta to setcar",cell);
 return(CAR(cell) = value);}

LISP setcdr(LISP cell, LISP value)
{if NCONSP(cell) err("wta to setcdr",cell);
 return(CDR(cell) = value);}

LISP flocons(double x)
{LISP z;
 long n;
 if ((inums_dim > 0) &&
 ((x - (n = x)) == 0) &&
 (x >= 0) &&
 (n < inums_dim))
 return(inums[n]);
 NEWCELL(z,tc_flonum);
 FLONM(z) = x;
 return(z);}

LISP numberp(LISP x)
{if FLONUMP(x) return(truth); else return(NIL);}

LISP plus(LISP x,LISP y)
{if NFLONUMP(x) err("wta(1st) to plus",x);
 if NFLONUMP(y) err("wta(2nd) to plus",y);
 return(flocons(FLONM(x) + FLONM(y)));}

LISP ltimes(LISP x,LISP y)
{if NFLONUMP(x) err("wta(1st) to times",x);
 if NFLONUMP(y) err("wta(2nd) to times",y);
 return(flocons(FLONM(x)*FLONM(y)));}

LISP difference(LISP x,LISP y)
{LISP z;
 if NFLONUMP(x) err("wta(1st) to difference",x);
 if NFLONUMP(y) err("wta(2nd) to difference",y);
 return(flocons(FLONM(x) - FLONM(y)));}

LISP quotient(LISP x,LISP y)
{LISP z;
 if NFLONUMP(x) err("wta(1st) to quotient",x);
 if NFLONUMP(y) err("wta(2nd) to quotient",y);
 return(flocons(FLONM(x)/FLONM(y)));}

LISP greaterp(LISP x,LISP y)
{if NFLONUMP(x) err("wta(1st) to greaterp",x);
 if NFLONUMP(y) err("wta(2nd) to greaterp",y);
 if (FLONM(x)>FLONM(y)) return(truth);
 return(NIL);}

LISP lessp(LISP x,LISP y)
{if NFLONUMP(x) err("wta(1st) to lessp",x);
 if NFLONUMP(y) err("wta(2nd) to lessp",y);
 if (FLONM(x)<FLONM(y)) return(truth);
 return(NIL);}

LISP eq(LISP x,LISP y)
{if EQ(x,y) return(truth); else return(NIL);}

LISP eql(LISP x,LISP y)
{if EQ(x,y) return(truth); else 
 if NFLONUMP(x) return(NIL); else
 if NFLONUMP(y) return(NIL); else
 if (FLONM(x) == FLONM(y)) return(truth);
 return(NIL);}

LISP symcons(char *pname,LISP vcell)
{LISP z;
 NEWCELL(z,tc_symbol);
 PNAME(z) = pname;
 VCELL(z) = vcell;
 return(z);}

LISP symbolp(LISP x)
{if SYMBOLP(x) return(truth); else return(NIL);}

LISP symbol_boundp(LISP x,LISP env)
{LISP tmp;
 if NSYMBOLP(x) err("not a symbol",x);
 tmp = envlookup(x,env);
 if NNULLP(tmp) return(truth);
 if EQ(VCELL(x),unbound_marker) return(NIL); else return(truth);}

LISP symbol_value(LISP x,LISP env)
{LISP tmp;
 if NSYMBOLP(x) err("not a symbol",x);
 tmp = envlookup(x,env);
 if NNULLP(tmp) return(CAR(tmp));
 tmp = VCELL(x);
 if EQ(tmp,unbound_marker) err("unbound variable",x);
 return(tmp);}

char *must_malloc(unsigned long size)
{char *tmp;
 tmp = (char *) malloc(size);
 if (tmp == (char *)NULL) err("failed to allocate storage from system",NIL);
 return(tmp);}

LISP gen_intern(char *name,long copyp)
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
 sl = oblistvar;
 for(l=sl;NNULLP(l);l=CDR(l))
 if (strcmp(name,PNAME(CAR(l))) == 0)
 {no_interrupt(flag);
 return(CAR(l));}
 if (copyp == 1)
 {cname = (char *) must_malloc(strlen(name)+1);
 strcpy(cname,name);}
 else
 cname = name;
 sym = symcons(cname,unbound_marker);
 if (obarray_dim > 1) obarray[hash] = cons(sym,sl);
 oblistvar = cons(sym,oblistvar);
 no_interrupt(flag);
 return(sym);}

LISP cintern(char *name)
{return(gen_intern(name,0));}

LISP rintern(char *name)
{return(gen_intern(name,1));}

LISP subrcons(long type, char *name, LISP (*f)())
{LISP z;
 NEWCELL(z,type);
 (*z).storage_as.subr.name = name;
 (*z).storage_as.subr.f = f;
 return(z);}

LISP closure(LISP env,LISP code)
{LISP z;
 NEWCELL(z,tc_closure);
 (*z).storage_as.closure.env = env;
 (*z).storage_as.closure.code = code;
 return(z);}

void gc_protect(LISP *location)
{gc_protect_n(location,1);}

void gc_protect_n(LISP *location,long n)
{struct gc_protected *reg;
 reg = (struct gc_protected *) must_malloc(sizeof(struct gc_protected));
 (*reg).location = location;
 (*reg).length = n;
 (*reg).next = protected_registers;
 protected_registers = reg;}

void gc_protect_sym(LISP *location,char *st)
{*location = cintern(st);
 gc_protect(location);}

void scan_registers(void)
{struct gc_protected *reg;
 LISP *location;
 long j,n;
 for(reg = protected_registers; reg; reg = (*reg).next)
 {location = (*reg).location;
 n = (*reg).length;
 for(j=0;j<n;++j)
 location[j] = gc_relocate(location[j]);}}

void init_storage(void)
{long j;
 init_storage_1();
 init_storage_a();
 set_gc_hooks(tc_c_file,0,0,0,file_gc_free,&j);
 set_print_hooks(tc_c_file,file_prin1);}

void init_storage_1(void)
{LISP ptr,next,end;
 long j;
 tkbuffer = (char *) must_malloc(TKBUFFERN+1);
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
 gc_protect(&oblistvar);
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
 gc_protect(&open_files);
 if (inums_dim > 0)
 {inums = (LISP *) must_malloc(sizeof(LISP) * inums_dim);
 for(j=0;j<inums_dim;++j)
 {NEWCELL(ptr,tc_flonum);
 FLONM(ptr) = j;
 inums[j] = ptr;}
 gc_protect_n(inums,inums_dim);}}
 
void init_subr(char *name, long type, LISP (*fcn)())
{setvar(cintern(name),subrcons(type,name,fcn),NIL);}

LISP assq(LISP x,LISP alist)
{LISP l,tmp;
 for(l=alist;CONSP(l);l=CDR(l))
 {tmp = CAR(l);
 if (CONSP(tmp) && EQ(CAR(tmp),x)) return(tmp);}
 if EQ(l,NIL) return(NIL);
 err("improper list to assq",alist);}

struct user_type_hooks *get_user_type_hooks(long type)
{long j;
 if (user_type_hooks == NULL)
 {user_type_hooks = (struct user_type_hooks *)
 must_malloc(sizeof(struct user_type_hooks) * tc_table_dim);
 for(j=0;j<tc_table_dim;++j)
 memset(&user_type_hooks[j],0,sizeof(struct user_type_hooks));}
 if ((type >= 0) && (type < tc_table_dim))
 return(&user_type_hooks[type]);
 else
 err("type number out of range",NIL);}

void set_gc_hooks(long type,
		 LISP (*rel)(),
		 LISP (*mark)(),
		 void (*scan)(),
		 void (*free)(),
		 long *kind)
{struct user_type_hooks *p;
 p = get_user_type_hooks(type);
 p->gc_relocate = rel;
 p->gc_scan = scan;
 p->gc_mark = mark;
 p->gc_free = free;
 *kind = gc_kind_copying;}

LISP gc_relocate(LISP x)
{LISP new;
 struct user_type_hooks *p;
 if EQ(x,NIL) return(NIL);
 if ((*x).gc_mark == 1) return(CAR(x));
 switch TYPE(x)
 {case tc_flonum:
 case tc_cons:
 case tc_symbol:
 case tc_closure:
 case tc_subr_0:
 case tc_subr_1:
 case tc_subr_2:
 case tc_subr_3:
 case tc_lsubr:
 case tc_fsubr:
 case tc_msubr:
 if ((new = heap) >= heap_end) gc_fatal_error();
 heap = new+1;
 memcpy(new,x,sizeof(struct obj));
 break;
 default:
 p = get_user_type_hooks(TYPE(x));
 if (p->gc_relocate)
	new = (*p->gc_relocate)(x);
 else
	{if ((new = heap) >= heap_end) gc_fatal_error();
	 heap = new+1;
	 memcpy(new,x,sizeof(struct obj));}}
 (*x).gc_mark = 1;
 CAR(x) = new;
 return(new);}

LISP get_newspace(void)
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

void scan_newspace(LISP newspace)
{LISP ptr;
 struct user_type_hooks *p;
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
 case tc_flonum:
 case tc_subr_0:
 case tc_subr_1:
 case tc_subr_2:
 case tc_subr_3:
 case tc_lsubr:
 case tc_fsubr:
 case tc_msubr:
	 break;
 default:
	 p = get_user_type_hooks(TYPE(ptr));
	 if (p->gc_scan) (*p->gc_scan)(ptr);}}}

void free_oldspace(LISP space,LISP end)
{LISP ptr;
 struct user_type_hooks *p;
 for(ptr=space; ptr < end; ++ptr)
 if (ptr->gc_mark == 0)
 switch TYPE(ptr)
 {case tc_cons:
	case tc_closure:
	case tc_symbol:
	case tc_flonum:
	case tc_subr_0:
	case tc_subr_1:
	case tc_subr_2:
	case tc_subr_3:
	case tc_lsubr:
	case tc_fsubr:
	case tc_msubr:
	 break;
	default:
	 p = get_user_type_hooks(TYPE(ptr));
	 if (p->gc_free) (*p->gc_free)(ptr);}}
  
void gc_stop_and_copy(void)
{LISP newspace,oldspace,end;
 long flag;
 flag = no_interrupt(1);
 errjmp_ok = 0;
 oldspace = heap_org;
 end = heap;
 old_heap_used = end - oldspace;
 newspace = get_newspace();
 scan_registers();
 scan_newspace(newspace);
 free_oldspace(oldspace,end);
 errjmp_ok = 1;
 no_interrupt(flag);}

void gc_for_newcell(void)
{long flag;
 if (errjmp_ok == 0) gc_fatal_error();
 flag = no_interrupt(1);
 errjmp_ok = 0;
 gc_mark_and_sweep();
 errjmp_ok = 1;
 no_interrupt(flag);
 if NULLP(freelist) gc_fatal_error();}

void gc_mark_and_sweep(void)
{LISP stack_end;
 gc_ms_stats_start();
 setjmp(save_regs_gc_mark);
 mark_locations((LISP *) save_regs_gc_mark,
		(LISP *) (((char *) save_regs_gc_mark) + sizeof(save_regs_gc_mark)));
 mark_protected_registers();
 mark_locations((LISP *) stack_start_ptr,
		(LISP *) &stack_end);
#ifdef THINK_C
 mark_locations((LISP *) ((char *) stack_start_ptr + 2),
		(LISP *) ((char *) &stack_end + 2));
#endif
 gc_sweep();
 gc_ms_stats_end();}

void gc_ms_stats_start(void)
{gc_rt = myruntime();
 gc_cells_collected = 0;
 if (gc_status_flag)
 printf("[starting GC]\n");}

void gc_ms_stats_end(void)
{gc_rt = myruntime() - gc_rt;
 gc_time_taken = gc_time_taken + gc_rt;
 if (gc_status_flag)
 printf("[GC took %g cpu seconds, %ld cells collected]\n",
	 gc_rt,
	 gc_cells_collected);}

void gc_mark(LISP ptr)
{struct user_type_hooks *p;
 gc_mark_loop:
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
 break;
 default:
 p = get_user_type_hooks(TYPE(ptr));
 if (p->gc_mark)
	ptr = (*p->gc_mark)(ptr);}}

void mark_protected_registers(void)
{struct gc_protected *reg;
 LISP *location;
 long j,n;
 for(reg = protected_registers; reg; reg = (*reg).next)
 {location = (*reg).location;
 n = (*reg).length;
 for(j=0;j<n;++j)
 gc_mark(location[j]);}}

void mark_locations(LISP *start,LISP *end)
{LISP *tmp;
 long n;
 if (start > end)
 {tmp = start;
 start = end;
 end = tmp;}
 n = end - start;
 mark_locations_array(start,n);}

void mark_locations_array(LISP *x,long n)
{int j;
 LISP p;
 for(j=0;j<n;++j)
 {p = x[j];
 if ((p >= heap_org) &&
	(p < heap_end) &&
	(((((char *)p) - ((char *)heap_org)) % sizeof(struct obj)) == 0) &&
	NTYPEP(p,tc_free_cell))
 gc_mark(p);}}

void gc_sweep(void)
{LISP ptr,end,nfreelist;
 long n;
 struct user_type_hooks *p;
 end = heap_end;
 n = 0;
 nfreelist = freelist;
 for(ptr=heap_org; ptr < end; ++ptr)
 if (((*ptr).gc_mark == 0))
 {switch((*ptr).type)
	{case tc_free_cell:
	 case tc_cons:
	 case tc_closure:
	 case tc_symbol:
	 case tc_flonum:
	 case tc_subr_0:
	 case tc_subr_1:
	 case tc_subr_2:
	 case tc_subr_3:
	 case tc_lsubr:
	 case tc_fsubr:
	 case tc_msubr:
	 break;
	 default:
	 p = get_user_type_hooks(TYPE(ptr));
	 if (p->gc_free)
	 (*p->gc_free)(ptr);}
 ++n;
 (*ptr).type = tc_free_cell;
 CDR(ptr) = nfreelist;
 nfreelist = ptr;}
 else
 (*ptr).gc_mark = 0;
 gc_cells_collected = n;
 freelist = nfreelist;}

LISP user_gc(LISP args)
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
 
LISP gc_status(LISP args)
{LISP l;
 int n;
 if NNULLP(args) 
 if NULLP(car(args)) gc_status_flag = 0; else gc_status_flag = 1;
 if (gc_kind_copying == 1)
 {if (gc_status_flag)
 put_st("garbage collection is on\n");
 else
 put_st("garbage collection is off\n");
 sprintf(tkbuffer,"%ld allocated %ld free\n",
	 heap - heap_org, heap_end - heap);
 put_st(tkbuffer);}
 else
 {if (gc_status_flag)
 put_st("garbage collection verbose\n");
 else
 put_st("garbage collection silent\n");
 {for(n=0,l=freelist;NNULLP(l); ++n) l = CDR(l);
 sprintf(tkbuffer,"%ld allocated %ld free\n",
	 (heap_end - heap_org) - n,n);
 put_st(tkbuffer);}}
 return(NIL);}

LISP leval_args(LISP l,LISP env)
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

LISP extend_env(LISP actuals,LISP formals,LISP env)
{if SYMBOLP(formals)
 return(cons(cons(cons(formals,NIL),cons(actuals,NIL)),env));
 return(cons(cons(formals,actuals),env));}

LISP envlookup(LISP var,LISP env)
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

void set_eval_hooks(long type,LISP (*fcn)())
{struct user_type_hooks *p;
 p = get_user_type_hooks(type);
 p->leval = fcn;}

LISP leval(LISP x,LISP env)
{LISP tmp,arg1;
 struct user_type_hooks *p;
 STACK_CHECK(&x);
 loop:
 INTERRUPT_CHECK();
 switch TYPE(x)
 {case tc_symbol:
 tmp = envlookup(x,env);
 if NNULLP(tmp) return(CAR(tmp));
 tmp = VCELL(x);
 if EQ(tmp,unbound_marker) err("unbound variable",x);
 return(tmp);
 case tc_cons:
 tmp = CAR(x);
 switch TYPE(tmp)
	{case tc_symbol:
	 tmp = envlookup(tmp,env);
	 if NNULLP(tmp)
	 {tmp = CAR(tmp);
	 break;}
	 tmp = VCELL(CAR(x));
	 if EQ(tmp,unbound_marker) err("unbound variable",CAR(x));
	 break;
	 case tc_cons:
	 tmp = leval(tmp,env);
	 break;}
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
	 default:
	 p = get_user_type_hooks(TYPE(tmp));
	 if (p->leval)
5
	 err("bad function",tmp);}
 default:
 return(x);}}

LISP setvar(LISP var,LISP val,LISP env)
{LISP tmp;
 if NSYMBOLP(var) err("wta(non-symbol) to setvar",var);
 tmp = envlookup(var,env);
 if NULLP(tmp) return(VCELL(var) = val);
 return(CAR(tmp)=val);}
 
LISP leval_setq(LISP args,LISP env)
{return(setvar(car(args),leval(car(cdr(args)),env),env));}

LISP syntax_define(LISP args)
{if SYMBOLP(car(args)) return(args);
 return(syntax_define(
 cons(car(car(args)),
	cons(cons(sym_lambda,
	 cons(cdr(car(args)),
		 cdr(args))),
	 NIL))));}
  
LISP leval_define(LISP args,LISP env)
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
 
LISP leval_if(LISP *pform,LISP *penv)
{LISP args,env;
 args = cdr(*pform);
 env = *penv;
 if NNULLP(leval(car(args),env)) 
 *pform = car(cdr(args)); else *pform = car(cdr(cdr(args)));
 return(truth);}

LISP leval_lambda(LISP args,LISP env)
{LISP body;
 if NULLP(cdr(cdr(args)))
 body = car(cdr(args));
 else body = cons(sym_progn,cdr(args));
 return(closure(env,cons(arglchk(car(args)),body)));}
  
LISP leval_progn(LISP *pform,LISP *penv)
{LISP env,l,next;
 env = *penv;
 l = cdr(*pform);
 next = cdr(l);
 while(NNULLP(next)) {leval(car(l),env);l=next;next=cdr(next);}
 *pform = car(l); 
 return(truth);}

LISP leval_or(LISP *pform,LISP *penv)
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

LISP leval_and(LISP *pform,LISP *penv)
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

LISP leval_catch(LISP args,LISP env)
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

LISP lthrow(LISP tag,LISP value)
{struct catch_frame *l;
 for(l=catch_framep; l; l = (*l).next)
 if EQ((*l).tag,tag)
 {(*l).retval = value;
 longjmp((*l).cframe,2);}
 err("no *catch found with this tag",tag);
 return(NIL);}

LISP leval_let(LISP *pform,LISP *penv)
{LISP env,l;
 l = cdr(*pform);
 env = *penv;
 *penv = extend_env(leval_args(car(cdr(l)),env),car(l),env);
 *pform = car(cdr(cdr(l)));
 return(truth);}

LISP reverse(LISP l)
{LISP n,p;
 n = NIL;
 for(p=l;NNULLP(p);p=cdr(p)) n = cons(car(p),n);
 return(n);}

LISP let_macro(LISP form)
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
  
LISP leval_quote(LISP args,LISP env)
{return(car(args));}

LISP leval_tenv(LISP args,LISP env)
{return(env);}

LISP leval_while(LISP args,LISP env)
{LISP l;
 while NNULLP(leval(car(args),env))
 for(l=cdr(args);NNULLP(l);l=cdr(l))
 leval(car(l),env);
 return(NIL);}

LISP symbolconc(LISP args)
{long size;
 LISP l,s;
 size = 0;
 tkbuffer[0] = 0;
 for(l=args;NNULLP(l);l=cdr(l))
 {s = car(l);
 if NSYMBOLP(s) err("wta(non-symbol) to symbolconc",s);
 size = size + strlen(PNAME(s));
 if (size > TKBUFFERN) err("symbolconc buffer overflow",NIL);
 strcat(tkbuffer,PNAME(s));}
 return(rintern(tkbuffer));}

void set_print_hooks(long type,void (*fcn)())
{struct user_type_hooks *p;
 p = get_user_type_hooks(type);
 p->prin1 = fcn;}

LISP lprin1f(LISP exp,FILE *f)
{LISP tmp;
 struct user_type_hooks *p;
 STACK_CHECK(&exp);
 INTERRUPT_CHECK();
 switch TYPE(exp)
 {case tc_nil:
 fput_st(f,"()");
 break;
 case tc_cons:
 fput_st(f,"(");
 lprin1f(car(exp),f);
 for(tmp=cdr(exp);CONSP(tmp);tmp=cdr(tmp))
	{fput_st(f," ");lprin1f(car(tmp),f);}
 if NNULLP(tmp) {fput_st(f," . ");lprin1f(tmp,f);}
 fput_st(f,")");
 break;
 case tc_flonum:
 sprintf(tkbuffer,"%g",FLONM(exp));
 fput_st(f,tkbuffer);
 break;
 case tc_symbol:
 fput_st(f,PNAME(exp));
 break;
 case tc_subr_0:
 case tc_subr_1:
 case tc_subr_2:
 case tc_subr_3:
 case tc_lsubr:
 case tc_fsubr:
 case tc_msubr:
 sprintf(tkbuffer,"#<SUBR(%d) ",TYPE(exp));
 fput_st(f,tkbuffer);
 fput_st(f,(*exp).storage_as.subr.name);
 fput_st(f,">");
 break;
 case tc_closure:
 fput_st(f,"#<CLOSURE ");
 lprin1f(car((*exp).storage_as.closure.code),f);
 fput_st(f," ");
 lprin1f(cdr((*exp).storage_as.closure.code),f);
 fput_st(f,">");
 break;
 default:
 p = get_user_type_hooks(TYPE(exp));
 if (p->prin1)
	(*p->prin1)(exp,f);
 else
	{sprintf(tkbuffer,"#<UNKNOWN %d %lX>",TYPE(exp),exp);
	 fput_st(f,tkbuffer);}}
 return(NIL);}

LISP lprint(LISP exp)
{lprin1f(exp,stdout);
 put_st("\n");
 return(NIL);}

LISP lread(void)
{return(lreadf(stdin));}

int f_getc(FILE *f)
{long iflag,dflag;
 int c;
 iflag = no_interrupt(1);
 dflag = interrupt_differed;
 c = getc(f);
#ifdef VMS
 if ((dflag == 0) & interrupt_differed & (f == stdin))
 while((c != 0) & (c != EOF)) c = getc(f);
#endif
 no_interrupt(iflag);
 return(c);}

void f_ungetc(int c, FILE *f)
{ungetc(c,f);}

int flush_ws(struct gen_readio *f,char *eoferr)
{int c,commentp;
 commentp = 0;
 while(1)
 {c = GETC_FCN(f);
 if (c == EOF) if (eoferr) err(eoferr,NIL); else return(c);
 if (commentp) {if (c == '\n') commentp = 0;}
 else if (c == ';') commentp = 1;
 else if (!isspace(c)) return(c);}}

LISP lreadf(FILE *f)
{struct gen_readio s;
 s.getc_fcn = (int (*)(char *))f_getc;
 s.ungetc_fcn = (void (*)(int, char *))f_ungetc;
 s.cb_argument = (char *) f;
 return(readtl(&s));}

LISP readtl(struct gen_readio *f)
{int c;
 c = flush_ws(f,(char *)NULL);
 if (c == EOF) return(eof_val);
 UNGETC_FCN(c,f);
 return(lreadr(f));}
 
void set_read_hooks(char *all_set,char *end_set,
		 LISP (*fcn1)(),LISP (*fcn2)())
{user_ch_readm = all_set;
 user_te_readm = end_set;
 user_readm = fcn1;
 user_readt = fcn2;}

LISP lreadr(struct gen_readio *f)
{int c,j;
 char *p;
 STACK_CHECK(&f);
 p = tkbuffer;
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
 c = GETC_FCN(f);
 switch(c)
	{case '@':
	 p = "+internal-comma-atsign";
	 break;
	 case '.':
	 p = "+internal-comma-dot";
	 break;
	 default:
	 p = "+internal-comma";
	 UNGETC_FCN(c,f);}
 return(cons(cintern(p),lreadr(f)));
 case '"':
 return(lreadstring(f));
 default:
 if ((user_readm != NULL) && strchr(user_ch_readm,c))
	return((*user_readm)(c,f));}
 *p++ = c;
 for(j = 1; j<TKBUFFERN; ++j)
 {c = GETC_FCN(f);
 if (c == EOF) return(lreadtk(j));
 if (isspace(c)) return(lreadtk(j));
 if (strchr("()'`,;\"",c) || strchr(user_te_readm,c))
 {UNGETC_FCN(c,f);return(lreadtk(j));}
 *p++ = c;}
 err("token larger than TKBUFFERN",NIL);}

LISP lreadparen(struct gen_readio *f)
{int c;
 LISP tmp;
 c = flush_ws(f,"end of file inside list");
 if (c == ')') return(NIL);
 UNGETC_FCN(c,f);
 tmp = lreadr(f);
 if EQ(tmp,sym_dot)
 {tmp = lreadr(f);
 c = flush_ws(f,"end of file inside list");
 if (c != ')') err("missing close paren",NIL);
 return(tmp);}
 return(cons(tmp,lreadparen(f)));}

LISP lreadtk(long j)
{int k,flag;
 char c,*p;
 LISP tmp;
 int adigit;
 p = tkbuffer;
 p[j] = 0;
 if (user_readt != NULL)
 {tmp = (*user_readt)(p,j,&flag);
 if (flag) return(tmp);}
 if (*p == '-') p+=1;
 adigit = 0;
 while(isdigit(*p)) {p+=1; adigit=1;}
 if (*p=='.')
 {p += 1;
 while(isdigit(*p)) {p+=1; adigit=1;}}
 if (!adigit) goto a_symbol;
 if (*p=='e')
 {p+=1;
 if (*p=='-'||*p=='+') p+=1;
 if (!isdigit(*p)) goto a_symbol; else p+=1;
 while(isdigit(*p)) p+=1;}
 if (*p) goto a_symbol;
 return(flocons(atof(tkbuffer)));
 a_symbol:
 return(rintern(tkbuffer));}
  
LISP copy_list(LISP x)
{if NULLP(x) return(NIL);
 STACK_CHECK(&x);
 return(cons(car(x),copy_list(cdr(x))));}

LISP oblistfn(void)
{return(copy_list(oblistvar));}

void close_open_files(void)
{LISP l,p;
 for(l=open_files;NNULLP(l);l=cdr(l))
 {p = car(l);
 if (p->storage_as.c_file.f)
 {printf("closing a file left open: %s\n",
	 (p->storage_as.c_file.name) ? p->storage_as.c_file.name : "");
 file_gc_free(p);}}
 open_files = NIL;}

LISP fopen_c(char *name,char *how)
{LISP sym;
 long flag;
 flag = no_interrupt(1);
 sym = newcell(tc_c_file);
 sym->storage_as.c_file.f = (FILE *)NULL;
 sym->storage_as.c_file.name = (char *)NULL;
 open_files = cons(sym,open_files);
 if (!(sym->storage_as.c_file.f = fopen(name,how)))
 {perror(name);
 put_st("\n");
 err("could not open file",NIL);}
 sym->storage_as.c_file.name = (char *) must_malloc(strlen(name)+1);
 strcpy(sym->storage_as.c_file.name,name);
 no_interrupt(flag);
 return(sym);}

LISP fopen_l(LISP name,LISP how)
}

LISP delq(LISP elem,LISP l)
{if NULLP(l) return(l);
 STACK_CHECK(&elem);
 if EQ(elem,car(l)) return(cdr(l));
 setcdr(l,delq(elem,cdr(l)));
 return(l);}

LISP fclose_l(LISP p)
{long flag;
 flag = no_interrupt(1);
 if NTYPEP(p,tc_c_file) err("not a file",p);
 file_gc_free(p);
 open_files = delq(p,open_files);
 no_interrupt(flag);
 return(NIL);}

LISP vload(char *fname,long cflag)
{LISP form,result,tail,lf;
 FILE *f;
 put_st("loading ");
 put_st(fname);
 put_st("\n");
 lf = fopen_c(fname,"r");
 f = lf->storage_as.c_file.f;
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
 fclose_l(lf);
 put_st("done.\n");
 return(result);}

LISP load(LISP fname,LISP cflag)
{return(vload(get_c_string(fname),NULLP(cflag) ? 0 : 1));}

LISP save_forms(LISP fname,LISP forms,LISP how)
{char *cname,*chow;
 LISP l,lf;
 FILE *f;
 cname = get_c_string(fname);
 if EQ(how,NIL) chow = "w";
 else if EQ(how,cintern("a")) chow = "a";
 else err("bad argument to save-forms",how);
 put_st((*chow == 'a') ? "appending" : "saving");
 put_st(" forms to ");
 put_st(cname);
 put_st("\n");
 lf = fopen_c(cname,chow);
 f = lf->storage_as.c_file.f;
 for(l=forms;NNULLP(l);l=cdr(l))
 {lprin1f(car(l),f);
 putc('\n',f);}
 fclose_l(lf);
 put_st("done.\n");
 return(truth);}

LISP quit(void)
{longjmp(errjmp,2);
 return(NIL);}

LISP nullp(LISP x)
{if EQ(x,NIL) return(truth); else return(NIL);}

LISP arglchk(LISP x)
{LISP l;
 if SYMBOLP(x) return(x);
 for(l=x;CONSP(l);l=CDR(l));
 if NNULLP(l) err("improper formal argument list",x);
 return(x);}

void file_gc_free(LISP ptr)
{if (ptr->storage_as.c_file.f)
 {fclose(ptr->storage_as.c_file.f);
 ptr->storage_as.c_file.f = (FILE *) NULL;}
 if (ptr->storage_as.c_file.name)
 {free(ptr->storage_as.c_file.name);
 ptr->storage_as.c_file.name = NULL;}}
  
void file_prin1(LISP ptr,FILE *f)
{char *name;
 name = ptr->storage_as.c_file.name;
 fput_st(f,"#<FILE ");
 sprintf(tkbuffer," %lX",ptr->storage_as.c_file.f);
 fput_st(f,tkbuffer);
 if (name)
 {fput_st(f," ");
 fput_st(f,name);}
 fput_st(f,">");}

FILE *get_c_file(LISP p,FILE *deflt)
{if (NULLP(p) && deflt) return(deflt);
 if NTYPEP(p,tc_c_file) err("not a file",p);
 if (!p->storage_as.c_file.f) err("file is closed",p);
 return(p->storage_as.c_file.f);}

LISP lgetc(LISP p)
{int i;
 i = f_getc(get_c_file(p,stdin));
 return((i == EOF) ? NIL : flocons((double)i));}

LISP lputc(LISP c,LISP p)
{long flag;
 int i;
 FILE *f;
 f = get_c_file(p,stdout);
 if FLONUMP(c)
 i = FLONM(c);
 else
 i = *get_c_string(c);
 putc(i,f);
 no_interrupt(flag);
 return(NIL);}
  
LISP lputs(LISP str,LISP p)
{fput_st(get_c_file(p,stdout),get_c_string(str));
 return(NIL);}

void init_subrs(void)
{init_subrs_1();
 init_subrs_a();}

void init_subrs_1(void)
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
 init_subr("delq",tc_subr_2,delq);
 init_subr("read",tc_subr_0,lread);
 init_subr("eof-val",tc_subr_0,get_eof_val);
 init_subr("print",tc_subr_1,lprint);
 init_subr("eval",tc_subr_2,leval);
 init_subr("define",tc_fsubr,leval_define);
 init_subr("lambda",tc_fsubr,leval_lambda);
 init_subr("if",tc_msubr,leval_if);
 init_subr("while",tc_fsubr,leval_while);
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
 init_subr("save-forms",tc_subr_3,save_forms);
 init_subr("fopen",tc_subr_2,fopen_l);
 init_subr("fclose",tc_subr_1,fclose_l);
 init_subr("getc",tc_subr_1,lgetc);
 init_subr("putc",tc_subr_2,lputc);
 init_subr("puts",tc_subr_2,lputs);
 init_subr("%%stack-limit",tc_subr_2,stack_limit);}

/* err0,pr,prp are convenient to call from the C-language debugger */

void err0(void)
{err("0",NIL);}

void pr(LISP p)
{if ((p >= heap_org) &&
 (p < heap_end) &&
 (((((char *)p) - ((char *)heap_org)) % sizeof(struct obj)) == 0))
 lprint(p);
 put_st("invalid\n");}

void prp(LISP *p)
{if (!p) return;
 pr(*p);}
