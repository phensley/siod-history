/*

 * COPYRIGHT (c) 1988-1994 BY *
 * PARADIGM ASSOCIATES INCORPORATED, CAMBRIDGE, MASSACHUSETTS. *
 * See the source file SLIB.C for more information. *


Array-hacking code moved to another source file.



*/

#include <stdio.h>

#include <string.h>
#include <setjmp.h>
#include <stdlib.h>
#include <ctype.h>



#include "siod.h"
#include "siodp.h"


LISP bashnum = NIL;

void init_storage_a1(long type)
{long j;
 struct user_type_hooks *p;
 set_gc_hooks(type,
 array_gc_relocate,
 array_gc_mark,
 array_gc_scan,
 array_gc_free,
 &j);
 set_print_hooks(type,array_prin1);
 p = get_user_type_hooks(type);
 p->fast_print = array_fast_print;
 p->fast_read = array_fast_read;
 p->equal = array_equal;
 p->c_sxhash = array_sxhash;}

void init_storage_a(void)
{gc_protect(&bashnum);
 bashnum = newcell(tc_flonum);
 init_storage_a1(tc_string);
 init_storage_a1(tc_double_array);
 init_storage_a1(tc_long_array);
 init_storage_a1(tc_lisp_array);}

LISP array_gc_relocate(LISP ptr)
{LISP nw;
 if ((nw = heap) >= heap_end) gc_fatal_error();
 heap = nw+1;
 memcpy(nw,ptr,sizeof(struct obj));
 return(nw);}

void array_gc_scan(LISP ptr)
{long j;
 if TYPEP(ptr,tc_lisp_array)
 for(j=0;j < ptr->storage_as.lisp_array.dim; ++j)
 ptr->storage_as.lisp_array.data[j] =
 gc_relocate(ptr->storage_as.lisp_array.data[j]);}

LISP array_gc_mark(LISP ptr)
{long j;
 if TYPEP(ptr,tc_lisp_array)
 for(j=0;j < ptr->storage_as.lisp_array.dim; ++j)
 gc_mark(ptr->storage_as.lisp_array.data[j]);
 return(NIL);}

void array_gc_free(LISP ptr)
{switch (ptr->type)
 {case tc_string:
 free(ptr->storage_as.string.data);
 break;
 case tc_double_array:
 free(ptr->storage_as.double_array.data);
 break;
 case tc_long_array:
 free(ptr->storage_as.long_array.data);
 break;
 case tc_lisp_array:
 free(ptr->storage_as.lisp_array.data);
 break;}}

void array_prin1(LISP ptr,FILE *f)
{int j;
 switch (ptr->type)
 {case tc_string:
 fput_st(f,"\"");
 fput_st(f,ptr->storage_as.string.data);
 fput_st(f,"\"");
 break;
 case tc_double_array:
 fput_st(f,"#(");
 for(j=0; j < ptr->storage_as.double_array.dim; ++j)
 {sprintf(tkbuffer,"%g",ptr->storage_as.double_array.data[j]);
 fput_st(f,tkbuffer);
 if ((j + 1) < ptr->storage_as.double_array.dim)
 fput_st(f," ");}
 fput_st(f,")");
 break;
 case tc_long_array:
 fput_st(f,"#(");
 for(j=0; j < ptr->storage_as.long_array.dim; ++j)
 {sprintf(tkbuffer,"%ld",ptr->storage_as.long_array.data[j]);
 fput_st(f,tkbuffer);
 if ((j + 1) < ptr->storage_as.long_array.dim)
 fput_st(f," ");}
 fput_st(f,")");
 break;
 case tc_lisp_array:
 fput_st(f,"#(");
 for(j=0; j < ptr->storage_as.lisp_array.dim; ++j)
 {lprin1f(ptr->storage_as.lisp_array.data[j],f);
 if ((j + 1) < ptr->storage_as.lisp_array.dim)
 fput_st(f," ");}
 fput_st(f,")");
 break;}}

