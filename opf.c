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
    if (!iswriteable(f)) error("invalidaccess");
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

void Fwritehexstring (context *ctx, object F, object S) {
    int n;
    FILE *f;
    char *s;
    if (!filestatus(ctx->lo, F)) error("ioerror");
    if (!iswriteable(F)) error("invalidaccess");
    f = filefile(ctx->lo, F);
    s = charstr(ctx, S);

    for(n=0; n < S.comp_.sz; n++) {
        if (fputc(hex[s[n] / 16], f)) error("ioerror");
        if (fputc(hex[s[n] % 16], f)) error("ioerror");
    }
}

void Freadstring (context *ctx, object F, object S) {
    int n;
    FILE *f;
    char *s;
    if (!filestatus(ctx->lo, F)) error("ioerror");
    if (!isreadable(F)) error("invalidaccess");
    f = filefile(ctx->lo, F);
    s = charstr(ctx, S);
    n = fread(s, 1, S.comp_.sz, f);
    if (n == S.comp_.sz) {
        push(ctx->lo, ctx->os, S);
        push(ctx->lo, ctx->os, consbool(true));
    } else {
        S.comp_.sz = n;
        push(ctx->lo, ctx->os, S);
        push(ctx->lo, ctx->os, consbool(false));
    }
}

void Fwritestring (context *ctx, object F, object S) {
    FILE *f;
    char *s;
    if (!filestatus(ctx->lo, F)) error("ioerror");
    if (!iswriteable(F)) error("invalidaccess");
    f = filefile(ctx->lo, F);
    s = charstr(ctx, S);
    if (fwrite(s, 1, S.comp_.sz, f) != S.comp_.sz)
        error("ioerror");
}

void Freadline (context *ctx, object F, object S) {
    FILE *f;
    char *s;
    int n, c;
    if (!filestatus(ctx->lo, F)) error("ioerror");
    if (!iswriteable(F)) error("invalidaccess");
    f = filefile(ctx->lo, F);
    s = charstr(ctx, S);
    for (n=0; n < S.comp_.sz; n++) {
        c = fgetc(f);
        if (c == EOF || c == '\n') break;
        s[n] = c;
    }
    if (n == S.comp_.sz && c != '\n') error("rangecheck");
    S.comp_.sz = n;
    push(ctx->lo, ctx->os, S);
    push(ctx->lo, ctx->os, consbool(c != EOF));
}

void Fbytesavailable (context *ctx, object F) {
    push(ctx->lo, ctx->os, consint(filebytesavailable(ctx->lo, F)));
}

void initopf (context *ctx, object sd) {
    oper *optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB));
    object n,op;

    op = consoper(ctx, "file", Sfile, 1, 2, stringtype, stringtype); INSTALL;
    op = consoper(ctx, "closefile", Fclosefile, 0, 1, filetype); INSTALL;
    op = consoper(ctx, "read", Fread, 1, 1, filetype); INSTALL;
    op = consoper(ctx, "write", Fwrite, 0, 2, filetype, integertype); INSTALL;
    op = consoper(ctx, "readhexstring", Freadhexstring, 2, 2, filetype, stringtype); INSTALL;
    op = consoper(ctx, "writehexstring", Fwritehexstring, 0, 2, filetype, stringtype); INSTALL;
    op = consoper(ctx, "readstring", Freadstring, 2, 2, filetype, stringtype); INSTALL;
    op = consoper(ctx, "writestring", Fwritestring, 0, 2, filetype, stringtype); INSTALL;
    op = consoper(ctx, "readline", Freadline, 2, 2, filetype, stringtype); INSTALL;
    op = consoper(ctx, "bytesavailable", Fbytesavailable, 1, 1, filetype); 

    /* dumpdic(ctx->gl, sd); fflush(NULL);
    bdcput(ctx, sd, consname(ctx, "mark"), mark); */
}


