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

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "xpost.h"
#include "xpost_log.h"
#include "xpost_memory.h" /* Xpost_Memory_File */
#include "xpost_object.h" /* Xpost_Object */
#include "xpost_free.h"

/*
   initialize the free-list in the memory file.
   free list head is in slot zero
   sz is 0 so gc will ignore it */
/* collection threshold in allocated bytes; overridable for testing
   and for embedders that want more frequent collections */
static int _xpost_free_gc_threshold(void)
{
    static int v = -1;
    if (v < 0)
    {
        const char *e = getenv("XPOST_GC_THRESHOLD");
        v = e ? atoi(e) : 0;
        if (v <= 0)
            v = XPOST_GARBAGE_COLLECTION_THRESHOLD;
    }
    return v;
}

/* The free list is segregated into size-class buckets so that both
   freeing and allocation are near-constant-time: a single sorted list
   makes every operation walk the entities smaller than the request,
   which dominates once a large collection has populated the list.
   Bucket b holds entities with size in [2^(b+4), 2^(b+5)), clamped to
   the first and last buckets. The head words live in the FREE special
   entity's data area. */
#define XPOST_FREE_NBUCKETS 16

static unsigned int _xpost_free_bucket(unsigned int sz)
{
    unsigned int b = 0;
    unsigned int s = sz >> 5;
    while (s && b < XPOST_FREE_NBUCKETS - 1)
    {
        s >>= 1;
        b++;
    }
    return b;
}

int xpost_free_init(Xpost_Memory_File *mem)
{
    unsigned int ent;
    unsigned int val = 0;
    int ret;

    /* allocate the free list head: 4 bytes in ent 0
       allocate additional 1k "scratch" space to protect
       interpreter data from NULL writes
     */
    ret = xpost_memory_table_alloc(mem, 1024, 0, &ent);
    if (!ret)
    {
        return 0;
    }

    /* make sure this is the correct ent */
    assert (ent == XPOST_MEMORY_TABLE_SPECIAL_FREE);

    /* set all bucket heads to zero (== NULL == end of list) */
    {
        unsigned int b;
        for (b = 0; b < XPOST_FREE_NBUCKETS; b++)
        {
            ret = xpost_memory_put(mem, ent, b * sizeof(unsigned int),
                                   sizeof(unsigned int), &val);
            if (!ret)
            {
                XPOST_LOG_ERR("xpost_free_init cannot access list head");
                return 0;
            }
        }
    }

    /* set zero size to enable guards against NULL writes */
    {
        Xpost_Memory_Table *tab = &mem->table;
        tab->tab[XPOST_MEMORY_TABLE_SPECIAL_FREE].sz = 0;
    }

    /* make free list available for general memory allocations */
    (void) xpost_memory_register_free_list_alloc_function(mem, xpost_free_alloc);
    mem->period = XPOST_GARBAGE_COLLECTION_PERIOD;
    mem->threshold = _xpost_free_gc_threshold();

    return 1;
}

/* free this ent! returns reclaimed size or -1 on error */
int xpost_free_memory_ent(Xpost_Memory_File *mem,
                          unsigned int ent)
{
    Xpost_Memory_Table *tab;
    unsigned int rent = ent; /* relative ent index */
    unsigned int z; /* free list pointer */
    unsigned int a; /* adr associated with ent */
    unsigned int sz; /* sz associated with adr */
    int ret;
    /* return; */

    if (ent < mem->start)
        return 0;

    if (ent >= mem->table.nextent)
    {
        XPOST_LOG_ERR("cannot free ent %u", ent);
        return -1;
    }
    tab = &mem->table;
    a = tab->tab[rent].adr;
    sz = tab->tab[rent].sz;
    if (sz == 0) return 0; /* do not add zero-size allocations to list */

    if (tab->tab[rent].tag == filetype)
    {
        FILE *fp;
        ret = xpost_memory_get(mem, ent, 0, sizeof(FILE *), &fp);
        if (!ret)
        {
            XPOST_LOG_ERR("cannot load FILE* from VM");
            return -1;
        }
        if (fp &&
            fp != stdin &&
            fp != stdout &&
            fp != stderr)
        {
            tab->tab[rent].tag = 0;
#ifdef DEBUG_FILE
            printf("gc:xpost_free_memory_ent closing FILE* %p\n", fp);
            fflush(stdout);
            /* if (fp < 0x1000) return 0; */
            printf("fclose");
#endif
            fclose(fp);
            fp = NULL;
            ret = xpost_memory_put(mem, ent, 0, sizeof(FILE *), &fp);
            if (!ret)
            {
                XPOST_LOG_ERR("cannot write NULL over FILE* in VM");
                return -1;
            }
        }
    }
    tab->tab[rent].tag = 0;

    ret = xpost_memory_table_get_addr(mem, XPOST_MEMORY_TABLE_SPECIAL_FREE, &z);
    if (!ret)
    {
        XPOST_LOG_ERR("unable to load free list head");
        return -1;
    }
    z += _xpost_free_bucket(sz) * sizeof(unsigned int);

    /* push onto the bucket: link word lives in the ent's data area */
    memcpy(mem->base + a, mem->base + z, sizeof(unsigned int));
    memcpy(mem->base + z, &ent, sizeof(unsigned int));

    return sz;
}

