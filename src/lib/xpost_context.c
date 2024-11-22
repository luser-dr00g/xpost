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
#include <stdio.h> /* FILE* */
#include <string.h> /* memset */

#ifdef HAVE_UNISTD_H
# include <unistd.h> /* close */
#endif

#ifdef _WIN32
# include <io.h> /* close */
#endif

#include "xpost.h"
#include "xpost_compat.h" /* xpost_mkstemp */
#include "xpost_object.h"
#include "xpost_memory.h"
#include "xpost_stack.h"
#include "xpost_free.h"  //  initializes free list
#include "xpost_error.h"
#include "xpost_save.h"  // initializes save/restore stacks

#include "xpost_context.h"

/* initialize the context list
   special entity in the mfile */
int xpost_context_init_ctxlist(Xpost_Memory_File *mem)
{
    unsigned int ent;
    Xpost_Memory_Table *tab;
    int ret;

    ret = xpost_memory_table_alloc(mem, MAXCONTEXT * sizeof(unsigned int), 0, &ent);
    if (!ret)
    {
        return 0; /* was unregistered error */
    }
    assert(ent == XPOST_MEMORY_TABLE_SPECIAL_CONTEXT_LIST);
    tab = &mem->table;
    memset(mem->base + tab->tab[XPOST_MEMORY_TABLE_SPECIAL_CONTEXT_LIST].adr, 0,
           MAXCONTEXT * sizeof(unsigned int));

    return 1;
}

/* add a context ID to the context list in mfile */
int xpost_context_append_ctxlist(Xpost_Memory_File *mem,
                  unsigned int cid)
{
    int i;
    Xpost_Memory_Table *tab;
    unsigned int *ctxlist;

    tab = &mem->table;
    ctxlist = (void *)(mem->base + tab->tab[XPOST_MEMORY_TABLE_SPECIAL_CONTEXT_LIST].adr);
    // find first empty
    for (i=0; i < MAXCONTEXT; i++)
    {
        if (ctxlist[i] == 0)
        {
            ctxlist[i] = cid;
            return 1;
        }
    }
    return 0;
}


/* build a stack, return address */
static
unsigned int makestack(Xpost_Memory_File *mem)
{
    unsigned int ret;
    xpost_stack_init(mem, &ret);
    return ret;
}

/* set up global vm in the context
 */
