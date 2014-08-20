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

#ifndef XPOST_DI_H
#define XPOST_DI_H

/*! \file di.h
   \brief dictionary functions

   a dictionary object is 8 bytes
   consisting of 4 16bit fields common to composite objects
     tag, type enum and flags
     sz, count of objects in array
     ent, entity number  --- nb. ents have outgrown their field! use xpost_object_get/set_ent()
     off, offset into allocation
   the entity data is a header structure
   followed by header->sz+1 key/value pairs of objects in a linear array
   null keys denote empty slots in the hash table.

   dicts are implicitly 1 entry larger than declared
   in order to simplify searching (terminate on null)
*/

/*! \typedef typedef struct {} dichead
*/
typedef struct {
    word tag;
    word sz;
    word nused;
    word pad;
} dichead;

typedef struct Xpost_Magic_Pair {
    int (*get)(Xpost_Context *ctx, Xpost_Object dict, Xpost_Object key, Xpost_Object *pval);
    int (*put)(Xpost_Context *ctx, Xpost_Object dict, Xpost_Object key, Xpost_Object val);
} Xpost_Magic_Pair;

/*! \def DICTABN
   DICTABN yields the number of real entries in the table
   for a dict of size n
*/
#define DICTABN(n) (2 * ((n)+1))

/*! \def DICTABSZ
   DICTABSZ yields the size in bytes of the table
   for a dict of size n
*/
#define DICTABSZ(n) (DICTABN(n) * sizeof(Xpost_Object))

/*! 
   compare objects (<,=,>) :: (-(x),0,+(x))
*/
int objcmp(Xpost_Context *ctx, Xpost_Object l, Xpost_Object r);

/*! 
   construct dictionary
   in the memory table of specified memory file
*/
Xpost_Object xpost_dict_cons_memory (/*@dependent@*/ Xpost_Memory_File *mem, unsigned sz);

/*! 
   construct dictionary
   selected the memory table with ctx->vmmode
*/
Xpost_Object xpost_dict_cons (Xpost_Context *ctx, unsigned sz);

/*! 
   investigate current number of entries in dictionary
 */
unsigned xpost_dict_length_memory (/*@dependent@*/ Xpost_Memory_File *mem, Xpost_Object d);

/*! 
   investigate current maximum size of dictionary
 */
unsigned xpost_dict_max_length_memory (/*@dependent@*/ Xpost_Memory_File *mem, Xpost_Object d);

/*! 
   investigate if size == maximum size.
 */
int xpost_dict_is_full_memory (/*@dependent@*/ Xpost_Memory_File *mem, Xpost_Object d);

/*! 
   print a dump of the diction contents to stdout
*/
void xpost_dict_dump_memory (Xpost_Memory_File *mem, Xpost_Object d);

/*! 
   return a double value containing the truncated value from 
   an extendedtype object
*/
double xpost_dict_convert_extended_to_double (Xpost_Object e);

/*! 
   convert an extendedtype object back to its original
   integer- or real-type object.
*/
Xpost_Object xpost_dict_convert_extended_to_number (Xpost_Object e);

/*! 
   test dictionary for key
 */
int xpost_dict_known_key(Xpost_Context *ctx, /*@dependent@*/ Xpost_Memory_File *mem, Xpost_Object d, Xpost_Object k);

/*! 
   lookup value using key in dictionary
*/
Xpost_Object xpost_dict_get_memory (Xpost_Context *ctx, /*@dependent@*/ Xpost_Memory_File *mem, Xpost_Object d, Xpost_Object k);

/*! 
   lookup value using key in banked dictionary
*/
Xpost_Object xpost_dict_get(Xpost_Context *ctx, Xpost_Object d, Xpost_Object k);

/*! 
   store key and value in dictionary
*/
int xpost_dict_put_memory(Xpost_Context *ctx, /*@dependent@*/ Xpost_Memory_File *mem, Xpost_Object d, Xpost_Object k, Xpost_Object v);

/*! 
   store key and value in banked dictionary
*/
int xpost_dict_put(Xpost_Context *ctx, Xpost_Object d, Xpost_Object k, Xpost_Object v);

/*! 
   undefine key in dictionary
   NOT IMPLEMENTED
*/
int xpost_dict_undef_memory(Xpost_Context *ctx, Xpost_Memory_File *mem, Xpost_Object d, Xpost_Object k);

/*! 
   undefine key in banked dictionary
   NOT IMPLEMENTED
*/
void xpost_dict_undef(Xpost_Context *ctx, Xpost_Object d, Xpost_Object k);

#endif
