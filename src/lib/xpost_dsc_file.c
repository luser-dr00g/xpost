/*
 * Xpost DSC - a DSC Postscript parser
 * Copyright (C) 2013-2016, Michael Joshua Ryan
 * Copyright (C) 2013-2016, Vincent Torri
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

#include <stdlib.h>

#ifdef _WIN32
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
# include <windows.h>
# undef WIN32_LEAN_AND_MEAN
#else
# include <sys/types.h>
# include <sys/stat.h>
# include <sys/mman.h>
# include <unistd.h>
# include <fcntl.h>
#endif

#include "xpost.h"
#include "xpost_log.h"
#include "xpost_compat.h"

#include "xpost_dsc.h"


/*============================================================================*
 *                                  Local                                     *
 *============================================================================*/


struct Xpost_Dsc_File
{
#ifdef _WIN32
    HANDLE h;
#else
    int fd;
#endif
    const unsigned char *base;
    size_t length;
    unsigned int from_file : 1;
};


/*============================================================================*
 *                                 Global                                     *
 *============================================================================*/


/*============================================================================*
 *                                   API                                      *
 *============================================================================*/


XPAPI Xpost_Dsc_File *
xpost_dsc_file_new_from_address(const unsigned char *base, size_t length)
{
    Xpost_Dsc_File *file;

    if (!base || (length == 0))
        return NULL;

    file = (Xpost_Dsc_File *)calloc(1, sizeof(Xpost_Dsc_File));
    if (!file)
    {
        XPOST_LOG_ERR("Can not allocate memory for File object");
        return NULL;
    }

    file->base = base;
    file->length = length;

    return file;
}

#ifdef _WIN32

XPAPI Xpost_Dsc_File *
xpost_dsc_file_new_from_file(const char *filename)
{
    BY_HANDLE_FILE_INFORMATION info;
    Xpost_Dsc_File *file;
    const unsigned char *base;
    HANDLE h;
    HANDLE fm;
    size_t length;

    h = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ,
                   NULL, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY,
                   NULL);
    if (h == INVALID_HANDLE_VALUE)
    {
        XPOST_LOG_ERR("Createfile failed");
        return NULL;
    }

    if (!GetFileInformationByHandle(h, &info))
    {
        XPOST_LOG_ERR("GetFileAttributesEx failed");
        goto close_handle;
    }

    if (!(info.dwFileAttributes | FILE_ATTRIBUTE_NORMAL))
    {
        XPOST_LOG_ERR("file %s is not a normal file", filename);
        goto close_handle;
    }

    fm = CreateFileMapping(h, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!fm)
    {
        XPOST_LOG_ERR("CreateFileMapping failed");
        goto close_handle;
    }

    base = (const unsigned char *)MapViewOfFile(fm, FILE_MAP_READ, 0, 0, 0);
    CloseHandle(fm);

    if (!base)
    {
        XPOST_LOG_ERR("MapViewOfFile failed");
        goto close_handle;
    }
#ifdef _WIN64
    length = (((size_t)info.nFileSizeHigh) << 32) | (size_t)info.nFileSizeLow;
#else
    length = (size_t)info.nFileSizeLow;
#endif

    file = xpost_dsc_file_new_from_address(base, length);
    if (!file)
        goto unmap_base;

    file->h = h;
    file->from_file = 1;

    return file;

  unmap_base:
    UnmapViewOfFile(base);
  close_handle:
    CloseHandle(h);

    return NULL;
}

XPAPI void
xpost_dsc_file_del(Xpost_Dsc_File *file)
{
    if (!file)
        return;

    if (file->from_file)
    {
        UnmapViewOfFile(file->base);
        CloseHandle(file->h);
    }

    free(file);
}

#else

XPAPI Xpost_Dsc_File *
xpost_dsc_file_new_from_file(const char *filename)
{
    struct stat st;
    const unsigned char *base;
    Xpost_Dsc_File *file;
    int fd;

    fd = open(filename, O_RDONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd == -1)
    {
        XPOST_LOG_ERR("Can not open file %s", filename);
        return NULL;
    }

    if (fstat(fd, &st) == -1)
    {
        XPOST_LOG_ERR("Can not retrieve stat from file %s", filename);
        goto close_fd;
    }

    if (!(S_ISREG(st.st_mode) || S_ISLNK(st.st_mode)))
    {
        XPOST_LOG_ERR("file %s is not a regular file nor a symbolic link", filename);
        goto close_fd;
    }

    base = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (!base)
    {
        XPOST_LOG_ERR("Can not map file %s into memory", filename);
        goto close_fd;
    }

    file = xpost_dsc_file_new_from_address(base, st.st_size);
    if (!file)
        goto unmap_base;

    file->fd = fd;
    file->from_file = 1;

    return file;

  unmap_base:
    munmap((void *)base, st.st_size);
  close_fd:
    close(fd);

    return NULL;
}

XPAPI void
xpost_dsc_file_del(Xpost_Dsc_File *file)
{
    if (!file)
        return;

    if (file->from_file)
    {
        munmap((void *)file->base, file->length);
        close(file->fd);
    }

    free(file);
}

#endif

XPAPI const unsigned char *
xpost_dsc_file_base_get(const Xpost_Dsc_File *file)
{
    return (file) ? file->base : NULL;
}

XPAPI const size_t
xpost_dsc_file_length_get(const Xpost_Dsc_File *file)
{
    return (file) ? file->length : 0;
}
