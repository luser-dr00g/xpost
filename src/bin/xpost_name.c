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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xpost_log.h"
#include "xpost_memory.h"  // name structures live in mfiles
#include "xpost_object.h"  // names are objects, with associated hidden string objects
#include "xpost_stack.h"  // name strings live on a stack

#include "xpost_context.h"
#include "xpost_interpreter.h"  // initialize interpreter to test
#include "xpost_error.h"
#include "xpost_string.h"  // access string objects
#include "xpost_name.h"  // double-check prototypes

#define CNT_STR(s) sizeof(s)-1, s

/* print a dump of the name string stacks, global and local */
void dumpnames(Xpost_Context *ctx)
{
    unsigned int stk;
    unsigned int cnt, i;
    Xpost_Object str;
    char *s;

    xpost_memory_table_get_addr(ctx->gl,
            XPOST_MEMORY_TABLE_SPECIAL_NAME_STACK, &stk);
    cnt = xpost_stack_count(ctx->gl, stk);
    printf("global names:\n");
    for (i=0; i < cnt; i++){
        str = xpost_stack_bottomup_fetch(ctx->gl, stk, i);
        s = charstr(ctx, str);
        printf("%u: %*s\n", i, str.comp_.sz, s);
    }
    xpost_memory_table_get_addr(ctx->lo,
            XPOST_MEMORY_TABLE_SPECIAL_NAME_STACK, &stk);
    cnt = xpost_stack_count(ctx->lo, stk);
    printf("local names:\n");
    for (i=0; i < cnt; i++) {
        str = xpost_stack_bottomup_fetch(ctx->lo, stk, i);
        s = charstr(ctx, str);
        printf("%u: %*s\n", i, str.comp_.sz, s);
    }
}

/* initialize the name special entities XPOST_MEMORY_TABLE_SPECIAL_NAME_STACK, NAMET */
int initnames(Xpost_Context *ctx)
{
    Xpost_Memory_Table *tab;
    unsigned int ent;
    unsigned int t;
    unsigned int mode;
    unsigned int nstk;
    int ret;

    mode = ctx->vmmode;
    ctx->vmmode = GLOBAL;
    ret = xpost_memory_table_alloc(ctx->gl, 0, 0, &ent); //gl:NAMES
    if (!ret)
    {
        return 0;
    }
    //assert(ent == XPOST_MEMORY_TABLE_SPECIAL_NAME_STACK);
    if (ent != XPOST_MEMORY_TABLE_SPECIAL_NAME_STACK)
        XPOST_LOG_ERR("Warning: name stack is not in special position");
    ret = xpost_memory_table_alloc(ctx->gl, 0, 0, &ent); //gl:NAMET
    if (!ret)
    {
        return 0;
    }
    //assert(ent == XPOST_MEMORY_TABLE_SPECIAL_NAME_TREE);
    if (ent != XPOST_MEMORY_TABLE_SPECIAL_NAME_TREE)
        XPOST_LOG_ERR("Warning: name tree is not in special position");

    xpost_stack_init(ctx->gl, &t);
    tab = (void *)ctx->gl->base; //recalc pointer
    tab->tab[XPOST_MEMORY_TABLE_SPECIAL_NAME_STACK].adr = t;
    tab->tab[XPOST_MEMORY_TABLE_SPECIAL_NAME_TREE].adr = 0;
    xpost_memory_table_get_addr(ctx->gl,
            XPOST_MEMORY_TABLE_SPECIAL_NAME_STACK, &nstk);
    xpost_stack_push(ctx->gl, nstk, consbst(ctx, CNT_STR("_not_a_name_")));
    assert (xpost_object_get_ent(xpost_stack_topdown_fetch(ctx->gl, nstk, 0)) == XPOST_MEMORY_TABLE_SPECIAL_BOGUS_NAME);

    ctx->vmmode = LOCAL;
    ret = xpost_memory_table_alloc(ctx->lo, 0, 0, &ent); //lo:NAMES
    if (!ret)
    {
        return 0;
    }
    //assert(ent == XPOST_MEMORY_TABLE_SPECIAL_NAME_STACK);
    if (ent != XPOST_MEMORY_TABLE_SPECIAL_NAME_STACK)
        XPOST_LOG_ERR("Warning: name stack is not in special position");
    ret = xpost_memory_table_alloc(ctx->lo, 0, 0, &ent); //lo:NAMET
    if (!ret)
    {
        return 0;
    }
    //assert(ent == XPOST_MEMORY_TABLE_SPECIAL_NAME_TREE);
    if (ent != XPOST_MEMORY_TABLE_SPECIAL_NAME_TREE)
        XPOST_LOG_ERR("Warning: name tree is not in special position");

    xpost_stack_init(ctx->lo, &t);
    tab = (void *)ctx->lo->base; //recalc pointer
    tab->tab[XPOST_MEMORY_TABLE_SPECIAL_NAME_STACK].adr = t;
    tab->tab[XPOST_MEMORY_TABLE_SPECIAL_NAME_TREE].adr = 0;
    xpost_memory_table_get_addr(ctx->lo,
            XPOST_MEMORY_TABLE_SPECIAL_NAME_STACK, &nstk);
    xpost_stack_push(ctx->lo, nstk, consbst(ctx, CNT_STR("_not_a_name_")));
    //assert (xpost_object_get_ent(xpost_stack_topdown_fetch(ctx->lo, nstk, 0)) == XPOST_MEMORY_TABLE_SPECIAL_BOGUS_NAME);
    if (xpost_object_get_ent(xpost_stack_topdown_fetch(ctx->lo, nstk, 0)) != XPOST_MEMORY_TABLE_SPECIAL_BOGUS_NAME)
        XPOST_LOG_ERR("Warning: bogus name not in special position");

    ctx->vmmode = mode;

    return 1;
}

