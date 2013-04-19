#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
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

/* allocate a stack as a "special entry",
   and double-check that it's the right entry */
void makestack(mfile *mem, unsigned stk) {
	unsigned ent = mtalloc(mem, 0, 0); /* allocate an entry of zero length */
	assert(ent == stk);
	mtab *tab = (void *)mem->base;
	tab->tab[ent].adr = initstack(mem);
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
	ctx->gl->roots[0] = VS;

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
	ctx->lo->roots[0] = VS;

	makestack(ctx->lo, OS);
	makestack(ctx->lo, ES);
	makestack(ctx->lo, DS);
	makestack(ctx->lo, HOLD);
	ctx->os = adrent(ctx->lo, OS); /* shortcuts */
	ctx->es = adrent(ctx->lo, ES);
	ctx->ds = adrent(ctx->lo, DS);
	ctx->hold = adrent(ctx->lo, HOLD);
	ctx->lo->roots[1] = DS;
	ctx->lo->start = HOLD + 1; /* so HOLD is not collected and not scanned. */
}

/* initialize context */
void initcontext(context *ctx) {
	initlocal(ctx);
	initglobal(ctx);
	ctx->vmmode = LOCAL;
}

/* destroy context */
void exitcontext(context *ctx) {
	exitmem(ctx->gl);
	exitmem(ctx->lo);
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
	default /* > 1 */: push(ctx->lo, ctx->es,
							   arrgetinterval(a, 1, a.comp_.sz - 1) );
	case 1: push(ctx->lo, ctx->es,
					barget(ctx, a, 0) );
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

context ctx;
#define CNT_STR(s) sizeof(s), s

void init(void) {
	pgsz = getpagesize();
	initevaltype();
	initcontext(&ctx);

	push(ctx.lo, adrent(ctx.lo, ES), invalid); /* schedule a quit */
	push(ctx.lo, ctx.es, consoper(&ctx, "count", NULL,0,0));

	push(ctx.lo, ctx.es, consoper(&ctx, "pop", NULL,0,0));
	push(ctx.lo, ctx.es, consoper(&ctx, "pop", NULL,0,0));
	push(ctx.lo, ctx.es, consoper(&ctx, "pop", NULL,0,0));

	push(ctx.lo, ctx.es, consint(1));
	push(ctx.lo, ctx.es, consoper(&ctx, "dup", NULL,0,0));
	push(ctx.lo, ctx.es, consint(2));
	push(ctx.lo, ctx.es, consint(3));

	push(ctx.lo, ctx.es, consoper(&ctx, "exch", NULL,0,0));

	push(ctx.lo, ctx.es, consreal(4.0));
	push(ctx.lo, ctx.es, consreal(42.0));

	push(ctx.lo, ctx.es, consname(&ctx, "pop"));
	push(ctx.lo, ctx.es, consname(&ctx, "potato"));
	push(ctx.lo, ctx.es, consbool(true));

	push(ctx.lo, ctx.es, consbst(&ctx, CNT_STR("abcdefgh")));
	push(ctx.lo, ctx.es, consbst(&ctx, CNT_STR("abracadavra")));
	push(ctx.lo, ctx.es, consbst(&ctx, CNT_STR("01234567")));

	object a = consbar(&ctx, 2);
		barput(&ctx, a, 0, consint(6));
		barput(&ctx, a, 1, consreal(7.0));
		push(ctx.lo, ctx.es, a);
	object d = consbdc(&ctx, 2);
		bdcput(&ctx, d, consint(0), consint(1));
		push(ctx.lo, ctx.es, d);
	push(ctx.lo, ctx.es, consoper(&ctx, "count", NULL,0,0));
}

void xit() {
	exitcontext(&ctx);
}

int main(void) {
	init();

	printf("\n^test itp.c\n");
	printf("initial es:\n");
	dumpstack(ctx.lo, ctx.es);
	puts("");
	mainloop(&ctx);
	printf("final os:\n");
	dumpstack(ctx.lo, ctx.os);
	return 0;
	printf("ctx.lo:\n");
	dumpmfile(ctx.lo);
	dumpmtab(ctx.lo, 0);
	printf("ctx.gl:\n");
	dumpmfile(ctx.gl);
	dumpmtab(ctx.gl, 0);

	xit();
	return 0;
}

#endif

