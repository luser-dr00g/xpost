/*
 * Xpost - a Level-2 Postscript interpreter
 * Copyright (C) 2013-2016, Michael Joshua Ryan
 * Copyright (C) 2013, Thorsten Behrens
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
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h> /* NULL */
#include <string.h> /* memcpy */

#include "xpost.h"
#include "xpost_log.h"
#include "xpost_memory.h"  // accesses mfile
#include "xpost_object.h"  // operators are objects
#include "xpost_stack.h"  // uses a stack for argument passing
#include "xpost_free.h"  // grow signatures using xpost_free_realloc
#include "xpost_context.h"
#include "xpost_error.h"  // operator functions may throw errors
#include "xpost_string.h"  // uses string function to dump operator name
#include "xpost_name.h"  // operator objects have associated names
#include "xpost_dict.h"  // install operators in systemdict, a dict

//#include "xpost_interpreter.h"  // works with context struct
#include "xpost_operator.h"  // double-check prototypes


/* convert an integertype object to a realtype object */
static
Xpost_Object _promote_integer_to_real(Xpost_Object o)
{
    return xpost_real_cons((real)o.int_.val);
}

/* copied from the header file for reference:
   typedef struct Xpost_Signature {
   int (*fp)(Xpost_Context *ctx);
   int in;
   unsigned t;
   int (*checkstack)(Xpost_Context *ctx);
   int out;
   } Xpost_Signature;

   typedef struct Xpost_Operator {
   unsigned name;
   int n; // number of sigs
   unsigned sigadr;
   } Xpost_Operator;

   enum typepat ( anytype = stringtype + 1,
   floattype, numbertype, proctype };

   #define MAXOPS 20
*/

/* the number of ops, at any given time. */
static
int _xpost_noops = 0;

static
int _stack_none(Xpost_Context *ctx)
{
    (void)ctx;
    return 0;
}

static
int _stack_int(Xpost_Context *ctx)
{
    Xpost_Object s0;
    s0 = xpost_stack_topdown_fetch(ctx->lo, ctx->os, 0);
    switch(xpost_object_get_type(s0))
    {
        case invalidtype:
            return stackunderflow;
        case integertype:
            return 0;
        default:
            return typecheck;
    }
}

static
int _stack_real(Xpost_Context *ctx)
{
    Xpost_Object s0;
    s0 = xpost_stack_topdown_fetch(ctx->lo, ctx->os, 0);
    switch(xpost_object_get_type(s0))
    {
        case invalidtype:
            return stackunderflow;
        case realtype:
            return 0;
        default:
            return typecheck;
    }
}

static
int _stack_float(Xpost_Context *ctx)
{
    Xpost_Object s0;
    s0 = xpost_stack_topdown_fetch(ctx->lo, ctx->os, 0);
    switch(xpost_object_get_type(s0))
    {
        case invalidtype:
            return stackunderflow;
        case integertype:
            xpost_stack_topdown_replace(ctx->lo, ctx->os, 0, s0 = _promote_integer_to_real(s0));
        case realtype:
            return 0;
        default:
            return typecheck;
    }
}

static
int _stack_any(Xpost_Context *ctx)
{
    if (xpost_stack_count(ctx->lo, ctx->os) >= 1)
        return 0;
    return stackunderflow;
}

static
int _stack_bool_bool(Xpost_Context *ctx)
{
    Xpost_Object s0, s1;
    s0 = xpost_stack_topdown_fetch(ctx->lo, ctx->os, 0);
    switch(xpost_object_get_type(s0))
    {
        case invalidtype:
            return stackunderflow;
        case booleantype:
            s1 = xpost_stack_topdown_fetch(ctx->lo, ctx->os, 1);
            switch(xpost_object_get_type(s1))
            {
                case invalidtype:
                    return stackunderflow;
                case booleantype:
                    return 0;
                default:
                    return typecheck;
            }
        default:
            return typecheck;
    }
}

static
int _stack_int_int(Xpost_Context *ctx)
{
    Xpost_Object s0, s1;
    s0 = xpost_stack_topdown_fetch(ctx->lo, ctx->os, 0);
    switch(xpost_object_get_type(s0))
    {
        case invalidtype:
            return stackunderflow;
        case integertype:
            s1 = xpost_stack_topdown_fetch(ctx->lo, ctx->os, 1);
            switch(xpost_object_get_type(s1))
            {
                case invalidtype:
                    return stackunderflow;
                case integertype:
                    return 0;
                default:
                    return typecheck;
            }
        default:
            return typecheck;
    }
}

