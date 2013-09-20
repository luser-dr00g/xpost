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
#include <stdbool.h>
#include <stdio.h> /* printf */
#include <stdlib.h> /* NULL */

#define PI (4.0 * atan(1.0))
double RAD_PER_DEG /* = PI / 180.0 */;

#include "m.h"
#include "ob.h"
#include "s.h"
#include "itp.h"
#include "nm.h"
#include "di.h"
#include "op.h"
#include "opm.h"

bool subwillunder(long x, long y);
bool addwillover(long x, long y) {
    if (y < 0) return subwillunder(x, -y);
    if (x > LONG_MAX - y) return true;
    if (y == LONG_MIN) return true;
    return false;
}

bool subwillunder(long x, long y) {
    if (y < 0) return addwillover(x, -y);
    if (x < LONG_MIN + y) return true;
    if (y == LONG_MIN) return true;
    return false;
}

bool mulwillover(long x, long y) {
    if (x == 0||y == 0) return false;
    if (x < 0) x = -x;
    if (y < 0) y = -y;
    if (x > LONG_MAX / y) return true;
    return false;
}

/* num1 num2  add  sum
   num1 plus num2 */
void Iadd (context *ctx, object x, object y) {
    if (addwillover(x.int_.val, y.int_.val))
        push(ctx->lo, ctx->os, consreal((real)x.int_.val + y.int_.val));
    else
        push(ctx->lo, ctx->os, consint(x.int_.val + y.int_.val));
}

void Radd (context *ctx, object x, object y) {
    push(ctx->lo, ctx->os, consreal(x.real_.val + y.real_.val));
}

/* num1 num2  div  quotient
   num1 divided by num2 */
void Rdiv (context *ctx, object x, object y) {
    push(ctx->lo, ctx->os, consreal(x.real_.val / y.real_.val));
}

/* num1 num2  idiv  quotient
   integer divide */
void Iidiv (context *ctx, object x, object y) {
    push(ctx->lo, ctx->os, consint(x.int_.val / y.int_.val));
}

/* num1 num2  mod  remainder
   num1 mod num2 */
void Imod (context *ctx, object x, object y) {
    push(ctx->lo, ctx->os, consint(x.int_.val % y.int_.val));
}

/* num1 num2  mul  product
   num1 times num2 */
void Imul (context *ctx, object x, object y) {
    if (mulwillover(x.int_.val, y.int_.val))
        push(ctx->lo, ctx->os, consreal((real)x.int_.val * y.int_.val));
    else
        push(ctx->lo, ctx->os, consint(x.int_.val * y.int_.val));
}

void Rmul (context *ctx, object x, object y) {
    push(ctx->lo, ctx->os, consreal(x.real_.val * y.real_.val));
}

/* num1 num2  sub  difference
   num1 minus num2 */
void Isub (context *ctx, object x, object y) {
    if (subwillunder(x.int_.val, y.int_.val))
        push(ctx->lo, ctx->os, consreal((real)x.int_.val - y.int_.val));
    else
        push(ctx->lo, ctx->os, consint(x.int_.val - y.int_.val));
}

void Rsub (context *ctx, object x, object y) {
    push(ctx->lo, ctx->os, consreal(x.real_.val - y.real_.val));
}

/* num1  abs  num2
   absolute value of num1 */
void Iabs (context *ctx, object x) {
    if (x.int_.val == INT_MIN)
        push(ctx->lo, ctx->os, consreal(- (real)INT_MIN));
    else
        push(ctx->lo, ctx->os, consint(x.int_.val>0? x.int_.val: -x.int_.val));
}

void Rabs (context *ctx, object x) {
    push(ctx->lo, ctx->os, consreal(fabs(x.real_.val)));
}

/* num1  neg  num2
   negative of num1 */
void Ineg (context *ctx, object x) {
    if (x.int_.val == INT_MIN)
        push(ctx->lo, ctx->os, consreal(- (real)INT_MIN));
    else
        push(ctx->lo, ctx->os, consint(-x.int_.val));
}