/* perform a search using the ternary search tree */
static
unsigned int tstsearch(Xpost_Memory_File *mem,
                   unsigned int tadr,
                   char *s)
{
    while (tadr) {
        tst *p = (void *)(mem->base + tadr);
        if ((unsigned int)*s < p->val) {
            tadr = p->lo;
        } else if ((unsigned int)*s == p->val) {
            if (*s++ == 0) return p->eq; /* payload when val == '\0' */
            tadr = p->eq;
        } else {
            tadr = p->hi;
        }
    }
    return 0;
}

/* add a string to the ternary search tree */
static
int tstinsert(Xpost_Memory_File *mem,
                   unsigned int tadr,
                   char *s,
                   unsigned int *retval)
{
    tst *p;
    unsigned int t; //temporary
    unsigned int nstk;
    int ret;

    if (!tadr) {
        if (!xpost_memory_file_alloc(mem, sizeof(tst), &tadr))
        {
            XPOST_LOG_ERR("cannot allocate tree node");
            return VMerror;
        }
        p = (void *)(mem->base + tadr);
        p->val = *s;
        p->lo = p->eq = p->hi = 0;
    }
    p = (void *)(mem->base + tadr);
    if ((unsigned int)*s < p->val) {
        ret = tstinsert(mem, p->lo, s, &t);
        if (ret)
            return ret;
        p = (void *)(mem->base + tadr); //recalc pointer
        p->lo = t;
    } else if ((unsigned int)*s == p->val) {
        if (*s) {
            ret = tstinsert(mem, p->eq, ++s, &t);
            if (ret)
                return ret;
            p = (void *)(mem->base + tadr); //recalc pointer
            p->eq = t;
        }else {
            xpost_memory_table_get_addr(mem,
                    XPOST_MEMORY_TABLE_SPECIAL_NAME_STACK, &nstk);
            p->eq = xpost_stack_count(mem, nstk); /* payload when val == '\0' */
        }
    } else {
        ret = tstinsert(mem, p->hi, s, &t);
        if (ret)
            return ret;
        p = (void *)(mem->base + tadr); //recalc pointer
        p->hi = t;
    }
    //return tadr;
    *retval = tadr;
    return 0;
}

