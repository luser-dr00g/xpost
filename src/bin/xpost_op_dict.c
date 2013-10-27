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
#include <stdio.h>

#include "xpost_memory.h"
#include "xpost_object.h"
#include "xpost_stack.h"
#include "xpost_save.h"
#include "xpost_interpreter.h"
#include "xpost_error.h"
#include "xpost_string.h"
#include "xpost_array.h"
#include "xpost_dict.h"
#include "xpost_name.h"
#include "xpost_operator.h"
#include "xpost_op_stack.h"
#include "xpost_op_dict.h"

int DEBUGLOAD = 0;
void Awhere(context *ctx, Xpost_Object K); /* forward decl */

/* int  dict  dict
   create dictionary with capacity for int elements */
static
void Idict(context *ctx,
           Xpost_Object I)
{
    push(ctx->lo, ctx->os, xpost_object_cvlit(consbdc(ctx, I.int_.val)));
}

/* -  <<  mark
   start dictionary construction */

/* mark k_1 v_1 ... k_N v_N  >>  dict
   construct dictionary from pairs on stack */
static
void dictomark(context *ctx)
{
    int i;
    Xpost_Object d, k, v;
    Zcounttomark(ctx);
    i = pop(ctx->lo, ctx->os).int_.val;
    d = consbdc(ctx, i);
    for ( ; i > 0; i -= 2){
        v = pop(ctx->lo, ctx->os);
        k = pop(ctx->lo, ctx->os);
        bdcput(ctx, d, k, v);
    }
    (void)pop(ctx->lo, ctx->os); // pop mark
    push(ctx->lo, ctx->os, d);
}

/* dict  length  int
   number of key-value pairs in dict */
static
void Dlength(context *ctx,
             Xpost_Object D)
{
    push(ctx->lo, ctx->os, xpost_cons_int(diclength(
                    bank(ctx, D) /*D.tag&FBANK?ctx->gl:ctx->lo*/,
                    D)));
}

/* dict  maxlength  int
   capacity of dict */
static
void Dmaxlength(context *ctx,
                Xpost_Object D)
{
    push(ctx->lo, ctx->os, xpost_cons_int(dicmaxlength(
                    bank(ctx, D) /*D.tag&FBANK?ctx->gl:ctx->lo*/,
                    D)));
}

/* dict  begin  -
   push dict on dict stack */
static
void Dbegin(context *ctx,
            Xpost_Object D)
{
    push(ctx->lo, ctx->ds, D);
}

/* -  end  -
   pop dict stack */
static
void Zend(context *ctx)
{
    if (count(ctx->lo, ctx->ds) <= 3)
        error(dictstackunderflow, "end");
    (void)pop(ctx->lo, ctx->ds);
}

/* key value  def  -
   associate key with value in current dict */
static
void Adef(context *ctx,
          Xpost_Object K,
          Xpost_Object V)
{
    //Xpost_Object D = top(ctx->lo, ctx->ds, 0);
    //dumpdic(bank(ctx, D), D); puts("");
    bdcput(ctx, top(ctx->lo, ctx->ds, 0), K, V);
    //puts("!def!");
    //dumpdic(bank(ctx, D), D); puts("");
}

/* key  load  value
   search dict stack for key and return associated value */
void Aload(context *ctx,
           Xpost_Object K)
{
    int i;
    int z = count(ctx->lo, ctx->ds);
    if (DEBUGLOAD) {
        printf("\nload:");
        xpost_object_dump(K);
        dumpstack(ctx->lo, ctx->ds);
    }

    for (i = 0; i < z; i++) {
        Xpost_Object D = top(ctx->lo,ctx->ds,i);

    if (DEBUGLOAD) {
        dumpdic(bank(ctx, D), D);
        (void)puts("");
    }

        if (dicknown(ctx, bank(ctx, D), D, K)) {
            push(ctx->lo, ctx->os, bdcget(ctx, D, K));
            return;
        }
    }

    if (DEBUGLOAD) {
        dumpmfile(ctx->lo);
        dumpmtab(ctx->lo, 0);
        dumpmfile(ctx->gl);
        dumpmtab(ctx->gl, 0);
        dumpstack(ctx->gl, adrent(ctx->gl, NAMES));
        xpost_object_dump(K);
    }

    error(undefined, "Aload");
}