LISP strcons(long length,char *data)


{long flag;
 LISP s;
 flag = no_interrupt(1);
 s = cons(NIL,NIL);

 s->type = tc_string;
 s->storage_as.string.data = must_malloc(length+1);
 s->storage_as.string.dim = length;
 if (data)
 {memcpy(s->storage_as.string.data,data,length);
 s->storage_as.string.data[length] = 0;}
 no_interrupt(flag);
 return(s);}



int rfs_getc(unsigned char **p)

{int i;
 i = **p;
 if (!i) return(EOF);
 *p = *p + 1;
 return(i);}



void rfs_ungetc(unsigned char c,unsigned char **p)

{*p = *p - 1;}

LISP read_from_string(LISP x)
{char *p;
 struct gen_readio s;
 p = get_c_string(x);
 s.getc_fcn = (int (*)(char *))rfs_getc;
 s.ungetc_fcn = (void (*)(int, char *))rfs_ungetc;
 s.cb_argument = (char *) &p;
 return(readtl(&s));}

LISP aref1(LISP a,LISP i)
{long k;
 if NFLONUMP(i) err("bad index to aref",i);
 k = (long) FLONM(i);
 if (k < 0) err("negative index to aref",i);
 switch (a->type)
 {case tc_string:
 if (k >= a->storage_as.string.dim) err("index too large",i);
 return(flocons((double) a->storage_as.string.data[k]));
 case tc_double_array:
 if (k >= a->storage_as.double_array.dim) err("index too large",i);
 return(flocons(a->storage_as.double_array.data[k]));
 case tc_long_array:
 if (k >= a->storage_as.long_array.dim) err("index too large",i);
 return(flocons(a->storage_as.long_array.data[k]));
 case tc_lisp_array:
 if (k >= a->storage_as.lisp_array.dim) err("index too large",i);
 return(a->storage_as.lisp_array.data[k]);
 default:
 return(err("invalid argument to aref",a));}}

void err1_aset1(LISP i)
{err("index to aset too large",i);}

void err2_aset1(LISP v)
{err("bad value to store in array",v);}



LISP aset1(LISP a,LISP i,LISP v)

{long k;
 if NFLONUMP(i) err("bad index to aset",i);
 k = (long) FLONM(i);
 if (k < 0) err("negative index to aset",i);
 switch (a->type)
 {case tc_string:
 if NFLONUMP(v) err2_aset1(v);
 if (k >= a->storage_as.string.dim) err1_aset1(i);
 a->storage_as.string.data[k] = (char) FLONM(v);
 return(v);
 case tc_double_array:
 if NFLONUMP(v) err2_aset1(v);
 if (k >= a->storage_as.double_array.dim) err1_aset1(i);
 a->storage_as.double_array.data[k] = FLONM(v);
 return(v);
 case tc_long_array:
 if NFLONUMP(v) err2_aset1(v);
 if (k >= a->storage_as.long_array.dim) err1_aset1(i);
 a->storage_as.long_array.data[k] = (long) FLONM(v);
 return(v);
 case tc_lisp_array:
 if (k >= a->storage_as.lisp_array.dim) err1_aset1(i);
 a->storage_as.lisp_array.data[k] = v;
 return(v);
 default:
 return(err("invalid argument to aset",a));}}

