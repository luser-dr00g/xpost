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
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "xpost.h"
#include "xpost_log.h"
#include "xpost_compat.h" /* mkstemp, xpost_isatty */
#include "xpost_memory.h"  /* itp contexts contain mfiles and mtabs */
#include "xpost_object.h"  /* eval functions examine objects */
#include "xpost_stack.h"  /* eval functions manipulate stacks */
#include "xpost_error.h"
#include "xpost_context.h"
#include "xpost_save.h"  /* save/restore vm */
#include "xpost_string.h"  /* eval functions examine strings */
#include "xpost_array.h"  /* eval functions examine arrays */
#include "xpost_name.h"  /* eval functions examine names */
#include "xpost_dict.h"  /* eval functions examine dicts */
#include "xpost_file.h"  /* eval functions examine files */

#include "xpost_interpreter.h" /* uses: context itp MAXCONTEXT MAXMFILE */
#include "xpost_garbage.h"  /*  test gc, install collect() in context's memory files */
#include "xpost_operator.h"  /* eval functions call operators */
#include "xpost_oplib.h"

static
Xpost_Object namedollarerror; /* cached result of xpost_name_cons(ctx, "$error")
                                 to reduce time in error handler */
static Xpost_Object nameerrordict;

int _xpost_interpreter_is_tracing = 0;             /* output trace log */
Xpost_Interpreter *itpdata;  /* the global interpreter instance, containing all contexts and memory files */
static int _initializing = 1;  /* garbage collect does not run while _initializing is true.
                                  a getter function is exported in the memory file struct
                                  for the gc to access this global without #include'ing interpreter.h
                                  which would create a circular dependency. */

int eval(Xpost_Context *ctx);
int mainloop(Xpost_Context *ctx);
void init(void);
void xit(void);

/*
   global shortcut for a single-threaded interpreter
FIXME: "static context pointer". s.b. changed to a returned
   value from xpost_create()
   value now returned. this variable should be removed */
Xpost_Context *xpost_ctx;

/* getter function for _initializing, for export */
int xpost_interpreter_get_initializing(void)
{
    return _initializing;
}

/* setter function for _initializing, for consistency */
void xpost_interpreter_set_initializing(int i)
{
    _initializing = i;
}

/*  allocate a global memory file
    find the next unused mfile in the global memory table */
static Xpost_Memory_File *xpost_interpreter_alloc_global_memory(void)
{
    int i;

    for (i = 0; i < MAXMFILE; i++)
    {
        if (itpdata->gtab[i].base == NULL)
        {
            return &itpdata->gtab[i];
        }
    }
    XPOST_LOG_ERR("cannot allocate Xpost_Memory_File, gtab exhausted");
    return NULL;
}

/* allocate a local memory file
   find the next unused mfile in the local memory table */
static Xpost_Memory_File *xpost_interpreter_alloc_local_memory(void)
{
    int i;
    for (i = 0; i < MAXMFILE; i++)
    {
        if (itpdata->ltab[i].base == NULL)
        {
            return &itpdata->ltab[i];
        }
    }
    XPOST_LOG_ERR("cannot allocate Xpost_Memory_File, ltab exhausted");
    return NULL;
}


/* cursor to next cid number to try to allocate */
static
unsigned int nextid = 0;

/* allocate a context-id and associated context struct
   returns cid;
   a context in state zero is considered available for allocation,
   this corresponds to the C_FREE enumeration constant.
 */
static int xpost_interpreter_cid_init(unsigned int *cid)
{
    unsigned int startid = nextid;
    /*printf("cid_init\n"); */
    while ( xpost_interpreter_cid_get_context(++nextid)->state != 0 )
    {
        if (nextid == startid + MAXCONTEXT)
        {
            XPOST_LOG_ERR("ctab full. cannot create new process");
            return 0;
        }
    }
    *cid = nextid;
    return 1;
}

/* adapter:
           ctx <- cid
   yield pointer to context struct given cid
   this function is exported via function-pointer in the memory file struct
   so the garbage collector can discover relevant contexts given only a memory file.
 */
Xpost_Context *xpost_interpreter_cid_get_context(unsigned int cid)
{
    /*TODO reject cid 0 */
    return &itpdata->ctab[ (cid - 1) % MAXCONTEXT ];
}


/* initialize the name string stacks and name search trees (per memory file).
   seed the search trees.
   initialize and populate the optab and systemdict (global memory file).
   push systemdict on dict stack.
   allocate and push globaldict on dict stack.
   allocate and push userdict on dict stack.
   return 1 on success, 0 on failure
 */
