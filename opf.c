#include <alloca.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h> /* NULL */
#include <string.h>

#include "m.h"
#include "ob.h"
#include "s.h"
#include "itp.h"
#include "nm.h"
#include "st.h"
#include "di.h"
#include "op.h"
#include "f.h"

void Sfile (context *ctx, object fn, object mode) {
    object f;
    f = fileopen(ctx->lo, charstr(ctx, fn), charstr(ctx, mode));
    push(ctx->lo, ctx->os, f);
}

void initopf (context *ctx, object sd) {
    oper *optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB));
    object n,op;

    op = consoper(ctx, "file", Sfile, 1, 2,
        stringtype, stringtype); INSTALL;

    /* dumpdic(ctx->gl, sd); fflush(NULL);
    bdcput(ctx, sd, consname(ctx, "mark"), mark); */

}


