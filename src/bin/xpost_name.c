/*
 * Xpost - a Level-2 Postscript interpreter
 * Copyright (C) 2013, Michael Joshua Ryan
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

#ifdef __MINGW32__
# include "osmswin.h" /* xpost_getpagesize */
#else
# include "osunix.h" /* xpost_getpagesize */
#endif

#include "xpost_memory.h"  // name structures live in mfiles
#include "xpost_object.h"  // names are objects, with associated hidden string objects
#include "xpost_garbage.h"  // strings are allocated using gballoc
#include "xpost_stack.h"  // name strings live on a stack
#include "xpost_interpreter.h"  // initialize interpreter to test
#include "xpost_string.h"  // access string objects
#include "xpost_name.h"  // double-check prototypes

#define CNT_STR(s) sizeof(s)-1, s

/* print a dump of the name string stacks, global and local */
void dumpnames(context *ctx)
{
    unsigned stk;
    unsigned cnt, i;
    Xpost_Object str;
    char *s;

    stk = adrent(ctx->gl, NAMES);
    cnt = count(ctx->gl, stk);
    printf("global names:\n");
    for (i=0; i < cnt; i++){
        str = bot(ctx->gl, stk, i);
        s = charstr(ctx, str);
        printf("%d: %*s\n", i, str.comp_.sz, s);
    }
    stk = adrent(ctx->lo, NAMES);
    cnt = count(ctx->lo, stk);
    printf("local names:\n");
    for (i=0; i < cnt; i++) {
        str = bot(ctx->lo, stk, i);
        s = charstr(ctx, str);
        printf("%d: %*s\n", i, str.comp_.sz, s);
    }
}

/* initialize the name special entities NAMES, NAMET */
void initnames(context *ctx)
{
    mtab *tab;
    unsigned ent;
    unsigned t;
    unsigned mode;

    mode = ctx->vmmode;
    ctx->vmmode = GLOBAL;
    ent = mtalloc(ctx->gl, 0, 0, 0); //gl:NAMES
    assert(ent == NAMES);
    ent = mtalloc(ctx->gl, 0, 0, 0); //gl:NAMET
    assert(ent == NAMET);

    t = initstack(ctx->gl);
    tab = (void *)ctx->gl->base; //recalc pointer
    tab->tab[NAMES].adr = t;
    tab->tab[NAMET].adr = 0;
    push(ctx->gl, adrent(ctx->gl, NAMES), consbst(ctx, CNT_STR("_not_a_name_")));
    assert (top(ctx->gl, adrent(ctx->gl, NAMES), 0).comp_.ent == BOGUSNAME);

    ctx->vmmode = LOCAL;
    ent = mtalloc(ctx->lo, 0, 0, 0); //lo:NAMES
    assert(ent == NAMES);
    ent = mtalloc(ctx->lo, 0, 0, 0); //lo:NAMET
    assert(ent == NAMET);

    t = initstack(ctx->lo);
    tab = (void *)ctx->lo->base; //recalc pointer
    tab->tab[NAMES].adr = t;
    tab->tab[NAMET].adr = 0;
    push(ctx->lo, adrent(ctx->lo, NAMES), consbst(ctx, CNT_STR("_not_a_name_")));
    assert (top(ctx->lo, adrent(ctx->lo, NAMES), 0).comp_.ent == BOGUSNAME);

    ctx->vmmode = mode;
}

