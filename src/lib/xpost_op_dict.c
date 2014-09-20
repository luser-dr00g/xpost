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
#include <stdio.h>

#include "xpost_log.h"
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
#include "xpost_op_dict.h"

int DEBUGLOAD = 0;
int xpost_op_any_where (Xpost_Context *ctx, Xpost_Object K); /* forward decl.
                                                   store uses where */

/* int  dict  dict
   create dictionary with capacity for int elements */
static
int xpost_op_int_dict(Xpost_Context *ctx,
           Xpost_Object I)
{
    Xpost_Object dic;
    dic = xpost_dict_cons (ctx, I.int_.val);
    if (xpost_object_get_type(dic) == nulltype)
        return VMerror;
    xpost_stack_push(ctx->lo, ctx->os, xpost_object_cvlit(dic));
    return 0;
}

/* -  <<  mark
   start dictionary construction */

/* mark k_1 v_1 ... k_N v_N  >>  dict
   construct dictionary from pairs on stack */
static
int xpost_op_dict_to_mark(Xpost_Context *ctx)
{
    int i;
    Xpost_Object d, k, v;
    Xpost_Object t;

    if (xpost_op_counttomark(ctx))
        return unmatchedmark;
    t = xpost_stack_pop(ctx->lo, ctx->os);
    if (xpost_object_get_type(t) == invalidtype)
        return stackunderflow;
    i = t.int_.val;
    if ((i % 2) == 1)
        return rangecheck;
    d = xpost_dict_cons (ctx, i);
    if (xpost_object_get_type(d) == nulltype)
        return VMerror;
    for ( ; i > 0; i -= 2){
        v = xpost_stack_pop(ctx->lo, ctx->os);
        if (xpost_object_get_type(v) == invalidtype)
            return stackunderflow;
        k = xpost_stack_pop(ctx->lo, ctx->os);
        if (xpost_object_get_type(k) == invalidtype)
            return stackunderflow;
        xpost_dict_put(ctx, d, k, v);
    }
    (void)xpost_stack_pop(ctx->lo, ctx->os); // pop mark
    xpost_stack_push(ctx->lo, ctx->os, d);
    return 0;
}

/* dict  length  int
   number of key-value pairs in dict */
static
int xpost_op_dict_length(Xpost_Context *ctx,
             Xpost_Object D)
{
    xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons(xpost_dict_length_memory (
                    xpost_context_select_memory(ctx, D) /*D.tag&FBANK?ctx->gl:ctx->lo*/,
                    D)));
    return 0;
}

/* dict  maxlength  int
   capacity of dict */
static
int xpost_op_dict_maxlength(Xpost_Context *ctx,
                Xpost_Object D)
{
    xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons(xpost_dict_max_length_memory (
                    xpost_context_select_memory(ctx, D) /*D.tag&FBANK?ctx->gl:ctx->lo*/,
                    D)));
    return 0;
}

/* dict  begin  -
   push dict on dict stack */
static
int xpost_op_dict_begin(Xpost_Context *ctx,
            Xpost_Object D)
{
    if (!xpost_stack_push(ctx->lo, ctx->ds, D))
        return dictstackoverflow;
    return 0;
}

/* -  end  -
   pop dict stack */
static
int xpost_op_end(Xpost_Context *ctx)
{
    if (xpost_stack_count(ctx->lo, ctx->ds) <= 3)
        return dictstackunderflow;
    (void)xpost_stack_pop(ctx->lo, ctx->ds);
    return 0;
}

/* key value  def  -
   associate key with value in current dict */
static
int xpost_op_any_any_def(Xpost_Context *ctx,
          Xpost_Object K,
          Xpost_Object V)
{
    int ret;
    //Xpost_Object D = xpost_stack_topdown_fetch(ctx->lo, ctx->ds, 0);
    //xpost_dict_dump_memory (xpost_context_select_memory(ctx, D), D); puts("");
    ret = xpost_dict_put(ctx, xpost_stack_topdown_fetch(ctx->lo, ctx->ds, 0), K, V);
    if (ret)
        return ret;
    //puts("!def!");
    //xpost_dict_dump_memory (xpost_context_select_memory(ctx, D), D); puts("");
    return 0;
}

/* key  load  value
   search dict stack for key and return associated value */
