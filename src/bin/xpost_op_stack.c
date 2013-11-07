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
#include "xpost_interpreter.h"
#include "xpost_error.h"
#include "xpost_name.h"
#include "xpost_dict.h"
#include "xpost_operator.h"
#include "xpost_op_stack.h"


/* any  pop  -
   discard top element */
static
void Apop (context *ctx,
           Xpost_Object x)
{
    (void)ctx;
    (void)x;
}

/* any1 any2  exch  any2 any1
   exchange top two elements */
static
void AAexch (context *ctx,
             Xpost_Object x,
             Xpost_Object y)
{
    xpost_stack_push(ctx->lo, ctx->os, y);
    xpost_stack_push(ctx->lo, ctx->os, x);
}

/* any  dup  any any
   duplicate top element */
static
void Adup (context *ctx,
           Xpost_Object x)
{
    xpost_stack_push(ctx->lo, ctx->os, x);
    xpost_stack_push(ctx->lo, ctx->os, x);
}

/* any1..anyN N  copy  any1..anyN any1..anyN
   duplicate top n elements */
static
void Icopy (context *ctx,
            Xpost_Object n)
{
    int i;
    if (n.int_.val < 0) error(rangecheck, "Icopy");
    if (n.int_.val > xpost_stack_count(ctx->lo, ctx->os)) error(stackunderflow, "Icopy");
    for (i=0; i < n.int_.val; i++)
        xpost_stack_push(ctx->lo, ctx->os, xpost_stack_topdown_fetch(ctx->lo, ctx->os, n.int_.val - 1));
}

/* anyN..any0 N  index  anyN..any0 anyN
   duplicate arbitrary element */
static
void Iindex (context *ctx,
             Xpost_Object n)
{
    if (n.int_.val < 0) error(rangecheck, "Iindex");
    if (n.int_.val >= xpost_stack_count(ctx->lo, ctx->os)) error(stackunderflow, "Iindex");
    //printf("index %d\n", n.int_.val);
    xpost_stack_push(ctx->lo, ctx->os, xpost_stack_topdown_fetch(ctx->lo, ctx->os, n.int_.val));
}

/* a(n-1)..a(0) n j  roll  a((j-1)mod n)..a(0) a(n-1)..a(j mod n)
   roll n elements j times */
static
void IIroll (context *ctx,
             Xpost_Object N,
             Xpost_Object J)
{
    Xpost_Object *t;
    int i;
    int n = N.int_.val;
    int j = J.int_.val;
    if (n < 0) error(rangecheck, "IIroll");
    if (n == 0) return;
    if (j < 0) j = n - ( (- j) % n);
    j %= n;
    if (j == 0) return;
    
    t = alloca((n-j) * sizeof(Xpost_Object));
    for (i = 0; i < n-j; i++)
        t[i] = xpost_stack_topdown_fetch(ctx->lo, ctx->os, n - 1 - i);
    for (i = 0; i < j; i++)
        xpost_stack_topdown_replace(ctx->lo, ctx->os, n - 1 - i,
                xpost_stack_topdown_fetch(ctx->lo, ctx->os, j - 1 - i));
    for (i = 0; i < n-j; i++)
        xpost_stack_topdown_replace(ctx->lo, ctx->os, n - j - 1 - i, t[i]);
}

/* |- any1..anyN  clear  |-
   discard all elements */
static
void Zclear (context *ctx)
{
    Xpost_Stack *s = (void *)(ctx->lo->base + ctx->os);
    s->top = 0;
}

/* |- any1..anyN  count  |- any1..anyN N
   count elements on stack */
static
void Zcount (context *ctx)
{
    xpost_stack_push(ctx->lo, ctx->os, xpost_cons_int(xpost_stack_count(ctx->lo, ctx->os)));
}

/* -  mark  mark
   push mark on stack */
/* the name "mark" is defined in systemdict as a marktype object */

/* mark obj1..objN  cleartomark  -
   discard elements down through mark */
static
void Zcleartomark (context *ctx)
{
    Xpost_Object o;
    do {
        o = xpost_stack_pop(ctx->lo, ctx->os);
    } while (o.tag != marktype);
}

/* mark obj1..objN  counttomark  N
   count elements down to mark */
void Zcounttomark (context *ctx)
{
    unsigned i;
    unsigned z;
    z = xpost_stack_count(ctx->lo, ctx->os);
    for (i = 0; i < z; i++) {
        if (xpost_stack_topdown_fetch(ctx->lo, ctx->os, i).tag == marktype) {
            xpost_stack_push(ctx->lo, ctx->os, xpost_cons_int(i));
            return;
        }
    }
    error(unmatchedmark, "Zcounttomark");
}

/*
   -  currentcontext  context
   return current context identifier

   mark obj1..objN proc  fork  context
   create context executing proc with obj1..objN as operands

   context  join  mark obj1..objN
   await context termination and return its results

   context  detach  -
   enable context to terminate immediately when done

   -  lock  lock
   create lock object

   lock proc  monitor  -
   execute proc while holding lock

   -  condition  condition
   create condition object

   local condition  wait  -
   release lock, wait for condition, reacquire lock

   condition  notify  -
   resume contexts waiting for condition

   -  yield  -
   suspend current context momentarily
   */

void initops(context *ctx,
             Xpost_Object sd)
{
    oper *optab;
    Xpost_Object n,op;
    unsigned int optadr;

    assert(ctx->gl->base);
    xpost_memory_table_get_addr(ctx->gl,
            XPOST_MEMORY_TABLE_SPECIAL_OPERATOR_TABLE, &optadr);
    optab = (void *)(ctx->gl->base + optadr);
    op = consoper(ctx, "pop", Apop, 0, 1, anytype); INSTALL;
    op = consoper(ctx, "exch", AAexch, 2, 2, anytype, anytype); INSTALL;
    op = consoper(ctx, "dup", Adup, 2, 1, anytype); INSTALL;
    op = consoper(ctx, "copy", Icopy, 0, 1, integertype); INSTALL;
    op = consoper(ctx, "index", Iindex, 1, 1, integertype); INSTALL;
    //dumpdic(ctx->gl, sd); fflush(NULL);
    op = consoper(ctx, "roll", IIroll, 0, 2, integertype, integertype); INSTALL;
    op = consoper(ctx, "clear", Zclear, 0, 0); INSTALL;
    op = consoper(ctx, "count", Zcount, 1, 0); INSTALL;
    bdcput(ctx, sd, consname(ctx, "mark"), mark);
    op = consoper(ctx, "cleartomark", Zcleartomark, 0, 0); INSTALL;
    op = consoper(ctx, "counttomark", Zcounttomark, 1, 0); INSTALL;
}
