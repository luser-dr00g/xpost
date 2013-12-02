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

#include <assert.h>
#include <string.h>

#include "xpost_log.h"
#include "xpost_memory.h"  /* save/restore works with mtabs */
#include "xpost_object.h"  /* save/restore examines objects */
#include "xpost_stack.h"  /* save/restore manipulates (internal) stacks */

#include "xpost_error.h"
#include "xpost_save.h"  /* double-check prototypes */

/*
typedef struct {
    word tag;
    word lev;
    unsigned stk;
} save_;

typedef struct {
    word tag;
    word pad;
    word src;
    word cpy;
} saverec_;
*/

/* create a stack in slot XPOST_MEMORY_TABLE_SPECIAL_SAVE_STACK.
   sz is 0 so gc will ignore it. */
int xpost_save_init (Xpost_Memory_File *mem)
{
    unsigned t;
    unsigned ent;
    Xpost_Memory_Table *tab;
    int ret;

    ret = xpost_memory_table_alloc(mem, 0, 0, &ent); /* allocate an entry of zero length */
    if (!ret)
    {
        return 0;
    }
    assert(ent == XPOST_MEMORY_TABLE_SPECIAL_SAVE_STACK);

    xpost_stack_init(mem, &t);
    tab = (void *)mem->base;
    tab->tab[ent].adr = t;

    return 1;
}

/* push a new save object on the save stack
   this object is itself a stack (contains a stackadr) */
Xpost_Object xpost_save_create_snapshot_object (Xpost_Memory_File *mem)
{
    Xpost_Object v;
    unsigned int vs;
    int ret;

    v.tag = savetype;
    ret = xpost_memory_table_get_addr(mem,
            XPOST_MEMORY_TABLE_SPECIAL_SAVE_STACK, &vs);
    if (!ret)
    {
        XPOST_LOG_ERR("cannot load save stack");
        return null;
    }
    v.save_.lev = xpost_stack_count(mem, vs);
    xpost_stack_init(mem, &v.save_.stk);
    xpost_stack_push(mem, vs, v);
    return v;
}

/* check ent's llev and tlev
   against current save level (save-stack count)
   returns 1 if ent is saved (or not necessary to save),
   returns 0 if ent needs to be saved before changing. 
 */
unsigned xpost_save_ent_is_saved (Xpost_Memory_File *mem,
                  unsigned ent)
{
    Xpost_Memory_Table *tab;
    unsigned int llev;
    unsigned int tlev;
    unsigned int vs;
    int ret;
    Xpost_Object sav;

    ret = xpost_memory_table_get_addr(mem,
            XPOST_MEMORY_TABLE_SPECIAL_SAVE_STACK, &vs);
    if (!ret)
    {
        XPOST_LOG_ERR("cannot load save stack");
        return 0;
    }
    sav = xpost_stack_topdown_fetch(mem, vs, 0);
    ret = xpost_memory_table_find_relative(mem, &tab, &ent);
    if (!ret)
    {
        XPOST_LOG_ERR("cannot find table for ent %u", ent);
        return 0;
    }
    tlev = (tab->tab[ent].mark & XPOST_MEMORY_TABLE_MARK_DATA_TOPLEVEL_MASK)
        >> XPOST_MEMORY_TABLE_MARK_DATA_TOPLEVEL_OFFSET;
    llev = (tab->tab[ent].mark & XPOST_MEMORY_TABLE_MARK_DATA_LOWLEVEL_MASK)
        >> XPOST_MEMORY_TABLE_MARK_DATA_LOWLEVEL_OFFSET;

    return llev < sav.save_.lev ?
        tlev == sav.save_.lev : 1;
}

/* make a clone of ent, return new ent */
static
unsigned int _copy_ent(Xpost_Memory_File *mem,
              unsigned ent)
{
    Xpost_Memory_Table *tab;
    unsigned new;
    unsigned tent = ent;
    unsigned int adr;
    int ret;

    ret = xpost_memory_table_find_relative(mem, &tab, &ent);
    if (!ret)
    {
        XPOST_LOG_ERR("cannot find table for ent %u", ent);
        return 0;
    }
    if (!xpost_memory_table_alloc(mem, tab->tab[ent].sz, tab->tab[ent].tag, &new))
    {
        XPOST_LOG_ERR("cannot allocate entity to backup object");
        return 0;
    }
    ent = tent;
    ret = xpost_memory_table_find_relative(mem, &tab, &ent); //recalc
    if (!ret)
    {
        XPOST_LOG_ERR("cannot find table for ent %u", ent);
        return 0;
    }
    ret = xpost_memory_table_get_addr(mem, new, &adr);
    if (!ret)
    {
        XPOST_LOG_ERR("cannot find table for ent %u", ent);
        return 0;
    }
    memcpy(mem->base + adr,
            mem->base + tab->tab[ent].adr,
            tab->tab[ent].sz);

    XPOST_LOG_INFO("ent %u copied to ent %u", ent, new);
    return new;
}

