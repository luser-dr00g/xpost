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
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h> /* close */
#endif

#include "xpost.h"
#include "xpost_log.h"
#include "xpost_compat.h" /* mkstemp */
#include "xpost_memory.h"
#include "xpost_object.h"
#include "xpost_stack.h"
#include "xpost_free.h"
#include "xpost_context.h"
#include "xpost_array.h"
#include "xpost_string.h"
#include "xpost_dict.h"
#include "xpost_save.h"
#include "xpost_name.h"

//#include "xpost_interpreter.h"
#include "xpost_garbage.h"

#ifdef DEBUG_GC
#include <stdio.h>
#endif


/* iterate through all tables,
    clear the MARK in the mark. */
static
void _xpost_garbage_unmark(Xpost_Memory_File *mem)
{
    unsigned int i;

    if (!mem) return;

    for (i = mem->start; i < mem->table.nextent; i++)
    {
        mem->table.tab[i].mark &= ~XPOST_MEMORY_TABLE_MARK_DATA_MARK_MASK;
    }
}

/* set the MARK in the mark in the tab[ent] */
static
int _xpost_garbage_mark_ent(Xpost_Memory_File *mem,
                            unsigned int ent)
{
    if (!mem) return 0;

    if (ent < mem->start)
    {
        XPOST_LOG_ERR("attempt to mark ent %u < mem->start", ent);
        return 1;
    }

    if (ent >= mem->table.nextent)
    {
        XPOST_LOG_ERR("cannot find ent %u", ent);
        return 0;
    }
    mem->table.tab[ent].mark |= XPOST_MEMORY_TABLE_MARK_DATA_MARK_MASK;
    return 1;
}

/* is it marked? */
static
int _xpost_garbage_ent_is_marked(Xpost_Memory_File *mem,
                                 unsigned int ent,
                                 int *retval)
{
    if (!mem) return 0;

    if (ent >= mem->table.nextent)
    {
        XPOST_LOG_ERR("cannot find table for ent %u", ent);
        return 0;
    }
    *retval = (mem->table.tab[ent].mark & XPOST_MEMORY_TABLE_MARK_DATA_MARK_MASK)
        >> XPOST_MEMORY_TABLE_MARK_DATA_MARK_OFFSET;

    return 1;
}

/* recursively mark an object */
static
int _xpost_garbage_mark_object(Xpost_Context *ctx, Xpost_Memory_File *mem, Xpost_Object o, int markall);

/* recursively mark a dictionary */
static
int _xpost_garbage_mark_dict(Xpost_Context *ctx,
                             Xpost_Memory_File *mem,
                             unsigned int adr,
                             int markall)
{
    if (!mem) return 0;

    {
        dichead *dp = (void *)(mem->base + adr);
        dicrec *tp = (void *)(mem->base + adr + sizeof(dichead));
        int j;
#ifdef DEBUG_GC
        Xpost_Object_Type type;
        printf("markdict: nused=%d\n", dp->nused);
#endif

        for (j = 0; j < DICTABN(dp->sz); j++)
        {
            if (xpost_object_get_type(tp[j].key) != nulltype){
                if (!_xpost_garbage_mark_object(ctx,
                            xpost_context_select_memory(ctx,tp[j].key), tp[j].key, markall))
                    return 0;
#ifdef DEBUG_GC
            switch(type = xpost_object_get_type(tp[j].key)){
            default: {
                    printf("%s", xpost_object_type_names[type]);
                }
                break;
            case nametype: {
                unsigned int address;
                Xpost_Object str;

                xpost_memory_table_get_addr( xpost_context_select_memory(ctx,tp[j].key),
                    XPOST_MEMORY_TABLE_SPECIAL_NAME_STACK, &address);

                str = xpost_stack_bottomup_fetch(
                    xpost_context_select_memory(ctx,tp[j].key),
                    address, tp[j].key.mark_.padw);
                printf("%*s", str.comp_.sz, xpost_string_get_pointer(ctx,str));

                }
                break;
            }
            printf(":");
            printf("%s\n", xpost_object_type_names[xpost_object_get_type(tp[j].value)]);
#endif
                if (!_xpost_garbage_mark_object(ctx,
                            xpost_context_select_memory(ctx,tp[j].value), tp[j].value, markall))
                    return 0;
            }
        }
    }

    return 1;
}

