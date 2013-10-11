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
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "xpost_memory.h"  // itp contexts contain mfiles and mtabs
#include "xpost_object.h"  // eval functions examine objects
#include "xpost_stack.h"  // eval functions manipulate stacks
#include "xpost_interpreter.h" // uses: context itp MAXCONTEXT MAXMFILE
#include "xpost_error.h"  // interpreter catches errors
#include "xpost_string.h"  // eval functions examine strings
#include "xpost_array.h"  // eval functions examine arrays
#include "xpost_garbage.h"  // interpreter initializes garbage collector
#include "xpost_save.h"  // interpreter initializes save/restore stacks
#include "xpost_name.h"  // eval functions examine names
#include "xpost_dict.h"  // eval functions examine dicts
#include "xpost_file.h"  // eval functions examine files
#include "xpost_operator.h"  // eval functions call operators
#include "xpost_op_token.h"  // token operator functions
#include "xpost_op_dict.h"  // dictionary operator functions

#ifdef HAVE_WIN32
# include "osmswin.h" // mkstemp
#endif

int TRACE = 0;
itp *itpdata;
int initializing = 1;
int ignoreinvalidaccess = 0;

static unsigned makestack(mfile *mem);
void eval(context *ctx);
void mainloop(context *ctx);
void dumpctx(context *ctx);
void init(void);
void xit(void);

/* build a stack, return address */
static
unsigned makestack(mfile *mem)
{
    return initstack(mem);
}

/* initialize the context list 
   special entity in the mfile */
void initctxlist(mfile *mem)
{
    unsigned ent;
    mtab *tab;
    ent = mtalloc(mem, 0, MAXCONTEXT * sizeof(unsigned), 0);
    assert(ent == CTXLIST);
    tab = (void *)mem->base;
    memset(mem->base + tab->tab[CTXLIST].adr, 0,
            MAXCONTEXT * sizeof(unsigned));
}

/* add a context ID to the context list in mfile */
static
void addtoctxlist(mfile *mem,
                  unsigned cid)
{
    int i;
    mtab *tab;
    unsigned *ctxlist;

    tab = (void *)mem->base;
    ctxlist = (void *)(mem->base + tab->tab[CTXLIST].adr);
    // find first empty
    for (i=0; i < MAXCONTEXT; i++) {
        if (ctxlist[i] == 0) {
            ctxlist[i] = cid;
            return;
        }
    }
    error(unregistered, "ctxlist full");
}

/* find the next unused mfile in the global memory table */
static
mfile *nextgtab(void)
{
    int i;

    for (i=0; i < MAXMFILE; i++) {
        if (itpdata->gtab[i].base == NULL) {
            return &itpdata->gtab[i];
        }
    }
    error(unregistered, "cannot allocate mfile, gtab exhausted");
    exit(EXIT_FAILURE);
}

/* set up global vm in the context
 */
static
void initglobal(context *ctx)
{
    char g_filenam[] = "gmemXXXXXX";
    int fd;
    ctx->vmmode = GLOBAL;

    /* allocate and initialize global vm */
    //ctx->gl = malloc(sizeof(mfile));
    //ctx->gl = &itpdata->gtab[0];
    ctx->gl = nextgtab();

    fd = mkstemp(g_filenam);

    initmem(ctx->gl, g_filenam, fd);
    (void)initmtab(ctx->gl);
    initfree(ctx->gl);
    initsave(ctx->gl);
    initctxlist(ctx->gl);
    addtoctxlist(ctx->gl, ctx->id);

    ctx->gl->start = OPTAB + 1; /* so OPTAB is not collected and not scanned. */
}

/* find the next unused mfile in the local memory table */
static
mfile *nextltab(void)
{
    int i;
    for (i=0; i < MAXMFILE; i++) {
        if (itpdata->ltab[i].base == NULL) {
            return &itpdata->ltab[i];
        }
    }
    error(unregistered, "cannot allocate mfile, ltab exhausted");
    exit(EXIT_FAILURE);
}

/* set up local vm in the context
   allocates all stacks
 */
