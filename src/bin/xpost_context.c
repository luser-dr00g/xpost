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

#include <assert.h>
#include <stdio.h> /* FILE* */
#include <stdlib.h> /* mkstemp */
#include <string.h> /* memset */

#ifdef HAVE_UNISTD_H
# include <unistd.h> /* isattty */
#endif

#ifdef __MINGW32__
# include "osmswin.h" /* mkstemp xpost_getpagesize */
#else
# include "osunix.h" /* xpost_getpagesize */
#endif

#include "xpost_object.h"
#include "xpost_memory.h"
#include "xpost_stack.h"
#include "xpost_context.h"
#include "xpost_interpreter.h"
#include "xpost_error.h"
#include "xpost_free.h"  //  initializes free list
#include "xpost_save.h"  // initializes save/restore stacks
#include "xpost_name.h"  // create names
#include "xpost_dict.h"  // create dicts in context
#include "xpost_operator.h"  // eval functions call operators

/* initialize the context list
   special entity in the mfile */
void xpost_context_init_ctxlist(Xpost_Memory_File *mem)
{
    unsigned ent;
    Xpost_Memory_Table *tab;
    xpost_memory_table_alloc(mem, MAXCONTEXT * sizeof(unsigned), 0, &ent);
    assert(ent == XPOST_MEMORY_TABLE_SPECIAL_CONTEXT_LIST);
    tab = (void *)mem->base;
    memset(mem->base + tab->tab[XPOST_MEMORY_TABLE_SPECIAL_CONTEXT_LIST].adr, 0,
            MAXCONTEXT * sizeof(unsigned));
}

/* add a context ID to the context list in mfile */
void xpost_context_append_ctxlist(Xpost_Memory_File *mem,
                  unsigned cid)
{
    int i;
    Xpost_Memory_Table *tab;
    unsigned *ctxlist;

    tab = (void *)mem->base;
    ctxlist = (void *)(mem->base + tab->tab[XPOST_MEMORY_TABLE_SPECIAL_CONTEXT_LIST].adr);
    // find first empty
    for (i=0; i < MAXCONTEXT; i++) {
        if (ctxlist[i] == 0) {
            ctxlist[i] = cid;
            return;
        }
    }
    error(unregistered, "ctxlist full");
}


/* build a stack, return address */
static
unsigned makestack(Xpost_Memory_File *mem)
{
    unsigned int ret;
    xpost_stack_init(mem, &ret);
    return ret;
}

/* set up global vm in the context
 */
static
void initglobal(Xpost_Context *ctx)
{
    char g_filenam[] = "gmemXXXXXX";
    int fd;
    unsigned int tadr;

    ctx->vmmode = GLOBAL;

    /* allocate and initialize global vm */
    //ctx->gl = malloc(sizeof(Xpost_Memory_File));
    //ctx->gl = &itpdata->gtab[0];
    ctx->gl = nextgtab();

    fd = mkstemp(g_filenam);

    xpost_memory_file_init(ctx->gl, g_filenam, fd);
    xpost_memory_table_init(ctx->gl, &tadr);
    xpost_free_init(ctx->gl);
    initsave(ctx->gl);
    xpost_context_init_ctxlist(ctx->gl);
    xpost_context_append_ctxlist(ctx->gl, ctx->id);

    ctx->gl->start = XPOST_MEMORY_TABLE_SPECIAL_OPERATOR_TABLE + 1; /* so OPTAB is not collected and not scanned. */
}


/* set up local vm in the context
   allocates all stacks
 */
