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
#include <stdio.h>

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

#include "xpost_dsc_ctx.h"


/*============================================================================*
 *                                  Local                                     *
 *============================================================================*/


/*============================================================================*
 *                                 Global                                     *
 *============================================================================*/


Xpost_Dsc_Ctx *
xpost_dsc_ctx_new_from_address(const unsigned char *base, size_t length)
{
    Xpost_Dsc_Ctx *ctx;

    if (!base || (length == 0))
        return NULL;

    ctx = (Xpost_Dsc_Ctx *)calloc(1, sizeof(Xpost_Dsc_Ctx));
    if (!ctx)
        return NULL;

    ctx->base = base;
    ctx->cur_loc = base;
    ctx->length = length;

    return ctx;
}

#ifdef _WIN32

Xpost_Dsc_Ctx *
xpost_dsc_ctx_new_from_file(const char *filename)
{
    BY_HANDLE_FILE_INFORMATION info;
    Xpost_Dsc_Ctx *ctx;
    const unsigned char *base;
    HANDLE h;
    HANDLE fm;
    size_t length;

    h = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ,
                   NULL, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY,
                   NULL);
    if (h == INVALID_HANDLE_VALUE)
    {
        printf("Createfile failed\n");
        return NULL;
    }

    if (!GetFileInformationByHandle(h, &info))
    {
        printf("GetFileAttributesEx failed\n");
        goto close_handle;
    }

    if (!(info.dwFileAttributes | FILE_ATTRIBUTE_NORMAL))
    {
        printf("file %s is not a normal file", filename);
        goto close_handle;
    }

    fm = CreateFileMapping(h, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!fm)
    {
        printf("CreateFileMapping failed\n");
        goto close_handle;
    }

    base = (const unsigned char *)MapViewOfFile(fm, FILE_MAP_READ, 0, 0, 0);
    CloseHandle(fm);

    if (!base)
    {
        printf("MapViewOfFile failed\n");
        goto close_handle;
    }
#ifdef _WIN64
    length = (((size_t)info.nFileSizeHigh) << 32) | (size_t)info.nFileSizeLow;
#else
    length = (size_t)info.nFileSizeLow;
#endif

    ctx = xpost_dsc_ctx_new_from_address(base, length);
    if (!ctx)
    {
        printf("creation of ctx failed\n");
        goto unmap_base;
    }

    ctx->h = h;
    ctx->from_file = 1;

    return ctx;

  unmap_base:
    UnmapViewOfFile(base);
  close_handle:
    CloseHandle(h);

    return NULL;
}

void
xpost_dsc_ctx_del(Xpost_Dsc_Ctx *ctx)
{
    if (!ctx)
        return;

    if (ctx->from_file)
    {
        UnmapViewOfFile(ctx->base);
        CloseHandle(ctx->h);
    }

    free(ctx);
}

#else

Xpost_Dsc_Ctx *
xpost_dsc_ctx_new_from_file(const char *filename)
{
    struct stat st;
    const unsigned char *base;
    Xpost_Dsc_Ctx *ctx;
    int fd;

    fd = open(filename, O_RDONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd == -1)
    {
        printf("Can not open file %s\n", filename);
        return NULL;
    }

    if (fstat(fd, &st) == -1)
    {
        printf("Can not retrieve stat from file %s\n", filename);
        goto close_fd;
    }

    if (!(S_ISREG(st.st_mode) || S_ISLNK(st.st_mode)))
    {
        printf("file %s is not a regular file nor a symbolic link\n", filename);
        goto close_fd;
    }

    base = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, map->fd, 0);
    if (!base)
    {
        printf("Can not map file %s into memory", filename);
        goto close_fd;
    }

    ctx = xpost_dsc_ctx_new_from_address(base, st.st_size);
    if (!ctx)
    {
        printf("creation of ctx failed\n");
        goto unmap_base;
    }

    ctx->fd = fd;
    ctx->from_file = 1;

    return ctx;

  unmap_base:
    munmap((void *)base, st.st_size);
  close_fd:
    close(fd);

    return NULL;
}

void
xpost_dsc_ctx_del(Xpost_Dsc_Ctx *ctx)
{
    if (!ctx)
        return;

    if (ctx->from_file)
    {
        munmap((void *)ctx->base, ctx->length);
        close(ctx->fd);
    }

    free(ctx);
}

#endif


/*============================================================================*
 *                                   API                                      *
 *============================================================================*/
