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
#define DICTABSZ(n) (DICTABN(n) * sizeof(object))

/*! \fn int objcmp(context *ctx, object l, object r)
   compare objects (<,=,>) :: (-(x),0,+(x))
*/
int objcmp(context *ctx, object l, object r);

/*! \fn object consdic( mfile *mem, unsigned sz)
   construct dictionary
   in mtab of specified mfile
*/
object consdic(/*@dependent@*/ mfile *mem, unsigned sz);

/*! \fn object consbdc(context *ctx, unsigned sz)
   construct dictionary
   selected mtab with ctx->vmmode
*/
object consbdc(context *ctx, unsigned sz);

/*! \fn unsigned diclength( mfile *mem, object d)
   investigate current number of entries in dictionary
 */
unsigned diclength(/*@dependent@*/ mfile *mem, object d);

/*! \fn unsigned dicmaxlength( mfile *mem, object d)
   investigate current maximum size of dictionary
 */
unsigned dicmaxlength(/*@dependent@*/ mfile *mem, object d);

/*! \fn bool dicfull( mfile *mem, object d)
   investigate if size == maximum size.
 */
bool dicfull(/*@dependent@*/ mfile *mem, object d);

/*! \fn void dumpdic(mfile *mem, object d)
   print a dump of the diction contents to stdout
*/
void dumpdic(mfile *mem, object d);

/*! \fn double doubleextended (object e)
   return a double value containing the truncated value from 
   an extendedtype object
*/
double doubleextended (object e);

/*! \fn object unextend (object e)
   convert an extendedtype object back to its original
   integer- or real-type object.
*/
object unextend (object e);

/*! \fn bool dicknown(context *ctx,  mfile *mem, object d, object k)
   test dictionary for key
 */
bool dicknown(context *ctx, /*@dependent@*/ mfile *mem, object d, object k);

/*! \fn object dicget(context *ctx,  mfile *mem, object d, object k)
   lookup value using key in dictionary
*/
object dicget(context *ctx, /*@dependent@*/ mfile *mem, object d, object k);

/*! \fn object bdcget(context *ctx, object d, object k)
   lookup value using key in banked dictionary
*/
object bdcget(context *ctx, object d, object k);

/*! \fn void dicput(context *ctx,  mfile *mem, object d, object k, object v)
   store key and value in dictionary
*/
void dicput(context *ctx, /*@dependent@*/ mfile *mem, object d, object k, object v);

/*! \fn void bdcput(context *ctx, object d, object k, object v)
   store key and value in banked dictionary
*/
void bdcput(context *ctx, object d, object k, object v);

/*! \fn void dicundef(context *ctx, mfile *mem, object d, object k)
   undefine key in dictionary
   NOT IMPLEMENTED
*/
void dicundef(context *ctx, mfile *mem, object d, object k);

/*! \fn void bdcundef(context *ctx, object d, object k)
   undefine key in banked dictionary
   NOT IMPLEMENTED
*/
void bdcundef(context *ctx, object d, object k);

#endif
