#include <alloca.h>
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

object bind (context *ctx, object p) {
    object t, d, r;
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

void initopx(context *ctx, object sd) {
    oper *optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB));
    object n,op;

    op = consoper(ctx, "bind", Pbind, 1, 1, proctype); INSTALL;
    bdcput(ctx, sd, consname(ctx, "null"), null);

    /* dumpdic(ctx->gl, sd); fflush(NULL);
    bdcput(ctx, sd, consname(ctx, "mark"), mark); */

}


