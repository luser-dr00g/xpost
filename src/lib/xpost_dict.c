/*
 * Xpost - a Level-2 Postscript interpreter
 * Copyright (C) 2013-2016, Michael Joshua Ryan
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

#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdlib.h> /* malloc */
#include <stdio.h>

#include "xpost.h"
#include "xpost_log.h"
#include "xpost_memory.h"  /* dicts live in the memory file, accessed via memory table */
#include "xpost_object.h"  /* dict is an object, containing objects */
#include "xpost_stack.h"  /* may need to count the save stack */
#include "xpost_free.h"  /* dicts are allocated from the free list */

#include "xpost_save.h"  /* dicts obey save/restore */
#include "xpost_context.h"
#include "xpost_error.h"  /* dict functions may throw errors */
#include "xpost_string.h"  /* may need string functions (convert to name) */
#include "xpost_name.h"  /* may need name functions (create name) */
#include "xpost_file.h"
#include "xpost_dict.h"  /* double-check prototypes */



/*
typedef struct {
    word tag;
    word sz;
    word nused;
    word pad;
} dichead;

typedef struct
{
    unsigned int hash;
    object key;
    objecy value;
} dicrec;
*/

/* strict-aliasing compatible poking of double */
typedef union
{
    unsigned long long bits;
    double             number;
} Xpost_Ieee_Double_As_Int;

Xpost_Object_Tag_Access xpost_dict_get_access(Xpost_Context *ctx, Xpost_Object d)
{
    Xpost_Memory_File *mem;
    unsigned int ad;
    dichead *dp;
    mem = xpost_context_select_memory(ctx, d);
    xpost_memory_table_get_addr(mem, xpost_object_get_ent(d), &ad);
    dp = (void *)(mem->base + ad);
    return (Xpost_Object_Tag_Access)((dp->tag & XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_MASK) >>
                                     XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_OFFSET);
}

Xpost_Object xpost_dict_set_access(Xpost_Context *ctx, Xpost_Object d, Xpost_Object_Tag_Access access)
{
    Xpost_Memory_File *mem;
    unsigned int ad;
    dichead *dp;
    mem = xpost_context_select_memory(ctx, d);
    xpost_memory_table_get_addr(mem, xpost_object_get_ent(d), &ad);
    dp = (dichead *)(mem->base + ad);
    dp->tag &= ~XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_MASK;
    dp->tag |= access << XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_OFFSET;
    d.tag &= ~XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_MASK;
    d.tag |= access << XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_OFFSET;
    return d;
}

/* Compare two objects for "equality".
   return 0 if "equal"
          +value if L > R
          -value if L < R
 */