/* recursively mark all elements of array */
static
int _xpost_garbage_mark_array(Xpost_Context *ctx,
                              Xpost_Memory_File *mem,
                              unsigned int adr,
                              unsigned int sz,
                              int markall)
{
    if (!mem) return 0;

#ifdef DEBUG_GC
    printf("markarray: sz=%u\n", sz);
#endif

    {
        Xpost_Object *op = (void *)(mem->base + adr);
        unsigned int j;

        for (j = 0; j < sz; j++)
        {
#ifdef DEBUG_GC
            printf("%u:%s\n", j, xpost_object_type_names[xpost_object_get_type(op[j])]);
#endif
            if (!_xpost_garbage_mark_object(ctx,
                                            xpost_context_select_memory(ctx,op[j]),
                                            op[j], markall))
                return 0;
        }
    }

    return 1;
}

/* traverse the contents of composite objects
   if markall is true, this is a collection of global vm,
   so we must mark objects and recurse
   even if it means switching memory files
 */
static
int _xpost_garbage_mark_object(Xpost_Context *ctx,
                               Xpost_Memory_File *mem,
                               Xpost_Object o,
                               int markall)
{
    unsigned int ad;
    int ret;
    unsigned int tag;
    unsigned int ent;
    Xpost_Object_Type type;
    Xpost_Memory_File *objmem;

    if (!mem) return 0;

    if (!xpost_object_is_composite(o))
        return 1;

    ent = xpost_object_get_ent(o);
    type = xpost_object_get_type(o);

#ifdef DEBUG_GC
            printf("markobject: ent %d, addr %u, %s (size %d)\n",
                   ent,
                   xpost_context_select_memory(ctx,o)==mem?
                       (ent >= mem->table.nextent?
                        (unsigned)-1: mem->table.tab[ent].adr) : 0,
                   xpost_object_type_names[type],
                   o.comp_.sz);
#endif

    switch(type)
    {
        default: break;

        case arraytype:
            if (ent == 0)
            {
                return 1;
            }

            objmem = xpost_context_select_memory(ctx, o);
            if (objmem != mem) {
                if (!markall)
                    break;
            }
            if (ent < objmem->start)
            {
                XPOST_LOG_ERR("attempt to mark %s object %d",
                        xpost_object_type_names[type],
                        ent);
                return 0;
            }
            if (!objmem) return 0;
            if (!_xpost_garbage_ent_is_marked(objmem, ent, &ret))
                return 0;
            if (!ret) {
                ret = _xpost_garbage_mark_ent(objmem, ent);
                if (!ret)
                {
                    XPOST_LOG_ERR("cannot mark array %d", ent);
                    return 0;
                }
                ret = xpost_memory_table_get_addr(objmem, ent, &ad);
                if (!ret)
                {
                    XPOST_LOG_ERR("cannot retrieve address for array ent %u", ent);
                    return 0;
                }
                ret = xpost_memory_table_get_tag(objmem, ent, &tag);
                if (!ret)
                {
                    XPOST_LOG_ERR("cannot retrieve tag for array ent %u", ent);
                    return 0;
                }
                if (o.comp_.sz != objmem->table.tab[ent].used/sizeof(Xpost_Object))
                {
                    XPOST_LOG_INFO("o.comp_.sz %u != tab[ent].used/obj %u",
                            o.comp_.sz, objmem->table.tab[ent].used/sizeof(Xpost_Object));
                }
                if (!_xpost_garbage_mark_array(ctx, objmem, ad,
                            //mem->table.tab[ent].used/sizeof(Xpost_Object)
                            o.comp_.sz
                            , markall))
                    return 0;
            }
            break;

        case dicttype:
            objmem = xpost_context_select_memory(ctx, o);
            if (objmem != mem)
            {
                if (!markall)
                    break;
            }
            if (ent < objmem->start)
            {
                XPOST_LOG_ERR("attempt to mark %s object %d",
                        xpost_object_type_names[type],
                        ent);
                return 0;
            }
            if (!_xpost_garbage_ent_is_marked(objmem, ent, &ret))
                return 0;
            if (!ret)
            {
                ret = _xpost_garbage_mark_ent(objmem, ent);
                if (!ret)
                {
                    XPOST_LOG_ERR("cannot mark dict");
                    return 0;
                }
                ret = xpost_memory_table_get_addr(objmem, ent, &ad);
                if (!ret)
                {
                    XPOST_LOG_ERR("cannot retrieve address for dict ent %u", ent);
                    return 0;
                }
                if (!_xpost_garbage_mark_dict(ctx, objmem, ad, markall))
                    return 0;
            }
            break;

        case stringtype:
            if (ent == 0)
            {
                return 1;
            }

            objmem = xpost_context_select_memory(ctx, o);
            if (objmem != mem)
            {
                if (!markall)
                    break;
            }

            if (ent < objmem->start)
            {
                XPOST_LOG_ERR("attempt to mark %s object %d",
                        xpost_object_type_names[type],
                        ent);
                return 0;
            }
            ret = _xpost_garbage_mark_ent(objmem, ent);
            if (!ret)
            {
                XPOST_LOG_ERR("cannot mark string");
                return 0;
            }
            break;

        case filetype:
            objmem = xpost_context_select_memory(ctx, o);
            if (ent < objmem->start)
            {
                XPOST_LOG_ERR("attempt to mark %s object %d",
                        xpost_object_type_names[type],
                        ent);
                return 0;
            }
            if (objmem == ctx->gl)
            {
                printf("file found in global vm\n");
            } else {
                ret = _xpost_garbage_mark_ent(objmem, o.mark_.padw);
                if (!ret)
                {
                    XPOST_LOG_ERR("cannot mark file");
                    return 0;
                }
            }
            break;
    }

    return 1;
}