static
void initlocal(context *ctx)
{
    char l_filenam[] = "lmemXXXXXX";
    int fd;
    ctx->vmmode = LOCAL;

    /* allocate and initialize local vm */
    //ctx->lo = malloc(sizeof(mfile));
    //ctx->lo = &itpdata->ltab[0];
    ctx->lo = nextltab();

    fd = mkstemp(l_filenam);

    initmem(ctx->lo, l_filenam, fd);
    (void)initmtab(ctx->lo);
    initfree(ctx->lo);
    initsave(ctx->lo);
    initctxlist(ctx->lo);
    addtoctxlist(ctx->lo, ctx->id);
    //ctx->lo->roots[0] = VS;

    ctx->os = makestack(ctx->lo);
    ctx->es = makestack(ctx->lo);
    ctx->ds = makestack(ctx->lo);
    ctx->hold = makestack(ctx->lo);
    //ctx->lo->roots[1] = DS;
    //ctx->lo->start = HOLD + 1; /* so HOLD is not collected and not scanned. */
    //ctx->lo->start = CTXLIST + 1;
    ctx->lo->start = BOGUSNAME + 1;
}


/* cursor to next cid number to try to allocate */
static
unsigned nextid = 0;

/* allocate a context-id and associated context struct
   returns cid;
 */
static
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


/* initialize context
   allocates operator table
   allocates systemdict
   populates systemdict and optab with operators
 */
void initcontext(context *ctx)
{
    ctx->id = initctxid();
    initlocal(ctx);
    initglobal(ctx);

    initnames(ctx); /* NAMES NAMET */
    ctx->vmmode = GLOBAL;

    initoptab(ctx); /* allocate and zero the optab structure */

    (void)consname(ctx, "maxlength"); /* seed the tree with a word from the middle of the alphabet */
    (void)consname(ctx, "getinterval"); /* middle of the start */
    (void)consname(ctx, "setmiterlimit"); /* middle of the end */

    initop(ctx); /* populate the optab (and systemdict) with operators */

    {
        object gd; //globaldict
        gd = consbdc(ctx, 100);
        bdcput(ctx, bot(ctx->lo, ctx->ds, 0), consname(ctx, "globaldict"), gd);
        push(ctx->lo, ctx->ds, gd);
    }

    ctx->vmmode = LOCAL;
    (void)consname(ctx, "minimal"); /* seed the tree with a word from the middle of the alphabet */
    (void)consname(ctx, "interest"); /* middle of the start */
    (void)consname(ctx, "solitaire"); /* middle of the end */
    {
        object ud; //userdict
        ud = consbdc(ctx, 100);
        bdcput(ctx, ud, consname(ctx, "userdict"), ud);
        push(ctx->lo, ctx->ds, ud);
    }
}

/* destroy context */
void exitcontext(context *ctx)
{
    exitmem(ctx->gl);
    exitmem(ctx->lo);
}

/*
   fork new process with private global and private local vm
   (spawn jobserver)
   */
static
unsigned fork1(context *ctx)
{
    unsigned newcid;
    context *newctx;

    newcid = initctxid();
    newctx = ctxcid(newcid);
    initlocal(ctx);
    initglobal(ctx);
    ctx->vmmode = LOCAL;
    return newcid;
}

/*
   fork new process with shared global vm and private local vm
   (new "application"?)
   */
static
unsigned fork2(context *ctx)
{
    unsigned newcid;
    context *newctx;

    newcid = initctxid();
    newctx = ctxcid(newcid);
    initlocal(ctx);
    newctx->gl = ctx->gl;
    addtoctxlist(newctx->gl, newcid);
    push(newctx->lo, newctx->ds, bot(ctx->lo, ctx->ds, 0)); // systemdict
    return newcid;
}

/*
   fork new process with shared global and shared local vm
   (lightweight process)
   */
