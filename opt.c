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

void Acvx(context *ctx, object O){
    push(ctx->lo, ctx->os, cvx(O));
}

void Acvlit(context *ctx, object O){
    push(ctx->lo, ctx->os, cvlit(O));
}

void initopt(context *ctx, object sd) {
    oper *optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB));
    object n,op;

    op = consoper(ctx, "cvx", Acvx, 1, 1, anytype); INSTALL;
    op = consoper(ctx, "cvlit", Acvlit, 1, 1, anytype); INSTALL;
    /*
    op = consoper(ctx, "loop", Ploop, 0, 1, proctype); INSTALL;
    op = consoper(ctx, "eq", Aeq, 1, 2, anytype, anytype); INSTALL;
    //dumpdic(ctx->gl, sd); fflush(NULL);
    bdcput(ctx, sd, consname(ctx, "mark"), mark);
    */

}