/* mark all names in stack except 0::BOGUSNAME */
static
int _xpost_garbage_mark_names(Xpost_Context *ctx,
                              Xpost_Memory_File *mem,
                              unsigned int stackadr,
                              int markall)
{
    int start = 1;
    if (!mem) return 0;

    {
        Xpost_Stack *s = (Xpost_Stack *)(mem->base + stackadr);
        unsigned int i;

#ifdef DEBUG_GC
        printf("marking stack of size %u\n", xpost_stack_count(mem, stackadr));
#endif

next:
        for (i = start; i < s->top; i++)
        {
            if (!_xpost_garbage_mark_object(ctx, mem, s->data[i], markall))
                return 0;
        }
        if (i == XPOST_STACK_SEGMENT_SIZE) /* ie. s->top == XPOST_STACK_SEGMENT_SIZE */
        {
            if (s->nextseg == 0)
                return 0;
            s = (Xpost_Stack *)(mem->base + s->nextseg);
            start = 0;
            goto next;
        }
    }

    return 1;
}


/* mark all allocations referred to by objects in stack */
static
int _xpost_garbage_mark_stack(Xpost_Context *ctx,
                              Xpost_Memory_File *mem,
                              unsigned int stackadr,
                              int markall)
{
    if (!mem) return 0;

    {
        Xpost_Stack *s = (Xpost_Stack *)(mem->base + stackadr);
        unsigned int i;

#ifdef DEBUG_GC
        printf("marking stack of size %u\n", xpost_stack_count(mem, stackadr));
#endif

next:
        for (i = 0; i < s->top; i++)
        {
            Xpost_Memory_File *objmem;
            objmem = xpost_context_select_memory(ctx, s->data[i]);
            if (objmem == mem || markall)
                if (!_xpost_garbage_mark_object(ctx, objmem, s->data[i], markall))
                    return 0;
        }
        if (i == XPOST_STACK_SEGMENT_SIZE) /* ie. s->top == XPOST_STACK_SEGMENT_SIZE */
        {
            if (s->nextseg == 0)
                return 0;
            s = (Xpost_Stack *)(mem->base + s->nextseg);
            goto next;
        }
    }

    return 1;
}

