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

#ifdef HAVE_STDBOOL_H
# include <stdbool.h>
#else
# ifndef HAVE__BOOL
#  ifdef __cplusplus
typedef bool _Bool;
#  else
#   define _Bool signed char
#  endif
# endif
# define bool _Bool
# define false 0
# define true 1
# define __bool_true_false_are_defined 1
#endif

#include <stdio.h> /* printf */
#include <stdlib.h> /* NULL */

#ifdef __MINGW32__
# include "osmswin.h" /* xpost_getpagesize */
#else
# include "osunix.h" /* xpost_getpagesize */
#endif

#include "xpost_memory.h" /* mfile xpost_memory_file_alloc findtabent */
#include "xpost_object.h" /* object size */
/* typedef long long object; */
#include "xpost_interpreter.h"
#include "xpost_error.h" // stack functions may throw errors
#include "xpost_stack.h"  // double-check prototypes
/*#define STACKSEGSZ 10 */

/*
typedef struct {
    unsigned nextseg;
    unsigned top;
    Xpost_Object data[STACKSEGSZ];
} stack;
*/

/* allocate a stack segment,
   return address */
unsigned initstack(Xpost_Memory_File *mem)
{
    unsigned adr;
    stack *s;

    xpost_memory_file_alloc(mem, sizeof(stack), &adr);
    s = (void *)(mem->base + adr);
    s->nextseg = 0;
    s->top = 0;
    return adr;
}

/* print a dump of the stack */
void dumpstack(Xpost_Memory_File *mem,
               unsigned stackadr)
{
    stack *s = (void *)(mem->base + stackadr);
    unsigned i;
    unsigned a;
    a = 0;
    while (1) {
        for (i=0; i < s->top; i++) {
            printf("%d:", a++);
            xpost_object_dump(s->data[i]);
        }
        if (i != STACKSEGSZ) break;
        s = (void *)(mem->base + s->nextseg);
    }
}

/* free a stack segment */
void sfree(Xpost_Memory_File *mem,
           unsigned stackadr)
{
    stack *s = (void *)(mem->base + stackadr);
    Xpost_Memory_Table *tab;
    unsigned e;
    if (s->nextseg) sfree(mem, s->nextseg);
    xpost_memory_table_alloc(mem, 0, 0, &e); /* allocate entry with 0 size */
    xpost_memory_table_find_relative(mem, &tab, &e);
    tab->tab[e].adr = stackadr; /* insert address */
    tab->tab[e].sz = sizeof(stack); /* insert size */
    /* discard */
}

/* count the stack */
unsigned count(Xpost_Memory_File *mem,
               unsigned stackadr)
{
    stack *s = (void *)(mem->base + stackadr);
    unsigned ct = 0;
    while (s->top == STACKSEGSZ) {
        ct += STACKSEGSZ;
        s = (void *)(mem->base + s->nextseg);
    }
    return ct + s->top;
}

/* push an object on the stack */
void push(Xpost_Memory_File *mem,
          unsigned stackadr,
          Xpost_Object o)
{
    unsigned newst;
    unsigned stadr;
    stack *s = (void *)(mem->base + stackadr); /* load the stack */

    while (s->top == STACKSEGSZ) { /* find top segment */
        s = (void *)(mem->base + s->nextseg);
    }

    s->data[s->top++] = o; /* push value */

    /* if push maxxed the topmost segment, link a new one */
    if (s->top == STACKSEGSZ) {
        if (s->nextseg == 0) {
            stadr = (unsigned char *)s - mem->base;
            newst = initstack(mem);
            s = (void *)(mem->base + stadr);
            s->nextseg = newst;
        } else {
            s = (void *)(mem->base + s->nextseg);
            s->top = 0;
        }
    }
}


/* index the stack from top-down */
Xpost_Object top(Xpost_Memory_File *mem,
           unsigned stacadr,
           integer i)
{
    int cnt = count(mem, stacadr);
    return bot(mem, stacadr, cnt - 1 - i);
}


/* index from top-down and put item there.
   inverse of top. */
