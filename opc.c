#include <alloca.h>
#include <stdbool.h>
#include <stdio.h> /* printf */
#include <stdlib.h> /* NULL */

#include "m.h"
#include "ob.h"
#include "s.h"
#include "itp.h"
#include "nm.h"
#include "di.h"
#include "op.h"

void BPif(context *ctx, object B, object P) {
    if (B.int_.val)
        push(ctx->lo, ctx->es, P);
}

void Ploop(context *ctx, object P) {
    push(ctx->lo, ctx->es, consoper(ctx, "loop", NULL,0,0));
    push(ctx->lo, ctx->es, consoper(ctx, "cvx", NULL,0,0));
    push(ctx->lo, ctx->es, cvlit(P));
    push(ctx->lo, ctx->es, P);
}

void Zexit(context *ctx) {
    object oploop = consoper(ctx, "loop", NULL,0,0);
    object opforall = consoper(ctx, "forall", NULL,0,0);
    object x;
    printf("\nexit\n");
    dumpstack(ctx->lo, ctx->os);
    dumpstack(ctx->lo, ctx->es);
    printf("\n");
    while(1){
        x = pop(ctx->lo, ctx->es);
        dumpobject(x);
        if ( (objcmp(ctx, oploop, x) == 0)
          || (objcmp(ctx, opforall, x) == 0) )
            break;
    }
    printf("result:");
    dumpstack(ctx->lo, ctx->es);
}

void initopc(context *ctx, object sd) {
    oper *optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB));
    object n,op;

    op = consoper(ctx, "if", BPif, 0, 2, booleantype, proctype); INSTALL;
    op = consoper(ctx, "loop", Ploop, 0, 1, proctype); INSTALL;
    op = consoper(ctx, "exit", Zexit, 0, 0); INSTALL;
    /*
    op = consoper(ctx, "eq", Aeq, 1, 2, anytype, anytype); INSTALL;
    //dumpdic(ctx->gl, sd); fflush(NULL);
    bdcput(ctx, sd, consname(ctx, "mark"), mark);
    */

}


