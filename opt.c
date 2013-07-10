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
#include "di.h"
#include "op.h"

void Atype(context *ctx, object o) {
    push(ctx->lo, ctx->os, cvx(consname(ctx, types[type(o)])));
}

void Acvlit(context *ctx, object o){
    push(ctx->lo, ctx->os, cvlit(o));
}

void Acvx(context *ctx, object o){
    push(ctx->lo, ctx->os, cvx(o));
}

void Axcheck(context *ctx, object o) {
    push(ctx->lo, ctx->os, consbool(isx(o)));
}

void Aexecuteonly(context *ctx, object o) {
    o.tag &= ~FACCESS;
    o.tag |= (executeonly << FACCESSO);
    push(ctx->lo, ctx->os, o);
}

void Anoaccess(context *ctx, object o) {
    o.tag &= ~FACCESS;
    o.tag |= (noaccess << FACCESSO);
    push(ctx->lo, ctx->os, o);
}

void Areadonly(context *ctx, object o) {
    o.tag &= ~FACCESS;
    o.tag |= (readonly << FACCESSO);
    push(ctx->lo, ctx->os, o);
}

void Archeck(context *ctx, object o) {
    push(ctx->lo, ctx->os, consbool( (o.tag & FACCESS) >> FACCESSO >= readonly ));
}

void Awcheck(context *ctx, object o) {
    push(ctx->lo, ctx->os, consbool( (o.tag & FACCESS) >> FACCESSO == unlimited ));
}

void Scvn(context *ctx, object s) {
    char *t = alloca(s.comp_.sz+1);
    memcpy(t, charstr(ctx, s), s.comp_.sz);
    t[s.comp_.sz] = '\0';
    push(ctx->lo, ctx->os, consname(ctx, t));
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
                         printf("cvr %s\n", s);
                         o = consreal(strtod(s, NULL));
                     }

    }
    push(ctx->lo, ctx->os, o);
}

void initopt(context *ctx, object sd) {
    oper *optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB));
    object n,op;

    op = consoper(ctx, "type", Atype, 1, 1, anytype); INSTALL;
    op = consoper(ctx, "cvlit", Acvlit, 1, 1, anytype); INSTALL;
    op = consoper(ctx, "cvx", Acvx, 1, 1, anytype); INSTALL;
    op = consoper(ctx, "xcheck", Axcheck, 1, 1, anytype); INSTALL;
    op = consoper(ctx, "executeonly", Aexecuteonly, 1, 1, anytype); INSTALL;
    op = consoper(ctx, "noaccess", Anoaccess, 1, 1, anytype); INSTALL;
    op = consoper(ctx, "readonly", Areadonly, 1, 1, anytype); INSTALL;
    op = consoper(ctx, "rcheck", Archeck, 1, 1, anytype); INSTALL;
    op = consoper(ctx, "wcheck", Awcheck, 1, 1, anytype); INSTALL;
    op = consoper(ctx, "cvn", Scvn, 1, 1, stringtype); INSTALL;
    op = consoper(ctx, "cvr", Acvr, 1, 1, anytype); INSTALL;
    /* dumpdic(ctx->gl, sd); fflush(NULL);
    bdcput(ctx, sd, consname(ctx, "mark"), mark); */

}


