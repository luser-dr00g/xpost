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
 * @brief  Perform a garbage collection on mfile.
 *
 * dosweep controls whether a sweep is performed; if not, this
 * is just a marking operation. markall controls whether 
 * collect() should follow links across vm boundaries.
 *
 * For a local vm, dosweep should be 1 and markall should be 0.
 * For a global vm, dosweep should be 1 and markall should be 1.
 *
 * For a global vm, collect() calls itself recursively upon each
 * associated local vm, with dosweep = 0, markall = 1.
 *
 * returns size collected or -1 if error occured.
 */
int xpost_garbage_collect(Xpost_Memory_File *mem, int dosweep, int markall);

#if 0
/**
 * @brief perform a short functionality test
 */
int test_garbage_collect(int (*xpost_interpreter_cid_init)(unsigned int *cid),
                         Xpost_Context *(*xpost_interpreter_cid_get_context)(unsigned int cid),
                         int (*xpost_interpreter_get_initializing)(void),
                         void (*xpost_interpreter_set_initializing)(int),
                         Xpost_Memory_File *(*xpost_interpreter_alloc_local_memory)(void),
                         Xpost_Memory_File *(*xpost_interpreter_alloc_global_memory)(void));
#endif

#endif