int xpost_dict_compare_objects(Xpost_Context *ctx,
           Xpost_Object L,
           Xpost_Object R)
{
    /* fold nearly-comparable types to comparable */
    if (xpost_object_get_type(L) != xpost_object_get_type(R))
    {
        if (xpost_object_get_type(L) == integertype && xpost_object_get_type(R) == realtype)
        {
            L = xpost_real_cons((real)L.int_.val);
            goto cont;
        }
        if (xpost_object_get_type(R) == integertype && xpost_object_get_type(L) == realtype)
        {
            R = xpost_real_cons((real)R.int_.val);
            goto cont;
        }
        if (xpost_object_get_type(L) == nametype && xpost_object_get_type(R) == stringtype)
        {
            L = xpost_name_get_string(ctx, L);
            goto cont;
        }
        if (xpost_object_get_type(R) == nametype && xpost_object_get_type(L) == stringtype)
        {
            R = xpost_name_get_string(ctx, R);
            goto cont;
        }
        return xpost_object_get_type(L) - xpost_object_get_type(R);
    }

cont:
    switch (xpost_object_get_type(L))
    {
        default:
            XPOST_LOG_ERR("unhandled type (%s) in xpost_dict_compare_objects",
                    xpost_object_type_names[xpost_object_get_type(L)]);
            return -1;

        case marktype: return 0;
        case nulltype: return 0;
        case invalidtype: return 0;

        case booleantype: /*@fallthrough@*/
        case integertype: return L.int_.val - R.int_.val;

        case realtype: return (fabs(L.real_.val - R.real_.val) < 0.0001)?
                                0:
                                L.real_.val - R.real_.val > 0? 1: -1;
        case extendedtype:
        {
            double l,r;
            l = xpost_dict_convert_extended_to_double(L);
            r = xpost_dict_convert_extended_to_double(R);
            return (fabs(l - r) < 0.0001) ? 0 : l - r > 0? 1: -1;
        }

        case operatortype:  return L.mark_.padw - R.mark_.padw;

        case nametype: return (L.tag&XPOST_OBJECT_TAG_DATA_FLAG_BANK)==(R.tag&XPOST_OBJECT_TAG_DATA_FLAG_BANK)?
                                (signed)(L.mark_.padw - R.mark_.padw):
                                    (signed)((L.tag&XPOST_OBJECT_TAG_DATA_FLAG_BANK) - (R.tag&XPOST_OBJECT_TAG_DATA_FLAG_BANK));

        case dicttype: /*@fallthrough@*/ /*return !( xpost_object_get_ent(L) == xpost_object_get_ent(R) ); */
        case arraytype: return !( L.comp_.sz == R.comp_.sz
                                && (L.tag&XPOST_OBJECT_TAG_DATA_FLAG_BANK) == (R.tag&XPOST_OBJECT_TAG_DATA_FLAG_BANK)
                                && xpost_object_get_ent(L) == xpost_object_get_ent(R)
                                && L.comp_.off == R.comp_.off ); /* 0 if all eq */

        case stringtype: return L.comp_.sz == R.comp_.sz ?
                                memcmp(xpost_string_get_pointer(ctx, L), xpost_string_get_pointer(ctx, R), L.comp_.sz) :
                                L.comp_.sz - R.comp_.sz;
        case filetype: return xpost_file_get_file_pointer(ctx->lo, L) == xpost_file_get_file_pointer(ctx->lo, R);
    }
}

/* more like scrambled eggs */
static
unsigned int hash(Xpost_Object k)
{
    unsigned int h;
    h = ( (xpost_object_get_type(k)
            | (k.comp_.tag & XPOST_OBJECT_TAG_DATA_FLAG_BANK))
            << 1) /* ignore flags (except BANK!) */
        + (k.comp_.sz << 3)
        + (xpost_object_get_ent(k) << 7)
        + (k.comp_.off << 5);
    /* h = xpost_object_get_type(k); /\* test collisions. *\/ */
#ifdef DEBUGDIC
    printf("\nhash(");
    xpost_object_dump(k);
    printf(")=%u", h);
#endif
    return h;
}

/*
   Allocate a dictionary in the specified memory file.

   allocate an entity with xpost_memory_table_alloc,
   set the save level in the mark,
   extract the "pointer" from the entity,
   Initialize a dichead in memory,
   just after the head, clear a table of pairs. */