LISP cons_array(LISP dim,LISP kind)
{LISP a;
 long flag,n,j;
 if (NFLONUMP(dim) || (FLONM(dim) < 0))
 return(err("bad dimension to cons-array",dim));
 else
 n = (long) FLONM(dim);
 flag = no_interrupt(1);
 a = cons(NIL,NIL);
 if EQ(cintern("double"),kind)
 {a->type = tc_double_array;
 a->storage_as.double_array.dim = n;
 a->storage_as.double_array.data = (double *) must_malloc(n *
 sizeof(double));
 for(j=0;j<n;++j) a->storage_as.double_array.data[j] = 0.0;}
 else if EQ(cintern("long"),kind)
 {a->type = tc_long_array;
 a->storage_as.long_array.dim = n;
 a->storage_as.long_array.data = (long *) must_malloc(n * sizeof(long));
 for(j=0;j<n;++j) a->storage_as.long_array.data[j] = 0;}
 else if EQ(cintern("string"),kind)
 {a->type = tc_string;
 a->storage_as.double_array.dim = n+1;
 a->storage_as.string.data = (char *) must_malloc(n+1);
 a->storage_as.string.data[n] = 0;
 for(j=0;j<n;++j) a->storage_as.string.data[j] = ' ';}
 else if (EQ(cintern("lisp"),kind) || NULLP(kind))
 {a->type = tc_lisp_array;
 a->storage_as.lisp_array.dim = n;
 a->storage_as.lisp_array.data = (LISP *) must_malloc(n * sizeof(LISP));
 for(j=0;j<n;++j) a->storage_as.lisp_array.data[j] = NIL;}
 else
 err("bad type of array",kind);
 no_interrupt(flag);
 return(a);}

LISP string_append(LISP args)
{long size;
 LISP l,s;
 char *data;
 size = 0;
 for(l=args;NNULLP(l);l=cdr(l))
 size += strlen(get_c_string(car(l)));
 s = strcons(size,NULL);
 data = s->storage_as.string.data;
 data[0] = 0;
 for(l=args;NNULLP(l);l=cdr(l))
 strcat(data,get_c_string(car(l)));
 return(s);}

LISP substring(LISP str,LISP start,LISP end)
{long s,e,n;
 if NTYPEP(str,tc_string) err("not a string",str);
 n = str->storage_as.string.dim;
 s = get_c_long(start);
 if NULLP(end)
 e = n;
 else
 e = get_c_long(end);
 if ((s < 0) || (s > e)) err("bad start index",start);
 if ((e < 0) || (e > n)) err("bad end index",end);
 return(strcons(e-s,&str->storage_as.string.data[s]));}

LISP string_search(LISP token,LISP str)
{char *s1,*s2,*ptr;
 s1 = get_c_string(str);
 s2 = get_c_string(token);
 ptr = strstr(s1,s2);
 if (ptr)
 return(flocons(ptr - s1));
 else
 return(NIL);}

LISP string_trim(LISP str)
{char *start,*end;
 start = get_c_string(str);
 while(*start && (*start == ' ')) ++start;
 end = &start[strlen(start)];
 while((end > start) && (*(end-1) == ' ')) --end;
 return(strcons(end-start,start));}

LISP string_trim_left(LISP str)
{char *start,*end;
 start = get_c_string(str);
 while(*start && (*start == ' ')) ++start;
 end = &start[strlen(start)];
 return(strcons(end-start,start));}

LISP string_trim_right(LISP str)
{char *start,*end;
 start = get_c_string(str);
 end = &start[strlen(start)];
 while((end > start) && (*(end-1) == ' ')) --end;
 return(strcons(end-start,start));}

LISP string_upcase(LISP str)
{LISP result;
 char *s1,*s2;
 long j,n;
 s1 = get_c_string(str);
 n = strlen(s1);
 result = strcons(n,s1);
 s2 = get_c_string(result);
 for(j=0;j<n;++j) s2[j] = toupper(s2[j]);
 return(result);}

LISP string_downcase(LISP str)
{LISP result;
 char *s1,*s2;
 long j,n;
 s1 = get_c_string(str);
 n = strlen(s1);
 result = strcons(n,s1);
 s2 = get_c_string(result);
 for(j=0;j<n;++j) s2[j] = tolower(s2[j]);
 return(result);}

