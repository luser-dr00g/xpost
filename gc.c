#include <stdbool.h>
#include <string.h>

#include "m.h"
#include "ob.h"
#include "s.h"
#include "itp.h"
#include "ar.h"
#include "st.h"
#include "v.h"

#ifdef TESTMODULE
#include <stdio.h>
#endif

/* iterate through all tables,
   	clear the MARK in the mark. */
void unmark(mfile *mem) {
	mtab *tab = (void *)(mem->base);
	unsigned i;
	while (1) {
		for (i = mem->start; i < tab->nextent; i++) {
			tab->tab[i].mark &= ~MARKM;
		}
		if (i != TABSZ) break;
		tab = (void *)(mem->base + tab->nexttab);
	}
}

/* set the MARK in the mark in the tab[ent] */
void markent(mfile *mem, unsigned ent) {
	mtab *tab = (void *)(mem->base);
	findtabent(mem,&tab,&ent);
	tab->tab[ent].mark |= MARKM;
}

/* is it marked? */
int marked(mfile *mem, unsigned ent) {
	mtab *tab = (void *)(mem->base);
	findtabent(mem,&tab,&ent);
	return (tab->tab[ent].mark & MARKM) >> MARKO;
}

/* recursively mark all elements of array */
void markarray(mfile *mem, unsigned adr, unsigned sz) {
	object *op = (void *)(mem->base + adr);
	unsigned j;
	for (j=0; j < sz; j++) {
#ifdef TESTMODULE
		printf("markarray: %s\n", types[op->tag]);
#endif
		switch(op->tag) {
		case arraytype:
			if (!marked(mem, op->comp_.ent)) {
				markent(mem, op->comp_.ent);
				markarray(mem, adrent(mem, op->comp_.ent), op->comp_.sz);
			}
			break;
		case stringtype:
			markent(mem, op->comp_.ent);
			break;
		}
		++op;
	}
}

/* mark all allocations referred to by objects in stack */
void markstack(mfile *mem, unsigned stackadr) {
	stack *s = (void *)(mem->base + stackadr);
	unsigned i;
#ifdef TESTMODULE
	printf("marking stack of size %u\n", s->top);
#endif
next:
	for (i=0; i < s->top; i++) {
#ifdef TESTMODULE
		printf("markstack: %s\n", types[type(s->data[i])]);
#endif
		switch(s->data[i].tag) {
		case arraytype:
			if (!marked(mem, s->data[i].comp_.ent)) {
				markent(mem, s->data[i].comp_.ent);
				markarray(mem, adrent(mem, s->data[i].comp_.ent), s->data[i].comp_.sz);
			}
			break;
		case stringtype:
			markent(mem, s->data[i].comp_.ent);
			break;
		}
	}
	if (i==STACKSEGSZ) { /* ie. s->top == STACKSEGSZ */
		s = (void *)(mem->base + s->nextseg);
		goto next;
	}
}

/* free list head is in slow zero
   sz is 0 so gc will ignore it */
void initfree(mfile *mem) {
	(void)mtalloc(mem, 0, sizeof(unsigned));
	/*
	   unsigned ent = mtalloc(mem, 0, 0);
	   mtab *tab = (void *)mem->base;
	   tab->tab[ent].adr = mfalloc(mem, sizeof(unsigned));
   */
}


/* free this ent! */
void mfree(mfile *mem, unsigned ent) {
	unsigned a = adrent(mem, ent);
	if (szent(mem, ent) == 0) return; // ignore zero size allocs
	unsigned z = adrent(mem, FREE);
	//*(unsigned *)(mem->base + adrent(mem, ent)) = mem->avail;
	memcpy(mem->base+a, mem->base+z, sizeof(unsigned));
	//mem->avail = ent;
	memcpy(mem->base+z, &ent, sizeof(ent));
}

/* discard the free list.
   iterate through tables,
   		if element is unmarked and not zero-sized,
			free it.  */