static
int _stack_float_float(Xpost_Context *ctx)
{
    Xpost_Object s0, s1;
    s0 = xpost_stack_topdown_fetch(ctx->lo, ctx->os, 0);
    switch(xpost_object_get_type(s0))
    {
        case invalidtype:
            return stackunderflow;
        case integertype:
            xpost_stack_topdown_replace(ctx->lo, ctx->os, 0, s0 = _promote_integer_to_real(s0));
        case realtype:
            s1 = xpost_stack_topdown_fetch(ctx->lo, ctx->os, 1);
            switch(xpost_object_get_type(s1))
            {
                case invalidtype:
                    return stackunderflow;
                case integertype:
                    xpost_stack_topdown_replace(ctx->lo, ctx->os, 1, s1 = _promote_integer_to_real(s1));
                case realtype:
                    return 0;
                default:
                    return typecheck;
            }
        default:
            return typecheck;
    }
}

static
int _stack_number_number(Xpost_Context *ctx)
{
    Xpost_Object s0, s1;
    s0 = xpost_stack_topdown_fetch(ctx->lo, ctx->os, 0);
    switch(xpost_object_get_type(s0))
    {
        case invalidtype:
            return stackunderflow;
        case integertype: /* fallthrough */
        case realtype:
            s1 = xpost_stack_topdown_fetch(ctx->lo, ctx->os, 1);
            switch(xpost_object_get_type(s1))
            {
                case invalidtype:
                    return stackunderflow;
                case integertype: /* fallthrough */
                case realtype:
                    return 0;
                default:
                    return typecheck;
            }
        default:
            return typecheck;
    }
}

static
int _stack_any_any(Xpost_Context *ctx)
{
    if (xpost_stack_count(ctx->lo, ctx->os) >= 2)
        return 0;
    return stackunderflow;
}

typedef struct {
    int (*checkstack)(Xpost_Context *ctx);
    int n;
    int t[8];
} Xpost_Check_Stack;

static
Xpost_Check_Stack _check_stack_funcs[] = {
    { _stack_none, 0, { 0, 0, 0, 0, 0, 0, 0, 0} },
    { _stack_int, 1, { integertype } },
    { _stack_real, 1, { realtype } },
    { _stack_float, 1, { floattype } },
    { _stack_any, 1, { anytype } },
    { _stack_bool_bool, 2, { booleantype, booleantype } },
    { _stack_int_int, 2, { integertype, integertype } },
    { _stack_float_float, 2, { floattype, floattype } },
    { _stack_number_number, 2, { numbertype, numbertype } },
    { _stack_any_any, 2, { anytype, anytype } }
};


/* allocate the OPTAB structure in VM */
int xpost_operator_init_optab(Xpost_Context *ctx)
{
    unsigned ent;
    Xpost_Memory_Table *tab;
    int ret;

    ret = xpost_memory_table_alloc(ctx->gl, MAXOPS * sizeof(Xpost_Operator), 0, &ent);
    if (!ret)
    {
        return 0;
    }
    tab = &ctx->gl->table;
    assert(ent == XPOST_MEMORY_TABLE_SPECIAL_OPERATOR_TABLE);
    tab->tab[ent].sz = 0; // so gc will ignore it
    //printf("ent: %d\nOPTAB: %d\n", ent, (int)XPOST_MEMORY_TABLE_SPECIAL_OPERATOR_TABLE);

    return 1;
}

/* print a dump of the operator struct given opcode */
void xpost_operator_dump(Xpost_Context *ctx,
                         int opcode)
{
    Xpost_Operator *optab;
    Xpost_Operator op;
    Xpost_Object o;
    Xpost_Object str;
    char *s;
    Xpost_Signature *sig;
    unsigned int adr;
    uintptr_t fp;

    xpost_memory_table_get_addr(ctx->gl,
                                XPOST_MEMORY_TABLE_SPECIAL_OPERATOR_TABLE, &adr);
    optab = (void *)(ctx->gl->base + adr);
    op = optab[opcode];
    o.mark_.tag = nametype | XPOST_OBJECT_TAG_DATA_FLAG_BANK;
    o.mark_.pad0 = 0;
    o.mark_.padw = op.name;
    str = xpost_name_get_string(ctx, o);
    s = xpost_string_get_pointer(ctx, str);
    sig = (void *)(ctx->gl->base + op.sigadr);
    memcpy(&fp, &sig[0].fp, sizeof fp);
    printf("<operator %d %d:%*s %p>",
           opcode,
           str.comp_.sz, str.comp_.sz, s,
           (void *)fp );
}

