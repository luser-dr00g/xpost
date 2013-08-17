#include <alloca.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h> /* NULL strtod */
#include <string.h>

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
    puts("");
    printf("\nLocal Name stack: ");
    dumpstack(ctx->lo, adrent(ctx->lo, NAMES));
    puts("");
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

    op = consoper(ctx, "traceon", traceon, 0, 0); INSTALL;
    op = consoper(ctx, "traceoff", traceoff, 0, 0); INSTALL;
    op = consoper(ctx, "debugloadon", debugloadon, 0, 0); INSTALL;
    op = consoper(ctx, "debugloadoff", debugloadoff, 0, 0); INSTALL;
    op = consoper(ctx, "dumpnames", Odumpnames, 0, 0); INSTALL;
    op = consoper(ctx, "dumpvm", dumpvm, 0, 0); INSTALL;

    /* dumpdic(ctx->gl, sd); fflush(NULL);
    bdcput(ctx, sd, consname(ctx, "mark"), mark); */

}


