#ifndef XPOST_AR_H
#define XPOST_AR_H

/** \file xpost_array.h
   \brief array functions

   an array object is 8 bytes,
   consisting of 4 16bit fields common to all composite objects
     tag, type enum and flags
     sz, count of objects in array
     ent, entity number
     off, offset into allocation
   the entity data is a "C" array of objects

   "arr" functions require an mfile.
   "bar" functions select the mfile from a context, using the FBANK flag.
*/

/**
 * @brief consarr - construct an array object
 * in the mtab of specified mfile
*/
Xpost_Object consarr(mfile *mem, unsigned sz);

/**
 * @brief consbar - construct an array object
 * selecting mfile according to ctx->vmmode
*/
Xpost_Object consbar(context *ctx, unsigned sz);

/** 
   * @brief store value in an array
*/
void arrput(mfile *mem, Xpost_Object a, integer i, Xpost_Object o);

/** 
 * @brief store value in a banked array
*/
void barput(context *ctx, Xpost_Object a, integer i, Xpost_Object o);

/** 
 * @brief extract value from an array
*/
Xpost_Object arrget(mfile *mem, Xpost_Object a, integer i);

/** 
 * @brief extract value from a banked array
*/
Xpost_Object barget(context *ctx, Xpost_Object a, integer i);

/** 
 * @brief adjust the size and offset fields in the object
 * (works for strings, too)
*/
Xpost_Object arrgetinterval(Xpost_Object a, integer s, integer n);

#endif