/* create operator object by opcode number */
Xpost_Object xpost_operator_cons_opcode(int opcode)
{
    Xpost_Object op;
    op.mark_.tag = operatortype;
    op.mark_.pad0 = 0;
    op.mark_.padw = opcode;
    if (opcode >= _xpost_noops)
    {
        XPOST_LOG_ERR("opcode does not index a valid operator");
        return null;
    }
    return op;
}

/* construct an operator object by name
   If function-pointer fp is not NULL, attempts to install a new operator
   in OPTAB, otherwise just perform a lookup.
   If installing a new operator, out and in specify the number of
   output values the function may yield and the number of input
   values whose presence and types should be checked.
   There should follow 'in' number of typenames passed after 'in'.
*/
Xpost_Object xpost_operator_cons(Xpost_Context *ctx,
                                 const char *name,
                                 /*@null@*/ Xpost_Op_Func fp,
                                 int out,
                                 int in, ...)
{
    Xpost_Object nm;
    Xpost_Object o;
    int opcode;
    int i;
    unsigned si;
    unsigned t;
    unsigned vmmode;
    Xpost_Signature *sp;
    Xpost_Operator *optab;
    Xpost_Operator  op;
    unsigned int optadr;
    int ret;

    //fprintf(stderr, "name: %s\n", name);
    assert(ctx->gl->base);

    ret = xpost_memory_table_get_addr(ctx->gl,
                                      XPOST_MEMORY_TABLE_SPECIAL_OPERATOR_TABLE, &optadr);
    if (!ret)
    {
        XPOST_LOG_ERR("cannot load optab!");
        return null;
    }
    optab = (void *)(ctx->gl->base + optadr);

    if (!(in < XPOST_STACK_SEGMENT_SIZE))
    {
        printf("!(in < XPOST_STACK_SEGMENT_SIZE) in xpost_operator_cons(%s, %d. %d)\n", name, out, in);
        fprintf(stderr, "!(in < XPOST_STACK_SEGMENT_SIZE) in xpost_operator_cons(%s, %d. %d)\n", name, out, in);
        exit(EXIT_FAILURE);
    }
    //assert(in < XPOST_STACK_SEGMENT_SIZE); // or else xpost_operator_exec can't call it using HOLD

    vmmode=ctx->vmmode;
    ctx->vmmode = GLOBAL;
    nm = xpost_name_cons(ctx, name);
    if (xpost_object_get_type(nm) == invalidtype)
        return invalid;
    ctx->vmmode = vmmode;

    optab = (void *)(ctx->gl->base + optadr);
    for (opcode = 0; optab[opcode].name != nm.mark_.padw; opcode++)
    {
        if (opcode == _xpost_noops) break;
    }

    /* install a new signature (prototype) */
    if (fp)
    {
        if (opcode == _xpost_noops)
        { /* a new operator */
            unsigned adr;
            if (_xpost_noops == MAXOPS-1)
            {
                XPOST_LOG_ERR("optab too small in xpost_operator.h");
                XPOST_LOG_ERR("operator %s NOT installed", name);
                return null;
            }
            if (!xpost_memory_file_alloc(ctx->gl, sizeof(Xpost_Signature), &adr))
            {
                XPOST_LOG_ERR("cannot allocate signature block");
                XPOST_LOG_ERR("operator %s NOT installed", name);
                return null;
            }
            optab = (void *)(ctx->gl->base + optadr); // recalc
            op.name = nm.mark_.padw;
            op.n = 1;
            op.sigadr = adr;
            optab[opcode] = op;
            ++_xpost_noops;
            si = 0;
        }
        else
        { /* increase sig table by 1 */
            t = xpost_free_realloc(ctx->gl,
                                   optab[opcode].sigadr,
                                   optab[opcode].n * sizeof(Xpost_Signature),
                                   (optab[opcode].n + 1) * sizeof(Xpost_Signature));
            if (!t)
            {
                XPOST_LOG_ERR("cannot allocate new sig table");
                XPOST_LOG_ERR("operator %s NOT installed", name);
                return null;
            }
            optab = (void *)(ctx->gl->base + optadr); // recalc
            optab[opcode].sigadr = t;

            si = optab[opcode].n++; /* index of last sig */
        }

        sp = (void *)(ctx->gl->base + optab[opcode].sigadr);
        {
            unsigned int ad;
            if (!xpost_memory_file_alloc(ctx->gl, in, &ad))
            {
                XPOST_LOG_ERR("cannot allocate type block");
                XPOST_LOG_ERR("operator %s NOT installed", name);
                return null;
            }
            optab = (void *)(ctx->gl->base + optadr); // recalc
            sp = (void *)(ctx->gl->base + optab[opcode].sigadr); // recalc
            sp[si].t = ad;
        }
        {
            va_list args;
            byte *b = (void *)(ctx->gl->base + sp[si].t);
            va_start(args, in);
            for (i = in-1; i >= 0; i--) {
                b[i] = va_arg(args, int);
            }
            va_end(args);
            sp[si].in = in;
            sp[si].out = out;
            sp[si].fp = (int(*)(Xpost_Context *))fp;
            sp[si].checkstack = NULL;
            {
                int j;
                int k;
                int pass;
                for (j = 0; j < (int)(sizeof _check_stack_funcs/sizeof*_check_stack_funcs); j++)
                {
                    if (_check_stack_funcs[j].n == sp[si].in)
                    {
                        pass = 1;
                        for (k=0; k < _check_stack_funcs[j].n; k++)
                        {
                            if (b[k] != _check_stack_funcs[j].t[k])
                            {
                                pass = 0;
                                break;
                            }
                        }
                        if (pass)
                        {
                            sp[si].checkstack = _check_stack_funcs[j].checkstack;
                            break;
                        }
                    }
                }
            }
            //sp[si].checkstack = NULL;
        }
    }
    else if (opcode == _xpost_noops)
    {
        XPOST_LOG_ERR("operator not found");
        return null;
    }

    o.tag = operatortype;
    o.mark_.padw = opcode;
    return o;
}