static
int _xpost_interpreter_extra_context_init(Xpost_Context *ctx, const char *device)
{
    int ret;
    ret = xpost_name_init(ctx); /* NAMES NAMET */
    if (!ret)
    {
        xpost_memory_file_exit(ctx->lo);
        xpost_memory_file_exit(ctx->gl);
        return 0;
    }
    ctx->vmmode = GLOBAL;

    ret = xpost_operator_init_optab(ctx); /* allocate and zero the optab structure */
    if (!ret)
    {
        xpost_memory_file_exit(ctx->lo);
        xpost_memory_file_exit(ctx->gl);
        return 0;
    }

    /* seed the tree with a word from the middle of the alphabet */
    /* middle of the start */
    /* middle of the end */
    if (xpost_object_get_type(xpost_name_cons(ctx, "maxlength")) == invalidtype)
        return 0;
    if (xpost_object_get_type(xpost_name_cons(ctx, "getinterval")) == invalidtype)
        return 0;
    if (xpost_object_get_type(xpost_name_cons(ctx, "setmiterlimit")) == invalidtype)
        return 0;
    if (xpost_object_get_type((namedollarerror = xpost_name_cons(ctx, "$error"))) == invalidtype)
        return 0;
    if (xpost_object_get_type((nameerrordict = xpost_name_cons(ctx, "errordict"))) == invalidtype)
        return 0;

    xpost_oplib_init_ops(ctx); /* populate the optab (and systemdict) with operators */

    {
        Xpost_Object gd; /*globaldict */
        gd = xpost_dict_cons (ctx, 100);
        if (xpost_object_get_type(gd) == nulltype)
        {
            XPOST_LOG_ERR("cannot allocate globaldict");
            return 0;
        }
        ret = xpost_dict_put(ctx, xpost_stack_bottomup_fetch(ctx->lo, ctx->ds, 0), xpost_name_cons(ctx, "globaldict"), gd);
        if (ret)
            return 0;
        xpost_stack_push(ctx->lo, ctx->ds, gd);
    }

    ctx->vmmode = LOCAL;
    /* seed the tree with a word from the middle of the alphabet */
    /* middle of the start */
    /* middle of the end */
    if (xpost_object_get_type(xpost_name_cons(ctx, "minimal")) == invalidtype)
        return 0;
    if (xpost_object_get_type(xpost_name_cons(ctx, "interest")) == invalidtype)
        return 0;
    if (xpost_object_get_type(xpost_name_cons(ctx, "solitaire")) == invalidtype)
        return 0;
    {
        Xpost_Object ud; /*userdict */
        ud = xpost_dict_cons (ctx, 100);
        if (xpost_object_get_type(ud) == nulltype)
        {
            XPOST_LOG_ERR("cannot allocate userdict");
            return 0;
        }
        ret = xpost_dict_put(ctx, ud, xpost_name_cons(ctx, "userdict"), ud);
        if (ret)
            return 0;
        xpost_stack_push(ctx->lo, ctx->ds, ud);
    }

    ctx->device_str = device;

    return 1;
}


/* initialize itpdata.
   create and initialize a single context in ctab[0]
 */
int xpost_interpreter_init(Xpost_Interpreter *itpptr, const char *device)
{
    int ret;

    ret = xpost_context_init(&itpptr->ctab[0],
                             xpost_interpreter_cid_init,
                             xpost_interpreter_cid_get_context,
                             xpost_interpreter_get_initializing,
                             xpost_interpreter_set_initializing,
                             xpost_interpreter_alloc_local_memory,
                             xpost_interpreter_alloc_global_memory,
                             xpost_garbage_collect);
    if (!ret)
    {
        return 0;
    }
    ret = _xpost_interpreter_extra_context_init(&itpptr->ctab[0], device);
    if (!ret)
    {
        return 0;
    }

    itpptr->cid = itpptr->ctab[0].id;

    return 1;
}

/* destroy context in ctab[0] */
void xpost_interpreter_exit(Xpost_Interpreter *itpptr)
{
    xpost_context_exit(&itpptr->ctab[0]);
}


/*
 *  Interpreter eval##type() actions.
 *
 */

/* function type for interpreter action pointers */
typedef
int evalfunc(Xpost_Context *ctx);

/* quit the interpreter */
static
int evalquit(Xpost_Context *ctx)
{
    ++ctx->quit;
    return 0;
}

/* pop the execution stack */
static
int evalpop(Xpost_Context *ctx)
{
    if (!xpost_object_get_type(xpost_stack_pop(ctx->lo, ctx->es)) == invalidtype)
        return stackunderflow;
    return 0;
}

/* pop the execution stack onto the operand stack */
static
int evalpush(Xpost_Context *ctx)
{
    if (!xpost_stack_push(ctx->lo, ctx->os,
            xpost_stack_pop(ctx->lo, ctx->es)))
        return stackoverflow;
    return 0;
}

/* load executable name */
static
int evalload(Xpost_Context *ctx)
{
    int ret;
    if (_xpost_interpreter_is_tracing)
    {
        Xpost_Object s = xpost_name_get_string(ctx, xpost_stack_topdown_fetch(ctx->lo, ctx->es, 0));
        XPOST_LOG_DUMP("evalload <name \"%*s\">", s.comp_.sz, xpost_string_get_pointer(ctx, s));
    }

    if (!xpost_stack_push(ctx->lo, ctx->os,
            xpost_stack_pop(ctx->lo, ctx->es)))
        return stackoverflow;
    assert(ctx->gl->base);
    /*xpost_operator_exec(ctx, xpost_operator_cons(ctx, "load", NULL,0,0).mark_.padw); */
    ret = xpost_operator_exec(ctx, ctx->opcode_shortcuts.load);
    if (ret)
        return ret;
    if (xpost_object_is_exe(xpost_stack_topdown_fetch(ctx->lo, ctx->os, 0)))
    {
        Xpost_Object q;
        q = xpost_stack_pop(ctx->lo, ctx->os);
        if (xpost_object_get_type(q) == invalidtype)
            return undefined;
        if (!xpost_stack_push(ctx->lo, ctx->es, q))
            return ret;
    }
    return 0;
}

/* execute operator */
static
int evaloperator(Xpost_Context *ctx)
{
    int ret;
    Xpost_Object op = xpost_stack_pop(ctx->lo, ctx->es);
    if (xpost_object_get_type(op) == invalidtype)
        return stackunderflow;

    if (_xpost_interpreter_is_tracing)
        xpost_operator_dump(ctx, op.mark_.padw);
    ret = xpost_operator_exec(ctx, op.mark_.padw);
    if (ret)
        return ret;
    return 0;
}

/* extract head (&tail) of array */
static
int evalarray(Xpost_Context *ctx)
{
    Xpost_Object a = xpost_stack_pop(ctx->lo, ctx->es);
    Xpost_Object b;

    if (xpost_object_get_type(a) == invalidtype)
        return stackunderflow;

    switch (a.comp_.sz)
    {
        default /* > 1 */:
        {
            Xpost_Object interval;
            interval = xpost_object_get_interval(a, 1, a.comp_.sz - 1);
            if (xpost_object_get_type(interval) == invalidtype)
                return rangecheck;
            xpost_stack_push(ctx->lo, ctx->es, interval);
        }
        /*@fallthrough@*/
        case 1:
            b = xpost_array_get(ctx, a, 0);
            if (xpost_object_get_type(b) == arraytype)
            {
                if (!xpost_stack_push(ctx->lo, ctx->os, b))
                    return stackoverflow;
            }
            else
            {
                if (!xpost_stack_push(ctx->lo, ctx->es, b))
                    return execstackoverflow;
            }
            /*@fallthrough@*/
        case 0: /* drop */;
    }
    return 0;
}