LISP lreadstring(struct gen_readio *f)
{int j,c,n;
 char *p;
 j = 0;
 p = tkbuffer;
 while(((c = GETC_FCN(f)) != '"') && (c != EOF))
 {if (c == '\\')
 {c = GETC_FCN(f);
 if (c == EOF) err("eof after \\",NIL);
 switch(c)
 {case 'n':
 c = '\n';
 break;
 case 't':
 c = '\t';
 break;
 case 'r':
 c = '\r';
 break;
 case 'd':
 c = 0x04;
 break;
 case 'N':
 c = 0;
 break;
 case 's':
 c = ' ';
 break;
 case '0':
 n = 0;
 while(1)
 {c = GETC_FCN(f);
 if (c == EOF) err("eof after \\0",NIL);
 if (isdigit(c))
 n = n * 8 + c - '0';
 else
 {UNGETC_FCN(c,f);
 break;}}
 c = n;}}
 if ((j + 1) >= TKBUFFERN) err("read string overflow",NIL);
 ++j;
 *p++ = c;}
 *p = 0;
 return(strcons(j,tkbuffer));}

#define HASH_COMBINE(_h1,_h2,_mod) ((((_h1) * 17 + 1) ^ (_h2)) % (_mod))

long c_sxhash(LISP obj,long n)
{long hash;
 unsigned char *s;
 LISP tmp;
 struct user_type_hooks *p;
 STACK_CHECK(&obj);
 INTERRUPT_CHECK();
 switch TYPE(obj)
 {case tc_nil:
 return(0);
 case tc_cons:
 hash = c_sxhash(CAR(obj),n);
 for(tmp=CDR(obj);CONSP(tmp);tmp=CDR(tmp))
 hash = HASH_COMBINE(hash,c_sxhash(CAR(tmp),n),n);
 hash = HASH_COMBINE(hash,c_sxhash(tmp,n),n);
 return(hash);
 case tc_symbol:
 for(hash=0,s=(unsigned char *)PNAME(obj);*s;++s)
 hash = HASH_COMBINE(hash,*s,n);
 return(hash);
 case tc_subr_0:
 case tc_subr_1:
 case tc_subr_2:
 case tc_subr_3:
 case tc_lsubr:
 case tc_fsubr:
 case tc_msubr:
 for(hash=0,s=(unsigned char *) obj->storage_as.subr.name;*s;++s)
 hash = HASH_COMBINE(hash,*s,n);
 return(hash);
 case tc_flonum:
 return(((unsigned long)FLONM(obj)) % n);
 default:
 p = get_user_type_hooks(TYPE(obj));
 if (p->c_sxhash)
 return((*p->c_sxhash)(obj,n));
 else
 return(0);}}

LISP sxhash(LISP obj,LISP n)
{return(flocons(c_sxhash(obj,FLONUMP(n) ? (long) FLONM(n) : 10000)));}

LISP equal(LISP a,LISP b)
{struct user_type_hooks *p;
 long atype;
 STACK_CHECK(&a);
 loop:
 INTERRUPT_CHECK();
 if EQ(a,b) return(truth);
 atype = TYPE(a);
 if (atype != TYPE(b)) return(NIL);
 switch(atype)
 {case tc_cons:
 if NULLP(equal(car(a),car(b))) return(NIL);
 a = cdr(a);
 b = cdr(b);
 goto loop;
 case tc_flonum:
 return((FLONM(a) == FLONM(b)) ? truth : NIL);
 case tc_symbol:
 return(NIL);
 default:
 p = get_user_type_hooks(atype);
 if (p->equal)
 return((*p->equal)(a,b));
 else
 return(NIL);}}

