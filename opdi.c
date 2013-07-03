#define DEBUGLOAD

#include <stdbool.h>
#include <stdio.h>

#include "m.h"
#include "ob.h"
#include "s.h"
#include "v.h"
#include "itp.h"
#include "st.h"
#include "ar.h"
#include "di.h"
#include "nm.h"
#include "op.h"
#include "ops.h"

void Idict(context *ctx, object I) {
    push(ctx->lo, ctx->os, consbdc(ctx, I.int_.val));
}

void Dlength(context *ctx, object D) {
    push(ctx->lo, ctx->os, consint(diclength(
                    bank(ctx, D) /*D.tag&FBANK?ctx->gl:ctx->lo*/,
                    D)));
}

void Dmaxlength(context *ctx, object D) {
    push(ctx->lo, ctx->os, consint(dicmaxlength(
                    bank(ctx, D) /*D.tag&FBANK?ctx->gl:ctx->lo*/,
                    D)));
}

void Dbegin(context *ctx, object D) {
    push(ctx->lo, ctx->ds, D);
}

void Zend(context *ctx) {
    (void)pop(ctx->lo, ctx->ds);
}

void Adef(context *ctx, object K, object V) {
    bdcput(ctx, top(ctx->lo, ctx->ds, 0), K, V);
}

void DAknown(context *ctx, object D, object K) {
    printf("\nknown: ");
    dumpobject(D);
    dumpdic(bank(ctx, D), D); puts("");
    dumpobject(K);
    push(ctx->lo, ctx->os, consbool(dicknown(ctx, bank(ctx, D), D, K)));
}

void DAget(context *ctx, object D, object K) {
    push(ctx->lo, ctx->os, bdcget(ctx, D, K));
}

void DAAput(context *ctx, object D, object K, object V) {
    bdcput(ctx, D, K, V);
}

void Aload(context *ctx, object K) {
    int i;
    int z = count(ctx->lo, ctx->ds);
#ifdef DEBUGLOAD
    printf("\nload:");
    dumpobject(K);
    dumpstack(ctx->lo, ctx->ds);
#endif

    for (i = 0; i < z; i++) {
        object D = top(ctx->lo,ctx->ds,i);

#ifdef DEBUGLOAD
        dumpdic(bank(ctx, D), D); puts("");
#endif

        if (dicknown(ctx, bank(ctx, D), D, K)) {
            push(ctx->lo, ctx->os, bdcget(ctx, D, K));
            return;
        }
    }

#ifdef DEBUGLOAD
    dumpmfile(ctx->lo);
    dumpmtab(ctx->lo, 0);
    dumpmfile(ctx->gl);
    dumpmtab(ctx->gl, 0);
    dumpstack(ctx->gl, adrent(ctx->gl, NAMES));
    dumpobject(K);
#endif
    error("undefined (Aload)");
}

/* mark k_1 v_1 ... k_N v_N  >>  dict
   construct dictionary from pairs on stack
 */
void dictomark(context *ctx) {
    int i;
    object d, k, v;
    Zcounttomark(ctx);
    i = pop(ctx->lo, ctx->os).int_.val;
    d = consbdc(ctx, i);
    for ( ; i > 0; i -= 2){
        v = pop(ctx->lo, ctx->os);
        k = pop(ctx->lo, ctx->os);
        bdcput(ctx, d, k, v);
    }
    (void)pop(ctx->lo, ctx->os); // pop mark
    push(ctx->lo, ctx->os, d);
}

//TODO where forall currentdict countdictstack dictstack

void initopdi(context *ctx, object sd) {
    oper *optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB));
    object n,op;
    op = consoper(ctx, "dict", Idict, 1, 1, integertype); INSTALL;
    op = consoper(ctx, "length", Dlength, 1, 1, dicttype); INSTALL;
    op = consoper(ctx, "maxlength", Dmaxlength, 1, 1, dicttype); INSTALL;
    op = consoper(ctx, "begin", Dbegin, 0, 1, dicttype); INSTALL;
    op = consoper(ctx, "end", Zend, 0, 0); INSTALL;
    op = consoper(ctx, "def", Adef, 0, 2, anytype, anytype); INSTALL;
    op = consoper(ctx, "known", DAknown, 1, 2, dicttype, anytype); INSTALL;
    op = consoper(ctx, "get", DAget, 1, 2, dicttype, anytype); INSTALL;
    op = consoper(ctx, "put", DAAput, 1, 3,
            dicttype, anytype, anytype); INSTALL;
    op = consoper(ctx, "load", Aload, 1, 1, anytype); INSTALL;
    bdcput(ctx, sd, consname(ctx, "<<"), mark);
    op = consoper(ctx, ">>", dictomark, 1, 0); INSTALL;
}