void sweep(mfile *mem) {
	unsigned z = adrent(mem, FREE);
	memcpy(mem->base+z, &(unsigned){ 0 }, sizeof(unsigned));
	mtab *tab = (void *)(mem->base);
	int ntab = 0;
	unsigned i;
	while (1) {
		for (i = mem->start; i < tab->nextent; i++) {
			if (tab->tab[i].mark == 0
			&& tab->tab[i].sz != 0)
				mfree(mem, i + ntab*TABSZ);
		}
		if (i!=TABSZ) break;
		tab = (void *)(mem->base + tab->nexttab);
		++ntab;
	}
}

/* clear all marks,
   mark all root stacks,
   sweep. */
void collect(mfile *mem) {
	unmark(mem);
	unsigned i;
	for (i = mem->roots[0]; i<= mem->roots[1]; i++) {
		markstack(mem, adrent(mem, i));
	}
	sweep(mem);
}

enum { PERIOD = 10 };

/* scan the free list for a suitably sized bit of memory,
   if the allocator falls back to fresh memory 10 times,
   		it triggers a collection. */
unsigned gballoc(mfile *mem, unsigned sz) {
	unsigned z = adrent(mem, FREE);
	unsigned e;
	static int period = PERIOD;
	memcpy(&e, mem->base+z, sizeof(e));
try_again:
	while (e) {
		if (szent(mem,e) >= sz) {
			memcpy(mem->base+z, mem->base+adrent(mem,e), sizeof(unsigned));
			return e;
		}
		z = adrent(mem,e);
		memcpy(&e, mem->base+z, sizeof(e));
	}
	if (--period == 0) {
		period = PERIOD;
		collect(mem);
		goto try_again;
	}
	return mtalloc(mem, 0, sz);
}

/* allocate new entry, copy data, steal its adr, stash old adr, free it */
unsigned mfrealloc(mfile *mem, unsigned oldadr, unsigned oldsize, unsigned newsize) {
	mtab *tab = NULL;
	unsigned ent = mtalloc(mem, 0, newsize);
	findtabent(mem, &tab, &ent);
	unsigned newadr = tab->tab[ent].adr;
	memcpy(mem->base + newadr, mem->base + oldadr, oldsize);
	tab->tab[ent].adr = oldadr;
	tab->tab[ent].sz = oldsize;
	mfree(mem, ent);
	return newadr;
}

#ifdef TESTMODULE

mfile mem;
unsigned stac;

void init(void) {
	initmem(&mem);
	(void)initmtab(&mem);
	initfree(&mem);
	initsave(&mem);
	mtab *tab = (void *)mem.base;
	unsigned ent = mtalloc(&mem, 0, 0);
	//findtabent(&mem, &tab, &ent);
	stac = tab->tab[ent].adr = initstack(&mem);
	mem.roots[0] = VS;
	mem.roots[1] = ent;
	mem.start = ent+1;
}

int main(void) {
	init();
	printf("\n^test gc.c\n");

	push(&mem, stac, consint(5));
	push(&mem, stac, consint(6));
	push(&mem, stac, consreal(7.0));
	object ar;
	ar = consarr(&mem, 3);
	int i;
	for (i=0; i < 3; i++)
		arrput(&mem, ar, i, pop(&mem, stac));
	push(&mem, stac, ar);                   /* array on stack */

	push(&mem, stac, consint(1));
	push(&mem, stac, consint(2));
	push(&mem, stac, consint(3));
	ar = consarr(&mem, 3);
	for (i=0; i < 3; i++)
		arrput(&mem, ar, i, pop(&mem, stac));
	dumpobject(ar);
	/* array not on stack */

#define CNT_STR(x) sizeof(x), x
	push(&mem, stac, consstr(&mem, CNT_STR("string on stack")));

	dumpobject(consstr(&mem, CNT_STR("string not on stack")));

	collect(&mem);
	push(&mem, stac, consstr(&mem, CNT_STR("string on stack")));
	dumpobject(consstr(&mem, CNT_STR("string not on stack")));

	collect(&mem);
	dumpmfile(&mem);
	printf("stackaedr: %04x\n", stac);
	dumpmtab(&mem, 0);
	
	return 0;
}

#endif
