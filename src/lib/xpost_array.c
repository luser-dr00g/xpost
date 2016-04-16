/*
 * Xpost - a Level-2 Postscript interpreter
 * Copyright (C) 2013-2016, Michael Joshua Ryan
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

/** \file xpost_array.c
   array functions
*/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <stdlib.h> /* size_t */

#include "xpost.h"
#include "xpost_log.h"
#include "xpost_memory.h"  /* arrays live in mfile, accessed via mtab */
#include "xpost_object.h"  /* array is an object, containing objects */
#include "xpost_stack.h"  /* may count the save stack */
#include "xpost_free.h"  /* arrays are allocated from the free list */

#include "xpost_save.h"  /* arrays obey save/restore */
#include "xpost_context.h"
//#include "xpost_interpreter.h"  /* banked arrays may be in global or local mfiles */
#include "xpost_error.h"  /* array functions may throw errors */
#include "xpost_array.h"  /* double-check prototypes */



/**
  Allocate array in specified memory file.

  Allocate an entity with xpost_memory_table_alloc,
   find the appropriate mtab,
   set the current save level in the "mark" field,
   wrap it up in an object.
*/
Xpost_Object xpost_array_cons_memory(Xpost_Memory_File *mem,
                                     unsigned int sz)
{
    unsigned int ent;
    unsigned int rent;
    unsigned int cnt;
    Xpost_Memory_Table *tab;
    Xpost_Object o;
    unsigned int i;
    unsigned int vs;

    assert(mem->base);

    if (sz == 0)
    {
        ent = 0;
    }
    else
    {
        if (!xpost_memory_table_alloc(mem,
                                      (unsigned int)(sz * sizeof(Xpost_Object)),
                                      arraytype,
                                      &ent))
        {
            XPOST_LOG_ERR("cannot allocate array");
            return null;
        }
        tab = &mem->table;
        rent = ent;
        xpost_memory_table_get_addr(mem,
                                    XPOST_MEMORY_TABLE_SPECIAL_SAVE_STACK, &vs);
        cnt = xpost_stack_count(mem, vs);
        tab->tab[rent].mark = ( (0 << XPOST_MEMORY_TABLE_MARK_DATA_MARK_OFFSET)
                | (0 << XPOST_MEMORY_TABLE_MARK_DATA_REFCOUNT_OFFSET)
                | (cnt << XPOST_MEMORY_TABLE_MARK_DATA_LOWLEVEL_OFFSET)
                | (cnt << XPOST_MEMORY_TABLE_MARK_DATA_TOPLEVEL_OFFSET) );

        /* fill array with the null object */
        for (i = 0; i < sz; i++)
        {
            int ret;

            ret = xpost_memory_put(mem,
                                   ent, i, (unsigned int)sizeof(Xpost_Object), &null);
            if (!ret)
            {
                XPOST_LOG_ERR("cannot fill array value");
                return null;
            }
        }
    }

    /* return (Xpost_Object){ .comp_.tag = arraytype, .comp_.sz = sz, .comp_.ent = ent, .comp_.off = 0}; */
    o.tag = arraytype
        | (XPOST_OBJECT_TAG_ACCESS_UNLIMITED
                << XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_OFFSET);
    o.comp_.sz = (word)sz;
    //o.comp_.ent = (word)ent;
    o.comp_.off = 0;
    o = xpost_object_set_ent(o, ent);
    return o;
}

/**
  Allocate array in context's currently active memory file.

  Select a memory file according to vmmode,
   call xpost_array_cons_memory,
   set BANK flag.   object.tag&BANK?global:local
*/
Xpost_Object xpost_array_cons(Xpost_Context *ctx,
                              unsigned int sz)
{
    Xpost_Object a = xpost_array_cons_memory(ctx->vmmode==GLOBAL? ctx->gl: ctx->lo, sz);
    if (xpost_object_get_type(a) != nulltype)
    {
        xpost_stack_push(ctx->lo, ctx->hold, a); /* stash a reference on the hold stack in case of gc in caller */
        if (ctx->vmmode==GLOBAL)
            a.tag |= XPOST_OBJECT_TAG_DATA_FLAG_BANK;
    }
    return a;
}