static
unsigned fork3(context *ctx)
{
    unsigned newcid;
    context *newctx;

    newcid = initctxid();
    newctx = ctxcid(newcid);
    newctx->lo = ctx->lo;
    addtoctxlist(newctx->lo, newcid);
    newctx->gl = ctx->gl;
    addtoctxlist(newctx->gl, newcid);
    push(newctx->lo, newctx->ds, bot(ctx->lo, ctx->ds, 0)); // systemdict
    return newcid;
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


/* return the global or local memory file for the composite object */
/*@dependent@*/
mfile *bank(context *ctx,
            object o)
{
    return o.tag&FBANK? ctx->gl : ctx->lo;
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
    (void)pop(ctx->lo, ctx->es);
}

/* pop the execution stack onto the operand stack */
static
void evalpush(context *ctx)
{
    push(ctx->lo, ctx->os,
            pop(ctx->lo, ctx->es) );
}

/* load executable name */
static
void evalload(context *ctx)
{
    object s = strname(ctx, top(ctx->lo, ctx->es, 0));
    if (TRACE)
        printf("evalload <name \"%*s\">", s.comp_.sz, charstr(ctx, s));

    push(ctx->lo, ctx->os,
            pop(ctx->lo, ctx->es));
    assert(ctx->gl->base);
    //opexec(ctx, consoper(ctx, "load", NULL,0,0).mark_.padw);
    opexec(ctx, ctx->opcuts.load);
    if (isx(top(ctx->lo, ctx->os, 0))) {
        push(ctx->lo, ctx->es,
                pop(ctx->lo, ctx->os));
    }
}

/* execute operator */
static
void evaloperator(context *ctx)
{
    object op = pop(ctx->lo, ctx->es);

    if (TRACE)
        dumpoper(ctx, op.mark_.padw);
    opexec(ctx, op.mark_.padw);
}

/* extract head (&tail) of array */
static
void evalarray(context *ctx)
{
    object a = pop(ctx->lo, ctx->es);
    object b;

    switch (a.comp_.sz) {
    default /* > 1 */:
        push(ctx->lo, ctx->es, arrgetinterval(a, 1, a.comp_.sz - 1) );
        /*@fallthrough@*/
    case 1:
        b = barget(ctx, a, 0);
        if (type(b) == arraytype)
            push(ctx->lo, ctx->os, b);
        else
            push(ctx->lo, ctx->es, b);
        /*@fallthrough@*/
    case 0: /* drop */;
    }
}

/* extract token from string */
static
void evalstring(context *ctx)
{
    object b,t,s;

    s = pop(ctx->lo, ctx->es);
    push(ctx->lo, ctx->os, s);
    assert(ctx->gl->base);
    //opexec(ctx, consoper(ctx, "token",NULL,0,0).mark_.padw);
    opexec(ctx, ctx->opcuts.token);
    b = pop(ctx->lo, ctx->os);
    if (b.int_.val) {
        t = pop(ctx->lo, ctx->os);
        s = pop(ctx->lo, ctx->os);
        push(ctx->lo, ctx->es, s);
        push(ctx->lo, type(t)==arraytype? ctx->os: ctx->es, t);
    }
}

/* extract token from file */
static
void evalfile(context *ctx)
{
    object b,f,t;

    f = pop(ctx->lo, ctx->es);
    push(ctx->lo, ctx->os, f);
    assert(ctx->gl->base);
    //opexec(ctx, consoper(ctx, "token",NULL,0,0).mark_.padw);
    opexec(ctx, ctx->opcuts.token);
    b = pop(ctx->lo, ctx->os);
    if (b.int_.val) {
        t = pop(ctx->lo, ctx->os);
        push(ctx->lo, ctx->es, f);
        push(ctx->lo, type(t)==arraytype? ctx->os: ctx->es, t);
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

evalfunc *evalcontext = evalpush;
//evalfunc *evalfile = evalpush;
evalfunc *evalname = evalload;

/* install the evaltype functions (possibly via pointers) in the jump table */
evalfunc *evaltype[NTYPES + 1];
#define AS_EVALINIT(_) evaltype[ _ ## type ] = eval ## _ ;

/* use above macro to initialize function table
   keyed by enum types;
 */
static
void initevaltype(void)
{
    TYPES(AS_EVALINIT)
}

/* one iteration of the central loop */
void eval(context *ctx)
{
    object t = top(ctx->lo, ctx->es, 0);

    ctx->currentobject = t;
    assert(ctx);
    assert(ctx->lo);
    assert(ctx->lo->base);
    assert(ctx->gl);
    assert(ctx->gl->base);

    if (TRACE) {
        printf("\neval\n");
        printf("Executing: ");
        dumpobject(t);
        printf("\n");
        printf("Stack: ");
        dumpstack(ctx->lo, ctx->os);
        printf("\n");
        printf("Dict Stack: ");
        dumpstack(ctx->lo, ctx->ds);
        printf("\n");
        printf("Exec Stack: ");
        dumpstack(ctx->lo, ctx->es);
        printf("\n");
    }

    if ( isx(t) ) /* if executable */
        evaltype[type(t)](ctx);
    else
        evalpush(ctx);
}

/* the return point from all calls to error() that do not exit() */
jmp_buf jbmainloop;
bool jbmainloopset = false;

/* the big main central interpreter loop. */
void mainloop(context *ctx)
{
    volatile int err; 

    if ((err = setjmp(jbmainloop))) {
        onerror(ctx, err);
    }
    jbmainloopset = true;

    while(!ctx->quit)
        eval(ctx);

    jbmainloopset = false;
}

/* print a dump of the context struct */
void dumpctx(context *ctx)
{
    dumpmfile(ctx->gl);
    dumpmtab(ctx->gl, 0);
    dumpmfile(ctx->lo);
    dumpmtab(ctx->lo, 0);
    dumpnames(ctx);
}

//#ifdef TESTMODULE_ITP

/* global shortcut for a single-threaded interpreter */
context *ctx;

/* string constructor helper for literals */
#define CNT_STR(s) sizeof(s)-1, s

/* set global pagesize, initialize eval's jump-table */
void initalldata(void)
{
    pgsz = getpagesize();
    initializing = 1;
    initevaltype();

    /* allocate the top-level itpdata data structure. */
    null = cvlit(null);
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
    ctx = &itpdata->ctab[0];
}

void setdatadir(context *ctx, object sd)
{
    /* create a symbol to locate /data files */
    ctx->vmmode = GLOBAL;
    bdcput(ctx, sd, consname(ctx, "PACKAGE_DATA_DIR"),
            cvlit(consbst(ctx, CNT_STR(PACKAGE_DATA_DIR))));
    ctx->vmmode = LOCAL;
}

/* load init.ps and err.ps while systemdict is writeable */
void loadinitps(context *ctx)
{
    assert(ctx->gl->base);
    push(ctx->lo, ctx->es, consoper(ctx, "quit", NULL,0,0));
    push(ctx->lo, ctx->es,
        cvx(consbst(ctx,
            CNT_STR("(" PACKAGE_DATA_DIR "/init.ps) (r) file cvx exec"))));
    ctx->quit = 0;
    mainloop(ctx);
}

void copyudtosd(context *ctx, object ud, object sd)
{
    /* copy userdict names to systemdict
        Problem: This is clearly an invalidaccess,
        and yet is required by the PLRM. Discussion:
https://groups.google.com/d/msg/comp.lang.postscript/VjCI0qxkGY4/y0urjqRA1IoJ
     */
    bdcput(ctx, sd, consname(ctx, "userdict"), ud);
    bdcput(ctx, sd, consname(ctx, "errordict"),
            bdcget(ctx, ud, consname(ctx, "errordict")));
    bdcput(ctx, sd, consname(ctx, "$error"),
            bdcget(ctx, ud, consname(ctx, "$error")));
}

void createitp(void)
{
    object sd, ud;

    /* Allocate and initialize all interpreter data structures. */
    initalldata();

    /* extract systemdict and userdict for additional definitions */
    sd = bot(ctx->lo, ctx->ds, 0);
    ud = bot(ctx->lo, ctx->ds, 2);

    setdatadir(ctx, sd);

/* FIXME: Squeeze and eliminate this workaround.
   Ignoring errors is a bad idea.  */
ignoreinvalidaccess = 1;

    loadinitps(ctx);

    copyudtosd(ctx, ud, sd);

ignoreinvalidaccess = 0;

    /* make systemdict readonly */
    bdcput(ctx, sd, consname(ctx, "systemdict"), setfaccess(sd,readonly));
    tob(ctx->lo, ctx->ds, 0, setfaccess(sd, readonly));
}

void runitp(void)
{
    object gsav, lsav;
    int glev, llev;
    /* prime the exec stack
       so it starts with 'start',
       and if it ever gets to the bottom, it quits.  */
    push(ctx->lo, ctx->es, consoper(ctx, "quit", NULL,0,0)); 
        /* `start` proc defined in init.ps */
    push(ctx->lo, ctx->es, cvx(consname(ctx, "start")));

#if 0
    gsav = save(ctx->gl);
    lsav = save(ctx->lo);
#endif

    /* Run! */
    initializing = 0;
    ctx->quit = 0;
    mainloop(ctx);

#if 0
    for ( glev = count(ctx->gl, adrent(ctx->gl, VS));
            glev > gsav.save_.lev;
            glev-- ) {
        restore(ctx->gl);
    }
    for ( llev = count(ctx->lo, adrent(ctx->lo, VS));
            llev > lsav.save_.lev;
            llev-- ) {
        restore(ctx->lo);
    }
#endif
}

void destroyitp(void)
{
    dumpoper(ctx, 1); // is this pointer value constant?
    printf("bye!\n"); fflush(NULL);
    collect(itpdata->ctab->gl, true, true);
    exititp(itpdata);
    free(itpdata);
}

//#endif