int xpost_op_any_load(Xpost_Context *ctx,
           Xpost_Object K)
{
    int i;
    int z = xpost_stack_count(ctx->lo, ctx->ds);
    if (DEBUGLOAD) {
        printf("\nload:");
        xpost_object_dump(K);
        xpost_stack_dump(ctx->lo, ctx->ds);
    }

    for (i = 0; i < z; i++) {
        Xpost_Object D = xpost_stack_topdown_fetch(ctx->lo,ctx->ds,i);

        if (DEBUGLOAD) {
            xpost_dict_dump_memory (xpost_context_select_memory(ctx, D), D);
            (void)puts("");
        }

        if (xpost_dict_known_key(ctx, xpost_context_select_memory(ctx, D), D, K)) {
            xpost_stack_push(ctx->lo, ctx->os, xpost_dict_get(ctx, D, K));
            return 0;
        }
    }

    if (DEBUGLOAD) {
        unsigned int names;
        xpost_memory_file_dump(ctx->lo);
        xpost_memory_table_dump(ctx->lo);
        xpost_memory_file_dump(ctx->gl);
        xpost_memory_table_dump(ctx->gl);
        xpost_memory_table_get_addr(ctx->gl,
                XPOST_MEMORY_TABLE_SPECIAL_NAME_STACK, &names);
        xpost_stack_dump(ctx->gl, names);
        xpost_object_dump(K);
    }

    return undefined;
}

/* key value  store  -
   replace topmost definition of key */
static
int xpost_op_any_store(Xpost_Context *ctx,
            Xpost_Object K,
            Xpost_Object V)
{
    Xpost_Object D;
    xpost_op_any_where(ctx, K);
    if (xpost_stack_pop(ctx->lo, ctx->os).int_.val) { /* booleantype */
        D = xpost_stack_pop(ctx->lo, ctx->os);
    } else {
        D = xpost_stack_topdown_fetch(ctx->lo, ctx->ds, 0);
    }
    xpost_dict_put(ctx, D, K, V);
    return 0;
}

/* dict key  get  any
   get value associated with key in dict */
static
int xpost_op_dict_any_get(Xpost_Context *ctx,
           Xpost_Object D,
           Xpost_Object K)
{
    Xpost_Object v;

    v = xpost_dict_get(ctx, D, K);
    if (xpost_object_get_type(v) == invalidtype)
        return undefined;
    xpost_stack_push(ctx->lo, ctx->os, v);
    return 0;
}

/* dict key value  put  -
   associate key with value in dict */
static
int xpost_op_dict_any_any_put(Xpost_Context *ctx,
            Xpost_Object D,
            Xpost_Object K,
            Xpost_Object V)
{
    xpost_dict_put(ctx, D, K, V);
    return 0;
}

/* dict key  undef  -
   remove key and its value in dict */
static
int xpost_op_dict_any_undef(Xpost_Context *ctx,
             Xpost_Object D,
             Xpost_Object K)
{
    XPOST_LOG_WARN("FIXME: undef doesn't adequately fix the chain");
    xpost_dict_undef(ctx, D, K);
    return 0;
}

/* dict key  known  bool
   test whether key is in dict */
static
int xpost_op_dict_any_known(Xpost_Context *ctx,
             Xpost_Object D,
             Xpost_Object K)
{
#if 0
    printf("\nknown: ");
    xpost_object_dump(D);
    xpost_dict_dump_memory (xpost_context_select_memory(ctx, D), D); puts("");
    xpost_object_dump(K);
#endif
    xpost_stack_push(ctx->lo, ctx->os, xpost_bool_cons(xpost_dict_known_key(ctx, xpost_context_select_memory(ctx, D), D, K)));
    return 0;
}


/* key  where  dict true -or- false
   find dict in which key is defined */
int xpost_op_any_where(Xpost_Context *ctx,
            Xpost_Object K)
{
    int i;
    int z = xpost_stack_count(ctx->lo, ctx->ds);
    for (i = 0; i < z; i++) {
        Xpost_Object D = xpost_stack_topdown_fetch(ctx->lo, ctx->ds, i);
        if (xpost_dict_known_key(ctx, xpost_context_select_memory(ctx, D), D, K)) {
            xpost_stack_push(ctx->lo, ctx->os, D);
            xpost_stack_push(ctx->lo, ctx->os, xpost_bool_cons(1));
            return 0;
        }
    }
    xpost_stack_push(ctx->lo, ctx->os, xpost_bool_cons(0));
    return 0;
}

/* dict1 dict2  copy  dict2
   copy contents of dict1 to dict2 */
