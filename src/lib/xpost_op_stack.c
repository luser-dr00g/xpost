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
#include <stdlib.h> /* NULL */

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


/* any  pop  -
   discard top element */
static
int Apop (Xpost_Context *ctx,
           Xpost_Object x)
{
    (void)ctx;
    (void)x;
    return 0;
}

/* any1 any2  exch  any2 any1
   exchange top two elements */
static
int AAexch (Xpost_Context *ctx,
             Xpost_Object x,
             Xpost_Object y)
{
    xpost_stack_push(ctx->lo, ctx->os, y);
    xpost_stack_push(ctx->lo, ctx->os, x);
    return 0;
}

/* any  dup  any any
   duplicate top element */
static
int Adup (Xpost_Context *ctx,
           Xpost_Object x)
{
    xpost_stack_push(ctx->lo, ctx->os, x);
    if (!xpost_stack_push(ctx->lo, ctx->os, x))
        return stackoverflow;
    return 0;
}

/* any1..anyN N  copy  any1..anyN any1..anyN
   duplicate top n elements */
static
int Icopy (Xpost_Context *ctx,
            Xpost_Object n)
{
    int i;
    if (n.int_.val < 0)
        return rangecheck;
    if (n.int_.val > xpost_stack_count(ctx->lo, ctx->os))
        return stackunderflow;
    for (i=0; i < n.int_.val; i++)
        if (!xpost_stack_push(ctx->lo, ctx->os,
                xpost_stack_topdown_fetch(ctx->lo, ctx->os, n.int_.val - 1)))
            return stackoverflow;
    return 0;
}

/* anyN..any0 N  index  anyN..any0 anyN
   duplicate arbitrary element */
static
int Iindex (Xpost_Context *ctx,
             Xpost_Object n)
{
    if (n.int_.val < 0)
        return rangecheck;
    if (n.int_.val >= xpost_stack_count(ctx->lo, ctx->os))
        return stackunderflow;
    //printf("index %d\n", n.int_.val);
    if (!xpost_stack_push(ctx->lo, ctx->os,
                xpost_stack_topdown_fetch(ctx->lo, ctx->os, n.int_.val)))
        return stackoverflow;
    return 0;
}

/* a(n-1)..a(0) n j  roll  a((j-1)mod n)..a(0) a(n-1)..a(j mod n)
   roll n elements j times */
static
int IIroll (Xpost_Context *ctx,
             Xpost_Object N,
             Xpost_Object J)
{
    Xpost_Object *t;
    Xpost_Object r;
    int i;
    int n = N.int_.val;
    int j = J.int_.val;
    if (n < 0)
        return rangecheck;
    if (n == 0) return 0;
    if (j < 0) j = n - ( (- j) % n);
    j %= n;
    if (j == 0) return 0;
    
    t = alloca((n-j) * sizeof(Xpost_Object));
    for (i = 0; i < n-j; i++)
    {
        r = xpost_stack_topdown_fetch(ctx->lo, ctx->os, n - 1 - i);
        if (xpost_object_get_type(r) == invalidtype)
            return stackunderflow;
        t[i] = r;
    }
    for (i = 0; i < j; i++)
    {
        r = xpost_stack_topdown_fetch(ctx->lo, ctx->os, j - 1 - i);
        if (xpost_object_get_type(r) == invalidtype)
            return stackunderflow;
        if (!xpost_stack_topdown_replace(ctx->lo, ctx->os, n - 1 - i, r))
            return stackunderflow;
    }
    for (i = 0; i < n-j; i++)
    {
        if (!xpost_stack_topdown_replace(ctx->lo, ctx->os, n - j - 1 - i, t[i]))
            return stackunderflow;
    }
    return 0;
}

/* |- any1..anyN  clear  |-
   discard all elements */
static
int Zclear (Xpost_Context *ctx)
{
    Xpost_Stack *s = (void *)(ctx->lo->base + ctx->os);
    s->top = 0;
#if 0
    if (s->nextseg) /* trim the stack */
    {
        xpost_stack_free(ctx->lo, s->nextseg);
        s->nextseg = 0;
    }
#endif
    return 0;
}

/* |- any1..anyN  count  |- any1..anyN N
   count elements on stack */
static
int Zcount (Xpost_Context *ctx)
{
    if (!xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons(xpost_stack_count(ctx->lo, ctx->os))))
        return stackoverflow;
    return 0;
}

/* -  mark  mark
   push mark on stack */
/* the name "mark" is defined in systemdict as a marktype object */

/* mark obj1..objN  cleartomark  -
   discard elements down through mark */
int xpost_op_cleartomark (Xpost_Context *ctx)
{
    Xpost_Object o;
    do {
        o = xpost_stack_pop(ctx->lo, ctx->os);
        if (xpost_object_get_type(o) == invalidtype)
            return unmatchedmark;
    } while (o.tag != marktype);
    return 0;
}

/* mark obj1..objN  counttomark  N
   count elements down to mark */
int xpost_op_counttomark (Xpost_Context *ctx)
{
    unsigned i;
    unsigned z;
    z = xpost_stack_count(ctx->lo, ctx->os);
    for (i = 0; i < z; i++) {
        if (xpost_stack_topdown_fetch(ctx->lo, ctx->os, i).tag == marktype) {
            xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons(i));
            return 0;
        }
    }
    return unmatchedmark;
}

int xpost_oper_init_stack_ops (Xpost_Context *ctx,
             Xpost_Object sd)
{
    Xpost_Operator *optab;
    Xpost_Object n,op;
    unsigned int optadr;

    assert(ctx->gl->base);
    xpost_memory_table_get_addr(ctx->gl,
            XPOST_MEMORY_TABLE_SPECIAL_OPERATOR_TABLE, &optadr);
    optab = (void *)(ctx->gl->base + optadr);
    op = xpost_operator_cons(ctx, "pop", (Xpost_Op_Func)Apop, 0, 1, anytype);
    INSTALL;
    op = xpost_operator_cons(ctx, "exch", (Xpost_Op_Func)AAexch, 2, 2, anytype, anytype);
    INSTALL;
    op = xpost_operator_cons(ctx, "dup", (Xpost_Op_Func)Adup, 2, 1, anytype);
    INSTALL;
    op = xpost_operator_cons(ctx, "copy", (Xpost_Op_Func)Icopy, 0, 1, integertype);
    INSTALL;
    op = xpost_operator_cons(ctx, "index", (Xpost_Op_Func)Iindex, 1, 1, integertype);
    INSTALL;
    //xpost_dict_dump_memory (ctx->gl, sd); fflush(NULL);
    op = xpost_operator_cons(ctx, "roll", (Xpost_Op_Func)IIroll, 0, 2, integertype, integertype);
    INSTALL;
    op = xpost_operator_cons(ctx, "clear", (Xpost_Op_Func)Zclear, 0, 0);
    INSTALL;
    op = xpost_operator_cons(ctx, "count", (Xpost_Op_Func)Zcount, 1, 0);
    INSTALL;
    xpost_dict_put(ctx, sd, xpost_name_cons(ctx, "mark"), mark);
    op = xpost_operator_cons(ctx, "cleartomark", (Xpost_Op_Func)xpost_op_cleartomark, 0, 0);
    INSTALL;
    op = xpost_operator_cons(ctx, "counttomark", (Xpost_Op_Func)xpost_op_counttomark, 1, 0);
    INSTALL;
    return 0;
}
