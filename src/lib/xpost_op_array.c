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
#include "xpost_error.h"
#include "xpost_string.h"
#include "xpost_array.h"
#include "xpost_dict.h"
#include "xpost_name.h"

//#include "xpost_interpreter.h"
#include "xpost_operator.h"
#include "xpost_op_stack.h"
#include "xpost_op_array.h"


/* helper function */
static
int _xpost_op_array_copy_aux (Xpost_Context *ctx,
            Xpost_Object S,
            Xpost_Object D)
{
    unsigned i;
    Xpost_Object t;

    for (i = 0; i < S.comp_.sz; i++)
    {
        t = xpost_array_get(ctx, S, i);
        if (xpost_object_get_type(t) == invalidtype)
            return rangecheck;
        xpost_array_put(ctx, D, i, t);
    }

    return 0;
}

/* int  array  array
   create array of length int */
static
int xpost_op_int_array (Xpost_Context *ctx,
            Xpost_Object I)
{
    Xpost_Object t;

    t = xpost_array_cons(ctx, I.int_.val);
    if (xpost_object_get_type(t) == nulltype)
        return VMerror;
    xpost_stack_push(ctx->lo, ctx->os, xpost_object_cvlit(t));

    return 0;
}

/* -  [  mark
   start array construction */
/* [ is defined in systemdict as a marktype object */

/* mark obj0..objN-1  ]  array
   end array construction */
int xpost_op_array_to_mark (Xpost_Context *ctx)
{
    int i;
    Xpost_Object a, v;
    Xpost_Object t;

    if (xpost_op_counttomark(ctx))
        return unmatchedmark;
    t = xpost_stack_pop(ctx->lo, ctx->os);
    if (xpost_object_get_type(t) == invalidtype)
        return stackunderflow;
    i = t.int_.val;
    a = xpost_array_cons(ctx, i);
    if (xpost_object_get_type(a) == nulltype)
        return VMerror;
    for ( ; i > 0; i--){
        v = xpost_stack_pop(ctx->lo, ctx->os);
        if (xpost_object_get_type(v) == invalidtype)
            return stackunderflow;
        xpost_array_put(ctx, a, i-1, v);
    }
    (void)xpost_stack_pop(ctx->lo, ctx->os); // pop mark
    xpost_stack_push(ctx->lo, ctx->os, xpost_object_cvlit(a));

    return 0;
}

/* array  length  int
   number of elements in array */
static
int xpost_op_array_length (Xpost_Context *ctx,
              Xpost_Object A)
{
    if (!xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons(A.comp_.sz)))
        return stackoverflow;

    return 0;
}

/* array index  get  any
   get array element indexed by index */
static
int xpost_op_array_int_get (Xpost_Context *ctx,
           Xpost_Object A,
           Xpost_Object I)
{
    Xpost_Object t;
    if (I.int_.val < 0)
        return rangecheck;
    t = xpost_array_get(ctx, A, I.int_.val);
    if (xpost_object_get_type(t) == invalidtype)
        return rangecheck;
    if (!xpost_stack_push(ctx->lo, ctx->os, t))
        return stackoverflow;
    return 0;
}

/* array index any  put  -
   put any into array at index */
static
int xpost_op_array_int_any_put(Xpost_Context *ctx,
          Xpost_Object A,
          Xpost_Object I,
          Xpost_Object O)
{
    if (I.int_.val < 0)
        return rangecheck;
    return xpost_array_put(ctx, A, I.int_.val, O);
}

/* array index count  getinterval  subarray
   subarray of array starting at index for count elements */
static
int xpost_op_array_int_int_getinterval (Xpost_Context *ctx,
                   Xpost_Object A,
                   Xpost_Object I,
                   Xpost_Object L)
{
    Xpost_Object subarr;
    if (I.int_.val < 0)
        return rangecheck;
    subarr = xpost_object_get_interval(A, I.int_.val, L.int_.val);
    if (xpost_object_get_type(subarr) == invalidtype)
        return rangecheck;
    xpost_stack_push(ctx->lo, ctx->os, subarr);
    return 0;
}

/* array1 index array2  putinterval  -
   replace subarray of array1 starting at index by array2 */
static
int xpost_op_array_int_array_putinterval (Xpost_Context *ctx,
                   Xpost_Object D,
                   Xpost_Object I,
                   Xpost_Object S)
{
    Xpost_Object subarr;
    if (I.int_.val < 0)
        return rangecheck;
    if (I.int_.val + S.comp_.sz > D.comp_.sz)
        return rangecheck;
    subarr = xpost_object_get_interval(D, I.int_.val, S.comp_.sz);
    if (xpost_object_get_type(subarr) == invalidtype)
        return rangecheck;
    _xpost_op_array_copy_aux(ctx, S, subarr);
    return 0;
}

/* array  aload  a0..aN-1 array
   push all elements of array on stack */
static
int xpost_op_array_aload (Xpost_Context *ctx,
             Xpost_Object A)
{
    int i;

    for (i = 0; i < A.comp_.sz; i++)
        if (!xpost_stack_push(ctx->lo, ctx->os, xpost_array_get(ctx, A, i)))
            return stackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->os, A))
        return stackoverflow;
    return 0;
}

