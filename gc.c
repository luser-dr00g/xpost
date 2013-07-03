#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "m.h"
#include "ob.h"
#include "s.h"
#include "itp.h"
#include "ar.h"
#include "st.h"
#include "di.h"
#include "v.h"

#ifdef TESTMODULE
#include <stdio.h>
#endif

/* iterate through all tables,
    clear the MARK in the mark. */
void unmark(mfile *mem) {
    mtab *tab = (void *)(mem->base);
    unsigned i;
    for (i = mem->start; i < tab->nextent; i++) {
        tab->tab[i].mark &= ~MARKM;
    }
    while (tab->nexttab != 0) {
        tab = (void *)(mem->base + tab->nexttab);
        for (i = 0; i < tab->nextent; i++) {
            tab->tab[i].mark &= ~MARKM;
        }
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

void markobject(context *ctx, mfile *mem, object o);

void markdict(context *ctx, mfile *mem, unsigned adr) {
    dichead *dp = (void *)(mem->base + adr);
    object *tp = (void *)(mem->base + adr + sizeof(dichead));
    unsigned j;
    for (j=0; j < DICTABN(dp->sz); j++) {
        markobject(ctx, mem, tp[j]);
    }
}

/* recursively mark all elements of array */
void markarray(context *ctx, mfile *mem, unsigned adr, unsigned sz) {
    object *op = (void *)(mem->base + adr);
    unsigned j;
    for (j=0; j < sz; j++) {
        markobject(ctx, mem, op[j]);
    }
}

/* traverse the contents of composite objects */
void markobject(context *ctx, mfile *mem, object o) {
    switch(type(o)) {
    case arraytype:
#ifdef TESTMODULE
    printf("markobject: %s %d\n", types[type(o)], o.comp_.sz);
#endif
        if (bank(ctx, o) != mem) break;
        if (!marked(mem, o.comp_.ent)) {
            markent(mem, o.comp_.ent);
            markarray(ctx, mem, adrent(mem, o.comp_.ent), o.comp_.sz);
        }
        break;
    case dicttype:
#ifdef TESTMODULE
    printf("markobject: %s %d\n", types[type(o)], o.comp_.sz);
#endif
        if (bank(ctx, o) != mem) break;
        if (!marked(mem, o.comp_.ent)) {
            markent(mem, o.comp_.ent);
            markdict(ctx, mem, adrent(mem, o.comp_.ent));
        }
        break;
    case stringtype:
#ifdef TESTMODULE
    printf("markobject: %s %d\n", types[type(o)], o.comp_.sz);
#endif
        if (bank(ctx, o) != mem) break;
        markent(mem, o.comp_.ent);
        break;
    }
}

/* mark all allocations referred to by objects in stack */
void markstack(context *ctx, mfile *mem, unsigned stackadr) {
    stack *s = (void *)(mem->base + stackadr);
    unsigned i;
#ifdef TESTMODULE
    printf("marking stack of size %u\n", s->top);
#endif
next:
    for (i=0; i < s->top; i++) {
        markobject(ctx, mem, s->data[i]);
    }
    if (i==STACKSEGSZ) { /* ie. s->top == STACKSEGSZ */
        s = (void *)(mem->base + s->nextseg);
        goto next;
    }
}

/* mark all allocations referred to by objects in save object's stack of saverec_'s */
void marksavestack(context *ctx, mfile *mem, unsigned stackadr) {
    stack *s = (void *)(mem->base + stackadr);
    unsigned i;
#ifdef TESTMODULE
    printf("marking save stack of size %u\n", s->top);
#endif
next:
    for (i=0; i < s->top; i++) {
        //markobject(ctx, mem, s->data[i]);
        //marksavestack(ctx, mem, s->data[i].save_.stk);
        markent(mem, s->data[i].saverec_.src);
        markent(mem, s->data[i].saverec_.cpy);
    }
    if (i==STACKSEGSZ) { /* ie. s->top == STACKSEGSZ */
        s = (void *)(mem->base + s->nextseg);
        goto next;
    }
}

/* mark all allocations referred to by objects in save stack */
void marksave(context *ctx, mfile *mem, unsigned stackadr) {
    stack *s = (void *)(mem->base + stackadr);
    unsigned i;
#ifdef TESTMODULE
    printf("marking save stack of size %u\n", s->top);
#endif
next:
    for (i=0; i < s->top; i++) {
        //markobject(ctx, mem, s->data[i]);
        marksavestack(ctx, mem, s->data[i].save_.stk);
    }
    if (i==STACKSEGSZ) { /* ie. s->top == STACKSEGSZ */
        s = (void *)(mem->base + s->nextseg);
        goto next;
    }
}

/* free list head is in slot zero
   sz is 0 so gc will ignore it */
void initfree(mfile *mem) {
    unsigned ent = mtalloc(mem, 0, sizeof(unsigned));
    unsigned val = 0;
    assert (ent == FREE);
    put(mem, ent, 0, sizeof(unsigned), &val);
    
    /*
       unsigned ent = mtalloc(mem, 0, 0);
       mtab *tab = (void *)mem->base;
       tab->tab[ent].adr = mfalloc(mem, sizeof(unsigned));
   */
}

/* free this ent! */
void mfree(mfile *mem, unsigned ent) {
    unsigned a;
    unsigned z;
    a = adrent(mem, ent);
    if (szent(mem, ent) == 0) return; // ignore zero size allocs
    z = adrent(mem, FREE);

    /* copy the current free-list head to the data area of the ent. */
    // *(unsigned *)(mem->base + adrent(mem, ent)) = mem->avail;
    memcpy(mem->base+a, mem->base+z, sizeof(unsigned));

    /* copy the ent number into the free-list head */
    //mem->avail = ent;
    memcpy(mem->base+z, &ent, sizeof(unsigned));
}

/* discard the free list.
   iterate through tables,
        if element is unmarked and not zero-sized,
            free it.  */
void sweep(mfile *mem) {
    mtab *tab;
    int ntab;
    unsigned z;
    unsigned i;

    z = adrent(mem, FREE);
    //memcpy(mem->base+z, &(unsigned){ 0 }, sizeof(unsigned));
    *(unsigned *)(mem->base+z) = 0;
    tab = (void *)(mem->base);
    ntab = 0;
    for (i = mem->start; i < tab->nextent; i++) {
        if ((tab->tab[i].mark & MARKM) == 0 && tab->tab[i].sz != 0)
            mfree(mem, i + ntab*TABSZ);
    }
    while (tab->nexttab != 0) {
        tab = (void *)(mem->base + tab->nexttab);
        ++ntab;
        for (i = mem->start; i < tab->nextent; i++) {
            if ((tab->tab[i].mark & MARKM) == 0 && tab->tab[i].sz != 0)
                mfree(mem, i + ntab*TABSZ);
        }
        if (i!=TABSZ) break;
    }
}

/* clear all marks,
   determine GLOBAL/LOCAL and mark all root stacks,
   sweep. */
void collect(mfile *mem) {
    unsigned i;
    unsigned *cid;
    context *ctx;

    printf("\ncollect:\n");

    unmark(mem);

    /*for(i=mem->roots[0];i<=mem->roots[1];i++){markstack(mem,adrent(mem,i));}*/

    cid = (void *)(mem->base + adrent(mem, CTXLIST));
    ctx = ctxcid(cid[0]);
    /* markstack(ctx, mem, adrent(mem, VS)); */ // TODO will need a special routine
    marksave(ctx, mem, adrent(mem, VS));
    if (mem == ctx->lo) {

        for (i = 0; cid[i]; i++) {
            ctx = ctxcid(cid[i]);
            markstack(ctx, mem, ctx->os);
            markstack(ctx, mem, ctx->ds);
            markstack(ctx, mem, ctx->es);
        }

    } else {
        markstack(ctx, mem, adrent(mem, NAMES));
    }

    sweep(mem);
}

void dumpfree(mfile *mem) {
    unsigned z = adrent(mem, FREE);;
    unsigned e;
    printf("freelist: ");
    memcpy(&e, mem->base+z, sizeof(unsigned));
    while (e) {
        printf("%d(%d) ", e, szent(mem, e));
        z = adrent(mem, e);
        memcpy(&e, mem->base+z, sizeof(unsigned));
    }
}

enum { PERIOD = 200 };

/* scan the free list for a suitably sized bit of memory,
   if the allocator falls back to fresh memory PERIOD times,
        it triggers a collection. */
unsigned gballoc(mfile *mem, unsigned sz) {
#if 0 
    unsigned z = adrent(mem, FREE); // free pointer
    unsigned e;                     // working pointer
    static int period = PERIOD;
    memcpy(&e, mem->base+z, sizeof(unsigned)); // e = *z
try_again:
    while (e) { // e is not zero
        if (szent(mem,e) >= sz) {
            memcpy(mem->base+z,
                    mem->base+adrent(mem,e), sizeof(unsigned));
            return e;
        }
        z = adrent(mem,e);
        memcpy(&e, mem->base+z, sizeof(unsigned));
    }
    if (--period == 0) {
        period = PERIOD;
        collect(mem);
        goto try_again;
    }
#endif
    return mtalloc(mem, 0, sz);
}

/* allocate new entry, copy data, steal its adr, stash old adr, free it */
unsigned mfrealloc(mfile *mem, unsigned oldadr, unsigned oldsize, unsigned newsize) {
    mtab *tab = NULL;
    unsigned newadr;
    unsigned ent;
    unsigned rent; // relative ent

#ifdef DEBUGFREE
    printf("mfrealloc: ");
    printf("initial ");
    dumpfree(mem);
#endif

    /* allocate new entry */
    rent = ent = mtalloc(mem, 0, newsize);
    findtabent(mem, &tab, &rent);

    /* steal its adr */
    newadr = tab->tab[rent].adr;

    /* copy data */
    memcpy(mem->base + newadr, mem->base + oldadr, oldsize);

    /* stash old adr */
    tab->tab[rent].adr = oldadr;
    tab->tab[rent].sz = oldsize;

    /* free it */
    mfree(mem, ent);

#ifdef DEBUGFREE
    printf("final ");
    dumpfree(mem);
    printf("\n");
    dumpmtab(mem, 0);
    fflush(NULL);
#endif

    return newadr;
}

#ifdef TESTMODULE

context *ctx;
mfile *mem;
unsigned stac;

/*
void init(void) {
    initmem(&mem, "x.mem");
    (void)initmtab(&mem);
    initfree(&mem);
    initsave(&mem);
    initctxlist(&mem);
    mtab *tab = (void *)mem.base;
    unsigned ent = mtalloc(&mem, 0, 0);
    //findtabent(&mem, &tab, &ent);
    stac = tab->tab[ent].adr = initstack(&mem);
    //mem.roots[0] = VS;
    //mem.roots[1] = ent;
    mem.start = ent+1;
}
*/

//itp *itpdata;

void init(void) {
    itpdata = malloc(sizeof*itpdata);
    memset(itpdata, 0, sizeof*itpdata);
    inititp(itpdata);
}

int main(void) {
    init();
    printf("\n^test gc.c\n");
    ctx = &itpdata->ctab[0];
    mem = ctx->lo;
    stac = ctx->os;

    push(mem, stac, consint(5));
    push(mem, stac, consint(6));
    push(mem, stac, consreal(7.0));
    object ar;
    ar = consarr(mem, 3);
    int i;
    for (i=0; i < 3; i++)
        arrput(mem, ar, i, pop(mem, stac));
    push(mem, stac, ar);                   /* array on stack */

    push(mem, stac, consint(1));
    push(mem, stac, consint(2));
    push(mem, stac, consint(3));
    ar = consarr(mem, 3);
    for (i=0; i < 3; i++)
        arrput(mem, ar, i, pop(mem, stac));
    dumpobject(ar);
    /* array not on stack */

#define CNT_STR(x) sizeof(x), x
    push(mem, stac, consstr(mem, CNT_STR("string on stack")));

    dumpobject(consstr(mem, CNT_STR("string not on stack")));

    collect(mem);
    push(mem, stac, consstr(mem, CNT_STR("string on stack")));
    dumpobject(consstr(mem, CNT_STR("string not on stack")));

    collect(mem);
    dumpmfile(mem);
    printf("stackaedr: %04x\n", stac);
    dumpmtab(mem, 0);
    
    return 0;
}

#endif
