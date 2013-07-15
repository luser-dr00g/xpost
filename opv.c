#include <alloca.h>
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

void Zvmstatus (context *ctx) {
    push(ctx->lo, ctx->os, consint(count(ctx->lo, adrent(ctx->lo, VS))));
    push(ctx->lo, ctx->os, consint(ctx->lo->used));
    push(ctx->lo, ctx->os, consint(ctx->lo->max));
}

void initopv(context *ctx, object sd) {
    oper *optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB));
    object n,op;

    op = consoper(ctx, "save", Zsave, 1, 0); INSTALL;
    op = consoper(ctx, "restore", Vrestore, 0, 1, savetype); INSTALL;
    op = consoper(ctx, "vmstatus", Zvmstatus, 3, 0); INSTALL;

    /* dumpdic(ctx->gl, sd); fflush(NULL);
    bdcput(ctx, sd, consname(ctx, "mark"), mark); */

}