void pot (Xpost_Memory_File *mem,
          unsigned stacadr,
          integer i,
          Xpost_Object o)
{
    int cnt = count(mem, stacadr);
    tob(mem, stacadr, cnt - 1 - i, o);
}

/* index from bottom up */
Xpost_Object bot (Xpost_Memory_File *mem,
            unsigned stacadr,
            integer i)
{
    stack *s = (void *)(mem->base + stacadr);

    /* find desired segment */
    while (i >= STACKSEGSZ) {
        i -= STACKSEGSZ;
        s = (void *)(mem->base + s->nextseg);
    }
    return s->data[i];
}

/* index from bottom-up and put item there.
   inverse of bot. */
void tob (Xpost_Memory_File *mem,
          unsigned stacadr,
          integer i,
          Xpost_Object o)
{
    stack *s = (void *)(mem->base + stacadr);

    /* find desired segment */
    while (i >= STACKSEGSZ) {
        i -= STACKSEGSZ;
        if (s->nextseg == 0) error(stackunderflow, "tob");
        s = (void *)(mem->base + s->nextseg);
    }
    s->data[i] = o;
}

/* pop an object off the stack, return object */
Xpost_Object pop (Xpost_Memory_File *mem,
            unsigned stackadr)
{
    stack *s = (void *)(mem->base + stackadr);
    stack *p = NULL;

    /* find top segment */
    while (s->top == STACKSEGSZ) {
        p = s;
        s = (void *)(mem->base + s->nextseg);
    }
    if (s->top == 0) {
        if (p != NULL) s = p; /* back up if top is empty */
        else /* error("stack underflow"); */
            return invalid;
    }

    return s->data[--s->top]; /* pop value */
}

#ifdef TESTMODULE_S

#include <stdio.h>

Xpost_Memory_File mem;
unsigned s, t;

/* initialize everything */
void init (void)
{
    xpost_memory_pagesize = xpost_getpagesize();
    xpost_memory_file_init(&mem, "x.mem");
    s = initstack(&mem);
    t = initstack(&mem);
}

void xit (void)
{
    xpost_memory_file_exit(&mem);
}

int main()
{
    init();

    printf("\n^test s.c\n");
    printf("test stack by reversing a sequence\n");
    //object a = { .int_.val = 2 }, b = ( .int_.val = 12 }, c = { .int_.val = 0xF00 };
    Xpost_Object a = xpost_cons_int(2);
    Xpost_Object b = xpost_cons_int(12);
    Xpost_Object c = xpost_cons_int(0xF00);
    Xpost_Object x = a, y = b, z = c;
    printf("x = %d, y = %d, z = %d\n", x.int_.val, y.int_.val, z.int_.val);

    push(&mem, s, a);
    push(&mem, s, b);
    push(&mem, s, c);

    x = pop(&mem, s); /* x = c */
    push(&mem, t, x);
    y = pop(&mem, s); /* y = b */
    push(&mem, t, y);
    z = pop(&mem, s); /* z = a */
    push(&mem, t, z);
    printf("x = %d, y = %d, z = %d\n", x.int_.val, y.int_.val, z.int_.val);
    printf("top(0): %d\n", top(&mem, t, 0).int_.val);
    printf("top(1): %d\n", top(&mem, t, 1).int_.val);
    printf("top(2): %d\n", top(&mem, t, 2).int_.val);
    printf("bot(0): %d\n", bot(&mem, t, 0).int_.val);
    printf("bot(1): %d\n", bot(&mem, t, 1).int_.val);
    printf("bot(2): %d\n", bot(&mem, t, 2).int_.val);
    printf("tob(2, 55)\n");
    tob(&mem, t, 2, xpost_cons_int(55));
    printf("pot(1, 37)\n");
    pot(&mem, t, 1, xpost_cons_int(37));

    x = pop(&mem, t); /* x = a */
    y = pop(&mem, t); /* y = b */
    z = pop(&mem, t); /* z = c */
    printf("x = %d, y = %d, z = %d\n", x.int_.val, y.int_.val, z.int_.val);
    //z = pop(&mem, t);

    xit();
    return 0;
}

#endif
