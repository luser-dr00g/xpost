/*
 * Xpost - a Level-2 Postscript interpreter
 * Copyright (C) 2013, Michael Joshua Ryan
 * Copyright (C) 2013, Thorsten Behrens
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of the Xpost software product nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef STDC_HEADERS
# include <stdlib.h>
# include <stddef.h>
#else
# ifdef HAVE_STDLIB_H
#  include <stdlib.h>
# endif
#endif
#ifdef HAVE_ALLOCA_H
# include <alloca.h>
#elif !defined alloca
# ifdef __GNUC__
#  define alloca __builtin_alloca
# elif defined _AIX
#  define alloca __alloca
# elif defined _MSC_VER
#  include <malloc.h>
#  define alloca _alloca
# elif !defined HAVE_ALLOCA
#  ifdef  __cplusplus
extern "C"
#  endif
void *alloca (size_t);
# endif
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
#include <math.h>
#include <string.h>
#include <stdlib.h> /* malloc */
#include <stdio.h>

#include "xpost_memory.h"  /* dicts live in mfile, accessed via mtab */
#include "xpost_object.h"  /* dict is an object, containing objects */
#include "xpost_stack.h"  /* may need to count the save stack */
#include "xpost_garbage.h"  /* dicts are garbage collected */
#include "xpost_save.h"  /* dicts obey save/restore */
#include "xpost_interpreter.h"  /* banked dicts may live in global or local vm */
#include "xpost_error.h"  /* dict functions may throw errors */
#include "xpost_string.h"  /* may need string functions (convert to name) */
#include "xpost_name.h"  /* may need name functions (create name) */
#include "xpost_dict.h"  /* double-check prototypes */



/*
typedef struct {
    word tag;
    word sz;
    word nused;
    word pad;
} dichead;
*/

/* strict-aliasing compatible poking of double */
typedef union
{
    unsigned long long bits;
    double             number;
} Xpost_Ieee_Double_As_Int;

/* Compare two objects for "equality".
   return 0 if "equal"
          +value if L > R
          -value if L < R
 */
int objcmp(context *ctx,
           Xpost_Object L,
           Xpost_Object R)
{
    /* fold nearly-comparable types to comparable */
    if (xpost_object_get_type(L) != xpost_object_get_type(R)) {
        if (xpost_object_get_type(L) == integertype && xpost_object_get_type(R) == realtype) {
            L = xpost_cons_real(L.int_.val);
            goto cont;
        }
        if (xpost_object_get_type(R) == integertype && xpost_object_get_type(L) == realtype) {
            R = xpost_cons_real(R.int_.val);
            goto cont;
        }
        if (xpost_object_get_type(L) == nametype && xpost_object_get_type(R) == stringtype) {
            L = strname(ctx, L);
            goto cont;
        }
        if (xpost_object_get_type(R) == nametype && xpost_object_get_type(L) == stringtype) {
            R = strname(ctx, R);
            goto cont;
        }
        return xpost_object_get_type(L) - xpost_object_get_type(R);
    }

cont:
    switch (xpost_object_get_type(L)) {
        default:
            fprintf(stderr, "unhandled type (%s) in objcmp", xpost_object_type_names[xpost_object_get_type(L)]);
            error(unregistered, "");
            return -1;

        case marktype: return 0;
        case nulltype: return 0;
        case invalidtype: return 0;

        case booleantype: /*@fallthrough@*/
        case integertype: return L.int_.val - R.int_.val;

        case realtype: return (fabs(L.real_.val - R.real_.val) < 0.0001)?
                                0:
                                L.real_.val - R.real_.val > 0? 1: -1;
        case extendedtype: {
                               double l,r;
                               l = doubleextended(L);
                               r = doubleextended(R);
                               return (fabs(l - r) < 0.0001)?
                                   0:
                                   l - r > 0? 1: -1;
                           }

        case operatortype:  return L.mark_.padw - R.mark_.padw;

        case nametype: return (L.tag&XPOST_OBJECT_TAG_DATA_FLAG_BANK)==(R.tag&XPOST_OBJECT_TAG_DATA_FLAG_BANK)?
                                (signed)(L.mark_.padw - R.mark_.padw):
                                    (signed)((L.tag&XPOST_OBJECT_TAG_DATA_FLAG_BANK) - (R.tag&XPOST_OBJECT_TAG_DATA_FLAG_BANK));

        case dicttype: /*@fallthrough@*/ /*return !( L.comp_.ent == R.comp_.ent ); */
        case arraytype: return !( L.comp_.sz == R.comp_.sz
                                && (L.tag&XPOST_OBJECT_TAG_DATA_FLAG_BANK) == (R.tag&XPOST_OBJECT_TAG_DATA_FLAG_BANK)
                                && L.comp_.ent == R.comp_.ent
                                && L.comp_.off == R.comp_.off ); /* 0 if all eq */

        case stringtype: return L.comp_.sz == R.comp_.sz ?
                                memcmp(charstr(ctx, L), charstr(ctx, R), L.comp_.sz) :
                                L.comp_.sz - R.comp_.sz;
    }
}

