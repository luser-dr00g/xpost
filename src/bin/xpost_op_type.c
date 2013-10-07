#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef STDC_HEADERS
# include <stdlib.h>
# include <stddef.h>
#else
# ifdef HAVE_STDLIB_H
#  include <stdlib.h>
# endif
#endif
#ifdef HAVE_ALLOCA_H
# include <alloca.h>
#elif !defined alloca
# ifdef __GNUC__
#  define alloca __builtin_alloca
# elif defined _AIX
#  define alloca __alloca
# elif defined _MSC_VER
#  include <malloc.h>
#  define alloca _alloca
# elif !defined HAVE_ALLOCA
#  ifdef  __cplusplus
extern "C"
#  endif
void *alloca (size_t);
# endif
#endif

#ifdef HAVE_STDBOOL_H
# include <stdbool.h>
#else
# ifndef HAVE__BOOL
#  ifdef __cplusplus
typedef bool _Bool;
#  else
#   define _Bool signed char
#  endif
# endif
# define bool _Bool
# define false 0
# define true 1
# define __bool_true_false_are_defined 1
#endif

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h> /* NULL strtod */
#include <string.h>

#include "xpost_memory.h"
#include "xpost_object.h"
#include "xpost_stack.h"
#include "xpost_interpreter.h"
#include "xpost_error.h"
#include "xpost_name.h"
#include "xpost_string.h"
#include "xpost_dict.h"
#include "xpost_operator.h"
#include "xpost_op_type.h"

static
void Atype(context *ctx,
           object o)
{
    push(ctx->lo, ctx->os, cvx(consname(ctx, types[type(o)])));
}

static
void Acvlit(context *ctx,
            object o)
{
    push(ctx->lo, ctx->os, cvlit(o));
}

static
void Acvx(context *ctx,
          object o)
{
    push(ctx->lo, ctx->os, cvx(o));
}

static
void Axcheck(context *ctx,
             object o)
{
    push(ctx->lo, ctx->os, consbool(isx(o)));
}

static
void Aexecuteonly(context *ctx,
                  object o)
{
    o.tag &= ~FACCESS;
    o.tag |= (executeonly << FACCESSO);
    push(ctx->lo, ctx->os, o);
}

static
void Anoaccess(context *ctx,
               object o)
{
    o.tag &= ~FACCESS;
    o.tag |= (noaccess << FACCESSO);
    push(ctx->lo, ctx->os, o);
}

static
void Areadonly(context *ctx,
               object o)
{
    o.tag &= ~FACCESS;
    o.tag |= (readonly << FACCESSO);
    push(ctx->lo, ctx->os, o);
}

static
void Archeck(context *ctx,
             object o)
{
    push(ctx->lo, ctx->os, consbool( (o.tag & FACCESS) >> FACCESSO >= readonly ));
}

static
void Awcheck(context *ctx,
             object o)
{
    push(ctx->lo, ctx->os, consbool( (o.tag & FACCESS) >> FACCESSO == unlimited ));
}

static
void Ncvi(context *ctx,
          object n)
{
    if (type(n) == realtype)
        n = consint(n.real_.val);
    push(ctx->lo, ctx->os, n);
}

static
void Scvi(context *ctx,
          object s)
{
    double dbl;
    long num;
    char *t = alloca(s.comp_.sz+1);
    memcpy(t, charstr(ctx, s), s.comp_.sz);
    t[s.comp_.sz] = '\0';

    dbl = strtod(t, NULL);
    if ((dbl == HUGE_VAL || dbl -HUGE_VAL) && errno==ERANGE)
        error(limitcheck, "Scvr");
    if (dbl >= LONG_MAX || dbl <= LONG_MIN)
        error(limitcheck, "Scvi");
    num = dbl;

    /*
    num = strtol(t, NULL, 10);
    if ((num == LONG_MAX || num == LONG_MIN) && errno==ERANGE)
        error(limitcheck, "Scvi");
    */

    push(ctx->lo, ctx->os, consint(num));
}