static
void initlocal(Xpost_Context *ctx)
{
    char l_filenam[] = "lmemXXXXXX";
    int fd;
    unsigned int tadr;

    ctx->vmmode = LOCAL;

    /* allocate and initialize local vm */
    //ctx->lo = malloc(sizeof(Xpost_Memory_File));
    //ctx->lo = &itpdata->ltab[0];
    ctx->lo = nextltab();

    fd = mkstemp(l_filenam);

    xpost_memory_file_init(ctx->lo, l_filenam, fd);
    xpost_memory_table_init(ctx->lo, &tadr);
    xpost_free_init(ctx->lo);
    initsave(ctx->lo);
    xpost_context_init_ctxlist(ctx->lo);
    xpost_context_append_ctxlist(ctx->lo, ctx->id);
    //ctx->lo->roots[0] = XPOST_MEMORY_TABLE_SPECIAL_SAVE_STACK;

    ctx->os = makestack(ctx->lo);
    ctx->es = makestack(ctx->lo);
    ctx->ds = makestack(ctx->lo);
    ctx->hold = makestack(ctx->lo);
    //ctx->lo->roots[1] = DS;
    //ctx->lo->start = HOLD + 1; /* so HOLD is not collected and not scanned. */
    //ctx->lo->start = XPOST_MEMORY_TABLE_SPECIAL_CONTEXT_LIST + 1;
    ctx->lo->start = XPOST_MEMORY_TABLE_SPECIAL_BOGUS_NAME + 1;
}


/* initialize context
   allocates operator table
   allocates systemdict
   populates systemdict and optab with operators
 */
void xpost_context_init(Xpost_Context *ctx)
{
    ctx->id = initctxid();
    initlocal(ctx);
    initglobal(ctx);

    initnames(ctx); /* NAMES NAMET */
    ctx->vmmode = GLOBAL;

    initoptab(ctx); /* allocate and zero the optab structure */

    (void)consname(ctx, "maxlength"); /* seed the tree with a word from the middle of the alphabet */
    (void)consname(ctx, "getinterval"); /* middle of the start */
    (void)consname(ctx, "setmiterlimit"); /* middle of the end */

    initop(ctx); /* populate the optab (and systemdict) with operators */

    {
        Xpost_Object gd; //globaldict
        gd = consbdc(ctx, 100);
        bdcput(ctx, xpost_stack_bottomup_fetch(ctx->lo, ctx->ds, 0), consname(ctx, "globaldict"), gd);
        xpost_stack_push(ctx->lo, ctx->ds, gd);
    }

    ctx->vmmode = LOCAL;
    (void)consname(ctx, "minimal"); /* seed the tree with a word from the middle of the alphabet */
    (void)consname(ctx, "interest"); /* middle of the start */
    (void)consname(ctx, "solitaire"); /* middle of the end */
    {
        Xpost_Object ud; //userdict
        ud = consbdc(ctx, 100);
        bdcput(ctx, ud, consname(ctx, "userdict"), ud);
        xpost_stack_push(ctx->lo, ctx->ds, ud);
    }
}

/* destroy context */
void xpost_context_exit(Xpost_Context *ctx)
{
    xpost_memory_file_exit(ctx->gl);
    xpost_memory_file_exit(ctx->lo);
}

/* return the global or local memory file for the composite object */
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
    dumpnames(ctx);
}

/*
   fork new process with private global and private local vm
   (spawn jobserver)
   */
static
unsigned fork1(Xpost_Context *ctx)
{
    unsigned newcid;
    Xpost_Context *newctx;

    newcid = initctxid();
    newctx = ctxcid(newcid);
    initlocal(ctx);
    initglobal(ctx);
    ctx->vmmode = LOCAL;
    return newcid;
}

/*
   fork new process with shared global vm and private local vm
   (new "application"?)
   */
static
unsigned fork2(Xpost_Context *ctx)
{
    unsigned newcid;
    Xpost_Context *newctx;

    newcid = initctxid();
    newctx = ctxcid(newcid);
    initlocal(ctx);
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
static
unsigned fork3(Xpost_Context *ctx)
{
    unsigned newcid;
    Xpost_Context *newctx;

    newcid = initctxid();
    newctx = ctxcid(newcid);
    newctx->lo = ctx->lo;
    xpost_context_append_ctxlist(newctx->lo, newcid);
    newctx->gl = ctx->gl;
    xpost_context_append_ctxlist(newctx->gl, newcid);
    xpost_stack_push(newctx->lo, newctx->ds, 
            xpost_stack_bottomup_fetch(ctx->lo, ctx->ds, 0)); // systemdict
    return newcid;
}