static
int xpost_op_dict_copy(Xpost_Context *ctx,
           Xpost_Object S,
           Xpost_Object D)
{
    int i, sz;
    Xpost_Memory_File *mem;
    unsigned ad;
    Xpost_Object *tp;
    int ret;

    mem = xpost_context_select_memory(ctx, S);
    sz = xpost_dict_max_length_memory (mem, S);
    ret = xpost_memory_table_get_addr(mem, xpost_object_get_ent(S), &ad);
    if (!ret)
    {
        XPOST_LOG_ERR("cannot retrieve address for dict ent %u",
                xpost_object_get_ent(S));
        return VMerror;
    }
    tp = (void *)(mem->base + ad + sizeof(dichead));
    for (i=0; i < sz+1; i++) {
        if (xpost_object_get_type(tp[2 * i]) != nulltype) {
            xpost_dict_put(ctx, D, tp[2*i], tp[2*i+1]);
            tp = (void *)(mem->base + ad + sizeof(dichead)); /* recalc */
        }
    }
    xpost_stack_push(ctx->lo, ctx->os, D);
    return 0;
}

/* dict proc  forall  -
   execute proc for each key value pair in dict */
static
int xpost_op_dict_proc_forall (Xpost_Context *ctx,
               Xpost_Object D,
               Xpost_Object P)
{
    Xpost_Memory_File *mem = xpost_context_select_memory(ctx, D);
    assert(mem->base);
    D.comp_.sz = xpost_dict_max_length_memory (mem, D); // cache size locally
    if (D.comp_.off <= D.comp_.sz) { // not finished?
        unsigned ad;
        Xpost_Object *tp; /* dict Table Pointer, indexed by pairs,
                             tp[2 * i] for a key and tp[2 * i + 1]
                             for that key's associated value */
        int ret;

        ret = xpost_memory_table_get_addr(mem, xpost_object_get_ent(D), &ad);
        if (!ret)
        {
            XPOST_LOG_ERR("cannot retrieve address for dict ent %u",
                    xpost_object_get_ent(D));
            return VMerror;
        }
        tp = (void *)(mem->base + ad + sizeof(dichead)); 

        for ( ; D.comp_.off <= D.comp_.sz; ++D.comp_.off) { // find next pair
            if (xpost_object_get_type(tp[2 * D.comp_.off]) != nulltype) { // found
                Xpost_Object k,v;

                k = tp[2 * D.comp_.off];
                if (xpost_object_get_type(k) == extendedtype)
                    k = xpost_dict_convert_extended_to_number(k);
                v = tp[2 * D.comp_.off + 1];

                if (!xpost_stack_push(ctx->lo, ctx->os, k))
                    return stackoverflow;
                if (!xpost_stack_push(ctx->lo, ctx->os, v))
                    return stackoverflow;

                if (!xpost_stack_push(ctx->lo, ctx->es,
                            xpost_operator_cons_opcode(ctx->opcode_shortcuts.forall)))
                    return execstackoverflow;
                if (!xpost_stack_push(ctx->lo, ctx->es,
                            xpost_operator_cons_opcode(ctx->opcode_shortcuts.cvx)))
                    return execstackoverflow;
                if (!xpost_stack_push(ctx->lo, ctx->es,
                            xpost_object_cvlit(P)))
                    return execstackoverflow;

                ++D.comp_.off; /* update offset in dict
                                  before push for next iteration */
                if (!xpost_stack_push(ctx->lo, ctx->es, D))
                    return execstackoverflow;

                if (!xpost_stack_push(ctx->lo, ctx->es, P))
                    return execstackoverflow;

                return 0; /* loop continues by ps continuation.
                             thus, no need to recalc pointer since
                             this function is re-entered from the
                             beginning, with a new dict with ++D.comp_.off */
            }
        }
    }
    return 0;
}

/* -  currentdict  dict
   push current dict on operand stack */
static
int xpost_op_currentdict(Xpost_Context *ctx)
{
    if (!xpost_stack_push(ctx->lo, ctx->os, xpost_stack_topdown_fetch(ctx->lo, ctx->ds, 0)))
        return stackoverflow;
    return 0;
}

/* -  errordict  dict   % error handler dictionary : err.ps
   -  $error  dict      % error control and status dictionary : err.ps
   -  systemdict  dict  % system dictionary : op.c init.ps
   -  userdict  dict    % writeable dictionary in local VM : xpost_context.c
   -  globaldict  dict  % writeable dictionary in global VM : xpost_context.c
   %-  statusdict  dict  % product-dependent dictionary
   */

/* -  countdictstack  int
   count elements on dict stack */
static
int xpost_op_countdictstack(Xpost_Context *ctx)
{
    if (!xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons(xpost_stack_count(ctx->lo, ctx->ds))))
        return stackoverflow;
    return 0;
}

/* array  dictstack  subarray
   copy dict stack into array */
static
int xpost_op_array_dictstack(Xpost_Context *ctx,
                Xpost_Object A)
{
    Xpost_Object subarr;
    int z = xpost_stack_count(ctx->lo, ctx->ds);
    int i;
    for (i=0; i < z; i++)
        xpost_array_put(ctx, A, i, xpost_stack_bottomup_fetch(ctx->lo, ctx->ds, i));
    subarr = xpost_object_get_interval(A, 0, z);
    if (xpost_object_get_type(subarr) == invalidtype)
        return rangecheck;
    xpost_stack_push(ctx->lo, ctx->os, subarr);
    return 0;
}

