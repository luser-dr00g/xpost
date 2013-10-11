#ifndef XPOST_M_H
#define XPOST_M_H

/** \file xpost_memory.c
    mfile and mtab - the memory management data structures.

    An mfile is a container representing *half* of virtual memory,
    either local or global. A global-vm mfile has more special entities
    in its mtab than a local-vm mfile.

    An mtab is a chain of address tables (starting at address 0)
    that manage the contents of the mfile.

    This file also contains enums that more properly belong in itp.h
    as they direct the global configuration of the interpreter, not just
    the memory interface.

    Within the context of mfile operations the terms "pointer" (in
    quotes) and "address" (with or without quotes) typically refer
    to an unsigned integer that specifies a byte offset from
    the mfile's base pointer. Stored addresses are thus relative
    to the current location of the memory structure in the computer's
    memory, and the memory may move if necessary to grow to
    accomodate an allocation.

    Within the context of mtab operations, an entity serves the
    purpose of a pointer, as an opaque handle that refers to
    contents in memory. Within mtab functions, relative
    entity numbers are used to directly index a single mtab
    segment. But only "absolute" entities should be stored,
    that is, relative to the root of the chain of tables.
*/

/** \var pgsz
  the "grain" of the mfile size
*/
extern unsigned pgsz /*= getpagesize()*/; /*=4096 (usually on 32bit Linux)*/

/** \typedef mfile
    \brief A memory/file. Bookkeeping data for region allocator.
    Used as the basis for the Postscript Virtual Memory.
*/
typedef struct {
    int fd; /**< file descriptor associated with this mfile */
    char fname[20]; /**< file name associated with thie mfile */
    /*@dependent@*/ unsigned char *base; /**< pointer to mapped memory */
    unsigned used; /**< size used, cursor to free space */
    unsigned max; /**< size available in memory pointed to by base */

    unsigned start; /**< first "live" entry in mtab.tab[] */
        /* the domain of the collector is entries >= start */
} mfile;

/** \fn void dumpmfile(mfile *mem)
  \brief dump metadata and contents to stdout
*/
void dumpmfile(mfile *mem);

/** \fn void initmem(mfile *mem, char *fname, int fd)
  \brief initialize the mfile, possibly from file
*/
void initmem(mfile *mem, char *fname, int fd);

/** \fn void exitmem(mfile *mem)
  \brief destroy the mfile, possibly writing to file
*/
void exitmem(mfile *mem);

/** \fn void growmem(mfile *mem, unsigned sz)
  \brief resize the mfile, possibly invalidating all vm pointers
*/
void growmem(mfile *mem, unsigned sz);

/** \fn unsigned mfalloc(mfile *mem, unsigned sz)
  \brief allocate memory, returns offset in memory file
*/
unsigned mfalloc(mfile *mem, unsigned sz);


/** \def TABSZ
    \brief number of entries in a single segment of the table

    This parameter may be tuned for performance.

    Most VM access (composite object data) has to go through
    the mtab, which is segmented. So depending on the number
    of live entries (k), accessing data from an object may 
    require chasing through k/TABSZ tables before finding
    the right one. This isn't a lengthy operation, but it
    is a complicated address calculation that may not be
    pipeline-friendly.
*/
#define TABSZ 200

/** \typdef mtab
    \brief the segmented table structure
*/
typedef struct {
    unsigned nexttab; /* next table in chain */
    unsigned nextent; /* next slot in table, or TABSZ if none */
    struct {
        unsigned adr;
        unsigned sz;
        unsigned mark;
        unsigned tag;
    } tab[TABSZ];
} mtab;


/** \def markfields
  \brief fields in mtab.tab[].mark

  There are 4 "virtual" bitfields packed in what is assumed
  to be a 32bit unsigned field. MARK, RFCT (= ReFerence CounT),
  LLEV (= Lowest saved LEVel), TLEV (= Top saved LEVel).
  The fields are described by *M symbols which are masks,
  and *O symbols which are offsets. These values are used
  in masking and shifting operations to access the fields
  in a direct, portable manner.
 */
enum markfields {
    MARKM = 0xFF000000,
    MARKO = 24,
    RFCTM = 0x00FF0000,
    RFCTO = 16,
    LLEVM = 0x0000FF00,
    LLEVO = 8,
    TLEVM = 0x000000FF,
    TLEVO = 0,
};

/* \def special
   \brief special entries for special entities

   local mfiles set .start to BOGUSNAME+1, and all
   context stacks are in the root set.
   global mfiles set .start to OPTAB+1 and NAMES
   is in the root set (and all context /globaldict's?).
*/
enum special {
    FREE,
    VS,
    CTXLIST,
    NAMES,
    NAMET,
    BOGUSNAME,
    OPTAB,  /* this 1 global only */
};

/** \fn void dumpmtab(mfile *mem, unsigned mtabadr)
  \brief dump mtab details to stdout
*/
void dumpmtab(mfile *mem, unsigned mtabadr);

/** \fn unsigned initmtab(mfile *mem)
  \brief allocate and initialize a new table
*/
unsigned initmtab(mfile *mem);

/** \fn unsigned mtalloc(mfile *mem, unsigned mtabadr, unsigned sz, unsigned tag)
  \brief allocate memory, returns table index
 */
unsigned mtalloc(mfile *mem, unsigned mtabadr, unsigned sz, unsigned tag);

/** \fn void findtabent(mfile *mem,  mtab **atab,  unsigned *aent)
  \brief find the table and relative entity index for an absolute entity index
*/
void findtabent(mfile *mem, /*@out@*/ mtab **atab, /*@out@*/ unsigned *aent);

/** \fn unsigned adrent(mfile *mem, unsigned ent)
  \brief get the address from an entity
*/
unsigned adrent(mfile *mem, unsigned ent);

/** \fn unsigned szent(mfile *mem, unsigned ent)
  \brief get the size of an entity
 */
unsigned szent(mfile *mem, unsigned ent);

/** \fn void get(mfile *mem, unsigned ent, unsigned offset, unsigned sz,  void *dest)
  \brief fetch a value from a composite object
 */
void get(mfile *mem,
        unsigned ent, unsigned offset, unsigned sz,
        /*@out@*/ void *dest);

/** \fn void put(mfile *mem, unsigned ent, unsigned offset, unsigned sz,  void *src)
  \brief put a value into a composite object
 */
void put(mfile *mem,
        unsigned ent, unsigned offset, unsigned sz,
        /*@in@*/ void *src);

/** \fn int test_memory()
  perform functionality tests on the memory module.
*/
int test_memory();

#endif
