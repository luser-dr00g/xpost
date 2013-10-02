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

#ifdef HAVE_STDBOOL_H
# include <stdbool.h>
#else
# ifndef HAVE__BOOL
#  ifdef __cplusplus
typedef bool _Bool;
#  else
#   define _Bool signed char
#  endif
# endif
# define bool _Bool
# define false 0
# define true 1
# define __bool_true_false_are_defined 1
#endif

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h> /* NULL */
#include <string.h>

#include "m.h"
#include "ob.h"
#include "s.h"
#include "itp.h"
#include "err.h"
#include "nm.h"
#include "st.h"
#include "ar.h"
#include "di.h"
#include "op.h"
#include "xpost_file.h"
#include "opf.h"

#ifdef HAVE_WIN32
# include "glob.h"
# include "osmswin.h"
#else
# include <glob.h>
# include <stdio_ext.h> /* __fpurge */
# include "osunix.h"
#endif

static
void Sfile (context *ctx,
            object fn,
            object mode)
{
    object f;
    f = fileopen(ctx->lo, charstr(ctx, fn), charstr(ctx, mode));
    push(ctx->lo, ctx->os, cvlit(f));
}

static
void Fclosefile (context *ctx,
                 object f)
{
    fileclose(ctx->lo, f);
}

static
void Fread (context *ctx,
            object f)
{
    object b;
    if (!isreadable(f)) error(invalidaccess, "Fread");
    b = fileread(ctx->lo, f);
    if (b.int_.val != EOF) {
        push(ctx->lo, ctx->os, b);
        push(ctx->lo, ctx->os, consbool(true));
    } else {
        push(ctx->lo, ctx->os, consbool(false));
    }
}

static
void Fwrite (context *ctx,
             object f,
             object i)
{
    if (!iswriteable(f)) error(invalidaccess, "Fwrite");
    filewrite(ctx->lo, f, i);
}

char *hex = "0123456789" "ABCDEF" "abcdef";

static
void Freadhexstring (context *ctx,
                     object F,
                     object S)
{
    int n;
    int c[2];
    int eof = 0;
    FILE *f;
    char *s;
    if (!filestatus(ctx->lo, F)) error(ioerror, "Freadhexstring");
    if (!isreadable(F)) error(invalidaccess, "Freadhexstring");
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

static
void Fwritehexstring (context *ctx,
                      object F,
                      object S)
{
    int n;
    FILE *f;
    char *s;
    if (!filestatus(ctx->lo, F)) error(ioerror, "Fwritehexstring");
    if (!iswriteable(F)) error(invalidaccess, "Fwritehexstring");
    f = filefile(ctx->lo, F);
    s = charstr(ctx, S);

