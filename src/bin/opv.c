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
#include <stdio.h>
#include <stdlib.h> /* NULL strtod */
#include <string.h>

#include "m.h"
#include "ob.h"
#include "s.h"
#include "v.h"
#include "itp.h"
#include "nm.h"
#include "st.h"
#include "di.h"
#include "op.h"
#include "opv.h"

void Zsave (context *ctx) {
    push(ctx->lo, ctx->os, save(ctx->lo));
}

void Vrestore (context *ctx, object V) {
    int z = count(ctx->lo, adrent(ctx->lo, VS));
    while(z > V.save_.lev) {
        restore(ctx->lo);
        z--;
    }
}

void Bsetglobal (context *ctx, object B) {
    ctx->vmmode = B.int_.val? GLOBAL: LOCAL;
}

void Zcurrentglobal (context *ctx) {
    push(ctx->lo, ctx->os, consbool(ctx->vmmode==GLOBAL));
}

void Agcheck (context *ctx, object A) {
    object r;
    switch(type(A)) {
    default:
            r = consbool(false); break;
    case stringtype:
    case nametype:
    case dicttype:
    case arraytype:
            r = consbool((A.tag&FBANK)!=0);
    }
    push(ctx->lo, ctx->os, r);
}

void Zvmstatus (context *ctx) {
    push(ctx->lo, ctx->os, consint(count(ctx->lo, adrent(ctx->lo, VS))));
    push(ctx->lo, ctx->os, consint(ctx->lo->used));
    push(ctx->lo, ctx->os, consint(ctx->lo->max));
}

void initopv(context *ctx, object sd) {
    oper *optab;
    object n,op;
    assert(ctx->gl->base);
    optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB));

    op = consoper(ctx, "save", Zsave, 1, 0); INSTALL;
    op = consoper(ctx, "restore", Vrestore, 0, 1, savetype); INSTALL;
    op = consoper(ctx, "setglobal", Bsetglobal, 0, 1, booleantype); INSTALL;
    op = consoper(ctx, "currentglobal", Zcurrentglobal, 1, 0); INSTALL;
    op = consoper(ctx, "gcheck", Agcheck, 1, 1, anytype); INSTALL;
    op = consoper(ctx, "vmstatus", Zvmstatus, 3, 0); INSTALL;

    /* dumpdic(ctx->gl, sd); fflush(NULL);
    bdcput(ctx, sd, consname(ctx, "mark"), mark); */

}


