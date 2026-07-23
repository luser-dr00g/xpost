/*
 * Xpost - a Level-2 Postscript interpreter
 * Copyright (C) 2013-2016, Michael Joshua Ryan
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

/* control operators */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <stdio.h> /* printf */
#include <stdlib.h> /* NULL */

#include "xpost.h"
#include "xpost_log.h"
#include "xpost_memory.h"
#include "xpost_object.h"
#include "xpost_stack.h"
#include "xpost_context.h"
#include "xpost_error.h"
#include <string.h>

#include "xpost_name.h"
#include "xpost_string.h"
#include "xpost_array.h"
#include "xpost_dict.h"

//#include "xpost_interpreter.h"
#include "xpost_operator.h"
#include "xpost_op_control.h"

/* any  exec  -
   execute arbitrary object */
static
int xpost_op_any_exec (Xpost_Context *ctx,
                       Xpost_Object O)
{
    if (!xpost_stack_push(ctx->lo, ctx->es, O))
        return execstackoverflow;
    return 0;
}

/* bool proc  if  -
   execute proc if bool is true */
static
int xpost_op_bool_proc_if (Xpost_Context *ctx,
                           Xpost_Object B,
                           Xpost_Object P)
{
    if (B.int_.val)
        if (!xpost_stack_push(ctx->lo, ctx->es, P))
            return execstackoverflow;
    return 0;
}

/* bool proc1 proc2  ifelse  -
   execute proc1 if bool is true,
   proc2 if bool is false */
static
int xpost_op_bool_proc_proc_ifelse (Xpost_Context *ctx,
                                    Xpost_Object B,
                                    Xpost_Object Then,
                                    Xpost_Object Else)
{
    if (B.int_.val)
    {
        if (!xpost_stack_push(ctx->lo, ctx->es, Then))
        {
            return execstackoverflow;
        }
    }
    else
    {
        if (!xpost_stack_push(ctx->lo, ctx->es, Else))
        {
            return execstackoverflow;
        }
    }
    return 0;
}

/* initial increment limit proc  for  -
   execute proc with values from initial by steps
   of increment to limit */
static
int xpost_op_int_int_int_proc_for (Xpost_Context *ctx,
                                   Xpost_Object init,
                                   Xpost_Object incr,
                                   Xpost_Object lim,
              Xpost_Object P)
{
    integer i = init.int_.val;
    integer j = incr.int_.val;
    integer n = lim.int_.val;
    int up = j > 0;
    if (up? i > n : i < n) return 0;
    assert(ctx->gl->base);

    /* loop frame: the sentinel loop operator (which exit searches for)
       under literal state that the iterate operator updates in place */
    if (!xpost_stack_push(ctx->lo, ctx->es,
                          xpost_operator_cons_opcode(ctx->opcode_shortcuts.opfor)))
            return execstackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->es, xpost_object_cvlit(P)))
        return execstackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->es, incr))
        return execstackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->es, lim))
        return execstackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->es, xpost_int_cons(i + j)))
        return execstackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->es,
                          xpost_operator_cons_opcode(ctx->opcode_shortcuts.forcont)))
        return execstackoverflow;

    if (!xpost_stack_push(ctx->lo, ctx->es, P))
        return execstackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->os, init))
        return stackoverflow;

    return 0;
}

/* continue a for loop: es holds (from the top) the next value, the
   limit, the increment, the literal proc, and the sentinel */