/* mark all allocations referred to by objects in save object's stack of saverec_'s */
static
int _xpost_garbage_mark_save_stack(Xpost_Context *ctx,
                                   Xpost_Memory_File *mem,
                                   unsigned int stackadr)
{
    if (!mem) return 0;

    {
        Xpost_Stack *s = (Xpost_Stack *)(mem->base + stackadr);
        unsigned int i;
        unsigned int ad;
        int ret;
        (void)ctx;

#ifdef DEBUG_GC
        printf("marking saverec stack of size %u\n", xpost_stack_count(mem, stackadr));
#endif

next:
        for (i = 0; i < s->top; i++)
        {
            /* _xpost_garbage_mark_object(ctx, mem, s->data[i]); */
            /* _xpost_garbage_mark_save_stack(ctx, mem, s->data[i].save_.stk); */
            ret = _xpost_garbage_mark_ent(mem, s->data[i].saverec_.src);
            if (!ret)
            {
                XPOST_LOG_ERR("cannot mark array");
                return 0;
            }
            ret = _xpost_garbage_mark_ent(mem, s->data[i].saverec_.cpy);
            if (!ret)
            {
                XPOST_LOG_ERR("cannot mark array");
                return 0;
            }
            if (s->data[i].saverec_.tag == dicttype)
            {
                ret = xpost_memory_table_get_addr(mem, s->data[i].saverec_.src, &ad);
                if (!ret)
                {
                    XPOST_LOG_ERR("cannot retrieve address for ent %u",
                                  s->data[i].saverec_.src);
                    return 0;
                }
                if (!_xpost_garbage_mark_dict(ctx, mem, ad, 0))
                    return 0;
                ret = xpost_memory_table_get_addr(mem, s->data[i].saverec_.cpy, &ad);
                if (!ret)
                {
                    XPOST_LOG_ERR("cannot retrieve address for ent %u",
                                  s->data[i].saverec_.cpy);
                    return 0;
                }
                if (!_xpost_garbage_mark_dict(ctx, mem, ad, 0))
                    return 0;
            }
            if (s->data[i].saverec_.tag == arraytype)
            {
                unsigned int sz = s->data[i].saverec_.pad;
                ret = xpost_memory_table_get_addr(mem, s->data[i].saverec_.src, &ad);
                if (!ret)
                {
                    XPOST_LOG_ERR("cannot retrieve address for array ent %u",
                                  s->data[i].saverec_.src);
                    return 0;
                }
                if (!_xpost_garbage_mark_array(ctx, mem, ad, sz, 0))
                    return 0;
                ret = xpost_memory_table_get_addr(mem, s->data[i].saverec_.cpy, &ad);
                if (!ret)
                {
                    XPOST_LOG_ERR("cannot retrieve address for array ent %u",
                                  s->data[i].saverec_.cpy);
                    return 0;
                }
                if (!_xpost_garbage_mark_array(ctx, mem, ad, sz, 0))
                    return 0;
            }
        }
        if (i == XPOST_STACK_SEGMENT_SIZE) /* ie. s->top == XPOST_STACK_SEGMENT_SIZE */
        {
            s = (Xpost_Stack *)(mem->base + s->nextseg);
            goto next;
        }
    }

    return 1;
}

/* mark all allocations referred to by objects in save stack */
static
int _xpost_garbage_mark_save(Xpost_Context *ctx,
                             Xpost_Memory_File *mem,
                             unsigned int stackadr)
{
    if (!mem) return 0;
    {

        Xpost_Stack *s = (Xpost_Stack *)(mem->base + stackadr);
        unsigned int i;

#ifdef DEBUG_GC
        printf("marking save stack of size %u\n", xpost_stack_count(mem, stackadr));
#endif

    next:
        for (i = 0; i < s->top; i++)
        {
            /* _xpost_garbage_mark_object(ctx, mem, s->data[i]); */
            if (!_xpost_garbage_mark_save_stack(ctx, mem, s->data[i].save_.stk))
                return 0;
        }
        if (i == XPOST_STACK_SEGMENT_SIZE) /* ie. s->top == XPOST_STACK_SEGMENT_SIZE */
        {
            s = (void *)(mem->base + s->nextseg);
            goto next;
        }
    }
    return 1;
}

/* discard the free list.
   iterate through tables,
        if element is unmarked and not zero-sized,
            free it.
   return reclaimed size
 */
static
unsigned int _xpost_garbage_sweep(Xpost_Memory_File *mem)
{
    unsigned int zero = 0;
    unsigned int z;
    unsigned int i;
    unsigned int sz = 0;
    int ret;

    ret = xpost_memory_table_get_addr(mem, XPOST_MEMORY_TABLE_SPECIAL_FREE, &z); /* address of the free list head */
    if (!ret)
    {
        XPOST_LOG_ERR("cannot load free list head");
        return 0;
    }

    memcpy(mem->base+z, &zero, sizeof(unsigned int)); /* discard list */
    /* *(unsigned int *)(mem->base+z) = 0; */

#ifdef DEBUG_GC
    printf("freeing ");
#endif
    /* scan table */
    for (i = mem->start; i < mem->table.nextent; i++)
    {
        if (((mem->table.tab[i].mark & XPOST_MEMORY_TABLE_MARK_DATA_MARK_MASK) == 0) &&
            (mem->table.tab[i].sz != 0))
        {
#ifdef DEBUG_GC
            printf("%u ", i);
#endif
            if (mem->table.tab[i].tag == filetype)
                continue;
            ret = xpost_free_memory_ent(mem, i);
            if (ret < 0)
            {
                XPOST_LOG_ERR("cannot free ent");
                return sz;
            }
            sz += (unsigned int)ret;
        }
    }
#ifdef DEBUG_GC
    printf("\n");
#endif

    return sz;
}

