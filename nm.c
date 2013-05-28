#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "m.h"
#include "ob.h"
#include "gc.h"
#include "s.h"
#include "itp.h"
#include "st.h"
#include "nm.h"

void initnames(context *ctx) {
	mtab *tab;
	unsigned ent;
	unsigned t;

	ent = mtalloc(ctx->gl, 0, 0); //NAMES
	assert(ent == NAMES);
	ent = mtalloc(ctx->gl, 0, 0); //NAMET
	assert(ent == NAMET);

	t = initstack(ctx->gl);
	tab = (void *)ctx->gl->base; //recalc pointer
	tab->tab[NAMES].adr = t;
	tab->tab[NAMET].adr = 0;
#define CNT_STR(s) sizeof(s), s
	push(ctx->gl, adrent(ctx->gl, NAMES), consstr(ctx->gl, CNT_STR("_not_a_name_")));
}

unsigned tstsearch(mfile *mem, unsigned tadr, char *s) {
	while (tadr) {
		tst *p = (void *)(mem->base + tadr);
		if ((unsigned)*s < p->val) {
			tadr = p->lo;
		} else if ((unsigned)*s == p->val) {
			if (*s++ == 0) return p->eq; /* payload when val == '\0' */
			tadr = p->eq;
		} else {
			tadr = p->hi;
		}
	}
	return 0;
}

unsigned tstinsert(mfile *mem, unsigned tadr, char *s) {
	tst *p;
	unsigned t; //temporary
	if (!tadr) {
		tadr = mfalloc(mem, sizeof(tst));
		p = (void *)(mem->base + tadr);
		p->val = *s;
		p->lo = p->eq = p->hi = 0;
	}
	p = (void *)(mem->base + tadr);
	if ((unsigned)*s < p->val) {
		t = tstinsert(mem, p->lo, s);
		p = (void *)(mem->base + tadr); //recalc pointer
		p->lo = t;
	} else if ((unsigned)*s == p->val) {
		if (*s) {
			t = tstinsert(mem, p->eq, ++s);
			p = (void *)(mem->base + tadr); //recalc pointer
			p->eq = t;
		}else {
			p->eq = count(mem, adrent(mem, NAMES)); /* payload when val == '\0' */
		}
	} else {
		t = tstinsert(mem, p->hi, s);
		p = (void *)(mem->base + tadr); //recalc pointer
		p->hi = t;
	}
	return tadr;
}

unsigned addname(context *ctx, char *s) {
	unsigned names = adrent(ctx->gl, NAMES);
	unsigned u = count(ctx->gl, names);
	//dumpmfile(ctx->gl);
	//dumpmtab(ctx->gl, 0);
	push(ctx->gl, names, consstr(ctx->gl, strlen(s), s));
	return u;
}

object consname(context *ctx, char *s) {
	unsigned u;
	unsigned t;
	object o;
	u = tstsearch(ctx->gl, adrent(ctx->gl, NAMET), s);
	if (!u) {
		mtab *tab = (void *)ctx->gl->base;
		t = tstinsert(ctx->gl, tab->tab[NAMET].adr, s);
		tab = (void *)ctx->gl->base; //recalc pointer
		tab->tab[NAMET].adr = t;
		u = addname(ctx, s);
	}
	o.tag = nametype;
	o.mark_.padw = u;
	return o;
}

#ifdef TESTMODULE
#include <stdio.h>
#include <unistd.h>

void init(context *ctx) {
	pgsz = getpagesize();
	ctx->gl = malloc(sizeof(mfile));
	initmem(ctx->gl, "x.mem");
	(void)initmtab(ctx->gl); /* create mtab at address zero */
	//(void)mtalloc(ctx->gl, 0, 0); //FREE
	initfree(ctx->gl);
	(void)mtalloc(ctx->gl, 0, 0); //VS
	initctxlist(ctx->gl);

	initnames(ctx);
}

context ctx;

int main(void) {
	printf("\n^test nm\n");
	init(&ctx);

	printf("pop ");
	dumpobject(consname(&ctx, "pop"));
	printf("NAMES at %u\n", adrent(ctx.gl, NAMES));
	dumpstack(ctx.gl, adrent(ctx.gl, NAMES)); puts("");

	printf("apple ");
	dumpobject(consname(&ctx, "apple"));
	dumpobject(consname(&ctx, "apple"));
	//printf("NAMES at %u\n", adrent(ctx.gl, NAMES));
	dumpstack(ctx.gl, adrent(ctx.gl, NAMES)); puts("");

	printf("banana ");
	dumpobject(consname(&ctx, "banana"));
	dumpobject(consname(&ctx, "banana"));
	//printf("NAMES at %u\n", adrent(ctx.gl, NAMES));
	dumpstack(ctx.gl, adrent(ctx.gl, NAMES)); puts("");

	printf("currant ");
	dumpobject(consname(&ctx, "currant"));
	dumpobject(consname(&ctx, "currant"));
	//printf("NAMES at %u\n", adrent(ctx.gl, NAMES));
	dumpstack(ctx.gl, adrent(ctx.gl, NAMES)); puts("");

	printf("apple ");
	dumpobject(consname(&ctx, "apple"));
	printf("banana ");
	dumpobject(consname(&ctx, "banana"));
	printf("currant ");
	dumpobject(consname(&ctx, "currant"));
	printf("date ");
	//printf("NAMES at %u\n", adrent(ctx.gl, NAMES));
	dumpobject(consname(&ctx, "date"));
	//printf("NAMES at %u\n", adrent(ctx.gl, NAMES));
	dumpstack(ctx.gl, adrent(ctx.gl, NAMES)); puts("");
	//printf("NAMES at %u\n", adrent(ctx.gl, NAMES));
	printf("elderberry ");
	dumpobject(consname(&ctx, "elderberry"));

	printf("pop ");
	dumpobject(consname(&ctx, "pop"));

	//dumpmfile(ctx.gl);
	//dumpmtab(ctx.gl, 0);
	puts("");
	return 0;
}

#endif
