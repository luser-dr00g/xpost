#include <alloca.h>
#include <assert.h>
#include <stdbool.h>
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


