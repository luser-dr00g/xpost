#ifdef HAVE_CONFIG_H
# include <config.h>
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
             break;
    case -1: /* disable automatic collection in local vm */
             break;
    case 0: /* enable automatic collection */
             break;
    case 1: /* perform immediate collection in local vm */
             collect(ctx->lo);
             break;
    case 2: /* perform immediate collection in local and global vm */
             collect(ctx->gl);
             break;
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


