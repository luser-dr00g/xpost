#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef HAVE_STDBOOL_H
# include <stdbool.h>
#else
# ifndef HAVE__BOOL
#  ifdef __cplusplus
typedef bool _Bool;
#  else
#   define _Bool signed char
#  endif
# endif
# define bool _Bool
# define false 0
# define true 1
# define __bool_true_false_are_defined 1
#endif

#include <assert.h>
#include <ctype.h> /* isprint */
#include <stdlib.h> /* exit free malloc realloc */
#include <stdio.h> /* fprintf printf perror */
#include <string.h> /* memset */
#include <unistd.h> /* getpagesize */

#include <sys/stat.h> /* open */
#include <fcntl.h> /* open */

#ifdef HAVE_SYS_MMAN_H
# include <sys/mman.h> /* mmap munmap mremap */
#endif

#ifdef _WIN32
# include <io.h>
#endif



#include "m.h"  // double-check prototypes
#include "ob.h"  // mfiles contain objects
#include "itp.h"  // initialize interpreter to test
#include "err.h"  // memory functions may throw errors

unsigned pgsz /*= getpagesize()*/ = 4096;

int getmemfile(char *fname);

/*
typedef struct {
    unsigned char *base;
    unsigned used;
    unsigned max;
} mfile;
*/

/* dump mfile details to stdout */
void dumpmfile(mfile *mem)
{
    unsigned u, v;

    printf("{mfile: base = %p, "
            "used = 0x%x (%u), "
            "max = 0x%x (%u), "
            //"roots = [%d %d], "
            "start = %d}\n",
            mem->base,
            mem->used, mem->used,
            mem->max, mem->max,
            //mem->roots[0], mem->roots[1],
            mem->start);
    for (u=0; u < mem->used; u++) {
        if (u%16 == 0) {
            if (u != 0) {
                for (v= u-16; v < u; v++) {
                    (void)putchar(isprint(mem->base[v])? mem->base[v] : '.');
                }
            }
            printf("\n%06u %04x: ", u, u);
        }
        printf("%02x ", (unsigned) mem->base[u]);
    }
    if ((u-1)%16 != 0) { //did not print in the last iteration of the loop
        for (v= u; v%16 != 0; v++) printf("   ");
        for (v= u - (u%16); v < u; v++) {
            (void)putchar(isprint(mem->base[v])? mem->base[v] : '.');
        }
    }
    (void)puts("");
}

#ifdef _WIN32
# define XPOST_MODE_READ_WRITE _S_IREAD | _S_IWRITE
#else
# define XPOST_MODE_READ_WRITE S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH
#endif

/* memfile exists in path */
int getmemfile(char *fname)
{
    int fd;
    fd = open(
            fname, //"x.mem",
            O_RDWR | O_CREAT,
            XPOST_MODE_READ_WRITE);
    if (fd == -1)
        perror(fname);
    return fd;
}

