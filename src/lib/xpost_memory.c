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
#include <errno.h>
#include <stdlib.h> /* free malloc realloc */
#include <stdio.h> /* remove puts */
#include <string.h> /* memset strerror */

#include <sys/stat.h> /* open */
#include <fcntl.h> /* open */

#ifdef HAVE_UNISTD_H
# include <unistd.h> /* ftruncate close sysconf getpagesize */
#endif

#ifdef HAVE_SYS_MMAN_H
# include <sys/mman.h> /* mmap munmap mremap */
#endif

#ifdef _WIN32
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
# include <windows.h>
# undef WIN32_LEAN_AND_MEAN
# include <io.h>
# define read(f, p, s) _read(f, p, s)
# define lseek(f, p, fl) _lseek(f, p, fl)
# define close(f) _close(f)
#endif


#include "xpost_log.h"
#include "xpost_error.h"
#include "xpost_memory.h"
#include "xpost_object.h"


/* FIXME: use xpost_log instead */


unsigned int xpost_memory_page_size;

/*
   initialize the global extern page_size variable
 */
int
xpost_memory_init(void)
{
#ifdef _WIN32
    SYSTEM_INFO si;

    GetSystemInfo(&si);

    xpost_memory_page_size = (int)si.dwPageSize;
    return 1;
#elif defined HAVE_SYSCONF_PAGESIZE
    xpost_memory_page_size = (int)sysconf(_SC_PAGESIZE);
    return 1;
#elif defined HAVE_SYSCONF_PAGE_SIZE
    xpost_memory_page_size = (int)sysconf(_SC_PAGE_SIZE);
    return 1;
#elif defined HAVE_GETPAGESIZE
    xpost_memory_page_size = getpagesize();
    return 1;
#else
    XPOST_LOG_ERR("Could not find a way to retrieve the page size");
    return 0;
#endif
}

/*
   initialize the memory file structure,
   possibly using filename or file descriptor.
   install pointers to interpreter functions (so gc can discover contexts given only a memory file)
 */
int xpost_memory_file_init (Xpost_Memory_File *mem,
                            const char *fname,
                            int fd,
                            struct _Xpost_Context *(*xpost_interpreter_cid_get_context)(unsigned int cid),
                            int (*xpost_interpreter_get_initializing)(void),
                            void (*xpost_interpreter_set_initializing)(int))
{
    struct stat buf;
    size_t sz = xpost_memory_page_size;
#ifdef _WIN32
    HANDLE h;
    HANDLE fm;
#endif

    if (!mem)
    {
        XPOST_LOG_ERR("%d mem pointer is NULL", VMerror);
        return 0;
    }
    XPOST_LOG_INFO("init memory file%s%s",
                   fname ? " for " : "", fname ? fname : "");

    mem->interpreter_cid_get_context = xpost_interpreter_cid_get_context;
    mem->interpreter_get_initializing = xpost_interpreter_get_initializing;
    mem->interpreter_set_initializing = xpost_interpreter_set_initializing;

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
            if (sz < xpost_memory_page_size)
            {
                sz = xpost_memory_page_size;
#if defined (HAVE_MMAP) || defined (_WIN32)
                if (fd != -1)
                {
                    if (ftruncate(fd, sz) == -1)
                        XPOST_LOG_ERR("ftruncate(%d, %d) returned -1 (error: %s)",
                                      fd, sz, strerror(errno));
                }
#endif
            }
        }
    }


#ifdef _WIN32
    if (fd == -1)
        h = INVALID_HANDLE_VALUE;
    else
    {
        h = (HANDLE)_get_osfhandle(fd);
        if (h == INVALID_HANDLE_VALUE)
        {
            XPOST_LOG_ERR("Invalid handle");
            close(fd);
            return 0;
        }
    }

    fm = CreateFileMapping(h, NULL, PAGE_READWRITE, sz >> 32, sz & 0xffffffff, NULL);
    if (!fm)
    {
        XPOST_LOG_ERR("CreateFileMapping failed (%ld)", GetLastError());
        if (fd != -1) close(fd);
        return 0;
    }

    mem->base = (unsigned char *)MapViewOfFile(fm, FILE_MAP_ALL_ACCESS, 0, 0, sz);
    CloseHandle(fm);
    if (!mem->base)
    {
#elif defined (HAVE_MMAP)
    mem->base = (unsigned char *)mmap(NULL,
                                      sz,
                                      PROT_READ | PROT_WRITE,
                                      (fd == -1 ? MAP_PRIVATE   : MAP_SHARED) |
                                      (fd == -1 ? MAP_ANONYMOUS : 0),
                                      fd, 0);
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
    {
        if (read(fd, mem->base, sz) == -1)
            XPOST_LOG_ERR("%d failed to read memory file (error: %s)",
                          VMerror, strerror(errno));
    }
#endif
    if (fd == -1)
        memset(mem->base, 0, mem->max);

    return 1;
}

