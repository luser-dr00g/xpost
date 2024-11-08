/*
 * Xpost - a Level-2 Postscript interpreter
 * Copyright (C) 2013-2016, Michael Joshua Ryan
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

#include <stdlib.h> /* NULL */
#include <stddef.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
//#include <poll.h>
#include <stdio.h>
#include <string.h>

#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif

#ifdef _WIN32
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
# include <winsock2.h> /* select */
# undef WIN32_LEAN_AND_MEAN
#endif

#include "xpost.h"
#include "xpost_log.h"
#include "xpost_compat.h"
#include "xpost_memory.h"
#include "xpost_object.h"
#include "xpost_stack.h"
#include "xpost_context.h"
#include "xpost_error.h"
#include "xpost_name.h"
#include "xpost_string.h"
#include "xpost_array.h"
#include "xpost_dict.h"
#include "xpost_file.h"

//#include "xpost_interpreter.h"
#include "xpost_operator.h"
#include "xpost_op_file.h"

#ifndef _WIN32
# include <stdio_ext.h> /* __fpurge */
#endif

/* filename mode  file  file
   create file object for filename with access mode */
static
int xpost_op_string_mode_file (Xpost_Context *ctx,
                               Xpost_Object fn,
                               Xpost_Object mode)
{
    Xpost_Object f;
    char *cfn, *cmode;
    int ret;

    cfn = xpost_string_allocate_cstring(ctx, fn);
    cmode = xpost_string_allocate_cstring(ctx, mode);

    ret = xpost_file_open(ctx->lo, cfn, cmode, &f);
    if (ret){
        free(cfn);
	free(cmode);
        return ret;
    }
    xpost_stack_push(ctx->lo, ctx->os, xpost_object_cvlit(f));
    free(cfn);
    free(cmode);
    return 0;
}

/* file  closefile  -
   close file object */
static
int xpost_op_file_closefile (Xpost_Context *ctx,
                             Xpost_Object f)
{
    int ret;
    ret = xpost_file_object_close(ctx->lo, f);
    if (ret)
        return ret;
    return 0;
}

/* file  read  int true
               false
   read a byte from file */
static
int xpost_op_file_read(Xpost_Context *ctx,
                       Xpost_Object f)
{
    Xpost_Object b;
    if (!xpost_object_is_readable(ctx,f))
        return invalidaccess;
    b = xpost_file_read_byte(ctx->lo, f);
    if (xpost_object_get_type(b) == invalidtype)
        return ioerror;
    if (b.int_.val != EOF)
    {
        xpost_stack_push(ctx->lo, ctx->os, b);
        xpost_stack_push(ctx->lo, ctx->os, xpost_bool_cons(1));
    }
    else
    {
        xpost_stack_push(ctx->lo, ctx->os, xpost_bool_cons(0));
    }
    return 0;
}

/* file int  write  -
   write a byte to file */
static
int xpost_op_file_write (Xpost_Context *ctx,
                         Xpost_Object f,
                         Xpost_Object i)
{
    int ret;
    if (!xpost_object_is_writeable(ctx, f))
        return invalidaccess;
    ret = xpost_file_write_byte(ctx->lo, f, i);
    if (ret)
        return ret;
    return 0;
}

const char *hex = "0123456789" "ABCDEF" "abcdef";

static
int read_hex_digit( Xpost_File *f, int *p )
{
    int eof = 0;
    do
        if ((*p = xpost_file_getc(f)) == EOF)
            ++eof;
    while ( !eof && !strchr(hex, *p) );
    return eof;
}

/* file string  readhexstring  substring true
                               substring false
   read hex-encoded data from file into string */
static
int xpost_op_file_readhexstring (Xpost_Context *ctx,
                                 Xpost_Object F,
                                 Xpost_Object S)
{
    int n;
    int c[2];
    int eof = 0;
    Xpost_File *f;
    char *s;
    if (!xpost_file_get_status(ctx->lo, F))
        return ioerror;
    if (!xpost_object_is_readable(ctx,F))
        return invalidaccess;
    f = xpost_file_get_file_pointer(ctx->lo, F);
    s = xpost_string_get_pointer(ctx, S);

    for (n = 0; n < S.comp_.sz; n++)
    {
        eof = read_hex_digit(f, &c[0]);
	XPOST_LOG_INFO("read %c", c[0]);
        if (!eof) eof = read_hex_digit(f, &c[1]);
        if (eof) break;
	XPOST_LOG_INFO("read %c", c[1]);
        s[n] = ((strchr(hex, toupper(c[0])) - hex) << 4)
             + (strchr(hex, toupper(c[1])) - hex);
    }
    fflush(stdout);
    S.comp_.sz = n;
    xpost_stack_push(ctx->lo, ctx->os, S);
    xpost_stack_push(ctx->lo, ctx->os, xpost_bool_cons(!eof));
    return 0;
}