static
int xpost_op_for_iterate (Xpost_Context *ctx)
{
    Xpost_Stack *root = (Xpost_Stack *)(ctx->lo->base + ctx->es);
    Xpost_Stack *top = (Xpost_Stack *)(ctx->lo->base + root->prevseg);
    Xpost_Object i, lim, incr, P;

    if (top->top >= 5 && top->top < XPOST_STACK_SEGMENT_SIZE - 2)
    {
        i    = top->data[top->top - 1];
        lim  = top->data[top->top - 2];
        incr = top->data[top->top - 3];
        P    = top->data[top->top - 4];
        if (incr.int_.val > 0 ? i.int_.val > lim.int_.val
                              : i.int_.val < lim.int_.val)
        {
            top->top -= 5; /* drop the frame */
            return 0;
        }
        if (!xpost_stack_push(ctx->lo, ctx->os, i))
            return stackoverflow;
        /* the push may grow the memory file and move its base:
           re-derive the frame pointers before writing through them */
        root = (Xpost_Stack *)(ctx->lo->base + ctx->es);
        top = (Xpost_Stack *)(ctx->lo->base + root->prevseg);
        top->data[top->top - 1] = xpost_int_cons(i.int_.val + incr.int_.val);
        top->data[top->top]     = xpost_operator_cons_opcode(ctx->opcode_shortcuts.forcont);
        top->data[top->top + 1] = xpost_object_cvx(P);
        top->top += 2;
        return 0;
    }

    i    = xpost_stack_topdown_fetch(ctx->lo, ctx->es, 0);
    lim  = xpost_stack_topdown_fetch(ctx->lo, ctx->es, 1);
    incr = xpost_stack_topdown_fetch(ctx->lo, ctx->es, 2);
    P    = xpost_stack_topdown_fetch(ctx->lo, ctx->es, 3);
    if (xpost_object_get_type(i) == invalidtype)
        return execstackunderflow;
    if (incr.int_.val > 0 ? i.int_.val > lim.int_.val
                          : i.int_.val < lim.int_.val)
    {
        int k;
        for (k = 0; k < 5; k++)
            (void)xpost_stack_pop(ctx->lo, ctx->es);
        return 0;
    }
    if (!xpost_stack_push(ctx->lo, ctx->os, i))
        return stackoverflow;
    if (!xpost_stack_topdown_replace(ctx->lo, ctx->es, 0,
                                     xpost_int_cons(i.int_.val + incr.int_.val)))
        return execstackunderflow;
    if (!xpost_stack_push(ctx->lo, ctx->es,
                          xpost_operator_cons_opcode(ctx->opcode_shortcuts.forcont)))
        return execstackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->es, xpost_object_cvx(P)))
        return execstackoverflow;
    return 0;
}

/* same as IIIPfor but for reals */
static
int xpost_op_real_real_real_proc_for (Xpost_Context *ctx,
                                      Xpost_Object init,
                                      Xpost_Object incr,
                                      Xpost_Object lim,
                                      Xpost_Object P)
{
    real i = init.real_.val;
    real j = incr.real_.val;
    real n = lim.real_.val;
    int up = j > 0;
    if (up? i > n : i < n) return 0;
    //xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons(ctx, "for", NULL,0,0));
    if (!xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(ctx->opcode_shortcuts.opfor)))
        return execstackoverflow;
    //xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons(ctx, "cvx", NULL,0,0));
    if (!xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(ctx->opcode_shortcuts.cvx)))
        return execstackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->es, xpost_object_cvlit(P)))
        return execstackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->es, lim))
        return execstackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->es, incr))
        return execstackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->es, xpost_real_cons(i + j)))
        return execstackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->es, P))
        return execstackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->os, init))
        return stackoverflow;
    return 0;
}

/* int proc  repeat  -
   execute proc int times */
static
int xpost_op_int_proc_repeat (Xpost_Context *ctx,
                              Xpost_Object n,
                              Xpost_Object P)
{
    if (n.int_.val <= 0) return 0;

    /* loop frame, as for the for operator */
    if (!xpost_stack_push(ctx->lo, ctx->es,
                          xpost_operator_cons_opcode(ctx->opcode_shortcuts.repeat)))
        return execstackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->es, xpost_object_cvlit(P)))
        return execstackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->es, xpost_int_cons(n.int_.val - 1)))
        return execstackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->es,
                          xpost_operator_cons_opcode(ctx->opcode_shortcuts.repeatcont)))
        return execstackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->es, P))
        return execstackoverflow;

    return 0;
}

/* continue a repeat loop: es holds (from the top) the remaining
   count, the literal proc, and the sentinel */
