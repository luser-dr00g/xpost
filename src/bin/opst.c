#ifdef HAVE_CONFIG_H
# include <config.h>
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
#include <stdio.h> /* printf */
#include <stdlib.h> /* NULL */

#include "m.h"
#include "ob.h"
#include "s.h"
#include "itp.h"
#include "err.h"
#include "st.h"
#include "ar.h"
#include "di.h"
#include "op.h"
#include "opst.h"

void Istring(context *ctx, object I) {
    push(ctx->lo, ctx->os, cvlit(consbst(ctx, I.int_.val, NULL)));
}

void Slength(context *ctx, object S) {
    push(ctx->lo, ctx->os, consint(S.comp_.sz));
}

void s_copy(context *ctx, object S, object D) {
    unsigned i;
    for (i = 0; i < S.comp_.sz; i++)
        bstput(ctx, D, i, bstget(ctx, S, i));
}

void Scopy(context *ctx, object S, object D) {
    if (D.comp_.sz < S.comp_.sz) error(rangecheck, "Scopy");
    s_copy(ctx, S, D);
    push(ctx->lo, ctx->os, arrgetinterval(D, 0, S.comp_.sz));
}

void Sget(context *ctx, object S, object I) {
    push(ctx->lo, ctx->os, consint(bstget(ctx, S, I.int_.val)));
}

void Sput(context *ctx, object S, object I, object C) {
    bstput(ctx, S, I.int_.val, C.int_.val);
}

void Sgetinterval(context *ctx, object S, object I, object L) {
    push(ctx->lo, ctx->os, arrgetinterval(S, I.int_.val, L.int_.val));
}

void Sputinterval(context *ctx, object D, object I, object S) {
    s_copy(ctx, S, arrgetinterval(D, I.int_.val, S.comp_.sz));
}

int ancsearch(char *str, char *seek, int seekn) {
    int i;
    for (i = 0; i < seekn; i++)
        if (str[i] != seek[i])
            return false;
    return true;
}

void Sanchorsearch(context *ctx, object str, object seek) {
    char *s, *k;
    if (seek.comp_.sz > str.comp_.sz) error(rangecheck, "Sanchorsearch");
    s = charstr(ctx, str);
    k = charstr(ctx, seek);
    if (ancsearch(s, k, seek.comp_.sz)) {
        push(ctx->lo, ctx->os,
                arrgetinterval(str, seek.comp_.sz, 
                    str.comp_.sz - seek.comp_.sz)); /* post */
        push(ctx->lo, ctx->os,
                arrgetinterval(str, 0, seek.comp_.sz)); /* match */
        push(ctx->lo, ctx->os, consbool(true));
    } else {
        push(ctx->lo, ctx->os, str);
        push(ctx->lo, ctx->os, consbool(false));
    }
}

void Ssearch(context *ctx, object str, object seek) {
    int i;
    char *s, *k;
    if (seek.comp_.sz > str.comp_.sz) error(rangecheck, "Ssearch");
    s = charstr(ctx, str);
    k = charstr(ctx, seek);
    for (i = 0; i <= (str.comp_.sz - seek.comp_.sz); i++) {
        if (ancsearch(s+i, k, seek.comp_.sz)) {
            push(ctx->lo, ctx->os, 
                    arrgetinterval(str, i + seek.comp_.sz,
                        str.comp_.sz - seek.comp_.sz - i)); /* post */
            push(ctx->lo, ctx->os,
                    arrgetinterval(str, i, seek.comp_.sz)); /* match */
            push(ctx->lo, ctx->os,
                    arrgetinterval(str, 0, i)); /* pre */
            push(ctx->lo, ctx->os, consbool(true));
            return;
        }
    }
    push(ctx->lo, ctx->os, str);
    push(ctx->lo, ctx->os, consbool(false));
}

void Sforall(context *ctx, object S, object P) {
    if (S.comp_.sz == 0) return;
    assert(ctx->gl->base);
    push(ctx->lo, ctx->es, consoper(ctx, "forall", NULL,0,0));
    push(ctx->lo, ctx->es, consoper(ctx, "cvx", NULL,0,0));
    push(ctx->lo, ctx->es, cvlit(P));
    push(ctx->lo, ctx->es, cvlit(arrgetinterval(S, 1, S.comp_.sz-1)));
    if (isx(S)) push(ctx->lo, ctx->es, consoper(ctx, "cvx", NULL,0,0));
    push(ctx->lo, ctx->es, P);
    push(ctx->lo, ctx->es, consint(bstget(ctx, S, 0)));
}

// token : see optok.c

void initopst(context *ctx, object sd) {
    oper *optab;
    object n,op;
    assert(ctx->gl->base);
    optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB));
    op = consoper(ctx, "string", Istring, 1, 1,
            integertype); INSTALL;
    op = consoper(ctx, "length", Slength, 1, 1,
            stringtype); INSTALL;
    op = consoper(ctx, "copy", Scopy, 1, 2,
            stringtype, stringtype); INSTALL;
    op = consoper(ctx, "get", Sget, 1, 2,
            stringtype, integertype); INSTALL;
    op = consoper(ctx, "put", Sput, 0, 3,
            stringtype, integertype, integertype); INSTALL;
    op = consoper(ctx, "getinterval", Sgetinterval, 1, 3,
            stringtype, integertype, integertype); INSTALL;
    op = consoper(ctx, "putinterval", Sputinterval, 0, 3,
            stringtype, integertype, stringtype); INSTALL;
    op = consoper(ctx, "anchorsearch", Sanchorsearch, 3, 2,
            stringtype, stringtype); INSTALL;
    op = consoper(ctx, "search", Ssearch, 4, 2,
            stringtype, stringtype); INSTALL;
    op = consoper(ctx, "forall", Sforall, 0, 2,
            stringtype, proctype); INSTALL;
    //bdcput(ctx, sd, consname(ctx, "mark"), mark);
}

