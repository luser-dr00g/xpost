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

#include "xpost_memory.h"  /* save/restore works with mtabs */
#include "xpost_object.h"  /* save/restore examines objects */
#include "xpost_stack.h"  /* save/restore manipulates (internal) stacks */
#include "xpost_save.h"  /* double-check prototypes */
#include "xpost_context.h" /* context for error */
#include "xpost_error.h" /* error */

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
int initsave (Xpost_Memory_File *mem)
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
Xpost_Object save (Xpost_Memory_File *mem)
{
    Xpost_Object v;
    unsigned int vs;

    v.tag = savetype;
    xpost_memory_table_get_addr(mem,
            XPOST_MEMORY_TABLE_SPECIAL_SAVE_STACK, &vs);
    v.save_.lev = xpost_stack_count(mem, vs);
    xpost_stack_init(mem, &v.save_.stk);
    xpost_stack_push(mem, vs, v);
    return v;
}

/* check ent's tlev against current save level (save-stack count) */
unsigned stashed (Xpost_Memory_File *mem,
                  unsigned ent)
{
    Xpost_Memory_Table *tab;
    unsigned cnt;
    unsigned tlev;
    unsigned int vs;

    xpost_memory_table_get_addr(mem,
            XPOST_MEMORY_TABLE_SPECIAL_SAVE_STACK, &vs);
    cnt = xpost_stack_count(mem, vs);
    xpost_memory_table_find_relative(mem, &tab, &ent);
    tlev = (tab->tab[ent].mark & XPOST_MEMORY_TABLE_MARK_DATA_TOPLEVEL_MASK)
        >> XPOST_MEMORY_TABLE_MARK_DATA_TOPLEVEL_OFFSET;

    return tlev == cnt;
}

/* make a clone of ent, return new ent */
static unsigned copy(Xpost_Memory_File *mem,
              unsigned ent)
{
    Xpost_Memory_Table *tab;
    unsigned new;
    unsigned tent = ent;
    unsigned int adr;

    xpost_memory_table_find_relative(mem, &tab, &ent);
    if (!xpost_memory_table_alloc(mem, tab->tab[ent].sz, tab->tab[ent].tag, &new))
        error(VMerror, "copy cannot allocate entity to backup object");
    ent = tent;
    xpost_memory_table_find_relative(mem, &tab, &ent); //recalc
    xpost_memory_table_get_addr(mem, new, &adr);
    memcpy(mem->base + adr,
            mem->base + tab->tab[ent].adr,
            tab->tab[ent].sz);

    return new;
}

/* set tlev for ent to current save level
   push saverec relating ent to saved copy */
void stash(Xpost_Memory_File *mem,
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

    xpost_memory_table_get_addr(mem,
            XPOST_MEMORY_TABLE_SPECIAL_SAVE_STACK, &adr);
    sav = xpost_stack_topdown_fetch(mem, adr, 0);

    xpost_memory_table_find_relative(mem, &tab, &rent);
    tlev = sav.save_.lev;
    tab->tab[rent].mark &= ~XPOST_MEMORY_TABLE_MARK_DATA_TOPLEVEL_MASK; // clear TLEV field
    tab->tab[rent].mark |= (tlev << XPOST_MEMORY_TABLE_MARK_DATA_TOPLEVEL_OFFSET);  // set TLEV field

    o.saverec_.tag = tag;
    o.saverec_.pad = pad;
    o.saverec_.src = ent;
    o.saverec_.cpy = copy(mem, ent);
    xpost_stack_push(mem, sav.save_.stk, o);
}

/* for each saverec from current save stack
        exchange adrs between src and cpy
        pop saverec
    pop save stack */
void restore(Xpost_Memory_File *mem)
{
    unsigned v;
    Xpost_Object sav;
    Xpost_Memory_Table *stab, *ctab;
    unsigned cnt;
    unsigned sent, cent;

    xpost_memory_table_get_addr(mem, XPOST_MEMORY_TABLE_SPECIAL_SAVE_STACK, &v); // save-stack address
    sav = xpost_stack_pop(mem, v); // save-object (stack of saverec_'s)
    cnt = xpost_stack_count(mem, sav.save_.stk);
    while (cnt--) {
        Xpost_Object rec;
        unsigned hold;
        rec = xpost_stack_pop(mem, sav.save_.stk);
        sent = rec.saverec_.src;
        cent = rec.saverec_.cpy;
        xpost_memory_table_find_relative(mem, &stab, &sent);
        xpost_memory_table_find_relative(mem, &ctab, &cent);
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
    initsave(mem);
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
    (void)save(mem);
    arrput(mem, a, 0, xpost_cons_int(77));
    show("save and alter", mem, a);

    restore(mem);
    show("restored", mem, a);

    puts("");
    return 0;
}

#endif


