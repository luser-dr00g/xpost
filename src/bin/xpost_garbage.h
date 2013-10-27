#ifndef XPOST_GC_H
#define XPOST_GC_H

/* garbage collector
   */

enum {
    PERIOD = 1000  /* number of times to grow before collecting */
};

/* initialize the FREE special entity which points to the head of the free list */
void initfree(mfile *mem);
void dumpfree(mfile *mem);

/* allocate data, re-using garbage if possible */
unsigned gballoc(mfile *mem, unsigned sz, unsigned tag);

/* explicitly add ent to free list */
unsigned mfree(mfile *mem, unsigned ent);

/* perform a collection on mfile */
unsigned collect(mfile *mem, int dosweep, int markall);

/* reallocate data, preserving (the maximum of) original contents */
unsigned mfrealloc(mfile *mem, unsigned oldadr, unsigned oldsize, unsigned newsize);

int test_garbage_collect(void);
#endif
