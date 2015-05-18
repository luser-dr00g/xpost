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

#define _USE_MATH_DEFINES /* needed for M_PI with Visual Studio */
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h> /* printf */
#include <stdlib.h> /* NULL */

//#define PI (4.0 * atan(1.0))
//double RAD_PER_DEG /* = PI / 180.0 */;
#define RAD_PER_DEG (M_PI / 180.0)

#include "xpost.h"
#include "xpost_compat.h"
#include "xpost_memory.h"
#include "xpost_object.h"
#include "xpost_stack.h"
#include "xpost_context.h"
#include "xpost_error.h"
#include "xpost_name.h"
#include "xpost_dict.h"

//#include "xpost_interpreter.h"
#include "xpost_operator.h"
#include "xpost_op_math.h"

static
int subwillunder(long x, long y);

static
int addwillover(long x,
                 long y)
{
    if (y == LONG_MIN) return x!=0;
    if (y < 0) return subwillunder(x, -y);
    if (x > LONG_MAX - y) return 1;
    return 0;
}

static
int subwillunder(long x,
                  long y)
{
    if (y == LONG_MIN) return 1;
    if (y < 0) return addwillover(x, -y);
    if (x < LONG_MIN + y) return 1;
    return 0;
}

static
int mulwillover(long x,
                 long y)
{
    if (x == 0||y == 0) return 0;
    if (x < 0) x = -x;
    if (y < 0) y = -y;
    if (x > LONG_MAX / y) return 1;
    return 0;
}

/* num1 num2  add  sum
   num1 plus num2 */
static
int Iadd (Xpost_Context *ctx,
           Xpost_Object x,
           Xpost_Object y)
{
    if (addwillover(x.int_.val, y.int_.val))
        xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons((real)x.int_.val + y.int_.val));
    else
        xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons(x.int_.val + y.int_.val));
    return 0;
}

static
int Radd (Xpost_Context *ctx,
           Xpost_Object x,
           Xpost_Object y)
{
    xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons(x.real_.val + y.real_.val));
    return 0;
}

/* num1 num2  div  quotient
   num1 divided by num2 */
static
int Rdiv (Xpost_Context *ctx,
           Xpost_Object x,
           Xpost_Object y)
{
    if (y.real_.val == 0.0)
        return undefinedresult;
    xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons(x.real_.val / y.real_.val));
    return 0;
}

/* num1 num2  idiv  quotient
   integer divide */
static
int Iidiv (Xpost_Context *ctx,
            Xpost_Object x,
            Xpost_Object y)
{
    if (y.int_.val == 0)
        return undefinedresult;
    xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons(x.int_.val / y.int_.val));
    return 0;
}

/* num1 num2  mod  remainder
   num1 mod num2 */
static
int Imod (Xpost_Context *ctx,
           Xpost_Object x,
           Xpost_Object y)
{
    xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons(x.int_.val % y.int_.val));
    return 0;
}

/* num1 num2  mul  product
   num1 times num2 */
static
int Imul (Xpost_Context *ctx,
           Xpost_Object x,
           Xpost_Object y)
{
    if (mulwillover(x.int_.val, y.int_.val))
        xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons((real)x.int_.val * y.int_.val));
    else
        xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons(x.int_.val * y.int_.val));
    return 0;
}

static
int Rmul (Xpost_Context *ctx,
           Xpost_Object x,
           Xpost_Object y)
{
    xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons(x.real_.val * y.real_.val));
    return 0;
}

/* num1 num2  sub  difference
   num1 minus num2 */
static
int Isub (Xpost_Context *ctx,
           Xpost_Object x,
           Xpost_Object y)
{
    if (subwillunder(x.int_.val, y.int_.val))
        xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons((real)x.int_.val - y.int_.val));
    else
        xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons(x.int_.val - y.int_.val));
    return 0;
}

static
int Rsub (Xpost_Context *ctx,
           Xpost_Object x,
           Xpost_Object y)
{
    xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons(x.real_.val - y.real_.val));
    return 0;
}

/* num1  abs  num2
   absolute value of num1 */
static
int Iabs (Xpost_Context *ctx,
           Xpost_Object x)
{
    if (x.int_.val == INT_MIN)
        xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons(- (real)INT_MIN));
    else
        xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons(x.int_.val>0? x.int_.val: -x.int_.val));
    return 0;
}

static
int Rabs (Xpost_Context *ctx,
           Xpost_Object x)
{
    xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons((real)fabs(x.real_.val)));
    return 0;
}

/* num1  neg  num2
   negative of num1 */
static
int Ineg (Xpost_Context *ctx,
           Xpost_Object x)
{
    if (x.int_.val == INT_MIN)
        xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons(- (real)INT_MIN));
    else
        xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons(-x.int_.val));
    return 0;
}

static
int Rneg (Xpost_Context *ctx,
           Xpost_Object x)
{
    xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons(-x.real_.val));
    return 0;
}

