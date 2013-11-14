/*
 * Xpost - a Level-2 Postscript interpreter
 * Copyright (C) 2013, Michael Joshua Ryan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of the Xpost software product nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

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
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h> /* NULL */
#include <string.h>

#include "xpost_compat.h"
#include "xpost_memory.h"
#include "xpost_object.h"
#include "xpost_stack.h"

#include "xpost_context.h"
#include "xpost_interpreter.h"
#include "xpost_error.h"
#include "xpost_name.h"
#include "xpost_string.h"
#include "xpost_array.h"
#include "xpost_dict.h"
#include "xpost_operator.h"
#include "xpost_file.h"
#include "xpost_op_file.h"

#ifdef HAVE_WIN32
# include "glob.h"
#else
# include <glob.h>
# include <stdio_ext.h> /* __fpurge */
#endif

static
int Sfile (Xpost_Context *ctx,
            Xpost_Object fn,
            Xpost_Object mode)
{
    Xpost_Object f;
    char *cfn, *cmode;
    cfn = alloca(fn.comp_.sz + 1);
    memcpy(cfn, charstr(ctx, fn), fn.comp_.sz);
    cfn[fn.comp_.sz] = '\0';
    cmode = alloca(mode.comp_.sz + 1);
    memcpy(cmode, charstr(ctx, mode), mode.comp_.sz);
    cmode[mode.comp_.sz] = '\0';

    f = fileopen(ctx->lo, cfn, cmode);
    xpost_stack_push(ctx->lo, ctx->os, xpost_object_cvlit(f));
    return 0;
}

static
int Fclosefile (Xpost_Context *ctx,
                 Xpost_Object f)
{
    fileclose(ctx->lo, f);
    return 0;
}

static
int Fread (Xpost_Context *ctx,
            Xpost_Object f)
{
    Xpost_Object b;
    if (!xpost_object_is_readable(f)) error(invalidaccess, "Fread");
    b = fileread(ctx->lo, f);
    if (b.int_.val != EOF) {
        xpost_stack_push(ctx->lo, ctx->os, b);
        xpost_stack_push(ctx->lo, ctx->os, xpost_cons_bool(1));
    } else {
        xpost_stack_push(ctx->lo, ctx->os, xpost_cons_bool(0));
    }
    return 0;
}

static
int Fwrite (Xpost_Context *ctx,
             Xpost_Object f,
             Xpost_Object i)
{
    if (!xpost_object_is_writeable(f)) error(invalidaccess, "Fwrite");
    filewrite(ctx->lo, f, i);
    return 0;
}

char *hex = "0123456789" "ABCDEF" "abcdef";

static
int Freadhexstring (Xpost_Context *ctx,
                     Xpost_Object F,
                     Xpost_Object S)
{
    int n;
    int c[2];
    int eof = 0;
    FILE *f;
    char *s;
    if (!filestatus(ctx->lo, F)) error(ioerror, "Freadhexstring");
    if (!xpost_object_is_readable(F)) error(invalidaccess, "Freadhexstring");
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
    xpost_stack_push(ctx->lo, ctx->os, S);
    xpost_stack_push(ctx->lo, ctx->os, xpost_cons_bool(!eof));
    return 0;
}

static
int Fwritehexstring (Xpost_Context *ctx,
                      Xpost_Object F,
                      Xpost_Object S)
{
    int n;
    FILE *f;
    char *s;
    if (!filestatus(ctx->lo, F)) error(ioerror, "Fwritehexstring");
    if (!xpost_object_is_writeable(F)) error(invalidaccess, "Fwritehexstring");
    f = filefile(ctx->lo, F);
    s = charstr(ctx, S);

    for(n=0; n < S.comp_.sz; n++) {
        if (fputc(hex[s[n] / 16], f)) error(ioerror, "Fwritehexstring");
        if (fputc(hex[s[n] % 16], f)) error(ioerror, "Fwritehexstring");
    }
    return 0;
}

static
int Freadstring (Xpost_Context *ctx,
                  Xpost_Object F,
                  Xpost_Object S)
{
    int n;
    FILE *f;
    char *s;
    if (!filestatus(ctx->lo, F)) error(ioerror, "Freadstring");
    if (!xpost_object_is_readable(F)) error(invalidaccess, "Freadstring");
    f = filefile(ctx->lo, F);
    s = charstr(ctx, S);
    n = fread(s, 1, S.comp_.sz, f);
    if (n == S.comp_.sz) {
        xpost_stack_push(ctx->lo, ctx->os, S);
        xpost_stack_push(ctx->lo, ctx->os, xpost_cons_bool(1));
    } else {
        S.comp_.sz = n;
        xpost_stack_push(ctx->lo, ctx->os, S);
        xpost_stack_push(ctx->lo, ctx->os, xpost_cons_bool(0));
    }
    return 0;
}