/* initialize the memory file */
void initmem(mfile *mem,
             char *fname)
{
    int fd = -1;
    struct stat buf;
    size_t sz = pgsz;

    if (fname) {
        fd = getmemfile(fname);
    }
    mem->fd = fd;
    if (fd != -1){
        if (fstat(fd, &buf) == 0) {
            sz = buf.st_size;
            if (sz < pgsz) sz = pgsz;
        }
    }

#ifdef HAVE_MMAP
    mem->base = mmap(NULL,
            sz,
            PROT_READ|PROT_WRITE,
            MAP_SHARED //MAP_PRIVATE
# ifndef HAVE_MREMAP
            |MAP_AUTOGROW
# endif
            | (fd == -1? MAP_ANONYMOUS : 0) , fd, 0);
    if (mem->base == MAP_FAILED) { // .
#else
    mem->base = malloc(sz);
    if (mem->base == NULL) { // ..
#endif
        error(VMerror, "VM error: failed to allocate mfile data");
        exit(EXIT_FAILURE);
    } // . ..
    mem->used = 0;
    mem->max = sz;
#ifndef HAVE_MMAP
    /* read file into malloc'd memory */
    if (fd != -1)
        read(fd, mem->base, sz);
#endif
}

/* destroy the memory file */
void exitmem(mfile *mem)
{
#ifdef HAVE_MMAP
    msync(mem->base, mem->used, MS_SYNC);
    munmap(mem->base, mem->max);
#else
    if (mem->fd != -1) {
        (void)lseek(mem->fd, 0, SEEK_SET);
        write(mem->fd, mem->base, mem->used);
    }
    free(mem->base);
#endif
    mem->base = NULL;
    mem->used = 0;
    mem->max = 0;

    if (mem->fd != -1) {
        /* int ftruncate(int fd, off_t length); */
        /* The truncate() and ftruncate() functions cause the
           regular file named by path or referenced by fd to
           be truncated to a size of precisely length bytes. */
        close(mem->fd);
    }
}

/* reallocate and possibly move mem->base */
mfile *growmem(mfile *mem,
               unsigned sz)
{
    void *tmp;

    printf("growmem: %p %u + %u\n", mem->base, mem->max, sz);
    if (sz < pgsz) sz = pgsz;
    else sz = (sz/pgsz + 1) * pgsz;
    sz += mem->max;
    printf("growmem: new size: %u\n", sz);
#ifdef HAVE_MMAP
# ifdef HAVE_MREMAP
    tmp = mremap(mem->base, mem->max, sz, MREMAP_MAYMOVE);
# else
    tmp = mem->base; /* without mremap, rely on MAP_AUTOGROW */
# endif
    if (tmp == MAP_FAILED)
#else
    tmp = realloc(mem->base, sz);
    if (tmp == NULL)
#endif
        error(VMerror, "unable to grow memory");
    mem->base = tmp;
    mem->max = sz;
    return mem;
}

/* allocate memory, returns offset in memory file
   possible growmem.
   MUST recalculate all pointers derived from mem->base
        after this function
 */
unsigned mfalloc(mfile *mem,
                 unsigned sz)
{
    unsigned adr = mem->used;

    if (sz) {
        if (sz + mem->used >= mem->max)
            mem = growmem(mem,sz);
        mem->used += sz;
        memset(mem->base+adr, 0, sz);  //bzero(mem->base+adr, sz);
        /* bus error with mremap(SHARED,ANON)! */
    }
    return adr;
}


#if 0
#define TABSZ 1000
typedef struct {
    unsigned nexttab; /* next table in chain */
    unsigned nextent; /* next slot in table, or TABSZ if none */
    struct {
        unsigned adr;
        unsigned sz;
        /* add fields here for ref counts or marks */
    } tab[TABSZ];
} mtab;
#endif

/* dump mtab details to stdout */
void dumpmtab(mfile *mem,
              unsigned mtabadr)
{
    unsigned i;
    unsigned e;
    mtab *tab;
    e = 0;

next_table:
    tab = (void *)(mem->base + mtabadr);
    printf("nexttab: 0x%04x\n", tab->nexttab);
    printf("nextent: %u\n", tab->nextent);
    for (i=0; i<tab->nextent; i++, e++) {
        unsigned u;
        printf("ent %d (%d): adr %u 0x%04x, sz [%u], mark %s rfct %d llev %d tlev %d\n",
                e, i,
                tab->tab[i].adr, tab->tab[i].adr,
                tab->tab[i].sz,
                tab->tab[i].mark & MARKM ?"#":"_",
                (tab->tab[i].mark & RFCTM) >> RFCTO,
                (tab->tab[i].mark & LLEVM) >> LLEVO,
                (tab->tab[i].mark & TLEVM) >> TLEVO );
        for (u=0; u < tab->tab[i].sz; u++) {
            printf(" %02x%c",
                    (unsigned)mem->base[ tab->tab[i].adr + u ],
                    isprint((unsigned)mem->base[ tab->tab[i].adr + u ]) ?
                        (unsigned)mem->base[ tab->tab[i].adr + u ] :
                        ' ');
        }
        (void)puts("");
    }
    if (tab->nextent == TABSZ) {
        mtabadr = tab->nexttab;
        goto next_table;
    }
        //dumpmtab(mem, tab->nexttab);
}


/* allocate and initialize a new table */
unsigned initmtab(mfile *mem)
{
    mtab *tab;
    unsigned adr;

    adr = mfalloc(mem, sizeof(mtab));
    tab = (void *)(mem->base + adr);
    tab->nexttab = 0;
    tab->nextent = 0;
    return adr;
}

/* allocate memory, returns table index
   possible growmem.
   MUST recalculate all pointers derived from mem->base
        after this function
 */
unsigned mtalloc(mfile *mem,
                 unsigned mtabadr,
                 unsigned sz)
{
    unsigned ent;
    unsigned adr;
    mtab *tab = (void *)(mem->base + mtabadr);
    int ntab = 0;

    while (tab->nextent >= TABSZ) {
        tab = (void *)(mem->base + tab->nexttab);
        ++ntab;
    }

    ent = tab->nextent;
    ++tab->nextent;

    adr = mfalloc(mem, sz);
    ent += ntab*TABSZ; //recalc
    findtabent(mem, &tab, &ent); //recalc
    tab->tab[ent].adr = adr;
    tab->tab[ent].sz = sz;

    if (tab->nextent == TABSZ){
        unsigned newtab = initmtab(mem);
        ent += ntab*TABSZ; //recalc
        findtabent(mem, &tab, &ent); //recalc
        tab->nexttab = newtab;
    }
    return ent + ntab*TABSZ;
}

/* find the table and relative entity index for an absolute entity index */
void findtabent(mfile *mem,
                /*@out@*/ mtab **atab,
                /*@in@*/ unsigned *aent)
{
    *atab = (void *)(mem->base); // just use mtabadr=0
    while (*aent >= TABSZ) {
        *aent -= TABSZ;
        if ((*atab)->nexttab == 0) {
            error(unregistered, "ent doesn't exist");
        }
        *atab = (void *)(mem->base + (*atab)->nexttab);
    }
}

/* get the address from an entity */
unsigned adrent(mfile *mem,
                unsigned ent)
{
    mtab *tab;// = (void *)(mem->base); // just use mtabadr=0

    assert(mem);
    assert(mem->base);
    findtabent(mem,&tab,&ent);
    assert(tab);
    return tab->tab[ent].adr;
}

/* get the size of an entity */
unsigned szent(mfile *mem,
               unsigned ent)
{
    mtab *tab;// = (void *)(mem->base); // just use mtabadr=0
    assert(mem);
    findtabent(mem,&tab,&ent);
    return tab->tab[ent].sz;
}

/* fetch a value from a composite object */
void get(mfile *mem,
        unsigned ent,
        unsigned offset,
        unsigned sz,
        /*@out@*/ void *dest)
{
    mtab *tab;
    unsigned mtabadr = 0;

    tab = (void *)(mem->base + mtabadr);
    while (ent >= TABSZ) {
        mtabadr = tab->nexttab;
        tab = (void *)(mem->base + mtabadr);
        ent -= TABSZ;
    }

    if (offset*sz /*+ sz*/ > tab->tab[ent].sz)
        error(rangecheck, "get: out of bounds");

    memcpy(dest, mem->base + tab->tab[ent].adr + offset*sz, sz);
}

/* put a value into a composite object */
void put(mfile *mem,
        unsigned ent,
        unsigned offset,
        unsigned sz,
        /*@in@*/ void *src)
{
    mtab *tab;
    unsigned mtabadr = 0;

    tab = (void *)(mem->base + mtabadr);
    while (ent >= TABSZ){
        mtabadr = tab->nexttab;
        tab = (void *)(mem->base + mtabadr);
        ent -= TABSZ;
    }

    if (offset*sz /*+ sz*/ > tab->tab[ent].sz)
        error(rangecheck, "put: out of bounds");

    memcpy(mem->base + tab->tab[ent].adr + offset*sz, src, sz);
}

#ifdef TESTMODULE_M

mfile mem;

/* initialize everything */
void init(void)
{
    pgsz = getpagesize();
    initmem(&mem, "x.mem");
    (void)initmtab(&mem); /* create mtab at address zero */
}

void xit(void)
{
    exitmem(&mem);
}

int main(){
    init();
    unsigned ent;
    int seven = 7;
    int ret;

    printf("\n^test m.c\n");
    //printf("getmemfile: %d\n", getmemfile());

    ent = mtalloc(&mem, 0, sizeof seven);
    put(&mem, ent, 0, sizeof seven, &seven);
    get(&mem, ent, 0, sizeof seven, &ret);
    printf("put %d, got %d\n", seven, ret);

    unsigned ent2;
    ent2 = mtalloc(&mem, 0, 8*sizeof seven);
    put(&mem, ent2, 6, sizeof seven, &seven);
    get(&mem, ent2, 6, sizeof seven, &ret);
    printf("put %d in slot 7, got %d\n", seven, ret);
    //get(&mem, ent2, 9, sizeof seven, &ret);
    //printf("attempted to retrieve element 10 from an 8-element array, got %d\n", ret);

    unsigned ent3;
    char str[] = "beads in buddha's necklace";
    char sret[sizeof str];
    ent3 = mtalloc(&mem, 0, strlen(str)+1);
    put(&mem, ent3, 0, sizeof str, str);
    get(&mem, ent3, 0, sizeof str, sret);
    printf("stored and retrieved %s\n", sret);

    dumpmtab(&mem, 0);
    xit();
    return 0;
}
#endif
