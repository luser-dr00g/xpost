/*
 * Xpost - a Level-2 Postscript interpreter
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

/**
 * @file xpost_compat.h
 * @brief This file provides the Xpost compatibility functions.
 *
 * This file is mostly for emulating unixisms on windows.
 *
 * @defgroup xpost_library Library functions
 *
 * @{
 */

#ifndef XPOST_COMPAT_H
#define XPOST_COMPAT_H

#include <stdio.h> /* FILE */

#ifdef _MSC_VER

# include <float.h>

# define ssize_t SSIZE_T

# define close(fd) _close(fd)
# define fdopen(fd, mode) _fdopen(fd, mode)
# define fileno(st) _fileno(st)
# define ftruncate(fd, sz) _chsize(fd, sz)
# define getcwd(buf, len) _getcwd(buf, len)
# ifndef isnan
#  define isnan(x) _isnan(x)
# endif
# ifndef isinf
#  define isinf(x) (!_finite(x))
# endif
# define putenv(s) _putenv(s)
# define snprintf _snprintf
# define strdup(s) _strdup(s)
# define trunc(x) ((x) > 0) ? floor(x) : ceil(x)
# ifndef va_copy
#  define va_copy(dst, src) ((dst) = (src))
# endif

#endif /* _MSC_VER */

#ifdef _WIN32
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
# include <windows.h> /* MAX_PATH */
# undef WIN32_LEAN_AND_MEAN
# define XPOST_PATH_MAX MAX_PATH
#else
# include <limits.h>
# define XPOST_PATH_MAX PATH_MAX
#endif

/**
 * @brief initialize the compatibility layer
 */
int xpost_compat_init(void);

/**
 * @brief quit the compatibility layer
 */
void xpost_compat_quit(void);

/**
 * @brief control the ECHO parameter of the terminal or console associated with file.
 */
void echoon(FILE *f);
void echooff(FILE *f);

/**
 * @brief discard any pending buffered I/O on the given stream.
 *
 * @param[in] f The stream to purge.
 *
 * @note On Windows, this function does nothing.
 */
void xpost_fpurge(FILE *f);

/**
 * @brief return the number of milliseconds, whatever the stating time is.
 *
 * @return The number of milliseconds.
 *
 * @note The returned value is platform dependant.
 */
long long xpost_get_realtime_ms(void);

/**
 * @brief return the number of milliseconds since the Postscript
 * interpreter has started.
 *
 * @return The number of milliseconds.
 */
long long xpost_get_usertime_ms(void);

int xpost_isatty(int fd);

/**
 * @brief open a temporary file using @p template to generate the name.
 *
 * @param[inout] template The template.
 * @param[out] fd A buffer to get the new file descriptor.
 * @returns 1 on success, 0 otherwise.
 *
 * The @p template parameter must finish with "XXXXXX" and is modified.
 */
int xpost_mkstemp(char *template, int *fd);

#ifdef _WIN32

typedef struct
{
    int gl_pathc;
    char **gl_pathv;
    int gl_offs;
} glob_t;

#else
# include <glob.h>  /* for the glob_t type */
#endif /* _WIN32 */

int xpost_glob(const char *pattern, glob_t *pglob);
void xpost_glob_free(glob_t *pglob);

unsigned char xpost_module_path_get(int (*fp)(void), char *buf, unsigned int size);

char *xpost_realpath(const char *path);

/*
 * Open @p rel for reading beneath directory @p root, with the operating
 * system confining name resolution to @p root: it refuses to escape via
 * ".." or by following a symlink (POSIX openat2 RESOLVE_BENEATH, else a
 * realpath prefix check; Windows verifies the opened handle's final path).
 * @p rel should already be composed of validated path components. Returns
 * an stdio stream, or NULL with errno set.
 */
FILE *xpost_open_beneath(const char *root, const char *rel);

/* Access requested of xpost_openat2_beneath, mapped to open(2) flags inside
   the compat layer so callers need not reference O_* (absent on Windows). */
#define XPOST_OPEN_WRITE  0x01  /* the stream may be written */
#define XPOST_OPEN_RDWR   0x02  /* opened for both reading and writing */
#define XPOST_OPEN_CREATE 0x04  /* create the target if absent */
#define XPOST_OPEN_TRUNC  0x08  /* truncate an existing target */
#define XPOST_OPEN_APPEND 0x10  /* position writes at end of file */

/*
 * Open @p rel beneath @p root as xpost_open_beneath does, but with the access
 * described by @p access (a mask of XPOST_OPEN_*), using the kernel's atomic
 * beneath-root resolution so no name is resolved twice (no check-then-open
 * race). @p mode is the stdio mode for the returned stream. On success sets
 * *@p supported to 1 and returns the stream (or NULL with errno on a genuine
 * failure). Where the atomic primitive is unavailable (older kernel, or any
 * non-Linux build) sets *@p supported to 0 and returns NULL without touching
 * the filesystem, so the caller may apply a portable fallback.
 */
FILE *xpost_openat2_beneath(const char *root, const char *rel,
                            const char *mode, int access, int *supported);

/*
 * Write the canonical path of the object behind descriptor @p fd into @p buf.
 * Resolves the file actually open rather than a name, so the result is stable
 * against a concurrent rename or symlink swap. Returns 1 on success, 0 where
 * the identity cannot be established on this platform.
 */
int xpost_fd_realpath(int fd, char *buf, size_t buflen);

/*
 * Delete (unlink) @p rel beneath @p root, resolving the parent directory
 * atomically beneath @p root before acting, so the target cannot be repointed
 * after a check. Sets *@p supported as xpost_openat2_beneath does; returns 0
 * on success or -1 with errno.
 */
int xpost_unlinkat_beneath(const char *root, const char *rel, int *supported);

/*
 * Rename @p oldrel beneath @p oldroot to @p newrel beneath @p newroot, with
 * both parents resolved atomically beneath their roots. Sets *@p supported as
 * xpost_openat2_beneath does; returns 0 on success or -1 with errno.
 */
int xpost_renameat_beneath(const char *oldroot, const char *oldrel,
                           const char *newroot, const char *newrel,
                           int *supported);

/**
 * @}
 */

#endif
