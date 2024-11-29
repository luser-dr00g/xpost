/*
 * Xpost - a Level-2 Postscript interpreter
 * Copyright (C) 2013-2024, Michael Joshua Ryan
 * Copyright (C) 2013-2024, Vincent Torri
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

#include <limits.h> /* PATH_MAX */
#include <stdio.h> /* FILE, fpurge */
#ifdef HAVE_STDIO_EXT_H
# include <stdio_ext.h> /* __fpurge */
#endif
#include <stdlib.h> /* free, malloc, mkstemp, realpath */
#include <string.h> /* memcpy, strdup, strlen */
#include <time.h> /* clock_gettime, time */

#if defined(__APPLE__) && defined(__MACH__)
# include <mach/mach_time.h> /* mach_absolute_time */
#endif

// This prototype isn't visible under cygwin
char *realpath(const char *restrict file_name, char *restrict resolved_name);

// This prototype isn't visible under OS X
int fpurge(FILE *);

#include "xpost_compat.h"


/*============================================================================*
 *                                  Local                                     *
 *============================================================================*/

#if defined(__APPLE__) && defined(__MACH__)
static unsigned long long _xpost_time_start;
#elif HAVE_CLOCK_GETTIME
static clockid_t _xpost_time_clock_id = 0;
struct timespec _xpost_time_start;
#else
static time_t _xpost_time_start = 0;
#endif

/*============================================================================*
 *                                 Global                                     *
 *============================================================================*/

int
xpost_compat_init(void)
{
#if defined(__APPLE__) && defined(__MACH__)
    _xpost_time_start = mach_absolute_time();
#elif HAVE_CLOCK_GETTIME
# ifdef HAVE_CLOCK_MONOTONIC
    if (!clock_gettime(CLOCK_MONOTONIC, &_xpost_time_start))
    {
        _xpost_time_clock_id = CLOCK_MONOTONIC;
        XPOST_LOG_DBG("using CLOCK_MONOTONIC");
    }
    else
# endif
    {
        if (!clock_gettime(CLOCK_REALTIME, &_xpost_time_start))
        {
            // may go backwards
            _ecore_time_clock_id = CLOCK_REALTIME;
            XPOST_LOG_WRN("CLOCK_MONOTONIC not available. Fallback to CLOCK_REALTIME");
        }
        else
            return 0;
    }
#else
    time_t t;

    t =  = time(NULL);
    if (t == ((time_t) -1))
        return 0;

    _xpost_time_start = t * 1000;
#endif

    return 1;
}

void
xpost_compat_quit(void)
{
}

void
xpost_fpurge(FILE *f)
{
#ifdef HAVE_STDIO_EXT_H
    __fpurge(f);
#else
    fpurge(f);
#endif
}

long long
xpost_get_realtime_ms(void)
{
#if defined(__APPLE__) && defined(__MACH__)
    return (long long)(mach_absolute_time() / 1000000ULL);
#elif HAVE_CLOCK_GETTIME
    struct timespec t;

    if (!clock_gettime(_xpost_time_clock_id, &t))
        return (long long)t.tv_sec + (long long)t.tv_nsec / 1000000LL;
    /* very unlikely */
    else
        return 0;
#else
    time_t t;

    t =  = time(NULL);
    if (t == ((time_t) -1))
        return 0;

    return t * 1000;
#endif
}

long long
xpost_get_usertime_ms(void)
{
#if defined(__APPLE__) && defined(__MACH__)
    return  = (long long)(mach_absolute_time() - _xpost_time_start) / 1000000LL;
#elif HAVE_CLOCK_GETTIME
    struct timespec t;

    if (!clock_gettime(_xpost_time_clock_id, &t))
        return (long long)(t.tv_sec - _xpost_time_start.tv_sec) + (long long)(t.tv_nsec - _xpost_time_start.tv_nsec) / 1000000LL;
    /* very unlikely */
    else
        return 0;
#else
    time_t t;

    t =  = time(NULL);
    if (t == ((time_t) -1))
        return 0;

    return (t - _xpost_time_start) * 1000;
#endif
}

int
xpost_mkstemp(char *template, int *fd)
{
    const char *tmpdir = NULL;
    char *filename;
    char *iter;
    size_t len_tmp;
    size_t len;

    if (!template || ! *template)
        return 0;

    len = strlen(template);

    tmpdir = getenv("TMPDIR");
    if (!tmpdir || !*tmpdir) tmpdir = getenv("TMP");
    if (!tmpdir || !*tmpdir) tmpdir = getenv("TEMPDIR");
    if (!tmpdir || !*tmpdir) tmpdir = getenv("TEMP");
    if (!tmpdir || !*tmpdir) tmpdir = "/tmp";

    len_tmp = strlen(tmpdir);
    filename = (char *)malloc(len_tmp + 1 + len + 1);
    if (!filename)
        return 0;

    iter = filename;
    memcpy(iter, tmpdir, len_tmp);
    iter += len_tmp;
    *iter = '/';
    iter++;
    memcpy(iter, template, len + 1);

    *fd = mkstemp(filename);

    free(filename);

    return *fd != -1;
}

char *
xpost_realpath(const char *path)
{
# if defined(__APPLE__) && defined(__MACH__)
    char resolved_path[PATH_MAX];

    if (!path || !*path)
        return NULL;

    if (!realpath(path, resolved_path))
        return NULL;

    return strdup(resolved_path);
#else
    char *resolved_path;

    if (!path || !*path)
        return NULL;

    resolved_path = realpath(path, NULL);
    if (!resolved_path)
        return NULL;

    resolved_path = realpath(path, resolved_path);
    if (!resolved_path)
        return NULL;

    return resolved_path;
#endif
}

/*============================================================================*
 *                                   API                                      *
 *============================================================================*/