/* extract token from string */
static
int evalstring(Xpost_Context *ctx)
{
    Xpost_Object b,t,s;
    int ret;

    s = xpost_stack_pop(ctx->lo, ctx->es);
    if (!xpost_stack_push(ctx->lo, ctx->os, s))
        return stackoverflow;
    assert(ctx->gl->base);
    /*xpost_operator_exec(ctx, xpost_operator_cons(ctx, "token",NULL,0,0).mark_.padw); */
    ret = xpost_operator_exec(ctx, ctx->opcode_shortcuts.token);
    if (ret)
        return ret;
    b = xpost_stack_pop(ctx->lo, ctx->os);
    if (xpost_object_get_type(b) == invalidtype)
        return stackunderflow;
    if (b.int_.val)
    {
        t = xpost_stack_pop(ctx->lo, ctx->os);
        if (xpost_object_get_type(t) == invalidtype)
            return stackunderflow;
        s = xpost_stack_pop(ctx->lo, ctx->os);
        if (xpost_object_get_type(s) == invalidtype)
            return stackunderflow;
        if (!xpost_stack_push(ctx->lo, ctx->es, s))
            return execstackoverflow;
        if (xpost_object_get_type(t)==arraytype)
        {
            if (!xpost_stack_push(ctx->lo, ctx->os , t))
                return stackoverflow;
        }
        else
        {
            if (!xpost_stack_push(ctx->lo, ctx->es , t))
                return execstackoverflow;
        }
    }
    return 0;
}

/* extract token from file */
static
int evalfile(Xpost_Context *ctx)
{
    Xpost_Object b,f,t;
    int ret;

    f = xpost_stack_pop(ctx->lo, ctx->es);
    if (!xpost_stack_push(ctx->lo, ctx->os, f))
        return stackoverflow;
    assert(ctx->gl->base);
    /*xpost_operator_exec(ctx, xpost_operator_cons(ctx, "token",NULL,0,0).mark_.padw); */
    ret = xpost_operator_exec(ctx, ctx->opcode_shortcuts.token);
    if (ret)
        return ret;
    b = xpost_stack_pop(ctx->lo, ctx->os);
    if (b.int_.val)
    {
        t = xpost_stack_pop(ctx->lo, ctx->os);
        if (!xpost_stack_push(ctx->lo, ctx->es, f))
            return execstackoverflow;
        if (xpost_object_get_type(t)==arraytype)
        {
            if (!xpost_stack_push(ctx->lo, ctx->os, t))
                return stackoverflow;
        }
        else
        {
            if (!xpost_stack_push(ctx->lo, ctx->es, t))
                return execstackoverflow;
        }
    }
    else
    {
        ret = xpost_file_object_close(ctx->lo, f);
        if (ret)
            XPOST_LOG_ERR("%s error closing file", errorname[ret]);
    }
    return 0;
}

/* interpreter actions for executable types */
evalfunc *evalinvalid = evalquit;
evalfunc *evalmark = evalpush;
evalfunc *evalnull = evalpop;
evalfunc *evalinteger = evalpush;
evalfunc *evalboolean = evalpush;
evalfunc *evalreal = evalpush;
evalfunc *evalsave = evalpush;
evalfunc *evaldict = evalpush;
evalfunc *evalextended = evalquit;
evalfunc *evalglob = evalpush;
evalfunc *evalmagic = evalquit;

evalfunc *evalcontext = evalpush;
evalfunc *evalname = evalload;

/* install the evaltype functions (possibly via pointers) in the jump table */
evalfunc *evaltype[XPOST_OBJECT_NTYPES + 1];
#define AS_EVALINIT(_) evaltype[ _ ## type ] = eval ## _ ;

/* use above macro to initialize function table
   keyed by enum types;
 */
static
void initevaltype(void)
{
    XPOST_OBJECT_TYPES(AS_EVALINIT)
}


/*
   call window device's event_handler function
   which should check for Events or Messages from the
   underlying Window System, process one or more of them,
   and then return 0.
   it should leave all stacks undisturbed.
 */
int idleproc (Xpost_Context *ctx)
{
    int ret;

    if ((xpost_object_get_type(ctx->event_handler) == operatortype) &&
        (xpost_object_get_type(ctx->window_device) == dicttype))
    {
        if (!xpost_stack_push(ctx->lo, ctx->os, ctx->window_device))
        {
            return stackoverflow;
        }
        ret = xpost_operator_exec(ctx, ctx->event_handler.mark_.padw);
        if (ret)
        {
            XPOST_LOG_ERR("event_handler returned %d (%s)",
                    ret, errorname[ret]);
            XPOST_LOG_ERR("disabling event_handler");
            ctx->event_handler = null;
            return ret;
        }
    }
    return 0;
}

/*
   check basic pointers and addresses for sanity
 */
static
int validate_context(Xpost_Context *ctx)
{
    /*assert(ctx); */
    /*assert(ctx->lo); */
    /*assert(ctx->lo->base); */
    /*assert(ctx->gl); */
    /*assert(ctx->gl->base); */
    if (!ctx)
    {
        XPOST_LOG_ERR("ctx invalid");
        return 0;
    }
    if (!ctx->lo)
    {
        XPOST_LOG_ERR("ctx->lo invalid");
        return 0;
    }
    if (!ctx->lo->base)
    {
        XPOST_LOG_ERR("ctx->lo->base invalid");
        return 0;
    }
    if (!ctx->gl)
    {
        XPOST_LOG_ERR("ctx->gl invalid");
        return 0;
    }
    if (!ctx->gl->base)
    {
        XPOST_LOG_ERR("ctx->gl->base invalid");
        return 0;
    }
    return 1;
}