LISP array_equal(LISP a,LISP b)
{long j,len;
 switch(TYPE(a))
 {case tc_string:
 len = a->storage_as.string.dim;
 if (len != b->storage_as.string.dim) return(NIL);
 if (memcmp(a->storage_as.string.data,b->storage_as.string.data,len) == 0)
 return(truth);
 else
 return(NIL);
 case tc_long_array:
 len = a->storage_as.long_array.dim;
 if (len != b->storage_as.long_array.dim) return(NIL);
 if (memcmp(a->storage_as.long_array.data,
 b->storage_as.long_array.data,
 len * sizeof(long)) == 0)
 return(truth);
 else
 return(NIL);
 case tc_double_array:
 len = a->storage_as.double_array.dim;
 if (len != b->storage_as.double_array.dim) return(NIL);
 for(j=0;j<len;++j)
 if (a->storage_as.double_array.data[j] !=
 b->storage_as.double_array.data[j])
 return(NIL);
 return(truth);
 case tc_lisp_array:
 len = a->storage_as.lisp_array.dim;
 if (len != b->storage_as.lisp_array.dim) return(NIL);
 for(j=0;j<len;++j)
 if NULLP(equal(a->storage_as.lisp_array.data[j],
 b->storage_as.lisp_array.data[j]))
 return(NIL);
 return(truth);


 default:
 return(errswitch());}}


long array_sxhash(LISP a,long n)
{long j,len,hash;
 unsigned char *char_data;
 unsigned long *long_data;
 double *double_data;
 switch(TYPE(a))
 {case tc_string:
 len = a->storage_as.string.dim;
 for(j=0,hash=0,char_data=(unsigned char *)a->storage_as.string.data;
 j < len;
 ++j,++char_data)
 hash = HASH_COMBINE(hash,*char_data,n);
 return(hash);
 case tc_long_array:
 len = a->storage_as.long_array.dim;
 for(j=0,hash=0,long_data=(unsigned long *)a->storage_as.long_array.data;
 j < len;
 ++j,++long_data)
 hash = HASH_COMBINE(hash,*long_data % n,n);
 return(hash);
 case tc_double_array:
 len = a->storage_as.double_array.dim;
 for(j=0,hash=0,double_data=a->storage_as.double_array.data;
 j < len;
 ++j,++double_data)
 hash = HASH_COMBINE(hash,(unsigned long)*double_data % n,n);
 return(hash);
 case tc_lisp_array:
 len = a->storage_as.lisp_array.dim;
 for(j=0,hash=0; j < len; ++j)
 hash = HASH_COMBINE(hash,
 c_sxhash(a->storage_as.lisp_array.data[j],n),
 n);
 return(hash);
 default:
 errswitch();
 return(0);}}



long href_index(LISP table,LISP key)

{long index;
 if NTYPEP(table,tc_lisp_array) err("not a hash table",table);
 index = c_sxhash(key,table->storage_as.lisp_array.dim);
 if ((index < 0) || (index >= table->storage_as.lisp_array.dim))
 {err("sxhash inconsistency",table);
 return(0);}
 else
 return(index);}

LISP href(LISP table,LISP key)
{return(cdr(assoc(key,
 table->storage_as.lisp_array.data[href_index(table,key)])));}

LISP hset(LISP table,LISP key,LISP value)
{long index;
 LISP cell,l;
 index = href_index(table,key);
 l = table->storage_as.lisp_array.data[index];
 if NNULLP(cell = assoc(key,l))
 return(setcdr(cell,value));
 cell = cons(key,value);
 table->storage_as.lisp_array.data[index] = cons(cell,l);
 return(value);}

LISP assoc(LISP x,LISP alist)
{LISP l,tmp;
 for(l=alist;CONSP(l);l=CDR(l))
 {tmp = CAR(l);
 if (CONSP(tmp) && equal(CAR(tmp),x)) return(tmp);
 INTERRUPT_CHECK();}
 if EQ(l,NIL) return(NIL);
 return(err("improper list to assoc",alist));}

void put_long(long i,FILE *f)
{fwrite(&i,sizeof(long),1,f);}

long get_long(FILE *f)
{long i;
 fread(&i,sizeof(long),1,f);
 return(i);}



long fast_print_table(LISP obj,LISP table)

{FILE *f;
 LISP ht,index;
 f = get_c_file(car(table),(FILE *) NULL);
 if NULLP(ht = car(cdr(table)))
 return(1);
 index = href(ht,obj);
 if NNULLP(index)
 {putc(FO_fetch,f);
 put_long(get_c_long(index),f);
 return(0);}
 if NULLP(index = car(cdr(cdr(table))))
 return(1);
 hset(ht,obj,index);
 FLONM(bashnum) = 1.0;
 setcar(cdr(cdr(table)),plus(index,bashnum));
 putc(FO_store,f);
 put_long(get_c_long(index),f);
 return(1);}

