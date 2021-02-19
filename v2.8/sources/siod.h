/* Scheme In One Defun, but in C this time.
 
 *                   COPYRIGHT (c) 1988-1992 BY                             *
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
		struct obj * (*f)(
#if  !defined(VMS) && !defined(CRAY)
				  ...
#endif
				  );} subr;
	struct {struct obj *env;
		struct obj *code;} closure;
	struct {long dim;
		long *data;} long_array;
	struct {long dim;
		double *data;} double_array;
	struct {long dim;
		char *data;} string;
	struct {long dim;
		struct obj **data;} lisp_array;
	struct {FILE *f;
		char *name;} c_file;}
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
#define tc_string       13
#define tc_double_array 14
#define tc_long_array   15
#define tc_lisp_array   16
#define tc_c_file       17
#define tc_user_1 50
#define tc_user_2 51
#define tc_user_3 52
#define tc_user_4 53
#define tc_user_5 54

#define FO_fetch 127
#define FO_store 126
#define FO_list  125
#define FO_listd 124

#define tc_table_dim 100

typedef struct obj* LISP;

#define CONSP(x)   TYPEP(x,tc_cons)
#define FLONUMP(x) TYPEP(x,tc_flonum)
#define SYMBOLP(x) TYPEP(x,tc_symbol)

#define NCONSP(x)   NTYPEP(x,tc_cons)
#define NFLONUMP(x) NTYPEP(x,tc_flonum)
#define NSYMBOLP(x) NTYPEP(x,tc_symbol)

#define TKBUFFERN 256

struct gen_readio
{int (*getc_fcn)(char *);
 void (*ungetc_fcn)(int, char *);
 char *cb_argument;};

#define GETC_FCN(x) (*((*x).getc_fcn))((*x).cb_argument)
#define UNGETC_FCN(c,x) (*((*x).ungetc_fcn))(c,(*x).cb_argument)

void process_cla(int argc,char **argv,int warnflag);
void print_welcome(void);
void print_hs_1(void);
void print_hs_2(void);
long no_interrupt(long n);
LISP get_eof_val(void);
void repl_driver(long want_sigint,long want_init);
void set_repl_hooks(void (*puts_f)(),
		    LISP (*read_f)(),
		    LISP (*eval_f)(),
		    void (*print_f)());
void repl(void) ;
void err(char *message, LISP x);
char *get_c_string(LISP x);
long get_c_long(LISP x);
LISP lerr(LISP message, LISP x);

LISP newcell(long type);
LISP cons(LISP x,LISP y);
LISP consp(LISP x);
LISP car(LISP x);
LISP cdr(LISP x);
LISP setcar(LISP cell, LISP value);
LISP setcdr(LISP cell, LISP value);
LISP flocons(double x);
LISP numberp(LISP x);
LISP plus(LISP x,LISP y);
LISP ltimes(LISP x,LISP y);
LISP difference(LISP x,LISP y);
LISP quotient(LISP x,LISP y);
LISP greaterp(LISP x,LISP y);
LISP lessp(LISP x,LISP y);
LISP eq(LISP x,LISP y);
LISP eql(LISP x,LISP y);
LISP symcons(char *pname,LISP vcell);
LISP symbolp(LISP x);
LISP symbol_boundp(LISP x,LISP env);
LISP symbol_value(LISP x,LISP env);
LISP cintern(char *name);
LISP rintern(char *name);
LISP subrcons(long type, char *name, LISP (*f)());
LISP closure(LISP env,LISP code);
void gc_protect(LISP *location);
void gc_protect_n(LISP *location,long n);
void gc_protect_sym(LISP *location,char *st);

void init_storage(void);

void init_subr(char *name, long type, LISP (*fcn)());
LISP assq(LISP x,LISP alist);
LISP delq(LISP elem,LISP l);
void set_gc_hooks(long type,
		  LISP (*rel)(),
		  LISP (*mark)(),
		  void (*scan)(),
		  void (*free)(),
		  long *kind);
LISP gc_relocate(LISP x);
LISP user_gc(LISP args);
LISP gc_status(LISP args);
void set_eval_hooks(long type,LISP (*fcn)());
LISP leval(LISP x,LISP env);
LISP symbolconc(LISP args);
void set_print_hooks(long type,void (*fcn)());
LISP lprin1f(LISP exp,FILE *f);
LISP lprint(LISP exp);
LISP lread(void);
LISP lreadtk(long j);
LISP lreadf(FILE *f);
void set_read_hooks(char *all_set,char *end_set,
		    LISP (*fcn1)(),LISP (*fcn2)());
LISP oblistfn(void);
LISP vload(char *fname,long cflag);
LISP load(LISP fname,LISP cflag);
LISP save_forms(LISP fname,LISP forms,LISP how);
LISP quit(void);
LISP nullp(LISP x);
void init_subrs();
LISP strcons(long length,char *data);
LISP read_from_string(LISP x);
LISP aref1(LISP a,LISP i);
LISP aset1(LISP a,LISP i,LISP v);
LISP cons_array(LISP dim,LISP kind);
LISP string_append(LISP args);

void init_subrs(void);

LISP copy_list(LISP);


long c_sxhash(LISP,long);
LISP sxhash(LISP,LISP);

LISP href(LISP,LISP);
LISP hset(LISP,LISP,LISP);

LISP fast_print(LISP,LISP);
LISP fast_read(LISP);

LISP equal(LISP,LISP);

LISP assoc(LISP x,LISP alist);

LISP make_list(LISP x,LISP v);

void set_fatal_exit_hook(void (*fcn)(void));

LISP parse_number(LISP x);
