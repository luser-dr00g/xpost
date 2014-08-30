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


#include "xpost_main.h"
#include "xpost_memory.h"
#include "xpost_object.h"
#include "xpost_stack.h"
#include "xpost_context.h"
#include "xpost_error.h"
#include "xpost_name.h"
#include "xpost_string.h"
#include "xpost_array.h"
#include "xpost_dict.h"

#include "xpost_interpreter.h"
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
        t = xpost_array_get(ctx, p, i);
        switch(xpost_object_get_type(t)){
        default: break;
        case nametype:
            z = xpost_stack_count(ctx->lo, ctx->ds);
            for (j = 0; j < z; j++) {
                d = xpost_stack_topdown_fetch(ctx->lo, ctx->ds, j);
                if (xpost_dict_known_key(ctx, xpost_context_select_memory(ctx,d), d, t)) {
                    t = xpost_dict_get(ctx, d, t);
                    if (xpost_object_get_type(t) == operatortype) {
                        xpost_array_put(ctx, p, i, t);
                    }
                    break;
                }
            }
            break;
        case arraytype:
            if (xpost_object_is_exe(t)) {
                t = bind(ctx, t);
                xpost_array_put(ctx, p, i, t);
            }
        }
    }
    return xpost_object_set_access(p, XPOST_OBJECT_TAG_ACCESS_READ_ONLY);
}

/* proc  bind  proc
   replace names with operators in proc and make read-only */
static
int Pbind (Xpost_Context *ctx,
            Xpost_Object P)
{
    xpost_stack_push(ctx->lo, ctx->os, bind(ctx, P));
    return 0;
}

/* -  realtime  int
   return real time in milliseconds */
static
int realtime (Xpost_Context *ctx)
{
    double sec;
    long long lsec;
#ifdef HAVE_GETTIMEOFDAY
    struct timeval tv;
    gettimeofday(&tv, NULL);
    sec = tv.tv_sec * 1000 + tv.tv_usec / 1000.0;
#else
    sec = time(NULL) * 1000;
#endif
    lsec = sec;
    lsec &= 0x00000000ffffffff; /* truncate any large value */
    if (!xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons((int)lsec)))
        return stackoverflow;
    return 0;
}

/* -  usertime  int
   return execution time in milliseconds */
static
int usertime (Xpost_Context *ctx)
{
    double sec;
    long long lsec;
#ifdef HAVE_GETTIMEOFDAY
    struct timeval tv;
    gettimeofday(&tv, NULL);
    sec = tv.tv_sec * 1000 + tv.tv_usec / 1000.0;
#else
    sec = time(NULL) * 1000;
#endif
    lsec = sec - xpost_start_time_get();
    lsec &= 0x00000000ffffffff; /* truncate any large value */
    if (!xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons((int)lsec)))
        return stackoverflow;
    return 0;
}

/* string  getenv  string
   return value for environment variable */
static
int Sgetenv (Xpost_Context *ctx,
              Xpost_Object S)
{
    char *s;
    char *str;
    char *r;
    s = xpost_string_get_pointer(ctx, S);
    str = alloca(S.comp_.sz + 1);
    memcpy(str, s, S.comp_.sz);
    str[S.comp_.sz] = '\0';
    r = getenv(str);
    if (r)
    {
        Xpost_Object strobj;
        strobj = xpost_string_cons(ctx, strlen(r), r);
        if (xpost_object_get_type(strobj) == nulltype)
            return VMerror;
        xpost_stack_push(ctx->lo, ctx->os, strobj);
    }
    else
    {
        return undefined;
    }
    return 0;
}

/* string string  putenv
   set value for environment variable */
static
int SSputenv (Xpost_Context *ctx,
              Xpost_Object N,
              Xpost_Object S)
{
    char *n, *s, *r;
    n = xpost_string_get_pointer(ctx, N);
    if (xpost_object_get_type(S) == nulltype) {
        s = "";
        r = alloca(N.comp_.sz + 1);
        memcpy(r, n, N.comp_.sz);
        r[N.comp_.sz] = '\0';
    } else {
        s = xpost_string_get_pointer(ctx, S);
        r = alloca(N.comp_.sz + 1 + S.comp_.sz + 1);
        memcpy(r, n, N.comp_.sz);
        r[N.comp_.sz] = '=';
        memcpy(r + N.comp_.sz + 1, s, S.comp_.sz);
        r[N.comp_.sz + 1 + S.comp_.sz] = '\0';
    }
    putenv(r);
    return 0;
}