/* stub for integer  floor, ceiling, round, truncate */
static
int Istet (Xpost_Context *ctx,
            Xpost_Object x)
{
    xpost_stack_push(ctx->lo, ctx->os, x);
    return 0;
}

/* num1  ceiling  num2
   ceiling of num1 */
static
int Rceiling (Xpost_Context *ctx,
               Xpost_Object x)
{
    xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons((real)ceil(x.real_.val)));
    return 0;
}

/* num1  floor  num2
   floor of num1 */
static
int Rfloor (Xpost_Context *ctx,
             Xpost_Object x)
{
    xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons((real)floor(x.real_.val)));
    return 0;
}

/* num1  round  num2
   round num1 to nearest integer */
static
int Rround (Xpost_Context *ctx,
             Xpost_Object x)
{
    xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons((real)floor(x.real_.val + 0.5)));
#if 0
    if (x.real_.val > 0)
        xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons(round(x.real_.val)));
    else
        xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons(rint(x.real_.val)));
#endif
    return 0;
}

/* num1  truncate  num2
   remove fractional part of num1 */
static
int Rtruncate (Xpost_Context *ctx,
                Xpost_Object x)
{
    xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons((real)trunc(x.real_.val)));
    return 0;
}

/* num1  sqrt  num2
   square root of num1 */
static
int Rsqrt (Xpost_Context *ctx,
            Xpost_Object x)
{
    xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons((real)sqrt(x.real_.val)));
    return 0;
}

/* num den  atan  angle
   arctangent of num/den in degrees */
static
int Ratan (Xpost_Context *ctx,
            Xpost_Object num,
            Xpost_Object den)
{
    real ang = atan2((real)(num.real_.val * RAD_PER_DEG), (real)(den.real_.val * RAD_PER_DEG)) / RAD_PER_DEG;
    if (ang < 0.0) ang += 360.0;
    xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons(ang));
    return 0;
}

/* angle  cos  real
   cosine of angle (degrees) */
static
int Rcos (Xpost_Context *ctx,
           Xpost_Object x)
{
    xpost_stack_push(ctx->lo, ctx->os,
            xpost_real_cons((real)cos(RAD_PER_DEG * x.real_.val)));
    return 0;
}

/* angle  sin  real
   sine of angle (degrees) */
static
int Rsin (Xpost_Context *ctx,
           Xpost_Object x)
{
    xpost_stack_push(ctx->lo, ctx->os,
            xpost_real_cons((real)sin(RAD_PER_DEG * x.real_.val)));
    return 0;
}

/* base exponent  exp  real
   raise base to exponent power */
static
int Rexp (Xpost_Context *ctx,
           Xpost_Object base,
           Xpost_Object expn)
{
    if (base.real_.val < 0)
        expn.real_.val = (real)trunc(expn.real_.val);
    xpost_stack_push(ctx->lo, ctx->os,
            xpost_real_cons((real)pow(base.real_.val, expn.real_.val)));
    return 0;
}

/* num  ln  real
   natural logarithm of num */
static
int Rln (Xpost_Context *ctx,
          Xpost_Object x)
{
    xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons((real)log(x.real_.val)));
    return 0;
}

/* num  log  real
   logarithm (base 10) */
static
int Rlog (Xpost_Context *ctx,
           Xpost_Object x)
{
    xpost_stack_push(ctx->lo, ctx->es, xpost_real_cons((real)log10(x.real_.val)));
    return 0;
}

/* -  rand  int
   generate pseudo-random integer */
static
int Zrand (Xpost_Context *ctx)
{
    unsigned x;
    ctx->rand_next = ctx->rand_next * 1103515245 + 12345;
    x = ctx->rand_next << 16;
    ctx->rand_next = ctx->rand_next * 1103515245 + 12345;
    x |= ctx->rand_next & 0xffff;
    if (!xpost_stack_push(ctx->lo, ctx->es, xpost_int_cons(x & 0x7fffffff)))
        return stackoverflow;
    return 0;
}

/* int  srand  -
   set random number seed */
static
int Isrand (Xpost_Context *ctx,
             Xpost_Object seed)
{
    ctx->rand_next = seed.int_.val;
    return 0;
}

/* -  rrand  int
   return random number seed */
static
int Zrrand (Xpost_Context *ctx)
{
    if (!xpost_stack_push(ctx->lo, ctx->es, xpost_int_cons(ctx->rand_next)))
        return stackoverflow;
    return 0;
}