/*
   Close, deallocate, and destroy memory file structure
 */
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
    XPOST_LOG_INFO("exit memory file %s", mem->fname);

#ifdef _WIN32
    UnmapViewOfFile(mem->base);
#elif defined (HAVE_MMAP)
    munmap((void *)mem->base, mem->max);
#else
    if (mem->fd != -1)
    {
        (void) lseek(mem->fd, 0, SEEK_SET);
        if (write(mem->fd, mem->base, mem->used) == -1)
            XPOST_LOG_ERR("%d unable to write memory file (error: %s)",
                          VMerror, strerror(errno));
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

/* grow memory file by sz bytes, rounded up to the nearest system page size.
 */
int xpost_memory_file_grow (Xpost_Memory_File *mem,
                            unsigned int sz)
{
#ifdef _WIN32
    HANDLE h;
    HANDLE fm;
#endif
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

    if (sz < xpost_memory_page_size)
        sz = xpost_memory_page_size;
    else
        sz = (sz / xpost_memory_page_size + 1) * xpost_memory_page_size;
    sz += mem->max;

    XPOST_LOG_INFO("grow memory file%s%s (old: %d  new: %d)",
                   mem->fname ? " for " : "", mem->fname ? mem->fname : "",
                   mem->max, sz);

#ifdef _WIN32
    if (mem->fd != -1)
    {
        if (ftruncate(mem->fd, sz) == -1)
        {
            XPOST_LOG_ERR("ftruncate(%d, %d) returned -1", mem->fd, sz);
            XPOST_LOG_ERR("strerror: %s", strerror(errno));
        }
    }

    if (mem->fd == -1)
        h = INVALID_HANDLE_VALUE;
    else
    {
        h = (HANDLE)_get_osfhandle(mem->fd);
        if (h == INVALID_HANDLE_VALUE)
        {
            XPOST_LOG_ERR("Invalid handle");
            close(mem->fd);
            return 0;
        }
    }

    fm = CreateFileMapping(h, NULL, PAGE_READWRITE, (size_t)sz >> 32, sz & 0xffffffff, NULL);
    if (!fm)
    {
        XPOST_LOG_ERR("CreateFileMapping failed (%ld)", GetLastError());
        if (mem->fd != -1) close(mem->fd);
        return 0;
    }

    tmp = MapViewOfFile(fm, FILE_MAP_ALL_ACCESS, 0, 0, sz);
    CloseHandle(fm);
    if (tmp)
    {
        memcpy(tmp, mem->base, mem->used);
        UnmapViewOfFile(mem->base);
    }
    else
    {
#elif defined (HAVE_MMAP)
    if (mem->fd != -1)
    {
        if (ftruncate(mem->fd, sz) == -1)
            XPOST_LOG_ERR("ftruncate(%d, %d) returned -1 (error: %s)",
                          mem->fd, sz, strerror(errno));
    }
# ifdef HAVE_MREMAP
    tmp = mremap(mem->base, mem->max, sz, MREMAP_MAYMOVE);
# else
    if (mem->fd != -1)
    {
        msync((void *)mem->base, mem->used, MS_SYNC);
        munmap((void *)mem->base, mem->max);
        lseek(mem->fd, 0, SEEK_SET);
        if (ftruncate(mem->fd, sz) == -1)
            XPOST_LOG_ERR("ftruncate(%d, %d) returned -1 (error: %s)",
                          mem->fd, sz, strerror(errno));

        tmp = mmap(NULL, sz,
                   PROT_READ | PROT_WRITE,
                   MAP_SHARED,
                   mem->fd, 0);
    }
    else
    {
        tmp = mmap(NULL, sz,
                   PROT_READ | PROT_WRITE,
                   MAP_ANONYMOUS | MAP_PRIVATE,
                   mem->fd, 0);
        if (tmp != MAP_FAILED)
        {
            memcpy(tmp, mem->base, mem->used);
        }
    }
# endif
    if (tmp == MAP_FAILED)
    {
#else
    /* initialize mem (valgrind) */
    memset(mem->base + mem->used, 0, mem->max - mem->used);
    tmp = realloc(mem->base, sz);
    if (tmp == NULL)
    {
#endif
        XPOST_LOG_ERR("%d unable to grow memory", VMerror);
        ret = 0;
    /* FIXME: this should really be there ? */
#ifdef HAVE_MMAP
# ifndef HAVE_MREMAP
        tmp = mmap(NULL, mem->max,
                   PROT_READ | PROT_WRITE,
                   mem->fd == -1 ? MAP_ANONYMOUS | MAP_PRIVATE : MAP_SHARED,
                   mem->fd, 0);
# else
        munmap(mem->base, mem->max);
# endif
#endif
    }
    mem->base = (unsigned char *)tmp;
    mem->max = sz;

    return ret;
}


/*
   allocate data linearly from the memory file
   */
int xpost_memory_file_alloc (Xpost_Memory_File *mem,
                             unsigned int sz,
                             unsigned int *retaddr)
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

    *retaddr = adr;
    /*
    XPOST_LOG_INFO("allocated %u bytes at %u in %s", sz, adr, mem->fname);
      */
    return 1;
}


void xpost_memory_file_dump (const Xpost_Memory_File *mem)
{
    int u,v;

    if (!mem)
    {
        XPOST_LOG_ERR("%d mem pointer is NULL", VMerror);
        return;
    }

    XPOST_LOG_DUMP("{mfile: base = %p, "
            "used = 0x%x (%u), "
            "max = 0x%x (%u), "
            "start = %d}\n",
            mem->base,
            mem->used, mem->used,
            mem->max, mem->max,
            mem->start);

    return;

    for (u = 0; u < (int)mem->used; u++)
    {
        if (u%16 == 0)
        {
            if (u != 0)
            {
                for (v = u - 16; v < u; v++)
                {
                    XPOST_LOG_DUMP("%c",
                        isprint(mem->base[v]) ?
                        mem->base[v] : '.');
                }
            }
            XPOST_LOG_DUMP("\n%06u %04x: ", u, u);
        }
        XPOST_LOG_DUMP("%02x ", mem->base[u]);
    }

    if ((u-1)%16 != 0)
    { /* did not print in the last iteration of the loop */
        for (v = u; u%16 != 0; v++)
        {
            XPOST_LOG_DUMP("   ");
        }
        for (v = u - (u % 16); v < u; v++)
        {
            XPOST_LOG_DUMP("%c",
                    isprint(mem->base[v]) ?
                    mem->base[v] : '.');
        }
    }

    XPOST_LOG_DUMP("\n");
}


/*
   allocate and initialize a memory table data structure
   */
int xpost_memory_table_init (Xpost_Memory_File *mem,
                             unsigned int *retaddr)
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

    *retaddr = adr;
    return 1;
}


/* install free-list function into memory file */
int xpost_memory_register_free_list_alloc_function(Xpost_Memory_File *mem,
    int (*free_list_alloc)(struct Xpost_Memory_File *mem, unsigned sz, unsigned tag, unsigned int *entity))
{
    mem->free_list_alloc = free_list_alloc;
    mem->free_list_alloc_is_installed = 1;
    return 1;
}

/* install garbage-collect function into memory file */
int xpost_memory_register_garbage_collect_function(Xpost_Memory_File *mem,
    int (*garbage_collect)(struct Xpost_Memory_File *mem, int dosweep, int markall))
{
    mem->garbage_collect = garbage_collect;
    mem->garbage_collect_is_installed = 1;
    return 1;
}

/*
   allocate sz bytes as an 'ent' in the memory table
   */
static
int _xpost_memory_table_alloc_new (Xpost_Memory_File *mem,
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
    if (ent > XPOST_OBJECT_COMP_MAX_ENT)
    {
        XPOST_LOG_ERR("Warning: ent number %u exceed object storage max %u",
                ent, XPOST_OBJECT_COMP_MAX_ENT);
    }

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

/*
   allocate sz bytes in the memory table, using free-list if installed,
   possibly calling garbage collector, if installed
   */
int xpost_memory_table_alloc (Xpost_Memory_File *mem,
                              unsigned int sz,
                              unsigned int tag,
                              unsigned int *entity)
{
    int ret;

    if (mem->free_list_alloc_is_installed)
    {
        ret = mem->free_list_alloc(mem, sz, tag, entity);
        if (ret == 1)
        {
            return 1;
        }
        else if (ret == 2)
        {
            if (mem->garbage_collect_is_installed)
            {
                int sz_reclaimed;

                sz_reclaimed = mem->garbage_collect(mem, 1, 0);
                if (sz_reclaimed == -1)
                    return 0;
                if (sz_reclaimed > (int)sz)
                {
                    ret = mem->free_list_alloc(mem, sz, tag, entity);
                    if (ret == 1)
                    {
                        return 1;
                    }
                }
            }
        }
    }
    ret = _xpost_memory_table_alloc_new(mem, sz, tag, entity);
    XPOST_LOG_INFO("allocated %u bytes with tag %u as ent %u in %s",
            sz, tag, *entity, mem->fname);
    return ret;
}


/*
   find the appropriate memory table segment for the given ent,
   and replace argument ent with a "relative ent" that indexes the table segment
   */
int xpost_memory_table_find_relative (Xpost_Memory_File *mem,
                                      Xpost_Memory_Table **atab,
                                      unsigned int *aent)
{
    unsigned int ent = *aent;
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
            XPOST_LOG_ERR("%d cannot find table segment for ent %u", VMerror, ent);
            return 0;
        }
        *atab = (Xpost_Memory_Table *)(mem->base + (*atab)->nexttab);
    }
    return 1;
}


