/* garbage collector
   */

/* initialize the FREE special entity which points to the head of the free list */
void initfree(mfile *mem);

/* allocate data, re-using garbage if possible */
unsigned gballoc(mfile *mem, unsigned sz);

/* explicitly add ent to free list */
void mfree(mfile *mem, unsigned ent);

/* perform a collection on mfile */
void collect(mfile *mem);

/* reallocate data, preserving (the maximum of) original contents */
unsigned mfrealloc(mfile *mem, unsigned oldadr, unsigned oldsize, unsigned newsize);