static
void Scvn(context *ctx,
          object s)
{
    char *t = alloca(s.comp_.sz+1);
    memcpy(t, charstr(ctx, s), s.comp_.sz);
    t[s.comp_.sz] = '\0';
    push(ctx->lo, ctx->os, consname(ctx, t));
}

static
void Ncvr(context *ctx,
          object n)
{
    if (type(n) == integertype)
        n = consreal(n.int_.val);
    push(ctx->lo, ctx->os, n);
}

static
void Scvr(context *ctx,
          object str)
{
    double num;
    char *s = alloca(str.comp_.sz + 1);
    memcpy(s, charstr(ctx, str), str.comp_.sz);
    s[str.comp_.sz] = '\0';
    num = strtod(s, NULL);
    if ((num == HUGE_VAL || num -HUGE_VAL) && errno==ERANGE)
        error(limitcheck, "Scvr");
    push(ctx->lo, ctx->os, consreal(num));
}

static
int conv_rad(int num,
             int rad,
             char *s,
             int n)
{
    char *vec = "0123456789" "ABCDEFGHIJKLM" "NOPQRSTUVWXYZ";
    int off;
    if (n == 0) return 0;
    if (num < rad) {
        *s = vec[num];
        return 1;
    }
    off = conv_rad(num/rad, rad, s, n);
    if ((off == n) || (off == -1)) return -1;
    s[off] = vec[num%rad];
    return off+1;
}

static
void NRScvrs (context *ctx,
              object num,
              object rad,
              object str)
{
    int r, n;
    if (type(num) == realtype) num = consint(num.real_.val);
    r = rad.int_.val;
    if (r < 2 || r > 36) error(rangecheck, "NRScvrs");
    n = conv_rad(num.int_.val, r, charstr(ctx, str), str.comp_.sz);
    if (n == -1) error(rangecheck, "NRScvrs");
    if (n < str.comp_.sz) str.comp_.sz = n;
    push(ctx->lo, ctx->os, str);
}

static
int conv_integ(real num,
               char *s,
               int n)
{
    int off;
    if (num < 10.0) {
        *s = ((int)num) + '0';
        return 1;
    }
    off = conv_integ(num/10.0, s, n);
    if ((off == n) || (off == -1)) return -1;
    s[off] = (((int)num)%10) + '0';
    return off + 1;
}

static
int conv_frac (real num,
               char *s,
               int n)
{
    real integ, frac;
    num *= 10.0;
    integ = floor(num);
    frac = num - integ;
    *s = (int)integ + '0';
    //if (num == 0.0) return 1;
    if (num < 0.0001) return 1;
    return 1 + conv_frac(frac, s+1, n-1);
}

static
int conv_real (real num,
               char *s,
               int n)
{
    int off = 0;
    real integ, frac;
    if (n == 0) return -1;
    if (isinf(num)){
        if (n < 3) return -1;
        memcpy(s, "inf", 3);
        return 3;
    }
    if (isnan(num)){
        if (n < 3) return -1;
        memcpy(s, "nan", 3);
        return 3;
    }
    if (num < 0) {
        num = fabs(num);
        s[off++] = '-';
    }
    integ = floor(num);
    frac = num - integ;
    off += conv_integ(integ, s+off, n);
    s[off++] = '.';
    off += conv_frac(frac, s+off, n-off);
    return off;
}

static
void AScvs (context *ctx,
            object any,
            object str)
{
    char nostringval[] = "-nostringval-";
    char strue[] = "true";
    char sfalse[] = "false";
    char smark[] = "-mark-";
    int n;

