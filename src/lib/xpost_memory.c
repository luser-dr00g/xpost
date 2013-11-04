/*
 * Xpost - a Level-2 Postscript interpreter
 * Copyright (C) 2013, Michael Joshua Ryan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of the Xpost software product nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <ctype.h> /* isprint */
#include <stdlib.h> /* free malloc realloc */
#include <stdio.h> /* fprintf printf putchar puts */
#include <string.h> /* memset */

#include <sys/stat.h> /* open */
#include <fcntl.h> /* open */

#ifdef HAVE_UNISTD_H
# include <unistd.h> /* ftruncate close */
#endif

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

/* FIXME: use autotools to check if getpagesize exists ? */
unsigned int xpost_memory_pagesize /*= getpagesize()*/ = 4096;


int xpost_memory_file_init (
        Xpost_Memory_File *mem,
        const char *fname,
        int fd)
{
    struct stat buf;
    size_t sz = xpost_memory_pagesize;

    if (!mem)
    {
        XPOST_LOG_ERR("%d mem pointer is NULL", VMerror);
        return 0;
    }
    XPOST_LOG_INFO("init memory file%s%s",
            fname ? " for " : "", fname ? fname : "");

    if(fname)
    {
        strncpy(mem->fname, fname, sizeof(mem->fname));
        mem->fname[sizeof(mem->fname) - 1] = '\0';
    }
    else
        mem->fname[0] = '\0';

    mem->fd = fd;
    if (fd != -1)
    {
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
    { /* . */
#else
    mem->base = malloc(sz);
    if (mem->base == NULL)
    { /* .. */
#endif
        XPOST_LOG_ERR("%d failed to allocate memory-file data", VMerror);
        return 0;
    } /* . .. */
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
    if (!mem)
    {
        XPOST_LOG_ERR("%d mem pointer is NULL", VMerror);
        return 0;
    }

    if (mem->base == NULL)
    {
        XPOST_LOG_ERR("%d mem->base is NULL, mem not initialized ?", VMerror);
        return 0;
    }
    XPOST_LOG_INFO("exit memory file");

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
    int ret = 1;

    if (!mem)
    {
        XPOST_LOG_ERR("%d mem pointer is NULL", VMerror);
        return 0;
    }

    if (mem->base == NULL)
    {
        XPOST_LOG_ERR("%d mem->base is NULL", VMerror);
        return 0;
    }
    XPOST_LOG_INFO("grow memory file.");

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
        XPOST_LOG_ERR("%d unable to grow memory", VMerror);
        ret = 0;
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

    return ret;
}


int xpost_memory_file_alloc (
        Xpost_Memory_File *mem,
        unsigned int sz,
        unsigned int *addr)
{
    unsigned int adr;

    if (!mem)
    {
        XPOST_LOG_ERR("%d mem pointer is NULL", VMerror);
        return 0;
    }

    if (mem->base == NULL)
    {
        XPOST_LOG_ERR("%d mem->base is NULL, mem not initialized ?", VMerror);
        return 0;
    }

    adr = mem->used;

    if (sz)
    {
        if (sz + mem->used >= mem->max)
        {
            if (!xpost_memory_file_grow(mem, sz))
            {
                XPOST_LOG_ERR("%d unable to allocate memory", VMerror);
                return 0;
            }
        }

        mem->used += sz;
        memset(mem->base + adr, 0, sz);
    }

    *addr = adr;
    return 1;
}


void xpost_memory_file_dump (const Xpost_Memory_File *mem)
{
    unsigned int u,v;

    if (!mem)
    {
        XPOST_LOG_ERR("%d mem pointer is NULL", VMerror);
        return;
    }

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
    { /* did not print in the last iteration of the loop */
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


int xpost_memory_table_init (
        Xpost_Memory_File *mem,
        unsigned int *addr)
{
    Xpost_Memory_Table *tab;
    unsigned int adr;

    if (!mem)
    {
        XPOST_LOG_ERR("%d mem pointer is NULL", VMerror);
        return 0;
    }

    if (!xpost_memory_file_alloc(mem, sizeof(Xpost_Memory_Table), &adr))
    {
        XPOST_LOG_ERR("%d unable to initialize table", VMerror);
        return 0;
    }

    tab = (Xpost_Memory_Table *)(mem->base + adr);
    tab->nexttab = 0;
    tab->nextent = 0;

    *addr = adr;
    return 1;
}


int xpost_memory_table_alloc (Xpost_Memory_File *mem,
        unsigned int sz,
        unsigned int tag,
        unsigned int *entity)
{
    unsigned int mtabadr = 0;
    unsigned int ent;
    unsigned int adr;
    Xpost_Memory_Table *tab;
    int ntab = 0;

    if (!mem)
    {
        XPOST_LOG_ERR("%d mem pointer is NULL", VMerror);
        return 0;
    }

    tab = (Xpost_Memory_Table *)(mem->base + mtabadr);

    while (tab->nextent >= XPOST_MEMORY_TABLE_SIZE)
    {
        tab = (Xpost_Memory_Table *)(mem->base + tab->nexttab);
        ++ntab;
    }

    ent = tab->nextent;
    ++tab->nextent;

    if (!xpost_memory_file_alloc(mem, sz, &adr))
    {
        XPOST_LOG_ERR("%d unable to allocate entity", VMerror);
        return 0;
    }

    ent += ntab * XPOST_MEMORY_TABLE_SIZE; /* recalc */
    xpost_memory_table_find_relative(mem, &tab, &ent); /* recalc */
    tab->tab[ent].adr = adr;
    tab->tab[ent].sz = sz;
    tab->tab[ent].tag = tag;

    if (tab->nextent == XPOST_MEMORY_TABLE_SIZE)
    {
        unsigned int newtab;
        if (!xpost_memory_table_init(mem, &newtab))
        {
            XPOST_LOG_ERR("%d unable to extend table chain", VMerror);
        }
        ent += ntab * XPOST_MEMORY_TABLE_SIZE; /* recalc */
        xpost_memory_table_find_relative(mem, &tab, &ent); /* recalc */
        tab->nexttab = newtab;
    }

    *entity = ent + ntab * XPOST_MEMORY_TABLE_SIZE;
    return 1;
}


int xpost_memory_table_find_relative (
        Xpost_Memory_File *mem,
        Xpost_Memory_Table **atab,
        unsigned int *aent)
{
    if (!mem)
    {
        XPOST_LOG_ERR("%d mem pointer is NULL", VMerror);
        return 0;
    }

    if (mem->base == NULL)
    {
        XPOST_LOG_ERR("%d mem->base is NULL", VMerror);
        return 0;
    }

    *atab = (Xpost_Memory_Table *)(mem->base);
    while (*aent >= XPOST_MEMORY_TABLE_SIZE)
    {
        *aent -= XPOST_MEMORY_TABLE_SIZE;
        if ((*atab)->nexttab == 0)
        {
            XPOST_LOG_ERR("%d cannot find table segment for ent", VMerror);
            return 0;
        }
        *atab = (Xpost_Memory_Table *)(mem->base + (*atab)->nexttab);
    }
    return 1;
}


int xpost_memory_table_get_addr (
        Xpost_Memory_File *mem,
        unsigned int ent,
        unsigned int *addr)
{
    Xpost_Memory_Table *tab;
    if (!xpost_memory_table_find_relative(mem, &tab, &ent))
    {
        XPOST_LOG_ERR("%d entity not found", VMerror);
        return 0;
    }
    *addr = tab->tab[ent].adr;
    return 1;
}


int xpost_memory_table_set_addr (
        Xpost_Memory_File *mem,
        unsigned int ent,
        unsigned int addr)
{
    Xpost_Memory_Table *tab;
    if (!xpost_memory_table_find_relative(mem, &tab, &ent))
    {
        XPOST_LOG_ERR("%d entity not found", VMerror);
        return 0;
    }
    tab->tab[ent].adr = addr;
    return 1;
}


int xpost_memory_table_get_size (
        Xpost_Memory_File *mem,
        unsigned int ent,
        unsigned int *sz)
{
    Xpost_Memory_Table *tab;
    if (!xpost_memory_table_find_relative(mem, &tab, &ent))
    {
        XPOST_LOG_ERR("%d entity not found", VMerror);
        return 0;
    }
    *sz = tab->tab[ent].sz;
    return 1;
}


int xpost_memory_table_set_size (
        Xpost_Memory_File *mem,
        unsigned int ent,
        unsigned int size)
{
    Xpost_Memory_Table *tab;
    if (!xpost_memory_table_find_relative(mem, &tab, &ent))
    {
        XPOST_LOG_ERR("%d entity not found", VMerror);
        return 0;
    }
    tab->tab[ent].sz = size;
    return 1;
}


int xpost_memory_table_get_mark (
        Xpost_Memory_File *mem,
        unsigned int ent,
        unsigned int *mark)
{
    Xpost_Memory_Table *tab;
    if (!xpost_memory_table_find_relative(mem, &tab, &ent))
    {
        XPOST_LOG_ERR("%d entity not found", VMerror);
        return 0;
    }
    *mark = tab->tab[ent].mark;
    return 1;
}


int xpost_memory_table_set_mark (
        Xpost_Memory_File *mem,
        unsigned int ent,
        unsigned int mark)
{
    Xpost_Memory_Table *tab;
    if (!xpost_memory_table_find_relative(mem, &tab, &ent))
    {
        XPOST_LOG_ERR("%d entity not found", VMerror);
        return 0;
    }
    tab->tab[ent].mark = mark;
    return 1;
}


int xpost_memory_table_get_tag (
        Xpost_Memory_File *mem,
        unsigned int ent,
        unsigned int *tag)
{
    Xpost_Memory_Table *tab;
    if (!xpost_memory_table_find_relative(mem, &tab, &ent))
    {
        XPOST_LOG_ERR("%d entity not found", VMerror);
        return 0;
    }
    *tag = tab->tab[ent].tag;
    return 1;
}


int xpost_memory_table_set_tag (
        Xpost_Memory_File *mem,
        unsigned int ent,
        unsigned int tag)
{
    Xpost_Memory_Table *tab;
    if (!xpost_memory_table_find_relative(mem, &tab, &ent))
    {
        XPOST_LOG_ERR("%d entity not found", VMerror);
        return 0;
    }
    tab->tab[ent].tag = tag;
    return 1;
}


int xpost_memory_get (
        Xpost_Memory_File *mem,
        unsigned int ent,
        unsigned int offset,
        unsigned int sz,
        void *dest)
{
    Xpost_Memory_Table *tab;
    if (!xpost_memory_table_find_relative(mem, &tab, &ent))
    {
        XPOST_LOG_ERR("%d entity not found", VMerror);
        return 0;
    }

    if (offset * sz > tab->tab[ent].sz)
    {
        XPOST_LOG_ERR("%d out of bounds memory", rangecheck);
        return 0;
    }

    memcpy(dest, mem->base + tab->tab[ent].adr + offset * sz, sz);
    return 1;
}

int xpost_memory_put (
        Xpost_Memory_File *mem,
        unsigned int ent,
        unsigned int offset,
        unsigned int sz,
        const void *src)
{
    Xpost_Memory_Table *tab;
    if (!xpost_memory_table_find_relative(mem, &tab, &ent))
    {
        XPOST_LOG_ERR("%d entity not found", VMerror);
        return 0;
    }

    if (offset * sz > tab->tab[ent].sz)
    {
        XPOST_LOG_ERR("%d out of bounds memory", rangecheck);
        return 0;
    }

    memcpy(mem->base + tab->tab[ent].adr + offset * sz, src, sz);
    return 1;
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