static
int _array_swap(Xpost_Context *ctx,
                Xpost_Object a,
                Xpost_Object i,
                Xpost_Object j)
{
    Xpost_Object a_i, a_j;
    a_i = xpost_array_get(ctx, a, i.int_.val);
    a_j = xpost_array_get(ctx, a, j.int_.val);
    xpost_array_put(ctx, a, i.int_.val, a_j);
    xpost_array_put(ctx, a, j.int_.val, a_i);
    return 0;
}

static
int traceon (Xpost_Context *ctx)
{
    (void)ctx;
    TRACE = 1;
    return 0;
}
static
int traceoff (Xpost_Context *ctx)
{
    (void)ctx;
    TRACE = 0;
    return 0;
}

static
int debugloadon (Xpost_Context *ctx)
{
    (void)ctx;
    DEBUGLOAD = 1;
    return 0;
}
static
int debugloadoff (Xpost_Context *ctx)
{
    (void)ctx;
    DEBUGLOAD = 0;
    return 0;
}

static
int Odumpnames (Xpost_Context *ctx)
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
    return 0;
}

/*
FIXME: interaction with file dump mechanism ?
*/
static
int dumpvm (Xpost_Context *ctx)
{
    xpost_memory_file_dump(ctx->lo);
    xpost_memory_table_dump(ctx->lo);
    xpost_memory_file_dump(ctx->gl);
    xpost_memory_table_dump(ctx->gl);
    return 0;
}

int xpost_oper_init_misc_ops (Xpost_Context *ctx,
             Xpost_Object sd)
{
    Xpost_Operator *optab;
    Xpost_Object n,op;
    unsigned int optadr;

    char *productstr = "Xpost";
    char *versionstr = "0.0";
    int revno = 1;
    int serno = 0;

    assert(ctx->gl->base);
    xpost_memory_table_get_addr(ctx->gl,
            XPOST_MEMORY_TABLE_SPECIAL_OPERATOR_TABLE, &optadr);
    optab = (void *)(ctx->gl->base + optadr);

    op = xpost_operator_cons(ctx, "bind", Pbind, 1, 1, proctype);
    INSTALL;
    xpost_dict_put(ctx, sd, xpost_name_cons(ctx, "null"), null);
    xpost_dict_put(ctx, sd, xpost_name_cons(ctx, "version"),
            xpost_object_cvlit(xpost_string_cons(ctx, strlen(versionstr), versionstr)));
    op = xpost_operator_cons(ctx, "realtime", realtime, 1, 0);
    INSTALL;
    op = xpost_operator_cons(ctx, "usertime", usertime, 1, 0);
    INSTALL;
    //languagelevel
    xpost_dict_put(ctx, sd, xpost_name_cons(ctx, "product"),
            xpost_object_cvlit(xpost_string_cons(ctx, strlen(productstr), productstr)));
    xpost_dict_put(ctx, sd, xpost_name_cons(ctx, "revision"), xpost_int_cons(revno));
    xpost_dict_put(ctx, sd, xpost_name_cons(ctx, "serialnumber"), xpost_int_cons(serno));
    //executive: see init.ps
    //echo: see opf.c
    //prompt: see init.ps

    op = xpost_operator_cons(ctx, "getenv", Sgetenv, 1, 1, stringtype);
    INSTALL;
    op = xpost_operator_cons(ctx, "putenv", SSputenv, 0, 2, stringtype, stringtype);
    INSTALL;

    op = xpost_operator_cons(ctx, ".swap", _array_swap, 0, 3,
            arraytype, integertype, integertype);
    INSTALL;

    op = xpost_operator_cons(ctx, "traceon", traceon, 0, 0);
    INSTALL;
    op = xpost_operator_cons(ctx, "traceoff", traceoff, 0, 0);
    INSTALL;
    op = xpost_operator_cons(ctx, "debugloadon", debugloadon, 0, 0);
    INSTALL;
    op = xpost_operator_cons(ctx, "debugloadoff", debugloadoff, 0, 0);
    INSTALL;
    op = xpost_operator_cons(ctx, "dumpnames", Odumpnames, 0, 0);
    INSTALL;
    op = xpost_operator_cons(ctx, "dumpvm", dumpvm, 0, 0);
    INSTALL;

    /* xpost_dict_dump_memory (ctx->gl, sd); fflush(NULL);
    xpost_dict_put(ctx, sd, xpost_name_cons(ctx, "mark"), mark); */

    return 0;
}


