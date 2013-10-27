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

/* relational, boolean, and bitwise operators */

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
#include <stdlib.h> /* NULL */

#include "xpost_memory.h"
#include "xpost_object.h"
#include "xpost_stack.h"
#include "xpost_interpreter.h"
#include "xpost_name.h"
#include "xpost_dict.h"
#include "xpost_operator.h"
#include "xpost_op_boolean.h"

/* any1 any2  eq  bool
   test equal */
static
void Aeq (context *ctx,
          Xpost_Object x,
          Xpost_Object y)
{
    push(ctx->lo, ctx->os, xpost_cons_bool(objcmp(ctx,x,y) == 0));
}

/* any1 any2  ne  bool
   test not equal */
static
void Ane (context *ctx,
          Xpost_Object x,
          Xpost_Object y)
{
    push(ctx->lo, ctx->os, xpost_cons_bool(objcmp(ctx,x,y) != 0));
}

/* any1 any2  ge  bool
   test greater or equal */
static
void Age (context *ctx,
          Xpost_Object x,
          Xpost_Object y)
{
    push(ctx->lo, ctx->os, xpost_cons_bool(objcmp(ctx,x,y) >= 0));
}

/* any1 any2  gt  bool
   test greater than */
static
void Agt (context *ctx,
          Xpost_Object x,
          Xpost_Object y)
{
    push(ctx->lo, ctx->os, xpost_cons_bool(objcmp(ctx,x,y) > 0));
}

/* any1 any2  le  bool
   test less or equal */
static
void Ale (context *ctx,
          Xpost_Object x,
          Xpost_Object y)
{
    push(ctx->lo, ctx->os, xpost_cons_bool(objcmp(ctx,x,y) <= 0));
}

/* any1 any2  lt  bool
   test less than */
static
void Alt (context *ctx,
          Xpost_Object x,
          Xpost_Object y)
{
    push(ctx->lo, ctx->os, xpost_cons_bool(objcmp(ctx,x,y) < 0));
}

/* bool1|int1 bool2|int2  and  bool3|int3
   logical|bitwise and */
static
void Band (context *ctx,
           Xpost_Object x,
           Xpost_Object y)
{
    push(ctx->lo, ctx->os, xpost_cons_bool(x.int_.val & y.int_.val));
}

static
void Iand (context *ctx,
           Xpost_Object x,
           Xpost_Object y)
{
    push(ctx->lo, ctx->os, xpost_cons_int(x.int_.val & y.int_.val));
}

/* bool1|int1  not  bool2|int2
   logical|bitwise not */
static
void Bnot (context *ctx,
           Xpost_Object x)
{
    push(ctx->lo, ctx->os, xpost_cons_bool( ! x.int_.val ));
}

static
void Inot (context *ctx,
           Xpost_Object x)
{
    push(ctx->lo, ctx->os, xpost_cons_int( ~ x.int_.val ));
}

/* bool1|int1 bool2|int2  or  bool3|int3
   logical|bitwise inclusive or */
static
void Bor (context *ctx,
          Xpost_Object x,
          Xpost_Object y)
{
    push(ctx->lo, ctx->os, xpost_cons_bool(x.int_.val | y.int_.val));
}

static
void Ior (context *ctx,
          Xpost_Object x,
          Xpost_Object y)
{
    push(ctx->lo, ctx->os, xpost_cons_int(x.int_.val | y.int_.val));
}

/* bool1|int1 bool2|int2  xor  bool3|int3
   exclusive or */
static
void Bxor (context *ctx,
           Xpost_Object x,
           Xpost_Object y)
{
    push(ctx->lo, ctx->os, xpost_cons_bool(x.int_.val ^ y.int_.val));
}

static
void Ixor (context *ctx,
           Xpost_Object x,
           Xpost_Object y)
{
    push(ctx->lo, ctx->os, xpost_cons_int(x.int_.val ^ y.int_.val));
}

// true

// false

/* int1 shift  bitshift  int2
   bitwise shift of int1 (positive is left) */
static
void Ibitshift (context *ctx,
                Xpost_Object x,
                Xpost_Object y)
{
    if (y.int_.val >= 0)
        push(ctx->lo, ctx->os, xpost_cons_int(x.int_.val << y.int_.val));
    else
        push(ctx->lo, ctx->os, xpost_cons_int(
                    (unsigned long)x.int_.val >> -y.int_.val));
}

void initopb(context *ctx,
             Xpost_Object sd)
{
    oper *optab;
    Xpost_Object n,op;
    assert(ctx->gl->base);
    optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB));

    op = consoper(ctx, "eq", Aeq, 1, 2, anytype, anytype); INSTALL;
    op = consoper(ctx, "ne", Ane, 1, 2, anytype, anytype); INSTALL;
    op = consoper(ctx, "ge", Age, 1, 2, anytype, anytype); INSTALL;
    op = consoper(ctx, "gt", Agt, 1, 2, anytype, anytype); INSTALL;
    op = consoper(ctx, "le", Ale, 1, 2, anytype, anytype); INSTALL;
    op = consoper(ctx, "lt", Alt, 1, 2, anytype, anytype); INSTALL;
    op = consoper(ctx, "and", Band, 1, 2, booleantype, booleantype); INSTALL;
    op = consoper(ctx, "and", Iand, 1, 2, integertype, integertype); INSTALL;
    op = consoper(ctx, "not", Bnot, 1, 1, booleantype); INSTALL;
    op = consoper(ctx, "not", Inot, 1, 1, integertype); INSTALL;
    op = consoper(ctx, "or", Bor, 1, 2, booleantype, booleantype); INSTALL;
    op = consoper(ctx, "or", Ior, 1, 2, integertype, integertype); INSTALL;
    op = consoper(ctx, "xor", Bxor, 1, 2, booleantype, booleantype); INSTALL;
    op = consoper(ctx, "xor", Ixor, 1, 2, integertype, integertype); INSTALL;
    bdcput(ctx, sd, consname(ctx, "true"), xpost_cons_bool(true));
    bdcput(ctx, sd, consname(ctx, "false"), xpost_cons_bool(false));
    op = consoper(ctx, "bitshift", Ibitshift, 1, 2, integertype, integertype); INSTALL;

    /* dumpdic(ctx->gl, sd); fflush(NULL); */
}
