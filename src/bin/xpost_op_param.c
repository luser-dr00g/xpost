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

#include "xpost_memory.h"
#include "xpost_object.h"
#include "xpost_stack.h"
#include "xpost_garbage.h"
#include "xpost_save.h"
#include "itp.h"
#include "xpost_name.h"
#include "xpost_string.h"
#include "xpost_dict.h"
#include "xpost_operator.h"
#include "xpost_error.h"
#include "xpost_op_param.h"

static
void vmreclaim (context *ctx, object I) {
    switch (I.int_.val) {
    default: error(rangecheck, "invalid argument");
    case -2: /* disable automatic collection in local and global vm */
    case -1: /* disable automatic collection in local vm */
    case 0: /* enable automatic collection */
    case 1: /* perform immediate collection in local vm */
    case 2: /* perform immediate collection in local and global vm */
             ;
    }
}

static
void vmstatus (context *ctx) {
    int lev, used, max;

    lev = count(ctx->lo, adrent(ctx->lo, VS));
    used = ctx->gl->used + ctx->lo->used;
    max = ctx->gl->max + ctx->lo->max;

    push(ctx->lo, ctx->os, consint(lev));
    push(ctx->lo, ctx->os, consint(used));
    push(ctx->lo, ctx->os, consint(max));
}

void initopparam(context *ctx,
             object sd)
{
    oper *optab;
    object n,op;
    assert(ctx->gl->base);
    optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB));

    op = consoper(ctx, "vmreclaim", vmreclaim, 0, 1, integertype); INSTALL;
    op = consoper(ctx, "vmstatus", vmstatus, 3, 0); INSTALL;

    /* dumpdic(ctx->gl, sd); fflush(NULL);
    op = consoper(ctx, "save", Zsave, 1, 0); INSTALL;
    bdcput(ctx, sd, consname(ctx, "mark"), mark); */

}


