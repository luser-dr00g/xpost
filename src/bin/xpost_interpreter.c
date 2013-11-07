/*
 * Xpost - a Level-2 Postscript interpreter
 * Copyright (C) 2013, Michael Joshua Ryan
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
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h> /* isattty */
#endif

#ifdef __MINGW32__
# include "osmswin.h" /* mkstemp xpost_getpagesize */
#else
# include "osunix.h" /* xpost_getpagesize */
#endif

#include "xpost_memory.h"  // itp contexts contain mfiles and mtabs
#include "xpost_object.h"  // eval functions examine objects
#include "xpost_stack.h"  // eval functions manipulate stacks
#include "xpost_context.h"
#include "xpost_interpreter.h" // uses: context itp MAXCONTEXT MAXMFILE
#include "xpost_garbage.h"  //  test garbage collector
#include "xpost_save.h"  // save/restore vm
#include "xpost_error.h"  // interpreter catches errors
#include "xpost_string.h"  // eval functions examine strings
#include "xpost_array.h"  // eval functions examine arrays
#include "xpost_name.h"  // eval functions examine names
#include "xpost_dict.h"  // eval functions examine dicts
#include "xpost_file.h"  // eval functions examine files
#include "xpost_operator.h"  // eval functions call operators
#include "xpost_pathname.h" // determine whether xpost is installed

int TRACE = 0;
itp *itpdata;
int initializing = 1;
int ignoreinvalidaccess = 0;

void eval(context *ctx);
void mainloop(context *ctx);
void init(void);
void xit(void);

/* find the next unused mfile in the global memory table */
Xpost_Memory_File *nextgtab(void)
{
    int i;

    for (i=0; i < MAXMFILE; i++) {
        if (itpdata->gtab[i].base == NULL) {
            return &itpdata->gtab[i];
        }
    }
    error(unregistered, "cannot allocate Xpost_Memory_File, gtab exhausted");
    exit(EXIT_FAILURE);
}

/* find the next unused mfile in the local memory table */
Xpost_Memory_File *nextltab(void)
{
    int i;
    for (i=0; i < MAXMFILE; i++) {
        if (itpdata->ltab[i].base == NULL) {
            return &itpdata->ltab[i];
        }
    }
    error(unregistered, "cannot allocate Xpost_Memory_File, ltab exhausted");
    exit(EXIT_FAILURE);
}


/* cursor to next cid number to try to allocate */
static
unsigned nextid = 0;

/* allocate a context-id and associated context struct
   returns cid;
 */
unsigned initctxid(void)
{
    unsigned startid = nextid;
    while ( ctxcid(++nextid)->state != 0 ) {
        if (nextid == startid + MAXCONTEXT)
            error(unregistered, "ctab full. cannot create new process");
    }
    return nextid;
}

/* adapter:
           ctx <- cid
   yield pointer to context struct given cid */
context *ctxcid(unsigned cid)
{
    //TODO reject cid 0
    return &itpdata->ctab[ (cid-1) % MAXCONTEXT ];
}



/* initialize itp */
void inititp(itp *itpptr)
{
    initcontext(&itpptr->ctab[0]);
    itpptr->cid = itpptr->ctab[0].id;
}

/* destroy itp */
void exititp(itp *itpptr)
{
    exitcontext(&itpptr->ctab[0]);
}



/* function type for interpreter action pointers */
typedef
void evalfunc(context *ctx);

/* quit the interpreter */
static
void evalquit(context *ctx)
{
    ++ctx->quit;
}

/* pop the execution stack */
static
void evalpop(context *ctx)
{
    (void)xpost_stack_pop(ctx->lo, ctx->es);
}

/* pop the execution stack onto the operand stack */
static
void evalpush(context *ctx)
{
    xpost_stack_push(ctx->lo, ctx->os,
            xpost_stack_pop(ctx->lo, ctx->es) );
}

/* load executable name */
static
void evalload(context *ctx)
{
    Xpost_Object s = strname(ctx, xpost_stack_topdown_fetch(ctx->lo, ctx->es, 0));
    if (TRACE)
        printf("evalload <name \"%*s\">", s.comp_.sz, charstr(ctx, s));

    xpost_stack_push(ctx->lo, ctx->os,
            xpost_stack_pop(ctx->lo, ctx->es));
    assert(ctx->gl->base);
    //opexec(ctx, consoper(ctx, "load", NULL,0,0).mark_.padw);
    opexec(ctx, ctx->opcode_shortcuts.load);
    if (xpost_object_is_exe(xpost_stack_topdown_fetch(ctx->lo, ctx->os, 0))) {
        xpost_stack_push(ctx->lo, ctx->es,
                xpost_stack_pop(ctx->lo, ctx->os));
    }
}

/* execute operator */
static
void evaloperator(context *ctx)
{
    Xpost_Object op = xpost_stack_pop(ctx->lo, ctx->es);

    if (TRACE)
        dumpoper(ctx, op.mark_.padw);
    opexec(ctx, op.mark_.padw);
}

