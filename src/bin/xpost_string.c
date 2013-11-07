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

#include "xpost_memory.h"  // strings live in mfile, accessed via mtab
#include "xpost_object.h"  // strings are objects
#include "xpost_garbage.h"  // strings are allocated using gballoc
#include "xpost_context.h"
#include "xpost_interpreter.h"  // banked strings may live in local or global vm
#include "xpost_string.h"  // double-check prototypes

/* construct a stringtype object
   with optional string value
   */
Xpost_Object consstr(Xpost_Memory_File *mem,
               unsigned sz,
               /*@NULL@*/ char *ini)
{
    unsigned ent;
    Xpost_Object o;
    //xpost_memory_table_alloc(mem, (sz/sizeof(int) + 1)*sizeof(int), 0, &ent);
    ent = gballoc(mem, (sz/sizeof(int) + 1)*sizeof(int), stringtype);
    if (ini) xpost_memory_put(mem, ent, 0, sz, ini);
    o.tag = stringtype | (XPOST_OBJECT_TAG_ACCESS_UNLIMITED << XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_OFFSET);
    o.comp_.sz = sz;
    o.comp_.ent = ent;
    o.comp_.off = 0;
    return o;
}

/* construct a banked string object
   with optional string value
   */
Xpost_Object consbst(context *ctx,
               unsigned sz,
               /*@NULL@*/ char *ini)
{
    Xpost_Object s;
    s = consstr(ctx->vmmode==GLOBAL? ctx->gl: ctx->lo, sz, ini);
    if (ctx->vmmode==GLOBAL)
        s.tag |= XPOST_OBJECT_TAG_DATA_FLAG_BANK;
    return s;
}

/* adapter:
            char* <- string object
    yield a real, honest-to-goodness pointer to the
    string in a stringtype object
    */
/*@dependent@*/
char *charstr(context *ctx,
              Xpost_Object S)
{
    Xpost_Memory_File *f;
    Xpost_Memory_Table *tab;
    unsigned ent = S.comp_.ent;
    f = bank(ctx, S) /*S.tag&FBANK?ctx->gl:ctx->lo*/;
    xpost_memory_table_find_relative(f, &tab, &ent);
    return (void *)(f->base + tab->tab[ent].adr + S.comp_.off);
}


/* put a value at index into a string */
void strput(Xpost_Memory_File *mem,
            Xpost_Object s,
            integer i,
            integer c)
{
    byte b = c;
    xpost_memory_put(mem, s.comp_.ent, s.comp_.off + i, 1, &b);
}

/* put a value at index into a banked string */
void bstput(context *ctx,
            Xpost_Object s,
            integer i,
            integer c)
{
    strput(bank(ctx, s) /*s.tag&FBANK? ctx->gl: ctx->lo*/, s, i, c);
}

/* get a value from a string at index */
integer strget(Xpost_Memory_File *mem,
               Xpost_Object s,
               integer i)
{
    byte b;
    xpost_memory_get(mem, s.comp_.ent, s.comp_.off + i, 1, &b);
    return b;
}

/* get a value from a banked string at index */
integer bstget(context *ctx,
               Xpost_Object s,
               integer i)
{
    return strget(bank(ctx, s) /*s.tag&FBANK? ctx->gl: ctx->lo*/, s, i);
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

    s = consstr(&mem, CNT_STR("This is a string"));
    for (i=0; i < s.comp_.sz; i++) {
        putchar(strget(&mem, s, i));
    }
    putchar('\n');
    return 0;
}

#endif

