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

void Iadd (context *ctx, object x, object y) {
    if (addwillover(x.int_.val, y.int_.val))
        push(ctx->lo, ctx->os, consreal((real)x.int_.val + y.int_.val));
    else
        push(ctx->lo, ctx->os, consint(x.int_.val + y.int_.val));
}

void Radd (context *ctx, object x, object y) {
    push(ctx->lo, ctx->os, consreal(x.real_.val + y.real_.val));
}

void Isub (context *ctx, object x, object y) {
    if (subwillunder(x.int_.val, y.int_.val))
        push(ctx->lo, ctx->os, consreal((real)x.int_.val - y.int_.val));
    else
        push(ctx->lo, ctx->os, consint(x.int_.val - y.int_.val));
}

void Rsub (context *ctx, object x, object y) {
    push(ctx->lo, ctx->os, consreal(x.real_.val - y.real_.val));
}

void Imul (context *ctx, object x, object y) {
    if (mulwillover(x.int_.val, y.int_.val))
        push(ctx->lo, ctx->os, consreal((real)x.int_.val * y.int_.val));
    else
        push(ctx->lo, ctx->os, consint(x.int_.val * y.int_.val));
}

void Rmul (context *ctx, object x, object y) {
    push(ctx->lo, ctx->os, consreal(x.real_.val * y.real_.val));
}

void Iidiv (context *ctx, object x, object y) {
    push(ctx->lo, ctx->os, consint(x.int_.val / y.int_.val));
}

void Rdiv (context *ctx, object x, object y) {
    push(ctx->lo, ctx->os, consreal(x.real_.val / y.real_.val));
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
    /*
    op = consoper(ctx, "eq", Aeq, 1, 2, anytype, anytype); INSTALL;
    //dumpdic(ctx->gl, sd); fflush(NULL);
    bdcput(ctx, sd, consname(ctx, "mark"), mark);
    */

}


