/*
 * Xpost - a Level-2 Postscript interpreter
 * Copyright (C) 2013, Michael Joshua Ryan
 * Copyright (C) 2013-2015, Vincent Torri
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

/**
 * @brief control the ECHO parameter of the terminal or console associated with file.
 */
void echoon(FILE *f);
void echooff(FILE *f);

int xpost_isatty(int fd);

#ifdef _WIN32

/**
 * @brief open a temporary file using @p template to generate the name.
 *
 * @returns new file descriptor
 *
 * Also modifies the @p template argument string.
 */
int mkstemp(char *template);


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

/**
 * @}
 */

#endif