/* get the address of an allocation from the memory table */
int xpost_memory_table_get_addr (Xpost_Memory_File *mem,
                                 unsigned int ent,
                                 unsigned int *retaddr)
{
    Xpost_Memory_Table *tab;
    if (!xpost_memory_table_find_relative(mem, &tab, &ent))
    {
        XPOST_LOG_ERR("%d entity not found %u", VMerror, ent);
        return 0;
    }
    *retaddr = tab->tab[ent].adr;
    return 1;
}

/* change the address of an allocation in the memory table */
int xpost_memory_table_set_addr (Xpost_Memory_File *mem,
                                 unsigned int ent,
                                 unsigned int setaddr)
{
    Xpost_Memory_Table *tab;
    if (!xpost_memory_table_find_relative(mem, &tab, &ent))
    {
        XPOST_LOG_ERR("%d entity not found %u", VMerror, ent);
        return 0;
    }
    tab->tab[ent].adr = setaddr;
    return 1;
}


/* get the size of an allocation from the memory table */
int xpost_memory_table_get_size (Xpost_Memory_File *mem,
                                 unsigned int ent,
                                 unsigned int *sz)
{
    Xpost_Memory_Table *tab;
    if (!xpost_memory_table_find_relative(mem, &tab, &ent))
    {
        XPOST_LOG_ERR("%d entity not found %u", VMerror, ent);
        return 0;
    }
    *sz = tab->tab[ent].sz;
    return 1;
}

