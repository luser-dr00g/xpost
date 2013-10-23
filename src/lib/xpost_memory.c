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
#include <stdio.h> /* fprintf printf putchar puts */
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


#include "xpost_log.h"
#include "xpost_memory.h"

/* stubs */

/* FIXME: use xpost_log instead */

enum { rangecheck, VMerror, unregistered };
static void error(int err, char *msg)
{
    (void)err;
    XPOST_LOG_ERR("error: %d %s\n", err, msg);
    //fprintf(stderr, "error: %s\n", msg);
    //exit(EXIT_FAILURE);
}

/* FIXME: use autotools to check if getpagesize exists ? */
unsigned int xpost_memory_pagesize /*= getpagesize()*/ = 4096;


int xpost_memory_file_init (
        Xpost_Memory_File *mem,
        const char *fname,
        int fd)
{
    struct stat buf;
    size_t sz = xpost_memory_pagesize;

    if(fname)
    {
        strncpy(mem->fname, fname, sizeof(mem->fname));
        mem->fname[sizeof(mem->fname) - 1] = '\0';
    }
    else
        mem->fname[0] = '\0';

    mem->fd = fd;
    if (fd != -1){
        if (fstat(fd, &buf) == 0)
        {
            sz = buf.st_size;
            if (sz < xpost_memory_pagesize)
            {
                sz = xpost_memory_pagesize;
#ifdef HAVE_MMAP
                ftruncate(fd, sz);
#endif
            }
        }
    }

#ifdef HAVE_MMAP
    mem->base = mmap(NULL,
            sz,
            PROT_READ|PROT_WRITE,
            (fd == -1? MAP_PRIVATE : MAP_SHARED)
# ifndef HAVE_MREMAP
            | MAP_AUTOGROW
# endif
            | (fd == -1? MAP_ANONYMOUS : 0), fd, 0);
    if (mem->base == MAP_FAILED)
    { // .
#else
    mem->base = malloc(sz);
    if (mem->base == NULL)
    { // ..
#endif
        error(VMerror, "VM error: failed to allocate memory-file data");
        return 0;
        //exit(EXIT_FAILURE);
    } // . ..
    mem->used = 0;
    mem->max = sz;
#ifndef HAVE_MMAP
    /* read file into malloc'd memory */
    if (fd != -1)
        read(fd, mem->base, sz);
#endif
    if (fd == -1)
        memset(mem->base, 0, mem->max);

    return 1;
}


int xpost_memory_file_exit (Xpost_Memory_File *mem)
{
#ifdef HAVE_MMAP
    munmap(mem->base, mem->max);
#else
    if (mem->fd != -1)
    {
        (void) lseek(mem->fd, 0, SEEK_SET);
        write(mem->fd, mem->base, mem->used);
    }
    free(mem->base);
#endif
    mem->base = NULL;
    mem->used = 0;
    mem->max = 0;

    if (mem->fd != -1)
    {
        close(mem->fd);
        mem->fd = -1;
    }
    if (mem->fname[0] != '\0')
    {
        struct stat sb;
        if (stat(mem->fname, &sb) == 0)
            remove(mem->fname);
        mem->fname[0] = '\0';
    }

    return 1;
}

int xpost_memory_file_grow (
        Xpost_Memory_File *mem,
        unsigned int sz)
{
    void *tmp;

    if (sz < xpost_memory_pagesize)
        sz = xpost_memory_pagesize;
    else
        sz = (sz/xpost_memory_pagesize + 1) * xpost_memory_pagesize;
    sz += mem->max;

#ifdef HAVE_MMAP
    ftruncate(mem->fd, sz);
# ifdef HAVE_MREMAP
    tmp = mremap(mem->base, mem->max, sz, MREMAP_MAYMOVE);
# else
    msync(mem->base, mem->used, MS_SYNC);
    munmap(mem->base, mem->max);
    lseek(mem->fd, 0, SEEK_SET);
    ftruncate(mem->fd, sz);
    tmp = mmap(NULL, sz,
            PROT_READ | PROT_WRITE,
            mem->fd == -1? MAP_ANONYMOUS|MAP_PRIVATE : MAP_SHARED,
            mem->fd, 0);
# endif
    if (tmp == MAP_FAILED)
    {
#else
    tmp = realloc(mem->base, sz);
    if (tmp == NULL)
    {
#endif
        error(VMerror, "unable to grow memory");
#ifdef HAVE_MMAP
# ifndef HAVE_MREMAP
        tmp = mmap(NULL, mem->max,
                PROT_READ|PROT_WRITE,
                mem->fd == -1? MAP_ANONYMOUS|MAP_PRIVATE : MAP_SHARED,
                mem->fd, 0);
# endif
#endif
    }
    mem->base = tmp;
    mem->max = sz;

    return 1;
}


unsigned int xpost_memory_file_alloc (
        Xpost_Memory_File *mem,
        unsigned int sz)
{
    unsigned int adr = mem->used;

    if (sz)
    {
        if (sz + mem->used >= mem->max)
            xpost_memory_file_grow(mem, sz);

        mem->used += sz;
        memset(mem->base + adr, 0, sz);
    }
    return adr;
}


void xpost_memory_file_dump (const Xpost_Memory_File *mem)
{
    unsigned int u,v;

    printf("{mfile: base = %p, "
            "used = 0x%x (%u), "
            "max = 0x%x (%u), "
            "start = %d}\n",
            mem->base,
            mem->used, mem->used,
            mem->max, mem->max,
            mem->start);
    for (u = 0; u < mem->used; u++)
    {
        if (u%16 == 0)
        {
            if (u != 0)
            {
                for (v = u-16; v < u; v++)
                {
                    (void)putchar(
                            isprint(mem->base[v]) ?
                            mem->base[v] : '.');
                }
            }
            printf("\n%06u %04x: ", u, u);
        }
        printf("%02x ", mem->base[u]);
    }
    if ((u-1)%16 != 0)
    { // did not print in the last iteration of the loop
        for (v = u; u%16 != 0; v++)
        {
            printf("   ");
        }
        for (v = u - (u%16); v < u; v++)
        {
            (void)putchar(
                    isprint(mem->base[v]) ?
                    mem->base[v] : '.');
        }
    }
    (void)puts("");
}


unsigned int xpost_memory_table_init (Xpost_Memory_File *mem)
{
    Xpost_Memory_Table *tab;
    unsigned int adr;

    adr = xpost_memory_file_alloc(mem, sizeof(Xpost_Memory_Table));
    tab = (Xpost_Memory_Table *)(mem->base + adr);
    tab->nexttab = 0;
    tab->nextent = 0;
    return adr;
}


unsigned int xpost_memory_table_alloc (Xpost_Memory_File *mem,
        unsigned int sz,
        unsigned int tag)
{
    unsigned int mtabadr = 0;
    unsigned int ent;
    unsigned int adr;
    Xpost_Memory_Table *tab = (Xpost_Memory_Table *)(mem->base + mtabadr);
    int ntab = 0;

    while (tab->nextent >= XPOST_MEMORY_TABLE_SIZE)
    {
        tab = (Xpost_Memory_Table *)(mem->base + tab->nexttab);
        ++ntab;
    }

    ent = tab->nextent;
    ++tab->nextent;

    adr = xpost_memory_file_alloc(mem, sz);
    ent += ntab*XPOST_MEMORY_TABLE_SIZE; //recalc
    xpost_memory_table_find_relative(mem, &tab, &ent); //recalc
    tab->tab[ent].adr = adr;
    tab->tab[ent].sz = sz;
    tab->tab[ent].tag = tag;

    if (tab->nextent == XPOST_MEMORY_TABLE_SIZE)
    {
        unsigned int newtab = xpost_memory_table_init(mem);
        ent += ntab*XPOST_MEMORY_TABLE_SIZE; //recalc
        xpost_memory_table_find_relative(mem, &tab, &ent); //recalc
        tab->nexttab = newtab;
    }

    return ent + ntab*XPOST_MEMORY_TABLE_SIZE;
}


void xpost_memory_table_find_relative (
        Xpost_Memory_File *mem,
        Xpost_Memory_Table **atab,
        unsigned int *aent)
{
    *atab = (Xpost_Memory_Table *)(mem->base);
    while (*aent >= XPOST_MEMORY_TABLE_SIZE)
    {
        *aent -= XPOST_MEMORY_TABLE_SIZE;
        if ((*atab)->nexttab == 0)
        {
            error(unregistered, "ent doesn't exist");
        }
        *atab = (Xpost_Memory_Table *)(mem->base + (*atab)->nexttab);
    }
}


unsigned int xpost_memory_table_get_addr (
        Xpost_Memory_File *mem,
        unsigned int ent)
{
    Xpost_Memory_Table *tab;
    xpost_memory_table_find_relative(mem, &tab, &ent);
    return tab->tab[ent].adr;
}


void xpost_memory_table_set_addr (
        Xpost_Memory_File *mem,
        unsigned int ent,
        unsigned int addr)
{
    Xpost_Memory_Table *tab;
    xpost_memory_table_find_relative(mem, &tab, &ent);
    tab->tab[ent].adr = addr;
}


unsigned int xpost_memory_table_get_size (
        Xpost_Memory_File *mem,
        unsigned int ent)
{
    Xpost_Memory_Table *tab;
    xpost_memory_table_find_relative(mem, &tab, &ent);
    return tab->tab[ent].sz;
}


void xpost_memory_table_set_size (
        Xpost_Memory_File *mem,
        unsigned int ent,
        unsigned int size)
{
    Xpost_Memory_Table *tab;
    xpost_memory_table_find_relative(mem, &tab, &ent);
    tab->tab[ent].sz = size;
}


unsigned int xpost_memory_table_get_mark (
        Xpost_Memory_File *mem,
        unsigned int ent)
{
    Xpost_Memory_Table *tab;
    xpost_memory_table_find_relative(mem, &tab, &ent);
    return tab->tab[ent].mark;
}


void xpost_memory_table_set_mark (
        Xpost_Memory_File *mem,
        unsigned int ent,
        unsigned int mark)
{
    Xpost_Memory_Table *tab;
    xpost_memory_table_find_relative(mem, &tab, &ent);
    tab->tab[ent].mark = mark;
}


unsigned int xpost_memory_table_get_tag (
        Xpost_Memory_File *mem,
        unsigned int ent)
{
    Xpost_Memory_Table *tab;
    xpost_memory_table_find_relative(mem, &tab, &ent);
    return tab->tab[ent].tag;
}


void xpost_memory_table_set_tag (
        Xpost_Memory_File *mem,
        unsigned int ent,
        unsigned int tag)
{
    Xpost_Memory_Table *tab;
    xpost_memory_table_find_relative(mem, &tab, &ent);
    tab->tab[ent].tag = tag;
}


void xpost_memory_get (
        Xpost_Memory_File *mem,
        unsigned int ent,
        unsigned int offset,
        unsigned int sz,
        void *dest)
{
    Xpost_Memory_Table *tab;
    xpost_memory_table_find_relative(mem, &tab, &ent);

    if (offset*sz > tab->tab[ent].sz)
        error(rangecheck, "xpost_memory_get: out of bounds");

    memcpy(dest, mem->base + tab->tab[ent].adr + offset*sz, sz);
}

void xpost_memory_put (
        Xpost_Memory_File *mem,
        unsigned int ent,
        unsigned int offset,
        unsigned int sz,
        const void *src)
{
    Xpost_Memory_Table *tab;
    xpost_memory_table_find_relative(mem, &tab, &ent);

    if (offset*sz > tab->tab[ent].sz)
        error(rangecheck, "xpost_memory_put: out of bounds");

    memcpy(mem->base + tab->tab[ent].adr + offset*sz, src, sz);
}


void xpost_memory_table_dump (const Xpost_Memory_File *mem)
{
    unsigned int i;
    unsigned int e = 0;
    unsigned int mtabadr = 0;
    Xpost_Memory_Table *tab;

next_table:
    tab = (Xpost_Memory_Table *)(mem->base + mtabadr);
    printf("nexttab: 0x%04x\n", tab->nexttab);
    printf("nextent: %u\n", tab->nextent);
    for (i = 0; i < tab->nextent; i++, e++)
    {
        unsigned int u;
        printf("ent %d (%d): "
                "adr %u 0x%04x, "
                "sz [%u], "
                "mark %s rfct %d llev %d tlev %d\n",
                e, i,
                tab->tab[i].adr, tab->tab[i].adr,
                tab->tab[i].sz,
                tab->tab[i].mark
                    & XPOST_MEMORY_TABLE_MARK_DATA_MARK_MASK ? "#" : "_",
                (tab->tab[i].mark
                    & XPOST_MEMORY_TABLE_MARK_DATA_REFCOUNT_MASK)
                    >> XPOST_MEMORY_TABLE_MARK_DATA_REFCOUNT_OFFSET,
                (tab->tab[i].mark
                    & XPOST_MEMORY_TABLE_MARK_DATA_LOWLEVEL_MASK)
                    >> XPOST_MEMORY_TABLE_MARK_DATA_LOWLEVEL_OFFSET,
                (tab->tab[i].mark
                    & XPOST_MEMORY_TABLE_MARK_DATA_TOPLEVEL_MASK)
                    >> XPOST_MEMORY_TABLE_MARK_DATA_TOPLEVEL_OFFSET);
        for (u = 0; u < tab->tab[i].sz; u++)
        {
            printf(" %02x%c",
                    mem->base[ tab->tab[i].adr + u ],
                    isprint(mem->base[ tab->tab[i].adr + u]) ?
                        mem->base[ tab->tab[i].adr + u ] :
                        ' ');
        }
        (void)puts("");
    }
    if (tab->nextent == XPOST_MEMORY_TABLE_SIZE)
    {
        mtabadr = tab->nexttab;
        goto next_table;
    }
}