static
int xpost_op_cleardictstack(Xpost_Context *ctx)
{
    int z = xpost_stack_count(ctx->lo, ctx->ds);
    while (z-- > 3) {
        (void)xpost_stack_pop(ctx->lo, ctx->ds);
    }
    /*
    Xpost_Stack *ds;
    unsigned int dsaddr;
    int ret;

    ret = xpost_memory_table_get_addr(ctx->lo, ctx->ds, &dsaddr);
    if (!ret) {
        XPOST_LOG_ERR("cannot retrieve address for dict stack");
        return VMerror;
    }
    ds = (Xpost_Stack *)(ctx->lo->base + dsaddr);
    ds->top = 3;
    */
    return 0;
}

int xpost_oper_init_dict_ops (Xpost_Context *ctx,
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
    op = xpost_operator_cons(ctx, "dict", (Xpost_Op_Func)xpost_op_int_dict, 1, 1, integertype);
    INSTALL;
    ret = xpost_dict_put(ctx, sd, xpost_name_cons(ctx, "<<"), mark);
    if (ret)
        return 0;
    op = xpost_operator_cons(ctx, ">>", (Xpost_Op_Func)xpost_op_dict_to_mark, 1, 0);
    INSTALL;
    op = xpost_operator_cons(ctx, "length", (Xpost_Op_Func)xpost_op_dict_length, 1, 1, dicttype);
    INSTALL;
    op = xpost_operator_cons(ctx, "maxlength", (Xpost_Op_Func)xpost_op_dict_maxlength, 1, 1, dicttype);
    INSTALL;
    op = xpost_operator_cons(ctx, "begin", (Xpost_Op_Func)xpost_op_dict_begin, 0, 1, dicttype);
    INSTALL;
    op = xpost_operator_cons(ctx, "end", (Xpost_Op_Func)xpost_op_end, 0, 0);
    INSTALL;
    op = xpost_operator_cons(ctx, "def", (Xpost_Op_Func)xpost_op_any_any_def, 0, 2, anytype, anytype);
    INSTALL;
    op = xpost_operator_cons(ctx, "load", (Xpost_Op_Func)xpost_op_any_load, 1, 1, anytype);
    INSTALL;
    ctx->opcode_shortcuts.load = op.mark_.padw;
    op = xpost_operator_cons(ctx, "store", (Xpost_Op_Func)xpost_op_any_store, 0, 2, anytype, anytype);
    INSTALL;
    op = xpost_operator_cons(ctx, "get", (Xpost_Op_Func)xpost_op_dict_any_get, 1, 2, dicttype, anytype);
    INSTALL;
    op = xpost_operator_cons(ctx, "put", (Xpost_Op_Func)xpost_op_dict_any_any_put, 1, 3,
            dicttype, anytype, anytype);
    INSTALL;
    op = xpost_operator_cons(ctx, "undef", (Xpost_Op_Func)xpost_op_dict_any_undef, 0, 2, dicttype, anytype);
    INSTALL;
    op = xpost_operator_cons(ctx, "known", (Xpost_Op_Func)xpost_op_dict_any_known, 1, 2, dicttype, anytype);
    INSTALL;
    op = xpost_operator_cons(ctx, "where", (Xpost_Op_Func)xpost_op_any_where, 2, 1, anytype);
    INSTALL;
    op = xpost_operator_cons(ctx, "copy", (Xpost_Op_Func)xpost_op_dict_copy, 1, 2, dicttype, dicttype);
    INSTALL;
    op = xpost_operator_cons(ctx, "forall", (Xpost_Op_Func)xpost_op_dict_proc_forall, 0, 2, dicttype, proctype);
    INSTALL;
    ctx->opcode_shortcuts.forall = op.mark_.padw;
    op = xpost_operator_cons(ctx, "currentdict", (Xpost_Op_Func)xpost_op_currentdict, 1, 0);
    INSTALL;
    op = xpost_operator_cons(ctx, "countdictstack", (Xpost_Op_Func)xpost_op_countdictstack, 1, 0);
    INSTALL;
    op = xpost_operator_cons(ctx, "dictstack", (Xpost_Op_Func)xpost_op_array_dictstack, 1, 1, arraytype);
    INSTALL;
    op = xpost_operator_cons(ctx, "cleardictstack", (Xpost_Op_Func)xpost_op_cleardictstack, 0, 0);
    INSTALL;
    return 0;
}

