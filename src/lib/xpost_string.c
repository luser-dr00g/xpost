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

#include "xpost_log.h"
#include "xpost_memory.h"  // strings live in mfile, accessed via mtab
#include "xpost_object.h"  // strings are objects
#include "xpost_stack.h"
#include "xpost_context.h"
#include "xpost_error.h"

#include "xpost_string.h"  // double-check prototypes

/* construct a stringtype object
   with optional string value
   */
Xpost_Object xpost_cons_string_memory(Xpost_Memory_File *mem,
                     unsigned sz,
                     /*@NULL@*/ const char *ini)
{
    unsigned ent;
    Xpost_Object o;
    int ret;

    //xpost_memory_table_alloc(mem, (sz/sizeof(int) + 1)*sizeof(int), 0, &ent);
    if (!xpost_memory_table_alloc(mem, (sz/sizeof(int) + 1)*sizeof(int), stringtype, &ent))
    {
        XPOST_LOG_ERR("cannot allocate string");
        return null;
    }
    if (ini)
    {
        ret = xpost_memory_put(mem, ent, 0, sz, ini);
        if (!ret)
        {
            XPOST_LOG_ERR("cannot store initial value in string");
            return null;
        }
    }
    o.tag = stringtype | (XPOST_OBJECT_TAG_ACCESS_UNLIMITED << XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_OFFSET);
    o.comp_.sz = sz;
    //o.comp_.ent = ent;
    o = xpost_object_set_ent(o, ent);
    o.comp_.off = 0;

    return o;
}

/* construct a banked string object
   with optional string value
   */
Xpost_Object xpost_cons_string(Xpost_Context *ctx,
                     unsigned sz,
                     /*@NULL@*/ const char *ini)
{
    Xpost_Object s;
    s = xpost_cons_string_memory(ctx->vmmode==GLOBAL? ctx->gl: ctx->lo, sz, ini);
    if (xpost_object_get_type(s) != nulltype) {
        xpost_stack_push(ctx->lo, ctx->hold, s);
        if (ctx->vmmode==GLOBAL)
            s.tag |= XPOST_OBJECT_TAG_DATA_FLAG_BANK;
    }
    return s;
}

/* adapter:
            char* <- string object
    yield a real, honest-to-goodness pointer to the
    string in a stringtype object
    */
/*@dependent@*/
char *xpost_string_get_pointer(Xpost_Context *ctx,
              Xpost_Object S)
{
    Xpost_Memory_File *f;
    Xpost_Memory_Table *tab;
    unsigned ent = xpost_object_get_ent(S);
    f = xpost_context_select_memory(ctx, S) /*S.tag&FBANK?ctx->gl:ctx->lo*/;
    xpost_memory_table_find_relative(f, &tab, &ent);
    return (void *)(f->base + tab->tab[ent].adr + S.comp_.off);
}


/* put a value at index into a string */
int xpost_string_put_memory(Xpost_Memory_File *mem,
            Xpost_Object s,
            integer i,
            integer c)
{
    byte b = c;
    int ret;

    ret = xpost_memory_put(mem, xpost_object_get_ent(s), s.comp_.off + i, 1, &b);
    if (!ret)
    {
        return rangecheck;
    }
    return 0;
}

/* put a value at index into a banked string */
int xpost_string_put(Xpost_Context *ctx,
            Xpost_Object s,
            integer i,
            integer c)
{
    return xpost_string_put_memory(xpost_context_select_memory(ctx, s) /*s.tag&FBANK? ctx->gl: ctx->lo*/, s, i, c);
}

/* get a value from a string at index */
int xpost_string_get_memory(Xpost_Memory_File *mem,
               Xpost_Object s,
               integer i,
               integer *retval)
{
    byte b;
    int ret;

    ret = xpost_memory_get(mem, xpost_object_get_ent(s), s.comp_.off + i, 1, &b);
    if (!ret)
    {
        return rangecheck;
    }

    *retval = b;
    return 0;
}

/* get a value from a banked string at index */
int xpost_string_get(Xpost_Context *ctx,
               Xpost_Object s,
               integer i,
               integer *retval)
{
    return xpost_string_get_memory(xpost_context_select_memory(ctx, s) /*s.tag&FBANK? ctx->gl: ctx->lo*/, s, i, retval);
}

#ifdef TESTMODULE_ST
#include <stdio.h>

#define CNT_STR(s) sizeof(s), s

Xpost_Memory_File mem;

int main (void)
{
    Xpost_Object s;
    int i;
    printf("\n^ st.c\n");
    xpost_memory_file_init(&mem, "x.mem");
    (void)xpost_memory_table_init(&mem);

    s = xpost_cons_string_memory(&mem, CNT_STR("This is a string"));
    for (i=0; i < s.comp_.sz; i++) {
        putchar(xpost_string_get_memory(&mem, s, i));
    }
    putchar('\n');
    return 0;
}

#endif