/* clear hold and pop n objects from opstack to hold stack.
   The hold stack is used as temporary storage to hold the
   arguments for an operator-function call.
   If the operator-function does not itself call xpost_operator_exec,
   the arguments may be restored by xpost_interpreter.c:_on_error().
   xpost_operator_exec checks its argument with ctx->currentobject
   and sets a flag indicating consistency which is then checked by
   on_error()
   Composite Object constructors also add their objects to the
   hold stack, in defense against garbage collection occurring
   from a subsequent allocation before the object is returned
   to the stack.
   on_error() also uses the number of args from ctx->currentobject.mark_.pad0
   instead of the stack count so these extra gc-defense stack objects
   will not be erroneously returned to postscript in response to an
   operator error.
*/
static
void _xpost_operator_push_args_to_hold(Xpost_Context *ctx,
                                       Xpost_Memory_File *mem,
                                       unsigned stacadr,
                                       int n)
{
    int j;

    assert(n < XPOST_MEMORY_TABLE_SIZE);
    xpost_stack_clear(ctx->lo, ctx->hold);
    for (j = n; j--;)
    {  /* copy */
        xpost_stack_push(ctx->lo, ctx->hold,
                         xpost_stack_topdown_fetch(mem, stacadr, j));
    }
    for (j = n; j--;)
    {  /* pop */
        (void)xpost_stack_pop(mem, stacadr);
    }
}

