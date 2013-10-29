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

#ifndef XPOST_STACK_H
#define XPOST_STACK_H

/* stacks
   Stacks consist of a chain of tables, much like mtab chains.
   */

/* must include ob.h */
/*typedef long long object;*/

#define STACKSEGSZ 20

typedef struct {
    unsigned nextseg;
    unsigned top;
    Xpost_Object data[STACKSEGSZ];
} stack;

/* create a stack data structure. returns vm address */
unsigned initstack(mfile *mem);

/* dump the contents of a stack to stdout using dumpobject */
void dumpstack(mfile *mem, unsigned stackadr);

/* free a stack, and all succeeding segments */
void sfree(mfile *mem, unsigned stackadr);

/* count elements on stack */
unsigned count(mfile *mem, unsigned stackadr);

/* push an object on top of the stack */
void push(mfile *mem, unsigned stackadr, Xpost_Object o);

/* index the stack from the top down, fetching object */
Xpost_Object top(mfile *mem, unsigned stackadr, integer i);
/* index the stack from the top down, replacing object */
void pot(mfile *mem, unsigned stackadr, integer i, Xpost_Object o);

/* index the stack from the bottom up, fetching object */
Xpost_Object bot(mfile *mem, unsigned stackadr, integer i);
/* index the stack from the bottom up, replacing object */
void tob(mfile *mem, unsigned stacadr, integer i, Xpost_Object o);

/* pop the stack. remove and return top object */
Xpost_Object pop(mfile *mem, unsigned stackadr);
/*int pop(mfile *mem, unsigned stackadr, object *po);*/

#endif