LISP fast_print(LISP obj,LISP table)
{FILE *f;
 long len;
 LISP tmp;
 struct user_type_hooks *p;
 STACK_CHECK(&obj);
 f = get_c_file(car(table),(FILE *) NULL);
 switch(TYPE(obj))
 {case tc_nil:
 putc(tc_nil,f);
 return(NIL);
 case tc_cons:
 for(len=0,tmp=obj;CONSP(tmp);tmp=CDR(tmp)) {INTERRUPT_CHECK();++len;}
 if (len == 1)
 {putc(tc_cons,f);
 fast_print(car(obj),table);
 fast_print(cdr(obj),table);}
 else if NULLP(tmp)
 {putc(FO_list,f);
 put_long(len,f);
 for(tmp=obj;CONSP(tmp);tmp=CDR(tmp))
 fast_print(CAR(tmp),table);}
 else
 {putc(FO_listd,f);
 put_long(len,f);
 for(tmp=obj;CONSP(tmp);tmp=CDR(tmp))
 fast_print(CAR(tmp),table);
 fast_print(tmp,table);}
 return(NIL);
 case tc_flonum:
 putc(tc_flonum,f);
 fwrite(&obj->storage_as.flonum.data,
 sizeof(obj->storage_as.flonum.data),
 1,
 f);
 return(NIL);
 case tc_symbol:
 if (fast_print_table(obj,table))
 {putc(tc_symbol,f);
 len = strlen(PNAME(obj));
 if (len >= TKBUFFERN)
 err("symbol name too long",obj);
 put_long(len,f);
 fwrite(PNAME(obj),len,1,f);
 return(truth);}
 else
 return(NIL);
 default:
 p = get_user_type_hooks(TYPE(obj));
 if (p->fast_print)
 return((*p->fast_print)(obj,table));
 else
 return(err("cannot fast-print",obj));}}

LISP fast_read(LISP table)
{FILE *f;
 LISP tmp,l;
 struct user_type_hooks *p;
 int c;
 long len;
 f = get_c_file(car(table),(FILE *) NULL);
 c = getc(f);
 if (c == EOF) return(table);
 switch(c)
 {case FO_fetch:
 len = get_long(f);
 FLONM(bashnum) = len;
 return(href(car(cdr(table)),bashnum));
 case FO_store:
 len = get_long(f);
 tmp = fast_read(table);
 hset(car(cdr(table)),flocons(len),tmp);
 return(tmp);
 case tc_nil:
 return(NIL);
 case tc_cons:
 tmp = fast_read(table);
 return(cons(tmp,fast_read(table)));
 case FO_list:
 case FO_listd:
 len = get_long(f);
 FLONM(bashnum) = len;
 l = make_list(bashnum,NIL);
 tmp = l;
 while(len > 1)
 {CAR(tmp) = fast_read(table);
 tmp = CDR(tmp);
 --len;}
 CAR(tmp) = fast_read(table);
 if (c == FO_listd)
 CDR(tmp) = fast_read(table);
 return(l);
 case tc_flonum:
 tmp = newcell(tc_flonum);
 fread(&tmp->storage_as.flonum.data,
 sizeof(tmp->storage_as.flonum.data),
 1,
 f);
 return(tmp);
 case tc_symbol:
 len = get_long(f);
 if (len >= TKBUFFERN)
 err("symbol name too long",NIL);
 fread(tkbuffer,len,1,f);
 tkbuffer[len] = 0;
 return(rintern(tkbuffer));
 default:
 p = get_user_type_hooks(c);
 if (p->fast_read)
 return(*p->fast_read)(c,table);
 else
 return(err("unknown fast-read opcode",flocons(c)));}}

