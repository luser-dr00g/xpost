#include <alloca.h>
#include <assert.h>
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
#include "opar.h"

void packedarray (context *ctx, object n) {
    int i;
    object a, v;
    a = consbar(ctx, n.int_.val);
    
    for (i=n.int_.val; i > 0; i--) {
        v = pop(ctx->lo, ctx->os);
        barput(ctx, a, i-1, v);
    }
    a = setfaccess(cvlit(a), readonly);
    push(ctx->lo, ctx->os, a);
}

void setpacking (context *ctx, object b) {
    object sd = bot(ctx->lo, ctx->ds, 0);
    bdcput(ctx, sd, consname(ctx, "currentpacking"), b);
}

void initoppa(context *ctx, object sd) {
    oper *optab;
    object n,op;
    assert(ctx->gl->base);
    optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB));

    op = consoper(ctx, "packedarray", packedarray, 1, 1, integertype); INSTALL;
    bdcput(ctx, sd, consname(ctx, "currentpacking"), consbool(false));
    op = consoper(ctx, "setpacking", setpacking, 0, 1, booleantype); INSTALL;

    /* dumpdic(ctx->gl, sd); fflush(NULL);
    bdcput(ctx, sd, consname(ctx, "mark"), mark); */

}


