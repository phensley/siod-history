/* Scheme In One Defun, but in C this time.
 
 *                        COPYRIGHT (c) 1989 BY                             *
 *        PARADIGM ASSOCIATES INCORPORATED, CAMBRIDGE, MASSACHUSETTS.       *
 *        See the source file SLIB.C for more information.                  *

*/

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

#define NIL ((struct obj *) 0)
#define EQ(x,y) ((x) == (y))
#define NEQ(x,y) ((x) != (y))
#define NULLP(x) EQ(x,NIL)
#define NNULLP(x) NEQ(x,NIL)

#define TYPE(x) (((x) == NIL) ? 0 : ((*(x)).type))

#define TYPEP(x,y) (TYPE(x) == (y))
#define NTYPEP(x,y) (TYPE(x) != (y))

#define tc_nil    0
#define tc_cons   1
#define tc_flonum 2
#define tc_symbol 3
#define tc_subr_0 4
#define tc_subr_1 5
#define tc_subr_2 6
#define tc_subr_3 7
#define tc_lsubr  8
#define tc_fsubr  9
#define tc_msubr  10
#define tc_closure 11
#define tc_free_cell 12
#define tc_user_1 13
#define tc_user_2 14
#define tc_user_3 15
#define tc_user_4 16
#define tc_user_5 17


typedef struct obj* LISP;

#define CONSP(x)   TYPEP(x,tc_cons)
#define FLONUMP(x) TYPEP(x,tc_flonum)
#define SYMBOLP(x) TYPEP(x,tc_symbol)

#define NCONSP(x)   NTYPEP(x,tc_cons)
#define NFLONUMP(x) NTYPEP(x,tc_flonum)
#define NSYMBOLP(x) NTYPEP(x,tc_symbol)

#define TKBUFFERN 256

char *must_malloc();

LISP cons(), car(), cdr(), setcar();
LISP setcdr(),consp();

LISP symcons(),rintern(), cintern();
LISP cintern_soft();
LISP symbolp();

LISP flocons();
LISP plus(),ltimes(),difference();
LISP quotient(), greaterp(), lessp();

LISP eq(),eql(),numberp();
LISP assq();

LISP lread(),leval(),lprint(),lprin1();

LISP subrcons();
LISP closure();

LISP leval_define(),leval_lambda(),leval_if();
LISP leval_progn(),leval_setq(),leval_let(),let_macro();
LISP leval_args(),extend_env(),setvar();
LISP leval_quote(),leval_and(),leval_or();
LISP oblistfn(),copy_list();
LISP gc_relocate(),get_newspace(),gc_status();
LISP vload(),load();
LISP leval_tenv(),lerr(),quit(),nullp();
LISP symbol_boundp(),symbol_value();
LISP envlookup(),arglchk(),reverse();


void gc_protect();
void gc_protect_n();

long no_interrupt();

void init_subr();

LISP get_eof_val();
void set_repl_hooks();

void set_gc_hooks();

void set_eval_hooks();

void set_print_hooks();

void set_read_hooks();

LISP gen_read();

struct gen_readio
{int (*getc_fcn)();
 void (*ungetc_fcn)();
 char *cb_argument;};

#define GETC_FCN(x) (*((*x).getc_fcn))((*x).cb_argument)
#define UNGETC_FCN(c,x) (*((*x).ungetc_fcn))(c,(*x).cb_argument)
