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

#include <assert.h>
#include <stdlib.h> /* NULL */

#include "xpost_memory.h"
#include "xpost_object.h"
#include "xpost_stack.h"

#include "xpost_context.h"
#include "xpost_interpreter.h"
#include "xpost_error.h"
#include "xpost_string.h"
#include "xpost_array.h"
#include "xpost_dict.h"
#include "xpost_operator.h"
#include "xpost_op_string.h"

static
int Istring(Xpost_Context *ctx,
             Xpost_Object I)
{
    xpost_stack_push(ctx->lo, ctx->os, xpost_object_cvlit(consbst(ctx, I.int_.val, NULL)));
    return 0;
}

static
int Slength(Xpost_Context *ctx,
             Xpost_Object S)
{
    xpost_stack_push(ctx->lo, ctx->os, xpost_cons_int(S.comp_.sz));
    return 0;
}

static
int s_copy(Xpost_Context *ctx,
            Xpost_Object S,
            Xpost_Object D)
{
    unsigned i;
    for (i = 0; i < S.comp_.sz; i++)
        bstput(ctx, D, i, bstget(ctx, S, i));
    return 0;
}

static
int Scopy(Xpost_Context *ctx,
           Xpost_Object S,
           Xpost_Object D)
{
    if (D.comp_.sz < S.comp_.sz)
        return rangecheck;
    s_copy(ctx, S, D);
    xpost_stack_push(ctx->lo, ctx->os, arrgetinterval(D, 0, S.comp_.sz));
    return 0;
}

static
int Sget(Xpost_Context *ctx,
          Xpost_Object S,
          Xpost_Object I)
{
    xpost_stack_push(ctx->lo, ctx->os, xpost_cons_int(bstget(ctx, S, I.int_.val)));
    return 0;
}

static
int Sput(Xpost_Context *ctx,
          Xpost_Object S,
          Xpost_Object I,
          Xpost_Object C)
{
    bstput(ctx, S, I.int_.val, C.int_.val);
    return 0;
}

static
int Sgetinterval(Xpost_Context *ctx,
                  Xpost_Object S,
                  Xpost_Object I,
                  Xpost_Object L)
{
    xpost_stack_push(ctx->lo, ctx->os, arrgetinterval(S, I.int_.val, L.int_.val));
    return 0;
}

static
int Sputinterval(Xpost_Context *ctx,
                  Xpost_Object D,
                  Xpost_Object I,
                  Xpost_Object S)
{
    s_copy(ctx, S, arrgetinterval(D, I.int_.val, S.comp_.sz));
    return 0;
}

static
int ancsearch(char *str,
              char *seek,
              int seekn)
{
    int i;
    for (i = 0; i < seekn; i++)
        if (str[i] != seek[i])
            return 0;
    return 1;
}

static
int Sanchorsearch(Xpost_Context *ctx,
                   Xpost_Object str,
                   Xpost_Object seek)
{
    char *s, *k;
    if (seek.comp_.sz > str.comp_.sz)
        return rangecheck;
    s = charstr(ctx, str);
    k = charstr(ctx, seek);
    if (ancsearch(s, k, seek.comp_.sz)) {
        xpost_stack_push(ctx->lo, ctx->os,
                arrgetinterval(str, seek.comp_.sz, 
                    str.comp_.sz - seek.comp_.sz)); /* post */
        xpost_stack_push(ctx->lo, ctx->os,
                arrgetinterval(str, 0, seek.comp_.sz)); /* match */
        xpost_stack_push(ctx->lo, ctx->os, xpost_cons_bool(1));
    } else {
        xpost_stack_push(ctx->lo, ctx->os, str);
        xpost_stack_push(ctx->lo, ctx->os, xpost_cons_bool(0));
    }
    return 0;
}

static
int Ssearch(Xpost_Context *ctx,
             Xpost_Object str,
             Xpost_Object seek)
{
    int i;
    char *s, *k;
    if (seek.comp_.sz > str.comp_.sz)
        return rangecheck;
    s = charstr(ctx, str);
    k = charstr(ctx, seek);
    for (i = 0; i <= (str.comp_.sz - seek.comp_.sz); i++) {
        if (ancsearch(s+i, k, seek.comp_.sz)) {
            xpost_stack_push(ctx->lo, ctx->os, 
                    arrgetinterval(str, i + seek.comp_.sz,
                        str.comp_.sz - seek.comp_.sz - i)); /* post */
            xpost_stack_push(ctx->lo, ctx->os,
                    arrgetinterval(str, i, seek.comp_.sz)); /* match */
            xpost_stack_push(ctx->lo, ctx->os,
                    arrgetinterval(str, 0, i)); /* pre */
            xpost_stack_push(ctx->lo, ctx->os, xpost_cons_bool(1));
            return 0;
        }
    }
    xpost_stack_push(ctx->lo, ctx->os, str);
    xpost_stack_push(ctx->lo, ctx->os, xpost_cons_bool(0));
    return 0;
}

static
int Sforall(Xpost_Context *ctx,
             Xpost_Object S,
             Xpost_Object P)
{
    if (S.comp_.sz == 0) return 0;
    assert(ctx->gl->base);
    //xpost_stack_push(ctx->lo, ctx->es, consoper(ctx, "forall", NULL,0,0));
    if (!xpost_stack_push(ctx->lo, ctx->es, operfromcode(ctx->opcode_shortcuts.forall)))
        return execstackoverflow;
    //xpost_stack_push(ctx->lo, ctx->es, consoper(ctx, "cvx", NULL,0,0));
    if (!xpost_stack_push(ctx->lo, ctx->es, operfromcode(ctx->opcode_shortcuts.cvx)))
        return execstackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->es, xpost_object_cvlit(P)))
        return execstackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->es, xpost_object_cvlit(arrgetinterval(S, 1, S.comp_.sz-1))))
        return execstackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->es, P))
        return execstackoverflow;
    if (!xpost_object_is_exe(S)) {
        //xpost_stack_push(ctx->lo, ctx->es, consoper(ctx, "cvx", NULL,0,0));
        if (!xpost_stack_push(ctx->lo, ctx->es, operfromcode(ctx->opcode_shortcuts.cvx)))
            return execstackoverflow;
    }
    if (!xpost_stack_push(ctx->lo, ctx->os, xpost_cons_int(bstget(ctx, S, 0))))
        return stackoverflow;
    return 0;
}

// token : see optok.c

int initopst(Xpost_Context *ctx,
              Xpost_Object sd)
{
    oper *optab;
    Xpost_Object n,op;
    unsigned int optadr;

    assert(ctx->gl->base);
    xpost_memory_table_get_addr(ctx->gl,
            XPOST_MEMORY_TABLE_SPECIAL_OPERATOR_TABLE, &optadr);
    optab = (void *)(ctx->gl->base + optadr);
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
    return 0;
}

