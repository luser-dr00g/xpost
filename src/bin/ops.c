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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h> /* NULL */

#include "m.h"
#include "ob.h"
#include "s.h"
#include "itp.h"
#include "err.h"
#include "nm.h"
#include "di.h"
#include "op.h"
#include "ops.h"

/* any  pop  -
   discard top element */
void Apop (context *ctx, object x) {
    (void)ctx;
    (void)x;
}

/* any1 any2  exch  any2 any1
   exchange top two elements */
void AAexch (context *ctx, object x, object y) {
    push(ctx->lo, ctx->os, y);
    push(ctx->lo, ctx->os, x);
}

/* any  dup  any any
   duplicate top element */
void Adup (context *ctx, object x) {
    push(ctx->lo, ctx->os, x);
    push(ctx->lo, ctx->os, x);
}

/* any1..anyN N  copy  any1..anyN any1..anyN
   duplicate top n elements */
void Icopy (context *ctx, object n) {
    int i;
    if (n.int_.val < 0) error(rangecheck, "Icopy");
    if ((unsigned)n.int_.val > count(ctx->lo, ctx->os)) error(stackunderflow, "Icopy");
    for (i=0; i < n.int_.val; i++)
        push(ctx->lo, ctx->os, top(ctx->lo, ctx->os, n.int_.val - 1));
}

/* anyN..any0 N  index  anyN..any0 anyN
   duplicate arbitrary element */
void Iindex (context *ctx, object n) {
    if (n.int_.val < 0) error(rangecheck, "Iindex");
    if ((unsigned)n.int_.val >= count(ctx->lo, ctx->os)) error(stackunderflow, "Iindex");
    //printf("index %d\n", n.int_.val);
    push(ctx->lo, ctx->os, top(ctx->lo, ctx->os, n.int_.val));
}

/* a(n-1)..a(0) n j  roll  a((j-1)mod n)..a(0) a(n-1)..a(j mod n)
   roll n elements j times */
void IIroll (context *ctx, object N, object J) {
    object *t;
    int i;
    int n = N.int_.val;
    int j = J.int_.val;
    if (n < 0) error(rangecheck, "IIroll");
    if (n == 0) return;
    if (j < 0) j = n - ( (- j) % n);
    j %= n;
    if (j == 0) return;
    
    t = alloca((n-j) * sizeof(object));
    for (i = 0; i < n-j; i++)
        t[i] = top(ctx->lo, ctx->os, n - 1 - i);
    for (i = 0; i < j; i++)
        pot(ctx->lo, ctx->os, n - 1 - i,
                top(ctx->lo, ctx->os, j - 1 - i));
    for (i = 0; i < n-j; i++)
        pot(ctx->lo, ctx->os, n - j - 1 - i, t[i]);
}

/* |- any1..anyN  clear  |-
   discard all elements */
void Zclear (context *ctx) {
    stack *s = (void *)(ctx->lo->base + ctx->os);
    s->top = 0;
}

/* |- any1..anyN  count  |- any1..anyN N
   count elements on stack */
void Zcount (context *ctx) {
    push(ctx->lo, ctx->os, consint(count(ctx->lo, ctx->os)));
}

/* -  mark  mark
   push mark on stack */
/* the name "mark" is defined in systemdict as a marktype object */

/* mark obj1..objN  cleartomark  -
   discard elements down through mark */
void Zcleartomark (context *ctx) {
    object o;
    do {
        o = pop(ctx->lo, ctx->os);
    } while (o.tag != marktype);
}

/* mark obj1..objN  counttomark  N
   count elements down to mark */
void Zcounttomark (context *ctx) {
    unsigned i;
    unsigned z;
    z = count(ctx->lo, ctx->os);
    for (i = 0; i < z; i++) {
        if (top(ctx->lo, ctx->os, i).tag == marktype) {
            push(ctx->lo, ctx->os, consint(i));
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

void initops(context *ctx, object sd) {
    oper *optab;
    object n,op;
    assert(ctx->gl->base);
    optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB));
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