/*
   one iteration of the central loop
   called repeatedly by mainloop()
 */
int eval(Xpost_Context *ctx)
{
    int ret;
    Xpost_Object t = xpost_stack_topdown_fetch(ctx->lo, ctx->es, 0);

    ctx->currentobject = t; /* for _onerror to determine if hold stack contents are restoreable.
                               if opexec(opcode) discovers opcode != ctx->currentobject.mark_.padw
                               it sets a flag indicating the hold stack does not contain
                               ctx->currentobject's arguments.
                               if an error is encountered, currentobject is reported as the
                               errant object since it is the "entry point" to the interpreter.
                             */

    if (!validate_context(ctx))
        return unregistered;

    if (_xpost_interpreter_is_tracing)
    {
        XPOST_LOG_DUMP("eval(): Executing: ");
        xpost_object_dump(t);
        XPOST_LOG_DUMP("Stack: ");
        xpost_stack_dump(ctx->lo, ctx->os);
        XPOST_LOG_DUMP("Dict Stack: ");
        xpost_stack_dump(ctx->lo, ctx->ds);
        XPOST_LOG_DUMP("Exec Stack: ");
        xpost_stack_dump(ctx->lo, ctx->es);
    }

    ret = idleproc(ctx); /* periodically process asynchronous events */
    if (ret)
        return ret;

    { /* check object for sanity before using jump table */
        Xpost_Object_Type type = xpost_object_get_type(t);
        if (type == invalidtype || type >= XPOST_OBJECT_NTYPES)
            return unregistered;
    }
    if ( xpost_object_is_exe(t) ) /* if executable */
        ret = evaltype[xpost_object_get_type(t)](ctx);
    else
        ret = evalpush(ctx);

    return ret;
}

/* called by mainloop() after propagated error codes.
   pushes postscript-level error procedures
   and resumes normal execution.
 */
static
void _onerror(Xpost_Context *ctx,
        unsigned int err)
{
    Xpost_Object sd;
    Xpost_Object ed;
    Xpost_Object dollarerror;

    if (err > unknownerror) err = unknownerror;

    if (!validate_context(ctx))
        XPOST_LOG_ERR("context not valid");

    if (itpdata->in_onerror > 5)
    {
        fprintf(stderr, "LOOP in error handler\nabort\n");
        ++ctx->quit;
        /*exit(undefinedresult); */
    }

    ++itpdata->in_onerror;

#ifdef EMITONERROR
    fprintf(stderr, "err: %s\n", errorname[err]);
#endif

    /* reset stack */
    if ((xpost_object_get_type(ctx->currentobject) == operatortype) &&
        (ctx->currentobject.tag & XPOST_OBJECT_TAG_DATA_FLAG_OPARGSINHOLD))
    {
        int n = ctx->currentobject.mark_.pad0;
        int i;
        for (i = 0; i < n; i++)
        {
            xpost_stack_push(ctx->lo, ctx->os,
                    xpost_stack_bottomup_fetch(ctx->lo, ctx->hold, i));
        }
    }

    /* printf("1\n"); */
    sd = xpost_stack_bottomup_fetch(ctx->lo, ctx->ds, 0);

    /* printf("2\n"); */
    dollarerror = xpost_dict_get(ctx, sd, namedollarerror);
    if (xpost_object_get_type(dollarerror) == invalidtype)
    {
        XPOST_LOG_ERR("cannot load $error dict for error: %s",
                errorname[err]);
        xpost_stack_push(ctx->lo, ctx->es,
                xpost_object_cvx(xpost_name_cons(ctx, "stop")));
        /*itpdata->in_onerror = 0; */
        return;
    }

    /* printf("3\n"); */
    /* printf("4\n"); */
    /* printf("5\n"); */
    xpost_stack_push(ctx->lo, ctx->os, ctx->currentobject);

#if 0
    /* printf("6\n"); */
    xpost_stack_push(ctx->lo, ctx->os, xpost_object_cvlit(xpost_name_cons(ctx, errorname[err])));
    /* printf("7\n"); */
    xpost_stack_push(ctx->lo, ctx->es, xpost_object_cvx(xpost_name_cons(ctx, "signalerror")));
#endif
    ed = xpost_dict_get(ctx, sd, nameerrordict);
    xpost_stack_push(ctx->lo, ctx->es,
            xpost_dict_get(ctx, ed,
                xpost_name_cons(ctx, errorname[err])));

    /* printf("8\n"); */
    itpdata->in_onerror = 0;
}


/*
   select a new context to execute and return it
   scan for the next context in the C_RUN state
   along the way, change C_WAIT contexts to C_RUN
   to retry wait conditions.
 */
static
Xpost_Context *_switch_context(Xpost_Context *ctx)
{
    int i;

    return ctx;

    /* return next context to execute */
    printf("--switching contexts--\n");
    /*putchar('.'); fflush(0); */
    for (i = (ctx - itpdata->ctab) + 1; i < MAXCONTEXT; i++)
    {
        /*printf("--%d-- %d\n", itpdata->ctab[i].id, itpdata->ctab[i].state); */
        if (itpdata->ctab[i].state == C_RUN)
        {
            return &itpdata->ctab[i];
        }
        if (itpdata->ctab[i].state == C_WAIT || itpdata->ctab[i].state == C_IOBLOCK)
        {
            itpdata->ctab[i].state = C_RUN;
        }
    }
    for (i = 0; i <= ctx-itpdata->ctab; i++)
    {
        /*printf("--%d-- %d\n", itpdata->ctab[i].id, itpdata->ctab[i].state); */
        if (itpdata->ctab[i].state == C_RUN)
        {
            return &itpdata->ctab[i];
        }
        if (itpdata->ctab[i].state == C_WAIT || itpdata->ctab[i].state == C_IOBLOCK)
        {
            itpdata->ctab[i].state = C_RUN;
        }
    }
    for (i = (ctx - itpdata->ctab) + 1; i < MAXCONTEXT; i++)
    {
        /*printf("--%d-- %d\n", itpdata->ctab[i].id, itpdata->ctab[i].state); */
        if (itpdata->ctab[i].state == C_RUN)
        {
            return &itpdata->ctab[i];
        }
    }
    for (i = 0; i <= ctx-itpdata->ctab; i++)
    {
        /*printf("--%d-- %d\n", itpdata->ctab[i].id, itpdata->ctab[i].state); */
        if (itpdata->ctab[i].state == C_RUN)
        {
            return &itpdata->ctab[i];
        }
    }

    return ctx;
}