/*
   determine GLOBAL/LOCAL
   clear all marks,
   mark all root stacks,
   sweep.
   return reclaimed size or -1 if error occured.
 */
int xpost_garbage_collect(Xpost_Memory_File *mem, int dosweep, int markall)
{
    unsigned int i;
    unsigned int *cid;
    Xpost_Context *ctx = NULL;
    int isglobal;
    unsigned int sz = 0;
    unsigned int ad;
    int ret;

    if (mem->interpreter_get_initializing()) /* do not collect while initializing */
        return 0;

    /* printf("\ncollect:\n"); */

    /* determine global/local */
    isglobal = 0;
    ret = xpost_memory_table_get_addr(mem,
                                      XPOST_MEMORY_TABLE_SPECIAL_CONTEXT_LIST, &ad);
    if (!ret)
    {
        XPOST_LOG_ERR("cannot load context list");
        return -1;
    }
    cid = (void *)(mem->base + ad);
    for (i = 0; i < MAXCONTEXT && cid[i]; i++)
    {
        ctx = mem->interpreter_cid_get_context(cid[i]);
        if (ctx->state != 0)
        {
            if (mem == ctx->gl)
            {
                isglobal = 1;
                break;
            }
        }
    }
    if (ctx == NULL)
    {
        XPOST_LOG_ERR("cannot find context");
        return -1;
    }
#ifdef DEBUG_GC
    printf("using cid=%d\n", ctx->id);
#endif

    if (isglobal)
    {
        dosweep = 0;
        return 0; /* do not perform global collections at this time */

        _xpost_garbage_unmark(mem);

        ret = xpost_memory_table_get_addr(mem,
                                          XPOST_MEMORY_TABLE_SPECIAL_SAVE_STACK, &ad);
        if (!ret)
        {
            XPOST_LOG_ERR("cannot load save stack for global memory");
            return -1;
        }
        if (!_xpost_garbage_mark_save(ctx, mem, ad))
            return -1;
        ret = xpost_memory_table_get_addr(mem,
                                          XPOST_MEMORY_TABLE_SPECIAL_NAME_STACK, &ad);
        if (!ret)
        {
            XPOST_LOG_ERR("cannot load name stack for global memory");
            return -1;
        }
        if (!_xpost_garbage_mark_names(ctx, mem, ad, markall))
            return -1;

        for (i = 0; i < MAXCONTEXT && cid[i]; i++)
        {
            ctx = mem->interpreter_cid_get_context(cid[i]);
            xpost_garbage_collect(ctx->lo, 0, markall);
        }

    }
    else /* local */
    {
        //printf("collect!\n");
        _xpost_garbage_unmark(mem);
        if (markall)
            _xpost_garbage_unmark(ctx->gl);

        ret = xpost_memory_table_get_addr(mem,
                                          XPOST_MEMORY_TABLE_SPECIAL_SAVE_STACK, &ad);
        if (!ret)
        {
            XPOST_LOG_ERR("cannot load save stack for local memory");
            return -1;
        }
        if (!_xpost_garbage_mark_save(ctx, mem, ad))
            return -1;
        ret = xpost_memory_table_get_addr(mem,
                                          XPOST_MEMORY_TABLE_SPECIAL_NAME_STACK, &ad);
        if (!ret)
        {
            XPOST_LOG_ERR("cannot load name stack for local memory");
            return -1;
        }
#ifdef DEBUG_GC
        printf("marking name stack\n");
#endif
        if (!_xpost_garbage_mark_names(ctx, mem, ad, markall))
            return -1;

        for (i = 0; i < MAXCONTEXT && cid[i]; i++)
        {
            ctx = mem->interpreter_cid_get_context(cid[i]);

#ifdef DEBUG_GC
            printf("marking os\n");
#endif
            if (!_xpost_garbage_mark_stack(ctx, mem, ctx->os, markall))
                return -1;

#ifdef DEBUG_GC
            printf("marking ds\n");
#endif
            if (!_xpost_garbage_mark_stack(ctx, mem, ctx->ds, markall))
                return -1;

#ifdef DEBUG_GC
            printf("marking es\n");
#endif
            if (!_xpost_garbage_mark_stack(ctx, mem, ctx->es, markall))
                return -1;

#ifdef DEBUG_GC
            printf("marking hold\n");
#endif
            if (!_xpost_garbage_mark_stack(ctx, mem, ctx->hold, markall))
                return -1;
#ifdef DEBUG_GC
            printf("marking window device\n");
#endif
            if (!_xpost_garbage_mark_object(ctx, mem, ctx->window_device, markall))
                return -1;
#if 0
#ifdef DEBUG_GC
            printf("marking event handler\n");
#endif
            if (!_xpost_garbage_mark_object(ctx, mem, ctx->event_handler, markall))
                return -1;
#endif
        }
    }

    if (dosweep) {
#ifdef DEBUG_GC
        printf("sweep\n");
#endif
        sz += _xpost_garbage_sweep(mem);
        if (isglobal)
        {
            for (i = 0; i < MAXCONTEXT && cid[i]; i++)
            {
#ifdef DEBUG_GC
                printf("sweep context(%d)->gl\n", cid[i]);
#endif
                ctx = mem->interpreter_cid_get_context(cid[i]);
                sz += _xpost_garbage_sweep(ctx->lo);
            }
        }
    }

    printf("collect recovered %u bytes\n", sz);
    return sz;
}