Xpost_Object xpost_dict_cons_memory (Xpost_Memory_File *mem,
               unsigned int sz)
{
    Xpost_Memory_Table *tab;
    Xpost_Object d;
    unsigned int rent;
    unsigned int cnt;
    unsigned int ad;
    dichead *dp;
    dicrec *tp;
    unsigned int i;
    unsigned int vs;
    unsigned int ent;
    unsigned int hashnull;

    if (sz < 8) sz = 8;
    sz = (unsigned int)ceil((double)sz * 1.25);

    assert(mem->base);
    d.tag = dicttype | (XPOST_OBJECT_TAG_ACCESS_UNLIMITED << XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_OFFSET);
    d.comp_.sz = sz;
    d.comp_.off = 0;
    if (!xpost_memory_table_alloc(mem, sizeof(dichead) + DICTABSZ(sz), dicttype, &ent))
    {
        XPOST_LOG_ERR("cannot allocate dictionary");
        return null;
    }
    //d.comp_.ent = ent;
    d = xpost_object_set_ent(d, ent);

    tab = &mem->table;
    rent = ent;
    xpost_memory_table_get_addr(mem, XPOST_MEMORY_TABLE_SPECIAL_SAVE_STACK, &vs);
    cnt = xpost_stack_count(mem, vs);
    tab->tab[rent].mark = ( (0 << XPOST_MEMORY_TABLE_MARK_DATA_MARK_OFFSET)
            | (0 << XPOST_MEMORY_TABLE_MARK_DATA_REFCOUNT_OFFSET)
            | (cnt << XPOST_MEMORY_TABLE_MARK_DATA_LOWLEVEL_OFFSET)
            | (cnt << XPOST_MEMORY_TABLE_MARK_DATA_TOPLEVEL_OFFSET) );

    xpost_memory_table_get_addr(mem, ent, &ad);
    dp = (void *)(mem->base + ad); /* clear header */
    dp->tag = d.tag;
    dp->sz = sz;
    dp->nused = 0;
    dp->pad = 0;

    tp = (void *)(mem->base + ad + sizeof(dichead)); /* clear table */
    hashnull = hash(null);
    for (i=0; i < DICTABN(sz); i++){
        tp[i].hash = hashnull;
        tp[i].key = null; /* remember our null object is not all-zero! */
        tp[i].value = null;
    }
#ifdef DEBUGDIC
    printf("xpost_dict_cons_memory : "); xpost_dict_dump_memory (mem, d);
#endif
    return d;
}

/*
   Allocate a dictionary in the currently active memory.

   select the memory file according to vmmode,
   call xpost_dict_cons_memory ,
   set the BANK flag. */
Xpost_Object xpost_dict_cons (Xpost_Context *ctx,
               unsigned int sz)
{
    Xpost_Object d = xpost_dict_cons_memory (ctx->vmmode==GLOBAL? ctx->gl: ctx->lo, sz);
    if (xpost_object_get_type(d) != nulltype)
    {
        if (ctx->vmmode == GLOBAL)
            d.tag |= XPOST_OBJECT_TAG_DATA_FLAG_BANK;
        xpost_stack_push(ctx->lo, ctx->hold, d); /* stash a reference on the hold stack in case of gc in caller */
    }
    return d;
}

/* get the nused field from the dichead */
unsigned int xpost_dict_length_memory (Xpost_Memory_File *mem,
                   Xpost_Object d)
{
    unsigned int da;
    dichead *dp;
    xpost_memory_table_get_addr(mem, xpost_object_get_ent(d), &da);
    dp = (void *)(mem->base + da);
    return dp->nused;
}

/* get the sz field from the dichead */
unsigned int xpost_dict_max_length_memory (Xpost_Memory_File *mem,
                      Xpost_Object d)
{
    unsigned int da;
    dichead *dp;
    xpost_memory_table_get_addr(mem, xpost_object_get_ent(d), &da);
    dp = (void *)(mem->base + da);
    return dp->sz;
}

/*
   grow a dictionary to a larger size.

   allocate a new dictionary,
   copy over all non-null key/value pairs,
   swap adrs in the two table slots. */