/*
   the big main central interpreter loop.
   processes return codes from eval().
   0 indicate noerror
   yieldtocaller indicates `showpage` has been called using SHOWPAGE_RETURN semantics.
   ioblock indicates a blocked io operation.
   contextswitch indicates the `yield` operator has been called.
   all other values indicate an error condition to be returned to postscript.
 */
int mainloop(Xpost_Context *ctx)
{
    int ret;

ctxswitch:
    xpost_ctx = ctx = _switch_context(ctx);
    itpdata->cid = ctx->id;

    while(!ctx->quit)
    {
        ret = eval(ctx);
        if (ret)
            switch (ret)
            {
            case yieldtocaller:
                return 1;
            case ioblock:
                ctx->state = C_IOBLOCK; /* fallthrough */
            case contextswitch:
                goto ctxswitch;
            default:
                _onerror(ctx, ret);
            }
    }

    return 0;
}




/*
   string constructor helper for literals
   sizeof("") is 1, ie. it includes the terminating \0 byte.
   our ps strings are counted and do not need (and should not have)
   a nul byte, or this byte may produce garbage output when printed.
 */
#define CNT_STR(s) sizeof(s) - 1, s

/*
   set global pagesize,
   initialize eval's jump-tabl
   allocate global itpdata interpreter instance
   call xpost_interpreter_init
        which initializes the first context
 */
static
int initalldata(const char *device)
{
    int ret;

    initevaltype();
    xpost_object_install_dict_get_access(xpost_dict_get_access);
    xpost_object_install_dict_set_access(xpost_dict_set_access);

    /* allocate the top-level itpdata data structure. */
    null = xpost_object_cvlit(null);
    itpdata = malloc(sizeof*itpdata);
    if (!itpdata)
    {
        XPOST_LOG_ERR("itpdata=malloc failed");
        return 0;
    }
    memset(itpdata, 0, sizeof*itpdata);

    /* allocate and initialize the first context structure
       and associated memory structures.
       populate OPTAB and systemdict with operators.
       push systemdict, globaldict, and userdict on dict stack
     */
    ret = xpost_interpreter_init(itpdata, device);
    if (!ret)
    {
        return 0;
    }

    /* set global shortcut to context_0
       (the only context in a single-threaded interpreter)
       TODO remove this variable
     */
    xpost_ctx = &itpdata->ctab[0];

    return 1;
}

/* FIXME remove duplication of effort here and in bin/xpost_main.c
         (ie. there should be 1 table, not 2)

    Generates postscript code to initialize the selected device

    currentglobal false setglobal              % allocate in local memory
    device_requires_loading? { loadXXXdevice } if  % load if necessary
    userdict /DEVICE 612 792 newXXXdevice put  % instantiate the device
    setglobal                                  % reset previous allocation mode

    initialization of the device is deferred until the start procedure has
    initialized graphics (importantly, the ppmimage base class).
    the loadXXXdevice operators all specialize the ppmimage base class
    and so must wait until it is available.

    also creates the definitions PACKAGE_DATA_DIR PACKAGE_INSTALL_DIR and EXE_DIR
 */
