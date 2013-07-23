#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "m.h"
#include "ob.h"
#include "s.h"
#include "itp.h" /* context itp MAXCONTEXT MAXMFILE */
#include "st.h"
#include "ar.h"
#include "gc.h"
#include "v.h"
#include "nm.h"
#include "di.h"
#include "f.h"
#include "op.h"
#include "optok.h"

itp *itpdata;

#if 0
/* allocate a stack as a "special entry",
   and double-check that it's the right entry */
void makestack(mfile *mem, unsigned stk) {
    unsigned ent;
    mtab *tab;
    ent = mtalloc(mem, 0, 0); /* allocate an entry of zero length */
    assert(ent == stk);
    tab = (void *)mem->base;
    tab->tab[ent].adr = initstack(mem);
}
#endif

unsigned makestack(mfile *mem){
    return initstack(mem);
}

void initctxlist(mfile *mem) {
    unsigned ent;
    mtab *tab;
    ent = mtalloc(mem, 0, MAXCONTEXT * sizeof(unsigned));
    assert(ent == CTXLIST);
    tab = (void *)mem->base;
    memset(mem->base + tab->tab[CTXLIST].adr, 0,
            MAXCONTEXT * sizeof(unsigned));
}

void addtoctxlist(mfile *mem, unsigned cid) {
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
    error("ctxlist full");
}

mfile *nextgtab() {
    int i;
    for (i=0; i < MAXMFILE; i++) {
        if (itpdata->gtab[i].base == NULL) {
            return &itpdata->gtab[i];
        }
    }
    error("cannot allocate mfile, gtab exhausted"), exit(EXIT_FAILURE);
}

/* set up global vm in the context */
void initglobal(context *ctx) {
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
    //ctx->gl->roots[0] = VS;

    initnames(ctx); /* NAMES NAMET */
    //ctx->gl->roots[1] = NAMES;
    initoptab(ctx);
    ctx->gl->start = OPTAB + 1; /* so OPTAB is not collected and not scanned. */
    (void)consname(ctx, "maxlength"); /* seed the tree with a word from the middle of the alphabet */
    (void)consname(ctx, "getinterval"); /* middle of the start */
    (void)consname(ctx, "setmiterlimit"); /* middle of the end */
    initop(ctx);
}

mfile *nextltab() {
    int i;
    for (i=0; i < MAXMFILE; i++) {
        if (itpdata->ltab[i].base == NULL) {
            return &itpdata->ltab[i];
        }
    }
    error("cannot allocate mfile, ltab exhausted"), exit(EXIT_FAILURE);
}

/* set up local vm in the context */
void initlocal(context *ctx) {
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
    ctx->lo->start = CTXLIST + 1;
}


unsigned nextid = 0;
unsigned initctxid(void) {
    unsigned startid = nextid;
    while ( ctxcid(++nextid)->state != 0 ) {
        if (nextid == startid + MAXCONTEXT)
            error("ctab full. cannot create new process");
    }
    return nextid;
}

context *ctxcid(unsigned cid) {
    //TODO reject cid 0
    return &itpdata->ctab[ (cid-1) % MAXCONTEXT ];
}


/* initialize context */
void initcontext(context *ctx) {
    ctx->id = initctxid();
    initlocal(ctx);
    initglobal(ctx);
    ctx->vmmode = LOCAL;
}

/* destroy context */
void exitcontext(context *ctx) {
    exitmem(ctx->gl);
    exitmem(ctx->lo);
}

/*
   fork new process with private global and private local vm
   (spawn jobserver)
   */