static
int dicgrow(Xpost_Context *ctx,
             Xpost_Object d)
{
    Xpost_Memory_File *mem;
    unsigned int sz;
    unsigned int newsz;
    unsigned int ad;
    dichead *dp;
    dicrec *tp;
    Xpost_Object n;
    unsigned int i;

    xpost_stack_push(ctx->lo, ctx->hold, d);
    mem = xpost_context_select_memory(ctx, d);
#ifdef DEBUGDIC
    printf("DI growing dict\n");
    xpost_dict_dump_memory (mem, d);
#endif
    n = xpost_dict_cons_memory (mem, newsz = 2 * xpost_dict_max_length_memory (mem, d));
    if (xpost_object_get_type(n) == nulltype){
        XPOST_LOG_ERR("cannot grow dict");
        return 0;
    }
    if (mem == ctx->gl)
        n.tag |= XPOST_OBJECT_TAG_DATA_FLAG_BANK;

    xpost_memory_table_get_addr(mem, xpost_object_get_ent(d), &ad);
    dp = (void *)(mem->base + ad);
    sz = DICTABN(dp->sz);
    tp = (void *)(mem->base + ad + sizeof(dichead)); /* copy data */
    for (i = 0; i < sz; i++)
    {
        if (xpost_object_get_type(tp[i].key) != nulltype)
        {
            xpost_dict_put_memory(ctx, mem, n, tp[i].key, tp[i].value);
        }
    }
#ifdef DEBUGDIC
    printf("n: ");
    xpost_dict_dump_memory (mem, n);
#endif

    {   /* exchange entities */
        Xpost_Memory_Table *tab = &mem->table;
        unsigned int dent, nent;
        unsigned int hold;

        dent = xpost_object_get_ent(d);
        nent = xpost_object_get_ent(n);

        /* exchange adrs */
        hold = tab->tab[dent].adr;
               tab->tab[dent].adr = tab->tab[nent].adr;
                                    tab->tab[nent].adr = hold;

        /* exchange sizes */
        hold = tab->tab[dent].sz;
               tab->tab[dent].sz = tab->tab[nent].sz;
                                   tab->tab[nent].sz = hold;

#if 0
        if (xpost_free_memory_ent(mem, nent) < 0)
        {
            XPOST_LOG_ERR("cannot free old dict");
            return 0;
        }
#endif
    }
    return 1;
}

/* is it full? (y/n) */
int xpost_dict_is_full_memory (Xpost_Memory_File *mem,
             Xpost_Object d)
{
    return xpost_dict_length_memory (mem, d) == xpost_dict_max_length_memory (mem, d);
}

/* print a dump of the dictionary data */
void xpost_dict_dump_memory (Xpost_Memory_File *mem,
             Xpost_Object d)
{
    unsigned int ad;
    dichead *dp;
    dicrec *tp;
    unsigned int sz;
    unsigned int i;

    xpost_memory_table_get_addr(mem, xpost_object_get_ent(d), &ad);
    dp = (void *)(mem->base + ad);
    tp = (void *)(mem->base + ad + sizeof(dichead));
    sz = DICTABN(dp->sz);

    printf("\n");
    for (i = 0; i < sz; i++)
    {
        printf("%u:", i);
        if (xpost_object_get_type(tp[i].key) != nulltype)
        {
            xpost_object_dump(tp[i].key);
        }
    }
}

/* construct an extendedtype object
   from a double value */
/*n.b. Caller Must set EXTENDEDINT or EXTENDEDREAL flag */
/*     in order to xpost_dict_convert_extended_to_number() later. */
static
Xpost_Object consextended (double d)
{
    Xpost_Ieee_Double_As_Int r;
    Xpost_Object o;

    r.number = d;
    o.extended_.tag = extendedtype;
    o.extended_.sign_exp = (r.bits >> 52) & 0x7FF;
    o.extended_.fraction = (r.bits >> 20) & 0xFFFFFFFF;
    return o;
}

/* adapter:
   double <- extendedtype object */
double xpost_dict_convert_extended_to_double (Xpost_Object e)
{
    Xpost_Ieee_Double_As_Int r;
    r.bits = ((unsigned long long)e.extended_.sign_exp << 52)
             | ((unsigned long long)e.extended_.fraction << 20);
    return r.number;
}

/* convert an extendedtype object to integertype or realtype
   depending upon flag */
Xpost_Object xpost_dict_convert_extended_to_number (Xpost_Object e)
{
    Xpost_Object o;
    double d = xpost_dict_convert_extended_to_double(e);

    if (e.tag & XPOST_OBJECT_TAG_DATA_EXTENDED_INT)
    {
        o = xpost_int_cons((integer)d);
    }
    else if (e.tag & XPOST_OBJECT_TAG_DATA_EXTENDED_REAL)
    {
        o = xpost_real_cons((real)d);
    }
    else
    {
        XPOST_LOG_ERR("invalid extended number object");
        return null;
    }
    return o;
}

