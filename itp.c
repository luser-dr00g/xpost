#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "m.h"
#include "ob.h"
#include "s.h"
#include "itp.h"
#include "st.h"
#include "ar.h"
#include "gc.h"
#include "v.h"
#include "nm.h"
#include "di.h"
//#include "f.h"
#include "op.h"

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

/* set up global vm in the context */
void initglobal(context *ctx) {
	ctx->vmmode = GLOBAL;

	/* allocate and initialize global vm */
	ctx->gl = malloc(sizeof(mfile));
	initmem(ctx->gl, "g.mem");
	(void)initmtab(ctx->gl);
	initfree(ctx->gl);
	initsave(ctx->gl);
	initctxlist(ctx->gl);
	addtoctxlist(ctx->gl, ctx->id);
	//ctx->gl->roots[0] = VS;

	initnames(ctx); /* NAMES NAMET */
	ctx->gl->roots[1] = NAMES;
	initoptab(ctx);
	ctx->gl->start = OPTAB + 1; /* so OPTAB is not collected and not scanned. */
	(void)consname(ctx, "maxlength"); /* seed the tree with a word from the middle of the alphabet */
	(void)consname(ctx, "getinterval"); /* middle of the start */
	(void)consname(ctx, "setmiterlimit"); /* middle of the end */
	initop(ctx);
}

/* set up local vm in the context */
void initlocal(context *ctx) {
	ctx->vmmode = LOCAL;

	/* allocate and initialize local vm */
	ctx->lo = malloc(sizeof(mfile));
	initmem(ctx->lo, "l.mem");
	(void)initmtab(ctx->lo);
	initfree(ctx->lo);
	initsave(ctx->lo);
	initctxlist(ctx->lo);
	addtoctxlist(ctx->lo, ctx->id);
	ctx->lo->roots[0] = VS;

	ctx->os = makestack(ctx->lo);
	ctx->es = makestack(ctx->lo);
	ctx->ds = makestack(ctx->lo);
	ctx->hold = makestack(ctx->lo);
	//ctx->lo->roots[1] = DS;
	//ctx->lo->start = HOLD + 1; /* so HOLD is not collected and not scanned. */
}

unsigned nextid = 0;
unsigned initctxid(void) {
	return ++nextid;
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

/* initialize itp */
void inititp(itp *itp){
	initcontext(&itp->ctab[0]);
}

/* destroy itp */
void exititp(itp *itp){
}

/* return the global or local memory file for the composite object */
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

/* interpreter actions for executable types */
evalfunc *evalinvalid = evalquit;
evalfunc *evalmark = evalpop;
evalfunc *evalnull = evalpop;
evalfunc *evalinteger = evalpush;
evalfunc *evalboolean = evalpush;
evalfunc *evalreal = evalpush;
evalfunc *evalsave = evalpush;
evalfunc *evaldict = evalpush;

evalfunc *evalcontext = evalpush;
evalfunc *evalfile = evalpush;
evalfunc *evalstring = evalpush;
evalfunc *evalname = evalpush;

void evaloperator(context *ctx) {
	object op = pop(ctx->lo, ctx->es);
	opexec(ctx, op.mark_.padw);
}

void evalarray(context *ctx) {
	object a = pop(ctx->lo, ctx->es);
	switch (a.comp_.sz) {
	default /* > 1 */:
		push(ctx->lo, ctx->es, arrgetinterval(a, 1, a.comp_.sz - 1) );
    /*@fallthrough@*/
	case 1:
		push(ctx->lo, ctx->es, barget(ctx, a, 0) );
	/*@fallthrough@*/
	case 0: /* drop */;
	}
}

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
#define CNT_STR(s) sizeof(s), s

void init(void) {
	pgsz = getpagesize();
	initevaltype();
	ctx = malloc(sizeof *ctx);
	memset(ctx, 0, sizeof ctx);
	initcontext(ctx);
}

void xit() {
	exitcontext(ctx);
}

int main(void) {
	init();

	printf("\n^test itp.c\n");

	push(ctx->lo, ctx->es, invalid);

	int i;
	for (i = 8; i >= 0; i--)
		push(ctx->lo, ctx->os, consint(i));

	push(ctx->lo, ctx->os, consint(9));
	push(ctx->lo, ctx->os, consint(-4));
	dumpstack(ctx->lo, ctx->os); puts("");

	push(ctx->lo, ctx->es, consoper(ctx,"roll",NULL,0,0));

	ctx->quit = 0;
	mainloop(ctx);

	dumpstack(ctx->lo, ctx->os); puts("");

	printf("ctx->lo:\n");
	dumpmfile(ctx->lo);
	dumpmtab(ctx->lo, 0);
	printf("ctx->gl:\n");
	dumpmfile(ctx->gl);
	dumpmtab(ctx->gl, 0);


	xit();
	return 0;
}

#endif