static void _dump_chain(Xpost_Memory_File *mem, unsigned int z)
{
    unsigned int e;
    memcpy(&e, mem->base + z, sizeof(unsigned int));
    while (e)
    {
        unsigned int sz;
        if (!xpost_memory_table_get_size(mem, e, &sz)) return;
        printf("%u(%u) ", e, sz);
        if (!xpost_memory_table_get_addr(mem, e, &z)) return;
        memcpy(&e, mem->base + z, sizeof(unsigned int));
    }
}

/* print a dump of the free list */
void xpost_free_dump(Xpost_Memory_File *mem)
{
    unsigned int e;
    unsigned int z;
    int ret;

    ret = xpost_memory_table_get_addr(mem, XPOST_MEMORY_TABLE_SPECIAL_FREE, &z);
    if (!ret)
    {
        return;
    }

    printf("freelist: ");
    {
        unsigned int b, headz = z;
        for (b = 0; b < XPOST_FREE_NBUCKETS; b++)
        {
            z = headz + b * sizeof(unsigned int);
            memcpy(&e, mem->base + z, sizeof(unsigned int));
            if (e) printf("[bucket %u] ", b);
            _dump_chain(mem, z);
        }
    }
    return;
    memcpy(&e, mem->base + z, sizeof(unsigned int));
    while (e)
    {
        unsigned int sz;
        ret = xpost_memory_table_get_size(mem, e, &sz);
        if (!ret)
        {
            return;
        }
        printf("%u(%u) ", e, sz);
        ret = xpost_memory_table_get_addr(mem, e, &z);
        if (!ret)
        {
            return;
        }
        memcpy(&e, mem->base + z, sizeof(unsigned int));
    }
}

/* scan the free list for a suitably-sized bit of memory,

   if the allocator falls back to fresh memory XPOST_GARBAGE_COLLECTION_PERIOD times,
        it triggers a collection.
    Returns 1 on success, 0 on failure, 2 to request garbage collection and re-call.
 */