/* more like scrambled eggs */
static
unsigned hash(Xpost_Object k)
{
    unsigned h;
    h = ( (xpost_object_get_type(k)
            | (k.comp_.tag & XPOST_OBJECT_TAG_DATA_FLAG_BANK))
            << 1) /* ignore flags (except BANK!) */
        + (k.comp_.sz << 3)
        + (k.comp_.ent << 7)
        + (k.comp_.off << 5);
    /* h = xpost_object_get_type(k); /\* test collisions. *\/ */
#ifdef DEBUGDIC
    printf("\nhash(");
    xpost_object_dump(k);
    printf(")=%d", h);
#endif
    return h;
}

/* allocate an entity with gballoc,
   set the save level in the mark,
   extract the "pointer" from the entity,
   Initialize a dichead in memory,
   just after the head, clear a table of pairs. */
Xpost_Object consdic(mfile *mem,
               unsigned sz)
{
    mtab *tab;
    Xpost_Object d;
    unsigned rent;
    unsigned cnt;
    unsigned ad;
    dichead *dp;
    Xpost_Object *tp;
    unsigned i;

    if (sz < 5) sz = 5;

    assert(mem->base);
    d.tag = dicttype | (XPOST_OBJECT_TAG_ACCESS_UNLIMITED << XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_OFFSET);
    d.comp_.sz = sz;
    d.comp_.off = 0;
    /* d.comp_.ent = mtalloc(mem, 0, sizeof(dichead) + DICTABSZ(sz), 0 ); */
    d.comp_.ent = gballoc(mem, sizeof(dichead) + DICTABSZ(sz), dicttype);

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
Xpost_Object consbdc(context *ctx,
               unsigned sz)
{
    Xpost_Object d = consdic(ctx->vmmode==GLOBAL? ctx->gl: ctx->lo, sz);
    if (ctx->vmmode == GLOBAL)
        d.tag |= XPOST_OBJECT_TAG_DATA_FLAG_BANK;
    return d;
}

/* get the nused field from the dichead */
unsigned diclength(mfile *mem,
                   Xpost_Object d)
{
    dichead *dp = (void *)(mem->base + adrent(mem, d.comp_.ent));
    return dp->nused;
}

/* get the sz field from the dichead */
unsigned dicmaxlength(mfile *mem,
                      Xpost_Object d)
{
    dichead *dp = (void *)(mem->base + adrent(mem, d.comp_.ent));
    return dp->sz;
}

/* allocate a new dictionary,
   copy over all non-null key/value pairs,
   swap adrs in the two table slots. */
static
void dicgrow(context *ctx,
             Xpost_Object d)
{
    mfile *mem;
    unsigned sz;
    unsigned newsz;
    unsigned ad;
    dichead *dp;
    Xpost_Object *tp;
    Xpost_Object n;
    unsigned i;

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
        /* if (objcmp(ctx, tp[2*i], null) != 0) { */
        if (xpost_object_get_type(tp[2*i]) != nulltype) {
            dicput(ctx, mem, n, tp[2*i], tp[2*i+1]);
        }
#ifdef DEBUGDIC
    printf("n: ");
    dumpdic(mem, n);
#endif

    {   /* exchange entities */
        mtab *dtab, *ntab;
        unsigned dent, nent;
        unsigned hold;

        dent = d.comp_.ent;
        nent = n.comp_.ent;
        findtabent(mem, &dtab, &dent);
        findtabent(mem, &ntab, &nent);

        /* exchange adrs */
        hold = dtab->tab[dent].adr;
        dtab->tab[dent].adr = ntab->tab[nent].adr;
        ntab->tab[nent].adr = hold;

        /* exchange sizes */
        hold = dtab->tab[dent].sz;
        dtab->tab[dent].sz = ntab->tab[nent].sz;
        ntab->tab[nent].sz = hold;

        mfree(mem, n.comp_.ent);
    }
}

/* is it full? (y/n) */
int dicfull(mfile *mem,
             Xpost_Object d)
{
    return diclength(mem, d) == dicmaxlength(mem, d);
}

/* print a dump of the dictionary data */
void dumpdic(mfile *mem,
             Xpost_Object d)
{
    unsigned ad = adrent(mem, d.comp_.ent);
    dichead *dp = (void *)(mem->base + ad);
    Xpost_Object *tp = (void *)(mem->base + ad + sizeof(dichead));
    unsigned sz = (dp->sz + 1);
    unsigned i;

    printf("\n");
    for (i=0; i < sz; i++) {
        printf("%d:", i);
        if (xpost_object_get_type(tp[2*i]) != nulltype) {
            xpost_object_dump(tp[2*i]);
        }
    }
}

/* construct an extendedtype object
   from a double value */
/*n.b. Caller Must set EXTENDEDINT or EXTENDEDREAL flag */
/*     in order to unextend() later. */
static
Xpost_Object consextended (double d)
{
    Xpost_Ieee_Double_As_Int r;
    Xpost_Object_Extended e;

    r.number = d;
    e.tag = extendedtype;
    e.sign_exp = (r.bits >> 52) & 0x7FF;
    e.fraction = (r.bits >> 20) & 0xFFFFFFFF;
    return (Xpost_Object)e;
}

/* adapter:
   double <- extendedtype object */
double doubleextended (Xpost_Object e)
{
    Xpost_Ieee_Double_As_Int r;
    r.bits = ((unsigned long long)e.extended_.sign_exp << 52)
             | ((unsigned long long)e.extended_.fraction << 20);
    return r.number;
}

/* convert an extendedtype object to integertype or realtype
   depending upon flag */
Xpost_Object unextend (Xpost_Object e)
{
    Xpost_Object o;
    double d = doubleextended(e);

    if (e.tag & XPOST_OBJECT_TAG_DATA_EXTENDED_INT) {
        o = xpost_cons_int(d);
    } else if (e.tag & XPOST_OBJECT_TAG_DATA_EXTENDED_REAL) {
        o = xpost_cons_real(d);
    } else {
        error(unregistered, "unextend: invalid extended number object");
    }
    return o;
}

/* make key the proper type for hashing */
static
Xpost_Object clean_key (context *ctx,
                  Xpost_Object k)
{
    switch(xpost_object_get_type(k)) {
    default: break;
    case stringtype: {
        char *s = alloca(k.comp_.sz+1);
        memcpy(s, charstr(ctx, k), k.comp_.sz);
        s[k.comp_.sz] = '\0';
        k = consname(ctx, s);
    }
    break;
    case integertype:
        k = consextended(k.int_.val);
        k.tag |= XPOST_OBJECT_TAG_DATA_EXTENDED_INT;
    break;
    case realtype:
        k = consextended(k.real_.val);
        k.tag |= XPOST_OBJECT_TAG_DATA_EXTENDED_REAL;
    break;
    }
    return k;
}

/* repeated loop body from the lookup function */
#define RETURN_TAB_I_IF_EQ_K_OR_NULL    \
    if (objcmp(ctx, tp[2*i], k) == 0    \
    || objcmp(ctx, tp[2*i], null) == 0) \
            return tp + (2*i);

/* perform a hash-assisted lookup.
   returns a pointer to the desired pair (if found)), or a null-pair. */
/*@dependent@*/ /*@null@*/
static
Xpost_Object *diclookup(context *ctx,
        /*@dependent@*/ mfile *mem,
        Xpost_Object d,
        Xpost_Object k)
{
    unsigned ad;
    dichead *dp;
    Xpost_Object *tp;
    unsigned sz;
    unsigned h;
    unsigned i;

    k = clean_key(ctx, k);

    ad = adrent(mem, d.comp_.ent);
    dp = (void *)(mem->base + ad);
    tp = (void *)(mem->base + ad + sizeof(dichead));
    sz = (dp->sz + 1);

    h = hash(k) % sz;
    i = h;
#ifdef DEBUGDIC
    printf("diclookup(");
    xpost_object_dump(k);
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
int dicknown(context *ctx,
        /*@dependent@*/ mfile *mem,
        Xpost_Object d,
        Xpost_Object k)
{
    Xpost_Object *r;

    r = diclookup(ctx, mem, d, k);
    if (r == NULL) return false;
    return xpost_object_get_type(*r) != nulltype;
}

/* call diclookup,
   return the value if the key is non-null. */
Xpost_Object dicget(context *ctx,
        /*@dependent@*/ mfile *mem,
        Xpost_Object d,
        Xpost_Object k)
{
    Xpost_Object *e;

    e = diclookup(ctx, mem, d, k);
    if (e == NULL || xpost_object_get_type(e[0]) == nulltype) {
        error(undefined, "dicget");
        return null;
    }
    return e[1];
}

/* select mfile according to BANK field,
   call dicget. */
Xpost_Object bdcget(context *ctx,
        Xpost_Object d,
        Xpost_Object k)
{
    return dicget(ctx, bank(ctx, d) /*d.tag&XPOST_OBJECT_TAG_DATA_FLAG_BANK?ctx->gl:ctx->lo*/, d, k);
}

/* save data if not save at this level,
   lookup the key,
   if key is null, check if the dict is full,
       increase nused,
       set key,
       update value. */
void dicput(context *ctx,
        mfile *mem,
        Xpost_Object d,
        Xpost_Object k,
        Xpost_Object v)
{
    Xpost_Object *e;
    dichead *dp;

    if (!stashed(mem, d.comp_.ent)) stash(mem, dicttype, 0, d.comp_.ent);
retry:
    e = diclookup(ctx, mem, d, k);
    if (e == NULL) {
        /* rror("dict overfull"); */
        /* row dict! */
        dicgrow(ctx, d);
        goto retry;
    }
    if (xpost_object_get_type(e[0]) == invalidtype) {
        fprintf(stderr, "warning: invalidtype key in dict\n");
        e[0] = null;
    }
    if (xpost_object_get_type(e[0]) == nulltype) {
        if (dicfull(mem, d)) {
            /*error("dict full"); */
            /*grow dict! */
            dicgrow(ctx, d);
            goto retry;
        }
        dp = (void *)(mem->base + adrent(mem, d.comp_.ent));
        ++ dp->nused;
        e[0] = clean_key(ctx, k);
    }
    e[1] = v;
}

/* select mfile according to BANK field,
   call dicput. */
void bdcput(context *ctx,
        Xpost_Object d,
        Xpost_Object k,
        Xpost_Object v)
{
    mfile *mem = bank(ctx, d);
    if (!ignoreinvalidaccess) {
        if ( mem == ctx->gl
                && xpost_object_is_composite(k)
                && mem != bank(ctx, k))
            error(invalidaccess, "local key into global dict");
        if ( mem == ctx->gl
                && xpost_object_is_composite(v)
                && mem != bank(ctx, v)) {
            xpost_object_dump(v);
            error(invalidaccess, "local value into global dict");
        }
    }

    dicput(ctx, bank(ctx, d) /*d.tag&XPOST_OBJECT_TAG_DATA_FLAG_BANK?ctx->gl:ctx->lo*/, d, k, v);
}

/* undefine key from dict */
void dicundef(context *ctx,
        mfile *mem,
        Xpost_Object d,
        Xpost_Object k)
{
    Xpost_Object *e;
    unsigned ad;
    dichead *dp;
    Xpost_Object *tp;
    unsigned sz;
    unsigned h;
    unsigned i;
    unsigned last = 0;
    int lastisset = false;
    int found = false;

    if (!stashed(mem, d.comp_.ent)) stash(mem, dicttype, 0, d.comp_.ent);

    ad = adrent(mem, d.comp_.ent);
    dp = (void *)(mem->base + ad);
    tp = (void *)(mem->base + ad + sizeof(dichead));

    k = clean_key(ctx, k);

    e = diclookup(ctx, mem, d, k); /*find slot for key */
    if (e == NULL || objcmp(ctx,e[0],null) == 0) {
        error(undefined, "dicundef");
    }

    /*find last chained key and value with same hash */
    sz = (dp->sz + 1);
    h = hash(k) % sz;

    for (i=h; i < sz; i++)
        if (h == hash(tp[2*i]) % sz) {
            last = i;
            lastisset = true;
        } else if (objcmp(ctx, tp[2*i], null) == 0) {
            if (lastisset) {
                found = true;
                break;
            }
        }

    if (!found)
        for (i=0; i < h; i++)
            if (h == hash(tp[2*i]) % sz) {
                last = i;
                lastisset = true;
            } else if (objcmp(ctx, tp[2*i], null) == 0) {
                if (lastisset) {
                    found = true;
                    break;
                }
            }

    if (found) { /* f found: move last key and value to slot */
        e[0] = tp[2*last];
        e[1] = tp[2*last+1];
        tp[2*last] = null;
        tp[2*last+1] = null;
    }
    else { /* ot found: write null over key and value */
        e[0] = null;
        e[1] = null;
    }

}

/* undefine key from banked dict */
void bdcundef(context *ctx,
        Xpost_Object d,
        Xpost_Object k)
{
    dicundef(ctx, bank(ctx, d), d, k);
}


#ifdef TESTMODULE_DI
#include <stdio.h>

/*context ctx; */
context *ctx;

void init() {
    /*initcontext(&ctx); */
    itpdata=malloc(sizeof*itpdata);
    inititp(itpdata);
    ctx = &itpdata->ctab[0];
}

int main(void) {
    printf("\n^test di.c\n");
    init();

    Xpost_Object d;
    d = consbdc(ctx, 12);
    printf("1 2 def\n");
    bdcput(ctx, d, xpost_cons_int(1), xpost_cons_int(2));
    printf("3 4 def\n");
    bdcput(ctx, d, xpost_cons_int(3), xpost_cons_int(4));

    printf("1 load =\n");
    xpost_object_dump(bdcget(ctx, d, xpost_cons_int(1)));
    /* xpost_object_dump(bdcget(ctx, d, xpost_cons_int(2))); /\* error("undefined"); *\/ */
    printf("\n3 load =\n");
    xpost_object_dump(bdcget(ctx, d, xpost_cons_int(3)));


    /*dumpmfile(ctx->gl); */
    /*dumpmtab(ctx->gl, 0); */
    puts("");
    return 0;
}

#endif
