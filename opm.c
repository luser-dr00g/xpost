#include <alloca.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h> /* printf */
#include <stdlib.h> /* NULL */

#include "m.h"
#include "ob.h"
#include "s.h"
#include "itp.h"
#include "nm.h"
#include "di.h"
#include "op.h"

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

/* num1 num2  idiv  quotient
   integer divide */
void Iidiv (context *ctx, object x, object y) {
    push(ctx->lo, ctx->os, consint(x.int_.val / y.int_.val));
}

/* num1 num2  div  quotient
   num1 divided by num2 */
void Rdiv (context *ctx, object x, object y) {
    push(ctx->lo, ctx->os, consreal(x.real_.val / y.real_.val));
}

/* num1 num2  mod  remainder
   num1 mod num2 */
void Imod (context *ctx, object x, object y) {
    push(ctx->lo, ctx->os, consint(x.int_.val % y.int_.val));
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

void Rfloor (context *ctx, object x) {
    push(ctx->lo, ctx->os, consreal(floor(x.real_.val)));
}

void Rceiling (context *ctx, object x) {
    push(ctx->lo, ctx->os, consreal(ceil(x.real_.val)));
}

void Rround (context *ctx, object x) {
    push(ctx->lo, ctx->os, consreal(round(x.real_.val)));
}

void Rtruncate (context *ctx, object x) {
    push(ctx->lo, ctx->os, consreal(trunc(x.real_.val)));
}

void Rsqrt (context *ctx, object x) {
    push(ctx->lo, ctx->os, consreal(sqrt(x.real_.val)));
}

void initopm (context *ctx, object sd) {
    oper *optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB));
    object n,op;

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

    /* op = consoper(ctx, "eq", Aeq, 1, 2, anytype, anytype); INSTALL;
    //dumpdic(ctx->gl, sd); fflush(NULL);
    bdcput(ctx, sd, consname(ctx, "mark"), mark); */
}