    switch(type(any)) {
    default:
        if (str.comp_.sz < sizeof(nostringval)-1) error(rangecheck, "AScvs");
        memcpy(charstr(ctx, str), nostringval, sizeof(nostringval)-1);
        str.comp_.sz = sizeof(nostringval)-1;
        break;
    case marktype:
        if (str.comp_.sz < sizeof(smark)-1) error(rangecheck, "AScvs");
        memcpy(charstr(ctx, str), smark, sizeof(smark)-1);
        str.comp_.sz = sizeof(smark)-1;
        break;

    case booleantype:
        {
            if (any.int_.val) {
                if (str.comp_.sz < sizeof(strue)-1) error(rangecheck, "AScvs booleantype case");
                memcpy(charstr(ctx, str), strue, sizeof(strue)-1);
                str.comp_.sz = sizeof(strue)-1;
            } else {
                if (str.comp_.sz < sizeof(sfalse)-1) error(rangecheck, "AScvs booleantype case");
                memcpy(charstr(ctx, str), sfalse, sizeof(sfalse)-1);
                str.comp_.sz = sizeof(sfalse)-1;
            }
        }
        break;
    case integertype:
        {
            //n = conv_rad(any.int_.val, 10, charstr(ctx, str), str.comp_.sz);
            char *s = charstr(ctx, str);
            int sz = str.comp_.sz;
            n = 0;
            if (any.int_.val < 0) {
                s[n++] = '-';
                any.int_.val = abs(any.int_.val);
                --sz;
            }
            n += conv_integ(any.int_.val, s + n, sz);
            if (n == -1) error(rangecheck, "AScvs integertype case");
            if (n < str.comp_.sz) str.comp_.sz = n;
            break;
        }
    case realtype:
        n = conv_real(any.real_.val, charstr(ctx, str), str.comp_.sz);
        if (n == -1) error(rangecheck, "AScvs realtype case");
        if (n < str.comp_.sz) str.comp_.sz = n;
        break;

    case operatortype:
        {
            oper *optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB));
            oper op = optab[any.mark_.padw];
            mark_ nm = { nametype | FBANK, 0, op.name };
            any.mark_ = nm;
        }
        /*@fallthrough@*/
    case nametype:
        any = strname(ctx, any);
        /*@fallthrough@*/
    case stringtype:
        if (any.comp_.sz > str.comp_.sz) error(rangecheck, "AScvs stringtype case");
        if (any.comp_.sz < str.comp_.sz) str.comp_.sz = any.comp_.sz;
        memcpy(charstr(ctx, str), charstr(ctx, any), any.comp_.sz);
        break;
    }

    push(ctx->lo, ctx->os, str);
}

void initopt (context *ctx,
              object sd)
{
    oper *optab;
    object n,op;
    assert(ctx->gl->base);
    optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB));

    op = consoper(ctx, "type", Atype, 1, 1, anytype); INSTALL;
    op = consoper(ctx, "cvlit", Acvlit, 1, 1, anytype); INSTALL;
    op = consoper(ctx, "cvx", Acvx, 1, 1, anytype); INSTALL;
    op = consoper(ctx, "xcheck", Axcheck, 1, 1, anytype); INSTALL;
    op = consoper(ctx, "executeonly", Aexecuteonly, 1, 1, anytype); INSTALL;
    op = consoper(ctx, "noaccess", Anoaccess, 1, 1, anytype); INSTALL;
    op = consoper(ctx, "readonly", Areadonly, 1, 1, anytype); INSTALL;
    op = consoper(ctx, "rcheck", Archeck, 1, 1, anytype); INSTALL;
    op = consoper(ctx, "wcheck", Awcheck, 1, 1, anytype); INSTALL;
    op = consoper(ctx, "cvi", Ncvi, 1, 1, numbertype); INSTALL;
    op = consoper(ctx, "cvi", Scvi, 1, 1, stringtype); INSTALL;
    op = consoper(ctx, "cvn", Scvn, 1, 1, stringtype); INSTALL;
    op = consoper(ctx, "cvr", Ncvr, 1, 1, numbertype); INSTALL;
    op = consoper(ctx, "cvr", Scvr, 1, 1, stringtype); INSTALL;
    op = consoper(ctx, "cvrs", NRScvrs, 1, 3, numbertype, integertype, stringtype); INSTALL;
    op = consoper(ctx, "cvs", AScvs, 1, 2, anytype, stringtype); INSTALL;

    /* dumpdic(ctx->gl, sd); fflush(NULL);
    bdcput(ctx, sd, consname(ctx, "mark"), mark); */
}


