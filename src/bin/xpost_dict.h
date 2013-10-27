#ifndef XPOST_DI_H
#define XPOST_DI_H

/*! \file di.h
   \brief dictionary functions

   a dictionary object is 8 bytes
   consisting of 4 16bit fields common to composite objects
     tag, type enum and flags
     sz, count of objects in array
     ent, entity number
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
int objcmp(context *ctx, Xpost_Object l, Xpost_Object r);

/*! 
   construct dictionary
   in mtab of specified mfile
*/
Xpost_Object consdic(/*@dependent@*/ mfile *mem, unsigned sz);

/*! 
   construct dictionary
   selected mtab with ctx->vmmode
*/
Xpost_Object consbdc(context *ctx, unsigned sz);

/*! 
   investigate current number of entries in dictionary
 */
unsigned diclength(/*@dependent@*/ mfile *mem, Xpost_Object d);

/*! 
   investigate current maximum size of dictionary
 */
unsigned dicmaxlength(/*@dependent@*/ mfile *mem, Xpost_Object d);

/*! 
   investigate if size == maximum size.
 */
bool dicfull(/*@dependent@*/ mfile *mem, Xpost_Object d);

/*! 
   print a dump of the diction contents to stdout
*/
void dumpdic(mfile *mem, Xpost_Object d);

/*! 
   return a double value containing the truncated value from 
   an extendedtype object
*/
double doubleextended (Xpost_Object e);

/*! 
   convert an extendedtype object back to its original
   integer- or real-type object.
*/
Xpost_Object unextend (Xpost_Object e);

/*! 
   test dictionary for key
 */
bool dicknown(context *ctx, /*@dependent@*/ mfile *mem, Xpost_Object d, Xpost_Object k);

/*! 
   lookup value using key in dictionary
*/
Xpost_Object dicget(context *ctx, /*@dependent@*/ mfile *mem, Xpost_Object d, Xpost_Object k);

/*! 
   lookup value using key in banked dictionary
*/
Xpost_Object bdcget(context *ctx, Xpost_Object d, Xpost_Object k);

/*! 
   store key and value in dictionary
*/
void dicput(context *ctx, /*@dependent@*/ mfile *mem, Xpost_Object d, Xpost_Object k, Xpost_Object v);

/*! 
   store key and value in banked dictionary
*/
void bdcput(context *ctx, Xpost_Object d, Xpost_Object k, Xpost_Object v);

/*! 
   undefine key in dictionary
   NOT IMPLEMENTED
*/
void dicundef(context *ctx, mfile *mem, Xpost_Object d, Xpost_Object k);

/*! 
   undefine key in banked dictionary
   NOT IMPLEMENTED
*/
void bdcundef(context *ctx, Xpost_Object d, Xpost_Object k);

#endif
