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

#ifndef XPOST_OBJECT_H
# error MUST #include "xpost_object.h" before this file
#endif

#ifndef XPOST_MEMORY_H
# error MUST #include "xpost_memory.h" before this file
#endif

/**
 * @brief Number of object in one segment of the stack.
 */
#define XPOST_STACK_SEGMENT_SIZE 20

typedef struct
{
    unsigned int nextseg;
    unsigned int top;
    Xpost_Object data[XPOST_STACK_SEGMENT_SIZE];
} Xpost_Stack;

/**
 * @brief Create a stack data structure, returns vm address in addr.
 */
int xpost_stack_init (Xpost_Memory_File *mem, unsigned int *addr);

/**
 * @brief Dump the contents of a stack to stdout using xpost_object_dump.
 */
void xpost_stack_dump (Xpost_Memory_File *mem, unsigned int stackadr);

/**
 * @brief Free a stack, and all succeeding segments.
 */
void xpost_stack_free (Xpost_Memory_File *mem,
        unsigned int stackadr);

/**
 * @brief Count elements in stack.
 */
int xpost_stack_count (Xpost_Memory_File *mem,
        unsigned int stackadr);

/**
 * @brief Put an object on top of the stack.
 */
int xpost_stack_push (Xpost_Memory_File *mem,
        unsigned int stackadr,
        Xpost_Object obj);

/**
 * @brief Index the stack from the top down, fetching object.
 */
Xpost_Object xpost_stack_topdown_fetch (Xpost_Memory_File *mem,
        unsigned stackadr,
        int i);

/**
 * @brief Index the stack from the top down, replacing object.
 */
int xpost_stack_topdown_replace (Xpost_Memory_File *mem,
        unsigned stackadr,
        int i,
        Xpost_Object obj);

/**
 * @brief Index the stack from the bottom up, fetching object.
 */
Xpost_Object xpost_stack_bottomup_fetch (Xpost_Memory_File *mem,
        unsigned stackadr,
        int i);

/**
 * @brief Index the stack from the bottom up, replacing object.
 */
int xpost_stack_bottomup_replace (Xpost_Memory_File *mem,
        unsigned stackadr,
        int i,
        Xpost_Object obj);

/**
 * @brief Pop the stack, remove and return top object.
 */
Xpost_Object xpost_stack_pop (Xpost_Memory_File *mem,
        unsigned stackadr);

#endif