/* add the name to the name stack, return index */
static
unsigned int addname(Xpost_Context *ctx,
                 char *s)
{
    Xpost_Memory_File *mem = ctx->vmmode==GLOBAL?ctx->gl:ctx->lo;
    unsigned int names;
    unsigned int u;
    Xpost_Object str;

    xpost_memory_table_get_addr(mem,
            XPOST_MEMORY_TABLE_SPECIAL_NAME_STACK, &names);
    u = xpost_stack_count(mem, names);

    //xpost_memory_file_dump(ctx->gl);
    //dumpmtab(ctx->gl, 0);
    //unsigned int vmmode = ctx->vmmode;
    //ctx->vmmode = GLOBAL;
    str = consbst(ctx, strlen(s), s);
    if (xpost_object_get_type(str) == nulltype)
    {
        XPOST_LOG_ERR("cannot allocate name string");
        return 0;
    }
    xpost_stack_push(mem, names, str);
    //ctx->vmmode = vmmode;
    return u;
}

/* construct a name object from a string
   searches and if necessary installs string
   in ternary search tree,
   adding string to stack if so.
   returns a generic object with
       nametype tag with BANK field, 
       mark_.pad0 set to zero
       mark_.padw contains XPOST_MEMORY_TABLE_SPECIAL_NAME_STACK stack index
 */
Xpost_Object consname(Xpost_Context *ctx,
                char *s)
{
    unsigned int u;
    unsigned int t;
    Xpost_Object o;
    unsigned int tstk;
    int ret;

    xpost_memory_table_get_addr(ctx->lo,
            XPOST_MEMORY_TABLE_SPECIAL_NAME_TREE, &tstk);
    u = tstsearch(ctx->lo, tstk, s);
    if (!u) {
        xpost_memory_table_get_addr(ctx->gl,
                XPOST_MEMORY_TABLE_SPECIAL_NAME_TREE, &tstk);
        u = tstsearch(ctx->gl, tstk, s);
        if (!u) {
            Xpost_Memory_File *mem = ctx->vmmode==GLOBAL?ctx->gl:ctx->lo;
            Xpost_Memory_Table *tab = (void *)mem->base;
            ret = tstinsert(mem, tab->tab[XPOST_MEMORY_TABLE_SPECIAL_NAME_TREE].adr, s, &t);
            if (ret)
            {
                //error(ret, ""); //this can only be a VMerror
                return invalid;
            }
            tab = (void *)mem->base; //recalc pointer
            tab->tab[XPOST_MEMORY_TABLE_SPECIAL_NAME_TREE].adr = t;
            u = addname(ctx, s); // obeys vmmode
            o.mark_.tag = nametype | (ctx->vmmode==GLOBAL?XPOST_OBJECT_TAG_DATA_FLAG_BANK:0);
            o.mark_.pad0 = 0;
            o.mark_.padw = u;
        } else {
            o.mark_.tag = nametype | XPOST_OBJECT_TAG_DATA_FLAG_BANK; // global
            o.mark_.pad0 = 0;
            o.mark_.padw = u;
        }
    } else {
        o.mark_.tag = nametype; // local
        o.mark_.pad0 = 0;
        o.mark_.padw = u;
        }
    return o;
}

/* adapter:
            string <- name
    yield the string object from the name string stack
    */
Xpost_Object strname(Xpost_Context *ctx,
               Xpost_Object n)
{
    Xpost_Memory_File *mem = xpost_context_select_memory(ctx, n);
    unsigned int names;
    Xpost_Object str;
    xpost_memory_table_get_addr(mem,
            XPOST_MEMORY_TABLE_SPECIAL_NAME_STACK, &names);
    str = xpost_stack_bottomup_fetch(mem, names, n.mark_.padw);
    //str.tag |= XPOST_OBJECT_TAG_DATA_FLAG_BANK;
    return str;
}

#ifdef TESTMODULE_NM
#include <stdio.h>
#include <unistd.h>

/*
void init(Xpost_Context *ctx) {
    ctx->gl = malloc(sizeof(Xpost_Memory_File));
    xpost_memory_file_init(ctx->gl, "x.mem");
    (void)xpost_memory_table_init(ctx->gl); // create mtab at address zero
    //(void)xpost_memory_table_alloc(ctx->gl, 0, 0, 0); //FREE
    xpost_free_init(ctx->gl);
    (void)xpost_memory_table_alloc(ctx->gl, 0, 0, 0); //VS
    xpost_context_init_ctxlist(ctx->gl);

    initnames(ctx);
}
Xpost_Context ctx;
*/

