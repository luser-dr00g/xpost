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
#include <limits.h>
#include <math.h>
#include <stdio.h> /* printf */
#include <stdlib.h> /* NULL */

//#define PI (4.0 * atan(1.0))
//double RAD_PER_DEG /* = PI / 180.0 */;
#define RAD_PER_DEG (M_PI / 180.0)

#include "xpost_memory.h"
#include "xpost_object.h"
#include "xpost_stack.h"
#include "xpost_interpreter.h"
#include "xpost_name.h"
#include "xpost_dict.h"
#include "xpost_operator.h"
#include "xpost_op_math.h"

static
int subwillunder(long x, long y);

static
int addwillover(long x,
                 long y)
{
    if (y < 0) return subwillunder(x, -y);
    if (x > LONG_MAX - y) return 1;
    if (y == LONG_MIN) return 1;
    return 0;
}

static
int subwillunder(long x,
                  long y)
{
    if (y < 0) return addwillover(x, -y);
    if (x < LONG_MIN + y) return 1;
    if (y == LONG_MIN) return 1;
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
void Iadd (context *ctx,
           Xpost_Object x,
           Xpost_Object y)
{
    if (addwillover(x.int_.val, y.int_.val))
        xpost_stack_push(ctx->lo, ctx->os, xpost_cons_real((real)x.int_.val + y.int_.val));
    else
        xpost_stack_push(ctx->lo, ctx->os, xpost_cons_int(x.int_.val + y.int_.val));
}

static
void Radd (context *ctx,
           Xpost_Object x,
           Xpost_Object y)
{
    xpost_stack_push(ctx->lo, ctx->os, xpost_cons_real(x.real_.val + y.real_.val));
}

/* num1 num2  div  quotient
   num1 divided by num2 */
static
void Rdiv (context *ctx,
           Xpost_Object x,
           Xpost_Object y)
{
    xpost_stack_push(ctx->lo, ctx->os, xpost_cons_real(x.real_.val / y.real_.val));
}

/* num1 num2  idiv  quotient
   integer divide */
static
void Iidiv (context *ctx,
            Xpost_Object x,
            Xpost_Object y)
{
    xpost_stack_push(ctx->lo, ctx->os, xpost_cons_int(x.int_.val / y.int_.val));
}

/* num1 num2  mod  remainder
   num1 mod num2 */
static
void Imod (context *ctx,
           Xpost_Object x,
           Xpost_Object y)
{
    xpost_stack_push(ctx->lo, ctx->os, xpost_cons_int(x.int_.val % y.int_.val));
}

/* num1 num2  mul  product
   num1 times num2 */
static
void Imul (context *ctx,
           Xpost_Object x,
           Xpost_Object y)
{
    if (mulwillover(x.int_.val, y.int_.val))
        xpost_stack_push(ctx->lo, ctx->os, xpost_cons_real((real)x.int_.val * y.int_.val));
    else
        xpost_stack_push(ctx->lo, ctx->os, xpost_cons_int(x.int_.val * y.int_.val));
}

static
void Rmul (context *ctx,
           Xpost_Object x,
           Xpost_Object y)
{
    xpost_stack_push(ctx->lo, ctx->os, xpost_cons_real(x.real_.val * y.real_.val));
}

/* num1 num2  sub  difference
   num1 minus num2 */
static
void Isub (context *ctx,
           Xpost_Object x,
           Xpost_Object y)
{
    if (subwillunder(x.int_.val, y.int_.val))
        xpost_stack_push(ctx->lo, ctx->os, xpost_cons_real((real)x.int_.val - y.int_.val));
    else
        xpost_stack_push(ctx->lo, ctx->os, xpost_cons_int(x.int_.val - y.int_.val));
}

static
void Rsub (context *ctx,
           Xpost_Object x,
           Xpost_Object y)
{
    xpost_stack_push(ctx->lo, ctx->os, xpost_cons_real(x.real_.val - y.real_.val));
}

/* num1  abs  num2
   absolute value of num1 */
static
void Iabs (context *ctx,
           Xpost_Object x)
{
    if (x.int_.val == INT_MIN)
        xpost_stack_push(ctx->lo, ctx->os, xpost_cons_real(- (real)INT_MIN));
    else
        xpost_stack_push(ctx->lo, ctx->os, xpost_cons_int(x.int_.val>0? x.int_.val: -x.int_.val));
}

static
void Rabs (context *ctx,
           Xpost_Object x)
{
    xpost_stack_push(ctx->lo, ctx->os, xpost_cons_real(fabs(x.real_.val)));
}

/* num1  neg  num2
   negative of num1 */
static
void Ineg (context *ctx,
           Xpost_Object x)
{
    if (x.int_.val == INT_MIN)
        xpost_stack_push(ctx->lo, ctx->os, xpost_cons_real(- (real)INT_MIN));
    else
        xpost_stack_push(ctx->lo, ctx->os, xpost_cons_int(-x.int_.val));
}

static
void Rneg (context *ctx,
           Xpost_Object x)
{
    xpost_stack_push(ctx->lo, ctx->os, xpost_cons_real(-x.real_.val));
}

/* stub for integer  floor, ceiling, round, truncate */
static
void Istet (context *ctx,
            Xpost_Object x)
{
    xpost_stack_push(ctx->lo, ctx->os, x);
}

/* num1  ceiling  num2
   ceiling of num1 */
static
void Rceiling (context *ctx,
               Xpost_Object x)
{
    xpost_stack_push(ctx->lo, ctx->os, xpost_cons_real(ceil(x.real_.val)));
}

/* num1  floor  num2
   floor of num1 */
static
void Rfloor (context *ctx,
             Xpost_Object x)
{
    xpost_stack_push(ctx->lo, ctx->os, xpost_cons_real(floor(x.real_.val)));
}

/* num1  round  num2
   round num1 to nearest integer */
static
void Rround (context *ctx,
             Xpost_Object x)
{
    xpost_stack_push(ctx->lo, ctx->os, xpost_cons_real(floor(x.real_.val + 0.5)));
#if 0
    if (x.real_.val > 0)
        xpost_stack_push(ctx->lo, ctx->os, xpost_cons_real(round(x.real_.val)));
    else
        xpost_stack_push(ctx->lo, ctx->os, xpost_cons_real(rint(x.real_.val)));
#endif
}

/* num1  truncate  num2
   remove fractional part of num1 */
static
void Rtruncate (context *ctx,
                Xpost_Object x)
{
    xpost_stack_push(ctx->lo, ctx->os, xpost_cons_real(trunc(x.real_.val)));
}

/* num1  sqrt  num2
   square root of num1 */
static
void Rsqrt (context *ctx,
            Xpost_Object x)
{
    xpost_stack_push(ctx->lo, ctx->os, xpost_cons_real(sqrt(x.real_.val)));
}

/* num den  atan  angle
   arctangent of num/den in degrees */
static
void Ratan (context *ctx,
            Xpost_Object num,
            Xpost_Object den)
{
    real ang = atan2(num.real_.val * RAD_PER_DEG, den.real_.val * RAD_PER_DEG) / RAD_PER_DEG;
    if (ang < 0.0) ang += 360.0;
    xpost_stack_push(ctx->lo, ctx->os, xpost_cons_real(ang));
}

/* angle  cos  real
   cosine of angle (degrees) */
static
void Rcos (context *ctx,
           Xpost_Object x)
{
    xpost_stack_push(ctx->lo, ctx->os,
            xpost_cons_real(cos(RAD_PER_DEG * x.real_.val)));
}

/* angle  sin  real
   sine of angle (degrees) */
static
void Rsin (context *ctx,
           Xpost_Object x)
{
    xpost_stack_push(ctx->lo, ctx->os,
            xpost_cons_real(sin(RAD_PER_DEG * x.real_.val)));
}

/* base exponent  exp  real
   raise base to exponent power */
static
void Rexp (context *ctx,
           Xpost_Object base,
           Xpost_Object expn)
{
    if (base.real_.val < 0)
        expn.real_.val = trunc(expn.real_.val);
    xpost_stack_push(ctx->lo, ctx->os,
            xpost_cons_real(pow(base.real_.val, expn.real_.val)));
}

/* num  ln  real
   natural logarithm of num */
static
void Rln (context *ctx,
          Xpost_Object x)
{
    xpost_stack_push(ctx->lo, ctx->os, xpost_cons_real(log(x.real_.val)));
}

/* num  log  real
   logarithm (base 10) */
static
void Rlog (context *ctx,
           Xpost_Object x)
{
    xpost_stack_push(ctx->lo, ctx->es, xpost_cons_real(log10(x.real_.val)));
}

/* -  rand  int
   generate pseudo-random integer */
static
void Zrand (context *ctx)
{
    unsigned x;
    ctx->rand_next = ctx->rand_next * 1103515245 + 12345;
    x = ctx->rand_next << 16;
    ctx->rand_next = ctx->rand_next * 1103515245 + 12345;
    x |= ctx->rand_next & 0xffff;
    xpost_stack_push(ctx->lo, ctx->es, xpost_cons_int(x & 0x7fffffff));
}

/* int  srand  -
   set random number seed */
static
void Isrand (context *ctx,
             Xpost_Object seed)
{
    ctx->rand_next = seed.int_.val;
}

/* -  rrand  int
   return random number seed */
static
void Zrrand (context *ctx)
{
    xpost_stack_push(ctx->lo, ctx->es, xpost_cons_int(ctx->rand_next));
}

void initopm (context *ctx,
              Xpost_Object sd)
{
    oper *optab;
    Xpost_Object n,op;
    unsigned int optadr;

    assert(ctx->gl->base);
    xpost_memory_table_get_addr(ctx->gl,
            XPOST_MEMORY_TABLE_SPECIAL_OPERATOR_TABLE, &optadr);
    optab = (void *)(ctx->gl->base + optadr);
    //RAD_PER_DEG = PI / 180.0;

    op = consoper(ctx, "add", Iadd, 1, 2, integertype, integertype); INSTALL;
    op = consoper(ctx, "add", Radd, 1, 2, floattype, floattype); INSTALL;
    op = consoper(ctx, "sub", Isub, 1, 2, integertype, integertype); INSTALL;
    op = consoper(ctx, "sub", Rsub, 1, 2, floattype, floattype); INSTALL;
    op = consoper(ctx, "mul", Imul, 1, 2, integertype, integertype); INSTALL;
    op = consoper(ctx, "mul", Rmul, 1, 2, floattype, floattype); INSTALL;
    op = consoper(ctx, "idiv", Iidiv, 1, 2, integertype, integertype); INSTALL;
    op = consoper(ctx, "div", Rdiv, 1, 2, floattype, floattype); INSTALL;
    op = consoper(ctx, "mod", Imod, 1, 2, integertype, integertype); INSTALL;
    op = consoper(ctx, "abs", Iabs, 1, 1, integertype); INSTALL;
    op = consoper(ctx, "abs", Rabs, 1, 1, realtype); INSTALL;
    op = consoper(ctx, "neg", Ineg, 1, 1, integertype); INSTALL;
    op = consoper(ctx, "neg", Rneg, 1, 1, realtype); INSTALL;
    op = consoper(ctx, "floor", Istet, 1, 1, integertype); INSTALL;
    op = consoper(ctx, "floor", Rfloor, 1, 1, realtype); INSTALL;
    op = consoper(ctx, "ceiling", Istet, 1, 1, integertype); INSTALL;
    op = consoper(ctx, "ceiling", Rceiling, 1, 1, realtype); INSTALL;
    op = consoper(ctx, "round", Istet, 1, 1, integertype); INSTALL;
    op = consoper(ctx, "round", Rround, 1, 1, realtype); INSTALL;
    op = consoper(ctx, "truncate", Istet, 1, 1, integertype); INSTALL;
    op = consoper(ctx, "truncate", Rtruncate, 1, 1, realtype); INSTALL;
    op = consoper(ctx, "sqrt", Rsqrt, 1, 1, floattype); INSTALL;
    op = consoper(ctx, "atan", Ratan, 1, 2, floattype, floattype); INSTALL;
    op = consoper(ctx, "cos", Rcos, 1, 1, floattype); INSTALL;
    op = consoper(ctx, "sin", Rsin, 1, 1, floattype); INSTALL;
    op = consoper(ctx, "exp", Rexp, 1, 2, floattype, floattype); INSTALL;
    op = consoper(ctx, "ln", Rln, 1, 1, floattype); INSTALL;
    op = consoper(ctx, "log", Rlog, 1, 1, floattype); INSTALL;
    op = consoper(ctx, "rand", Zrand, 1, 0); INSTALL;
    op = consoper(ctx, "srand", Isrand, 0, 1, integertype); INSTALL;
    op = consoper(ctx, "rrand", Zrrand, 1, 0); INSTALL;

    /* op = consoper(ctx, "eq", Aeq, 1, 2, anytype, anytype); INSTALL;
    //dumpdic(ctx->gl, sd); fflush(NULL);
    bdcput(ctx, sd, consname(ctx, "mark"), mark); */
}