/* file string  writehexstring  -
   write string to file in hex-encoding */
static
int xpost_op_file_writehexstring (Xpost_Context *ctx,
                                  Xpost_Object F,
                                  Xpost_Object S)
{
    int n;
    Xpost_File *f;
    char *s;
    if (!xpost_file_get_status(ctx->lo, F))
        return ioerror;
    if (!xpost_object_is_writeable(ctx, F))
        return invalidaccess;
    f = xpost_file_get_file_pointer(ctx->lo, F);
    s = xpost_string_get_pointer(ctx, S);

    for (n = 0; n < S.comp_.sz; n++)
    {
        if (xpost_file_putc(f, hex[s[n] / 16]) == EOF)
            return ioerror;
        if (xpost_file_putc(f, hex[s[n] % 16]) == EOF)
            return ioerror;
    }
    return 0;
}

/* file string  readstring  substring true
                            substring false
   read from file into string */
static
int xpost_op_file_readstring (Xpost_Context *ctx,
                              Xpost_Object F,
                              Xpost_Object S)
{
    int n;
    Xpost_File *f;
    char *s;
    if (!xpost_file_get_status(ctx->lo, F))
        return ioerror;
    if (!xpost_object_is_readable(ctx,F))
        return invalidaccess;
    f = xpost_file_get_file_pointer(ctx->lo, F);
    s = xpost_string_get_pointer(ctx, S);
    n = xpost_file_read(s, 1, S.comp_.sz, f);
    if (n == S.comp_.sz)
    {
        xpost_stack_push(ctx->lo, ctx->os, S);
        xpost_stack_push(ctx->lo, ctx->os, xpost_bool_cons(1));
    }
    else
    {
        S.comp_.sz = n;
        xpost_stack_push(ctx->lo, ctx->os, S);
        xpost_stack_push(ctx->lo, ctx->os, xpost_bool_cons(0));
    }
    return 0;
}

/* file string  writestring  -
   write string to file */
static
int xpost_op_file_writestring (Xpost_Context *ctx,
                               Xpost_Object F,
                               Xpost_Object S)
{
    Xpost_File *f;
    char *s;
    if (!xpost_file_get_status(ctx->lo, F))
        return ioerror;
    if (!xpost_object_is_writeable(ctx, F))
        return invalidaccess;
    f = xpost_file_get_file_pointer(ctx->lo, F);
    s = xpost_string_get_pointer(ctx, S);
    if (xpost_file_write(s, 1, S.comp_.sz, f) != S.comp_.sz)
        return ioerror;
    return 0;
}

/* file string  readline  substring true
                          substring false
   read a line of text from file */
static
int xpost_op_file_readline (Xpost_Context *ctx,
                            Xpost_Object F,
                            Xpost_Object S)
{
    Xpost_File *f;
    char *s;
    int n, c = ' ';
    if (!xpost_file_get_status(ctx->lo, F))
        return ioerror;
    if (!xpost_object_is_readable(ctx,F))
        return invalidaccess;
    f = xpost_file_get_file_pointer(ctx->lo, F);
    s = xpost_string_get_pointer(ctx, S);
    for (n = 0; n < S.comp_.sz; n++)
    {
        c = xpost_file_getc(f);
        if (c == EOF || c == '\n')
            break;
        s[n] = c;
    }
    if (n == S.comp_.sz && c != '\n')
        return rangecheck;
    S.comp_.sz = n;
    xpost_stack_push(ctx->lo, ctx->os, S);
    xpost_stack_push(ctx->lo, ctx->os, xpost_bool_cons(c != EOF));
    return 0;
}

/* file  bytesavailable  int
   return number of bytes available to read or -1 if not known */
static
int xpost_op_file_bytesavailable (Xpost_Context *ctx,
                                  Xpost_Object F)
{
    int bytes;
    int ret;
    ret = xpost_file_get_bytes_available(ctx->lo, F, &bytes);
    if (ret)
        return ret;
    xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons(bytes));
    return 0;
}