static
int xpost_op_repeat_iterate (Xpost_Context *ctx)
{
    Xpost_Stack *root = (Xpost_Stack *)(ctx->lo->base + ctx->es);
    Xpost_Stack *top = (Xpost_Stack *)(ctx->lo->base + root->prevseg);
    Xpost_Object n, P;

    if (top->top >= 3 && top->top < XPOST_STACK_SEGMENT_SIZE - 2)
    {
        n = top->data[top->top - 1];
        P = top->data[top->top - 2];
        if (n.int_.val <= 0)
        {
            top->top -= 3; /* drop the frame */
            return 0;
        }
        top->data[top->top - 1] = xpost_int_cons(n.int_.val - 1);
        top->data[top->top]     = xpost_operator_cons_opcode(ctx->opcode_shortcuts.repeatcont);
        top->data[top->top + 1] = xpost_object_cvx(P);
        top->top += 2;
        return 0;
    }

    n = xpost_stack_topdown_fetch(ctx->lo, ctx->es, 0);
    P = xpost_stack_topdown_fetch(ctx->lo, ctx->es, 1);
    if (xpost_object_get_type(n) == invalidtype)
        return execstackunderflow;
    if (n.int_.val <= 0)
    {
        int k;
        for (k = 0; k < 3; k++)
            (void)xpost_stack_pop(ctx->lo, ctx->es);
        return 0;
    }
    if (!xpost_stack_topdown_replace(ctx->lo, ctx->es, 0,
                                     xpost_int_cons(n.int_.val - 1)))
        return execstackunderflow;
    if (!xpost_stack_push(ctx->lo, ctx->es,
                          xpost_operator_cons_opcode(ctx->opcode_shortcuts.repeatcont)))
        return execstackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->es, xpost_object_cvx(P)))
        return execstackoverflow;
    return 0;
}

/* proc  loop  -
   execute proc an indefinite number of times */
static
int xpost_op_proc_loop (Xpost_Context *ctx,
                        Xpost_Object P)
{
    /* loop frame, as for the for operator */
    if (!xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(ctx->opcode_shortcuts.loop)))
        return execstackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->es, xpost_object_cvlit(P)))
        return execstackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(ctx->opcode_shortcuts.loopcont)))
        return execstackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->es, P))
        return execstackoverflow;
    return 0;
}

/* continue a loop: es holds (from the top) the literal proc and the
   sentinel; only exit or stop ends the loop */
static
int xpost_op_loop_iterate (Xpost_Context *ctx)
{
    Xpost_Stack *root = (Xpost_Stack *)(ctx->lo->base + ctx->es);
    Xpost_Stack *top = (Xpost_Stack *)(ctx->lo->base + root->prevseg);
    Xpost_Object P;

    if (top->top >= 2 && top->top < XPOST_STACK_SEGMENT_SIZE - 2)
    {
        P = top->data[top->top - 1];
        top->data[top->top]     = xpost_operator_cons_opcode(ctx->opcode_shortcuts.loopcont);
        top->data[top->top + 1] = xpost_object_cvx(P);
        top->top += 2;
        return 0;
    }

    P = xpost_stack_topdown_fetch(ctx->lo, ctx->es, 0);
    if (xpost_object_get_type(P) == invalidtype)
        return execstackunderflow;
    if (!xpost_stack_push(ctx->lo, ctx->es,
                          xpost_operator_cons_opcode(ctx->opcode_shortcuts.loopcont)))
        return execstackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->es, xpost_object_cvx(P)))
        return execstackoverflow;
    return 0;
}

/* -  exit  -
   exit innermost active loop */