/* extract head (&tail) of array */
static
void evalarray(context *ctx)
{
    Xpost_Object a = xpost_stack_pop(ctx->lo, ctx->es);
    Xpost_Object b;

    switch (a.comp_.sz) {
    default /* > 1 */:
        xpost_stack_push(ctx->lo, ctx->es, arrgetinterval(a, 1, a.comp_.sz - 1) );
        /*@fallthrough@*/
    case 1:
        b = barget(ctx, a, 0);
        if (xpost_object_get_type(b) == arraytype)
            xpost_stack_push(ctx->lo, ctx->os, b);
        else
            xpost_stack_push(ctx->lo, ctx->es, b);
        /*@fallthrough@*/
    case 0: /* drop */;
    }
}

/* extract token from string */
static
void evalstring(context *ctx)
{
    Xpost_Object b,t,s;

    s = xpost_stack_pop(ctx->lo, ctx->es);
    xpost_stack_push(ctx->lo, ctx->os, s);
    assert(ctx->gl->base);
    //opexec(ctx, consoper(ctx, "token",NULL,0,0).mark_.padw);
    opexec(ctx, ctx->opcode_shortcuts.token);
    b = xpost_stack_pop(ctx->lo, ctx->os);
    if (b.int_.val) {
        t = xpost_stack_pop(ctx->lo, ctx->os);
        s = xpost_stack_pop(ctx->lo, ctx->os);
        xpost_stack_push(ctx->lo, ctx->es, s);
        xpost_stack_push(ctx->lo, xpost_object_get_type(t)==arraytype? ctx->os: ctx->es, t);
    }
}

