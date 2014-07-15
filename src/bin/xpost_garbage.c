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
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h> /* close */
#endif

#include "xpost_compat.h" /* mkstemp */
#include "xpost_log.h"
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

#include "xpost_interpreter.h"
//#include "xpost_operator.h"
#include "xpost_garbage.h"

#ifdef DEBUG_GC
#include <stdio.h>
#endif


/* iterate through all tables,
    clear the MARK in the mark. */
static
void unmark(Xpost_Memory_File *mem)
{
    Xpost_Memory_Table *tab = (void *)(mem->base);
    unsigned int i;

    for (i = mem->start; i < tab->nextent; i++) {
        tab->tab[i].mark &= ~XPOST_MEMORY_TABLE_MARK_DATA_MARK_MASK;
    }
    while (tab->nexttab != 0) {
        tab = (void *)(mem->base + tab->nexttab);

        for (i = 0; i < tab->nextent; i++) {
            tab->tab[i].mark &= ~XPOST_MEMORY_TABLE_MARK_DATA_MARK_MASK;
        }
    }
}

/* set the MARK in the mark in the tab[ent] */
static
int markent(Xpost_Memory_File *mem,
        unsigned int ent)
{
    Xpost_Memory_Table *tab;

    if (ent < mem->start)
        return 1;

    tab = (void *)(mem->base);

    if (!xpost_memory_table_find_relative(mem,&tab,&ent))
    {
        XPOST_LOG_ERR("cannot find table for ent %u", ent);
        return 0;
    }
    tab->tab[ent].mark |= XPOST_MEMORY_TABLE_MARK_DATA_MARK_MASK;
    return 1;
}

/* is it marked? */
static
int marked(Xpost_Memory_File *mem,
        unsigned int ent, int *retval)
{
    Xpost_Memory_Table *tab = (void *)(mem->base);
    if (!xpost_memory_table_find_relative(mem,&tab,&ent))
    {
        XPOST_LOG_ERR("cannot find table for ent %u", ent);
        return 0;
    }
    *retval = (tab->tab[ent].mark & XPOST_MEMORY_TABLE_MARK_DATA_MARK_MASK) >> XPOST_MEMORY_TABLE_MARK_DATA_MARK_OFFSET;
    return 1;
}

/* recursively mark an object */
static
int markobject(Xpost_Context *ctx, Xpost_Memory_File *mem, Xpost_Object o, int markall);

/* recursively mark a dictionary */
static
int markdict(Xpost_Context *ctx,
        Xpost_Memory_File *mem,
        unsigned int adr,
        int markall)
{
    dichead *dp = (void *)(mem->base + adr);
    Xpost_Object *tp = (void *)(mem->base + adr + sizeof(dichead));
    int j;

    for (j=0; j < DICTABN(dp->sz); j++) {
        if (!markobject(ctx, mem, tp[j], markall))
            return 0;
    }
    return 1;
}

/* recursively mark all elements of array */
static
int markarray(Xpost_Context *ctx,
        Xpost_Memory_File *mem,
        unsigned int adr,
        unsigned int sz,
        int markall)
{
    Xpost_Object *op = (void *)(mem->base + adr);
    unsigned int j;

    for (j=0; j < sz; j++) {
        if (!markobject(ctx, mem, op[j], markall))
            return 0;
    }
    return 1;
}

/* traverse the contents of composite objects
   if markall is true, this is a collection of global vm,
   so we must mark objects and recurse
   even if it means switching memory files
 */
