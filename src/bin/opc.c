/* control operators */

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
#include <stdio.h> /* printf */
#include <stdlib.h> /* NULL */

#include "m.h"
#include "ob.h"
#include "s.h"
#include "itp.h"
#include "err.h"
#include "nm.h"
#include "ar.h"
#include "di.h"
#include "op.h"
#include "opc.h"

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
    assert(ctx->gl->base);
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
    object opfor = consoper(ctx, "for", NULL,0,0);
    object oprepeat = consoper(ctx, "repeat", NULL,0,0);
    object oploop = consoper(ctx, "loop", NULL,0,0);
    object opforall = consoper(ctx, "forall", NULL,0,0);
    object x;

#if 0
    printf("\nexit\n");
    dumpobject(opfor);
    dumpobject(oprepeat);
    dumpobject(oploop);
    dumpobject(opforall);

    dumpstack(ctx->lo, ctx->os);
    dumpstack(ctx->lo, ctx->es);
    printf("\n");
#endif

    while (1) {
        x = pop(ctx->lo, ctx->es);
        //dumpobject(x);
        if ( (objcmp(ctx, x, opfor)    == 0)
          || (objcmp(ctx, x, oprepeat) == 0)
          || (objcmp(ctx, x, oploop)   == 0)
          || (objcmp(ctx, x, opforall) == 0)
           ) {
            break;
        }
    }

#if 0
    printf("result:");
    dumpstack(ctx->lo, ctx->es);
#endif
}

/* The stopped context is a boolean 'false' on the exec stack,
   so normal execution simply falls through and pushes the 
   false onto the operand stack. 'stop' then merely has to 
   search for 'false' and push a 'true'.  */

void Zstop(context *ctx) {
    object f = consbool(false);
    int c = count(ctx->lo, ctx->es);
    object x;
    while (c--) {
        x = pop(ctx->lo, ctx->es);
        if(objcmp(ctx, f, x) == 0) {
            push(ctx->lo, ctx->os, consbool(true));
            return;
        }
    }
    error(unregistered, "no stopped context in 'stop'");
}

void Astopped(context *ctx, object o) {
    push(ctx->lo, ctx->es, consbool(false));
    push(ctx->lo, ctx->es, o);
}

void Zcountexecstack(context *ctx) {
    push(ctx->lo, ctx->os, consint(count(ctx->lo, ctx->es)));
}

void Aexecstack(context *ctx, object A) {
    int z = count(ctx->lo, ctx->es);
    int i;
    for (i=0; i < z; i++)
        barput(ctx, A, i, bot(ctx->lo, ctx->es, i));
    push(ctx->lo, ctx->os, arrgetinterval(A, 0, z));
}

//TODO start

void Zquit(context *ctx) {
    ctx->quit = 1;
}

void initopc (context *ctx, object sd) {
    oper *optab;
    object n,op;
    assert(ctx->gl->base);
    optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB));

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
    op = consoper(ctx, "stop", Zstop, 0, 0); INSTALL;
    op = consoper(ctx, "stopped", Astopped, 0, 1, anytype); INSTALL;
    op = consoper(ctx, "countexecstack", Zcountexecstack, 1, 0); INSTALL;
    op = consoper(ctx, "execstack", Aexecstack, 1, 1, arraytype); INSTALL;
    op = consoper(ctx, "quit", Zquit, 0, 0); INSTALL;
    /*
    op = consoper(ctx, "eq", Aeq, 1, 2, anytype, anytype); INSTALL;
    //dumpdic(ctx->gl, sd); fflush(NULL);
    bdcput(ctx, sd, consname(ctx, "mark"), mark);
    */

}


