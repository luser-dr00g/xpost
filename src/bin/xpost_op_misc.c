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
#include <stdio.h>
#include <stdlib.h> /* NULL strtod */
#include <string.h>

#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif


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
#include "xpost_op_dict.h"
#include "xpost_op_misc.h"

static
Xpost_Object bind (Xpost_Context *ctx,
             Xpost_Object p)
{
    Xpost_Object t, d;
    int i, j, z;
    for (i = 0; i < p.comp_.sz; i++) {
        t = barget(ctx, p, i);
        switch(xpost_object_get_type(t)){
        default: break;
        case nametype:
            z = xpost_stack_count(ctx->lo, ctx->ds);
            for (j = 0; j < z; j++) {
                d = xpost_stack_topdown_fetch(ctx->lo, ctx->ds, j);
                if (dicknown(ctx, xpost_context_select_memory(ctx,d), d, t)) {
                    t = bdcget(ctx, d, t);
                    if (xpost_object_get_type(t) == operatortype) {
                        barput(ctx, p, i, t);
                    }
                    break;
                }
            }
            break;
        case arraytype:
            if (xpost_object_is_exe(t)) {
                t = bind(ctx, t);
                barput(ctx, p, i, t);
            }
        }
    }
    return xpost_object_set_access(p, XPOST_OBJECT_TAG_ACCESS_READ_ONLY);
}

static
void Pbind (Xpost_Context *ctx,
            Xpost_Object P)
{
    xpost_stack_push(ctx->lo, ctx->os, bind(ctx, P));
}

static
void realtime (Xpost_Context *ctx)
{
    double sec;
#ifdef HAVE_GETTIMEOFDAY
        struct timeval tv;
        gettimeofday(&tv, NULL);
        sec = tv.tv_sec * 1000 + tv.tv_usec / 1000.0;
#else
        sec = time(NULL) * 1000;
#endif
    xpost_stack_push(ctx->lo, ctx->os, xpost_cons_int(sec));
}

static
void Sgetenv (Xpost_Context *ctx,
              Xpost_Object S)
{
    char *s;
    char *str;
    char *r;
    s = charstr(ctx, S);
    str = alloca(S.comp_.sz + 1);
    memcpy(str, s, S.comp_.sz);
    str[S.comp_.sz] = '\0';
    r = getenv(str);
    if (r)
        xpost_stack_push(ctx->lo, ctx->os, consbst(ctx, strlen(r), r));
    else
        error(undefined, "getenv returned NULL");
}

static
void SSputenv (Xpost_Context *ctx,
              Xpost_Object N,
              Xpost_Object S)
{
    char *n, *s, *r;
    n = charstr(ctx, N);
    if (xpost_object_get_type(S) == nulltype) {
        s = "";
        r = alloca(N.comp_.sz + 1);
        memcpy(r, n, N.comp_.sz);
        r[N.comp_.sz] = '\0';
    } else {
        s = charstr(ctx, S);
        r = alloca(N.comp_.sz + 1 + S.comp_.sz + 1);
        memcpy(r, n, N.comp_.sz);
        r[N.comp_.sz] = '=';
        memcpy(r + N.comp_.sz + 1, s, S.comp_.sz);
        r[N.comp_.sz + 1 + S.comp_.sz] = '\0';
    }
    putenv(r);
}

static
void traceon (Xpost_Context *ctx)
{
    (void)ctx;
    TRACE = 1;
}
static
void traceoff (Xpost_Context *ctx)
{
    (void)ctx;
    TRACE = 0;
}

static
void debugloadon (Xpost_Context *ctx)
{
    (void)ctx;
    DEBUGLOAD = 1;
}
static
void debugloadoff (Xpost_Context *ctx)
{
    (void)ctx;
    DEBUGLOAD = 0;
}

static
void Odumpnames (Xpost_Context *ctx)
{
    unsigned int names;
    printf("\nGlobal Name stack: ");
    xpost_memory_table_get_addr(ctx->gl,
            XPOST_MEMORY_TABLE_SPECIAL_NAME_STACK, &names);
    xpost_stack_dump(ctx->gl, names);
    (void)puts("");
    printf("\nLocal Name stack: ");
    xpost_memory_table_get_addr(ctx->lo,
            XPOST_MEMORY_TABLE_SPECIAL_NAME_STACK, &names);
    xpost_stack_dump(ctx->lo, names);
    (void)puts("");
}

static
void dumpvm (Xpost_Context *ctx)
{
    xpost_memory_file_dump(ctx->lo);
    xpost_memory_table_dump(ctx->lo);
    xpost_memory_file_dump(ctx->gl);
    xpost_memory_table_dump(ctx->gl);
}

void initopx(Xpost_Context *ctx,
             Xpost_Object sd)
{
    oper *optab;
    Xpost_Object n,op;
    unsigned int optadr;

    assert(ctx->gl->base);
    xpost_memory_table_get_addr(ctx->gl,
            XPOST_MEMORY_TABLE_SPECIAL_OPERATOR_TABLE, &optadr);
    optab = (void *)(ctx->gl->base + optadr);

    op = consoper(ctx, "bind", Pbind, 1, 1, proctype); INSTALL;
    bdcput(ctx, sd, consname(ctx, "null"), null);
    //version: see init.ps
    op = consoper(ctx, "realtime", realtime, 1, 0); INSTALL;
    //usertime
    //languagelevel
    //product: see init.ps (Xpost3)
    //revision
    //serialnumber
    //executive: see init.ps
    //echo: see opf.c
    //prompt: see init.ps

    op = consoper(ctx, "getenv", Sgetenv, 1, 1, stringtype); INSTALL;
    op = consoper(ctx, "putenv", SSputenv, 0, 2, stringtype, stringtype); INSTALL;

    op = consoper(ctx, "traceon", traceon, 0, 0); INSTALL;
    op = consoper(ctx, "traceoff", traceoff, 0, 0); INSTALL;
    op = consoper(ctx, "debugloadon", debugloadon, 0, 0); INSTALL;
    op = consoper(ctx, "debugloadoff", debugloadoff, 0, 0); INSTALL;
    op = consoper(ctx, "dumpnames", Odumpnames, 0, 0); INSTALL;
    op = consoper(ctx, "dumpvm", dumpvm, 0, 0); INSTALL;

    /* dumpdic(ctx->gl, sd); fflush(NULL);
    bdcput(ctx, sd, consname(ctx, "mark"), mark); */

}