void Rneg (context *ctx, object x) {
    push(ctx->lo, ctx->os, consreal(-x.real_.val));
}

/* stub for integer  floor, ceiling, round, truncate */
void Istet (context *ctx, object x) {
    push(ctx->lo, ctx->os, x);
}

/* num1  ceiling  num2
   ceiling of num1 */
void Rceiling (context *ctx, object x) {
    push(ctx->lo, ctx->os, consreal(ceil(x.real_.val)));
}

/* num1  floor  num2
   floor of num1 */
void Rfloor (context *ctx, object x) {
    push(ctx->lo, ctx->os, consreal(floor(x.real_.val)));
}

/* num1  round  num2
   round num1 to nearest integer */
void Rround (context *ctx, object x) {
    push(ctx->lo, ctx->os, consreal(floor(x.real_.val + 0.5)));
#if 0
    if (x.real_.val > 0)
        push(ctx->lo, ctx->os, consreal(round(x.real_.val)));
    else
        push(ctx->lo, ctx->os, consreal(rint(x.real_.val)));
#endif
}

/* num1  truncate  num2
   remove fractional part of num1 */
void Rtruncate (context *ctx, object x) {
    push(ctx->lo, ctx->os, consreal(trunc(x.real_.val)));
}

/* num1  sqrt  num2
   square root of num1 */
void Rsqrt (context *ctx, object x) {
    push(ctx->lo, ctx->os, consreal(sqrt(x.real_.val)));
}

/* num den  atan  angle
   arctangent of num/den in degrees */
void Ratan (context *ctx, object num, object den) {
    real ang = atan2(num.real_.val * RAD_PER_DEG, den.real_.val * RAD_PER_DEG) / RAD_PER_DEG;
    if (ang < 0.0) ang += 360.0;
    push(ctx->lo, ctx->os, consreal(ang));
}

/* angle  cos  real
   cosine of angle (degrees) */
void Rcos (context *ctx, object x) {
    push(ctx->lo, ctx->os,
            consreal(cos(RAD_PER_DEG * x.real_.val)));
}

/* angle  sin  real
   sine of angle (degrees) */
void Rsin (context *ctx, object x) {
    push(ctx->lo, ctx->os,
            consreal(sin(RAD_PER_DEG * x.real_.val)));
}

/* base exponent  exp  real
   raise base to exponent power */
void Rexp (context *ctx, object base, object expn) {
    if (base.real_.val < 0)
        expn.real_.val = trunc(expn.real_.val);
    push(ctx->lo, ctx->os,
            consreal(pow(base.real_.val, expn.real_.val)));
}

/* num  ln  real
   natural logarithm of num */
void Rln (context *ctx, object x) {
    push(ctx->lo, ctx->os, consreal(log(x.real_.val)));
}

/* num  log  real
   logarithm (base 10) */
void Rlog (context *ctx, object x) {
    push(ctx->lo, ctx->es, consreal(log10(x.real_.val)));
}

/* -  rand  int
   generate pseudo-random integer */
void Zrand (context *ctx) {
    unsigned x;
    ctx->rand_next = ctx->rand_next * 1103515245 + 12345;
    x = ctx->rand_next << 16;
    ctx->rand_next = ctx->rand_next * 1103515245 + 12345;
    x |= ctx->rand_next & 0xffff;
    push(ctx->lo, ctx->es, consint(x & 0x7fffffff));
}

/* int  srand  -
   set random number seed */
void Isrand (context *ctx, object seed) {
    ctx->rand_next = seed.int_.val;
}

/* -  rrand  int
   return random number seed */
void Zrrand (context *ctx) {
    push(ctx->lo, ctx->es, consint(ctx->rand_next));
}

void initopm (context *ctx, object sd) {
    oper *optab;
    object n,op;
    assert(ctx->gl->base);
    optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB));
    RAD_PER_DEG = PI / 180.0;

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


