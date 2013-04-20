
#include <stdbool.h>

#include "m.h"
#include "ob.h"
#include "s.h"
#include "gc.h"
#include "v.h"
#include "itp.h"

/* Allocate an entity with gballoc,
   find the appropriate mtab,
   set the current save level in the "mark" field,
   wrap it up in an object. */
object consarr(mfile *mem, unsigned sz) {
	unsigned ent;
	unsigned rent;
	unsigned cnt;
	mtab *tab;
	object o;
	//unsigned ent = mtalloc(mem, 0, sz * sizeof(object));
	ent = gballoc(mem, (unsigned)(sz * sizeof(object)));
	tab = (void *)(mem->base);
	rent = ent;
	findtabent(mem, &tab, &rent);
	cnt = count(mem, adrent(mem, VS));
	tab->tab[rent].mark = ( (0 << MARKO) | (0 << RFCTO) |
			(cnt << LLEVO) | (cnt << TLEVO) );

	//return (object){ .comp_.tag = arraytype, .comp_.sz = sz, .comp_.ent = ent, .comp_.off = 0};
	o.tag = arraytype;
	o.comp_.sz = (word)sz;
	o.comp_.ent = (word)ent;
	o.comp_.off = 0;
	return o;
} 

/* Select a memory file according to vmmode,
   call consarr,
   set BANK flag. */
object consbar(context *ctx, unsigned sz) {
	object a = consarr(ctx->vmmode==GLOBAL?
			ctx->gl: ctx->lo, sz);
	if (ctx->vmmode==GLOBAL)
		a.tag |= FBANK;
	return a;
}

/* Copy if necessary,
   call put. */
void arrput(mfile *mem, object a, integer i, object o) {
	if (!stashed(mem, a.comp_.ent)) stash(mem, a.comp_.ent);
	put(mem, a.comp_.ent, (unsigned)(a.comp_.off + i), (unsigned)sizeof(object), &o);
}

/* Select mfile according to BANK flag,
   call arrput. */
void barput(context *ctx, object a, integer i, object o) {
	arrput(a.tag&FBANK? ctx->gl: ctx->lo, a, i, o);
}

/* call get. */
object arrget(mfile *mem, object a, integer i) {
	object o;
	get(mem, a.comp_.ent, (unsigned)(a.comp_.off +i), (unsigned)(sizeof(object)), &o);
	return o;
}

/* Select mfile according to BANK flag,
   call arrget. */
object barget(context *ctx, object a, integer i) {
	return arrget(a.tag&FBANK? ctx->gl: ctx->lo, a, i);
}

/* adjust the offset and size fields in the object. */
object arrgetinterval(object a, integer s, integer n) {
	if (s + n > a.comp_.off + a.comp_.sz)
		error("getinterval can only shrink!");
	a.comp_.off += s;
	a.comp_.sz = n;
	return a;
}

#ifdef TESTMODULE
#include <stdio.h>

mfile mem;

int main(void) {
	initmem(&mem, "x.mem");
	(void)initmtab(&mem);
	initfree(&mem);
	initsave(&mem);

	enum { SIZE = 10 };
	printf("\n^test ar.c\n");
	printf("allocating array occupying %zu bytes\n", SIZE*sizeof(object));
	object a = consarr(&mem, SIZE);

	//printf("the memory table:\n"); dumpmtab(&mem, 0);

	printf("test array by filling\n");
	int i;
	for (i=0; i < SIZE; i++) {
		printf("%d ", i+1);
		arrput(&mem, a, i, consint( i+1 ));
	}
	puts("");

	printf("and accessing.\n");
	for (i=0; i < SIZE; i++) {
		object t;
		t = arrget(&mem, a, i);
		printf("%d: %d\n", i, t.int_.val);
	}

	printf("the memory table:\n");
	dumpmtab(&mem, 0);

	return 0;
}

#endif