/* extract token from file */
static
void evalfile(context *ctx)
{
    Xpost_Object b,f,t;

    f = xpost_stack_pop(ctx->lo, ctx->es);
    xpost_stack_push(ctx->lo, ctx->os, f);
    assert(ctx->gl->base);
    //opexec(ctx, consoper(ctx, "token",NULL,0,0).mark_.padw);
    opexec(ctx, ctx->opcode_shortcuts.token);
    b = xpost_stack_pop(ctx->lo, ctx->os);
    if (b.int_.val) {
        t = xpost_stack_pop(ctx->lo, ctx->os);
        xpost_stack_push(ctx->lo, ctx->es, f);
        xpost_stack_push(ctx->lo, xpost_object_get_type(t)==arraytype? ctx->os: ctx->es, t);
    } else {
        fileclose(ctx->lo, f);
    }
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
//evalfunc *evalfile = evalpush;
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

/* one iteration of the central loop */
void eval(context *ctx)
{
    Xpost_Object t = xpost_stack_topdown_fetch(ctx->lo, ctx->es, 0);

    ctx->currentobject = t;
    assert(ctx);
    assert(ctx->lo);
    assert(ctx->lo->base);
    assert(ctx->gl);
    assert(ctx->gl->base);

    if (TRACE) {
        printf("\neval\n");
        printf("Executing: ");
        xpost_object_dump(t);
        printf("\n");
        printf("Stack: ");
        xpost_stack_dump(ctx->lo, ctx->os);
        printf("\n");
        printf("Dict Stack: ");
        xpost_stack_dump(ctx->lo, ctx->ds);
        printf("\n");
        printf("Exec Stack: ");
        xpost_stack_dump(ctx->lo, ctx->es);
        printf("\n");
    }

    if ( xpost_object_is_exe(t) ) /* if executable */
        evaltype[xpost_object_get_type(t)](ctx);
    else
        evalpush(ctx);
}


/* the return point from all calls to error() that do not exit() */
jmp_buf jbmainloop;
int jbmainloopset = 0;

/* the big main central interpreter loop. */
void mainloop(context *ctx)
{
    volatile int err;

    if ((err = setjmp(jbmainloop))) {
        onerror(ctx, err);
    }
    jbmainloopset = 1;

    while(!ctx->quit)
        eval(ctx);

    jbmainloopset = 0;
}

//#ifdef TESTMODULE_ITP

/* global shortcut for a single-threaded interpreter */
context *xpost_ctx;

/* string constructor helper for literals */
#define CNT_STR(s) sizeof(s)-1, s

/* set global pagesize, initialize eval's jump-table */
static void initalldata(void)
{
    xpost_memory_pagesize = xpost_getpagesize();
    initializing = 1;
    initevaltype();

    /* allocate the top-level itpdata data structure. */
    null = xpost_object_cvlit(null);
    itpdata = malloc(sizeof*itpdata);
    if (!itpdata) error(unregistered, "itpdata=malloc failed");
    memset(itpdata, 0, sizeof*itpdata);

    /* allocate and initialize the first context structure
       and associated memory structures.
       populate OPTAB and systemdict with operators.
       push systemdict, globaldict, and userdict on dict stack
     */
    inititp(itpdata);

    /* set global shortcut to context_0
       (the only context in a single-threaded interpreter) */
    xpost_ctx = &itpdata->ctab[0];
}

static void setdatadir(context *ctx, Xpost_Object sd)
{
    /* create a symbol to locate /data files */
    ctx->vmmode = GLOBAL;
    if (is_installed) {
        bdcput(ctx, sd, consname(ctx, "PACKAGE_DATA_DIR"),
            xpost_object_cvlit(consbst(ctx,
                    CNT_STR(PACKAGE_DATA_DIR))));
        bdcput(ctx, sd, consname(ctx, "PACKAGE_INSTALL_DIR"),
            xpost_object_cvlit(consbst(ctx,
                    CNT_STR(PACKAGE_INSTALL_DIR))));
    }
    bdcput(ctx, sd, consname(ctx, "EXE_DIR"),
            xpost_object_cvlit(consbst(ctx,
                    strlen(exedir), exedir)));
    ctx->vmmode = LOCAL;
}

/* load init.ps and err.ps while systemdict is writeable */
static void loadinitps(context *ctx)
{
    assert(ctx->gl->base);
    xpost_stack_push(ctx->lo, ctx->es, consoper(ctx, "quit", NULL,0,0));
/*splint doesn't like the composed macros*/
#ifndef S_SPLINT_S
    if (is_installed)
        xpost_stack_push(ctx->lo, ctx->es,
            xpost_object_cvx(consbst(ctx,
             CNT_STR("(" PACKAGE_DATA_DIR "/init.ps) (r) file cvx exec"))));
    else {
        char buf[1024];
        snprintf(buf, sizeof buf,
                "(%s/../../data/init.ps) (r) file cvx exec",
                exedir);
        xpost_stack_push(ctx->lo, ctx->es,
            xpost_object_cvx(consbst(ctx,
                    strlen(buf), buf)));
    }
#endif
    ctx->quit = 0;
    mainloop(ctx);
}

static void copyudtosd(context *ctx, Xpost_Object ud, Xpost_Object sd)
{
    /* copy userdict names to systemdict
        Problem: This is clearly an invalidaccess,
        and yet is required by the PLRM. Discussion:
https://groups.google.com/d/msg/comp.lang.postscript/VjCI0qxkGY4/y0urjqRA1IoJ
     */

    ignoreinvalidaccess = 1;
    bdcput(ctx, sd, consname(ctx, "userdict"), ud);
    bdcput(ctx, sd, consname(ctx, "errordict"),
           bdcget(ctx, ud, consname(ctx, "errordict")));
    bdcput(ctx, sd, consname(ctx, "$error"),
           bdcget(ctx, ud, consname(ctx, "$error")));
    ignoreinvalidaccess = 0;
}


int createitp(void)
{
    Xpost_Object sd, ud;

    //test_memory();
    if (!test_garbage_collect())
        return 0;

    nextid = 0; //reset process counter

    /* Allocate and initialize all interpreter data structures. */
    initalldata();

    /* extract systemdict and userdict for additional definitions */
    sd = xpost_stack_bottomup_fetch(xpost_ctx->lo, xpost_ctx->ds, 0);
    ud = xpost_stack_bottomup_fetch(xpost_ctx->lo, xpost_ctx->ds, 2);

    setdatadir(xpost_ctx, sd);

    loadinitps(xpost_ctx);

    copyudtosd(xpost_ctx, ud, sd);

    /* make systemdict readonly */
    bdcput(xpost_ctx, sd, consname(xpost_ctx, "systemdict"), xpost_object_set_access(sd, XPOST_OBJECT_TAG_ACCESS_READ_ONLY));
    xpost_stack_bottomup_replace(xpost_ctx->lo, xpost_ctx->ds, 0, xpost_object_set_access(sd, XPOST_OBJECT_TAG_ACCESS_READ_ONLY));

    return 1;
}


void runitp(void)
{
    Xpost_Object lsav;
    int llev;
    unsigned int vs;

    /* prime the exec stack
       so it starts with 'start',
       and if it ever gets to the bottom, it quits.  */
    xpost_stack_push(xpost_ctx->lo, xpost_ctx->es, consoper(xpost_ctx, "quit", NULL,0,0));
        /* `start` proc defined in init.ps runs `executive` which prompts for user input
           'startstdin' does not prompt
         */
    if (isatty(fileno(stdin)))
        xpost_stack_push(xpost_ctx->lo, xpost_ctx->es, xpost_object_cvx(consname(xpost_ctx, "start")));
    else
        xpost_stack_push(xpost_ctx->lo, xpost_ctx->es, xpost_object_cvx(consname(xpost_ctx, "startstdin")));

    (void) save(xpost_ctx->gl);
    lsav = save(xpost_ctx->lo);

    /* Run! */
    initializing = 0;
    xpost_ctx->quit = 0;
    mainloop(xpost_ctx);

    restore(xpost_ctx->gl);
    xpost_memory_table_get_addr(xpost_ctx->lo,
            XPOST_MEMORY_TABLE_SPECIAL_SAVE_STACK, &vs);
    for ( llev = xpost_stack_count(xpost_ctx->lo, vs);
            llev > lsav.save_.lev;
            llev-- )
    {
        restore(xpost_ctx->lo);
    }
}

void destroyitp(void)
{
    //dumpoper(ctx, 1); // is this pointer value constant?
    printf("bye!\n");
    fflush(NULL);
    collect(itpdata->ctab->gl, 1, 1);
    exititp(itpdata);
    free(itpdata);
}

//#endif