static
void setlocalconfig(Xpost_Context *ctx,
                    Xpost_Object sd,
                    const char *device,
                    const char *outfile,
                    const char *bufferin,
                    char **bufferout,
                    Xpost_Showpage_Semantics semantics,
                    Xpost_Set_Size set_size,
                    int width,
                    int height)
{
    const char *device_strings[][3] =
    {
        { "pgm",    "",                 "newPGMIMAGEdevice" },
        { "ppm",    "",                 "newPPMIMAGEdevice" },
        { "null",   "",                 "newnulldevice"     },
        { "xcb",    "loadxcbdevice",    "newxcbdevice"      },
        { "gdi",    "loadwin32device",  "newwin32device"    },
        { "gl",     "loadwin32device",  "newwin32device"    },
        { "bgr",    "loadbgrdevice",    "newbgrdevice"      },
        { "raster", "loadrasterdevice", "newrasterdevice"   },
        { "png",    "loadpngdevice",    "newpngdevice"      },
        { "jpeg",   "loadjpegdevice",   "newjpegdevice"      },
        { NULL, NULL, NULL }
    };
    const char *strtemplate = "currentglobal false setglobal "
                        "%s userdict /DEVICE %s %s put "
                        "setglobal";
    Xpost_Object namenewdev;
    Xpost_Object newdevstr;
    int i;
    char *devstr;
    char *subdevice;
    char *dimensions;

    ctx->vmmode = GLOBAL;

#ifdef _WIN32
    xpost_dict_put(ctx, sd, xpost_name_cons(ctx, "WIN32"), xpost_bool_cons(1));
#endif

    devstr = strdup(device); /*  Parse device string for mode selector "dev:mode" */
    if ((subdevice=strchr(devstr, ':'))) {
        *subdevice++ = '\0';
        xpost_dict_put(ctx, sd, xpost_name_cons(ctx, "SUBDEVICE"),
                xpost_object_cvlit(xpost_string_cons(ctx, strlen(subdevice), subdevice)));
    }

    /* define the /newdefaultdevice name called by /start */
    for (i = 0; device_strings[i][0]; i++)
    {
        if (strcmp(devstr, device_strings[i][0]) == 0)
        {
            break;
        }
    }
    if (set_size == XPOST_USE_SIZE){
        dimensions = malloc(2 + (int)ceil(log10(width)) + (int)ceil(log10(height)));
        sprintf(dimensions, "%d %d", width, height);
    } else {
        dimensions = "612 792";
    }
    newdevstr = xpost_string_cons(ctx,
                                  strlen(strtemplate) - 6
                                  + strlen(device_strings[i][1])
                                  + strlen(dimensions)
                                  + strlen(device_strings[i][2]) + 1,
                                  NULL);
    sprintf(xpost_string_get_pointer(ctx, newdevstr), strtemplate,
            device_strings[i][1], dimensions, device_strings[i][2]);
    --newdevstr.comp_.sz; /* trim the '\0' */

    namenewdev = xpost_name_cons(ctx, "newdefaultdevice");
    xpost_dict_put(ctx, sd, namenewdev, xpost_object_cvx(newdevstr));

    xpost_dict_put(ctx, sd, xpost_name_cons(ctx, "ShowpageSemantics"), xpost_int_cons(semantics));

    if (outfile)
    {
        xpost_dict_put(ctx, sd,
                       xpost_name_cons(ctx, "OutputFileName"),
                       xpost_object_cvlit(xpost_string_cons(ctx, strlen(outfile), outfile)));
    }

    if (bufferin)
    {
        Xpost_Object s = xpost_object_cvlit(xpost_string_cons(ctx, sizeof(bufferin), NULL));
        xpost_object_set_access(ctx, s, XPOST_OBJECT_TAG_ACCESS_NONE);
        memcpy(xpost_string_get_pointer(ctx, s), &bufferin, sizeof(bufferin));
        xpost_dict_put(ctx, sd, xpost_name_cons(ctx, "OutputBufferIn"), s);
    }

    if (bufferout)
    {
        Xpost_Object s = xpost_object_cvlit(xpost_string_cons(ctx, sizeof(bufferout), NULL));
        xpost_object_set_access(ctx, s, XPOST_OBJECT_TAG_ACCESS_NONE);
        memcpy(xpost_string_get_pointer(ctx, s), &bufferout, sizeof(bufferout));
        xpost_dict_put(ctx, sd, xpost_name_cons(ctx, "OutputBufferOut"), s);
    }

    ctx->vmmode = LOCAL;
    free(devstr);
}

/*
   load init.ps (which also loads err.ps) while systemdict is writeable
   ignore invalidaccess errors.
 */
static
void loadinitps(Xpost_Context *ctx)
{
    char buf[1024];
    char path_init_ps[XPOST_PATH_MAX];
    struct stat statbuf;
    char *path_init;
    char *path;
    int n;

    assert(ctx->gl->base);
    xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons(ctx, "quit", NULL,0,0));
    ctx->ignoreinvalidaccess = 1;

#define XPOST_PATH_INIT \
    do \
    { \
        snprintf(path_init_ps, sizeof(path_init_ps), "%s/init.ps", path); \
        if (stat(path_init_ps, &statbuf) == 0) \
        { \
            path_init = path; \
            goto load_init_ps; \
        } \
        else \
            XPOST_LOG_DBG("init.ps not present in", path_init_ps); \
    } while (0)

    /* environment variable XPOST_DATA_DIR */
    if ((path = getenv("XPOST_DATA_DIR")))
        XPOST_PATH_INIT;

    /* directory of the shared library */
    path = (char *)xpost_data_dir_get(); /* always well-defined */
    XPOST_PATH_INIT;

#ifdef PACKAGE_DATA_DIR
    path = PACKAGE_DATA_DIR;
    XPOST_PATH_INIT;
#endif

    XPOST_LOG_ERR("init.ps can not be found");

    return;

  load_init_ps:
    /* backslashes are not supported in path because they are inserted in
    * PostScript files, and PostScript */
#ifdef _WIN32
    path = path_init_ps;
    while (*path++) if (*path == '\\') *path = '/';
    path = path_init;
    while (*path++) if (*path == '\\') *path = '/';
#endif
    n = snprintf(buf, sizeof(buf),
                 "(%s) (r) file cvx "
                 "/DATA_DIR (%s) def exec ", path_init_ps, path_init);
    xpost_stack_push(ctx->lo, ctx->es,
                     xpost_object_cvx(xpost_string_cons(ctx, n, buf)));

    ctx->quit = 0;
    mainloop(ctx);
    ctx->ignoreinvalidaccess = 0;
}


/* copy userdict names to systemdict
    Problem: This is clearly an invalidaccess,
    and yet is required by the PLRM. Discussion:
https://groups.google.com/d/msg/comp.lang.postscript/VjCI0qxkGY4/y0urjqRA1IoJ
    The ignoreinvalidaccess exception has been isolated to this one case.
 */
static int copyudtosd(Xpost_Context *ctx, Xpost_Object ud, Xpost_Object sd)
{
    Xpost_Object ed, de;

    ctx->ignoreinvalidaccess = 1;
    xpost_dict_put(ctx, sd, xpost_name_cons(ctx, "userdict"), ud);
    ed = xpost_dict_get(ctx, ud, xpost_name_cons(ctx, "errordict"));
    if (xpost_object_get_type(ed) == invalidtype)
        return undefined;
    xpost_dict_put(ctx, sd, xpost_name_cons(ctx, "errordict"), ed);
    de = xpost_dict_get(ctx, ud, xpost_name_cons(ctx, "$error"));
    if (xpost_object_get_type(de) == invalidtype)
        return undefined;
    xpost_dict_put(ctx, sd, xpost_name_cons(ctx, "$error"), de);
    ctx->ignoreinvalidaccess = 0;
    return 0;
}