/* set the size of an allocation in the memory table */
int xpost_memory_table_set_size (Xpost_Memory_File *mem,
                                 unsigned int ent,
                                 unsigned int size)
{
    Xpost_Memory_Table *tab;
    if (!xpost_memory_table_find_relative(mem, &tab, &ent))
    {
        XPOST_LOG_ERR("%d entity not found %u", VMerror, ent);
        return 0;
    }
    tab->tab[ent].sz = size;
    return 1;
}

/* get the mark field of an allocation from the memory table */
int xpost_memory_table_get_mark (Xpost_Memory_File *mem,
                                 unsigned int ent,
                                 unsigned int *retmark)
{
    Xpost_Memory_Table *tab;
    if (!xpost_memory_table_find_relative(mem, &tab, &ent))
    {
        XPOST_LOG_ERR("%d entity not found %u", VMerror, ent);
        return 0;
    }
    *retmark = tab->tab[ent].mark;
    return 1;
}


/* change the mark field of an allocation in the memory table */
int xpost_memory_table_set_mark (Xpost_Memory_File *mem,
                                 unsigned int ent,
                                 unsigned int setmark)
{
    Xpost_Memory_Table *tab;
    if (!xpost_memory_table_find_relative(mem, &tab, &ent))
    {
        XPOST_LOG_ERR("%d entity not found %u", VMerror, ent);
        return 0;
    }
    tab->tab[ent].mark = setmark;
    return 1;
}