/* make key the proper type for hashing */
static
Xpost_Object clean_key (Xpost_Context *ctx,
                        Xpost_Object k)
{
    switch(xpost_object_get_type(k))
    {
        default: break;
        case stringtype:
        {
            char *s = alloca(k.comp_.sz+1);
            memcpy(s, xpost_string_get_pointer(ctx, k), k.comp_.sz);
            s[k.comp_.sz] = '\0';
            k = xpost_name_cons(ctx, s);
            break;
        }
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
    if (xpost_object_get_type(tp[i].key) == nulltype \
        || (hashval == tp[i].hash \
            && xpost_dict_compare_objects(ctx, tp[i].key, k) == 0)) \
        return tp + i

static dicrec invalidrec[] = {{ 0, {0}, {0}}};

/* perform a hash-assisted lookup.
   returns a pointer to the desired pair (if found)), or a null-pair. */
/*@dependent@*/ /*@null@*/
static
dicrec *diclookup(Xpost_Context *ctx,
        /*@dependent@*/ Xpost_Memory_File *mem,
        Xpost_Object d,
        Xpost_Object k)
{
    unsigned int ad;
    dichead *dp;
    dicrec *tp;
    unsigned int sz;
    unsigned int hashval;
    unsigned int h;
    unsigned int i;

    k = clean_key(ctx, k);
    if (xpost_object_get_type(k) == invalidtype)
        return invalidrec;

    if (!xpost_memory_table_get_addr(mem, xpost_object_get_ent(d), &ad))
        return invalidrec;
    dp = (void *)(mem->base + ad);
    tp = (void *)(mem->base + ad + sizeof(dichead));
    sz = (dp->sz + 1);

    hashval = hash(k);
    h = hashval % sz;
    i = h;
#ifdef DEBUGDIC
    printf("diclookup(");
    xpost_object_dump(k);
    printf(");");
    printf("%%%u=%u",sz, h);
#endif

    RETURN_TAB_I_IF_EQ_K_OR_NULL;
    for (++i; i < sz; i++)
    {
        RETURN_TAB_I_IF_EQ_K_OR_NULL;
    }
    for (i = 0; i < h; i++)
    {
        RETURN_TAB_I_IF_EQ_K_OR_NULL;
    }
    return NULL; /* i == h : dict is overfull: no null entry */
}

/* see if lookup returns a non-null pair. */
int xpost_dict_known_key(Xpost_Context *ctx,
                         /*@dependent@*/ Xpost_Memory_File *mem,
                         Xpost_Object d,
                         Xpost_Object k)
{
    dicrec *r;

    r = diclookup(ctx, mem, d, k);
    if (r == NULL) return 0;
    if (r == invalidrec) return 0;
    return xpost_object_get_type(r->key) != nulltype;
}

/*
   Get value from dict+key with specified memory file
   (dict must be valid for this memory file)

   call diclookup,
   return the value if the key is non-null
   or invalid if key is null (interpret as "undefined"). */
Xpost_Object xpost_dict_get_memory (Xpost_Context *ctx,
        /*@dependent@*/ Xpost_Memory_File *mem,
        Xpost_Object d,
        Xpost_Object k)
{
    dicrec *r;

    r = diclookup(ctx, mem, d, k);
    if (r == invalidrec){
        XPOST_LOG_ERR("warning: invalid key\n");
        return invalid;
    }
    if (r == NULL || xpost_object_get_type(r->key) == nulltype)
    {
        return invalid;
    }
    else if (xpost_object_get_type(r->value) == magictype)
    {
        Xpost_Object ret;
        r->value.magic_.pair->get(ctx, d, k, &ret);
        return ret;
    }
    return r->value;
}

/*
   Get value from dict+key.

   select the memory file according to BANK field,
   call xpost_dict_get_memory . */
Xpost_Object xpost_dict_get(Xpost_Context *ctx,
        Xpost_Object d,
        Xpost_Object k)
{
    return xpost_dict_get_memory (ctx, xpost_context_select_memory(ctx, d), d, k);
}

/*
   Put key+value in dict with specified memory file.
   (dict must be valid for this memory file)

   save data if not save at this level,
   lookup the key,
   if key is null, check if the dict is full,
       increase nused,
       set key,
       update value. */
int xpost_dict_put_memory(Xpost_Context *ctx,
        Xpost_Memory_File *mem,
        Xpost_Object d,
        Xpost_Object k,
        Xpost_Object v)
{
    dicrec *r;
    dichead *dp;
    unsigned int ad;
    int ret;

    if (!ctx->gl->interpreter_get_initializing())
        if (!xpost_object_is_writeable(ctx, d))
            return invalidaccess;

    if (!xpost_save_ent_is_saved(mem, xpost_object_get_ent(d)))
        if (!xpost_save_save_ent(mem, dicttype, 0, xpost_object_get_ent(d)))
            return VMerror;

    r = diclookup(ctx, mem, d, k);

    if (r == invalidrec){
        XPOST_LOG_ERR("warning: invalid key\n");
        return VMerror;
    }
    if (r == NULL)
    {
        /* dict overfull:  grow dict! */
        ret = dicgrow(ctx, d);
        if (!ret)
            return VMerror;

        r = diclookup(ctx, mem, d, k);
        if (r == NULL)
            return VMerror;
    }
    else if (xpost_object_get_type(r->key) == invalidtype)
    {
        XPOST_LOG_ERR("warning: invalidtype key in dict\n");
        r->key = null;
    }
    else if (xpost_object_get_type(r->key) == nulltype)
    {
        if (xpost_dict_is_full_memory (mem, d))
        {
            /* dict full:  grow dict! */
            ret = dicgrow(ctx, d);
            if (!ret)
                return VMerror;

            r = diclookup(ctx, mem, d, k);

            if (r == NULL)
                return VMerror;
        }

        xpost_memory_table_get_addr(mem, xpost_object_get_ent(d), &ad);
        dp = (void *)(mem->base + ad);
        ++ dp->nused;
        r->key = clean_key(ctx, k);
        r->hash = hash(r->key);
        if (xpost_object_get_type(r->key) == invalidtype)
            return VMerror;
    }
    else if (xpost_object_get_type(r->value) == magictype)
    {
        r->value.magic_.pair->put(ctx, d, k, v);
        return 0;
    }
    r->value = v;
    return 0;
}

/*
   Put key+value in dict.

   select the memory file according to BANK field,
   call xpost_dict_put_memory. */
int xpost_dict_put(Xpost_Context *ctx,
        Xpost_Object d,
        Xpost_Object k,
        Xpost_Object v)
{
    Xpost_Memory_File *mem = xpost_context_select_memory(ctx, d);
    if (!ctx->ignoreinvalidaccess)
    {
        if ( mem == ctx->gl
                && xpost_object_is_composite(k)
                && mem != xpost_context_select_memory(ctx, k))
        {
            XPOST_LOG_ERR("local key into global dict");
            return invalidaccess;
        }
        if ( mem == ctx->gl
                && xpost_object_is_composite(v)
                && mem != xpost_context_select_memory(ctx, v))
        {
            xpost_object_dump(v);
            XPOST_LOG_ERR("local value into global dict");
            return invalidaccess;
        }
    }
    xpost_stack_push(ctx->lo, ctx->hold, d);
    xpost_stack_push(ctx->lo, ctx->hold, k);
    xpost_stack_push(ctx->lo, ctx->hold, v);

    return xpost_dict_put_memory(ctx, xpost_context_select_memory(ctx, d), d, k, v);
}

/* undefine key from dict */
int xpost_dict_undef_memory(Xpost_Context *ctx,
        Xpost_Memory_File *mem,
        Xpost_Object d,
        Xpost_Object k)
{
    dicrec *e;
    unsigned int ad;
    dichead *dp;
    dicrec *tp;
    unsigned int sz;
    unsigned int hashval;
    unsigned int h;
    unsigned int i;
    unsigned int last = 0;
    int lastisset = 0;
    int found = 0;

    if (!xpost_save_ent_is_saved(mem, xpost_object_get_ent(d)))
        if (!xpost_save_save_ent(mem, dicttype, 0, xpost_object_get_ent(d)))
            return VMerror;

    xpost_memory_table_get_addr(mem, xpost_object_get_ent(d), &ad);
    dp = (void *)(mem->base + ad);
    tp = (void *)(mem->base + ad + sizeof(dichead));

    k = clean_key(ctx, k);
    if (xpost_object_get_type(k) == invalidtype)
        return VMerror;

    e = diclookup(ctx, mem, d, k); /*find slot for key */
    if (e == NULL || e == invalidrec || xpost_dict_compare_objects(ctx,e->key,null) == 0)
    {
        return undefined;
    }

    /*find last chained key and value with same hash */
    /*FIXME: need to repeat this process with this 'last chained key with same hash'
      until the key we're clearing is the actual last key in the chain, recursively. */
    sz = DICTABN(dp->sz);
    hashval = hash(k);
    h = hashval % sz;

    for (i = h; i < sz; i++)
    {
        if (h == hash(tp[i].key) % sz)
        {
            last = i;
            lastisset = 1;
        }
        else if (xpost_dict_compare_objects(ctx, tp[i].key, null) == 0)
        {
            if (lastisset)
            {
                found = 1;
                break;
            }
        }
    }

    if (!found)
    {
        for (i = 0; i < h; i++)
        {
            if (h == hash(tp[i].key) % sz)
            {
                last = i;
                lastisset = 1;
            }
            else if (xpost_dict_compare_objects(ctx, tp[i].key, null) == 0)
            {
                if (lastisset)
                {
                    found = 1;
                    break;
                }
            }
        }
    }

    if (found) /* f found: move last key and value to slot */
    {
        e->key = tp[last].key;
        e->value = tp[last].value;
        tp[last].key = null;
        tp[last].hash = hash(tp[last].key);
        tp[last].value = null;
    }
    else /* ot found: write null over key and value */
    {
        e->key = null;
        tp[last].hash = hash(tp[last].key);
        e->value = null;
    }

    return 0;
}

/* undefine key from banked dict */
void xpost_dict_undef(Xpost_Context *ctx,
        Xpost_Object d,
        Xpost_Object k)
{
    xpost_dict_undef_memory(ctx, xpost_context_select_memory(ctx, d), d, k);
}


#ifdef TESTMODULE_DI
#include <stdio.h>

/*Xpost_Context ctx; */
Xpost_Context *ctx;

void init()
{
    /*xpost_context_init(&ctx); */
    itpdata=malloc(sizeof*itpdata);
    xpost_interpreter_init(itpdata);
    ctx = &itpdata->ctab[0];
}

int main(void)
{
    if (!xpost_init())
    {
        fprintf(stderr, "Fail to initialize xpost dict test\n");
        return -1;
    }

    printf("\n^test di.c\n");
    init();

    Xpost_Object d;
    d = xpost_dict_cons (ctx, 12);
    printf("1 2 def\n");
    xpost_dict_put(ctx, d, xpost_int_cons(1), xpost_int_cons(2));
    printf("3 4 def\n");
    xpost_dict_put(ctx, d, xpost_int_cons(3), xpost_int_cons(4));

    printf("1 load =\n");
    xpost_object_dump(xpost_dict_get(ctx, d, xpost_int_cons(1)));
    /* xpost_object_dump(xpost_dict_get(ctx, d, xpost_int_cons(2)));  */
    printf("\n3 load =\n");
    xpost_object_dump(xpost_dict_get(ctx, d, xpost_int_cons(3)));


    /*xpost_memory_file_dump(ctx->gl); */
    /*dumpmtab(ctx->gl, 0); */
    puts("");

    xpost_quit();

    return 0;
}

#endif
