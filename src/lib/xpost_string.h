/*
 * Xpost - a Level-2 Postscript interpreter
 * Copyright (C) 2013-2016, Michael Joshua Ryan
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

#ifndef XPOST_STRING_H
#define XPOST_STRING_H

/**
 * @file xpost_string.h
 * @brief string functions
 *
 * An string object is 8 bytes,
 * consisting of 4 16bit fields common to all composite objects
 *   tag, type enum and flags
 *   sz, count of objects in array
 *   ent, entity number   --- nb. ents have outgrown their field: use xpost_object_get/set_ent()
 *   off, offset into allocation
 * the entity data is a "C" array of chars
 *
 * "_memory" functions require a memory file to be specified.
 * functions without "memory" select the memory file from a context, using the FBANK flag.
 * an array object with the FBANK flag properly set, is called a "banked array".
 *
 * @{
 */

/**
 * @brief construct a string object (possibly initialized) in the specified memory
 */
Xpost_Object xpost_string_cons_memory(Xpost_Memory_File *mem,
                                      unsigned sz,
                                      /*@NULL@*/ const char *ini);

/**
 * @brief construct a string object in currectly selected memory
 */
Xpost_Object xpost_string_cons(Xpost_Context *ctx,
                               unsigned sz,
                               /*@NULL@*/ const char *ini);

/**
 * @brief yield a "C" pointer to the char array of the string contents
 */
/*@dependent@*/
char *xpost_string_get_pointer(Xpost_Context *ctx,
                               Xpost_Object S);

/**
 * @brief put a value into a string with specified memory
 */
int xpost_string_put_memory(Xpost_Memory_File *mem,
                            Xpost_Object s,
                            integer i,
                            integer c);

/**
 * @brief put a value into a string
 */
int xpost_string_put(Xpost_Context *ctx,
                     Xpost_Object s,
                     integer i,
                     integer c);

/**
 * @brief get a value from a string with specified memory
 */
int xpost_string_get_memory(Xpost_Memory_File *mem,
                            Xpost_Object s,
                            integer i,
                            integer *retval);

/**
 * @brief get a value from a string
 */
int xpost_string_get(Xpost_Context *ctx,
                     Xpost_Object s,
                     integer i,
                     integer *retval);

/**
 * @}
 */

#endif