static
int Fwritestring (Xpost_Context *ctx,
                   Xpost_Object F,
                   Xpost_Object S)
{
    FILE *f;
    char *s;
    if (!filestatus(ctx->lo, F)) error(ioerror, "Fwritestring");
    if (!xpost_object_is_writeable(F)) error(invalidaccess, "Fwritestring");
    f = filefile(ctx->lo, F);
    s = charstr(ctx, S);
    if (fwrite(s, 1, S.comp_.sz, f) != S.comp_.sz)
        error(ioerror, "Fwritestring");
    return 0;
}

static
int Freadline (Xpost_Context *ctx,
                Xpost_Object F,
                Xpost_Object S)
{
    FILE *f;
    char *s;
    int n, c = ' ';
    if (!filestatus(ctx->lo, F)) error(ioerror, "Freadline");
    if (!xpost_object_is_writeable(F)) error(invalidaccess, "Freadline");
    f = filefile(ctx->lo, F);
    s = charstr(ctx, S);
    for (n=0; n < S.comp_.sz; n++) {
        c = fgetc(f);
        if (c == EOF || c == '\n') break;
        s[n] = c;
    }
    if (n == S.comp_.sz && c != '\n') error(rangecheck, "Freadline");
    S.comp_.sz = n;
    xpost_stack_push(ctx->lo, ctx->os, S);
    xpost_stack_push(ctx->lo, ctx->os, xpost_cons_bool(c != EOF));
    return 0;
}

static
int Fbytesavailable (Xpost_Context *ctx,
                      Xpost_Object F)
{
    xpost_stack_push(ctx->lo, ctx->os, xpost_cons_int(filebytesavailable(ctx->lo, F)));
    return 0;
}

static
int Zflush (Xpost_Context *ctx)
{
    int ret;
    (void)ctx;
    ret = fflush(NULL);
    if (ret != 0) error(ioerror, "fflush did not return 0");
    return 0;
}

static
int Fflushfile (Xpost_Context *ctx,
                 Xpost_Object F)
{
    int ret;
    FILE *f;
    if (!filestatus(ctx->lo, F)) return 0;
    f = filefile(ctx->lo, F);
    if (xpost_object_is_writeable(F)) {
        ret = fflush(f);
        if (ret != 0) error(ioerror, "fflush did not return 0");
    } else {
        int c;
        while ((c = fgetc(f)) != EOF)
            /**/;
    }
    return 0;
}

#ifndef HAVE_WIN32

static
int Fresetfile (Xpost_Context *ctx,
                 Xpost_Object F)
{
    FILE *f;
    if (!filestatus(ctx->lo, F)) return 0;
    f = filefile(ctx->lo, F);
    __fpurge(f);
    return 0;
}

#endif

static
int Fstatus (Xpost_Context *ctx,
              Xpost_Object F)
{
    xpost_stack_push(ctx->lo, ctx->os, xpost_cons_bool(filestatus(ctx->lo, F)));
    return 0;
}

static
int Zcurrentfile (Xpost_Context *ctx)
{
    int z = xpost_stack_count(ctx->lo, ctx->es);
    int i;
    Xpost_Object o;
    for (i=0; i<z; i++) {
        o = xpost_stack_topdown_fetch(ctx->lo, ctx->es, i);
        if (xpost_object_get_type(o) == filetype) {
            xpost_stack_push(ctx->lo, ctx->os, o);
            return 0;
        }
    }
    xpost_stack_push(ctx->lo, ctx->os, consfile(ctx->lo, NULL));
    return 0;
}

static
int deletefile (Xpost_Context *ctx,
                 Xpost_Object S)
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
    return 0;
}

static
int renamefile (Xpost_Context *ctx,
                 Xpost_Object Old,
                 Xpost_Object New)
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
    return 0;
}

//#ifndef HAVE_WIN32