int xpost_oper_init_math_ops (Xpost_Context *ctx,
              Xpost_Object sd)
{
    Xpost_Operator *optab;
    Xpost_Object n,op;
    unsigned int optadr;

    assert(ctx->gl->base);
    xpost_memory_table_get_addr(ctx->gl,
            XPOST_MEMORY_TABLE_SPECIAL_OPERATOR_TABLE, &optadr);
    optab = (void *)(ctx->gl->base + optadr);
    //RAD_PER_DEG = PI / 180.0;

    op = xpost_operator_cons(ctx, "add", (Xpost_Op_Func)Iadd, 1, 2, integertype, integertype);
    INSTALL;
    op = xpost_operator_cons(ctx, "add", (Xpost_Op_Func)Radd, 1, 2, floattype, floattype);
    INSTALL;
    op = xpost_operator_cons(ctx, "sub", (Xpost_Op_Func)Isub, 1, 2, integertype, integertype);
    INSTALL;
    op = xpost_operator_cons(ctx, "sub", (Xpost_Op_Func)Rsub, 1, 2, floattype, floattype);
    INSTALL;
    op = xpost_operator_cons(ctx, "mul", (Xpost_Op_Func)Imul, 1, 2, integertype, integertype);
    INSTALL;
    op = xpost_operator_cons(ctx, "mul", (Xpost_Op_Func)Rmul, 1, 2, floattype, floattype);
    INSTALL;
    op = xpost_operator_cons(ctx, "idiv", (Xpost_Op_Func)Iidiv, 1, 2, integertype, integertype);
    INSTALL;
    op = xpost_operator_cons(ctx, "div", (Xpost_Op_Func)Rdiv, 1, 2, floattype, floattype);
    INSTALL;
    op = xpost_operator_cons(ctx, "mod", (Xpost_Op_Func)Imod, 1, 2, integertype, integertype);
    INSTALL;
    op = xpost_operator_cons(ctx, "abs", (Xpost_Op_Func)Iabs, 1, 1, integertype);
    INSTALL;
    op = xpost_operator_cons(ctx, "abs", (Xpost_Op_Func)Rabs, 1, 1, realtype);
    INSTALL;
    op = xpost_operator_cons(ctx, "neg", (Xpost_Op_Func)Ineg, 1, 1, integertype);
    INSTALL;
    op = xpost_operator_cons(ctx, "neg", (Xpost_Op_Func)Rneg, 1, 1, realtype);
    INSTALL;
    op = xpost_operator_cons(ctx, "floor", (Xpost_Op_Func)Istet, 1, 1, integertype);
    INSTALL;
    op = xpost_operator_cons(ctx, "floor", (Xpost_Op_Func)Rfloor, 1, 1, realtype);
    INSTALL;
    op = xpost_operator_cons(ctx, "ceiling", (Xpost_Op_Func)Istet, 1, 1, integertype);
    INSTALL;
    op = xpost_operator_cons(ctx, "ceiling", (Xpost_Op_Func)Rceiling, 1, 1, realtype);
    INSTALL;
    op = xpost_operator_cons(ctx, "round", (Xpost_Op_Func)Istet, 1, 1, integertype);
    INSTALL;
    op = xpost_operator_cons(ctx, "round", (Xpost_Op_Func)Rround, 1, 1, realtype);
    INSTALL;
    op = xpost_operator_cons(ctx, "truncate", (Xpost_Op_Func)Istet, 1, 1, integertype);
    INSTALL;
    op = xpost_operator_cons(ctx, "truncate", (Xpost_Op_Func)Rtruncate, 1, 1, realtype);
    INSTALL;
    op = xpost_operator_cons(ctx, "sqrt", (Xpost_Op_Func)Rsqrt, 1, 1, floattype);
    INSTALL;
    op = xpost_operator_cons(ctx, "atan", (Xpost_Op_Func)Ratan, 1, 2, floattype, floattype);
    INSTALL;
    op = xpost_operator_cons(ctx, "cos", (Xpost_Op_Func)Rcos, 1, 1, floattype);
    INSTALL;
    op = xpost_operator_cons(ctx, "sin", (Xpost_Op_Func)Rsin, 1, 1, floattype);
    INSTALL;
    op = xpost_operator_cons(ctx, "exp", (Xpost_Op_Func)Rexp, 1, 2, floattype, floattype);
    INSTALL;
    op = xpost_operator_cons(ctx, "ln", (Xpost_Op_Func)Rln, 1, 1, floattype);
    INSTALL;
    op = xpost_operator_cons(ctx, "log", (Xpost_Op_Func)Rlog, 1, 1, floattype);
    INSTALL;
    op = xpost_operator_cons(ctx, "rand", (Xpost_Op_Func)Zrand, 1, 0);
    INSTALL;
    op = xpost_operator_cons(ctx, "srand", (Xpost_Op_Func)Isrand, 0, 1, integertype);
    INSTALL;
    op = xpost_operator_cons(ctx, "rrand", (Xpost_Op_Func)Zrrand, 1, 0);
    INSTALL;

    /* op = xpost_operator_cons(ctx, "eq", (Xpost_Op_Func)Aeq, 1, 2, anytype, anytype);
       INSTALL;
    //xpost_dict_dump_memory (ctx->gl, sd); fflush(NULL);
    xpost_dict_put(ctx, sd, xpost_name_cons(ctx, "mark"), mark); */
    return 0;
}


