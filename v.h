/* save/restore
   Each mfile has a special entity (VS) which holds the address
   of the "save stack". This stack holds save objects.

   The save object contains an address of a(nother) stack,
   this one containing saverec_ structures.
   A saverec object contains 2 entity numbers, one the source,
   the other the copy, of the "saved" array or dictionary.

   */

void initsave(mfile *mem);
object save(mfile *mem);
unsigned stashed(mfile *mem, unsigned ent);
unsigned copy(mfile *mem, unsigned ent);
void stash(mfile *mem, unsigned ent);
void restore(mfile *mem);