/**
  Put object into array with given memory file.
  (Array must be valid for this memory file)

  Copy if necessary for save/restore,
   call memory_put.
*/
int xpost_array_put_memory(Xpost_Memory_File *mem,
                           Xpost_Object a,
                           integer i,
                           Xpost_Object o)
{
    int ret;
    if (!xpost_save_ent_is_saved(mem, xpost_object_get_ent(a)))
        if (!xpost_save_save_ent(mem, arraytype, a.comp_.sz, xpost_object_get_ent(a)))
            return VMerror;
    if (i > a.comp_.sz)
    {
        XPOST_LOG_ERR("cannot put value in array (rangecheck) %u > [%u]", i, a.comp_.sz);
        /*breakhere((Xpost_Context *)mem);*/
        return rangecheck;
    }
    ret = xpost_memory_put(mem, xpost_object_get_ent(a),
                           (unsigned int)(a.comp_.off + i),
                           (unsigned int)sizeof(Xpost_Object), &o);
    if (!ret)
        return VMerror;
    return 0;
}

/**
  Put object into array.

  Select Xpost_Memory_File according to BANK flag,
   call xpost_array_put_memory.
*/
int xpost_array_put(Xpost_Context *ctx,
                    Xpost_Object a,
                    integer i,
                    Xpost_Object o)
{
    Xpost_Memory_File *mem = xpost_context_select_memory(ctx, a);
    if (!ctx->ignoreinvalidaccess)
    {
        if ( mem == ctx->gl &&
             xpost_object_is_composite(o) &&
             mem != xpost_context_select_memory(ctx, o))
            return invalidaccess;
    }

    return xpost_array_put_memory(mem, a, i, o);
}

/*
   Get object from array with specified memory file.
   (Array must be valid for this memory file)

   call memory_get.
 */
Xpost_Object xpost_array_get_memory(Xpost_Memory_File *mem,
                                    Xpost_Object a,
                                    integer i)
{
    Xpost_Object o;
    int ret;

    ret = xpost_memory_get(mem, xpost_object_get_ent(a),
                           (unsigned int)(a.comp_.off +i),
                           (unsigned int)(sizeof(Xpost_Object)), &o);
    if (!ret)
    {
        return invalid;
    }

    return o;
}

/*
   Get object from array.

   Select Xpost_Memory_File according to BANK flag,
   call xpost_array_get_memory.
 */
Xpost_Object xpost_array_get(Xpost_Context *ctx,
                             Xpost_Object a,
                             integer i)
{
    return xpost_array_get_memory(xpost_context_select_memory(ctx, a), a, i);
}

#ifdef TESTMODULE_AR
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Xpost_Context *ctx;
Xpost_Memory_File *mem;

int main(void)
{
    if (!xpost_init())
    {
        fprintf(stderr, "Fail to initialize xpost array test\n");
        return -1;
    }

    itpdata = malloc(sizeof*itpdata);
    memset(itpdata, 0, sizeof*itpdata);
    xpost_interpreter_init(itpdata);
    ctx = &itpdata->ctab[0];
    mem = ctx->lo;
    /* xpost_memory_file_init(&mem, "x.mem"); */
    /* (void)xpost_memory_table_init(&mem); */
    /* xpost_free_init(&mem); */
    /* xpost_save_init(&mem); */

    enum { SIZE = 10 };
    printf("\n^test ar.c\n");
    printf("allocating array occupying %zu bytes\n", SIZE*sizeof(Xpost_Object));
    Xpost_Object a = xpost_array_cons_memory(mem, SIZE);

    /* printf("the memory table:\n"); dumpmtab(mem, 0); */

    printf("test array by filling\n");
    int i;
    for (i=0; i < SIZE; i++) {
        printf("%d ", i+1);
        xpost_array_put_memory(mem, a, i, xpost_int_cons( i+1 ));
    }
    puts("");

    printf("and accessing.\n");
    for (i=0; i < SIZE; i++) {
        Xpost_Object t;
        t = xpost_array_get_memory(mem, a, i);
        printf("%d: %d\n", i, t.int_.val);
    }

    printf("the memory table:\n");
    dumpmtab(mem, 0);

    xpost_quit();

    return 0;
}

#endif