/* -  flush  -
   flush all output buffers */
static
int xpost_op_flush (Xpost_Context *ctx)
{
    int ret;
    (void)ctx;
    ret = fflush(NULL);
    if (ret != 0)
        return ioerror;
    return 0;
}

/* file  flushfile  -
   flush output buffer for file */
static
int xpost_op_file_flushfile (Xpost_Context *ctx,
                             Xpost_Object F)
{
    int ret;
    Xpost_File *f;
    if (!xpost_file_get_status(ctx->lo, F)) return 0;
    f = xpost_file_get_file_pointer(ctx->lo, F);
    if (xpost_object_is_writeable(ctx, F))
    {
        ret = xpost_file_flush(f);
        if (ret != 0)
            return ioerror;
    }
    else if (xpost_object_is_readable(ctx,F))
    { /* flush input file. yes yes I know ... but it's in the spec! */
        int c;
        while ((c = xpost_file_getc(f)) != EOF)
            /**/;
    }
    return 0;
}

#ifndef _WIN32

static
int xpost_op_file_resetfile (Xpost_Context *ctx,
                             Xpost_Object F)
{
    Xpost_File *f;
    if (!xpost_file_get_status(ctx->lo, F)) return 0;
    f = xpost_file_get_file_pointer(ctx->lo, F);
    xpost_file_purge(f);
    return 0;
}

#endif

/* file  status  bool
   return bool indicating whether file object is active or closed */
static
int xpost_op_file_status (Xpost_Context *ctx,
                          Xpost_Object F)
{
    xpost_stack_push(ctx->lo, ctx->os, xpost_bool_cons(xpost_file_get_status(ctx->lo, F)));
    return 0;
}

/* -  currentfile  file
   return topmost file from the exec stack */
static
int xpost_op_currentfile (Xpost_Context *ctx)
{
    int z = xpost_stack_count(ctx->lo, ctx->es);
    int i;
    Xpost_Object o;
    for (i = 0; i<z; i++)
    {
        o = xpost_stack_topdown_fetch(ctx->lo, ctx->es, i);
        if (xpost_object_get_type(o) == filetype)
        {
            xpost_stack_push(ctx->lo, ctx->os, o);
            return 0;
        }
    }
    o = xpost_file_cons(ctx->lo, NULL);
    if (xpost_object_get_type(o) == invalidtype)
        return VMerror;
    xpost_stack_push(ctx->lo, ctx->os, o);
    return 0;
}

/* string  deletefile  -
   delete named file from filesystem */
static
int xpost_op_string_deletefile (Xpost_Context *ctx,
                                Xpost_Object S)
{
    char *sbuf;
    int ret;

    sbuf = xpost_string_allocate_cstring(ctx, S);
    ret = remove(sbuf);
    if (ret != 0)
        switch (errno)
        {
            case ENOENT:
	      free(sbuf);
	      return undefinedfilename;
            default:
	      free(sbuf);
	      return ioerror;
        }
    free(sbuf);
    return 0;
}

/* old new  renamefile  -
   rename old file to new in filesystem */
static
int xpost_op_string_renamefile (Xpost_Context *ctx,
                                Xpost_Object Old,
                                Xpost_Object New)
{
    char *oldbuf, *newbuf;
    int ret;

    oldbuf = xpost_string_allocate_cstring(ctx, Old);

    newbuf = xpost_string_allocate_cstring(ctx, New);

    ret = rename(oldbuf, newbuf);
    if (ret != 0)
        switch(errno)
        {
            case ENOENT:
	      free(oldbuf);
	      free(newbuf);
	      return undefinedfilename;
            default:
	      free(oldbuf);
	      free(newbuf);
	      return ioerror;
        }
    free(oldbuf);
    free(newbuf);
    return 0;
}

//#ifndef _WIN32

