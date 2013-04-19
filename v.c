#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include "m.h"
#include "ob.h"
#include "s.h"
#include "itp.h"
#include "gc.h"

/*
typedef struct {
	word tag;
	word lev;
	unsigned stk;
} save_;

typedef struct {
	unsigned src;
	unsigned cpy;
} saverec_;
*/

/* create a stack in slot VS.
   sz is 0 so gc will ignore it. */
void initsave(mfile *mem) {
	unsigned t;
	unsigned ent = mtalloc(mem, 0, 0); /* allocate an entry of zero length */
	assert(ent == VS);
	t = initstack(mem);
	mtab *tab = (void *)mem->base;
	tab->tab[ent].adr = t;
}

/* push a new save object on the save stack
   this object is itself a stack (contains a stackadr) */
object save(mfile *mem) {
	object v;
	v.tag = savetype;
	v.save_.lev = count(mem, adrent(mem, VS));
	v.save_.stk = initstack(mem);
	push(mem, adrent(mem, VS), v);
	return v;
}

/* check ent's tlev against current save level (save-stack count) */
unsigned stashed(mfile *mem, unsigned ent) {
	//object sav = top(mem, adrent(mem, VS), 0);
	unsigned cnt = count(mem, adrent(mem, VS));
	mtab *tab;
	findtabent(mem, &tab, &ent);
	unsigned tlev = (tab->tab[ent].mark & TLEVM) >> TLEVO;
	return tlev == cnt;
}

/* make a clone of ent, return new ent */
unsigned copy(mfile *mem, unsigned ent) {
	mtab *tab;
	unsigned tent = ent;
	findtabent(mem, &tab, &ent);
	unsigned new = gballoc(mem, tab->tab[ent].sz);
	ent = tent;
	findtabent(mem, &tab, &ent); //recalc
	memcpy(mem->base + adrent(mem, new),
			mem->base + tab->tab[ent].adr,
			tab->tab[ent].sz);
	return new;
}

/* set tlev for ent to current save level
   push saverec relating ent to saved copy */
void stash(mfile *mem, unsigned ent) {
	object sav = top(mem, adrent(mem, VS), 0);
	mtab *tab;
	unsigned rent = ent;
	findtabent(mem, &tab, &rent);
	unsigned tlev = sav.save_.lev;
	tab->tab[rent].mark &= ~TLEVM; // clear TLEV field
	tab->tab[rent].mark |= (tlev << TLEVO);  // set TLEV field
	object o;
	o.saverec_.src = ent;
	o.saverec_.cpy = copy(mem, ent);
	push(mem, sav.save_.stk, o);
}

/* for each saverec from current save stack
   		exchange adrs between src and cpy
		pop saverec
	pop save stack */
void restore(mfile *mem) {
	unsigned v = adrent(mem, VS); // save-stack address
	object sav = pop(mem, v); // save-object (stack of saverec_'s)
	mtab *stab, *ctab;
	unsigned cnt = count(mem, sav.save_.stk);
	unsigned sent, cent;
	while (cnt--) {
		object rec = pop(mem, sav.save_.stk);
		sent = rec.saverec_.src;
		cent = rec.saverec_.cpy;
		findtabent(mem, &stab, &sent);
		findtabent(mem, &ctab, &cent);
		unsigned hold;
		hold = stab->tab[sent].adr;                 // tmp = src
		stab->tab[sent].adr = ctab->tab[cent].adr;  // src = cpy
		ctab->tab[cent].adr = hold;                 // cpy = tmp
	}
	sfree(mem, sav.save_.stk);
}

#ifdef TESTMODULE
#include "ar.h"
#include <stdio.h>

mfile mf;

void init(mfile *mem) {
	initmem(mem, "x.mem");
	(void)initmtab(mem);
	initfree(mem);
	initsave(mem);
}

void show(char *msg, mfile *mem, object a) {
	printf("%s ", msg);
	printf("%d ", arrget(mem, a, 0).int_.val);
	printf("%d\n", arrget(mem, a, 1).int_.val);
}

int main(void) {
	mfile *mem = &mf;
	init(mem);

	object a = consarr(mem, 2);
	arrput(mem, a, 0, consint(33));
	arrput(mem, a, 1, consint(66));
	show("initial", mem, a);

	//object v = 
	(void)save(mem);
	arrput(mem, a, 0, consint(77));
	show("save and alter", mem, a);

	restore(mem);
	show("restored", mem, a);

	return 0;
}

#endif