static
int initglobal(Xpost_Context *ctx,
               Xpost_Context *(*xpost_interpreter_cid_get_context)(unsigned int cid),
               int (*xpost_interpreter_get_initializing)(void),
               void (*xpost_interpreter_set_initializing)(int),
               Xpost_Memory_File *(*xpost_interpreter_alloc_global_memory)(void),
               int (*garbage_collect_function)(Xpost_Memory_File *mem, int dosweep, int markall))
{
    char g_filenam[] = "gmemXXXXXX";
    int fd;
    int ret;
    unsigned int safeadr;

    ctx->vmmode = GLOBAL;

    /* allocate and initialize global vm */
    //ctx->gl = malloc(sizeof(Xpost_Memory_File));
    //ctx->gl = &itpdata->gtab[0];
    ctx->gl = xpost_interpreter_alloc_global_memory();
    if (ctx->gl == NULL)
    {
        return 0;
    }

    if (!xpost_mkstemp(g_filenam, &fd))
    {
        return 0;
    }

    ret = xpost_memory_file_init(ctx->gl, g_filenam, fd, xpost_interpreter_cid_get_context,
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
    xpost_memory_file_alloc(ctx->gl, 64, &safeadr); //safety buffer
    ret = xpost_free_init(ctx->gl);
    if (!ret)
    {
        xpost_memory_file_exit(ctx->gl);
        return 0;
    }
    xpost_memory_register_garbage_collect_function(ctx->gl, garbage_collect_function);
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
    ret = xpost_context_append_ctxlist(ctx->gl, ctx->id);
    if (!ret)
    {
        xpost_memory_file_exit(ctx->gl);
        return 0;
    }

            /* so OPTAB is not collected and not scanned. */
    ctx->gl->start = XPOST_MEMORY_TABLE_SPECIAL_OPERATOR_TABLE + 1;

    return 1;
}


/* set up local vm in the context
   allocates all stacks
 */
static
int initlocal(Xpost_Context *ctx,
              Xpost_Context *(*xpost_interpreter_cid_get_context)(unsigned int cid),
              int (*xpost_interpreter_get_initializing)(void),
              void (*xpost_interpreter_set_initializing)(int),
              Xpost_Memory_File *(*xpost_interpreter_alloc_local_memory)(void),
              int (*garbage_collect_function)(Xpost_Memory_File *mem, int dosweep, int markall))
{
    char l_filenam[] = "lmemXXXXXX";
    int fd;
    int ret;
    unsigned int safeadr;

    ctx->vmmode = LOCAL;

    /* allocate and initialize local vm */
    //ctx->lo = malloc(sizeof(Xpost_Memory_File));
    //ctx->lo = &itpdata->ltab[0];
    ctx->lo = xpost_interpreter_alloc_local_memory();
    if (ctx->lo == NULL)
    {
        return 0;
    }

    if (!xpost_mkstemp(l_filenam, &fd))
    {
        return 0;
    }

    ret = xpost_memory_file_init(ctx->lo, l_filenam, fd, xpost_interpreter_cid_get_context,
            xpost_interpreter_get_initializing, xpost_interpreter_set_initializing);
    if (!ret)
    {
        close(fd);
        return 0;
    }

    ret = xpost_memory_table_init(ctx->lo);
    if (!ret)
    {
        xpost_memory_file_exit(ctx->lo);
        return 0;
    }
    xpost_memory_file_alloc(ctx->lo, 64, &safeadr); //safety buffer
    ret = xpost_free_init(ctx->lo);
    if (!ret)
    {
        xpost_memory_file_exit(ctx->lo);
        return 0;
    }
#ifndef XPOST_NO_GC
    xpost_memory_register_garbage_collect_function(ctx->lo, garbage_collect_function);
#endif
    ret = xpost_save_init(ctx->lo);
    if (!ret)
    {
        xpost_memory_file_exit(ctx->lo);
        return 0;
    }
    ret = xpost_context_init_ctxlist(ctx->lo);
    if (!ret)
    {
        xpost_memory_file_exit(ctx->lo);
        return 0;
    }
    ret = xpost_context_append_ctxlist(ctx->lo, ctx->id);
    if (!ret)
    {
        xpost_memory_file_exit(ctx->lo);
        return 0;
    }
    //ctx->lo->roots[0] = XPOST_MEMORY_TABLE_SPECIAL_SAVE_STACK;

    ctx->os = makestack(ctx->lo);
    ctx->es = makestack(ctx->lo);
    ctx->ds = makestack(ctx->lo);
    ctx->hold = makestack(ctx->lo);
    //ctx->lo->roots[1] = DS;
    //ctx->lo->start = HOLD + 1; /* so HOLD is not collected and not scanned. */
    //ctx->lo->start = XPOST_MEMORY_TABLE_SPECIAL_CONTEXT_LIST + 1;
    ctx->lo->start = XPOST_MEMORY_TABLE_SPECIAL_BOGUS_NAME + 1;

    return 1;
}


/* initialize context
   allocates operator table
   allocates systemdict
   populates systemdict and optab with operators
 */
int xpost_context_init(Xpost_Context *ctx,
                       int (*xpost_interpreter_cid_init)(unsigned int *cid),
                       Xpost_Context *(*xpost_interpreter_cid_get_context)(unsigned int cid),
                       int (*xpost_interpreter_get_initializing)(void),
                       void (*xpost_interpreter_set_initializing)(int),
                       Xpost_Memory_File *(*xpost_interpreter_alloc_local_memory)(void),
                       Xpost_Memory_File *(*xpost_interpreter_alloc_global_memory)(void),
                       int (*garbage_collect_function)(Xpost_Memory_File *mem, int dosweep, int markall))
{
    int ret;

    ret = xpost_interpreter_cid_init(&ctx->id);
    if (!ret)
        return 0;
    ctx->state = C_IDLE;

    ret = initlocal(ctx, xpost_interpreter_cid_get_context, 
            xpost_interpreter_get_initializing, xpost_interpreter_set_initializing, 
            xpost_interpreter_alloc_local_memory, garbage_collect_function);
    if (!ret)
    {
        return 0;
    }
    ret = initglobal(ctx, xpost_interpreter_cid_get_context, 
            xpost_interpreter_get_initializing, xpost_interpreter_set_initializing, 
            xpost_interpreter_alloc_global_memory, garbage_collect_function);
    if (!ret)
    {
        xpost_memory_file_exit(ctx->lo);
        return 0;
    }
    ctx->event_handler = null;
    ctx->ignoreinvalidaccess = 0;
    ctx->xpost_interpreter_cid_init = xpost_interpreter_cid_init;
    ctx->xpost_interpreter_alloc_local_memory = xpost_interpreter_alloc_local_memory;
    ctx->xpost_interpreter_alloc_global_memory = xpost_interpreter_alloc_global_memory;
    ctx->garbage_collect_function = garbage_collect_function;

    return 1;
}

/* destroy context
FIXME: delete cid from CTXLIST, destroy memory file when empty
 */
void xpost_context_exit(Xpost_Context *ctx)
{
    xpost_memory_file_exit(ctx->gl);
    xpost_memory_file_exit(ctx->lo);
}

/* return the appropriate global or local memory file for the composite object */
/*@dependent@*/
Xpost_Memory_File *xpost_context_select_memory(Xpost_Context *ctx,
                                               Xpost_Object o)
{
    return o.tag&XPOST_OBJECT_TAG_DATA_FLAG_BANK? ctx->gl : ctx->lo;
}


/* print a dump of the context struct */
void xpost_context_dump(Xpost_Context *ctx)
{
    xpost_memory_file_dump(ctx->gl);
    xpost_memory_table_dump(ctx->gl);
    xpost_memory_file_dump(ctx->lo);
    xpost_memory_table_dump(ctx->lo);
    /*dumpnames(ctx);*/
}

int xpost_context_install_event_handler(Xpost_Context *ctx,
                                        Xpost_Object operator,
                                        Xpost_Object device)
{
    ctx->event_handler = operator;
    ctx->window_device = device;
    return 1;
}

/*
   fork new process with private global and private local vm
   (spawn jobserver)
   */
unsigned int xpost_context_fork1(Xpost_Context *ctx,
                                 int (*xpost_interpreter_cid_init)(unsigned int *cid),
                                 Xpost_Context *(*xpost_interpreter_cid_get_context)(unsigned int cid),
                                 int (*xpost_interpreter_get_initializing)(void),
                                 void (*xpost_interpreter_set_initializing)(int),
                                 Xpost_Memory_File *(*xpost_interpreter_alloc_local_memory)(void),
                                 Xpost_Memory_File *(*xpost_interpreter_alloc_global_memory)(void),
                                 int (*garbage_collect_function)(Xpost_Memory_File *mem, int dosweep, int markall))
{
    unsigned int newcid;
    Xpost_Context *newctx;
    int ret;

    (void)ctx;
    ret = xpost_interpreter_cid_init(&newcid);
    if (!ret) return 0;
    newctx = xpost_interpreter_cid_get_context(newcid);
    *newctx = *ctx; // struct copy for defaults
    newctx->id = newcid;
    newctx->state = C_IDLE;
    initlocal(newctx, xpost_interpreter_cid_get_context, 
            xpost_interpreter_get_initializing, xpost_interpreter_set_initializing, 
            xpost_interpreter_alloc_local_memory, garbage_collect_function);
    initglobal(newctx, xpost_interpreter_cid_get_context, 
            xpost_interpreter_get_initializing, xpost_interpreter_set_initializing, 
            xpost_interpreter_alloc_global_memory, garbage_collect_function);
    newctx->vmmode = LOCAL;
    return newcid;
}

/*
   fork new process with shared global vm and private local vm
   (new "application"?)
   */
unsigned int xpost_context_fork2(Xpost_Context *ctx,
                                 int (*xpost_interpreter_cid_init)(unsigned int *cid),
                                 Xpost_Context *(*xpost_interpreter_cid_get_context)(unsigned int cid),
                                 int (*xpost_interpreter_get_initializing)(void),
                                 void (*xpost_interpreter_set_initializing)(int),
                                 Xpost_Memory_File *(*xpost_interpreter_alloc_local_memory)(void),
                                 Xpost_Memory_File *(*xpost_interpreter_alloc_global_memory)(void),
                                 int (*garbage_collect_function)(Xpost_Memory_File *mem, int dosweep, int markall))
{
    unsigned int newcid;
    Xpost_Context *newctx;
    int ret;

    (void)xpost_interpreter_alloc_global_memory;
    ret = xpost_interpreter_cid_init(&newcid);
    if (!ret) return 0;
    newctx = xpost_interpreter_cid_get_context(newcid);
    *newctx = *ctx; // struct copy for defaults
    newctx->id = newcid;
    newctx->state = C_IDLE;
    initlocal(ctx, xpost_interpreter_cid_get_context, 
            xpost_interpreter_get_initializing, xpost_interpreter_set_initializing, 
            xpost_interpreter_alloc_local_memory, garbage_collect_function);
    newctx->gl = ctx->gl;
    xpost_context_append_ctxlist(newctx->gl, newcid);
    xpost_stack_push(newctx->lo, newctx->ds,
            xpost_stack_bottomup_fetch(ctx->lo, ctx->ds, 0)); // systemdict
    return newcid;
}

/*
   fork new process with shared global and shared local vm
   (lightweight process)
   */
unsigned int xpost_context_fork3(Xpost_Context *ctx,
                                 int (*xpost_interpreter_cid_init)(unsigned int *cid),
                                 Xpost_Context *(*xpost_interpreter_cid_get_context)(unsigned int cid),
                                 Xpost_Memory_File *(*xpost_interpreter_alloc_local_memory)(void),
                                 Xpost_Memory_File *(*xpost_interpreter_alloc_global_memory)(void),
                                 int (*garbage_collect_function)(Xpost_Memory_File *mem, int dosweep, int markall))
{
    unsigned int newcid;
    Xpost_Context *newctx;
    int ret;

    (void)xpost_interpreter_alloc_global_memory;
    (void)xpost_interpreter_alloc_local_memory;
    (void)garbage_collect_function;
    ret = xpost_interpreter_cid_init(&newcid);
    if (!ret) return 0;
    newctx = xpost_interpreter_cid_get_context(newcid);
    *newctx = *ctx; // struct copy for defaults
    newctx->id = newcid;
    newctx->state = C_IDLE;
    newctx->lo = ctx->lo;
    xpost_context_append_ctxlist(newctx->lo, newcid);
    newctx->gl = ctx->gl;
    xpost_context_append_ctxlist(newctx->gl, newcid);

    newctx->os = makestack(newctx->lo);
    newctx->es = makestack(newctx->lo);
    newctx->ds = makestack(newctx->lo);
    newctx->hold = makestack(newctx->lo);
    newctx->lo->start = XPOST_MEMORY_TABLE_SPECIAL_BOGUS_NAME + 1;

    xpost_stack_push(newctx->lo, newctx->ds,
            xpost_stack_bottomup_fetch(ctx->lo, ctx->ds, 0)); // systemdict
    printf("fork cid %u, ctx->id %u\n", newcid, newctx->id);
    return newcid;
}


