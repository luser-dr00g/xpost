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

#include "m.h"
#include "ob.h"
#include "s.h"
#include "itp.h" /* context itp MAXCONTEXT MAXMFILE */
#include "err.h"
#include "st.h"
#include "ar.h"
#include "gc.h"
#include "v.h"
#include "nm.h"
#include "di.h"
#include "f.h"
#include "op.h"
#include "optok.h"
#include "opdi.h"

int TRACE = 0;
itp *itpdata;
int initializing = 0;

static
unsigned makestack(mfile *mem);

static
unsigned makestack(mfile *mem)
{
    return initstack(mem);
}

void initctxlist(mfile *mem)
{
    unsigned ent;
    mtab *tab;
    ent = mtalloc(mem, 0, MAXCONTEXT * sizeof(unsigned));
    assert(ent == CTXLIST);
    tab = (void *)mem->base;
    memset(mem->base + tab->tab[CTXLIST].adr, 0,
            MAXCONTEXT * sizeof(unsigned));
}

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

static
mfile *nextgtab()
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

/* set up global vm in the context */
static
void initglobal(context *ctx)
{
    ctx->vmmode = GLOBAL;

    /* allocate and initialize global vm */
    //ctx->gl = malloc(sizeof(mfile));
    //ctx->gl = &itpdata->gtab[0];
    ctx->gl = nextgtab();

    initmem(ctx->gl, "g.mem");
    (void)initmtab(ctx->gl);
    initfree(ctx->gl);
    initsave(ctx->gl);
    initctxlist(ctx->gl);
    addtoctxlist(ctx->gl, ctx->id);

    ctx->gl->start = OPTAB + 1; /* so OPTAB is not collected and not scanned. */
}

static
mfile *nextltab()
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

/* set up local vm in the context */
static
void initlocal(context *ctx)
{
    ctx->vmmode = LOCAL;

    /* allocate and initialize local vm */
    //ctx->lo = malloc(sizeof(mfile));
    //ctx->lo = &itpdata->ltab[0];
    ctx->lo = nextltab();

    initmem(ctx->lo, "l.mem");
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


static
unsigned nextid = 0;
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

context *ctxcid(unsigned cid)
{
    //TODO reject cid 0
    return &itpdata->ctab[ (cid-1) % MAXCONTEXT ];
}


/* initialize context */
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
        bdcput(ctx, bot(ctx->lo, ctx->ds, 0), consname(ctx, "userdict"), ud);
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
void inititp(itp *itp)
{
    initcontext(&itp->ctab[0]);
    itp->cid = itp->ctab[0].id;
}

/* destroy itp */
void exititp(itp *itp)
{
    exitcontext(&itp->ctab[0]);
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
    opexec(ctx, consoper(ctx, "load", NULL,0,0).mark_.padw);
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
    opexec(ctx, consoper(ctx, "token",NULL,0,0).mark_.padw);
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
    opexec(ctx, consoper(ctx, "token",NULL,0,0).mark_.padw);
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


void dumpctx(context *ctx)
{
    dumpmfile(ctx->gl);
    dumpmtab(ctx->gl, 0);
    dumpmfile(ctx->lo);
    dumpmtab(ctx->lo, 0);
    dumpnames(ctx);
}

#ifdef TESTMODULE_ITP

context *ctx;
#define CNT_STR(s) sizeof(s)-1, s

void init(void)
{
    pgsz = getpagesize();
    initializing = 1;
    initevaltype();

    null = cvlit(null);
    itpdata = malloc(sizeof*itpdata);
    if (!itpdata) error(unregistered, "itpdata=malloc failed");
    memset(itpdata, 0, sizeof*itpdata);
    inititp(itpdata);
    ctx = &itpdata->ctab[0];
}

void xit()
{
    exititp(itpdata);
    exit(0);
}


int main(void) {
    object sd;
    printf("\n^test itp.c\n");

    //TRACE=1;
    init();
    sd = bot(ctx->lo, ctx->ds, 0);
    bdcput(ctx, sd, consname(ctx, "PACKAGE_DATA_DIR"),
            cvlit(consbst(ctx, CNT_STR(PACKAGE_DATA_DIR))));

    //dumpoper(ctx, 19);
    //dumpctx(ctx);   /* double-check pre-initialized memory */
    //xit();

    /* load init.ps and err.ps */
    assert(ctx->gl->base);
    push(ctx->lo, ctx->es, consoper(ctx, "quit", NULL,0,0));
    push(ctx->lo, ctx->es,
        cvx(consbst(ctx,
            CNT_STR("(" PACKAGE_DATA_DIR "/init.ps) (r) file cvx exec"))));
    ctx->quit = 0;
    mainloop(ctx);

    /* Run! */
    push(ctx->lo, ctx->es, consoper(ctx, "quit", NULL,0,0)); 
    push(ctx->lo, ctx->es, cvx(consname(ctx, "start"))); /* `start` proc defined in init.ps */
    initializing = 0;
    ctx->quit = 0;
    mainloop(ctx);

    dumpoper(ctx, 1); // is this pointer value constant?

    printf("bye!\n"); fflush(NULL);
    xit();
    return 0;
}

#endif