/* execute an operator function by opcode
   the opcode is the payload of an operator object
*/
int xpost_operator_exec(Xpost_Context *ctx,
                        unsigned opcode)
{
    Xpost_Operator *optab;
    Xpost_Operator op;
    Xpost_Signature *sp;
    int i,j;
    int pass;
    int err = unregistered;
    Xpost_Stack *hold;
    int ct;
    unsigned int optadr;
    int ret;

    ret = xpost_memory_table_get_addr(ctx->gl,
                                      XPOST_MEMORY_TABLE_SPECIAL_OPERATOR_TABLE, &optadr);
    if (!ret)
    {
        XPOST_LOG_ERR("cannot load optab!");
        return VMerror;
    }
    optab = (void *)(ctx->gl->base + optadr);
    op = optab[opcode];
    sp = (void *)(ctx->gl->base + op.sigadr);

    ct = xpost_stack_count(ctx->lo, ctx->os);
    if (op.n == 0)
    {
        XPOST_LOG_ERR("operator has no signatures");
        return unregistered;
    }
    for (i =0 ; i < op.n; i++)
    { /* try each signature */
        byte *t;

        /* call signature's stack-checking proc, if available */
        if (sp[i].checkstack)
        {
            if ((ret = sp[i].checkstack(ctx)))
            {
                err = ret;
                continue;
            }
            goto call;
        }

        /* check stack size */
        if (ct < sp[i].in)
        {
            pass = 0;
            err = stackunderflow;
            continue;
        }

        /* check type-pattern against stack */
        pass = 1;
        t = (void *)(ctx->gl->base + sp[i].t);
        for (j=0; j < sp[i].in; j++)
        {
            Xpost_Object el = xpost_stack_topdown_fetch(ctx->lo, ctx->os, j);
            if (t[j] == anytype)
                continue;
            if (t[j] == xpost_object_get_type(el))
                continue;
            if ((t[j] == numbertype) &&
                (((xpost_object_get_type(el) == integertype) ||
                  (xpost_object_get_type(el) == realtype))))
                continue;
            if (t[j] == floattype)
            {
                if (xpost_object_get_type(el) == integertype)
                {
                    if (!xpost_stack_topdown_replace(ctx->lo, ctx->os, j, el = _promote_integer_to_real(el)))
                        return unregistered;
                    continue;
                }
                if (xpost_object_get_type(el) == realtype)
                    continue;
            }
            if ((t[j] == proctype) &&
                (xpost_object_get_type(el) == arraytype) &&
                xpost_object_is_exe(el))
                continue;
            pass = 0;
            err = typecheck;
            break;
        }

        if (pass) goto call;
    }
    return err;

  call:
    /* If we're executing the context's "currentobject",
       set the number of arguments consumed in the pad0 of currentobject,
       and set a flag declaring that this has been done.
       This is so onerror() can reset the stack
       (if hold has not been clobbered by another call to xpost_operator_exec).
    */
    if ((ctx->currentobject.tag == operatortype) &&
        (ctx->currentobject.mark_.padw == opcode))
    {
        ctx->currentobject.mark_.pad0 = sp[i].in;
        ctx->currentobject.tag |= XPOST_OBJECT_TAG_DATA_FLAG_OPARGSINHOLD;
    }
    else
    {
        /* Not executing current op.
           HOLD may *not* be assumed to contain currentobject's arguments.
           clear the flag.
        */
        ctx->currentobject.tag &= ~XPOST_OBJECT_TAG_DATA_FLAG_OPARGSINHOLD;
    }

    _xpost_operator_push_args_to_hold(ctx, ctx->lo, ctx->os, sp[i].in);
    hold = (void *)(ctx->lo->base + ctx->hold);

    switch(sp[i].in)
    {
        case 0:
            ret = sp[i].fp(ctx); break;
        case 1:
            ret = ((int(*)(Xpost_Context*,Xpost_Object))sp[i].fp)
                (ctx, hold->data[0]); break;
        case 2:
            ret = ((int(*)(Xpost_Context*,Xpost_Object,Xpost_Object))sp[i].fp)
                (ctx, hold->data[0], hold->data[1]); break;
        case 3:
            ret = ((int(*)(Xpost_Context*,Xpost_Object,Xpost_Object,Xpost_Object))sp[i].fp)
                (ctx, hold->data[0], hold->data[1], hold->data[2]); break;
        case 4:
            ret = ((int(*)(Xpost_Context*,Xpost_Object,Xpost_Object,Xpost_Object,Xpost_Object))sp[i].fp)
                (ctx, hold->data[0], hold->data[1], hold->data[2], hold->data[3]); break;
        case 5:
            ret = ((int(*)(Xpost_Context*,Xpost_Object,Xpost_Object,Xpost_Object,Xpost_Object,Xpost_Object))sp[i].fp)
                (ctx, hold->data[0], hold->data[1], hold->data[2], hold->data[3], hold->data[4]); break;
        case 6:
            ret = ((int(*)(Xpost_Context*,Xpost_Object,Xpost_Object,Xpost_Object,Xpost_Object,Xpost_Object,Xpost_Object))sp[i].fp)
                (ctx, hold->data[0], hold->data[1], hold->data[2], hold->data[3], hold->data[4], hold->data[5]); break;
        case 7:
            ret = ((int(*)(Xpost_Context*,Xpost_Object,Xpost_Object,Xpost_Object,Xpost_Object,Xpost_Object,Xpost_Object,Xpost_Object))sp[i].fp)
                (ctx, hold->data[0], hold->data[1], hold->data[2], hold->data[3], hold->data[4], hold->data[5], hold->data[6]); break;
        case 8:
            ret =
                ((int(*)(Xpost_Context*,Xpost_Object,Xpost_Object,Xpost_Object,Xpost_Object,Xpost_Object,Xpost_Object,Xpost_Object,Xpost_Object))sp[i].fp)
                (ctx, hold->data[0], hold->data[1], hold->data[2], hold->data[3], hold->data[4], hold->data[5], hold->data[6], hold->data[7]); break;
        default:
            ret = unregistered;
    }
    if (ret)
        return ret;
    return 0;
}