/*
   create an executable context using the given device,
   output configuration, and semantics.
 */
XPAPI Xpost_Context *xpost_create(const char *device,
                                  Xpost_Output_Type output_type,
                                  const void *outputptr,
                                  Xpost_Showpage_Semantics semantics,
                                  Xpost_Output_Message output_msg,
                                  Xpost_Set_Size set_size,
                                  int width,
                                  int height)
{
    Xpost_Object sd, ud;
    int ret;
    const char *outfile = NULL;
    const char *bufferin = NULL;
    char **bufferout = NULL;
    int quiet;

    switch (output_msg)
    {
        case XPOST_OUTPUT_MESSAGE_QUIET:
            quiet = 1;
            _xpost_interpreter_is_tracing = 0;
            break;
        case XPOST_OUTPUT_MESSAGE_VERBOSE:
            quiet = 0;
            _xpost_interpreter_is_tracing = 0;
            break;
        case XPOST_OUTPUT_MESSAGE_TRACING:
            quiet = 0;
            _xpost_interpreter_is_tracing = 1;
            break;
        default:
            XPOST_LOG_ERR("Wrong output message value");
            return NULL;;
    }


    switch (output_type)
    {
        case XPOST_OUTPUT_FILENAME:
            outfile = outputptr;
            break;
        case XPOST_OUTPUT_BUFFERIN:
            bufferin = outputptr;
            break;
        case XPOST_OUTPUT_BUFFEROUT:
            bufferout = (char **)outputptr;
            break;
        case XPOST_OUTPUT_DEFAULT:
            break;
    }

#if 0
    test_memory();
    if (!test_garbage_collect(xpost_interpreter_cid_init,
                              xpost_interpreter_cid_get_context,
                              xpost_interpreter_get_initializing,
                              xpost_interpreter_set_initializing,
                              xpost_interpreter_alloc_local_memory,
                              xpost_interpreter_alloc_global_memory))
        return NULL;
#endif

    nextid = 0; /*reset process counter */

    /* Allocate and initialize all interpreter data structures. */
    ret = initalldata(device);
    if (!ret)
    {
        return NULL;
    }

    /* extract systemdict and userdict for additional definitions */
    sd = xpost_stack_bottomup_fetch(xpost_ctx->lo, xpost_ctx->ds, 0);
    ud = xpost_stack_bottomup_fetch(xpost_ctx->lo, xpost_ctx->ds, 2);

    setlocalconfig(xpost_ctx, sd,
                   device, outfile, bufferin, bufferout,
                   semantics, set_size, width, height);

    if (quiet)
    {
        xpost_dict_put(xpost_ctx,
                       sd /*xpost_stack_bottomup_fetch(ctx->lo, ctx->ds, 0)*/ ,
                       xpost_name_cons(xpost_ctx, "QUIET"),
                       null);
    }

    xpost_stack_clear(xpost_ctx->lo, xpost_ctx->hold);
    xpost_interpreter_set_initializing(0);
    loadinitps(xpost_ctx);

    ret = copyudtosd(xpost_ctx, ud, sd);
    if (ret)
    {
        XPOST_LOG_ERR("%s error in copyudtosd", errorname[ret]);
        return NULL;
    }

    /* make systemdict readonly FIXME: use new access semantics */
    xpost_dict_put(xpost_ctx, sd, xpost_name_cons(xpost_ctx, "systemdict"), sd);
    xpost_object_set_access(xpost_ctx, sd, XPOST_OBJECT_TAG_ACCESS_READ_ONLY);
#if 0
    if (!xpost_stack_bottomup_replace(xpost_ctx->lo, xpost_ctx->ds, 0, xpost_object_set_access(xpost_ctx, sd, XPOST_OBJECT_TAG_ACCESS_READ_ONLY)))
    {
        XPOST_LOG_ERR("cannot replace systemdict in dict stack");
        return NULL;
    }
#endif

    xpost_interpreter_set_initializing(0);

    return xpost_ctx;
}

static
Xpost_Object get_token(Xpost_Context *ctx, char *str){
    Xpost_Object o;
    xpost_stack_push(ctx->lo, ctx->os, xpost_string_cons(ctx, strlen(str), str));
    xpost_operator_exec(ctx, xpost_operator_cons(ctx, "token",NULL,0,0).mark_.padw);
    if (xpost_stack_pop(ctx->lo, ctx->os).int_.val){
        o = xpost_stack_pop(ctx->lo, ctx->os);
        xpost_stack_pop(ctx->lo, ctx->os);
    } else {
        o = null;
    }
    return o;
}

XPAPI int xpost_add_definitions(Xpost_Context *ctx, int cnt, char *defs[])
{
    int i;
    Xpost_Object ud;

    if (!ctx) return 0;
    XPOST_LOG_INFO("adding %d defs", cnt);

    ud = xpost_stack_bottomup_fetch(ctx->lo, ctx->ds, 2);
    for (i = 0; i < cnt; i++)
    {
        char *eq = strchr(defs[i], '=');

        XPOST_LOG_INFO("%s", defs[i]);
        if (eq)
        {
            *eq++ = '\0';
            xpost_dict_put(ctx, ud,
                    xpost_name_cons(ctx, defs[i]),
                    get_token(ctx, eq));
            eq[-1] = '=';
        }
        else
        {
            xpost_dict_put(ctx, ud,
                    xpost_name_cons(ctx, defs[i]),
                    null);
        }
    }
    return 1;
}

/*
   execute ps program until quit, fall-through to quit,
   SHOWPAGE_RETURN semantic, or error (default action: message, purge and quit).
 */