/* internal continuation operator for filenameforall */
static
int xpost_op_contfilenameforall (Xpost_Context *ctx,
                                 Xpost_Object oglob,
                                 Xpost_Object Proc,
                                 Xpost_Object Scr)
{
    glob_t *globbuf;
    char *str;
    char *src;
    int len;
    Xpost_Object interval;

    globbuf = oglob.glob_.ptr;
    if (oglob.glob_.off < globbuf->gl_pathc)
    {
        /* xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons(ctx, "contfilenameforall", NULL,0,0)); */
        xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(ctx->opcode_shortcuts.contfilenameforall));
        xpost_stack_push(ctx->lo, ctx->es, Scr);
        /* xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons(ctx, "cvx", NULL,0,0)); */
        xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(ctx->opcode_shortcuts.cvx));
        xpost_stack_push(ctx->lo, ctx->es, xpost_object_cvlit(Proc));
        ++oglob.glob_.off;
        xpost_stack_push(ctx->lo, ctx->es, oglob);

        str = xpost_string_get_pointer(ctx, Scr);
        src = globbuf->gl_pathv[ oglob.glob_.off-1 ];
        len = strlen(src);
        if (len > Scr.comp_.sz)
            return rangecheck;
        memcpy(str, src, len);
        interval = xpost_object_get_interval(Scr, 0, len);
        if (xpost_object_get_type(interval) == invalidtype)
            return rangecheck;
        xpost_stack_push(ctx->lo, ctx->os, interval);
        xpost_stack_push(ctx->lo, ctx->es, Proc);

    }
    else
    {
        xpost_glob_free(globbuf); /* reference has already been popped */
    }
    return 0;
}

/* template proc scratch  filenameforall  -
   execute proc for all filenames matching template using scratch string */
static
int xpost_op_filenameforall (Xpost_Context *ctx,
                             Xpost_Object Tmp,
                             Xpost_Object Proc,
                             Xpost_Object Scr)
{
    char *tmpbuf;
    glob_t *globbuf;
    Xpost_Object oglob;
    int ret;

    tmpbuf = xpost_string_allocate_cstring(ctx, Tmp);
    globbuf = malloc(sizeof *globbuf);
    if (!globbuf){
        free(tmpbuf);
        return unregistered;
    }
    ret = xpost_glob(tmpbuf, globbuf);
    if (ret != 0)
    {
        free(tmpbuf);
        free(globbuf);
        return ioerror;
    }

    oglob.glob_.tag = globtype;
    oglob.glob_.off = 0;
    oglob.glob_.ptr = globbuf;

    xpost_op_contfilenameforall(ctx, oglob, Proc, xpost_object_cvlit(Scr));
    free(tmpbuf);
    return 0;
}

//#endif

/* file int  setfileposition  -
   set position of read/write head for file */
static
int xpost_op_setfileposition (Xpost_Context *ctx,
                              Xpost_Object F,
                              Xpost_Object pos)
{
    int ret = xpost_file_seek(xpost_file_get_file_pointer(ctx->lo, F), pos.int_.val);
    if (ret != 0)
        return ioerror;
    return 0;
}

/* file  fileposition  int
   return position of read/write head for file */
static
int xpost_op_fileposition (Xpost_Context *ctx,
                           Xpost_Object F)
{
    long pos;
    pos = xpost_file_tell(xpost_file_get_file_pointer(ctx->lo, F));
    if (pos == -1)
        return ioerror;
    else
        xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons(pos));
    return 0;
}

/* string  print  -
   write string to stdout */
static
int xpost_op_string_print (Xpost_Context *ctx,
                           Xpost_Object S)
{
    size_t ret;
    char *s;
    s = xpost_string_get_pointer(ctx, S);
    ret = fwrite(s, 1, S.comp_.sz, stdout);
    if (ret != S.comp_.sz)
        return ioerror;
    return 0;
}

/* bool  echo  -
   enable/disable terminal echoing of input characters */
static
int xpost_op_bool_echo (Xpost_Context *ctx,
                        Xpost_Object b)
{
    (void)ctx;
    if (b.int_.val)
        echoon(stdin);
    else
        echooff(stdin);
    return 0;
}

