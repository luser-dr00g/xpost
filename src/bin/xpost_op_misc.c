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
Xpost_Object bind (context *ctx,
             Xpost_Object p)
{
    Xpost_Object t, d;
    int i, j, z;
    for (i = 0; i < p.comp_.sz; i++) {
        t = barget(ctx, p, i);
        switch(xpost_object_get_type(t)){
        default: break;
        case nametype:
            z = count(ctx->lo, ctx->ds);
            for (j = 0; j < z; j++) {
                d = top(ctx->lo, ctx->ds, j);
                if (dicknown(ctx, bank(ctx,d), d, t)) {
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
void Pbind (context *ctx,
            Xpost_Object P)
{
    push(ctx->lo, ctx->os, bind(ctx, P));
}

static
void realtime (context *ctx)
{
    double sec;
#ifdef HAVE_GETTIMEOFDAY
        struct timeval tv;
        gettimeofday(&tv, NULL);
        sec = tv.tv_sec * 1000 + tv.tv_usec / 1000.0;
#else
        sec = time(NULL) * 1000;
#endif
    push(ctx->lo, ctx->os, xpost_cons_int(sec));
}

static
void Sgetenv (context *ctx,
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
        push(ctx->lo, ctx->os, consbst(ctx, strlen(r), r));
    else
        error(undefined, "getenv returned NULL");
}

static
void SSputenv (context *ctx,
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
void traceon (context *ctx)
{
    (void)ctx;
    TRACE = 1;
}
static
void traceoff (context *ctx)
{
    (void)ctx;
    TRACE = 0;
}

static
void debugloadon (context *ctx)
{
    (void)ctx;
    DEBUGLOAD = 1;
}
static
void debugloadoff (context *ctx)
{
    (void)ctx;
    DEBUGLOAD = 0;
}

static
void Odumpnames (context *ctx)
{
    printf("\nGlobal Name stack: ");
    dumpstack(ctx->gl, adrent(ctx->gl, NAMES));
    (void)puts("");
    printf("\nLocal Name stack: ");
    dumpstack(ctx->lo, adrent(ctx->lo, NAMES));
    (void)puts("");
}

static
void dumpvm (context *ctx)
{
    dumpmfile(ctx->lo);
    dumpmtab(ctx->lo, 0);
    dumpmfile(ctx->gl);
    dumpmtab(ctx->gl, 0);
}

void initopx(context *ctx,
             Xpost_Object sd)
{
    oper *optab;
    Xpost_Object n,op;
    assert(ctx->gl->base);
    optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB));

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


