#include <alloca.h>
#include <ctype.h>
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
    object b;
    if (!isreadable(f)) error("invalidaccess");
    b = fileread(ctx->lo, f);
    if (b.int_.val != EOF) {
        push(ctx->lo, ctx->os, b);
        push(ctx->lo, ctx->os, consbool(true));
    } else {
        push(ctx->lo, ctx->os, consbool(false));
    }
}

void Fwrite (context *ctx, object f, object i) {
    if (!iswritable(f)) error("invalidaccess");
    filewrite(ctx->lo, f, i);
}

char *hex = "0123456789" "ABCDEF" "abcdef";

void Freadhexstring (context *ctx, object F, object S) {
    int n;
    int c[2];
    int eof = 0;
    FILE *f;
    char *s;
    if (!filestatus(ctx->lo, F)) error("ioerror");
    if (!isreadable(F)) error("invalidaccess");
    f = filefile(ctx->lo, F);
    s = charstr(ctx, S);

    for(n=0; !eof && n < S.comp_.sz; n++) {
        do {
            c[0] = fgetc(f);
            if (c[0] == EOF) ++eof;
        } while(!eof && strchr(hex, c[0]) == NULL);
        if (!eof) {
            do {
                c[1] = fgetc(f);
                if (c[1] == EOF) ++eof;
            } while(!eof && strchr(hex, c[1]) == NULL);
        } else {
            c[1] = '0';
        }
        s[n] = ((strchr(hex, toupper(c[0])) - hex) << 4)
             | (strchr(hex, toupper(c[1])) - hex);
    }
    S.comp_.sz = n;
    push(ctx->lo, ctx->os, S);
    push(ctx->lo, ctx->os, consbool(!eof));
}

void Fwritehexstring (context *ctx, object f, object s) {
}

void initopf (context *ctx, object sd) {
    oper *optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB));
    object n,op;

    op = consoper(ctx, "file", Sfile, 1, 2, stringtype, stringtype); INSTALL;
    op = consoper(ctx, "closefile", Fclosefile, 0, 1, filetype); INSTALL;
    op = consoper(ctx, "read", Fread, 1, 1, filetype); INSTALL;
    op = consoper(ctx, "write", Fwrite, 0, 2, filetype, integertype); INSTALL;
    op = consoper(ctx, "readhexstring", Freadhexstring, 2, 2, \
            filetype, stringtype); INSTALL;

    /* dumpdic(ctx->gl, sd); fflush(NULL);
    bdcput(ctx, sd, consname(ctx, "mark"), mark); */
}


