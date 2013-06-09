#include <stdlib.h> /* NULL */
#include <alloca.h>
#include <stdbool.h>

#include "m.h"
#include "ob.h"
#include "s.h"
#include "itp.h"
#include "nm.h"
#include "di.h"
#include "op.h"

void Aeq (context *ctx, object x, object y) {
    push(ctx->lo, ctx->os, consbool(objcmp(ctx,x,y) == 0));
}

void Ane (context *ctx, object x, object y) {
    push(ctx->lo, ctx->os, consbool(objcmp(ctx,x,y) != 0));
}

void Age (context *ctx, object x, object y) {
    push(ctx->lo, ctx->os, consbool(objcmp(ctx,x,y) >= 0));
}

void Agt (context *ctx, object x, object y) {
    push(ctx->lo, ctx->os, consbool(objcmp(ctx,x,y) > 0));
}

void Ale (context *ctx, object x, object y) {
    push(ctx->lo, ctx->os, consbool(objcmp(ctx,x,y) <= 0));
}

void Alt (context *ctx, object x, object y) {
    push(ctx->lo, ctx->os, consbool(objcmp(ctx,x,y) < 0));
}

void Bnot (context *ctx, object x) {
    push(ctx->lo, ctx->os, consbool( ! (bool) x.int_.val));
}

void Inot (context *ctx, object x) {
    push(ctx->lo, ctx->os, consint( ! x.int_.val));
}


void initopb(context *ctx, object sd) {
    oper *optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB));
    object n,op;


    op = consoper(ctx, "eq", Aeq, 1, 2, anytype, anytype); INSTALL;
    op = consoper(ctx, "ne", Ane, 1, 2, anytype, anytype); INSTALL;
    op = consoper(ctx, "ge", Age, 1, 2, anytype, anytype); INSTALL;
    op = consoper(ctx, "gt", Agt, 1, 2, anytype, anytype); INSTALL;
    op = consoper(ctx, "le", Ale, 1, 2, anytype, anytype); INSTALL;
    op = consoper(ctx, "lt", Alt, 1, 2, anytype, anytype); INSTALL;
    op = consoper(ctx, "not", Bnot, 1, 1, booleantype); INSTALL;
    op = consoper(ctx, "not", Inot, 1, 1, integertype); INSTALL;

    /* dumpdic(ctx->gl, sd); fflush(NULL); */
    bdcput(ctx, sd, consname(ctx, "true"), consbool(true));
    bdcput(ctx, sd, consname(ctx, "false"), consbool(false));
}
