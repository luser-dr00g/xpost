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

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "xpost_log.h"
#include "xpost_memory.h" /* Xpost_Memory_File */
#include "xpost_object.h" /* Xpost_Object */
#include "xpost_context.h"
#include "xpost_error.h"
#include "xpost_garbage.h" /* PERIOD */
#include "xpost_free.h"

/* free list head is in slot zero
   sz is 0 so gc will ignore it */
int xpost_free_init(Xpost_Memory_File *mem)
{
    unsigned ent;
    unsigned val = 0;
    int ret;

    ret = xpost_memory_table_alloc(mem, sizeof(unsigned), 0, &ent);
    if (!ret)
    {
        return 0;
    }
    assert (ent == XPOST_MEMORY_TABLE_SPECIAL_FREE);
    ret = xpost_memory_put(mem, ent, 0, sizeof(unsigned), &val);
    if (!ret)
    {
        XPOST_LOG_ERR("xpost_free_init cannot access list head");
        return 0;
    }

    /*
       unsigned ent;
       xpost_memory_table_alloc(mem, 0, 0, &ent);
       Xpost_Memory_Table *tab = (void *)mem->base;
       xpost_memory_file_alloc(mem, sizeof(unsigned), &tab->tab[ent].adr);
   */
    return 1;
}

/* free this ent! returns reclaimed size */
unsigned xpost_free_memory_ent(Xpost_Memory_File *mem,
        unsigned ent)
{
    Xpost_Memory_Table *tab;
    unsigned rent = ent;
    unsigned a;
    unsigned z;
    unsigned sz;
    /* return; */

    if (ent < mem->start)
        return 0;

    xpost_memory_table_find_relative(mem, &tab, &rent);
    a = tab->tab[rent].adr;
    sz = tab->tab[rent].sz;
    if (sz == 0) return 0;

    if (tab->tab[rent].tag == filetype) {
        FILE *fp;
        int ret;
        ret = xpost_memory_get(mem, ent, 0, sizeof(FILE *), &fp);
        if (!ret)
        {
            error(unregistered, "xpost_free_memory_ent cannot load FILE* from FM");
        }
        if (fp
                && fp != stdin
                && fp != stdout
                && fp != stderr) {
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
                error(unregistered,
                        "xpost_free_memory_ent cannot write NULL over FILE* in VM");
            }
        }
    }
    tab->tab[rent].tag = 0;

    xpost_memory_table_get_addr(mem, XPOST_MEMORY_TABLE_SPECIAL_FREE, &z);
    /* printf("freeing %d bytes\n", xpost_memory_table_get_size(mem, ent)); */

    /* copy the current free-list head to the data area of the ent. */
    memcpy(mem->base+a, mem->base+z, sizeof(unsigned));

    /* copy the ent number into the free-list head */
    memcpy(mem->base+z, &ent, sizeof(unsigned));

    return sz;
}

/* print a dump of the free list */
void xpost_free_dump(Xpost_Memory_File *mem)
{
    unsigned e;
    unsigned z;
    xpost_memory_table_get_addr(mem,
            XPOST_MEMORY_TABLE_SPECIAL_FREE, &z);;

    printf("freelist: ");
    memcpy(&e, mem->base+z, sizeof(unsigned));
    while (e) {
        unsigned int sz;
        xpost_memory_table_get_size(mem, e, &sz);
        printf("%d(%d) ", e, sz);
        xpost_memory_table_get_addr(mem, e, &z);
        memcpy(&e, mem->base+z, sizeof(unsigned));
    }
}

/* scan the free list for a suitably sized bit of memory,
   if the allocator falls back to fresh memory PERIOD times,
        it triggers a collection. */
int xpost_free_alloc(Xpost_Memory_File *mem,
        unsigned sz,
        unsigned tag,
        unsigned int *entity)
{
    unsigned z;
    unsigned e;                     /* working pointer */
    static int period = PERIOD;
    unsigned int rent;

    xpost_memory_table_get_addr(mem,
            XPOST_MEMORY_TABLE_SPECIAL_FREE, &z); /* free pointer */

/*#if 0 */
try_again:
    memcpy(&e, mem->base+z, sizeof(unsigned)); /* e = *z */
    while (e) { /* e is not zero */
        unsigned int tsz;
        xpost_memory_table_get_size(mem,e, &tsz);
        if (tsz >= sz) {
            Xpost_Memory_Table *tab;
            unsigned ent;
            unsigned int ad;
            xpost_memory_table_get_addr(mem,e, &ad);
            memcpy(mem->base+z, mem->base + ad, sizeof(unsigned));
            ent = e;
            xpost_memory_table_find_relative(mem, &tab, &ent);
            tab->tab[ent].tag = tag;
            *entity = e;
            return 1;
        }
        xpost_memory_table_get_addr(mem, e, &z);
        memcpy(&e, mem->base+z, sizeof(unsigned));
    }
    if (--period == 0) {
        period = PERIOD;
        collect(mem, 1, 0);
        goto try_again;
    }
/*#endif */
    if (xpost_memory_table_alloc(mem, sz, tag, &rent))
    {
        *entity = rent;
        return 1;
    }
    else
    {
        return 0;
    }
}

/*
   use the free-list and tables to now provide a realloc for 
   "raw" vm addresses (mem->base offsets rather than ents).
  
   Allocate new entry, copy data, steal its adr, stash old adr, free it.
 */
unsigned xpost_free_realloc(Xpost_Memory_File *mem,
        unsigned oldadr,
        unsigned oldsize,
        unsigned newsize)
{
    Xpost_Memory_Table *tab = NULL;
    unsigned newadr;
    unsigned ent;
    unsigned rent; /* relative ent */

#ifdef DEBUGFREE
    printf("xpost_free_realloc: ");
    printf("initial ");
    xpost_free_dump(mem);
#endif

    /* allocate new entry */
    xpost_memory_table_alloc(mem, newsize, 0, &ent);
    rent = ent;
    xpost_memory_table_find_relative(mem, &tab, &rent);

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