static
int xpost_op_exit (Xpost_Context *ctx)
{
    //Xpost_Object opfor = xpost_operator_cons(ctx, "for", NULL,0,0);
    Xpost_Object opfor = xpost_operator_cons_opcode(ctx->opcode_shortcuts.opfor);
    //Xpost_Object oprepeat = xpost_operator_cons(ctx, "repeat", NULL,0,0);
    Xpost_Object oprepeat = xpost_operator_cons_opcode(ctx->opcode_shortcuts.repeat);
    //Xpost_Object oploop = xpost_operator_cons(ctx, "loop", NULL,0,0);
    Xpost_Object oploop = xpost_operator_cons_opcode(ctx->opcode_shortcuts.loop);
    //Xpost_Object opforall = xpost_operator_cons(ctx, "forall", NULL,0,0);
    Xpost_Object opforall = xpost_operator_cons_opcode(ctx->opcode_shortcuts.forall);
    Xpost_Object x;

#if 0
    printf("\nexit\n");
    xpost_object_dump(opfor);
    xpost_object_dump(oprepeat);
    xpost_object_dump(oploop);
    xpost_object_dump(opforall);

    xpost_stack_dump(ctx->lo, ctx->os);
    xpost_stack_dump(ctx->lo, ctx->es);
    printf("\n");
#endif

    while (1) {
        x = xpost_stack_pop(ctx->lo, ctx->es);
        if (xpost_object_get_type(x) == invalidtype)
            return execstackunderflow;
        //xpost_object_dump(x);
        if ((xpost_dict_compare_objects(ctx, x, opfor)    == 0) ||
            (xpost_dict_compare_objects(ctx, x, oprepeat) == 0) ||
            (xpost_dict_compare_objects(ctx, x, oploop)   == 0) ||
            (xpost_dict_compare_objects(ctx, x, opforall) == 0))
        {
            break;
        }
    }

#if 0
    printf("result:");
    xpost_stack_dump(ctx->lo, ctx->es);
#endif
    return 0;
}

/* record what ended the run for the embedding caller. $error is the
   authority: a program may raise through the error machinery or set
   $error and stop directly, and either way its errorname and errorinfo
   describe the failure. */
static
void _record_run_error(Xpost_Context *ctx)
{
    {
        Xpost_Object userdict = xpost_stack_bottomup_fetch(ctx->lo, ctx->ds, 2);
        Xpost_Object derr = xpost_dict_get(ctx, userdict,
                xpost_name_cons(ctx, "$error"));
        if (xpost_object_get_type(derr) == dicttype)
        {
            Xpost_Object nm = xpost_dict_get(ctx, derr,
                    xpost_name_cons(ctx, "errorname"));
            Xpost_Object info = xpost_dict_get(ctx, derr,
                    xpost_name_cons(ctx, "errorinfo"));
            if (xpost_object_get_type(nm) == nametype)
                nm = xpost_name_get_string(ctx, nm);
            if (xpost_object_get_type(nm) == stringtype)
            {
                unsigned int n = nm.comp_.sz;
                if (n > sizeof ctx->run_error_name - 1)
                    n = sizeof ctx->run_error_name - 1;
                memcpy(ctx->run_error_name,
                       xpost_string_get_pointer(ctx, nm), n);
                ctx->run_error_name[n] = '\0';
            }
            if (xpost_object_get_type(info) == stringtype)
            {
                unsigned int n = info.comp_.sz;
                if (n > sizeof ctx->run_error_info - 1)
                    n = sizeof ctx->run_error_info - 1;
                memcpy(ctx->run_error_info,
                       xpost_string_get_pointer(ctx, info), n);
                ctx->run_error_info[n] = '\0';
            }
        }
    }
    ctx->run_uncaught = 1;
}

/* The stopped context is a boolean 'false' on the exec stack,
   so normal execution simply falls through and pushes the
   false onto the operand stack. 'stop' then merely has to
   search for 'false' and push a 'true', popping as it goes.  */

/* -  stop  -
   terminate stopped context */