#if 0

static
Xpost_Context *ctx;

static
int init_test_garbage(int (*xpost_interpreter_cid_init)(unsigned int *cid),
                      Xpost_Context *(*xpost_interpreter_cid_get_context)(unsigned int cid),
                      int (*xpost_interpreter_get_initializing)(void),
                      void (*xpost_interpreter_set_initializing)(int),
                      Xpost_Memory_File *(*xpost_interpreter_alloc_global_memory)(void),
                      Xpost_Memory_File *(*xpost_interpreter_alloc_local_memory)(void))
{
    int fd;
    unsigned int cid;
    char fname[] = "xmemXXXXXX";
    unsigned int tadr;
    int ret;
    unsigned int ent;

    /* create interpreter and context */
    itpdata = malloc(sizeof*itpdata);
    if (!itpdata) return 0;
    memset(itpdata, 0, sizeof*itpdata);
    ret = xpost_interpreter_cid_init(&cid);
    if (!ret)
        return 0;
    ctx = xpost_interpreter_cid_get_context(cid);
    ctx->id = cid;

    /* create global memory file */
    ctx->gl = xpost_interpreter_alloc_global_memory();
    if (ctx->gl == NULL)
    {
        return 0;
    }
    fd = mkstemp(fname);
    ret = xpost_memory_file_init(ctx->gl, fname, fd, xpost_interpreter_cid_get_context,
            xpost_interpreter_get_initializing, xpost_interpreter_set_initializing);
    if (!ret)
    {
        close(fd);
        return 0;
    }
    ret = xpost_memory_table_init(ctx->gl);
    if (!ret)
    {
        xpost_memory_file_exit(ctx->gl);
        return 0;
    }
    ret = xpost_free_init(ctx->gl);
    if (!ret)
    {
        xpost_memory_file_exit(ctx->gl);
        return 0;
    }
    ret = xpost_save_init(ctx->gl);
    if (!ret)
    {
        xpost_memory_file_exit(ctx->gl);
        return 0;
    }
    ret = xpost_context_init_ctxlist(ctx->gl);
    if (!ret)
    {
        xpost_memory_file_exit(ctx->gl);
        return 0;
    }
    xpost_context_append_ctxlist(ctx->gl, ctx->id);
    ctx->gl->start = XPOST_MEMORY_TABLE_SPECIAL_OPERATOR_TABLE + 1;

    /* create local memory file */
    ctx->lo = xpost_interpreter_alloc_local_memory();
    if (ctx->lo == NULL)
    {
        xpost_memory_file_exit(ctx->gl);
        return 0;
    }
    strcpy(fname, "xmemXXXXXX");
    fd = mkstemp(fname);
    ret = xpost_memory_file_init(ctx->lo, fname, fd, xpost_interpreter_cid_get_context,
            xpost_interpreter_get_initializing, xpost_interpreter_set_initializing);
    if (!ret)
    {
        close(fd);
        xpost_memory_file_exit(ctx->gl);
        return 0;
    }
    ret = xpost_memory_table_init(ctx->lo);
    if (!ret)
    {
        xpost_memory_file_exit(ctx->gl);
        xpost_memory_file_exit(ctx->lo);
        return 0;
    }
    ret = xpost_free_init(ctx->lo);
    if (!ret)
    {
        xpost_memory_file_exit(ctx->gl);
        xpost_memory_file_exit(ctx->lo);
        return 0;
    }
    ret = xpost_save_init(ctx->lo);
    if (!ret)
    {
        xpost_memory_file_exit(ctx->gl);
        xpost_memory_file_exit(ctx->lo);
        return 0;
    }
    ret = xpost_context_init_ctxlist(ctx->lo);
    if (!ret)
    {
        xpost_memory_file_exit(ctx->gl);
        xpost_memory_file_exit(ctx->lo);
        return 0;
    }
    xpost_context_append_ctxlist(ctx->lo, ctx->id);
    ctx->lo->start = XPOST_MEMORY_TABLE_SPECIAL_BOGUS_NAME + 1;

    /* create names in both mfiles */
    ret = xpost_name_init(ctx);
    if (!ret)
    {
        xpost_memory_file_exit(ctx->gl);
        xpost_memory_file_exit(ctx->lo);
        return 0;
    }

    /* create global OPTAB */
    ctx->vmmode = GLOBAL;
    /*ret = initoptab(ctx);*/ /* NO! only need to allocate something in the table slot */
    ret = xpost_memory_table_alloc(ctx->gl, 1024, 0, &ent);
    if (!ret)
    {
        xpost_memory_file_exit(ctx->gl);
        xpost_memory_file_exit(ctx->lo);
        return 0;
    }
    /* ... no initop(). don't need operators for this. */

    /* only need one stack */
    ctx->vmmode = LOCAL;
    xpost_stack_init(ctx->lo, &ctx->hold);
    ctx->os = ctx->ds = ctx->es = ctx->hold;

    ctx->gl->interpreter_set_initializing(0); /* garbage collector won't run otherwise */

    return 1;
}

