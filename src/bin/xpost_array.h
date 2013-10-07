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

/** \fn object consarr(mfile *mem, unsigned sz)
  consarr - construct an array object
  in the mtab of specified mfile
*/
object consarr(mfile *mem, unsigned sz);

/** \fn object consbar(context *ctx, unsigned sz)
  consbar - construct an array object
  selecting mfile according to ctx->vmmode
*/
object consbar(context *ctx, unsigned sz);

/** \fn void arrput(mfile *mem, object a, integer i, object o)
   store value in an array
*/
void arrput(mfile *mem, object a, integer i, object o);

/** \fn void barput(context *ctx, object a, integer i, object o)
   store value in a banked array
*/
void barput(context *ctx, object a, integer i, object o);

/** \fn object arrget(mfile *mem, object a, integer i)
   extract value from an array
*/
object arrget(mfile *mem, object a, integer i);

/** \fn object barget(context *ctx, object a, integer i)
   extract value from a banked array
*/
object barget(context *ctx, object a, integer i);

/** \fn object arrgetinterval(object a, integer s, integer n)
   adjust the size and offset fields in the object
   (works for strings, too)
*/
object arrgetinterval(object a, integer s, integer n);

#endif
