/* Scheme In One Defun, but in C this time.
(c) Copyright 1988 George Carrette, g...@bu-it.bu.edu
For demonstration purposes only.

If your interests run to practical applications of symbolic programming
techniques, in LISP, Macsyma, C, or other language:

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

This example is small, has a garbage collector, and can run a good deal
of the code in Structure and Interpretation of Computer Programs.
(Start it up with the siod.scm file for more features).
Replacing the evaluator with an explicit control "flat-coded" one
as in chapter 5 would allow garbage collection to take place
at any time, not just at toplevel in the read-eval-print loop,
as herein implemented. This is left as an exersize for the reader.

Techniques used will be familiar to most lisp implementors.
Having objects be all the same size, and having only two statically
allocated spaces simplifies and speeds up both consing and gc considerably.
The MSUBR hack allows for a modular implementation of tail recursion,
an extension of the FSUBR that is, as far as I know, original.

Error handling is rather crude. A topic taken with machine fault,
exception handling, tracing, debugging, and state recovery
which we could cover in detail, but clearly beyond the scope of
this implementation. Suffice it to say that if you have a good
symbolic debugger you can set a break point at "err" and observe
in detail all the arguments and local variables of the procedures
in question, since there is no ugly "casting" of data types.
If X is an offending or interesting object then examining
X->type will give you the type, and X->storage_as.cons will
show the car and the cdr.

*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <signal.h>
#include <math.h>

struct obj
{short gc_mark;
short type;
union {struct {struct obj * car;
struct obj * cdr;} cons;
struct {double data;} flonum;
struct {char *pname;
struct obj * vcell;} symbol;
struct {char *name;
struct obj * (*f)();} subr;
struct {struct obj *env;
struct obj *code;} closure;}
storage_as;};

#define CAR(x) ((*x).storage_as.cons.car)
#define CDR(x) ((*x).storage_as.cons.cdr)
#define PNAME(x) ((*x).storage_as.symbol.pname)
#define VCELL(x) ((*x).storage_as.symbol.vcell)
#define SUBRF(x) (*((*x).storage_as.subr.f))
#define FLONM(x) ((*x).storage_as.flonum.data)

struct obj *heap_1;
struct obj *heap_2;
struct obj *heap,*heap_end,*heap_org;
long heap_size = 5000;
long old_heap_used;
int which_heap;
int gc_status_flag = 1;
char *init_file = (char *) NULL;

#define TKBUFFERN 100

char tkbuffer[TKBUFFERN];

jmp_buf errjmp;
int errjmp_ok = 0;
int nointerrupt = 1;

struct obj *cons(), *car(), *cdr(), *setcar(), *setcdr(),*consp();
struct obj *symcons(),*rintern(),*cintern(),*cintern_soft(),*symbolp();
struct obj *flocons(),*plus(),*ltimes(),*difference(),*quotient();
struct obj *greaterp(),*lessp(),*eq(),*eql(),*numberp();
struct obj *assq();
struct obj *lread(),*leval(),*lprint(),*lprin1();
struct obj *lreadr(),*lreadparen(),*lreadtk(),*lreadf();
struct obj *subrcons(),*closure();
struct obj *leval_define(),*leval_lambda(),*leval_if();
struct obj *leval_progn(),*leval_setq(),*leval_let(),*let_macro();
struct obj *leval_args(),*extend_env(),*setvar();
struct obj *leval_quote(),*leval_and(),*leval_or();
struct obj *oblistfn(),*copy_list();
struct obj *gc_relocate(),*get_newspace(),*gc_status();
struct obj *vload(),*load();
struct obj *leval_tenv(),*lerr(),*quit(),*nullp();
struct obj *symbol_boundp(),*symbol_value();
struct obj *envlookup(),*arglchk(),*sys_edit(),*reverse();


int handle_sigfpe();
int handle_sigint();

#define NIL ((struct obj *) 0)
#define EQ(x,y) ((x) == (y))
#define NEQ(x,y) ((x) != (y))
#define NULLP(x) EQ(x,NIL)
#define NNULLP(x) NEQ(x,NIL)

#define TYPE(x) (((x) == NIL) ? 0 : ((*(x)).type))

#define TYPEP(x,y) (TYPE(x) == (y))
#define NTYPEP(x,y) (TYPE(x) != (y))

#define tc_nil 0
#define tc_cons 1
#define tc_flonum 2
#define tc_symbol 3
#define tc_subr_0 4
#define tc_subr_1 5
#define tc_subr_2 6
#define tc_subr_3 7
#define tc_lsubr 8
#define tc_fsubr 9
#define tc_msubr 10
#define tc_closure 11

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
#ifdef vms
init_subr("edit",tc_subr_1,sys_edit);
#endif
init_subr("reverse",tc_subr_1,reverse);
}

struct obj *oblist = NIL;
struct obj *truth = NIL;
struct obj *eof_val = NIL;
struct obj *sym_errobj = NIL;
struct obj *sym_progn = NIL;
struct obj *sym_lambda = NIL;
struct obj *sym_quote = NIL;
struct obj *open_files = NIL;
struct obj *unbound_marker = NIL;

scan_registers()
{oblist = gc_relocate(oblist);
eof_val = gc_relocate(eof_val);
truth = gc_relocate(truth);
sym_errobj = gc_relocate(sym_errobj);
sym_progn = gc_relocate(sym_progn);
sym_lambda = gc_relocate(sym_lambda);
sym_quote = gc_relocate(sym_quote);
open_files = gc_relocate(open_files);
unbound_marker = gc_relocate(unbound_marker);}

main(argc,argv)
int argc; char **argv;
{printf("Welcome to SIOD, Scheme In One Defun, Version 1.3\n");
printf("(C) Copyright 1988, George Carrette\n");
process_cla(argc,argv);
printf("heap_size = %d cells, %d bytes\n",
heap_size,heap_size*sizeof(struct obj));
init_storage();
printf("heap_1 at 0x%X, heap_2 at 0x%X\n",heap_1,heap_2);
repl_driver();
printf("EXIT\n");}

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
#ifdef sun
double myruntime(){return(clock()*1.0e-6);}
#else
#ifdef encore
double myruntime(){return(clock()*1.0e-6);}
#else
#include <sys/types.h>
#include <sys/times.h>
struct tms time_buffer;
double myruntime(){times(&time_buffer);return(time_buffer.tms_utime/60.0);}
#endif
#endif
#else
#ifdef vms
#include <stdlib.h>
double myruntime(){return(clock() * 1.0e-2);}
#include <descrip.h>
struct obj *
sys_edit(fname)
struct obj *fname;
{struct dsc$descriptor_s d;
if NTYPEP(fname,tc_symbol) err("filename not a symbol",fname);
d.dsc$b_dtype = DSC$K_DTYPE_T;
d.dsc$b_class = DSC$K_CLASS_S;
d.dsc$w_length = strlen(PNAME(fname));
d.dsc$a_pointer = PNAME(fname);
nointerrupt = 1;
edt$edit(&d);
nointerrupt = 0;
return(fname);}
#else
double myruntime(){long x;long time();time(&x);return(x);}
#endif
#endif

handle_sigfpe(sig,code,scp)
int sig,code; struct sigcontext *scp;
{signal(SIGFPE,handle_sigfpe);
err("floating point exception",NIL);}

handle_sigint(sig,code,scp)
int sig,code; struct sigcontext *scp;
{signal(SIGINT,handle_sigint);
if (nointerrupt == 0) err("control-c interrupt",NIL);
printf("interrupts disabled\n");}

repl()
{struct obj *x,*cw;
double rt;
while(1)
{if ((gc_status_flag) || heap >= heap_end)
{rt = myruntime();
gc();
printf("GC took %g seconds, %d compressed to %d, %d free\n",
myruntime()-rt,old_heap_used,heap-heap_org,heap_end-heap);}
printf("> ");
x = lread();
if EQ(x,eof_val) break;
rt = myruntime();
cw = heap;
x = leval(x,NIL);
printf("Evaluation took %g seconds %d cons work\n",
myruntime()-rt,heap-cw);
lprint(x);}}

err(message,x)
char *message; struct obj *x;
{nointerrupt = 1;
if NNULLP(x)
printf("ERROR: %s (see errobj)\n",message);
else printf("ERROR: %s\n",message);
if (errjmp_ok == 1) {setvar(sym_errobj,x,NIL); longjmp(errjmp,1);}
printf("FATAL ERROR DURING STARTUP OR CRITICAL CODE SECTION\n");
exit(1);}

struct obj *
lerr(message,x)
struct obj *message,*x;
{if NTYPEP(message,tc_symbol) err("argument to error not a symbol",message);
err(PNAME(message),x);
return(NIL);}

struct obj *
cons(x,y)
struct obj *x,*y;
{register struct obj *z;
if ((z = heap) >= heap_end) err("ran out of storage",NIL);
heap = z+1;
(*z).gc_mark = 0;
(*z).type = tc_cons;
CAR(z) = x;
CDR(z) = y;
return(z);}

struct obj *
consp(x)
struct obj *x;
{if TYPEP(x,tc_cons) return(truth); else return(NIL);}

struct obj *
car(x)
struct obj *x;
{switch TYPE(x)
{case tc_nil:
return(NIL);
case tc_cons:
return(CAR(x));
default:
err("wta to car",x);}}

struct obj *
cdr(x)
struct obj *x;
{switch TYPE(x)
{case tc_nil:
return(NIL);
case tc_cons:
return(CDR(x));
default:
err("wta to cdr",x);}}

struct obj *
setcar(cell,value)
struct obj *cell,*value;
{if NTYPEP(cell,tc_cons) err("wta to setcar",cell);
return(CAR(cell) = value);}

struct obj *
setcdr(cell,value)
struct obj *cell,*value;
{if NTYPEP(cell,tc_cons) err("wta to setcdr",cell);
return(CDR(cell) = value);}

struct obj *
flocons(x)
double x;
{register struct obj *z;
if ((z = heap) >= heap_end) err("ran out of storage",NIL);
heap = z+1;
(*z).gc_mark = 0;
(*z).type = tc_flonum;
(*z).storage_as.flonum.data = x;
return(z);}

struct obj *
numberp(x)
struct obj *x;
{if TYPEP(x,tc_flonum) return(truth); else return(NIL);}

struct obj *
plus(x,y)
struct obj *x,*y;
{if NTYPEP(x,tc_flonum) err("wta(1st) to plus",x);
if NTYPEP(y,tc_flonum) err("wta(2nd) to plus",y);
return(flocons(FLONM(x)+FLONM(y)));}

struct obj *
ltimes(x,y)
struct obj *x,*y;
{if NTYPEP(x,tc_flonum) err("wta(1st) to times",x);
if NTYPEP(y,tc_flonum) err("wta(2nd) to times",y);
return(flocons(FLONM(x)*FLONM(y)));}

struct obj *
difference(x,y)
struct obj *x,*y;
{if NTYPEP(x,tc_flonum) err("wta(1st) to difference",x);
if NTYPEP(y,tc_flonum) err("wta(2nd) to difference",y);
return(flocons(FLONM(x)-FLONM(y)));}

struct obj *
quotient(x,y)
struct obj *x,*y;
{if NTYPEP(x,tc_flonum) err("wta(1st) to quotient",x);
if NTYPEP(y,tc_flonum) err("wta(2nd) to quotient",y);
return(flocons(FLONM(x)/FLONM(y)));}

struct obj *
greaterp(x,y)
struct obj *x,*y;
{if NTYPEP(x,tc_flonum) err("wta(1st) to greaterp",x);
if NTYPEP(y,tc_flonum) err("wta(2nd) to greaterp",y);
if (FLONM(x)>FLONM(y)) return(truth);
return(NIL);}

struct obj *
lessp(x,y)
struct obj *x,*y;
{if NTYPEP(x,tc_flonum) err("wta(1st) to lessp",x);
if NTYPEP(y,tc_flonum) err("wta(2nd) to lessp",y);
if (FLONM(x)<FLONM(y)) return(truth);
return(NIL);}

struct obj *
eq(x,y)
struct obj *x,*y;
{if EQ(x,y) return(truth); else return(NIL);}

struct obj *
eql(x,y)
struct obj *x,*y;
{if EQ(x,y) return(truth); else
if NTYPEP(x,tc_flonum) return(NIL); else
if NTYPEP(y,tc_flonum) return(NIL); else
if (FLONM(x) == FLONM(y)) return(truth);
return(NIL);}

struct obj *
symcons(pname,vcell)
char *pname; struct obj *vcell;
{register struct obj *z;
if ((z = heap) >= heap_end) err("ran out of storage",NIL);
heap = z+1;
(*z).gc_mark = 0;
(*z).type = tc_symbol;
PNAME(z) = pname;
VCELL(z) = vcell;
return(z);}

struct obj *
symbolp(x)
struct obj *x;
{if TYPEP(x,tc_symbol) return(truth); else return(NIL);}

struct obj *
symbol_boundp(x,env)
struct obj *x,*env;
{struct obj *tmp;
if NTYPEP(x,tc_symbol) err("not a symbol",x);
tmp = envlookup(x,env);
if NNULLP(tmp) return(truth);
if EQ(VCELL(x),unbound_marker) return(NIL); else return(truth);}

struct obj *
symbol_value(x,env)
struct obj *x,*env;
{struct obj *tmp;
if NTYPEP(x,tc_symbol) err("not a symbol",x);
tmp = envlookup(x,env);
if NNULLP(tmp) return(CAR(tmp));
tmp = VCELL(x);
if EQ(tmp,unbound_marker) err("unbound variable",x);
return(tmp);}

struct obj *
cintern_soft(name)
char *name;
{struct obj *l;
for(l=oblist;NNULLP(l);l=CDR(l))
if (strcmp(name,PNAME(CAR(l))) == 0) return(CAR(l));
return(NIL);}

struct obj *
cintern(name)
char *name;
{struct obj *sym;
sym = cintern_soft(name);
if(sym) return(sym);
sym = symcons(name,unbound_marker);
oblist = cons(sym,oblist);
return(sym);}

char *
must_malloc(size)
unsigned long size;
{char *tmp;
tmp = (char *) malloc(size);
if (tmp == (char *)NULL) err("failed to allocate storage from system",NIL);
return(tmp);}

struct obj *
rintern(name)
char *name;
{struct obj *sym;
char *newname;
sym = cintern_soft(name);
if(sym) return(sym);
newname = must_malloc(strlen(name)+1);
strcpy(newname,name);
sym = symcons(newname,unbound_marker);
oblist = cons(sym,oblist);
return(sym);}

struct obj *
subrcons(type,name,f)
int type; char *name; struct obj * (*f)();
{register struct obj *z;
if ((z = heap) >= heap_end) err("ran out of storage",NIL);
heap = z+1;
(*z).gc_mark = 0;
(*z).type = type;
(*z).storage_as.subr.name = name;
(*z).storage_as.subr.f = f;
return(z);}

struct obj *
closure(env,code)
struct obj *env,*code;
{register struct obj *z;
if ((z = heap) >= heap_end) err("ran out of storage",NIL);
heap = z+1;
(*z).gc_mark = 0;
(*z).type = tc_closure;
(*z).storage_as.closure.env = env;
(*z).storage_as.closure.code = code;
return(z);}

init_storage()
{int j;
heap_1 = (struct obj *)must_malloc(sizeof(struct obj)*heap_size);
heap_2 = (struct obj *)must_malloc(sizeof(struct obj)*heap_size);
heap = heap_1;
which_heap = 1;
heap_org = heap;
heap_end = heap + heap_size;
unbound_marker = cons(cintern("**unbound-marker**"),NIL);
eof_val = cons(cintern("eof"),NIL);
truth = cintern("t");
setvar(truth,truth,NIL);
setvar(cintern("nil"),NIL,NIL);
setvar(cintern("let"),cintern("let-internal-macro"),NIL);
sym_errobj = cintern("errobj");
setvar(sym_errobj,NIL,NIL);
sym_progn = cintern("begin");
sym_lambda = cintern("lambda");
sym_quote = cintern("quote");
init_subrs();}

init_subr(name,type,fcn)
char *name; int type; struct obj *(*fcn)();
{setvar(cintern(name),subrcons(type,name,fcn),NIL);}

struct obj *
assq(x,alist)
struct obj *x,*alist;
{register struct obj *l,*tmp;
for(l=alist;TYPEP(l,tc_cons);l=CDR(l))
{tmp = CAR(l);
if (TYPEP(tmp,tc_cons) && EQ(CAR(tmp),x)) return(tmp);}
if EQ(l,NIL) return(NIL);
err("improper list to assq",alist);}

struct obj *
gc_relocate(x)
struct obj *x;
{struct obj *new;
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

struct obj *
get_newspace()
{struct obj * newspace;
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
struct obj *newspace;
{register struct obj *ptr;
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
{struct obj *newspace;
errjmp_ok = 0;
nointerrupt = 1;
old_heap_used = heap - heap_org;
newspace = get_newspace();
scan_registers();
scan_newspace(newspace);
errjmp_ok = 1;
nointerrupt = 0;}

struct obj *
gc_status(args)
struct obj *args;
{if NNULLP(args)
if NULLP(car(args)) gc_status_flag = 0; else gc_status_flag = 1;
if (gc_status_flag)
printf("garbage collection is on\n"); else
printf("garbage collection is off\n");
printf("%d allocated %d free\n",heap - heap_org, heap_end - heap);
return(NIL);}

struct obj *
leval_args(l,env)
struct obj *l,*env;
{struct obj *result,*v1,*v2,*tmp;
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

struct obj *
extend_env(actuals,formals,env)
struct obj *actuals,*formals,*env;
{if TYPEP(formals,tc_symbol)
return(cons(cons(cons(formals,NIL),cons(actuals,NIL)),env));
return(cons(cons(formals,actuals),env));}

struct obj *
envlookup(var,env)
struct obj *var,*env;
{struct obj *frame,*al,*fl,*tmp;
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

struct obj *
leval(x,env)
struct obj *x,*env;
{struct obj *tmp;
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

struct obj *
setvar(var,val,env)
struct obj *var,*val,*env;
{struct obj *tmp;
if NTYPEP(var,tc_symbol) err("wta(non-symbol) to setvar",var);
tmp = envlookup(var,env);
if NULLP(tmp) return(VCELL(var) = val);
return(CAR(tmp)=val);}

struct obj *
leval_setq(args,env)
struct obj *args,*env;
{return(setvar(car(args),leval(car(cdr(args)),env),env));}

struct obj *
syntax_define(args)
struct obj *args;
{if TYPEP(car(args),tc_symbol) return(args);
return(syntax_define(
cons(car(car(args)),
cons(cons(sym_lambda,
cons(cdr(car(args)),
cdr(args))),
NIL))));}

struct obj *
leval_define(args,env)
struct obj *args,*env;
{struct obj *tmp,*var,*val;
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

struct obj *
leval_if(pform,penv)
struct obj **pform,**penv;
{struct obj *args,*env;
args = cdr(*pform);
env = *penv;
if NNULLP(leval(car(args),env))
*pform = car(cdr(args)); else *pform = car(cdr(cdr(args)));
return(truth);}

struct obj *
leval_lambda(args,env)
struct obj *args,*env;
{struct obj *body;
if NULLP(cdr(cdr(args)))
body = car(cdr(args));
else body = cons(sym_progn,cdr(args));
return(closure(env,cons(arglchk(car(args)),body)));}

struct obj *
leval_progn(pform,penv)
struct obj **pform,**penv;
{struct obj *env,*l,*next;
env = *penv;
l = cdr(*pform);
next = cdr(l);
while(NNULLP(next)) {leval(car(l),env);l=next;next=cdr(next);}
*pform = car(l);
return(truth);}

struct obj *
leval_or(pform,penv)
struct obj **pform,**penv;
{struct obj *env,*l,*next,*val;
env = *penv;
l = cdr(*pform);
next = cdr(l);
while(NNULLP(next))
{val = leval(car(l),env);
if NNULLP(val) {*pform = val; return(NIL);}
l=next;next=cdr(next);}
*pform = car(l);
return(truth);}

struct obj *
leval_and(pform,penv)
struct obj **pform,**penv;
{struct obj *env,*l,*next;
env = *penv;
l = cdr(*pform);
if NULLP(l) {*pform = truth; return(NIL);}
next = cdr(l);
while(NNULLP(next))
{if NULLP(leval(car(l),env)) {*pform = NIL; return(NIL);}
l=next;next=cdr(next);}
*pform = car(l);
return(truth);}

struct obj *
leval_let(pform,penv)
struct obj **pform,**penv;
{struct obj *env,*l;
l = cdr(*pform);
env = *penv;
*penv = extend_env(leval_args(car(cdr(l)),env),car(l),env);
*pform = car(cdr(cdr(l)));
return(truth);}

struct obj *
reverse(l)
struct obj *l;
{struct obj *n,*p;
n = NIL;
for(p=l;NNULLP(p);p=cdr(p)) n = cons(car(p),n);
return(n);}

struct obj *
let_macro(form)
struct obj *form;
{struct obj *p,*fl,*al,*tmp;
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

struct obj *
leval_quote(args,env)
struct obj *args,*env;
{return(car(args));}

struct obj *
leval_tenv(args,env)
struct obj *args,*env;
{return(env);}

struct obj *
lprint(exp)
struct obj *exp;
{lprin1(exp);
printf("\n");
return(NIL);}

struct obj *
lprin1(exp)
struct obj *exp;
{struct obj *tmp;
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

struct obj *
lread()
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

struct obj *
lreadf(f)
FILE *f;
{int c;
c = flush_ws(f,(char *)NULL);
if (c == EOF) return(eof_val);
ungetc(c,f);
return(lreadr(f));}

struct obj *
lreadr(f)
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

struct obj *
lreadparen(f)
FILE *f;
{int c;
struct obj *tmp;
c = flush_ws(f,"end of file inside list");
if (c == ')') return(NIL);
ungetc(c,f);
tmp = lreadr(f);
return(cons(tmp,lreadparen(f)));}

struct obj *
lreadtk(j)
int j;
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

struct obj *
copy_list(x)
struct obj *x;
{if NULLP(x) return(NIL);
return(cons(car(x),copy_list(cdr(x))));}

struct obj *
oblistfn()
{return(copy_list(oblist));}

close_open_files()
{struct obj *l;
FILE *p;
for(l=open_files;NNULLP(l);l=cdr(l))
{p = (FILE *) PNAME(car(l));
if (p)
{printf("closing a file left open\n");
fclose(p);}}
open_files = NIL;}


struct obj *
vload(fname)
char *fname;
{struct obj *sym,*form;
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

struct obj *
load(fname)
struct obj *fname;
{if NTYPEP(fname,tc_symbol) err("filename not a symbol",fname);
return(vload(PNAME(fname)));}

struct obj *
quit()
{longjmp(errjmp,2);
return(NIL);}

struct obj *
nullp(x)
struct obj *x;
{if EQ(x,NIL) return(truth); else return(NIL);}

struct obj *
arglchk(x)
struct obj *x;
{struct obj *l;
if TYPEP(x,tc_symbol) return(x);
for(l=x;TYPEP(l,tc_cons);l=CDR(l));
if NNULLP(l) err("improper formal argument list",x);
return(x);}


