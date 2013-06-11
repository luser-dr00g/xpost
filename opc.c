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

void Aexec (context *ctx, object O) {
    push(ctx->lo, ctx->es, O);
}

void BPif (context *ctx, object B, object P) {
    if (B.int_.val)
        push(ctx->lo, ctx->es, P);
}

void BPPifelse (context *ctx, object B, object Then, object Else) {
    if (B.int_.val)
        push(ctx->lo, ctx->es, Then);
    else
        push(ctx->lo, ctx->es, Else);
}

void IIIPfor (context *ctx, object init, object incr, object lim, object P) {
    integer i = init.int_.val;
    integer j = incr.int_.val;
    integer n = lim.int_.val;
    bool up = j > 0;
    if (up? i > n : i < n) return;
    push(ctx->lo, ctx->es, consoper(ctx, "for", NULL,0,0));
    push(ctx->lo, ctx->es, consoper(ctx, "cvx", NULL,0,0));
    push(ctx->lo, ctx->es, cvlit(P));
    push(ctx->lo, ctx->es, lim);
    push(ctx->lo, ctx->es, incr);
    push(ctx->lo, ctx->es, consint(i + j));
    push(ctx->lo, ctx->es, P);
    push(ctx->lo, ctx->es, init);
}

void RRRPfor (context *ctx, object init, object incr, object lim, object P) {
    real i = init.real_.val;
    real j = incr.real_.val;
    real n = lim.real_.val;
    bool up = j > 0;
    if (up? i > n : i < n) return;
    push(ctx->lo, ctx->es, consoper(ctx, "for", NULL,0,0));
    push(ctx->lo, ctx->es, consoper(ctx, "cvx", NULL,0,0));
    push(ctx->lo, ctx->es, cvlit(P));
    push(ctx->lo, ctx->es, lim);
    push(ctx->lo, ctx->es, incr);
    push(ctx->lo, ctx->es, consreal(i + j));
    push(ctx->lo, ctx->es, P);
    push(ctx->lo, ctx->es, init);
}

void IPrepeat (context *ctx, object n, object P) {
    if (n.int_.val <= 0) return;
    push(ctx->lo, ctx->es, consoper(ctx, "repeat", NULL,0,0));
    push(ctx->lo, ctx->es, consoper(ctx, "cvx", NULL,0,0));
    push(ctx->lo, ctx->es, cvlit(P));
    push(ctx->lo, ctx->es, consint(n.int_.val - 1));
    push(ctx->lo, ctx->es, P);
}

void Ploop (context *ctx, object P) {
    push(ctx->lo, ctx->es, consoper(ctx, "loop", NULL,0,0));
    push(ctx->lo, ctx->es, consoper(ctx, "cvx", NULL,0,0));
    push(ctx->lo, ctx->es, cvlit(P));
    push(ctx->lo, ctx->es, P);
}

void Zexit (context *ctx) {
    object oploop = consoper(ctx, "loop", NULL,0,0);
    object opforall = consoper(ctx, "forall", NULL,0,0);
    object opfor = consoper(ctx, "for", NULL,0,0);
    object oprepeat = consoper(ctx, "repeat", NULL,0,0);

    object x;
    printf("\nexit\n");
    dumpstack(ctx->lo, ctx->os);
    dumpstack(ctx->lo, ctx->es);
    printf("\n");
    while(1){
        x = pop(ctx->lo, ctx->es);
        dumpobject(x);
        if ( (objcmp(ctx, oploop, x) == 0)
          || (objcmp(ctx, opforall, x) == 0)
          || (objcmp(ctx, opfor, x) == 0)
          || (objcmp(ctx, oprepeat, x) == 0)
          )
            break;
    }
    printf("result:");
    dumpstack(ctx->lo, ctx->es);
}

void initopc (context *ctx, object sd) {
    oper *optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB));
    object n,op;

    op = consoper(ctx, "exec", Aexec, 0, 1, anytype); INSTALL;
    op = consoper(ctx, "if", BPif, 0, 2, booleantype, proctype); INSTALL;
    op = consoper(ctx, "ifelse", BPPifelse, 0, 3, booleantype, proctype, proctype); INSTALL;
    op = consoper(ctx, "for", IIIPfor, 0, 4, \
            integertype, integertype, integertype, proctype); INSTALL;
    op = consoper(ctx, "for", RRRPfor, 0, 4, \
            floattype, floattype, floattype, proctype); INSTALL;
    op = consoper(ctx, "repeat", IPrepeat, 0, 2, integertype, proctype); INSTALL;
    op = consoper(ctx, "loop", Ploop, 0, 1, proctype); INSTALL;
    op = consoper(ctx, "exit", Zexit, 0, 0); INSTALL;
    /*
    op = consoper(ctx, "eq", Aeq, 1, 2, anytype, anytype); INSTALL;
    //dumpdic(ctx->gl, sd); fflush(NULL);
    bdcput(ctx, sd, consname(ctx, "mark"), mark);
    */

}