XPAPI int xpost_run(Xpost_Context *ctx, Xpost_Input_Type input_type, const void *inputptr, size_t set_size)
{
    Xpost_Object lsav = null;
    int llev = 0;
    unsigned int vs;
    const char *ps_str = NULL;
    const char *ps_file = NULL;
    const FILE *ps_file_ptr = NULL;
    int ret;
    Xpost_Object device;
    Xpost_Object semantic;

    switch(input_type)
    {
        case XPOST_INPUT_FILENAME:
            ps_file = inputptr;
            break;
        case XPOST_INPUT_STRING:
            ps_str = inputptr;
            ps_file_ptr = tmpfile();
            if (set_size)
                fwrite(ps_str, 1, set_size, (FILE*)ps_file_ptr);
            else
                fwrite(ps_str, 1, strlen(ps_str), (FILE*)ps_file_ptr);
            rewind((FILE*)ps_file_ptr);
            break;
        case XPOST_INPUT_FILEPTR:
            ps_file_ptr = inputptr;
            break;
        case XPOST_INPUT_RESUME: /* resuming a returned session, skip startup */
            goto run;
    }

    /* prime the exec stack
       so it starts with a 'start*' procedure,
       and if it ever gets to the bottom, it quits.
       These procedures are all defined in data/init.ps
     */
    xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons(ctx, "quit", NULL,0,0));
    /*
       if ps_file is NULL:
         if stdin is a tty
           `start` proc defined in init.ps runs `executive` which prompts for user input
         else
           'startstdin' executes stdin but does not prompt

       if ps_file is not NULL:
       'startfile' executes a named file wrapped in a stopped context with handleerror
    */
    if (ps_file)
    {
        /*printf("ps_file\n"); */
        xpost_stack_push(ctx->lo, ctx->os, xpost_object_cvlit(xpost_string_cons(ctx, strlen(ps_file), ps_file)));
        xpost_stack_push(ctx->lo, ctx->es, xpost_object_cvx(xpost_name_cons(ctx, "startfilename")));
    }
    else if (ps_file_ptr)
    {
        xpost_stack_push(ctx->lo, ctx->os, xpost_object_cvlit(xpost_file_cons(ctx->lo, ps_file_ptr)));
        xpost_stack_push(ctx->lo, ctx->es, xpost_object_cvx(xpost_name_cons(ctx, "startfile")));
    }
    else
    {
        if (xpost_isatty(fileno(stdin)))
            xpost_stack_push(ctx->lo, ctx->es, xpost_object_cvx(xpost_name_cons(ctx, "start")));
        else
            xpost_stack_push(ctx->lo, ctx->es, xpost_object_cvx(xpost_name_cons(ctx, "startstdin")));
    }

    (void) xpost_save_create_snapshot_object(ctx->gl);
    lsav = xpost_save_create_snapshot_object(ctx->lo);

    /* Run! */
run:
    ctx->quit = 0;
    ctx->state = C_RUN;
    ret = mainloop(ctx);

    semantic = xpost_dict_get(ctx,
                  xpost_stack_bottomup_fetch(ctx->lo, ctx->ds, 0),
                  xpost_name_cons(ctx, "ShowpageSemantics"));
    if (semantic.int_.val == XPOST_SHOWPAGE_RETURN)
        return ret == 1 ? yieldtocaller : 0;

    XPOST_LOG_INFO("destroying device");
    device = xpost_dict_get(ctx,
            xpost_stack_bottomup_fetch(ctx->lo, ctx->ds, 2),
            xpost_name_cons(ctx, "DEVICE"));
    XPOST_LOG_INFO("device type=%s", xpost_object_type_names[xpost_object_get_type(device)]);
    /*xpost_operator_dump(ctx, 1); // is this pointer value constant? */
    if (xpost_object_get_type(device) == arraytype){
        XPOST_LOG_INFO("running proc");
        xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons(ctx, "quit", NULL,0,0));
        xpost_stack_push(ctx->lo, ctx->es, device);

        ctx->quit = 0;
        mainloop(ctx);

        device = xpost_stack_pop(ctx->lo, ctx->os);
    }
    if (xpost_object_get_type(device) == dicttype)
    {
        Xpost_Object Destroy;
        XPOST_LOG_INFO("destroying device dict");
        Destroy = xpost_dict_get(ctx, device, xpost_name_cons(ctx, "Destroy"));
        if (xpost_object_get_type(Destroy) == operatortype)
        {
            int res;
            xpost_stack_push(ctx->lo, ctx->os, device);
            res = xpost_operator_exec(ctx, Destroy.mark_.padw);
            if (res)
                XPOST_LOG_ERR("%s error destroying device", errorname[res]);
            else
                XPOST_LOG_INFO("destroyed device");
        }
    }

    xpost_save_restore_snapshot(ctx->gl);
    xpost_memory_table_get_addr(ctx->lo,
                                XPOST_MEMORY_TABLE_SPECIAL_SAVE_STACK, &vs);
    if (xpost_object_get_type(lsav) == savetype)
    {
        for ( llev = xpost_stack_count(ctx->lo, vs);
                llev > lsav.save_.lev;
                llev-- )
        {
            xpost_save_restore_snapshot(ctx->lo);
        }
    }

    return noerror;
}

/*
   destroy the given context and associated memory files (if not in use by a shared context)
   exit interpreter if all contexts are destroyed.
 */
XPAPI void xpost_destroy(Xpost_Context *ctx)
{

    if (!xpost_dict_known_key(ctx, ctx->gl, xpost_stack_bottomup_fetch(ctx->lo, ctx->ds, 0), xpost_name_cons(ctx, "QUIET")))
    {
        printf("bye!\n");
        fflush(NULL);
    }
    /*xpost_garbage_collect(itpdata->ctab->gl, 1, 1); */
    /*xpost_garbage_collect(itpdata->ctab->lo, 1, 1); */
#if 0
#ifndef XPOST_NO_GC
    xpost_garbage_collect(ctx->gl, 1, 1);
    xpost_garbage_collect(ctx->lo, 1, 0);
#endif
#endif

    /* exit if all contexts are destroyed */
    /*xpost_interpreter_exit(itpdata); */
    /*free(itpdata); */
}
