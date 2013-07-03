#include <math.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h> /* malloc */
#include <stdio.h>
#include "m.h"
#include "ob.h"
#include "s.h"
#include "gc.h"
#include "v.h"
#include "itp.h"
#include "st.h"
#include "nm.h"
#include "di.h"

/*
typedef struct {
    word tag;
    word sz;
    word nused;
    word pad;
} dichead;
*/

/* Compare two objects for "equality". 
   return 0 if "equal"
          +value if L > R
          -value if L < R
 */
int objcmp(context *ctx, object L, object R) {
    if (type(L) == type(R))
        switch (type(L)) {
            default: error("unhandled type in objcmp");

            case marktype:
            case nulltype:
            case invalidtype: return 0;

            case integertype: return L.int_.val - R.int_.val;
            case realtype: return (fabs(L.real_.val - R.real_.val) < 0.0001)?
                                    0:
                                    L.real_.val - R.real_.val;

            case operatortype: 
            case nametype: return !( L.mark_.padw == R.mark_.padw );

            case dicttype: return !( L.comp_.ent == R.comp_.ent );
            case arraytype: return !( L.comp_.sz == R.comp_.sz
                                    && (L.tag&FBANK) == (R.tag&FBANK)
                                    && L.comp_.ent == R.comp_.ent
                                    && L.comp_.off == R.comp_.off ); // 0 if all eq
            case stringtype: return L.comp_.sz == R.comp_.sz ?
                             memcmp( (bank(ctx, L) /*L.tag&FBANK?ctx->gl:ctx->lo*/)->base
                                     + adrent(bank(ctx, L) /*L.tag&FBANK?ctx->gl:ctx->lo*/,
                                         L.comp_.ent),
                                     (bank(ctx, R) /*R.tag&FBANK?ctx->gl:ctx->lo*/)->base
                                     + adrent(bank(ctx, R) /*R.tag&FBANK?ctx->gl:ctx->lo*/,
                                         R.comp_.ent),
                                     L.comp_.sz) :
                                         L.comp_.sz - R.comp_.sz;
        }
    return type(L) - type(R);
}

/* more like scrambled eggs */
unsigned hash(object k) {
    unsigned h;
    h = (type(k) << 1) /* ignore flags */
        + (k.comp_.sz << 3)
        + (k.comp_.ent << 7)
        + (k.comp_.off << 5);
#ifdef DEBUGDIC
    printf("\nhash(");
    dumpobject(k);
    printf(")=%d", h);
#endif
    return h;
}

/* allocate an entity with gballoc,
   set the save level in the mark,
   extract the "pointer" from the entity,
   Initialize a dichead in memory,
   just after the head, clear a table of pairs. */
object consdic(mfile *mem, unsigned sz) {
    mtab *tab;
    object d;
    unsigned rent;
    unsigned cnt;
    unsigned ad;
    dichead *dp;
    object *tp;
    unsigned i;

    d.tag = dicttype;
    d.comp_.sz = sz;
    d.comp_.off = 0;
    //d.comp_.ent = mtalloc(mem, 0, sizeof(dichead) + DICTABSZ(sz) );
    d.comp_.ent = gballoc(mem, sizeof(dichead) + DICTABSZ(sz) );

    tab = (void *)(mem->base);
    rent = d.comp_.ent;
    findtabent(mem, &tab, &rent);
    cnt = count(mem, adrent(mem, VS));
    tab->tab[rent].mark = ( (0 << MARKO) | (0 << RFCTO) |
            (cnt << LLEVO) | (cnt << TLEVO) );

    ad = adrent(mem, d.comp_.ent);
    dp = (void *)(mem->base + ad); /* clear header */
    dp->tag = dicttype;
    dp->sz = sz;
    dp->nused = 0;
    dp->pad = 0;

