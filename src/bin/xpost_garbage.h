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

#ifndef XPOST_GC_H
#define XPOST_GC_H

/**
 * @file xpost_garbage.h
 * @brief The Garbage Collector
 */


/**
 * @enum  Xpost_Garbage_Params
 * @brief private constants
 *
 * FIXME: PLRM describes garbage collection control to be based on
 * number of bytes allocated, not the number of allocations.
 * Also this should be a variable accessible through `setsystemparams`
 * operator.
 */
typedef enum {
    PERIOD = 2000  /* number of times to grow before collecting */
} Xpost_Garbage_Params;

/**
 * @brief  initialize the FREE special entity which points
 *         to the head of the free list
 */
void initfree(Xpost_Memory_File *mem);

/**
 * @brief  print a dump of the free list
 */
void dumpfree(Xpost_Memory_File *mem);

/**
 * @brief  allocate data, re-using garbage if possible
 */
unsigned gballoc(Xpost_Memory_File *mem, unsigned sz, unsigned tag);

/**
 * @brief  explicitly add ent to free list
 */
unsigned mfree(Xpost_Memory_File *mem, unsigned ent);

/**
 * @brief  Perform a garbage collection on mfile.
 *
 * dosweep controls whether a sweep is performed; if not this
 * is just a marking operation. markall controls whether 
 * collect() should follow links across vm boundaries.
 *
 * For a local vm, dosweep should be 1 and markall should be 0.
 * For a global vm, dosweep should be 1 and markall should be 1.
 *
 * For a global vm, collect() calls itself recursively upon each
 * associated local vm, with dosweep = 0, markall = 1.
 */
unsigned collect(Xpost_Memory_File *mem, int dosweep, int markall);

/**
 * @brief reallocate data, preserving (the maximum of) original contents
 */
unsigned mfrealloc(Xpost_Memory_File *mem, unsigned oldadr, unsigned oldsize, unsigned newsize);

/**
 * @brief perform a short functionality test
 */
int test_garbage_collect(void);
#endif
