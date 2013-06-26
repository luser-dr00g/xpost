#include <alloca.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h> /* NULL */
#include <string.h>

#include "m.h"
#include "ob.h"
#include "s.h"
#include "itp.h"
#include "nm.h"
#include "st.h"
#include "di.h"
#include "op.h"
#include "f.h"

void Sfile (context *ctx, object fn, object mode) {
    object f;
    f = fileopen(ctx->lo, charstr(ctx, fn), charstr(ctx, mode));
    push(ctx->lo, ctx->os, f);
}

void Fclosefile (context *ctx, object f) {
    fileclose(ctx->lo, f);
}

void Fread (context *ctx, object f) {
    push(ctx->lo, ctx->os, fileread(ctx->lo, f));
}

void Fwrite (context *ctx, object f, object i) {
    filewrite(ctx->lo, f, i);
}

void initopf (context *ctx, object sd) {
    oper *optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB));
    object n,op;

    op = consoper(ctx, "file", Sfile, 1, 2, stringtype, stringtype); INSTALL;
    op = consoper(ctx, "closefile", Fclosefile, 0, 1, filetype); INSTALL;
    op = consoper(ctx, "read", Fread, 1, 1, filetype); INSTALL;
    op = consoper(ctx, "write", Fwrite, 0, 2, filetype, integertype); INSTALL;

    /* dumpdic(ctx->gl, sd); fflush(NULL);
    bdcput(ctx, sd, consname(ctx, "mark"), mark); */
}