    tp = (void *)(mem->base + ad + sizeof(dichead)); /* clear table */
    for (i=0; i < DICTABN(sz); i++)
        tp[i] = null; /* remember our null object is not all-zero! */
#ifdef DEBUGDIC
    printf("consdic: "); dumpdic(mem, d);
#endif
    return d;
}

/* select mfile according to vmmode,
   call consdic,
   set the BANK flag. */
object consbdc(context *ctx, unsigned sz) {
    object d = consdic(ctx->vmmode==GLOBAL? ctx->gl: ctx->lo, sz);
    if (ctx->vmmode == GLOBAL)
        d.tag |= FBANK;
    return d;
}

/* get the nused field from the dichead */
unsigned diclength(mfile *mem, object d) {
    dichead *dp = (void *)(mem->base + adrent(mem, d.comp_.ent));
    return dp->nused;
}

/* get the sz field from the dichead */
unsigned dicmaxlength(mfile *mem, object d) {
    dichead *dp = (void *)(mem->base + adrent(mem, d.comp_.ent));
    return dp->sz;
}

void dicgrow(context *ctx, object d) {
    mfile *mem;
    unsigned sz;
    unsigned newsz;
    unsigned ad;
    dichead *dp;
    object *tp;
    object n;
    int i;
    mem = bank(ctx, d);
#ifdef DEBUGDIC
    printf("DI growing dict\n");
    dumpdic(mem, d);
#endif
    n = consdic(mem, newsz = 2 * dicmaxlength(mem, d));

    ad = adrent(mem, d.comp_.ent);
    dp = (void *)(mem->base + ad);
    sz = (dp->sz + 1);
    tp = (void *)(mem->base + ad + sizeof(dichead)); /* copy data */
    for ( i=0; i < sz; i++)
        //if (objcmp(ctx, tp[2*i], null) != 0) {
        if (type(tp[2*i]) != nulltype) {
            dicput(ctx, mem, n, tp[2*i], tp[2*i+1]);
        }
#ifdef DEBUGDIC
    printf("n: ");
    dumpdic(mem, n);
#endif

    {   // exchange entities
        mtab *dtab, *ntab;
        unsigned dent, nent;
        unsigned hold;

        dent = d.comp_.ent;
        nent = n.comp_.ent;
        findtabent(mem, &dtab, &dent);
        findtabent(mem, &ntab, &nent);

        // exchange adrs
        hold = dtab->tab[dent].adr;
        dtab->tab[dent].adr = ntab->tab[nent].adr;
        ntab->tab[nent].adr = hold;

        // exchange sizes
        hold = dtab->tab[dent].sz;
        dtab->tab[dent].sz = ntab->tab[nent].sz;
        ntab->tab[nent].sz = hold;

        mfree(mem, n.comp_.ent);
    }
}

/* is it full? (y/n) */
bool dicfull(mfile *mem, object d) {
    return diclength(mem, d) == dicmaxlength(mem, d);
}

void dumpdic(mfile *mem, object d) {
    unsigned ad = adrent(mem, d.comp_.ent);
    dichead *dp = (void *)(mem->base + ad);
    object *tp = (void *)(mem->base + ad + sizeof(dichead));
    unsigned sz = (dp->sz + 1);
    printf("\n");
    int i;
    for (i=0; i < sz; i++) {
        printf("%d:", i);
        if (type(tp[2*i]) != nulltype) {
            dumpobject(tp[2*i]);
        }
    }
}

#define RETURN_TAB_I_IF_EQ_K_OR_NULL    \
    if (objcmp(ctx, tp[2*i], k) == 0    \
    || objcmp(ctx, tp[2*i], null) == 0) \
            return tp + (2*i);

/* perform a hash-assisted lookup.
   returns a pointer to the desired pair (if found)), or a null-pair. */
/*@dependent@*/ /*@null@*/
object *diclookup(context *ctx, /*@dependent@*/ mfile *mem, object d, object k) {
    unsigned ad;
    dichead *dp;
    object *tp;
    unsigned sz;
    unsigned h;
    unsigned i;