    for(n=0; n < S.comp_.sz; n++) {
        if (fputc(hex[s[n] / 16], f)) error(ioerror, "Fwritehexstring");
        if (fputc(hex[s[n] % 16], f)) error(ioerror, "Fwritehexstring");
    }
}

static
void Freadstring (context *ctx,
                  object F,
                  object S)
{
    int n;
    FILE *f;
    char *s;
    if (!filestatus(ctx->lo, F)) error(ioerror, "Freadstring");
    if (!isreadable(F)) error(invalidaccess, "Freadstring");
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

static
void Fwritestring (context *ctx,
                   object F,
                   object S)
{
    FILE *f;
    char *s;
    if (!filestatus(ctx->lo, F)) error(ioerror, "Fwritestring");
    if (!iswriteable(F)) error(invalidaccess, "Fwritestring");
    f = filefile(ctx->lo, F);
    s = charstr(ctx, S);
    if (fwrite(s, 1, S.comp_.sz, f) != S.comp_.sz)
        error(ioerror, "Fwritestring");
}

static
void Freadline (context *ctx,
                object F,
                object S)
{
    FILE *f;
    char *s;
    int n, c = ' ';
    if (!filestatus(ctx->lo, F)) error(ioerror, "Freadline");
    if (!iswriteable(F)) error(invalidaccess, "Freadline");
    f = filefile(ctx->lo, F);
    s = charstr(ctx, S);
    for (n=0; n < S.comp_.sz; n++) {
        c = fgetc(f);
        if (c == EOF || c == '\n') break;
        s[n] = c;
    }
    if (n == S.comp_.sz && c != '\n') error(rangecheck, "Freadline");
    S.comp_.sz = n;
    push(ctx->lo, ctx->os, S);
    push(ctx->lo, ctx->os, consbool(c != EOF));
}

static
void Fbytesavailable (context *ctx,
                      object F)
{
    push(ctx->lo, ctx->os, consint(filebytesavailable(ctx->lo, F)));
}

static
void Zflush (context *ctx)
{
    int ret;
    (void)ctx;
    ret = fflush(NULL);
    if (ret != 0) error(ioerror, "fflush did not return 0");
}

static
void Fflushfile (context *ctx,
                 object F)
{
    int ret;
    FILE *f;
    if (!filestatus(ctx->lo, F)) return;
    f = filefile(ctx->lo, F);
    if (iswriteable(F)) {
        ret = fflush(f);
        if (ret != 0) error(ioerror, "fflush did not return 0");
    } else {
        int c;
        while ((c = fgetc(f)) != EOF)
            /**/;
    }
}

#ifndef HAVE_WIN32

static
void Fresetfile (context *ctx,
                 object F)
{
    FILE *f;
    if (!filestatus(ctx->lo, F)) return;
    f = filefile(ctx->lo, F);
    __fpurge(f);
}

#endif

static
void Fstatus (context *ctx,
              object F)
{
    push(ctx->lo, ctx->os, consbool(filestatus(ctx->lo, F)));
}

static
void Zcurrentfile (context *ctx)
{
    int z = count(ctx->lo, ctx->es);
    int i;
    object o;
    for (i=0; i<z; i++) {
        o = top(ctx->lo, ctx->es, i);
        if (type(o) == filetype) {
            push(ctx->lo, ctx->os, o);
            return;
        }
    }
    push(ctx->lo, ctx->os, consfile(ctx->lo, NULL));
}

static
void deletefile (context *ctx,
                 object S)
{
    char *s;
    int ret;
    s = charstr(ctx, S);
    ret = remove(s);
    if (ret != 0)
        switch (errno) {
            case ENOENT: error(undefinedfilename, "deletefile");
            default: error(ioerror, "deletefile");
        }
}

static
void renamefile (context *ctx,
                 object Old,
                 object New)
{
    char *old, *new;
    int ret;
    old = charstr(ctx, Old);
    new = charstr(ctx, New);
    ret = rename(old, new);
    if (ret != 0)
        switch(errno) {
            case ENOENT: error(undefinedfilename, "renamefile");
            default: error(ioerror, "renamefile");
        }
}

//#ifndef HAVE_WIN32

static
void contfilenameforall (context *ctx,
                         object oglob,
                         object Proc,
                         object Scr)
{
    glob_t *globbuf;
    char *str;
    char *src;
    int len;
    globbuf = oglob.glob_.ptr;
    if (oglob.glob_.off < globbuf->gl_pathc) {
        push(ctx->lo, ctx->es, consoper(ctx, "contfilenameforall", NULL,0,0));
        push(ctx->lo, ctx->es, Scr);
        push(ctx->lo, ctx->es, consoper(ctx, "cvx", NULL,0,0));
        push(ctx->lo, ctx->es, cvlit(Proc));
        ++oglob.glob_.off;
        push(ctx->lo, ctx->es, oglob);

        str = charstr(ctx, Scr);
        src = globbuf->gl_pathv[ oglob.glob_.off-1 ];
        len = strlen(src);
        if (len > Scr.comp_.sz)
            error(rangecheck, "contfilenameforall");
        memcpy(str, src, len);
        push(ctx->lo, ctx->os, arrgetinterval(Scr, 0, len));
        push(ctx->lo, ctx->es, Proc);

    } else {
        globfree(globbuf);
    }
}

static
void filenameforall (context *ctx,
                     object Tmp,
                     object Proc,
                     object Scr)
{
    char *tmp;
    glob_t *globbuf;
    object oglob;
    int ret;

