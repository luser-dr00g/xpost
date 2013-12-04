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

#ifndef XPOST_FREE_H
#define XPOST_FREE_H

/**
 *  @file xpost_free
 *  @brief adds de-allocation and re-allocation capabilities to xpost_memory
 *
 *  The free list is implemented to permanently occupy ent 0 of the memory table.
 *  xpost_free_init() should be the first function called after initializing
 *  the first memory table. xpost_free_init() asserts that this is so.
 *
 *  xpost_free_init() installs xpost_free_alloc as an alternate allocator
 *  for the memory file. After this function, calls to xpost_memory_table_alloc
 *  will first call xpost_free_alloc before falling back to increasing the size
 *  of the memory space.

 *  The free list is a chain of unused ents and their associated memory.
 *  The free list head is ent 0, which points (via the address field in
 *  the memory table entry for ent 0) to a 32bit int which is either 0
 *  (ie. a "NULL" "pointer", also a link back to the head (!)) or the ent
 *  number of the next free allocation. Any subsequent ents in the chain
 *  will have the next ent or 0 in the first 4 bytes of the allocation.
 *
 *  (All allocations are padded to at least an even word and zero-sized
 *  allocations are ignored, so any ent that can be put on the free list
 *  is guaranteed to have at least these 4 bytes allocated to it.)
 */

/**
 * @enum  Xpost_Garbage_Params
 * @brief private constants
 *
 * FIXME: PLRM describes garbage collection control to be based on
 * number of bytes allocated, not the number of allocations.
 * Also this should be a variable accessible through `setvmthreshold`
 * and `setsystemparams` operators.
 * PLRM, appendix C describes this variable, which is expected in the
 * dictionary argument of `setsystemparams`, and returned by
 * `currentsystemparams`:
 *    VMThreshold   integer   The frequency of automatic garbage collection,
 *                           which is triggered whenever this many bytes have
 *                           been allocated since the previous collection.
 */
typedef enum {
    XPOST_GARBAGE_COLLECTION_PERIOD = 5000,  /**< number of times to grow before collecting */
    XPOST_GARBAGE_COLLECTION_THRESHOLD = 10000  /**< number of bytes to allocate before collecting */
} Xpost_Garbage_Params;

/**
 * Maximum size to accept from an allocation relative to the size requested
 */
#define XPOST_FREE_ACCEPT_OVERSIZE 5
#define XPOST_FREE_ACCEPT_DENOM 1

/**
 * @brief  initialize the FREE special entity which points
 *         to the head of the free list
 */
int xpost_free_init(Xpost_Memory_File *mem);

/**
 * @brief  print a dump of the free list
 */
void xpost_free_dump(Xpost_Memory_File *mem);

/**
 * @brief  allocate data, re-using garbage if possible
 */
int xpost_free_alloc(Xpost_Memory_File *mem,
                     unsigned int sz,
                     unsigned int tag,
                     unsigned int *entity);

/**
 * @brief  explicitly add ent to free list
 */
int xpost_free_memory_ent(Xpost_Memory_File *mem,
                          unsigned int ent);

/**
 * @brief reallocate data, preserving original contents

 * Use the free-list and tables to now provide a realloc for
 * "raw" vm addresses (mem->base offsets rather than ents).
 * Assumes new size is larger than old size.

 * Allocate new entry, copy data, steal its adr, stash old adr, free it.
 */
unsigned int xpost_free_realloc(Xpost_Memory_File *mem,
                                unsigned int oldadr,
                                unsigned int oldsize,
                                unsigned int newsize);

#endif
