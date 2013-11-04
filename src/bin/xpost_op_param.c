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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h> /* NULL strtod */
#include <string.h>

#include "xpost_memory.h"
#include "xpost_object.h"
#include "xpost_stack.h"
#include "xpost_garbage.h"
#include "xpost_save.h"
#include "xpost_interpreter.h"
#include "xpost_name.h"
#include "xpost_string.h"
#include "xpost_dict.h"
#include "xpost_operator.h"
#include "xpost_error.h"
#include "xpost_op_param.h"

static
void vmreclaim (context *ctx, Xpost_Object I) {
    switch (I.int_.val) {
    default: error(rangecheck, "invalid argument");
    case -2: /* disable automatic collection in local and global vm */
             break;
    case -1: /* disable automatic collection in local vm */
             break;
    case 0: /* enable automatic collection */
             break;
    case 1: /* perform immediate collection in local vm */
             collect(ctx->lo, 1, 0);
             break;
    case 2: /* perform immediate collection in local and global vm */
             collect(ctx->gl, 1, 1);
             break;
    }
}

static
void vmstatus (context *ctx) {
    int lev, used, max;
    unsigned int vstk;

    xpost_memory_table_get_addr(ctx->lo,
            XPOST_MEMORY_TABLE_SPECIAL_SAVE_STACK, &vstk);
    lev = xpost_stack_count(ctx->lo, vstk);
    used = ctx->gl->used + ctx->lo->used;
    max = ctx->gl->max + ctx->lo->max;

    xpost_stack_push(ctx->lo, ctx->os, xpost_cons_int(lev));
    xpost_stack_push(ctx->lo, ctx->os, xpost_cons_int(used));
    xpost_stack_push(ctx->lo, ctx->os, xpost_cons_int(max));
}

void initopparam(context *ctx,
             Xpost_Object sd)
{
    oper *optab;
    Xpost_Object n,op;
    unsigned int optadr;

    assert(ctx->gl->base);
    xpost_memory_table_get_addr(ctx->gl,
            XPOST_MEMORY_TABLE_SPECIAL_OPERATOR_TABLE, &optadr);
    optab = (void *)(ctx->gl->base + optadr);

    op = consoper(ctx, "vmreclaim", vmreclaim, 0, 1, integertype); INSTALL;
    op = consoper(ctx, "vmstatus", vmstatus, 3, 0); INSTALL;

    /* dumpdic(ctx->gl, sd); fflush(NULL);
    op = consoper(ctx, "save", Zsave, 1, 0); INSTALL;
    bdcput(ctx, sd, consname(ctx, "mark"), mark); */

}