static
void exit_test_garbage(void)
{
    xpost_memory_file_exit(ctx->lo);
    xpost_memory_file_exit(ctx->gl);
    free(itpdata);
    itpdata = NULL;

    ctx->gl->interpreter_set_initializing(1);
}

static
int _clear_hold(Xpost_Context *_ctx)
{
    //Xpost_Stack *s = (Xpost_Stack *)(_ctx->lo->base + _ctx->hold);
    //s->top = 0;
    xpost_stack_clear(_ctx->lo, _ctx->hold);
    return 1;
}

int test_garbage_collect(int (*xpost_interpreter_cid_init)(unsigned int *cid),
                         Xpost_Context *(*xpost_interpreter_cid_get_context)(unsigned int cid),
                         int (*xpost_interpreter_get_initializing)(void),
                         void (*xpost_interpreter_set_initializing)(int),
                         Xpost_Memory_File *(*xpost_interpreter_alloc_local_memory)(void),
                         Xpost_Memory_File *(*xpost_interpreter_alloc_global_memory)(void))
{
    if (!init_test_garbage(xpost_interpreter_cid_init,
                           xpost_interpreter_cid_get_context,
                           xpost_interpreter_get_initializing,
                           xpost_interpreter_set_initializing,
                           xpost_interpreter_alloc_local_memory,
                           xpost_interpreter_alloc_global_memory))
        return 0;

    {
        Xpost_Object str;
        unsigned int pre, post, sz, ret;

        pre = ctx->lo->used;
        str = xpost_string_cons(ctx, 7, "0123456");
        post = ctx->lo->used;
        sz = post-pre;
        /* printf("str sz=%u\n", sz); */

        xpost_stack_push(ctx->lo, ctx->os, str);
        _clear_hold(ctx);
        ret = collect(ctx->lo, 1, 0);
        //assert(ret == 0);
        if (ret != 0)
        {
            XPOST_LOG_ERR("Warning: collect returned %d, expected %d", ret, 0);
        }

        xpost_stack_pop(ctx->lo, ctx->os);
        _clear_hold(ctx);
        ret = collect(ctx->lo, 1, 0);
        /* printf("collect returned %u\n", ret); */
        //assert(ret >= sz);
        if (! (ret >= sz) )
        {
            XPOST_LOG_ERR("Warning: collect returned %d, expected >= %d", ret, sz);
        }
    }
    {
        Xpost_Object arr;
        unsigned int pre, post, sz, ret;

        pre = ctx->lo->used;
        arr = xpost_array_cons(ctx, 5);
        xpost_array_put(ctx, arr, 0, xpost_int_cons(12));
        xpost_array_put(ctx, arr, 1, xpost_int_cons(13));
        xpost_array_put(ctx, arr, 2, xpost_int_cons(14));
        xpost_array_put(ctx, arr, 3, xpost_string_cons(ctx, 5, "fubar"));
        xpost_array_put(ctx, arr, 4, xpost_string_cons(ctx, 4, "buzz"));
        post = ctx->lo->used;
        sz = post-pre;

        xpost_stack_push(ctx->lo, ctx->os, arr);
        _clear_hold(ctx);
        ret = collect(ctx->lo, 1, 0);
        //assert(ret == 0);
        if (ret != 0)
        {
            XPOST_LOG_ERR("Warning: collect returned %d, expected %d", ret, 0);
        }

        xpost_stack_pop(ctx->lo, ctx->os);
        _clear_hold(ctx);
        ret = collect(ctx->lo, 1, 0);
        //assert(ret >= sz);
        if (! (ret >= sz) )
        {
            XPOST_LOG_ERR("Warning: collect returned %d, expected >= %d", ret, sz);
        }

    }
    exit_test_garbage();
    return 1;
}