static
int xpost_op_stop(Xpost_Context *ctx)
{
    Xpost_Object f = xpost_bool_cons(0);
    Xpost_Object x;
    /* Unwind the exec stack to the nearest enclosing stopped context --
       the false that `stopped` pushed. Pop straight to it: counting the
       whole stack first to bound the loop is O(depth) yet the marker is
       usually a few frames down, and an emptied stack (the pop yields
       invalidtype) is itself the no-context case handled below. */
    for (;;)
    {
        x = xpost_stack_pop(ctx->lo, ctx->es);
        if (xpost_object_get_type(x) == invalidtype)
            break;
        if(xpost_dict_compare_objects(ctx, f, x) == 0) {
            if (!xpost_stack_push(ctx->lo, ctx->os, xpost_bool_cons(1)))
                return stackoverflow;
            return 0;
        }
    }
    /* PLRM: stop with no enclosing stopped context prints a message
       and executes quit.  Returning an error here would re-enter
       errordict, whose handlers themselves finish with `stop`,
       recursing without bound. */
    XPOST_LOG_ERR("no stopped context in 'stop'");
    if (getenv("XPOST_TRAP_NOSTOP")) abort();
    _record_run_error(ctx);
    ctx->quit = 1;
    return 0;
}

/* -  .rundied  -
   the run's scheduling guard caught an error that the program did not:
   record it so the embedding caller sees the run as errored */
static
int xpost_op_rundied(Xpost_Context *ctx)
{
    _record_run_error(ctx);
    return 0;
}

/* name proc  .wrapop  operator
   install an operator that runs the procedure. A procedure that
   implements a standard operator becomes indistinguishable from a
   C-coded one: load answers operatortype and bind substitutes it.
   The procedure must stay reachable elsewhere; the operator table
   is outside the collector's view. */
static
int xpost_op_wrapop(Xpost_Context *ctx,
                    Xpost_Object name,
                    Xpost_Object proc)
{
    Xpost_Object o;

    o = xpost_operator_cons_wrapped(ctx, name, proc);
    if (xpost_object_get_type(o) != operatortype)
        return unregistered;
    if (!xpost_stack_push(ctx->lo, ctx->os, o))
        return stackoverflow;
    return 0;
}

/* any  stopped  bool
   establish context for catching stop */
static
int xpost_op_any_stopped(Xpost_Context *ctx,
                         Xpost_Object o)
{
    if (!xpost_stack_push(ctx->lo, ctx->es, xpost_bool_cons(0)))
        return execstackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->es, o))
        return execstackoverflow;
    return 0;
}

/* -  countexecstack  int
   count elements on execution stack */
static
int xpost_op_countexecstack(Xpost_Context *ctx)
{
    if (!xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons(xpost_stack_count(ctx->lo, ctx->es))))
        return stackoverflow;
    return 0;
}

/* array  execstack  subarray
   copy execution stack into array */
static
int xpost_op_array_execstack(Xpost_Context *ctx,
                             Xpost_Object A)
{
    Xpost_Object subarr;
    int z = xpost_stack_count(ctx->lo, ctx->es);
    int i;
    for (i = 0; i < z; i++)
    {
        int ret;
        ret = xpost_array_put(ctx, A, i, xpost_stack_bottomup_fetch(ctx->lo, ctx->es, i));
        if (ret)
            return ret;
    }
    subarr = xpost_object_get_interval(A, 0, z);
    if (xpost_object_get_type(subarr) == invalidtype)
        return rangecheck;
    if (!xpost_stack_push(ctx->lo, ctx->os, subarr))
        return stackoverflow;
    return 0;
}

/* -  quit  -
   terminate interpreter */
static
int xpost_op_quit(Xpost_Context *ctx)
{
    ctx->quit = 1;
    return 0;
}

/* - start -
   executed at interpreter startup */
/* implemented in data/init.ps */

