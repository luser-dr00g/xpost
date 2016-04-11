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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h> /* NULL */

#include "xpost.h"
#include "xpost_log.h"
#include "xpost_object.h"
#include "xpost_memory.h"
#include "xpost_error.h"
#include "xpost_stack.h"

/*
 * The stack type is a chain of segments.
 *
 * root->prevseg == top segment
 * tail->nextseg == 0
 *

typedef struct
{
    unsigned int nextseg;
    unsigned int prevseg;
    unsigned int top;
    Xpost_Object data[XPOST_STACK_SEGMENT_SIZE];
} Xpost_Stack;
*/

/* allocate memory for one stack segment */
XPCHECKAPI int xpost_stack_init(Xpost_Memory_File *mem,
                                unsigned int *paddr)
{
    unsigned int adr;
    Xpost_Stack *s;

    xpost_memory_file_alloc(mem, sizeof(Xpost_Stack), &adr);
    s = (Xpost_Stack *)(mem->base + adr);
    s->nextseg = 0;
    s->prevseg = adr;
    s->top = 0;
    *paddr = adr;
    return 1;
}

void xpost_stack_clear(Xpost_Memory_File *mem,
                       unsigned int stackadr)
{
    Xpost_Stack *s = (Xpost_Stack *)(mem->base + stackadr);
    s->top = 0;
    s->prevseg = stackadr;
}

void xpost_stack_dump(Xpost_Memory_File *mem,
                      unsigned int stackadr)
{
    Xpost_Stack *s = (Xpost_Stack *)(mem->base + stackadr);
    unsigned int i;
    unsigned int a;

    a = 0;
    while (1)
    {
        for (i = 0; i < s->top; i++)
        {
            XPOST_LOG_DUMP("%d:", a++);
            xpost_object_dump(s->data[i]);
        }
        if (i != XPOST_STACK_SEGMENT_SIZE)
            break;
        if (s->nextseg == 0)
            break;
        s = (Xpost_Stack *)(mem->base + s->nextseg);
    }
}

/* deallocate stack segment and any chained segments */
XPCHECKAPI void xpost_stack_free(Xpost_Memory_File *mem,
                                 unsigned int stackadr)
{
    Xpost_Stack *s = (Xpost_Stack *)(mem->base + stackadr);
    Xpost_Memory_Table *tab;
    unsigned int e;

    if (s->nextseg)
        xpost_stack_free(mem, s->nextseg);
    xpost_memory_table_alloc(mem, 0, 0, &e); /* allocate entry with 0 size */
    tab = &mem->table;
    tab->tab[e].adr = stackadr; /* insert address */
    tab->tab[e].sz = sizeof(Xpost_Stack); /* insert size */
    /* discard */
}

int xpost_stack_count(Xpost_Memory_File *mem,
                      unsigned int stackadr)
{
    Xpost_Stack *s = (Xpost_Stack *)(mem->base + stackadr);
    unsigned int ct = 0;
    while (s->top == XPOST_STACK_SEGMENT_SIZE)
    {
        ct += XPOST_STACK_SEGMENT_SIZE;
        s = (Xpost_Stack *)(mem->base + s->nextseg);
    }
    return ct + s->top;
}

XPCHECKAPI int xpost_stack_push(Xpost_Memory_File *mem,
                                unsigned int stackadr,
                                Xpost_Object obj)
{
    Xpost_Stack *root = (Xpost_Stack *)(mem->base + stackadr);
    Xpost_Stack *s = (Xpost_Stack *)(mem->base + root->prevseg); /* load top segment */

    if (xpost_object_get_type(obj) == invalidtype)
        return 0;

    s->data[s->top++] = obj; /* push value */

    /* if push filled the topmost segment, link a new one. */
    if (s->top == XPOST_STACK_SEGMENT_SIZE)
    {
        if (s->nextseg == 0)
        {
            size_t stadr;
            unsigned int newst;
            int ret;

            stadr = (unsigned char *)s - mem->base;
            ret = xpost_stack_init(mem, &newst);
            if (!ret)
                return 0;
            s = (Xpost_Stack *)(mem->base + stadr);
            root = (Xpost_Stack *)(mem->base + stackadr);
            s->nextseg = newst;
            root->prevseg = newst; //root->prev==top
            ((Xpost_Stack *)(mem->base + newst))->prevseg = stadr;
        }
        else
        {
            root->prevseg = s->nextseg;
            s = (Xpost_Stack *)(mem->base + s->nextseg);
            s->top = 0;
        }
    }

    return 1;
}