#endif

#ifdef TESTMODULE_GC

Xpost_Context *ctx;
Xpost_Memory_File *mem;
unsigned int stac;


/* void init(void) { */
/*     xpost_memory_file_init(&mem, "x.mem"); */
/*     (void)xpost_memory_table_init(&mem); */
/*     xpost_free_init(&mem); */
/*     xpost_save_init(&mem); */
/*     xpost_context_init_ctxlist(&mem); */
/*     Xpost_Memory_Table *tab = &mem->table; */
/*     unsigned int ent = xpost_memory_table_alloc(&mem, 0, 0); */
/*     stac = mem->table.tab[ent].adr = initstack(&mem); */
/*     /\* mem.roots[0] = XPOST_MEMORY_TABLE_SPECIAL_SAVE_STACK; *\/ */
/*     /\* mem.roots[1] = ent; *\/ */
/*     mem.start = ent+1; */
/* } */


extern Xpost_Interpreter *itpdata;

void init(void) {
    itpdata = malloc(sizeof*itpdata);
    memset(itpdata, 0, sizeof*itpdata);
    xpost_interpreter_init(itpdata);
}

int main(void)
{
    if (!xpost_init())
    {
        fprintf(stderr, "Fail to initialize xpost dict test\n");
        return -1;
    }

    init();
    printf("\n^test gc.c\n");
    ctx = &itpdata->ctab[0];
    mem = ctx->lo;
    stac = ctx->os;

    xpost_stack_push(mem, stac, xpost_int_cons(5));
    xpost_stack_push(mem, stac, xpost_int_cons(6));
    xpost_stack_push(mem, stac, xpost_real_cons(7.0));
    Xpost_Object ar;
    ar = xpost_array_cons_memory(mem, 3);
    int i;
    for (i=0; i < 3; i++)
        xpost_array_put_memory(mem, ar, i, xpost_stack_pop(mem, stac));
    xpost_stack_push(mem, stac, ar);                   /* array on stack */

    xpost_stack_push(mem, stac, xpost_int_cons(1));
    xpost_stack_push(mem, stac, xpost_int_cons(2));
    xpost_stack_push(mem, stac, xpost_int_cons(3));
    ar = xpost_array_cons_memory(mem, 3);
    for (i=0; i < 3; i++)
        xpost_array_put_memory(mem, ar, i, xpost_stack_pop(mem, stac));
    xpost_object_dump(ar);
    /* array not on stack */

#define CNT_STR(x) sizeof(x), x
    xpost_stack_push(mem, stac, xpost_string_cons_memory(mem, CNT_STR("string on stack")));

    xpost_object_dump(xpost_string_cons_memory(mem, CNT_STR("string not on stack")));

    collect(mem);
    xpost_stack_push(mem, stac, xpost_string_cons_memory(mem, CNT_STR("string on stack")));
    xpost_object_dump(xpost_string_cons_memory(mem, CNT_STR("string not on stack")));

    collect(mem);
    xpost_memory_file_dump(mem);
    printf("stackaedr: %04x\n", stac);
    dumpmtab(mem, 0);
    /*     ^ent 8 (8): adr 3404 0x0d4c, sz [24], mark _ */
    /*     ^ 06  00  00  00  6en 67g 20  6en 6fo 74t 20 */
    printf("gc: look at the mark field . . . . . . . .^\n");
    printf("also, see that the first 4 bytes of strings not on stack\n"
           "have been obliterated to link-up the free list.\n");

    xpost_quit();

// WTF???  }
    return 0;
}

#endif