Xpost_Context *ctx;

void init(void) {
    itpdata = malloc(sizeof*itpdata);
    memset(itpdata, 0, sizeof*itpdata);
    xpost_interpreter_init(itpdata);
    ctx = &itpdata->ctab[0];
    ctx->vmmode = GLOBAL;
}

int main(void)
{
    if (!xpost_init())
    {
        fprintf(stderr, "Fail to initialize xpost name test\n");
        return -1;
    }

    printf("\n^test nm\n");
    //init(&ctx);
    init();
    ctx->vmmode = LOCAL;

    printf("pop ");
    xpost_object_dump(consname(ctx, "pop"));
    printf("NAMES at %u\n", xpost_memory_table_get_addr(ctx->gl, XPOST_MEMORY_TABLE_SPECIAL_NAME_STACK));
    //xpost_stack_dump(ctx->gl, xpost_memory_table_get_addr(ctx->gl, XPOST_MEMORY_TABLE_SPECIAL_NAME_STACK)); puts("");

    printf("apple ");
    xpost_object_dump(consname(ctx, "apple"));
    xpost_object_dump(consname(ctx, "apple"));
    //printf("NAMES at %u\n", xpost_memory_table_get_addr(ctx->gl, XPOST_MEMORY_TABLE_SPECIAL_NAME_STACK));
    //xpost_stack_dump(ctx->gl, xpost_memory_table_get_addr(ctx->gl, XPOST_MEMORY_TABLE_SPECIAL_NAME_STACK)); puts("");

    printf("banana ");
    xpost_object_dump(consname(ctx, "banana"));
    xpost_object_dump(consname(ctx, "banana"));
    //printf("NAMES at %u\n", xpost_memory_table_get_addr(ctx->gl, XPOST_MEMORY_TABLE_SPECIAL_NAME_STACK));
    //xpost_stack_dump(ctx->gl, xpost_memory_table_get_addr(ctx->gl, XPOST_MEMORY_TABLE_SPECIAL_NAME_STACK)); puts("");

    printf("currant ");
    xpost_object_dump(consname(ctx, "currant"));
    xpost_object_dump(consname(ctx, "currant"));
    //printf("NAMES at %u\n", xpost_memory_table_get_addr(ctx->gl, XPOST_MEMORY_TABLE_SPECIAL_NAME_STACK));
    //xpost_stack_dump(ctx->gl, xpost_memory_table_get_addr(ctx->gl, XPOST_MEMORY_TABLE_SPECIAL_NAME_STACK)); puts("");

    printf("apple ");
    xpost_object_dump(consname(ctx, "apple"));
    printf("banana ");
    xpost_object_dump(consname(ctx, "banana"));
    printf("currant ");
    xpost_object_dump(consname(ctx, "currant"));
    printf("date ");
    //printf("NAMES at %u\n", xpost_memory_table_get_addr(ctx->gl, XPOST_MEMORY_TABLE_SPECIAL_NAME_STACK));
    xpost_object_dump(consname(ctx, "date"));
    //printf("NAMES at %u\n", xpost_memory_table_get_addr(ctx->gl, XPOST_MEMORY_TABLE_SPECIAL_NAME_STACK));
    xpost_stack_dump(ctx->gl, xpost_memory_table_get_addr(ctx->gl, XPOST_MEMORY_TABLE_SPECIAL_NAME_STACK)); puts("");
    //printf("NAMES at %u\n", xpost_memory_table_get_addr(ctx->gl, XPOST_MEMORY_TABLE_SPECIAL_NAME_STACK));
    printf("elderberry ");
    xpost_object_dump(consname(ctx, "elderberry"));

    printf("pop ");
    xpost_object_dump(consname(ctx, "pop"));

    //xpost_memory_file_dump(ctx->gl);
    //dumpmtab(ctx->gl, 0);
    puts("");

    xpost_quit();

    return 0;
}

#endif