/* perform a search using the ternary search tree */
static
unsigned tstsearch(mfile *mem,
                   unsigned tadr,
                   char *s)
{
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

/* add a string to the ternary search tree */
static
unsigned tstinsert(mfile *mem,
                   unsigned tadr,
                   char *s)
{
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

/* add the name to the name stack, return index */
static
unsigned addname(context *ctx,
                 char *s)
{
    mfile *mem = ctx->vmmode==GLOBAL?ctx->gl:ctx->lo;
    unsigned names = adrent(mem, NAMES);
    unsigned u = count(mem, names);

    //dumpmfile(ctx->gl);
    //dumpmtab(ctx->gl, 0);
    //unsigned vmmode = ctx->vmmode;
    //ctx->vmmode = GLOBAL;
    push(mem, names, consbst(ctx, strlen(s), s));
    //ctx->vmmode = vmmode;
    return u;
}

/* construct a name object from a string
   searches and if necessary installs string
   in ternary search tree,
   adding string to stack if so.
   returns a generic object with
       nametype tag with BANK field, 
       mark_.pad0 set to zero
       mark_.padw contains NAMES stack index
 */
Xpost_Object consname(context *ctx,
                char *s)
{
    unsigned u;
    unsigned t;
    Xpost_Object o;

    u = tstsearch(ctx->lo, adrent(ctx->lo, NAMET), s);
    if (!u) {
        u = tstsearch(ctx->gl, adrent(ctx->gl, NAMET), s);
        if (!u) {
            mfile *mem = ctx->vmmode==GLOBAL?ctx->gl:ctx->lo;
            mtab *tab = (void *)mem->base;
            t = tstinsert(mem, tab->tab[NAMET].adr, s);
            tab = (void *)mem->base; //recalc pointer
            tab->tab[NAMET].adr = t;
            u = addname(ctx, s); // obeys vmmode
            o.mark_.tag = nametype | (ctx->vmmode==GLOBAL?XPOST_OBJECT_TAG_DATA_FLAG_BANK:0);
            o.mark_.pad0 = 0;
            o.mark_.padw = u;
        } else {
            o.mark_.tag = nametype | XPOST_OBJECT_TAG_DATA_FLAG_BANK; // global
            o.mark_.pad0 = 0;
            o.mark_.padw = u;
        }
    } else {
        o.mark_.tag = nametype; // local
        o.mark_.pad0 = 0;
        o.mark_.padw = u;
        }
    return o;
}

/* adapter:
            string <- name
    yield the string object from the name string stack
    */
Xpost_Object strname(context *ctx,
               Xpost_Object n)
{
    mfile *mem = bank(ctx, n);
    unsigned names = adrent(mem, NAMES);
    Xpost_Object str = bot(mem, names, n.mark_.padw);
    //str.tag |= XPOST_OBJECT_TAG_DATA_FLAG_BANK;
    return str;
}

#ifdef TESTMODULE_NM
#include <stdio.h>
#include <unistd.h>

/*
void init(context *ctx) {
    pgsz = xpost_getpagesize();
    ctx->gl = malloc(sizeof(mfile));
    initmem(ctx->gl, "x.mem");
    (void)initmtab(ctx->gl); // create mtab at address zero
    //(void)mtalloc(ctx->gl, 0, 0, 0); //FREE
    initfree(ctx->gl);
    (void)mtalloc(ctx->gl, 0, 0, 0); //VS
    initctxlist(ctx->gl);

    initnames(ctx);
}
context ctx;
*/

context *ctx;

void init(void) {
    itpdata = malloc(sizeof*itpdata);
    memset(itpdata, 0, sizeof*itpdata);
    inititp(itpdata);
    ctx = &itpdata->ctab[0];
    ctx->vmmode = GLOBAL;
}

int main(void) {
    printf("\n^test nm\n");
    //init(&ctx);
    init();
    ctx->vmmode = LOCAL;

    printf("pop ");
    xpost_object_dump(consname(ctx, "pop"));
    printf("NAMES at %u\n", adrent(ctx->gl, NAMES));
    //dumpstack(ctx->gl, adrent(ctx->gl, NAMES)); puts("");

    printf("apple ");
    xpost_object_dump(consname(ctx, "apple"));
    xpost_object_dump(consname(ctx, "apple"));
    //printf("NAMES at %u\n", adrent(ctx->gl, NAMES));
    //dumpstack(ctx->gl, adrent(ctx->gl, NAMES)); puts("");

    printf("banana ");
    xpost_object_dump(consname(ctx, "banana"));
    xpost_object_dump(consname(ctx, "banana"));
    //printf("NAMES at %u\n", adrent(ctx->gl, NAMES));
    //dumpstack(ctx->gl, adrent(ctx->gl, NAMES)); puts("");

    printf("currant ");
    xpost_object_dump(consname(ctx, "currant"));
    xpost_object_dump(consname(ctx, "currant"));
    //printf("NAMES at %u\n", adrent(ctx->gl, NAMES));
    //dumpstack(ctx->gl, adrent(ctx->gl, NAMES)); puts("");

    printf("apple ");
    xpost_object_dump(consname(ctx, "apple"));
    printf("banana ");
    xpost_object_dump(consname(ctx, "banana"));
    printf("currant ");
    xpost_object_dump(consname(ctx, "currant"));
    printf("date ");
    //printf("NAMES at %u\n", adrent(ctx->gl, NAMES));
    xpost_object_dump(consname(ctx, "date"));
    //printf("NAMES at %u\n", adrent(ctx->gl, NAMES));
    dumpstack(ctx->gl, adrent(ctx->gl, NAMES)); puts("");
    //printf("NAMES at %u\n", adrent(ctx->gl, NAMES));
    printf("elderberry ");
    xpost_object_dump(consname(ctx, "elderberry"));

    printf("pop ");
    xpost_object_dump(consname(ctx, "pop"));

    //dumpmfile(ctx->gl);
    //dumpmtab(ctx->gl, 0);
    puts("");
    return 0;
}

#endif