unsigned fork1(context *ctx) {
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
unsigned fork2(context *ctx) {
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
unsigned fork3(context *ctx) {
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
void inititp(itp *itp){
    initcontext(&itp->ctab[0]);
    itp->cid = itp->ctab[0].id;
}

/* destroy itp */
void exititp(itp *itp){
    exitcontext(&itp->ctab[0]);
}


/* return the global or local memory file for the composite object */
/*@dependent@*/
mfile *bank(context *ctx, object o) {
    return o.tag&FBANK? ctx->gl : ctx->lo;
}


/* function type for interpreter action pointers */
typedef void evalfunc(context *ctx);

/* quit the interpreter */
void evalquit(context *ctx) { ++ctx->quit; }

/* pop the execution stack */
void evalpop(context *ctx) { (void)pop(ctx->lo, ctx->es); }

/* pop the execution stack onto the operand stack */
void evalpush(context *ctx) {
    push(ctx->lo, ctx->os,
            pop(ctx->lo, ctx->es) );
}

/* load executable name */
void evalload(context *ctx) {
    object s = strname(ctx, top(ctx->lo, ctx->es, 0));
    printf("<name \"%*s\">", s.comp_.sz, charstr(ctx, s));

    push(ctx->lo, ctx->os,
            pop(ctx->lo, ctx->es));
    opexec(ctx, consoper(ctx, "load", NULL,0,0).mark_.padw);
    if (isx(top(ctx->lo, ctx->os, 0))) {
        push(ctx->lo, ctx->es,
                pop(ctx->lo, ctx->os));
    }
}

void evaloperator(context *ctx) {
    object op = pop(ctx->lo, ctx->es);
    dumpoper(ctx, op.mark_.padw);
    opexec(ctx, op.mark_.padw);
}

void evalarray(context *ctx) {
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

void evalstring(context *ctx) {
    object b,t,s;
    s = pop(ctx->lo, ctx->es);
    push(ctx->lo, ctx->os, s);
    //push(ctx->lo, ctx->es, consoper(ctx, "if", NULL,0,0));
    //push(ctx->lo, ctx->es, consoper(ctx, "cvx", NULL,0,0));
    //push(ctx->lo, ctx->es, cvlit(arrstrhandler));
    //push(ctx->lo, ctx->es, consname(ctx, "token"));
    opexec(ctx, consoper(ctx, "token",NULL,0,0).mark_.padw);
    b = pop(ctx->lo, ctx->os);
    if (b.int_.val) {
        t = pop(ctx->lo, ctx->os);
        s = pop(ctx->lo, ctx->os);
        push(ctx->lo, ctx->es, s);
        push(ctx->lo, type(t)==arraytype? ctx->os: ctx->es, t);
    }
}

void evalfile(context *ctx) {
    object b,f,t;
    f = pop(ctx->lo, ctx->es);
    push(ctx->lo, ctx->os, f);
    //if (filestatus(ctx->lo, f)) {
        //object fs;
        //object str;
        //long fz;
        //push(ctx->lo, ctx->es, f);
        //fs = consfile(ctx->lo, statementedit(filefile(ctx->lo, f)));
        //fz = filebytesavailable(ctx->lo, fs);
        //str = consbst(ctx, fz, NULL);
        //fread(charstr(ctx, str), 1, fz, filefile(ctx->lo, fs));
        //push(ctx->lo, ctx->es, cvx(str));
    //}
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

evalfunc *evalcontext = evalpush;
//evalfunc *evalfile = evalpush;
evalfunc *evalname = evalload;

#if 0
//This way doesn't work, since function pointers are non-constant:
#define AS_EVALFUNC(_) eval ## _ ,
evalfunc *evaltype[] = { TYPES(AS_EVALFUNC) };
//So we have to initialize at runtime:
#endif

evalfunc *evaltype[NTYPES + 1];
#define AS_EVALINIT(_) evaltype[ _ ## type ] = eval ## _ ;
void initevaltype(void) {
    TYPES(AS_EVALINIT)
}

void eval(context *ctx) {
    object t = top(ctx->lo, ctx->es, 0);
    printf("\neval\n");
    printf("Stack: ");
    dumpstack(ctx->lo, ctx->os);
    printf("\n");
    printf("Exec Stack: ");
    dumpstack(ctx->lo, ctx->es);
    printf("\n");
    printf("Executing: ");
    dumpobject(t);
    printf("\n");
    if ( isx(t) ) /* if executable */
        evaltype[type(t)](ctx);
    else
        evalpush(ctx);
}

void mainloop(context *ctx) {
    while(!ctx->quit)
        eval(ctx);
}


#ifdef TESTMODULE

context *ctx;
#define CNT_STR(s) sizeof(s)-1, s

void init(void) {
    pgsz = getpagesize();
    initevaltype();
    //ctx = malloc(sizeof *ctx);
    //memset(ctx, 0, sizeof ctx);
    //initcontext(ctx);
    itpdata = malloc(sizeof*itpdata);
    if (!itpdata) error("itpdata=malloc failed");
    memset(itpdata, 0, sizeof*itpdata);
    inititp(itpdata);
    ctx = &itpdata->ctab[0];
}

void xit() {
    //exitcontext(ctx);
    exititp(itpdata);
}


int main(void) {
    printf("\n^test itp.c\n");

    init();
    dumpdic(ctx->gl, bot(ctx->lo, ctx->ds, 0));

    //push(ctx->lo, ctx->es, invalid);
    push(ctx->lo, ctx->es, consoper(ctx, "quit", NULL,0,0));

#if 0
    int i;
    for (i = 8; i >= 0; i--)
        push(ctx->lo, ctx->os, consint(i));

    push(ctx->lo, ctx->os, consint(9));
    push(ctx->lo, ctx->os, consint(-4));
    dumpstack(ctx->lo, ctx->os); puts("");

    push(ctx->lo, ctx->es, consoper(ctx,"roll",NULL,0,0));
#endif

    //push(ctx->lo, ctx->os, consbst(ctx, CNT_STR(" 127 ")));
    //push(ctx->lo, ctx->os, consbst(ctx, CNT_STR(" 16#FF ")));
    //push(ctx->lo, ctx->os, consbst(ctx, CNT_STR(" 8#377 ")));
    //push(ctx->lo, ctx->os, consbst(ctx, CNT_STR(" -.48 ")));
    //push(ctx->lo, ctx->os, consbst(ctx, CNT_STR(" is-a-name ")));
    //push(ctx->lo, ctx->os, consbst(ctx, CNT_STR(" /litname ")));
    //push(ctx->lo, ctx->os, consbst(ctx, CNT_STR(" (s(f)g) ")));
    //push(ctx->lo, ctx->os, consbst(ctx, CNT_STR(" (\\() ")));
    //push(ctx->lo, ctx->os, consbst(ctx, CNT_STR(" (--\\(--\\(--\\(--\\(--) ")));
    //push(ctx->lo, ctx->os, consbst(ctx, CNT_STR(" <68 65 6c 6c 6f 77 6f 72 6c 64> ")));
    //push(ctx->lo, ctx->os, consbst(ctx, CNT_STR(" << ")));
    //push(ctx->lo, ctx->os, consbst(ctx, CNT_STR(" >> ")));
    //push(ctx->lo, ctx->os, consbst(ctx, CNT_STR(" {} ")));
    //push(ctx->lo, ctx->os, consbst(ctx, CNT_STR(" {1 2 3.14 true} ")));
    //push(ctx->lo, ctx->os, consbst(ctx, CNT_STR(" //false ")));
    //push(ctx->lo, ctx->es, cvx(consbst(ctx, CNT_STR(" 1 2 add 3 mul 4 div "))));
    push(ctx->lo, ctx->es, cvx(consbst(ctx,
                    CNT_STR(" (init.ps) (r) file cvx exec (%stdin) run quit "))));

    //push(ctx->lo, ctx->os, cvx(consname(ctx,"token")));
    //dumpobject(top(ctx->lo, ctx->os, 0));
    //dumpstack(ctx->gl, adrent(ctx->gl, NAMES));
    //dumpdic(ctx->gl, top(ctx->lo, ctx->ds, 0));
    fflush(NULL);
    //push(ctx->lo, ctx->es, consname(ctx, "load"));
    //dumpoper(ctx, 12);

    printf("ctx->lo:\n");
    dumpmfile(ctx->lo);
    dumpmtab(ctx->lo, 0);

    ctx->quit = 0;
    mainloop(ctx);

    dumpstack(ctx->lo, ctx->os); puts("");

    printf("ctx->lo:\n");
    dumpmfile(ctx->lo);
    dumpmtab(ctx->lo, 0);
    printf("ctx->gl:\n");
    dumpmfile(ctx->gl);
    dumpmtab(ctx->gl, 0);
    dumpstack(ctx->gl, adrent(ctx->gl, NAMES));


    xit();
    return 0;
}

#endif
