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

#include "m.h"
#include "ob.h"
#include "s.h"
#include "itp.h"
#include "nm.h"
#include "di.h"
#include "op.h"
#include "opb.h"

/* any1 any2  eq  bool
   test equal */
void Aeq (context *ctx,
          object x,
          object y)
{
    push(ctx->lo, ctx->os, consbool(objcmp(ctx,x,y) == 0));
}

/* any1 any2  ne  bool
   test not equal */
void Ane (context *ctx,
          object x,
          object y)
{
    push(ctx->lo, ctx->os, consbool(objcmp(ctx,x,y) != 0));
}

/* any1 any2  ge  bool
   test greater or equal */
void Age (context *ctx,
          object x,
          object y)
{
    push(ctx->lo, ctx->os, consbool(objcmp(ctx,x,y) >= 0));
}

/* any1 any2  gt  bool
   test greater than */
void Agt (context *ctx,
          object x,
          object y)
{
    push(ctx->lo, ctx->os, consbool(objcmp(ctx,x,y) > 0));
}

/* any1 any2  le  bool
   test less or equal */
void Ale (context *ctx,
          object x,
          object y)
{
    push(ctx->lo, ctx->os, consbool(objcmp(ctx,x,y) <= 0));
}

/* any1 any2  lt  bool
   test less than */
void Alt (context *ctx,
          object x,
          object y)
{
    push(ctx->lo, ctx->os, consbool(objcmp(ctx,x,y) < 0));
}

/* bool1|int1 bool2|int2  and  bool3|int3
   logical|bitwise and */
void Band (context *ctx,
           object x,
           object y)
{
    push(ctx->lo, ctx->os, consbool(x.int_.val & y.int_.val));
}

void Iand (context *ctx,
           object x,
           object y)
{
    push(ctx->lo, ctx->os, consint(x.int_.val & y.int_.val));
}

/* bool1|int1  not  bool2|int2
   logical|bitwise not */
void Bnot (context *ctx,
           object x)
{
    push(ctx->lo, ctx->os, consbool( ! x.int_.val ));
}

void Inot (context *ctx,
           object x)
{
    push(ctx->lo, ctx->os, consint( ~ x.int_.val ));
}

/* bool1|int1 bool2|int2  or  bool3|int3
   logical|bitwise inclusive or */
void Bor (context *ctx,
          object x,
          object y)
{
    push(ctx->lo, ctx->os, consbool(x.int_.val | y.int_.val));
}

void Ior (context *ctx,
          object x,
          object y)
{
    push(ctx->lo, ctx->os, consint(x.int_.val | y.int_.val));
}

/* bool1|int1 bool2|int2  xor  bool3|int3
   exclusive or */
void Bxor (context *ctx,
           object x,
           object y)
{
    push(ctx->lo, ctx->os, consbool(x.int_.val ^ y.int_.val));
}

void Ixor (context *ctx,
           object x,
           object y)
{
    push(ctx->lo, ctx->os, consint(x.int_.val ^ y.int_.val));
}

// true

// false

/* int1 shift  bitshift  int2
   bitwise shift of int1 (positive is left) */
void Ibitshift (context *ctx,
                object x,
                object y)
{
    if (y.int_.val >= 0)
        push(ctx->lo, ctx->os, consint(x.int_.val << y.int_.val));
    else
        push(ctx->lo, ctx->os, consint(
                    (unsigned long)x.int_.val >> -y.int_.val));
}

void initopb(context *ctx,
             object sd)
{
    oper *optab;
    object n,op;
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
    bdcput(ctx, sd, consname(ctx, "true"), consbool(true));
    bdcput(ctx, sd, consname(ctx, "false"), consbool(false));
    op = consoper(ctx, "bitshift", Ibitshift, 1, 2, integertype, integertype); INSTALL;

    /* dumpdic(ctx->gl, sd); fflush(NULL); */
}
