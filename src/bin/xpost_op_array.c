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
#include "xpost_save.h"
#include "xpost_context.h"
#include "xpost_interpreter.h"
#include "xpost_error.h"
#include "xpost_string.h"
#include "xpost_array.h"
#include "xpost_dict.h"
#include "xpost_name.h"
#include "xpost_operator.h"
#include "xpost_op_stack.h"
#include "xpost_op_array.h"


/* helper function */
static
void a_copy (Xpost_Context *ctx,
             Xpost_Object S,
             Xpost_Object D)
{
    unsigned i;
    for (i = 0; i < S.comp_.sz; i++)
        barput(ctx, D, i, barget(ctx, S, i));
}

/* int  array  array
   create array of length int */
static
void Iarray (Xpost_Context *ctx,
             Xpost_Object I)
{
    xpost_stack_push(ctx->lo, ctx->os, xpost_object_cvlit(consbar(ctx, I.int_.val)));
}

/* -  [  mark
   start array construction */
/* [ is defined in systemdict as a marktype object */

/* mark obj0..objN-1  ]  array
   end array construction */
void arrtomark (Xpost_Context *ctx)
{
    int i;
    Xpost_Object a, v;
    Zcounttomark(ctx);
    i = xpost_stack_pop(ctx->lo, ctx->os).int_.val;
    a = consbar(ctx, i);
    for ( ; i > 0; i--){
        v = xpost_stack_pop(ctx->lo, ctx->os);
        barput(ctx, a, i-1, v);
    }
    (void)xpost_stack_pop(ctx->lo, ctx->os); // pop mark
    xpost_stack_push(ctx->lo, ctx->os, xpost_object_cvlit(a));
}

/* array  length  int
   number of elements in array */
static
void Alength (Xpost_Context *ctx,
              Xpost_Object A)
{
    xpost_stack_push(ctx->lo, ctx->os, xpost_cons_int(A.comp_.sz));
}

/* array index  get  any
   get array element indexed by index */
static
void Aget (Xpost_Context *ctx,
           Xpost_Object A,
           Xpost_Object I)
{
    xpost_stack_push(ctx->lo, ctx->os, barget(ctx, A, I.int_.val));
}

/* array index any  put  -
   put any into array at index */
static
void Aput(Xpost_Context *ctx,
          Xpost_Object A,
          Xpost_Object I,
          Xpost_Object O)
{
    barput(ctx, A, I.int_.val, O);
}

/* array index count  getinterval  subarray
   subarray of array starting at index for count elements */
static
void Agetinterval (Xpost_Context *ctx,
                   Xpost_Object A,
                   Xpost_Object I,
                   Xpost_Object L)
{
    xpost_stack_push(ctx->lo, ctx->os, arrgetinterval(A, I.int_.val, L.int_.val));
}

/* array1 index array2  putinterval  -
   replace subarray of array1 starting at index by array2 */
static
void Aputinterval (Xpost_Context *ctx,
                   Xpost_Object D,
                   Xpost_Object I,
                   Xpost_Object S)
{
    if (I.int_.val + S.comp_.sz > D.comp_.sz)
        error(rangecheck, "putinterval");
    a_copy(ctx, S, arrgetinterval(D, I.int_.val, S.comp_.sz));
}

/* array  aload  a0..aN-1 array
   push all elements of array on stack */
static
void Aaload (Xpost_Context *ctx,
             Xpost_Object A)
{
    int i;

    for (i = 0; i < A.comp_.sz; i++)
        xpost_stack_push(ctx->lo, ctx->os, barget(ctx, A, i));
    xpost_stack_push(ctx->lo, ctx->os, A);
}

/* any0..anyN-1 array  astore  array
   pop elements from stack into array */
static
void Aastore (Xpost_Context *ctx,
              Xpost_Object A)
{
    int i;

    for (i = A.comp_.sz - 1; i >= 0; i--)
        barput(ctx, A, i, xpost_stack_pop(ctx->lo, ctx->os));
    xpost_stack_push(ctx->lo, ctx->os, A);
}

/* array1 array2  copy  subarray2
   copy elements of array1 to initial subarray of array2 */
static
void Acopy (Xpost_Context *ctx,
            Xpost_Object S,
            Xpost_Object D)
{
    if (D.comp_.sz < S.comp_.sz)
        error(rangecheck, "Acopy");
    a_copy(ctx, S, D);
    xpost_stack_push(ctx->lo, ctx->os, arrgetinterval(D, 0, S.comp_.sz));
}

/* array proc  forall  -
   execute proc for each element of array */
static
void Aforall(Xpost_Context *ctx,
             Xpost_Object A,
             Xpost_Object P)
{
    if (A.comp_.sz == 0)
        return;

    assert(ctx->gl->base);
    //xpost_stack_push(ctx->lo, ctx->es, consoper(ctx, "forall", NULL,0,0));
    xpost_stack_push(ctx->lo, ctx->es, operfromcode(ctx->opcode_shortcuts.forall));
    //xpost_stack_push(ctx->lo, ctx->es, consoper(ctx, "cvx", NULL,0,0));
    xpost_stack_push(ctx->lo, ctx->es, operfromcode(ctx->opcode_shortcuts.cvx));
    xpost_stack_push(ctx->lo, ctx->es, xpost_object_cvlit(P));
    xpost_stack_push(ctx->lo, ctx->es, xpost_object_cvlit(arrgetinterval(A, 1, A.comp_.sz - 1)));
    xpost_stack_push(ctx->lo, ctx->es, P);
    if (xpost_object_is_exe(A)) {
        //xpost_stack_push(ctx->lo, ctx->es, consoper(ctx, "cvx", NULL,0,0));
        xpost_stack_push(ctx->lo, ctx->es, operfromcode(ctx->opcode_shortcuts.cvx));
    }
    xpost_stack_push(ctx->lo, ctx->os, barget(ctx, A, 0));
}

void initopar (Xpost_Context *ctx,
               Xpost_Object sd)
{
    oper *optab;
    Xpost_Object n,op;
    unsigned int optadr;

    assert(ctx->gl->base);
    xpost_memory_table_get_addr(ctx->gl,
            XPOST_MEMORY_TABLE_SPECIAL_OPERATOR_TABLE, &optadr);
    optab = (void *)(ctx->gl->base + optadr);

    op = consoper(ctx, "array", Iarray, 1, 1,
            integertype); INSTALL;
    bdcput(ctx, sd, consname(ctx, "["), mark);
    op = consoper(ctx, "]", arrtomark, 1, 0); INSTALL;
    op = consoper(ctx, "length", Alength, 1, 1,
            arraytype); INSTALL;
    op = consoper(ctx, "get", Aget, 1, 2,
            arraytype, integertype); INSTALL;
    op = consoper(ctx, "put", Aput, 0, 3,
            arraytype, integertype, anytype); INSTALL;
    op = consoper(ctx, "getinterval", Agetinterval, 1, 3,
            arraytype, integertype, integertype); INSTALL;
    op = consoper(ctx, "putinterval", Aputinterval, 0, 3,
            arraytype, integertype, arraytype); INSTALL;
    op = consoper(ctx, "aload", Aaload, 1, 1,
            arraytype); INSTALL;
    op = consoper(ctx, "astore", Aastore, 1, 1,
            arraytype); INSTALL;
    op = consoper(ctx, "copy", Acopy, 1, 2,
            arraytype, arraytype); INSTALL;
    op = consoper(ctx, "forall", Aforall, 0, 2,
            arraytype, proctype); INSTALL;
}