static
int markobject(Xpost_Context *ctx,
        Xpost_Memory_File *mem,
        Xpost_Object o,
        int markall)
{
    unsigned int ad;
    int ret;

    switch(xpost_object_get_type(o)) {
    default: break;

    case arraytype:
#ifdef DEBUG_GC
    printf("markobject: %s %d\n", xpost_object_type_names[xpost_object_get_type(o)], o.comp_.sz);
#endif
        if (xpost_context_select_memory(ctx, o) != mem) {
            if (markall)
                mem = xpost_context_select_memory(ctx, o);
            else
                break;
        }
        if (!mem) return 0;
        if (!marked(mem, xpost_object_get_ent(o), &ret))
            return 0;
        if (!ret) {
            ret = markent(mem, xpost_object_get_ent(o));
            if (!ret)
            {
                XPOST_LOG_ERR("cannot mark array");
                return 0;
            }
            ret = xpost_memory_table_get_addr(mem, xpost_object_get_ent(o), &ad);
            if (!ret)
            {
                XPOST_LOG_ERR("cannot retrieve address for array ent %u", xpost_object_get_ent(o));
                return 0;
            }
            if (!markarray(ctx, mem, ad, o.comp_.sz, markall))
                return 0;
        }
        break;

    case dicttype:
#ifdef DEBUG_GC
    printf("markobject: %s %d\n", xpost_object_type_names[xpost_object_get_type(o)], o.comp_.sz);
#endif
        if (xpost_context_select_memory(ctx, o) != mem) {
            if (markall)
                mem = xpost_context_select_memory(ctx, o);
            else
                break;
        }
        if (!marked(mem, xpost_object_get_ent(o), &ret))
            return 0;
        if (!ret) {
            ret = markent(mem, xpost_object_get_ent(o));
            if (!ret)
            {
                XPOST_LOG_ERR("cannot mark array");
                return 0;
            }
            ret = xpost_memory_table_get_addr(mem, xpost_object_get_ent(o), &ad);
            if (!ret)
            {
                XPOST_LOG_ERR("cannot retrieve address for dict ent %u", xpost_object_get_ent(o));
                return 0;
            }
            if (!markdict(ctx, mem, ad, markall))
                return 0;
        }
        break;

    case stringtype:
#ifdef DEBUG_GC
    printf("markobject: %s %d\n", xpost_object_type_names[xpost_object_get_type(o)], o.comp_.sz);
#endif
        if (xpost_context_select_memory(ctx, o) != mem) {
            if (markall)
                mem = xpost_context_select_memory(ctx, o);
            else
                break;
        }
        ret = markent(mem, xpost_object_get_ent(o));
        if (!ret)
        {
            XPOST_LOG_ERR("cannot mark array");
            return 0;
        }
        break;

    case filetype:
        if (mem == ctx->gl) {
            printf("file found in global vm\n");
        } else {
            ret = markent(mem, o.mark_.padw);
            if (!ret)
            {
                XPOST_LOG_ERR("cannot mark array");
                return 0;
            }
        }
        break;
    }
    return 1;
}

/* mark all allocations referred to by objects in stack */
static
int markstack(Xpost_Context *ctx,
        Xpost_Memory_File *mem,
        unsigned int stackadr,
        int markall)
{
    Xpost_Stack *s = (Xpost_Stack *)(mem->base + stackadr);
    unsigned int i;

#ifdef DEBUG_GC
    printf("marking stack of size %u\n", s->top);
#endif

next:
    for (i=0; i < s->top; i++) {
        if (!markobject(ctx, mem, s->data[i], markall))
            return 0;
    }
    if (i==XPOST_STACK_SEGMENT_SIZE) { /* ie. s->top == XPOST_STACK_SEGMENT_SIZE */
        if (s->nextseg == 0)
            return 0;
        s = (Xpost_Stack *)(mem->base + s->nextseg);
        goto next;
    }

    /* if (s->nextseg) { /\* maybe not. this is a MARK phase, after all *\/ */
    /*     xpost_stack_free(mem, s->nextseg); */
    /*     s->nextseg = 0; */
    /* } */
    return 1;
}

/* mark all allocations referred to by objects in save object's stack of saverec_'s */
static
int marksavestack(Xpost_Context *ctx,
        Xpost_Memory_File *mem,
        unsigned int stackadr)
{
    Xpost_Stack *s = (Xpost_Stack *)(mem->base + stackadr);
    unsigned int i;
    unsigned int ad;
    int ret;
    (void)ctx;

#ifdef DEBUG_GC
    printf("marking save stack of size %u\n", s->top);
#endif

next:
    for (i=0; i < s->top; i++) {
        /* markobject(ctx, mem, s->data[i]); */
        /* marksavestack(ctx, mem, s->data[i].save_.stk); */
        ret = markent(mem, s->data[i].saverec_.src);
        if (!ret)
        {
            XPOST_LOG_ERR("cannot mark array");
            return 0;
        }
        ret = markent(mem, s->data[i].saverec_.cpy);
        if (!ret)
        {
            XPOST_LOG_ERR("cannot mark array");
            return 0;
        }
        if (s->data[i].saverec_.tag == dicttype) {
            ret = xpost_memory_table_get_addr(mem, s->data[i].saverec_.src, &ad);
            if (!ret)
            {
                XPOST_LOG_ERR("cannot retrieve address for ent %u",
                        s->data[i].saverec_.src);
                return 0;
            }
            if (!markdict(ctx, mem, ad, 0))
                return 0;
            ret = xpost_memory_table_get_addr(mem, s->data[i].saverec_.cpy, &ad);
            if (!ret)
            {
                XPOST_LOG_ERR("cannot retrieve address for ent %u",
                        s->data[i].saverec_.cpy);
                return 0;
            }
            if (!markdict(ctx, mem, ad, 0))
                return 0;
        }
        if (s->data[i].saverec_.tag == arraytype) {
            unsigned int sz = s->data[i].saverec_.pad;
            ret = xpost_memory_table_get_addr(mem, s->data[i].saverec_.src, &ad);
            if (!ret)
            {
                XPOST_LOG_ERR("cannot retrieve address for array ent %u",
                        s->data[i].saverec_.src);
                return 0;
            }
            if (!markarray(ctx, mem, ad, sz, 0))
                return 0;
            ret = xpost_memory_table_get_addr(mem, s->data[i].saverec_.cpy, &ad);
            if (!ret)
            {
                XPOST_LOG_ERR("cannot retrieve address for array ent %u",
                        s->data[i].saverec_.cpy);
                return 0;
            }
            if (!markarray(ctx, mem, ad, sz, 0))
                return 0;
        }
    }
    if (i==XPOST_STACK_SEGMENT_SIZE) { /* ie. s->top == XPOST_STACK_SEGMENT_SIZE */
        s = (Xpost_Stack *)(mem->base + s->nextseg);
        goto next;
    }

    if (s->nextseg) {
        xpost_stack_free(mem, s->nextseg);
        s->nextseg = 0;
    }
    return 1;
}

