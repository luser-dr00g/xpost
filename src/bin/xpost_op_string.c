#ifdef HAVE_CONFIG_H
# include <config.h>
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
#include <stdio.h> /* printf */
#include <stdlib.h> /* NULL */

#include "xpost_memory.h"
#include "xpost_object.h"
#include "xpost_stack.h"
#include "xpost_interpreter.h"
#include "xpost_error.h"
#include "xpost_string.h"
#include "xpost_array.h"
#include "xpost_dict.h"
#include "xpost_operator.h"
#include "xpost_op_string.h"

static
void Istring(context *ctx,
             Xpost_Object I)
{
    push(ctx->lo, ctx->os, xpost_object_cvlit(consbst(ctx, I.int_.val, NULL)));
}

static
void Slength(context *ctx,
             Xpost_Object S)
{
    push(ctx->lo, ctx->os, xpost_cons_int(S.comp_.sz));
}

static
void s_copy(context *ctx,
            Xpost_Object S,
            Xpost_Object D)
{
    unsigned i;
    for (i = 0; i < S.comp_.sz; i++)
        bstput(ctx, D, i, bstget(ctx, S, i));
}

static
void Scopy(context *ctx,
           Xpost_Object S,
           Xpost_Object D)
{
    if (D.comp_.sz < S.comp_.sz) error(rangecheck, "Scopy");
    s_copy(ctx, S, D);
    push(ctx->lo, ctx->os, arrgetinterval(D, 0, S.comp_.sz));
}

static
void Sget(context *ctx,
          Xpost_Object S,
          Xpost_Object I)
{
    push(ctx->lo, ctx->os, xpost_cons_int(bstget(ctx, S, I.int_.val)));
}

static
void Sput(context *ctx,
          Xpost_Object S,
          Xpost_Object I,
          Xpost_Object C)
{
    bstput(ctx, S, I.int_.val, C.int_.val);
}

static
void Sgetinterval(context *ctx,
                  Xpost_Object S,
                  Xpost_Object I,
                  Xpost_Object L)
{
    push(ctx->lo, ctx->os, arrgetinterval(S, I.int_.val, L.int_.val));
}

static
void Sputinterval(context *ctx,
                  Xpost_Object D,
                  Xpost_Object I,
                  Xpost_Object S)
{
    s_copy(ctx, S, arrgetinterval(D, I.int_.val, S.comp_.sz));
}

static
int ancsearch(char *str,
              char *seek,
              int seekn)
{
    int i;
    for (i = 0; i < seekn; i++)
        if (str[i] != seek[i])
            return false;
    return true;
}

static
void Sanchorsearch(context *ctx,
                   Xpost_Object str,
                   Xpost_Object seek)
{
    char *s, *k;
    if (seek.comp_.sz > str.comp_.sz) error(rangecheck, "Sanchorsearch");
    s = charstr(ctx, str);
    k = charstr(ctx, seek);
    if (ancsearch(s, k, seek.comp_.sz)) {
        push(ctx->lo, ctx->os,
                arrgetinterval(str, seek.comp_.sz, 
                    str.comp_.sz - seek.comp_.sz)); /* post */
        push(ctx->lo, ctx->os,
                arrgetinterval(str, 0, seek.comp_.sz)); /* match */
        push(ctx->lo, ctx->os, xpost_cons_bool(true));
    } else {
        push(ctx->lo, ctx->os, str);
        push(ctx->lo, ctx->os, xpost_cons_bool(false));
    }
}

static
void Ssearch(context *ctx,
             Xpost_Object str,
             Xpost_Object seek)
{
    int i;
    char *s, *k;
    if (seek.comp_.sz > str.comp_.sz) error(rangecheck, "Ssearch");
    s = charstr(ctx, str);
    k = charstr(ctx, seek);
    for (i = 0; i <= (str.comp_.sz - seek.comp_.sz); i++) {
        if (ancsearch(s+i, k, seek.comp_.sz)) {
            push(ctx->lo, ctx->os, 
                    arrgetinterval(str, i + seek.comp_.sz,
                        str.comp_.sz - seek.comp_.sz - i)); /* post */
            push(ctx->lo, ctx->os,
                    arrgetinterval(str, i, seek.comp_.sz)); /* match */
            push(ctx->lo, ctx->os,
                    arrgetinterval(str, 0, i)); /* pre */
            push(ctx->lo, ctx->os, xpost_cons_bool(true));
            return;
        }
    }
    push(ctx->lo, ctx->os, str);
    push(ctx->lo, ctx->os, xpost_cons_bool(false));
}

static
void Sforall(context *ctx,
             Xpost_Object S,
             Xpost_Object P)
{
    if (S.comp_.sz == 0) return;
    assert(ctx->gl->base);
    //push(ctx->lo, ctx->es, consoper(ctx, "forall", NULL,0,0));
    push(ctx->lo, ctx->es, operfromcode(ctx->opcuts.forall));
    //push(ctx->lo, ctx->es, consoper(ctx, "cvx", NULL,0,0));
    push(ctx->lo, ctx->es, operfromcode(ctx->opcuts.cvx));
    push(ctx->lo, ctx->es, xpost_object_cvlit(P));
    push(ctx->lo, ctx->es, xpost_object_cvlit(arrgetinterval(S, 1, S.comp_.sz-1)));
    if (xpost_object_is_exe(S)) {
        //push(ctx->lo, ctx->es, consoper(ctx, "cvx", NULL,0,0));
        push(ctx->lo, ctx->es, operfromcode(ctx->opcuts.cvx));
    }
    push(ctx->lo, ctx->es, P);
    push(ctx->lo, ctx->es, xpost_cons_int(bstget(ctx, S, 0)));
}

// token : see optok.c

void initopst(context *ctx,
              Xpost_Object sd)
{
    oper *optab;
    Xpost_Object n,op;
    assert(ctx->gl->base);
    optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB));
    op = consoper(ctx, "string", Istring, 1, 1,
            integertype); INSTALL;
    op = consoper(ctx, "length", Slength, 1, 1,
            stringtype); INSTALL;
    op = consoper(ctx, "copy", Scopy, 1, 2,
            stringtype, stringtype); INSTALL;
    op = consoper(ctx, "get", Sget, 1, 2,
            stringtype, integertype); INSTALL;
    op = consoper(ctx, "put", Sput, 0, 3,
            stringtype, integertype, integertype); INSTALL;
    op = consoper(ctx, "getinterval", Sgetinterval, 1, 3,
            stringtype, integertype, integertype); INSTALL;
    op = consoper(ctx, "putinterval", Sputinterval, 0, 3,
            stringtype, integertype, stringtype); INSTALL;
    op = consoper(ctx, "anchorsearch", Sanchorsearch, 3, 2,
            stringtype, stringtype); INSTALL;
    op = consoper(ctx, "search", Ssearch, 4, 2,
            stringtype, stringtype); INSTALL;
    op = consoper(ctx, "forall", Sforall, 0, 2,
            stringtype, proctype); INSTALL;
    //bdcput(ctx, sd, consname(ctx, "mark"), mark);
}

