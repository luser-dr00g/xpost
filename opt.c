#include <alloca.h>
#include <stdbool.h>
#include <stdlib.h> /* NULL strtod */
#include <string.h>

#include "m.h"
#include "ob.h"
#include "s.h"
#include "itp.h"
#include "nm.h"
#include "st.h"
#include "di.h"
#include "op.h"

void Acvx(context *ctx, object o){
    push(ctx->lo, ctx->os, cvx(o));
}

void Acvlit(context *ctx, object o){
    push(ctx->lo, ctx->os, cvlit(o));
}

void Acvr(context *ctx, object o) {
    switch(type(o)){
    default: error("typecheck");
    case realtype: break;
    case integertype: o = consreal(o.int_.val);
    case stringtype: {
                         char *s = alloca(o.comp_.sz + 1);
                         memcpy(s, charstr(ctx, o), o.comp_.sz);
                         s[o.comp_.sz] = '\0';
                         o = consreal(strtod(s, NULL));
                     }

    }
    push(ctx->lo, ctx->os, o);
}

void initopt(context *ctx, object sd) {
    oper *optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB));
    object n,op;

    op = consoper(ctx, "cvx", Acvx, 1, 1, anytype); INSTALL;
    op = consoper(ctx, "cvlit", Acvlit, 1, 1, anytype); INSTALL;
    op = consoper(ctx, "cvr", Acvr, 1, 1, anytype); INSTALL;
    /* dumpdic(ctx->gl, sd); fflush(NULL);
    bdcput(ctx, sd, consname(ctx, "mark"), mark); */

}


