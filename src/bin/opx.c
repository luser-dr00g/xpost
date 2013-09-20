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

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h> /* NULL strtod */
#include <string.h>
//#include "config.h"
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#else
#include <time.h>
#endif

#include "m.h"
#include "ob.h"
#include "s.h"
#include "itp.h"
#include "nm.h"
#include "st.h"
#include "ar.h"
#include "di.h"
#include "op.h"
#include "opdi.h"
#include "opx.h"

object bind (context *ctx, object p) {
    object t, d;
    int i, j, z;
    for (i = 0; i < p.comp_.sz; i++) {
        t = barget(ctx, p, i);
        switch(type(t)){
        case nametype:
            z = count(ctx->lo, ctx->ds);
            for (j = 0; j < z; j++) {
                d = top(ctx->lo, ctx->ds, j);
                if (dicknown(ctx, bank(ctx,d), d, t)) {
                    t = bdcget(ctx, d, t);
                    if (type(t) == operatortype) {
                        barput(ctx, p, i, t);
                    }
                    break;
                }
            }
            break;
        case arraytype:
            if (isx(t)) {
                t = bind(ctx, t);
                barput(ctx, p, i, t);
            }
        }
    }
    return setfaccess(p, readonly);
}

void Pbind (context *ctx, object P) {
    push(ctx->lo, ctx->os, bind(ctx, P));
}

void realtime (context *ctx) {
    double sec;
    #ifdef HAVE_GETTIMEOFDAY
        struct timeval tv;
        gettimeofday(&tv, NULL);
        sec = tv.tv_sec * 1000 + tv.tv_usec / 1000.0;
    #else
        sec = time(NULL) * 1000;
    #endif
    push(ctx->lo, ctx->os, consint(sec));
}

void traceon (context *ctx) {
    (void)ctx;
    TRACE = 1;
}
void traceoff (context *ctx) {
    (void)ctx;
    TRACE = 0;
}

void debugloadon (context *ctx) {
    (void)ctx;
    DEBUGLOAD = 1;
}
void debugloadoff (context *ctx) {
    (void)ctx;
    DEBUGLOAD = 0;
}

void Odumpnames (context *ctx) {
    printf("\nGlobal Name stack: ");
    dumpstack(ctx->gl, adrent(ctx->gl, NAMES));
    (void)puts("");
    printf("\nLocal Name stack: ");
    dumpstack(ctx->lo, adrent(ctx->lo, NAMES));
    (void)puts("");
}

void dumpvm (context *ctx) {
    dumpmfile(ctx->lo);
    dumpmtab(ctx->lo, 0);
    dumpmfile(ctx->gl);
    dumpmtab(ctx->gl, 0);
}

void initopx(context *ctx, object sd) {
    oper *optab;
    object n,op;
    assert(ctx->gl->base);
    optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB));

    op = consoper(ctx, "bind", Pbind, 1, 1, proctype); INSTALL;
    bdcput(ctx, sd, consname(ctx, "null"), null);
    //version: see init.ps
    op = consoper(ctx, "realtime", realtime, 1, 0); INSTALL;
    //usertime
    //languagelevel
    //product: see init.ps (Xpost3)
    //revision
    //serialnumber
    //executive: see init.ps
    //echo: see opf.c
    //prompt: see init.ps

    op = consoper(ctx, "traceon", traceon, 0, 0); INSTALL;
    op = consoper(ctx, "traceoff", traceoff, 0, 0); INSTALL;
    op = consoper(ctx, "debugloadon", debugloadon, 0, 0); INSTALL;
    op = consoper(ctx, "debugloadoff", debugloadoff, 0, 0); INSTALL;
    op = consoper(ctx, "dumpnames", Odumpnames, 0, 0); INSTALL;
    op = consoper(ctx, "dumpvm", dumpvm, 0, 0); INSTALL;

    /* dumpdic(ctx->gl, sd); fflush(NULL);
    bdcput(ctx, sd, consname(ctx, "mark"), mark); */

}