/* key value  store  -
   replace topmost definition of key */
static
void Astore(context *ctx,
            Xpost_Object K,
            Xpost_Object V)
{
    Xpost_Object D;
    Awhere(ctx, K);
    if (pop(ctx->lo, ctx->os).int_.val) {
        D = pop(ctx->lo, ctx->os);
    } else {
        D = top(ctx->lo, ctx->ds, 0);
    }
    bdcput(ctx, D, K, V);
}

/* dict key  get  any
   get value associated with key in dict */
static
void DAget(context *ctx,
           Xpost_Object D,
           Xpost_Object K)
{
    push(ctx->lo, ctx->os, bdcget(ctx, D, K));
}

/* dict key value  put  -
   associate key with value in dict */
static
void DAAput(context *ctx,
            Xpost_Object D,
            Xpost_Object K,
            Xpost_Object V)
{
    bdcput(ctx, D, K, V);
}

/* dict key  undef  -
   remove key and its value in dict */
static
void DAundef(context *ctx,
             Xpost_Object D,
             Xpost_Object K)
{
    bdcundef(ctx, D, K);
}

/* dict key  known  bool
   test whether key is in dict */
static
void DAknown(context *ctx,
             Xpost_Object D,
             Xpost_Object K)
{
#if 0
    printf("\nknown: ");
    xpost_object_dump(D);
    dumpdic(bank(ctx, D), D); puts("");
    xpost_object_dump(K);
#endif
    push(ctx->lo, ctx->os, xpost_cons_bool(dicknown(ctx, bank(ctx, D), D, K)));
}


/* key  where  dict true -or- false
   find dict in which key is defined */
void Awhere(context *ctx,
            Xpost_Object K)
{
    int i;
    int z = count(ctx->lo, ctx->ds);
    for (i = 0; i < z; i++) {
        Xpost_Object D = top(ctx->lo, ctx->ds, i);
        if (dicknown(ctx, bank(ctx, D), D, K)) {
            push(ctx->lo, ctx->os, D);
            push(ctx->lo, ctx->os, xpost_cons_bool(true));
            return;
        }
    }
    push(ctx->lo, ctx->os, xpost_cons_bool(false));
}

/* dict1 dict2  copy  dict2
   copy contents of dict1 to dict2 */
static
void Dcopy(context *ctx,
           Xpost_Object S,
           Xpost_Object D)
{
    int i, sz;
    mfile *mem;
    unsigned ad;
    dichead *dp;
    Xpost_Object *tp;
    mem = bank(ctx, S);
    sz = dicmaxlength(mem, S);
    ad = adrent(mem, S.comp_.ent);
    dp = (void *)(mem->base + ad);
    tp = (void *)(mem->base + ad + sizeof(dichead));
    for (i=0; i < sz+1; i++) {
        if (xpost_object_get_type(tp[2 * i]) != nulltype) {
            bdcput(ctx, D, tp[2*i], tp[2*i+1]);
        }
    }
    push(ctx->lo, ctx->os, D);
}

static
void DPforall (context *ctx,
               Xpost_Object D,
               Xpost_Object P)
{
    mfile *mem = bank(ctx, D);
    assert(mem->base);
    D.comp_.sz = dicmaxlength(mem, D); // stash size locally
    if (D.comp_.off <= D.comp_.sz) { // not finished?
        unsigned ad;
        dichead *dp;
        Xpost_Object *tp;
        ad = adrent(mem, D.comp_.ent);
        dp = (void *)(mem->base + ad); 
        tp = (void *)(mem->base + ad + sizeof(dichead)); 

        for ( ; D.comp_.off <= D.comp_.sz; ++D.comp_.off) { // find next pair
            if (xpost_object_get_type(tp[2 * D.comp_.off]) != nulltype) { // found
                Xpost_Object k,v;

                k = tp[2 * D.comp_.off];
                if (xpost_object_get_type(k) == extendedtype)
                    k = unextend(k);
                v = tp[2 * D.comp_.off + 1];
                push(ctx->lo, ctx->os, k);
                push(ctx->lo, ctx->os, v);

                //push(ctx->lo, ctx->es, consoper(ctx, "forall", NULL,0,0));
                push(ctx->lo, ctx->es, operfromcode(ctx->opcuts.forall));
                //push(ctx->lo, ctx->es, consoper(ctx, "cvx", NULL,0,0));
                push(ctx->lo, ctx->es, operfromcode(ctx->opcuts.cvx));
                push(ctx->lo, ctx->es, xpost_object_cvlit(P));
                ++D.comp_.off;
                push(ctx->lo, ctx->es, D);

                push(ctx->lo, ctx->es, P);
                return;
            }
        }
    }
}