int xpost_oper_init_control_ops (Xpost_Context *ctx,
                                 Xpost_Object sd)
{
    Xpost_Operator *optab;
    Xpost_Object n,op;
    unsigned int optadr;

    assert(ctx->gl->base);
    //xpost_memory_table_get_addr(ctx->gl, XPOST_MEMORY_TABLE_SPECIAL_OPERATOR_TABLE, &optadr);
    //optab = (void *)(ctx->gl->base + optadr);

    op = xpost_operator_cons(ctx, "exec", (Xpost_Op_Func)xpost_op_any_exec, 0, 1, anytype);
    INSTALL;
    op = xpost_operator_cons(ctx, "if", (Xpost_Op_Func)xpost_op_bool_proc_if, 0, 2, booleantype, proctype);
    ctx->opcode_shortcuts.opif = op.mark_.padw;
    INSTALL;
    op = xpost_operator_cons(ctx, "ifelse", (Xpost_Op_Func)xpost_op_bool_proc_proc_ifelse, 0, 3, booleantype, proctype, proctype);
    ctx->opcode_shortcuts.opifelse = op.mark_.padw;
    INSTALL;
    op = xpost_operator_cons(ctx, "for", (Xpost_Op_Func)xpost_op_int_int_int_proc_for, 0, 4, \
                             integertype, integertype, integertype, proctype);
    INSTALL;
    op = xpost_operator_cons(ctx, "for", (Xpost_Op_Func)xpost_op_real_real_real_proc_for, 0, 4, \
                             floattype, floattype, floattype, proctype);
    INSTALL;
    ctx->opcode_shortcuts.opfor = op.mark_.padw;
    op = xpost_operator_cons(ctx, "repeat", (Xpost_Op_Func)xpost_op_int_proc_repeat, 0, 2, integertype, proctype);
    INSTALL;
    ctx->opcode_shortcuts.repeat = op.mark_.padw;
    op = xpost_operator_cons(ctx, "loop", (Xpost_Op_Func)xpost_op_proc_loop, 0, 1, proctype);
    INSTALL;
    ctx->opcode_shortcuts.loop = op.mark_.padw;
    /* internal loop-continuation operators, referenced by opcode only */
    op = xpost_operator_cons(ctx, "for.iterate", (Xpost_Op_Func)xpost_op_for_iterate, 0, 0);
    ctx->opcode_shortcuts.forcont = op.mark_.padw;
    op = xpost_operator_cons(ctx, "repeat.iterate", (Xpost_Op_Func)xpost_op_repeat_iterate, 0, 0);
    ctx->opcode_shortcuts.repeatcont = op.mark_.padw;
    op = xpost_operator_cons(ctx, "loop.iterate", (Xpost_Op_Func)xpost_op_loop_iterate, 0, 0);
    ctx->opcode_shortcuts.loopcont = op.mark_.padw;
    op = xpost_operator_cons(ctx, "exit", (Xpost_Op_Func)xpost_op_exit, 0, 0);
    INSTALL;
    op = xpost_operator_cons(ctx, "stop", (Xpost_Op_Func)xpost_op_stop, 0, 0);
    if (xpost_object_get_type(op) == invalidtype)
        return VMerror;
    xpost_dict_put(ctx, sd, xpost_name_cons(ctx, ".rundied"),
        xpost_operator_cons(ctx, ".rundied", (Xpost_Op_Func)xpost_op_rundied, 0, 0));
    INSTALL;
    op = xpost_operator_cons(ctx, ".wrapop", (Xpost_Op_Func)xpost_op_wrapop, 1, 2, nametype, proctype);
    INSTALL;
    op = xpost_operator_cons(ctx, "stopped", (Xpost_Op_Func)xpost_op_any_stopped, 0, 1, anytype);
    INSTALL;
    op = xpost_operator_cons(ctx, "countexecstack", (Xpost_Op_Func)xpost_op_countexecstack, 1, 0);
    INSTALL;
    op = xpost_operator_cons(ctx, "execstack", (Xpost_Op_Func)xpost_op_array_execstack, 1, 1, arraytype);
    INSTALL;
    op = xpost_operator_cons(ctx, "quit", (Xpost_Op_Func)xpost_op_quit, 0, 0);
    INSTALL;
    /*
    op = xpost_operator_cons(ctx, "eq", (Xpost_Op_Func)Aeq, 1, 2, anytype, anytype);
    INSTALL;
    //xpost_dict_dump_memory (ctx->gl, sd); fflush(NULL);
    */

    return 0;
}