    tmp = charstr(ctx, Tmp);
    globbuf = malloc(sizeof *globbuf);
    ret = glob(tmp, 0, NULL, globbuf);
    if (ret != 0)
        error(ioerror, "filenameforall");

    oglob.glob_.tag = globtype;
    oglob.glob_.off = 0;
    oglob.glob_.ptr = globbuf;

    contfilenameforall(ctx, oglob, Proc, cvlit(Scr));
}

//#endif

static
void Sprint (context *ctx,
             object S)
{
    size_t ret;
    char *s;
    s = charstr(ctx, S);
    ret = fwrite(s, 1, S.comp_.sz, stdout);
    if (ret != S.comp_.sz)
        error(ioerror, "Sprint() fwrite returned unexpected value");
}

static
void Becho (context *ctx,
            object b)
{
    (void)ctx;
    if (b.int_.val)
        echoon(stdin);
    else
        echooff(stdin);
}

void initopf (context *ctx,
              object sd)
{
    oper *optab;
    object n,op;

    assert(ctx->gl->base);

    optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB));

    op = consoper(ctx, "file", Sfile, 1, 2, stringtype, stringtype); INSTALL;
    //filter
    op = consoper(ctx, "closefile", Fclosefile, 0, 1, filetype); INSTALL;
    op = consoper(ctx, "read", Fread, 1, 1, filetype); INSTALL;
    op = consoper(ctx, "write", Fwrite, 0, 2, filetype, integertype); INSTALL;
    op = consoper(ctx, "readhexstring", Freadhexstring, 2, 2, filetype, stringtype); INSTALL;
    op = consoper(ctx, "writehexstring", Fwritehexstring, 0, 2, filetype, stringtype); INSTALL;
    op = consoper(ctx, "readstring", Freadstring, 2, 2, filetype, stringtype); INSTALL;
    op = consoper(ctx, "writestring", Fwritestring, 0, 2, filetype, stringtype); INSTALL;
    op = consoper(ctx, "readline", Freadline, 2, 2, filetype, stringtype); INSTALL;
    //token: see optok.c
    op = consoper(ctx, "bytesavailable", Fbytesavailable, 1, 1, filetype); INSTALL;
    op = consoper(ctx, "flush", Zflush, 0, 0); INSTALL;
    op = consoper(ctx, "flushfile", Fflushfile, 0, 1, filetype); INSTALL;
#ifndef HAVE_WIN32
    op = consoper(ctx, "resetfile", Fresetfile, 0, 1, filetype); INSTALL;
#endif
    op = consoper(ctx, "status", Fstatus, 1, 1, filetype); INSTALL;
    //string status
    //run: see init.ps
    op = consoper(ctx, "currentfile", Zcurrentfile, 1, 0); INSTALL;
    op = consoper(ctx, "deletefile", deletefile, 0, 1, stringtype); INSTALL;
    op = consoper(ctx, "renamefile", renamefile, 0, 2, stringtype, stringtype); INSTALL;
//#ifndef HAVE_WIN32
    op = consoper(ctx, "contfilenameforall", contfilenameforall, 0, 3, globtype, proctype, stringtype);
    op = consoper(ctx, "filenameforall", filenameforall, 0, 3, stringtype, proctype, stringtype); INSTALL;
//#endif
    //setfileposition
    //fileposition
    op = consoper(ctx, "print", Sprint, 0, 1, stringtype); INSTALL;
    //=: see init.ps
    //==: see init.ps
    //stack: see init.ps
    //pstack: see init.ps
    //printobject
    //writeobject
    //setobjectformat
    //currentobjectformat
    op = consoper(ctx, "echo", Becho, 0, 1, booleantype); INSTALL;

    /* dumpdic(ctx->gl, sd); fflush(NULL);
    bdcput(ctx, sd, consname(ctx, "mark"), mark); */
}