/* -  currentdict  dict
   push current dict on operand stack */
static
void Zcurrentdict(context *ctx)
{
    push(ctx->lo, ctx->os, top(ctx->lo, ctx->ds, 0));
}

/* -  errordict  dict   % error handler dictionary : err.ps
   -  $error  dict      % error control and status dictionary : err.ps
   -  systemdict  dict  % system dictionary : op.c init.ps
   -  userdict  dict    % writeable dictionary in local VM : itp.c
   %-  globaldict  dict  % writeable dictionary in global VM
   %-  statusdict  dict  % product-dependent dictionary
   */

/* -  countdictstack  int
   count elements on dict stack */
static
void Zcountdictstack(context *ctx)
{
    push(ctx->lo, ctx->os, xpost_cons_int(count(ctx->lo, ctx->ds)));
}

/* array  dictstack  subarray
   copy dict stack into array */
static
void Adictstack(context *ctx,
                Xpost_Object A)
{
    int z = count(ctx->lo, ctx->ds);
    int i;
    for (i=0; i < z; i++)
        barput(ctx, A, i, bot(ctx->lo, ctx->ds, i));
    push(ctx->lo, ctx->os, arrgetinterval(A, 0, z));
}

static
void cleardictstack(context *ctx)
{
    int z = count(ctx->lo, ctx->ds);
    while (z-- > 3) {
        (void)pop(ctx->lo, ctx->ds);
    }
}

void initopdi(context *ctx,
              Xpost_Object sd)
{
    oper *optab;
    Xpost_Object n,op;
    assert(ctx->gl->base);
    optab = (void *)(ctx->gl->base + adrent(ctx->gl, OPTAB));
    op = consoper(ctx, "dict", Idict, 1, 1, integertype); INSTALL;
    bdcput(ctx, sd, consname(ctx, "<<"), mark);
    op = consoper(ctx, ">>", dictomark, 1, 0); INSTALL;
    op = consoper(ctx, "length", Dlength, 1, 1, dicttype); INSTALL;
    op = consoper(ctx, "maxlength", Dmaxlength, 1, 1, dicttype); INSTALL;
    op = consoper(ctx, "begin", Dbegin, 0, 1, dicttype); INSTALL;
    op = consoper(ctx, "end", Zend, 0, 0); INSTALL;
    op = consoper(ctx, "def", Adef, 0, 2, anytype, anytype); INSTALL;
    op = consoper(ctx, "load", Aload, 1, 1, anytype); INSTALL;
    ctx->opcuts.load = op.mark_.padw;
    op = consoper(ctx, "store", Astore, 0, 2, anytype, anytype); INSTALL;
    op = consoper(ctx, "get", DAget, 1, 2, dicttype, anytype); INSTALL;
    op = consoper(ctx, "put", DAAput, 1, 3,
            dicttype, anytype, anytype); INSTALL;
    //op = consoper(ctx, "undef", DAundef, 0, 2, dicttype, anytype); INSTALL;
    op = consoper(ctx, "known", DAknown, 1, 2, dicttype, anytype); INSTALL;
    op = consoper(ctx, "where", Awhere, 2, 1, anytype); INSTALL;
    op = consoper(ctx, "copy", Dcopy, 1, 2, dicttype, dicttype); INSTALL;
    op = consoper(ctx, "forall", DPforall, 0, 2, dicttype, proctype); INSTALL;
    ctx->opcuts.forall = op.mark_.padw;
    op = consoper(ctx, "currentdict", Zcurrentdict, 1, 0); INSTALL;
    op = consoper(ctx, "countdictstack", Zcountdictstack, 1, 0); INSTALL;
    op = consoper(ctx, "dictstack", Adictstack, 1, 1, arraytype); INSTALL;
    op = consoper(ctx, "cleardictstack", cleardictstack, 0, 0); INSTALL;
}