/* mark all allocations referred to by objects in save stack */
static
int marksave(Xpost_Context *ctx,
        Xpost_Memory_File *mem,
        unsigned int stackadr)
{
    Xpost_Stack *s = (Xpost_Stack *)(mem->base + stackadr);
    unsigned int i;

#ifdef DEBUG_GC
    printf("marking save stack of size %u\n", s->top);
#endif

next:
    for (i=0; i < s->top; i++) {
        /* markobject(ctx, mem, s->data[i]); */
        if (!marksavestack(ctx, mem, s->data[i].save_.stk))
            return 0;
    }
    if (i==XPOST_STACK_SEGMENT_SIZE) { /* ie. s->top == XPOST_STACK_SEGMENT_SIZE */
        s = (void *)(mem->base + s->nextseg);
        goto next;
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
unsigned int sweep(Xpost_Memory_File *mem)
{
    Xpost_Memory_Table *tab;
    int ntab;
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

    /* scan first table */
    tab = (void *)(mem->base);
    ntab = 0;
    for (i = mem->start; i < tab->nextent; i++) {
        if ( (tab->tab[i].mark & XPOST_MEMORY_TABLE_MARK_DATA_MARK_MASK) == 0
                && tab->tab[i].sz != 0)
        {
            ret = xpost_free_memory_ent(mem, i);
            if (ret < 0)
            {
                XPOST_LOG_ERR("cannot free ent");
                return sz;
            }
            sz += (unsigned int)ret;
        }
    }

    /* scan linked tables */
    while (i < XPOST_MEMORY_TABLE_SIZE && tab->nexttab != 0) {
        tab = (void *)(mem->base + tab->nexttab);
        ++ntab;

        for (i = mem->start; i < tab->nextent; i++) {
            if ( (tab->tab[i].mark & XPOST_MEMORY_TABLE_MARK_DATA_MARK_MASK) == 0
                    && tab->tab[i].sz != 0)
            {
                ret = xpost_free_memory_ent(mem, i + ntab*XPOST_MEMORY_TABLE_SIZE);
                if (ret < 0)
                {
                    XPOST_LOG_ERR("cannot free ent");
                    return sz;
                }
                sz += (unsigned int)ret;
            }
        }
    }

    return sz;
}

/* clear all marks,
   determine GLOBAL/LOCAL and mark all root stacks,
   sweep.
   return reclaimed size or -1 if error occured.
 */
int collect(Xpost_Memory_File *mem, int dosweep, int markall)
{
    unsigned int i;
    unsigned int *cid;
    Xpost_Context *ctx = NULL;
    int isglobal;
    unsigned int sz = 0;
    unsigned int ad;
    int ret;

    if (initializing)
        return 0;

    /* printf("\ncollect:\n"); */

    /* determine global/glocal */
    isglobal = 0;
    ret = xpost_memory_table_get_addr(mem,
            XPOST_MEMORY_TABLE_SPECIAL_CONTEXT_LIST, &ad);
    if (!ret)
    {
        XPOST_LOG_ERR("cannot load context list");
        return -1;
    }
    cid = (void *)(mem->base + ad);
    for (i = 0; i < MAXCONTEXT && cid[i]; i++) {
        ctx = xpost_interpreter_cid_get_context(cid[i]);
        if (mem == ctx->gl) {
            isglobal = 1;
            break;
        }
    }

    if (isglobal) {
        return 0; /* do not perform global collections at this time */

        unmark(mem);

        ret = xpost_memory_table_get_addr(mem,
                XPOST_MEMORY_TABLE_SPECIAL_SAVE_STACK, &ad);
        if (!ret)
        {
            XPOST_LOG_ERR("cannot load save stack for %s memory",
                    mem == ctx->gl? "global" : "local");
            return -1;
        }
        if (!marksave(ctx, mem, ad))
            return -1;
        ret = xpost_memory_table_get_addr(mem,
                XPOST_MEMORY_TABLE_SPECIAL_NAME_STACK, &ad);
        if (!ret)
        {
            XPOST_LOG_ERR("cannot load name stack for %s memory",
                    mem == ctx->gl? "global" : "local");
            return -1;
        }
        if (!markstack(ctx, mem, ad, markall))
            return -1;

        for (i = 0; i < MAXCONTEXT && cid[i]; i++) {
            ctx = xpost_interpreter_cid_get_context(cid[i]);
            collect(ctx->lo, 0, markall);
        }

    } else { /* local */
        printf("collect!\n");
        unmark(mem);

        ret = xpost_memory_table_get_addr(mem,
                XPOST_MEMORY_TABLE_SPECIAL_SAVE_STACK, &ad);
        if (!ret)
        {
            XPOST_LOG_ERR("cannot load save stack for %s memory",
                    mem == ctx->gl? "global" : "local");
            return -1;
        }
        if (!marksave(ctx, mem, ad))
            return -1;
        ret = xpost_memory_table_get_addr(mem, 
                XPOST_MEMORY_TABLE_SPECIAL_NAME_STACK, &ad);
        if (!ret)
        {
            XPOST_LOG_ERR("cannot load name stack for %s memory",
                    mem == ctx->gl? "global" : "local");
            return -1;
        }
        if (!markstack(ctx, mem, ad, markall))
            return -1;

        for (i = 0; i < MAXCONTEXT && cid[i]; i++) {
            ctx = xpost_interpreter_cid_get_context(cid[i]);

#ifdef DEBUG_GC
            printf("marking os\n");
#endif
            if (!markstack(ctx, mem, ctx->os, markall))
                return -1;

#ifdef DEBUG_GC
            printf("marking ds\n");
#endif
            if (!markstack(ctx, mem, ctx->ds, markall))
                return -1;

#ifdef DEBUG_GC
            printf("marking es\n");
#endif
            if (!markstack(ctx, mem, ctx->es, markall))
                return -1;

#ifdef DEBUG_GC
            printf("marking hold\n");
#endif
            if (!markstack(ctx, mem, ctx->hold, markall))
                return -1;
        }
    }

    if (dosweep) {
#ifdef DEBUG_GC
        printf("sweep\n");
#endif
        sz += sweep(mem);
        if (isglobal) {
            for (i = 0; i < MAXCONTEXT && cid[i]; i++) {
                ctx = xpost_interpreter_cid_get_context(cid[i]);
                sz += sweep(ctx->lo);
            }
        }
    }

    printf("collect recovered %u bytes\n", sz);
    return sz;
}

static
Xpost_Context *ctx;

static
int init_test_garbage(int (*xpost_interpreter_cid_init)(unsigned int *cid),
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
    ret = xpost_memory_file_init(ctx->gl, fname, fd);
    if (!ret)
    {
        close(fd);
        return 0;
    }
    ret = xpost_memory_table_init(ctx->gl, &tadr);
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
    ret = xpost_memory_file_init(ctx->lo, fname, fd);
    if (!ret)
    {
        close(fd);
        xpost_memory_file_exit(ctx->gl);
        return 0;
    }
    ret = xpost_memory_table_init(ctx->lo, &tadr);
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
    //ret = initoptab(ctx);
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

    initializing = 0; /* garbage collector won't run otherwise */

    return 1;
}

static
void exit_test_garbage(void)
{
    xpost_memory_file_exit(ctx->lo);
    xpost_memory_file_exit(ctx->gl);
    free(itpdata);
    itpdata = NULL;

    initializing = 1;
}

static
int _clear_hold(Xpost_Context *_ctx)
{
    Xpost_Stack *s = (Xpost_Stack *)(_ctx->lo->base + _ctx->hold);
    s->top = 0;
    return 1;
}

int test_garbage_collect(int (*xpost_interpreter_cid_init)(unsigned int *cid),
                         Xpost_Memory_File *(*xpost_interpreter_alloc_local_memory)(void),
                         Xpost_Memory_File *(*xpost_interpreter_alloc_global_memory)(void))
{
    if (!init_test_garbage(xpost_interpreter_cid_init,
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
/*     Xpost_Memory_Table *tab = (void *)mem.base; */
/*     unsigned int ent = xpost_memory_table_alloc(&mem, 0, 0); */
/*     /\* xpost_memory_table_find_relative(&mem, &tab, &ent); *\/ */
/*     stac = tab->tab[ent].adr = initstack(&mem); */
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

    return 0;
}

#endif