Xpost_Object xpost_stack_topdown_fetch(Xpost_Memory_File *mem,
                                       unsigned int stackadr,
                                       int idx)
{
#if 0
    int i = idx;
    int cnt = xpost_stack_count(mem, stackadr);
    if (idx >= cnt) return invalid;
    return xpost_stack_bottomup_fetch(mem, stackadr, cnt - 1 - i);
#else
    int i = idx;
    Xpost_Stack *s = (Xpost_Stack *)(mem->base + stackadr);

    if (s->prevseg) s = (Xpost_Stack *)(mem->base + s->prevseg); /* find top seg */

    while (i >= (signed)(s->top)){
        i -= s->top;
        if (s == (Xpost_Stack *)(mem->base + stackadr)){
            XPOST_LOG_ERR("%d can't find stack segment for index -%d in stack of size %u",
                    unregistered, idx,
                    xpost_stack_count(mem, stackadr));
            return invalid;
        }
        s = (Xpost_Stack *)(mem->base + s->prevseg);
    }
    return s->data[s->top - 1 - i];
#endif
}

int xpost_stack_topdown_replace(Xpost_Memory_File *mem,
                                unsigned int stackadr,
                                int idx,
                                Xpost_Object obj)
{
#if 0
    int i = idx;
    int cnt = xpost_stack_count(mem, stackadr);
    if (idx >= cnt) return 0;
    return xpost_stack_bottomup_replace(mem, stackadr, cnt - 1 - i, obj);
#else
    int i = idx;
    Xpost_Stack *s = (Xpost_Stack *)(mem->base + stackadr);
    if (s->prevseg) s = (Xpost_Stack *)(mem->base + s->prevseg); /* find top seg */

    while (i >= (signed)(s->top)){
        i -= s->top;
        if (s == (Xpost_Stack *)(mem->base + stackadr)){
            XPOST_LOG_ERR("%d can't find stack segment for index -%d in stack of size %u",
                    unregistered, idx,
                    xpost_stack_count(mem, stackadr));
            return 0;
        }
        s = (Xpost_Stack *)(mem->base + s->prevseg);
    }
    s->data[s->top - 1 - i] = obj;
    return 1;
#endif
}

Xpost_Object xpost_stack_bottomup_fetch(Xpost_Memory_File *mem,
                                        unsigned int stackadr,
                                        int idx)
{
    Xpost_Stack *root = (Xpost_Stack *)(mem->base + stackadr);
    Xpost_Stack *s = root;
    int i = idx;

    /* find desired segment */
    while (i >= XPOST_STACK_SEGMENT_SIZE)
    {
        i -= XPOST_STACK_SEGMENT_SIZE;
        if (s->nextseg == 0)
        {
            XPOST_LOG_ERR("%d can't find stack segment for index %d in stack of size %u",
                    unregistered, idx,
                    xpost_stack_count(mem, stackadr));
            return invalid;
        }
        s = (Xpost_Stack *)(mem->base + s->nextseg);
    }
    if (i >= (signed)s->top){
        return invalid;
    }
    return s->data[i];
}

int xpost_stack_bottomup_replace(Xpost_Memory_File *mem,
                                 unsigned int stackadr,
                                 int idx,
                                 Xpost_Object obj)
{
    Xpost_Stack *root = (Xpost_Stack *)(mem->base + stackadr);
    Xpost_Stack *s = root;
    int i = idx;

    /* find desired segment */
    while (i >= XPOST_STACK_SEGMENT_SIZE)
    {
        i -= XPOST_STACK_SEGMENT_SIZE;
        if (s->nextseg == 0)
        {
            XPOST_LOG_ERR("%d can't find stack segment for index %d in stack of size %u",
                          unregistered, idx,
                          xpost_stack_count(mem, stackadr));
            return 0;
        }
        s = (Xpost_Stack *)(mem->base + s->nextseg);
    }
    if (i >= (signed)s->top){
        return 0;
    }
    s->data[i] = obj;
    return 1;
}

XPCHECKAPI Xpost_Object xpost_stack_pop(Xpost_Memory_File *mem,
                                        unsigned int stackadr)
{
    Xpost_Stack *root = (Xpost_Stack *)(mem->base + stackadr);
    Xpost_Stack *s = (Xpost_Stack *)(mem->base + root->prevseg); /* load top seg */

    if (s->top == 0) /* back up if top is empty */
    {
        if (s != root)
        {
            unsigned int soff = s->prevseg;
            s = (Xpost_Stack *)(mem->base + soff);
            root->prevseg = soff; // update root->top
        }
        else /* can't back up if stack is empty */
        {
            return invalid;
        }
    }

    return s->data[--s->top]; /* pop value */
}
