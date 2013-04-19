
void initfree(mfile *mem);
unsigned gballoc(mfile *mem, unsigned sz);
void mfree(mfile *mem, unsigned ent);
void collect(mfile *mem);
unsigned mfrealloc(mfile *mem, unsigned oldadr, unsigned oldsize, unsigned newsize);

