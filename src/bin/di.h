#ifndef XPOST_DI_H
#define XPOST_DI_H

/* dictionaries
   a dictionary object is 8 bytes
   consisting of 4 16bit fields common to composite objects
     tag, type enum and flags
     sz, count of objects in array
     ent, entity number
     off, offset into allocation
   the entity data is a header structure
   followed by header->sz+1 key/value pairs of objects in a linear array
   null keys denote empty slots in the hash table.
   */

typedef struct {
    word tag;
    word sz;
    word nused;
    word pad;
} dichead;

/* dicts are implicitly 1 entry larger than declared
   in order to simplify searching (terminate on null) */

/* DICTABN yields the number of real entries for a dict of size n */
#define DICTABN(n) (2 * ((n)+1))

/* DICTABSZ yields the size in bytes */
#define DICTABSZ(n) (DICTABN(n) * sizeof(object))

/* compare objects (<,=,>) :: (-(x),0,+(x)) */
int objcmp(context *ctx, object l, object r);

/* construct dictionary
   in mtab of specified mfile */
object consdic(/*@dependent@*/ mfile *mem, unsigned sz);

/* construct dictionary
   selected mtab with ctx->vmmode */
object consbdc(context *ctx, unsigned sz);

/* investigate current sizes of dictionary */
unsigned diclength(/*@dependent@*/ mfile *mem, object d);
unsigned dicmaxlength(/*@dependent@*/ mfile *mem, object d);
bool dicfull(/*@dependent@*/ mfile *mem, object d);

void dumpdic(mfile *mem, object d);

double doubleextended (object e);
object unextend (object e);

/* test dictionary for key */
bool dicknown(context *ctx, /*@dependent@*/ mfile *mem, object d, object k);

/* lookup value */
object dicget(context *ctx, /*@dependent@*/ mfile *mem, object d, object k);
object bdcget(context *ctx, object d, object k);

/* store value */
void dicput(context *ctx, /*@dependent@*/ mfile *mem, object d, object k, object v);
void bdcput(context *ctx, object d, object k, object v);

/* undefine key */
void bdcundef(context *ctx, object d, object k);

#endif