int xpost_free_alloc(Xpost_Memory_File *mem,
                     unsigned int sz,
                     unsigned int tag,
                     unsigned int *entity)
{
    unsigned int z;
    unsigned int e;                     /* working pointer */
    //static int period = XPOST_GARBAGE_COLLECTION_PERIOD;
    //static int threshold = XPOST_GARBAGE_COLLECTION_THRESHOLD;
    int ret;

    if (!mem->interpreter_get_initializing())
    {
#ifdef XPOST_USE_THRESHOLD
        if ((mem->threshold -= sz) <= 0)
        {
            mem->threshold = _xpost_free_gc_threshold();
            return 2;
        }
#else
        //(void)threshold;
        if (--mem->period == 0) /* check garbage-collection control */
        {
            mem->period = XPOST_GARBAGE_COLLECTION_PERIOD;
            return 2; /* not found, request garbage-collection and try-again */
            /* collect(mem, 1, 0); */
            /* goto try_again; */
        }
#endif
    }

    ret = xpost_memory_table_get_addr(mem, XPOST_MEMORY_TABLE_SPECIAL_FREE, &z); /* free pointer */
    if (!ret)
    {
        XPOST_LOG_ERR("unable to load free list head");
        return 0;
    }

    {
    unsigned int b;
    unsigned int headz = z;

    for (b = _xpost_free_bucket(sz); b < XPOST_FREE_NBUCKETS; b++)
    {
        unsigned int best = 0, bestz = 0, bestsz = 0;

        z = headz + b * sizeof(unsigned int);
        memcpy(&e, mem->base + z, sizeof(unsigned int));
        while (e) /* e is not zero */
        {
            unsigned int tsz;
            unsigned int ta;
            /* The links live inside the freed entities' data, where a stale
               write can turn one into an arbitrary number. Handing out an
               entity that is not actually free aliases two owners onto one
               allocation, so validate every node: freed entities carry a
               zero tag. On any inconsistency discard the lists and request
               a collection to rebuild them. */
            if (e > XPOST_OBJECT_COMP_MAX_ENT ||
                e >= mem->table.nextent ||
                mem->table.tab[e].tag != 0)
            {
                unsigned int zero = 0;
                unsigned int bb;
                XPOST_LOG_ERR("free list corrupt at ent %u (tag %u): discarding",
                        e, e < mem->table.nextent ? mem->table.tab[e].tag : 0);
                for (bb = 0; bb < XPOST_FREE_NBUCKETS; bb++)
                    xpost_memory_put(mem, 0, bb * sizeof(unsigned int),
                                     sizeof zero, &zero);
                return 2; /* request collection to fill the list */
            }
            ret = xpost_memory_table_get_size(mem, e, &tsz);
            if (!ret)
            {
                XPOST_LOG_ERR("cannot retrieve size of ent %u", e);
                return 0;
            }

            /* best fit within the request's own bucket: entity numbers
               are a fixed budget, so near-exact recycling matters more
               than the walk. An exact fit ends the search; any fit from
               a higher bucket is taken as-is (the byte waste is
               reclaimable by a later collection, a consumed entity
               number is not). */
            if (tsz >= sz && (best == 0 || tsz < bestsz))
            {
                best = e;
                bestz = z;
                bestsz = tsz;
                if (tsz == sz)
                    break;
            }

            ret = xpost_memory_table_get_addr(mem, e, &ta);
            if (!ret)
            {
                XPOST_LOG_ERR("cannot retrieve address for ent %u", e);
                return 0;
            }
            z = ta;
            memcpy(&e, mem->base + z, sizeof(unsigned int));
        }

        if (best)
        {
            Xpost_Memory_Table *tab = &mem->table;
            unsigned int ad;

            ret = xpost_memory_table_get_addr(mem, best, &ad);
            if (!ret)
            {
                XPOST_LOG_ERR("cannot retrieve address of ent %u", best);
                return 0;
            }
            /* unlink: the predecessor link slot was recorded when the
               node was reached */
            memcpy(mem->base + bestz, mem->base + ad, sizeof(unsigned int));
            tab->tab[best].tag = tag;
            *entity = best;
            return 1; /* found, return SUCCESS */
        }
    }
    }
    /* finished scanning free list */

    return 0; /* not found, fall-back to _new allocator */
}

/*
   use the free-list and tables to now provide a realloc for
   "raw" vm addresses (mem->base offsets rather than ents).

   Allocate new entry, copy data, steal its adr, stash old adr, free it.

   Currently this is only used to re-size signature blocks in the operator table.
 */
unsigned int xpost_free_realloc(Xpost_Memory_File *mem,
                                unsigned int oldadr,
                                unsigned int oldsize,
                                unsigned int newsize)
{
    Xpost_Memory_Table *tab = NULL;
    unsigned int newadr;
    unsigned int ent;
    unsigned int rent; /* relative ent */
    int ret;

#ifdef DEBUGFREE
    printf("xpost_free_realloc: ");
    printf("initial ");
    xpost_free_dump(mem);
#endif

    /* allocate new entry */
    ret = xpost_memory_table_alloc(mem, newsize, 0, &ent);
    if (!ret)
    {
        XPOST_LOG_ERR("cannot allocate new memory");
        return 0;
    }
    rent = ent;
    tab = &mem->table;
    if (ent >= mem->table.nextent)
    {
        XPOST_LOG_ERR("cannot find table for ent %u", ent);
        return 0;
    }

    /* steal its adr */
    newadr = tab->tab[rent].adr;

    /* copy data */
    memcpy(mem->base + newadr, mem->base + oldadr, oldsize);

    /* stash old adr */
    tab->tab[rent].adr = oldadr;
    tab->tab[rent].sz = oldsize;

    /* free it */
    (void) xpost_free_memory_ent(mem, ent);

#ifdef DEBUGFREE
    printf("final ");
    xpost_free_dump(mem);
    printf("\n");
    dumpmtab(mem, 0);
    fflush(NULL);
#endif

    return newadr;
}