    if (type(k) == stringtype) {
        char *s = alloca(k.comp_.sz+1);
        memcpy(s, charstr(ctx, k), k.comp_.sz);
        s[k.comp_.sz] = '\0';
        k = consname(ctx, s);
    }

    ad = adrent(mem, d.comp_.ent);
    dp = (void *)(mem->base + ad);
    tp = (void *)(mem->base + ad + sizeof(dichead));
    sz = (dp->sz + 1);

    h = hash(k) % sz;
    i = h;
#ifdef DEBUGDIC
    printf("diclookup(");
    dumpobject(k);
    printf(");");
    printf("%%%d=%d",sz, h);
#endif

    RETURN_TAB_I_IF_EQ_K_OR_NULL
    for (++i; i < sz; i++) {
        RETURN_TAB_I_IF_EQ_K_OR_NULL
    }
    for (i=0; i < h; i++) {
        RETURN_TAB_I_IF_EQ_K_OR_NULL
    }
    return NULL; /* i == h : dict is overfull: no null entry */
}

/* see if lookup returns a non-null pair. */
bool dicknown(context *ctx, /*@dependent@*/ mfile *mem, object d, object k) {
    object *r;
    r = diclookup(ctx, mem, d, k);
    if (r == NULL) return false;
    return type(*r) != nulltype;
}

/* call diclookup,
   return the value if the key is non-null. */
object dicget(context *ctx, /*@dependent@*/ mfile *mem, object d, object k) {
    object *e;
    e = diclookup(ctx, mem, d, k);
    if (type(e[0]) == nulltype)
        error("undefined");
    return e[1];
}

/* select mfile according to BANK field,
   call dicget. */
object bdcget(context *ctx, object d, object k) {
    return dicget(ctx, bank(ctx, d) /*d.tag&FBANK?ctx->gl:ctx->lo*/, d, k);
}

/* save data if not save at this level,
   lookup the key,
   if key is null, check if the dict is full,
       increase nused,
       set key,
       update value. */
void dicput(context *ctx, mfile *mem, object d, object k, object v) {
    object *e;
    dichead *dp;
retry:
    if (!stashed(mem, d.comp_.ent)) stash(mem, d.comp_.ent);
    e = diclookup(ctx, mem, d, k);
    if (e == NULL) {
        //error("dict overfull");
        //grow dict!
        dicgrow(ctx, d);
        goto retry;
    }
    if (type(e[0]) == nulltype) {
        if (dicfull(mem, d)) {
            //error("dict full");
            //grow dict!
            dicgrow(ctx, d);
            goto retry;
        }
        dp = (void *)(mem->base + adrent(mem, d.comp_.ent));
        ++ dp->nused;
        e[0] = k;
    }
    e[1] = v;
}

/* select mfile according to BANK field,
   call dicput. */
void bdcput(context *ctx, object d, object k, object v) {
    dicput(ctx, bank(ctx, d) /*d.tag&FBANK?ctx->gl:ctx->lo*/, d, k, v);
}


#ifdef TESTMODULE
#include <stdio.h>

//context ctx;
context *ctx;

void init() {
    //initcontext(&ctx);
    itpdata=malloc(sizeof*itpdata);
    inititp(itpdata);
    ctx = &itpdata->ctab[0];
}

int main(void) {
    printf("\n^test di.c\n");
    init();

    object d;
    d = consbdc(ctx, 12);
    printf("1 2 def\n");
    bdcput(ctx, d, consint(1), consint(2));
    printf("3 4 def\n");
    bdcput(ctx, d, consint(3), consint(4));

    printf("1 load =\n");
    dumpobject(bdcget(ctx, d, consint(1)));
    //dumpobject(bdcget(ctx, d, consint(2))); // error("undefined");
    printf("\n3 load =\n");
    dumpobject(bdcget(ctx, d, consint(3)));


    //dumpmfile(ctx->gl);
    //dumpmtab(ctx->gl, 0);
    puts("");
    return 0;
}

#endif
