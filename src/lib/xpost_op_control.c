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

/* control operators */

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
#include <stdio.h> /* printf */
#include <stdlib.h> /* NULL */

#include "xpost_log.h"
#include "xpost_memory.h"
#include "xpost_object.h"
#include "xpost_stack.h"
#include "xpost_context.h"
#include "xpost_error.h"
#include "xpost_name.h"
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

    if (!xpost_stack_push(ctx->lo, ctx->es,
                xpost_operator_cons_opcode(ctx->opcode_shortcuts.opfor)))
            return execstackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->es,
                xpost_operator_cons_opcode(ctx->opcode_shortcuts.cvx)))
        return execstackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->es, xpost_object_cvlit(P)))
        return execstackoverflow;

    if (!xpost_stack_push(ctx->lo, ctx->es, lim))
        return execstackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->es, incr))
        return execstackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->es, xpost_int_cons(i + j)))
        return execstackoverflow;

    if (!xpost_stack_push(ctx->lo, ctx->es, P))
        return execstackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->os, init))
        return stackoverflow;

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

    if (!xpost_stack_push(ctx->lo, ctx->es,
                xpost_operator_cons_opcode(ctx->opcode_shortcuts.repeat)))
        return execstackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->es,
                xpost_operator_cons_opcode(ctx->opcode_shortcuts.cvx)))
        return execstackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->es, xpost_object_cvlit(P)))
        return execstackoverflow;

    if (!xpost_stack_push(ctx->lo, ctx->es, xpost_int_cons(n.int_.val - 1)))
        return execstackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->es, P))
        return execstackoverflow;

    return 0;
}

/* proc  loop  -
   execute proc an indefinite number of times */
static
int xpost_op_proc_loop (Xpost_Context *ctx,
            Xpost_Object P)
{
    //xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons(ctx, "loop", NULL,0,0));
    if (!xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(ctx->opcode_shortcuts.loop)))
        return execstackoverflow;
    //xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons(ctx, "cvx", NULL,0,0));
    if (!xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(ctx->opcode_shortcuts.cvx)))
        return execstackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->es, xpost_object_cvlit(P)))
        return execstackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->es, P))
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
        if ( (objcmp(ctx, x, opfor)    == 0)
          || (objcmp(ctx, x, oprepeat) == 0)
          || (objcmp(ctx, x, oploop)   == 0)
          || (objcmp(ctx, x, opforall) == 0)
           ) {
            break;
        }
    }

#if 0
    printf("result:");
    xpost_stack_dump(ctx->lo, ctx->es);
#endif
    return 0;
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
    int c = xpost_stack_count(ctx->lo, ctx->es);
    Xpost_Object x;
    while (c--) {
        x = xpost_stack_pop(ctx->lo, ctx->es);
        if (xpost_object_get_type(x) == invalidtype)
            return execstackunderflow;
        if(objcmp(ctx, f, x) == 0) {
            if (!xpost_stack_push(ctx->lo, ctx->os, xpost_bool_cons(1)))
                return stackoverflow;
            return 0;
        }
    }
    XPOST_LOG_ERR("no stopped context in 'stop'");
    return unregistered;
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
    for (i=0; i < z; i++)
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
    xpost_memory_table_get_addr(ctx->gl,
            XPOST_MEMORY_TABLE_SPECIAL_OPERATOR_TABLE, &optadr);
    optab = (void *)(ctx->gl->base + optadr);

    op = xpost_operator_cons(ctx, "exec", (Xpost_Op_Func)xpost_op_any_exec, 0, 1, anytype);
    INSTALL;
    op = xpost_operator_cons(ctx, "if", (Xpost_Op_Func)xpost_op_bool_proc_if, 0, 2, booleantype, proctype);
    INSTALL;
    op = xpost_operator_cons(ctx, "ifelse", (Xpost_Op_Func)xpost_op_bool_proc_proc_ifelse, 0, 3, booleantype, proctype, proctype);
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
    op = xpost_operator_cons(ctx, "exit", (Xpost_Op_Func)xpost_op_exit, 0, 0);
    INSTALL;
    op = xpost_operator_cons(ctx, "stop", (Xpost_Op_Func)xpost_op_stop, 0, 0);
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


