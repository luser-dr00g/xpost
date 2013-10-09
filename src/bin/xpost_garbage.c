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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xpost_memory.h"
#include "xpost_object.h"
#include "xpost_stack.h"
#include "xpost_interpreter.h"
#include "xpost_array.h"
#include "xpost_string.h"
#include "xpost_dict.h"
#include "xpost_save.h"
#include "xpost_garbage.h"

#ifdef TESTMODULE_GC
#include <stdio.h>
#endif


/* iterate through all tables,
    clear the MARK in the mark. */
static
void unmark(mfile *mem)
{
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
static
void markent(mfile *mem,
        unsigned ent)
{
    mtab *tab;

    if (ent < mem->start)
        return;

    tab = (void *)(mem->base);

    findtabent(mem,&tab,&ent);
    tab->tab[ent].mark |= MARKM;
}

/* is it marked? */
static
int marked(mfile *mem,
        unsigned ent)
{
    mtab *tab = (void *)(mem->base);
    findtabent(mem,&tab,&ent);
    return (tab->tab[ent].mark & MARKM) >> MARKO;
}

/* recursively mark an object */
static
void markobject(context *ctx, mfile *mem, object o, int markall);

/* recursively mark a dictionary */
static
void markdict(context *ctx,
        mfile *mem,
        unsigned adr,
        int markall)
{
    dichead *dp = (void *)(mem->base + adr);
    object *tp = (void *)(mem->base + adr + sizeof(dichead));
    int j;

    for (j=0; j < DICTABN(dp->sz); j++) {
        markobject(ctx, mem, tp[j], markall);
    }
}

/* recursively mark all elements of array */
static
void markarray(context *ctx,
        mfile *mem,
        unsigned adr,
        unsigned sz,
        int markall)
{
    object *op = (void *)(mem->base + adr);
    unsigned j;

    for (j=0; j < sz; j++) {
        markobject(ctx, mem, op[j], markall);
    }
}

/* traverse the contents of composite objects */
static
void markobject(context *ctx,
        mfile *mem,
        object o,
        int markall)
{
    switch(type(o)) {

    case arraytype:
#ifdef TESTMODULE_GC
    printf("markobject: %s %d\n", types[type(o)], o.comp_.sz);
#endif
        if (bank(ctx, o) != mem) {
            if (markall)
                mem = bank(ctx, o);
            else
                break;
        }
        if (!marked(mem, o.comp_.ent)) {
            markent(mem, o.comp_.ent);
            markarray(ctx, mem, adrent(mem, o.comp_.ent), o.comp_.sz, markall);
        }
        break;

    case dicttype:
#ifdef TESTMODULE_GC
    printf("markobject: %s %d\n", types[type(o)], o.comp_.sz);
#endif
        if (bank(ctx, o) != mem) {
            if (markall)
                mem = bank(ctx, o);
            else
                break;
        }
        if (!marked(mem, o.comp_.ent)) {
            markent(mem, o.comp_.ent);
            markdict(ctx, mem, adrent(mem, o.comp_.ent), markall);
        }
        break;

    case stringtype:
#ifdef TESTMODULE_GC
    printf("markobject: %s %d\n", types[type(o)], o.comp_.sz);
#endif
        if (bank(ctx, o) != mem) {
            if (markall)
                mem = bank(ctx, o);
            else
                break;
        }
        markent(mem, o.comp_.ent);
        break;

    case filetype:
        if (mem == ctx->lo) {
            markent(mem, o.mark_.padw);
        } else {//shouldn't even find in global
            printf("file found in global vm\n");
        }
        break;
    }
}

/* mark all allocations referred to by objects in stack */
static
void markstack(context *ctx,
        mfile *mem,
        unsigned stackadr,
        int markall)
{
    stack *s = (void *)(mem->base + stackadr);
    unsigned i;

#ifdef TESTMODULE_GC
    printf("marking stack of size %u\n", s->top);
#endif

next:
    for (i=0; i < s->top; i++) {
        markobject(ctx, mem, s->data[i], markall);
    }
    if (i==STACKSEGSZ) { /* ie. s->top == STACKSEGSZ */
        s = (void *)(mem->base + s->nextseg);
        goto next;
    }

    //if (s->nextseg) { /* maybe not. this is a MARK phase, after all */
        //sfree(mem, s->nextseg);
        //s->nextseg = 0;
    //}
}

/* mark all allocations referred to by objects in save object's stack of saverec_'s */
static
void marksavestack(context *ctx,
        mfile *mem,
        unsigned stackadr)
{
    stack *s = (void *)(mem->base + stackadr);
    unsigned i;
    (void)ctx;

#ifdef TESTMODULE_GC
    printf("marking save stack of size %u\n", s->top);
#endif

next:
    for (i=0; i < s->top; i++) {
        //markobject(ctx, mem, s->data[i]);
        //marksavestack(ctx, mem, s->data[i].save_.stk);
        markent(mem, s->data[i].saverec_.src);
        markent(mem, s->data[i].saverec_.cpy);
        if (s->data[i].saverec_.tag == dicttype) {
            markdict(ctx, mem, adrent(mem, s->data[i].saverec_.src), false);
            markdict(ctx, mem, adrent(mem, s->data[i].saverec_.cpy), false);
        }
        if (s->data[i].saverec_.tag == arraytype) {
            unsigned sz = s->data[i].saverec_.pad;
            markarray(ctx, mem, adrent(mem, s->data[i].saverec_.src), sz, false);
            markarray(ctx, mem, adrent(mem, s->data[i].saverec_.cpy), sz, false);
        }
    }
    if (i==STACKSEGSZ) { /* ie. s->top == STACKSEGSZ */
        s = (void *)(mem->base + s->nextseg);
        goto next;
    }
    
    if (s->nextseg) {
        sfree(mem, s->nextseg);
        s->nextseg = 0;
    }
}

/* mark all allocations referred to by objects in save stack */
static
void marksave(context *ctx,
        mfile *mem,
        unsigned stackadr)
{
    stack *s = (void *)(mem->base + stackadr);
    unsigned i;

#ifdef TESTMODULE_GC
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
void initfree(mfile *mem)
{
    unsigned ent = mtalloc(mem, 0, sizeof(unsigned), 0);
    unsigned val = 0;
    assert (ent == FREE);
    put(mem, ent, 0, sizeof(unsigned), &val);
    
    /*
       unsigned ent = mtalloc(mem, 0, 0, 0);
       mtab *tab = (void *)mem->base;
       tab->tab[ent].adr = mfalloc(mem, sizeof(unsigned));
   */
}

/* free this ent! */
void mfree(mfile *mem,
        unsigned ent)
{
    mtab *tab;
    unsigned rent = ent;
    unsigned a;
    unsigned z;
    //return;

    if (ent < mem->start)
        return;

    findtabent(mem, &tab, &rent);
    //a = adrent(mem, ent);
    a = tab->tab[rent].adr;
    //if (szent(mem, ent) == 0) return; // ignore zero size allocs
    if (tab->tab[rent].sz == 0) return;

    if (tab->tab[rent].tag == filetype) {
        FILE *fp;
        get(mem, ent, 0, sizeof(FILE *), &fp);
        if (fp
                && fp != stdin
                && fp != stdout
                && fp != stderr) {
            printf("gc:mfree closing FILE*\n");
            fclose(fp);
        }
    }

    z = adrent(mem, FREE);
    //printf("freeing %d bytes\n", szent(mem, ent));

    /* copy the current free-list head to the data area of the ent. */
    memcpy(mem->base+a, mem->base+z, sizeof(unsigned));

    /* copy the ent number into the free-list head */
    memcpy(mem->base+z, &ent, sizeof(unsigned));
}

/* discard the free list.
   iterate through tables,
        if element is unmarked and not zero-sized,
            free it.  */
static
void sweep(mfile *mem)
{
    mtab *tab;
    int ntab;
    unsigned zero = 0;
    unsigned z;
    unsigned i;

    z = adrent(mem, FREE); // address of the free list head

    memcpy(mem->base+z, &zero, sizeof(unsigned)); // discard list
    //*(unsigned *)(mem->base+z) = 0;

    /* scan first table */
    tab = (void *)(mem->base);
    ntab = 0;
    for (i = mem->start; i < tab->nextent; i++) {
        if ( (tab->tab[i].mark & MARKM) == 0
                && tab->tab[i].sz != 0)
            mfree(mem, i);
    }

    /* scan linked tables */
    while (i < TABSZ && tab->nexttab != 0) {
        tab = (void *)(mem->base + tab->nexttab);
        ++ntab;

        for (i = mem->start; i < tab->nextent; i++) {
            if ( (tab->tab[i].mark & MARKM) == 0
                    && tab->tab[i].sz != 0)
                mfree(mem, i + ntab*TABSZ);
        }
    }
}

/* clear all marks,
   determine GLOBAL/LOCAL and mark all root stacks,
   sweep. */
void collect(mfile *mem, int dosweep, int markall)
{
    unsigned i;
    unsigned *cid;
    context *ctx = NULL;
    int isglobal;

    if (initializing) 
        return;

    //printf("\ncollect:\n");

    /* determine global/glocal */
    isglobal = false;
    cid = (void *)(mem->base + adrent(mem, CTXLIST));
    for (i = 0; i < MAXCONTEXT && cid[i]; i++) {
        ctx = ctxcid(cid[i]);
        if (mem == ctx->gl) {
            isglobal = true;
            break;
        }
    }

    if (isglobal) {
        unmark(mem);

        marksave(ctx, mem, adrent(mem, VS));
        markstack(ctx, mem, adrent(mem, NAMES), markall);

        for (i = 0; i < MAXCONTEXT && cid[i]; i++) {
            ctx = ctxcid(cid[i]);
            collect(ctx->lo, false, markall);
        }

    } else {
        unmark(mem);

        marksave(ctx, mem, adrent(mem, VS));
        markstack(ctx, mem, adrent(mem, NAMES), markall);

        for (i = 0; i < MAXCONTEXT && cid[i]; i++) {
            ctx = ctxcid(cid[i]);
            markstack(ctx, mem, ctx->os, markall);
            markstack(ctx, mem, ctx->ds, markall);
            markstack(ctx, mem, ctx->es, markall);
            markstack(ctx, mem, ctx->hold, markall);
        }
    }

    if (dosweep) {
        sweep(mem);
        if (isglobal) {
            for (i = 0; i < MAXCONTEXT && cid[i]; i++) {
                ctx = ctxcid(cid[i]);
                sweep(ctx->lo);
            }
        }
    }
}

/* print a dump of the free list */
void dumpfree(mfile *mem)
{
    unsigned e;
    unsigned z = adrent(mem, FREE);;

    printf("freelist: ");
    memcpy(&e, mem->base+z, sizeof(unsigned));
    while (e) {
        printf("%d(%d) ", e, szent(mem, e));
        z = adrent(mem, e);
        memcpy(&e, mem->base+z, sizeof(unsigned));
    }
}

/* scan the free list for a suitably sized bit of memory,
   if the allocator falls back to fresh memory PERIOD times,
        it triggers a collection. */
unsigned gballoc(mfile *mem,
        unsigned sz,
        unsigned tag)
{
    unsigned z = adrent(mem, FREE); // free pointer
    unsigned e;                     // working pointer
    static int period = PERIOD;

//#if 0
try_again:
    memcpy(&e, mem->base+z, sizeof(unsigned)); // e = *z
    while (e) { // e is not zero
        if (szent(mem,e) >= sz) {
            mtab *tab;
            unsigned ent;
            memcpy(mem->base+z,
                    mem->base+adrent(mem,e), sizeof(unsigned));
            ent = e;
            findtabent(mem, &tab, &ent);
            tab->tab[ent].tag = tag;
            return e;
        }
        z = adrent(mem,e);
        memcpy(&e, mem->base+z, sizeof(unsigned));
    }
    if (--period == 0) {
        period = PERIOD;
        collect(mem, true, false);
        goto try_again;
    }
//#endif
    return mtalloc(mem, 0, sz, tag);
}

/* allocate new entry, copy data, steal its adr, stash old adr, free it */
unsigned mfrealloc(mfile *mem,
        unsigned oldadr,
        unsigned oldsize,
        unsigned newsize)
{
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
    rent = ent = mtalloc(mem, 0, newsize, 0);
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

#ifdef TESTMODULE_GC

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

extern itp *itpdata;

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
    //     ^ent 8 (8): adr 3404 0x0d4c, sz [24], mark _
    //     ^ 06  00  00  00  6en 67g 20  6en 6fo 74t 20
    printf("gc: look at the mark field . . . . . . . .^\n");
    printf("also, see that the first 4 bytes of strings not on stack\n"
           "have been obliterated to link-up the free list.\n");

    return 0;
}

#endif
