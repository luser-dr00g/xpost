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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h> /* NULL */

#include "xpost.h"
#include "xpost_memory.h"
#include "xpost_object.h"
#include "xpost_stack.h"
#include "xpost_context.h"
#include "xpost_error.h"
#include "xpost_name.h"
#include "xpost_dict.h"

//#include "xpost_interpreter.h"
#include "xpost_operator.h"
#include "xpost_op_stack.h"
#include "xpost_op_context.h"


/* -  currentcontext  context
   return current context identifier */
static
int xpost_op_currentcontext (Xpost_Context *ctx)
{
    Xpost_Object ctxobj;
    ctxobj.mark_.tag = contexttype;
    ctxobj.mark_.padw = ctx->id;
    xpost_stack_push(ctx->lo, ctx->os, ctxobj);
    return 0;
}

/*
   mark obj1..objN proc  fork  context
   create context executing proc with obj1..objN as operands
*/
static
int xpost_op_fork (Xpost_Context *ctx, Xpost_Object proc)
{
    int cid, n;
    Xpost_Context *newctx;

    cid = xpost_context_fork3(ctx,
                              ctx->xpost_interpreter_cid_init,
                              ctx->gl->interpreter_cid_get_context,
                              ctx->xpost_interpreter_alloc_local_memory,
                              ctx->xpost_interpreter_alloc_global_memory,
                              ctx->garbage_collect_function);
    newctx = ctx->gl->interpreter_cid_get_context(cid);
    printf("op_fork ctx->id %u, cid %u, newctx->id %u\n", ctx->id, cid, newctx->id);

    (void)xpost_op_counttomark(ctx);
    n = xpost_stack_pop(ctx->lo, ctx->os).int_.val;
    /* copy n objects to new context's operand stack */
    while (n--)
        xpost_stack_push(newctx->lo, newctx->os,
                         xpost_stack_topdown_fetch(ctx->lo, ctx->os, n));
    (void)xpost_op_cleartomark(ctx);

    xpost_stack_push(newctx->lo, newctx->es, xpost_operator_cons(newctx, "_i_am_zombie_", NULL,0,0));
    xpost_stack_push(newctx->lo, newctx->es, proc);
    //xpost_op_currentcontext(newctx);
    newctx->state = C_RUN;
    {
        Xpost_Object ctxobj;
        ctxobj.mark_.tag = contexttype;
        ctxobj.mark_.padw = newctx->id;
        xpost_stack_push(ctx->lo, ctx->os, ctxobj);
    }
    return contextswitch;
}

static
int _i_am_zombie_ (Xpost_Context *ctx)
{
    ctx->state = C_ZOMB;
    printf("I AM ZOMBIE\n");
    return contextswitch;
}

static
int _i_am_free_ (Xpost_Context *ctx)
{
    ctx->state = C_FREE;
    printf("I AM FREE\n");
    return contextswitch;
}

/*
   context  join  mark obj1..objN
   await context termination and return its results
*/
static
int xpost_op_join (Xpost_Context *ctx, Xpost_Object context)
{
    //(void)context;
    Xpost_Context *child = ctx->gl->interpreter_cid_get_context(context.mark_.padw);
    if (child->state == C_ZOMB) {
        int i,n;
        printf("found zombie child\n");
        xpost_stack_push(ctx->lo, ctx->os, mark);
        // Copy operand stack
        n = xpost_stack_count(child->lo, child->os);
        for (i = 0; i < n; i++)
            xpost_stack_push(ctx->lo, ctx->os,
                    xpost_stack_bottomup_fetch(child->lo, child->os, i));
        // Cleanup child
        child->state = C_FREE;
        return 0;
    }

    /* continue */
    printf("waiting for child %u ==%u\n", context.mark_.padw, child->state);
    xpost_stack_push(ctx->lo, ctx->os, context);
    xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons(ctx, "join", NULL,0,0));
    ctx->state = C_WAIT;
    return contextswitch;
}


/*
   -  yield  -
   suspend current context momentarily
*/
static
int xpost_op_yield (Xpost_Context *ctx)
{
    (void)ctx;
    return contextswitch;
}

/*
   context  detach  -
   enable context to terminate immediately when done
*/
static
int xpost_op_detach (Xpost_Context *ctx, Xpost_Object context)
{
    Xpost_Context *child = ctx->gl->interpreter_cid_get_context(context.mark_.padw);
    xpost_stack_bottomup_replace(child->lo, child->es, 0,
                                 xpost_operator_cons(child, "_i_am_free_", NULL,0,0));
    return contextswitch;
}

/*
   -  lock  lock
   create lock object

   lock proc  monitor  -
   execute proc while holding lock

   -  condition  condition
   create condition object

   lock condition  wait  -
   release lock, wait for condition, reacquire lock

   condition  notify  -
   resume contexts waiting for condition
*/

int xpost_oper_init_context_ops (Xpost_Context *ctx,
                                 Xpost_Object sd)
{
    Xpost_Operator *optab;
    Xpost_Object n,op;
    unsigned int optadr;

    assert(ctx->gl->base);
    xpost_memory_table_get_addr(ctx->gl,
            XPOST_MEMORY_TABLE_SPECIAL_OPERATOR_TABLE, &optadr);
    optab = (void *)(ctx->gl->base + optadr);
    //xpost_dict_dump_memory (ctx->gl, sd); fflush(NULL);
    op = xpost_operator_cons(ctx, "currentcontext", (Xpost_Op_Func)xpost_op_currentcontext, 1, 0);
    INSTALL;
    op = xpost_operator_cons(ctx, "fork", (Xpost_Op_Func)xpost_op_fork, 1, 1, proctype);
    INSTALL;
    op = xpost_operator_cons(ctx, "_i_am_zombie_", (Xpost_Op_Func)_i_am_zombie_, 0, 0);
    INSTALL;
    op = xpost_operator_cons(ctx, "_i_am_free_", (Xpost_Op_Func)_i_am_free_, 0, 0);
    INSTALL;
    op = xpost_operator_cons(ctx, "join", (Xpost_Op_Func)xpost_op_join, 1, 1, contexttype);
    INSTALL;
    op = xpost_operator_cons(ctx, "yield", (Xpost_Op_Func)xpost_op_yield, 0, 0);
    INSTALL;
    op = xpost_operator_cons(ctx, "detach", (Xpost_Op_Func)xpost_op_detach, 0, 1, contexttype);
    INSTALL;
    //xpost_dict_put(ctx, sd, xpost_name_cons(ctx, "mark"), mark);
    //op = xpost_operator_cons(ctx, "counttomark", (Xpost_Op_Func)xpost_op_counttomark, 1, 0); INSTALL;
    return 0;
}