static
int contfilenameforall (Xpost_Context *ctx,
                         Xpost_Object oglob,
                         Xpost_Object Proc,
                         Xpost_Object Scr)
{
    glob_t *globbuf;
    char *str;
    char *src;
    int len;
    globbuf = oglob.glob_.ptr;
    if (oglob.glob_.off < globbuf->gl_pathc) {
        //xpost_stack_push(ctx->lo, ctx->es, consoper(ctx, "contfilenameforall", NULL,0,0));
        xpost_stack_push(ctx->lo, ctx->es, operfromcode(ctx->opcode_shortcuts.contfilenameforall));
        xpost_stack_push(ctx->lo, ctx->es, Scr);
        //xpost_stack_push(ctx->lo, ctx->es, consoper(ctx, "cvx", NULL,0,0));
        xpost_stack_push(ctx->lo, ctx->es, operfromcode(ctx->opcode_shortcuts.cvx));
        xpost_stack_push(ctx->lo, ctx->es, xpost_object_cvlit(Proc));
        ++oglob.glob_.off;
        xpost_stack_push(ctx->lo, ctx->es, oglob);

        str = charstr(ctx, Scr);
        src = globbuf->gl_pathv[ oglob.glob_.off-1 ];
        len = strlen(src);
        if (len > Scr.comp_.sz)
            error(rangecheck, "contfilenameforall");
        memcpy(str, src, len);
        xpost_stack_push(ctx->lo, ctx->os, arrgetinterval(Scr, 0, len));
        xpost_stack_push(ctx->lo, ctx->es, Proc);

    } else {
        globfree(globbuf);
    }
    return 0;
}

static
int filenameforall (Xpost_Context *ctx,
                     Xpost_Object Tmp,
                     Xpost_Object Proc,
                     Xpost_Object Scr)
{
    char *tmp;
    glob_t *globbuf;
    Xpost_Object oglob;
    int ret;

    tmp = charstr(ctx, Tmp);
    globbuf = malloc(sizeof *globbuf);
    ret = glob(tmp, 0, NULL, globbuf);
    if (ret != 0)
        error(ioerror, "filenameforall");

    oglob.glob_.tag = globtype;
    oglob.glob_.off = 0;
    oglob.glob_.ptr = globbuf;

    contfilenameforall(ctx, oglob, Proc, xpost_object_cvlit(Scr));
    return 0;
}

//#endif

static
int setfileposition (Xpost_Context *ctx,
            Xpost_Object F,
            Xpost_Object pos)
{
    int ret = fseek(filefile(ctx->lo, F), pos.int_.val, SEEK_SET);
    if (ret != 0)
    {
        error(ioerror, "fseek returned non-zero");
    }
    return 0;
}

static
int fileposition (Xpost_Context *ctx,
            Xpost_Object F)
{
    long pos;
    pos = ftell(filefile(ctx->lo, F));
    if (pos == -1)
        error(ioerror, "ftell returned -1");
    else
        xpost_stack_push(ctx->lo, ctx->os, xpost_cons_int(pos));
    return 0;
}

static
int Sprint (Xpost_Context *ctx,
             Xpost_Object S)
{
    size_t ret;
    char *s;
    s = charstr(ctx, S);
    ret = fwrite(s, 1, S.comp_.sz, stdout);
    if (ret != S.comp_.sz)
        error(ioerror, "Sprint() fwrite returned unexpected value");
    return 0;
}

static
int Becho (Xpost_Context *ctx,
            Xpost_Object b)
{
    (void)ctx;
    if (b.int_.val)
        echoon(stdin);
    else
        echooff(stdin);
    return 0;
}

int initopf (Xpost_Context *ctx,
              Xpost_Object sd)
{
    oper *optab;
    Xpost_Object n,op;
    unsigned int optadr;

    assert(ctx->gl->base);

    xpost_memory_table_get_addr(ctx->gl,
            XPOST_MEMORY_TABLE_SPECIAL_OPERATOR_TABLE, &optadr);
    optab = (void *)(ctx->gl->base + optadr);

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
    ctx->opcode_shortcuts.contfilenameforall = op.mark_.padw;
    op = consoper(ctx, "filenameforall", filenameforall, 0, 3, stringtype, proctype, stringtype); INSTALL;
//#endif
    op = consoper(ctx, "setfileposition", setfileposition, 0, 2, filetype, integertype); INSTALL;
    op = consoper(ctx, "fileposition", fileposition, 1, 1, filetype); INSTALL;
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
    return 0;
}