LISP array_fast_print(LISP ptr,LISP table)
{int j,len;
 FILE *f;
 f = get_c_file(car(table),(FILE *) NULL);
 switch (ptr->type)
 {case tc_string:
 putc(tc_string,f);
 len = ptr->storage_as.string.dim;
 put_long(len,f);
 fwrite(ptr->storage_as.string.data,len,1,f);
 return(NIL);
 case tc_double_array:
 putc(tc_double_array,f);
 len = ptr->storage_as.double_array.dim * sizeof(double);
 put_long(len,f);
 fwrite(ptr->storage_as.double_array.data,len,1,f);
 return(NIL);
 case tc_long_array:
 putc(tc_long_array,f);
 len = ptr->storage_as.long_array.dim * sizeof(long);
 put_long(len,f);
 fwrite(ptr->storage_as.long_array.data,len,1,f);
 return(NIL);
 case tc_lisp_array:
 putc(tc_lisp_array,f);
 len = ptr->storage_as.lisp_array.dim;
 put_long(len,f);
 for(j=0; j < len; ++j)
 fast_print(ptr->storage_as.lisp_array.data[j],table);
 return(NIL);


 default:
 return(errswitch());}}


LISP array_fast_read(int code,LISP table)
{long j,len,iflag;
 FILE *f;
 LISP ptr;
 f = get_c_file(car(table),(FILE *) NULL);
 switch (code)
 {case tc_string:
 len = get_long(f);
 ptr = strcons(len,NULL);
 fread(ptr->storage_as.string.data,len,1,f);
 ptr->storage_as.string.data[len] = 0;
 return(ptr);
 case tc_double_array:
 len = get_long(f);
 iflag = no_interrupt(1);
 ptr = newcell(tc_double_array);
 ptr->storage_as.double_array.dim = len;
 ptr->storage_as.double_array.data =
 (double *) must_malloc(len * sizeof(double));
 fread(ptr->storage_as.double_array.data,sizeof(double),len,f);
 no_interrupt(iflag);
 return(ptr);
 case tc_long_array:
 len = get_long(f);
 iflag = no_interrupt(1);
 ptr = newcell(tc_long_array);
 ptr->storage_as.long_array.dim = len;
 ptr->storage_as.long_array.data =
 (long *) must_malloc(len * sizeof(long));
 fread(ptr->storage_as.long_array.data,sizeof(long),len,f);
 no_interrupt(iflag);
 return(ptr);
 case tc_lisp_array:
 len = get_long(f);
 FLONM(bashnum) = len;
 ptr = cons_array(bashnum,NIL);
 for(j=0; j < len; ++j)
 ptr->storage_as.lisp_array.data[j] = fast_read(table);
 return(ptr);


 default:
 return(errswitch());}}


long get_c_long(LISP x)
{if NFLONUMP(x) err("not a number",x);
 return((long)FLONM(x));}

LISP make_list(LISP x,LISP v)
{long n;
 LISP l;
 n = get_c_long(x);
 l = NIL;
 while(n > 0)
 {l = cons(v,l); --n;}
 return(l);}

LISP lfread(LISP size,LISP file)
{long flag,n,ret,m;
 char *buffer;
 LISP s;
 FILE *f;
 f = get_c_file(file,NULL);
 flag = no_interrupt(1);
 if TYPEP(size,tc_string)
 {s = size;
 buffer = s->storage_as.string.data;
 n = s->storage_as.string.dim;
 m = 0;}
 else
 {n = get_c_long(size);
 buffer = (char *) must_malloc(n+1);
 buffer[n] = 0;
 m = 1;}
 ret = fread(buffer,1,n,f);
 if (ret == 0)
 {if (m)
 free(buffer);
 no_interrupt(flag);
 return(NIL);}
 if (m)
 {if (ret == n)
 {s = cons(NIL,NIL);
 s->type = tc_string;
 s->storage_as.string.data = buffer;
 s->storage_as.string.dim = n;}
 else
 {s = strcons(ret,NULL);
 memcpy(s->storage_as.string.data,buffer,ret);
 free(buffer);}
 no_interrupt(flag);
 return(s);}
 no_interrupt(flag);
 return(flocons((double)ret));}