/* any0..anyN-1 array  astore  array
   pop elements from stack into array */
static
int xpost_op_anyn_array_astore (Xpost_Context *ctx,
              Xpost_Object A)
{
    Xpost_Object t;
    int i;
    unsigned int cnt;
    int ret;

    cnt = xpost_stack_count(ctx->lo, ctx->os);
    if (cnt < A.comp_.sz)
        return stackunderflow;
    for (i = A.comp_.sz - 1; i >= 0; i--)
    {
        t = xpost_stack_pop(ctx->lo, ctx->os);
        //if (xpost_object_get_type(t) == invalidtype)
            //return stackunderflow;
        ret = xpost_array_put(ctx, A, i, t);
        if (ret)
            return ret;
    }
    xpost_stack_push(ctx->lo, ctx->os, A);
    return 0;
}

/* array1 array2  copy  subarray2
   copy elements of array1 to initial subarray of array2 */
static
int xpost_op_array_copy (Xpost_Context *ctx,
            Xpost_Object S,
            Xpost_Object D)
{
    Xpost_Object subarr;
    if (D.comp_.sz < S.comp_.sz)
        return rangecheck;
    _xpost_op_array_copy_aux(ctx, S, D);
    subarr = xpost_object_get_interval(D, 0, S.comp_.sz);
    if (xpost_object_get_type(subarr) == invalidtype)
        return rangecheck;
    xpost_stack_push(ctx->lo, ctx->os, subarr);
    return 0;
}

/* array proc  forall  -
   execute proc for each element of array */
static
int xpost_op_array_proc_forall(Xpost_Context *ctx,
             Xpost_Object A,
             Xpost_Object P)
{
    Xpost_Object interval;
    Xpost_Object element;
    if (A.comp_.sz == 0)
        return 0;

    assert(ctx->gl->base);
    if (!xpost_stack_push(ctx->lo, ctx->es,
                xpost_operator_cons_opcode(ctx->opcode_shortcuts.forall)))
        return execstackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->es,
                xpost_operator_cons_opcode(ctx->opcode_shortcuts.cvx)))
        return execstackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->es, xpost_object_cvlit(P)))
        return execstackoverflow;

    /* update array descriptor before push for next iteration */
    interval = xpost_object_get_interval(A, 1, A.comp_.sz - 1);
    if (xpost_object_get_type(interval) == invalidtype)
        return rangecheck;

    if (!xpost_stack_push(ctx->lo, ctx->es, xpost_object_cvlit(interval)))
        return execstackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->es, P))
        return execstackoverflow;
    element = xpost_array_get(ctx, A, 0);
    /*
    if (xpost_object_is_exe(element)) {
        if (!xpost_stack_push(ctx->lo, ctx->es,
                    xpost_operator_cons_opcode(ctx->opcode_shortcuts.cvx)))
            return execstackoverflow;
    }
    */
    if (!xpost_stack_push(ctx->lo, ctx->os, element))
        return stackoverflow;

    return 0;
}

int xpost_oper_init_array_ops (Xpost_Context *ctx,
               Xpost_Object sd)
{
    Xpost_Operator *optab;
    Xpost_Object n,op;
    unsigned int optadr;
    int ret;

    assert(ctx->gl->base);
    xpost_memory_table_get_addr(ctx->gl,
            XPOST_MEMORY_TABLE_SPECIAL_OPERATOR_TABLE, &optadr);
    optab = (void *)(ctx->gl->base + optadr);

    op = xpost_operator_cons(ctx, "array", xpost_op_int_array, 1, 1,
            integertype);
    INSTALL;
    ret = xpost_dict_put(ctx, sd, xpost_name_cons(ctx, "["), mark);
    if (ret)
        return 0;
    op = xpost_operator_cons(ctx, "]", xpost_op_array_to_mark, 1, 0);
    INSTALL;
    op = xpost_operator_cons(ctx, "length", xpost_op_array_length, 1, 1,
            arraytype);
    INSTALL;
    op = xpost_operator_cons(ctx, "get", xpost_op_array_int_get, 1, 2,
            arraytype, integertype);
    INSTALL;
    op = xpost_operator_cons(ctx, "put", xpost_op_array_int_any_put, 0, 3,
            arraytype, integertype, anytype);
    INSTALL;
    op = xpost_operator_cons(ctx, "getinterval", xpost_op_array_int_int_getinterval, 1, 3,
            arraytype, integertype, integertype);
    INSTALL;
    op = xpost_operator_cons(ctx, "putinterval", xpost_op_array_int_array_putinterval, 0, 3,
            arraytype, integertype, arraytype);
    INSTALL;
    op = xpost_operator_cons(ctx, "aload", xpost_op_array_aload, 1, 1,
            arraytype);
    INSTALL;
    op = xpost_operator_cons(ctx, "astore", xpost_op_anyn_array_astore, 1, 1,
            arraytype);
    INSTALL;
    op = xpost_operator_cons(ctx, "copy", xpost_op_array_copy, 1, 2,
            arraytype, arraytype);
    INSTALL;
    op = xpost_operator_cons(ctx, "forall", xpost_op_array_proc_forall, 0, 2,
            arraytype, proctype);
    INSTALL;

    return 1;
}

