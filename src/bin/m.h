#ifndef XPOST_M_H
#define XPOST_M_H

/*  memory (master)
    mfile and mtab

    An mfile is a container representing *half* of virtual memory,
    either local or global. A global-vm mfile has more special entities
    in its mtab than a local-vm mfile.

    An mtab is a chain of address tables (starting at address 0)
    that manage the contents of the mfile.

    This file also contains enums that more properly belong in itp.h
    as they direct the global configuration of the interpreter, not just
    the memory interface.
   */

/* the "grain" of the mfile size */
extern unsigned pgsz /*= getpagesize()*/; /*=4096 (usually on 32bit)*/

typedef struct {
    int fd;
    /*@dependent@*/ unsigned char *base;
    unsigned used;
    unsigned max;

    //no longer used, all sharing contexts have roots
    //unsigned roots[2]; /* low, high : entries */

    unsigned start; /* first "live" entry */
        /* the domain of the collector is entries >= start */
} mfile;

/* dump metadata and contents */
void dumpmfile(mfile *mem);

/* initialize the mfile, possibly from file */
void initmem(mfile *mem, char *fname);

/* destroy the mfile, possibly writing to file */
void exitmem(mfile *mem);

/* resize the mfile, possibly invalidating all vm pointers */
mfile *growmem(mfile *mem, unsigned sz);

/* allocate memory, returns offset in memory file */
unsigned mfalloc(mfile *mem, unsigned sz);


#define TABSZ 200
typedef struct {
    unsigned nexttab; /* next table in chain */
    unsigned nextent; /* next slot in table, or TABSZ if none */
    struct {
        unsigned adr;
        unsigned sz;
        unsigned mark;
    } tab[TABSZ];
} mtab;


/* fields in mtab.tab[].mark */
enum {
    MARKM = 0xFF000000,
    MARKO = 24,
    RFCTM = 0x00FF0000,
    RFCTO = 16,
    LLEVM = 0x0000FF00,
    LLEVO = 8,
    TLEVM = 0x000000FF,
    TLEVO = 0,
};

/* special entries */
/* local mfiles set .start to CTXLIST+1,
      and all context stacks are in the root set
   global mfiles set .start to OPTAB+1
      and NAMES is in the root set
      (and all context /globaldict's?)
 */
enum {
    FREE,
    VS,
    CTXLIST,
    NAMES,
    NAMET,
    BOGUSNAME,
    OPTAB,  /* this 1 global only */
};

/* dump mtab details to stdout */
void dumpmtab(mfile *mem, unsigned mtabadr);

/* allocate and initialize a new table */
unsigned initmtab(mfile *mem);

/* allocate memory, returns table index */
unsigned mtalloc(mfile *mem, unsigned mtabadr, unsigned sz);

/* find the table and relative entity index for an absolute entity index */
void findtabent(mfile *mem, /*@out@*/ mtab **atab, /*@out@*/ unsigned *aent);

/* get the address from an entity */
unsigned adrent(mfile *mem, unsigned ent);

/* get the size of an entity */
unsigned szent(mfile *mem, unsigned ent);

/* fetch a value from a composite object */
void get(mfile *mem,
        unsigned ent, unsigned offset, unsigned sz,
        /*@out@*/ void *dest);

/* put a value into a composite object */
void put(mfile *mem,
        unsigned ent, unsigned offset, unsigned sz,
        /*@in@*/ void *src);

#endif