int xpost_oper_init_file_ops (Xpost_Context *ctx,
                              Xpost_Object sd)
{
    Xpost_Operator *optab;
    Xpost_Object n,op;
    unsigned int optadr;

    assert(ctx->gl->base);

    //xpost_memory_table_get_addr(ctx->gl, XPOST_MEMORY_TABLE_SPECIAL_OPERATOR_TABLE, &optadr);
    //optab = (void *)(ctx->gl->base + optadr);

    op = xpost_operator_cons(ctx, "file", (Xpost_Op_Func)xpost_op_string_mode_file, 1, 2, stringtype, stringtype);
    INSTALL;
    /* filter */
    op = xpost_operator_cons(ctx, "closefile", (Xpost_Op_Func)xpost_op_file_closefile, 0, 1, filetype);
    INSTALL;
    op = xpost_operator_cons(ctx, "read", (Xpost_Op_Func)xpost_op_file_read, 1, 1, filetype);
    INSTALL;
    op = xpost_operator_cons(ctx, "write", (Xpost_Op_Func)xpost_op_file_write, 0, 2, filetype, integertype);
    INSTALL;
    op = xpost_operator_cons(ctx, "readhexstring", (Xpost_Op_Func)xpost_op_file_readhexstring, 2, 2, filetype, stringtype);
    INSTALL;
    op = xpost_operator_cons(ctx, "writehexstring", (Xpost_Op_Func)xpost_op_file_writehexstring, 0, 2, filetype, stringtype);
    INSTALL;
    op = xpost_operator_cons(ctx, "readstring", (Xpost_Op_Func)xpost_op_file_readstring, 2, 2, filetype, stringtype);
    INSTALL;
    op = xpost_operator_cons(ctx, "writestring", (Xpost_Op_Func)xpost_op_file_writestring, 0, 2, filetype, stringtype);
    INSTALL;
    op = xpost_operator_cons(ctx, "readline", (Xpost_Op_Func)xpost_op_file_readline, 2, 2, filetype, stringtype);
    INSTALL;
    /* token: see optok.c */
    op = xpost_operator_cons(ctx, "bytesavailable", (Xpost_Op_Func)xpost_op_file_bytesavailable, 1, 1, filetype);
    INSTALL;
    op = xpost_operator_cons(ctx, "flush", (Xpost_Op_Func)xpost_op_flush, 0, 0);
    INSTALL;
    op = xpost_operator_cons(ctx, "flushfile", (Xpost_Op_Func)xpost_op_file_flushfile, 0, 1, filetype);
    INSTALL;
#ifndef _WIN32
    op = xpost_operator_cons(ctx, "resetfile", (Xpost_Op_Func)xpost_op_file_resetfile, 0, 1, filetype);
    INSTALL;
#endif
    op = xpost_operator_cons(ctx, "status", (Xpost_Op_Func)xpost_op_file_status, 1, 1, filetype);
    INSTALL;
    /* string status */
    /* run: see init.ps */
    op = xpost_operator_cons(ctx, "currentfile", (Xpost_Op_Func)xpost_op_currentfile, 1, 0);
    INSTALL;
    op = xpost_operator_cons(ctx, "deletefile", (Xpost_Op_Func)xpost_op_string_deletefile, 0, 1, stringtype);
    INSTALL;
    op = xpost_operator_cons(ctx, "renamefile", (Xpost_Op_Func)xpost_op_string_renamefile, 0, 2, stringtype, stringtype);
    INSTALL;
//#ifndef _WIN32
    op = xpost_operator_cons(ctx, "contfilenameforall", (Xpost_Op_Func)xpost_op_contfilenameforall, 0, 3, globtype, proctype, stringtype);
    ctx->opcode_shortcuts.contfilenameforall = op.mark_.padw;
    op = xpost_operator_cons(ctx, "filenameforall", (Xpost_Op_Func)xpost_op_filenameforall, 0, 3, stringtype, proctype, stringtype);
    INSTALL;
//#endif
    op = xpost_operator_cons(ctx, "setfileposition", (Xpost_Op_Func)xpost_op_setfileposition, 0, 2, filetype, integertype);
    INSTALL;
    op = xpost_operator_cons(ctx, "fileposition", (Xpost_Op_Func)xpost_op_fileposition, 1, 1, filetype);
    INSTALL;
    op = xpost_operator_cons(ctx, "print", (Xpost_Op_Func)xpost_op_string_print, 0, 1, stringtype);
    INSTALL;
    /* =: see init.ps
     * ==: see init.ps
     * stack: see init.ps
     * pstack: see init.ps
     * printobject
     * writeobject
     * setobjectformat
     * currentobjectformat */
    op = xpost_operator_cons(ctx, "echo", (Xpost_Op_Func)xpost_op_bool_echo, 0, 1, booleantype);
    INSTALL;

    /* xpost_dict_dump_memory (ctx->gl, sd); fflush(NULL);
    xpost_dict_put(ctx, sd, xpost_name_cons(ctx, "mark"), mark); */
    return 0;
}
