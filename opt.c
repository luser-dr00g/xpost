#include <alloca.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h> /* NULL strtod */
#include <string.h>

#include "m.h"
#include "ob.h"
#include "s.h"
#include "itp.h"
#include "err.h"
#include "nm.h"
#include "st.h"
#include "di.h"
#include "op.h"

void Atype(context *ctx, object o) {
    push(ctx->lo, ctx->os, cvx(consname(ctx, types[type(o)])));
}

void Acvlit(context *ctx, object o){
    push(ctx->lo, ctx->os, cvlit(o));
}

void Acvx(context *ctx, object o){
    push(ctx->lo, ctx->os, cvx(o));
}

void Axcheck(context *ctx, object o) {
    push(ctx->lo, ctx->os, consbool(isx(o)));
}

void Aexecuteonly(context *ctx, object o) {
    o.tag &= ~FACCESS;
    o.tag |= (executeonly << FACCESSO);
    push(ctx->lo, ctx->os, o);
}

void Anoaccess(context *ctx, object o) {
    o.tag &= ~FACCESS;
    o.tag |= (noaccess << FACCESSO);
    push(ctx->lo, ctx->os, o);
}

void Areadonly(context *ctx, object o) {
    o.tag &= ~FACCESS;
    o.tag |= (readonly << FACCESSO);
    push(ctx->lo, ctx->os, o);
}

void Archeck(context *ctx, object o) {
    push(ctx->lo, ctx->os, consbool( (o.tag & FACCESS) >> FACCESSO >= readonly ));
}

void Awcheck(context *ctx, object o) {
    push(ctx->lo, ctx->os, consbool( (o.tag & FACCESS) >> FACCESSO == unlimited ));
}

void Ncvi(context *ctx, object n) {
    if (type(n) == realtype)
        n = consint(n.real_.val);
    push(ctx->lo, ctx->os, n);
}

void Scvi(context *ctx, object s) {
    long num;
    char *t = alloca(s.comp_.sz+1);
    memcpy(t, charstr(ctx, s), s.comp_.sz);
    t[s.comp_.sz] = '\0';
    num = strtol(t, NULL, 10);
    if ((num == LONG_MAX || num == LONG_MIN) && errno==ERANGE)
        error(limitcheck, "Scvi");
    push(ctx->lo, ctx->os, consint(num));
}

void Scvn(context *ctx, object s) {
    char *t = alloca(s.comp_.sz+1);
    memcpy(t, charstr(ctx, s), s.comp_.sz);
    t[s.comp_.sz] = '\0';
    push(ctx->lo, ctx->os, consname(ctx, t));
}

void Ncvr(context *ctx, object n) {
    if (type(n) == integertype)
        n = consreal(n.int_.val);
    push(ctx->lo, ctx->os, n);
}

void Scvr(context *ctx, object str) {
    double num;
    char *s = alloca(str.comp_.sz + 1);
    memcpy(s, charstr(ctx, str), str.comp_.sz);
    s[str.comp_.sz] = '\0';
    num = strtod(s, NULL);
    if ((num == HUGE_VAL || num -HUGE_VAL) && errno==ERANGE)
        error(limitcheck, "Scvr");
    push(ctx->lo, ctx->os, consreal(num));
}

int conv_rad(int num, int rad, char *s, int n) {
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

void NRScvrs(context *ctx, object num, object rad, object str) {
    int r, n;
    if (type(num) == realtype) num = consint(num.real_.val);
    r = rad.int_.val;
    if (r < 2 || r > 36) error(rangecheck, "NRScvrs");
    n = conv_rad(num.int_.val, r, charstr(ctx, str), str.comp_.sz);
    if (n == -1) error(rangecheck, "NRScvrs");
    if (n < str.comp_.sz) str.comp_.sz = n;
    push(ctx->lo, ctx->os, str);
}

int conv_integ(real num, char *s, int n) {
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

int conv_frac(real num, char *s, int n) {
    real integ, frac;
    num *= 10.0;
    integ = floor(num);
    frac = num - integ;
    *s = (int)integ + '0';
    if (num == 0.0) return 1;
    return 1 + conv_frac(frac, s+1, n-1);
}

int conv_real(real num, char *s, int n) {
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

void AScvs(context *ctx, object any, object str) {
    char nostringval[] = "-nostringval-";
    char strue[] = "true";
    char sfalse[] = "false";
    int n;

    switch(type(any)) {
    default:
        if (str.comp_.sz < sizeof(nostringval)-1) error(rangecheck, "AScvs");
        memcpy(charstr(ctx, str), nostringval, sizeof(nostringval)-1);
        str.comp_.sz = sizeof(nostringval)-1;
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
        n = conv_rad(any.int_.val, 10, charstr(ctx, str), str.comp_.sz);
        if (n == -1) error(rangecheck, "AScvs integertype case");
        if (n < str.comp_.sz) str.comp_.sz = n;
        break;
    case realtype:
        n = conv_real(any.real_.val, charstr(ctx, str), str.comp_.sz);
        if (n == -1) error(rangecheck, "AScvs realtype case");
        if (n < str.comp_.sz) str.comp_.sz = n;
        break;

    case operatortype:
        {
            oper *optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB));
            oper op = optab[any.mark_.padw];
            mark_ nm = { nametype, 0, op.name };
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

void initopt(context *ctx, object sd) {
    oper *optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB));
    object n,op;

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


