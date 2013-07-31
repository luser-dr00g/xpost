/* arrays
   an array object is 8 bytes,
   consisting of 4 16bit fields common to all composite objects
     tag, type enum and flags
     sz, count of objects in array
     ent, entity number
     off, offset into allocation
   the entity data is a "C" array of objects
   */

/* consarr - construct an array object
   in the mtab of specified mfile */
object consarr(mfile *mem, unsigned sz);

/* consbar - construct an array object
   selecting mfile according to ctx->vmmode */
object consbar(context *ctx, unsigned sz);

/* store value */
void arrput(mfile *mem, object a, integer i, object o);
void barput(context *ctx, object a, integer i, object o);

/* extract value */
object arrget(mfile *mem, object a, integer i);
object barget(context *ctx, object a, integer i);

/* adjust the size and offset fields in the object
   (works for strings, too)
 */
object arrgetinterval(object a, integer s, integer n);