LISP lfwrite(LISP string,LISP file)
{FILE *f;
 long flag;
 char *data;
 long dim;
 f = get_c_file(file,NULL);
 if NTYPEP(string,tc_string) err("not a string",string);
 data = string->storage_as.string.data;
 dim = string->storage_as.string.dim;
 flag = no_interrupt(1);
 fwrite(data,dim,1,f);


 no_interrupt(flag);
 return(NIL);}



LISP string_length(LISP string)
{if NTYPEP(string,tc_string) err("not a string",string);
 return(flocons((double)string->storage_as.string.dim));}

LISP llength(LISP obj)
{LISP l;
 long n;
 switch TYPE(obj)
 {case tc_string:
 return(flocons(obj->storage_as.string.dim));
 case tc_double_array:
 return(flocons(obj->storage_as.double_array.dim));
 case tc_long_array:
 return(flocons(obj->storage_as.long_array.dim));
 case tc_lisp_array:
 return(flocons(obj->storage_as.lisp_array.dim));
 case tc_nil:
 return(flocons(0.0));
 case tc_cons:
 for(l=obj,n=0;CONSP(l);l=CDR(l),++n) INTERRUPT_CHECK();
 if NNULLP(l) err("improper list to length",obj);
 return(flocons(n));
 default:
 return(err("wta to length",obj));}}

LISP number2string(LISP x,LISP b)
{char buffer[100];
 double y;
 long base;
 if NFLONUMP(x) err("wta",x);
 y = FLONM(x);
 if NULLP(b)
 sprintf(buffer,"%g",y);
 else if ((base = get_c_long(b)) == 10)
 sprintf(buffer,"%ld",(long)y);
 else if (base == 8)
 sprintf(buffer,"%lo",(long)y);
 else if (base == 16)
 sprintf(buffer,"%lX",(long)y);
 else
 err("number base not handled",b);
 return(strcons(strlen(buffer),buffer));}

LISP string2number(LISP x,LISP b)
{char *str;
 long base,value = 0;
 double result;
 str = get_c_string(x);
 if NULLP(b)
 result = atof(str);
 else if ((base = get_c_long(b)) == 10)
 {sscanf(str,"%ld",&value);
 result = (double) value;}
 else if (base == 8)
 {sscanf(str,"%lo",&value);
 result = (double) value;}
 else if (base == 16)
 {sscanf(str,"%lx",&value);
 result = (double) value;}
 else
 err("number base not handled",b);
 return(flocons(result));}

void init_subrs_a(void)
{init_subr_2("aref",aref1);
 init_subr_3("aset",aset1);
 init_lsubr("string-append",string_append);
 init_subr_1("string-length",string_length);
 init_subr_1("read-from-string",read_from_string);
 init_subr_2("cons-array",cons_array);
 init_subr_2("sxhash",sxhash);
 init_subr_2("equal?",equal);
 init_subr_2("href",href);
 init_subr_3("hset",hset);
 init_subr_2("assoc",assoc);
 init_subr_1("fast-read",fast_read);
 init_subr_2("fast-print",fast_print);
 init_subr_2("make-list",make_list);
 init_subr_2("fread",lfread);
 init_subr_2("fwrite",lfwrite);
 init_subr_1("length",llength);
 init_subr_2("number->string",number2string);
 init_subr_2("string->number",string2number);
 init_subr_3("substring",substring);
 init_subr_2("string-search",string_search);
 init_subr_1("string-trim",string_trim);
 init_subr_1("string-trim-left",string_trim_left);
 init_subr_1("string-trim-right",string_trim_right);
 init_subr_1("string-upcase",string_upcase);
 init_subr_1("string-downcase",string_downcase);}