/* set tlev for ent to current save level
   push saverec relating ent to saved copy */
void xpost_save_save_ent(Xpost_Memory_File *mem,
           unsigned tag,
           unsigned pad,
           unsigned ent)
{
    Xpost_Memory_Table *tab;
    Xpost_Object o;
    unsigned tlev;
    unsigned rent = ent;
    Xpost_Object sav;
    unsigned int adr;
    unsigned int cpy;
    int ret;

    ret = xpost_memory_table_get_addr(mem,
            XPOST_MEMORY_TABLE_SPECIAL_SAVE_STACK, &adr);
    if (!ret)
    {
        XPOST_LOG_ERR("cannot load save stack");
        return;
    }
    sav = xpost_stack_topdown_fetch(mem, adr, 0);

    ret = xpost_memory_table_find_relative(mem, &tab, &rent);
    if (!ret)
    {
        XPOST_LOG_ERR("cannot find table for ent %u", ent);
        return;
    }
    tlev = sav.save_.lev;
    tab->tab[rent].mark &= ~XPOST_MEMORY_TABLE_MARK_DATA_TOPLEVEL_MASK; // clear TLEV field
    tab->tab[rent].mark |= (tlev << XPOST_MEMORY_TABLE_MARK_DATA_TOPLEVEL_OFFSET);  // set TLEV field

    o.saverec_.tag = tag;
    o.saverec_.pad = pad;
    o.saverec_.src = ent;
    cpy = _copy_ent(mem, ent);
    if (cpy == 0)
    {
        XPOST_LOG_ERR("unable to make copy of ent %d", ent);
        return;
    }

    o.saverec_.cpy = cpy;
    xpost_stack_push(mem, sav.save_.stk, o);
}

/* for each saverec from current save stack
        exchange adrs between src and cpy
        pop saverec
    pop save stack */
void xpost_save_restore_snapshot(Xpost_Memory_File *mem)
{
    unsigned int v;
    Xpost_Object sav;
    Xpost_Memory_Table *stab, *ctab;
    unsigned int cnt;
    unsigned int sent, cent;
    int ret;

    ret = xpost_memory_table_get_addr(mem, XPOST_MEMORY_TABLE_SPECIAL_SAVE_STACK, &v); // save-stack address
    if (!ret)
    {
        XPOST_LOG_ERR("cannot load save stack");
        return;
    }
    sav = xpost_stack_pop(mem, v); // save-object (stack of saverec_'s)
    if (xpost_object_get_type(sav) == invalidtype)
        return;
    cnt = xpost_stack_count(mem, sav.save_.stk);
    while (cnt--) {
        Xpost_Object rec;
        unsigned hold;
        rec = xpost_stack_pop(mem, sav.save_.stk);
        if (xpost_object_get_type(rec) == invalidtype)
            return;
        sent = rec.saverec_.src;
        cent = rec.saverec_.cpy;
        ret = xpost_memory_table_find_relative(mem, &stab, &sent);
        if (!ret)
        {
            XPOST_LOG_ERR("cannot find table for ent %u", sent);
            return;
        }
        ret = xpost_memory_table_find_relative(mem, &ctab, &cent);
        if (!ret)
        {
            XPOST_LOG_ERR("cannot find table for ent %u", cent);
            return;
        }
        hold = stab->tab[sent].adr;                 // tmp = src
        stab->tab[sent].adr = ctab->tab[cent].adr;  // src = cpy
        ctab->tab[cent].adr = hold;                 // cpy = tmp
    }
    xpost_stack_free(mem, sav.save_.stk);
}

#ifdef TESTMODULE_V
#include "xpost_free.h"
#include "xpost_array.h"
#include <stdio.h>

Xpost_Memory_File mf;

void init (Xpost_Memory_File *mem)
{
    xpost_memory_file_init(mem, "x.mem");
    (void)xpost_memory_table_init(mem);
    xpost_free_init(mem);
    xpost_save_init(mem);
}

void show (char *msg, Xpost_Memory_File *mem, Xpost_Object a)
{
    printf("%s ", msg);
    printf("%d ", arrget(mem, a, 0).int_.val);
    printf("%d\n", arrget(mem, a, 1).int_.val);
}

int main (void)
{
    Xpost_Memory_File *mem = &mf;
    Xpost_Object a;
    printf("\n^test v\n");
    init(mem);

    a = consarr(mem, 2);
    arrput(mem, a, 0, xpost_cons_int(33));
    arrput(mem, a, 1, xpost_cons_int(66));
    show("initial", mem, a);

    //object v = 
    (void)xpost_save_create_snapshot_object(mem);
    arrput(mem, a, 0, xpost_cons_int(77));
    show("save and alter", mem, a);

    xpost_save_restore_snapshot(mem);
    show("restored", mem, a);

    puts("");
    return 0;
}

#endif