/* get the tag field of an allocation from the memory table */
int xpost_memory_table_get_tag (Xpost_Memory_File *mem,
                                unsigned int ent,
                                unsigned int *tag)
{
    Xpost_Memory_Table *tab;
    if (!xpost_memory_table_find_relative(mem, &tab, &ent))
    {
        XPOST_LOG_ERR("%d entity not found %u", VMerror, ent);
        return 0;
    }
    *tag = tab->tab[ent].tag;
    return 1;
}

/* change the tag field of an allocation in the memory table */
int xpost_memory_table_set_tag (Xpost_Memory_File *mem,
                                unsigned int ent,
                                unsigned int tag)
{
    Xpost_Memory_Table *tab;
    if (!xpost_memory_table_find_relative(mem, &tab, &ent))
    {
        XPOST_LOG_ERR("%d entity not found %u", VMerror, ent);
        return 0;
    }
    tab->tab[ent].tag = tag;
    return 1;
}


/* get sz bytes at offset*sz from a memory allocation */
int xpost_memory_get (Xpost_Memory_File *mem,
                      unsigned int ent,
                      unsigned int offset,
                      unsigned int sz,
                      void *dest)
{
    Xpost_Memory_Table *tab;
    if (!xpost_memory_table_find_relative(mem, &tab, &ent))
    {
        XPOST_LOG_ERR("%d entity not found %u", VMerror, ent);
        return 0;
    }

    if (offset * sz > tab->tab[ent].sz)
    {
        XPOST_LOG_ERR("%d out of bounds memory %u * %u > %u", rangecheck,
                offset, sz, tab->tab[ent].sz);
        return 0;
    }

    memcpy(dest, mem->base + tab->tab[ent].adr + offset * sz, sz);
    return 1;
}

/* put sz bytes at offset*sz in a memory allocation */
int xpost_memory_put (Xpost_Memory_File *mem,
                      unsigned int ent,
                      unsigned int offset,
                      unsigned int sz,
                      const void *src)
{
    Xpost_Memory_Table *tab;
    if (!xpost_memory_table_find_relative(mem, &tab, &ent))
    {
        XPOST_LOG_ERR("%d entity not found %u", VMerror, ent);
        return 0;
    }

    if (offset * sz > tab->tab[ent].sz)
    {
        XPOST_LOG_ERR("%d out of bounds memory %u * %u > %u", rangecheck,
                offset, sz, tab->tab[ent].sz);
        return 0;
    }

    memcpy(mem->base + tab->tab[ent].adr + offset * sz, src, sz);
    return 1;
}


void xpost_memory_table_dump_ent (Xpost_Memory_File *mem,
                                  unsigned int ent)
{
    Xpost_Memory_Table *tab;
    unsigned int u;
    unsigned int i = ent;
    unsigned int e = ent;
    if (!xpost_memory_table_find_relative(mem, &tab, &i))
    {
        XPOST_LOG_ERR("%d entity not found %u", VMerror, e);
        return;
    }
    XPOST_LOG_DUMP("ent %d (%d): "
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
            XPOST_LOG_DUMP(" %02x%c",
                    mem->base[ tab->tab[i].adr + u ],
                    isprint(mem->base[ tab->tab[i].adr + u]) ?
                        mem->base[ tab->tab[i].adr + u ] :
                        ' ');
        }
}

void xpost_memory_table_dump (const Xpost_Memory_File *mem)
{
    unsigned int i;
    unsigned int e = 0;
    unsigned int mtabadr = 0;
    Xpost_Memory_Table *tab;

next_table:
    tab = (Xpost_Memory_Table *)(mem->base + mtabadr);
    XPOST_LOG_DUMP("nexttab: 0x%04x\n", tab->nexttab);
    XPOST_LOG_DUMP("nextent: %u\n", tab->nextent);
    for (i = 0; i < tab->nextent; i++, e++)
    {
        unsigned int u;
        XPOST_LOG_DUMP("ent %d (%d): "
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
            XPOST_LOG_DUMP(" %02x%c",
                    mem->base[ tab->tab[i].adr + u ],
                    isprint(mem->base[ tab->tab[i].adr + u]) ?
                        mem->base[ tab->tab[i].adr + u ] :
                        ' ');
        }
        XPOST_LOG_DUMP("\n");
    }
    if (tab->nextent == XPOST_MEMORY_TABLE_SIZE)
    {
        mtabadr = tab->nexttab;
        goto next_table;
    }
}
