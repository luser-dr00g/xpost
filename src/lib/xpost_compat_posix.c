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

#if defined(__linux__)
# ifndef _GNU_SOURCE
#  define _GNU_SOURCE /* O_PATH, syscall, openat2 */
# endif
#elif defined(__APPLE__)
# ifndef _DARWIN_C_SOURCE
#  define _DARWIN_C_SOURCE /* O_NOFOLLOW and other BSD extensions */
# endif
#endif

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
#include <errno.h> /* errno */
#include <fcntl.h> /* open, O_* */
#include <unistd.h> /* close */
#if defined(__linux__)
# include <stdint.h> /* uint64_t */
# include <sys/syscall.h> /* SYS_openat2 */
#endif

#if defined(__APPLE__) && defined(__MACH__)
# include <mach/mach_time.h> /* mach_absolute_time */
#endif

// This prototype isn't visible under cygwin
char *realpath(const char *restrict file_name, char *restrict resolved_name);

// This prototype isn't visible under OS X
int fpurge(FILE *);

#include "xpost.h"
#include "xpost_compat.h"
#include "xpost_log.h"


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
            _xpost_time_clock_id = CLOCK_REALTIME;
            XPOST_LOG_WARN("CLOCK_MONOTONIC not available. Fallback to CLOCK_REALTIME");
        }
        else
            return 0;
    }
#else
    time_t t;

    t = time(NULL);
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
        return (long long)(t.tv_sec - _xpost_time_start.tv_sec) * 1000 + (long long)(t.tv_nsec - _xpost_time_start.tv_nsec) / 1000000LL;
    /* very unlikely */
    else
        return 0;
#else
    time_t t;

    t = time(NULL);
    if (t == ((time_t) -1))
        return 0;

    return t * 1000;
#endif
}

long long
xpost_get_usertime_ms(void)
{
#if defined(__APPLE__) && defined(__MACH__)
    return  (long long)(mach_absolute_time() - _xpost_time_start) / 1000000LL;
#elif HAVE_CLOCK_GETTIME
    struct timespec t;

    if (!clock_gettime(_xpost_time_clock_id, &t))
        return (long long)(t.tv_sec - _xpost_time_start.tv_sec) * 1000 + (long long)(t.tv_nsec - _xpost_time_start.tv_nsec) / 1000000LL;
    /* very unlikely */
    else
        return 0;
#else
    time_t t;

    t = time(NULL);
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

/* struct open_how / RESOLVE_* may predate the installed kernel headers */
#ifndef XPOST_RESOLVE_NO_SYMLINKS
# define XPOST_RESOLVE_NO_SYMLINKS 0x04
#endif
#ifndef XPOST_RESOLVE_BENEATH
# define XPOST_RESOLVE_BENEATH 0x08
#endif
/* the anchor dirfd needs no read access; fall back to a plain directory
   open where O_PATH is unavailable */
#ifndef O_PATH
# define O_PATH 0
#endif

FILE *
xpost_open_beneath(const char *root, const char *rel)
{
    int fd = -1;
    int resolved = 0; /* the OS-confined path produced an fd or a real error */

    if (!root || !rel || !*rel)
    {
        errno = ENOENT;
        return NULL;
    }

#if defined(__linux__) && defined(SYS_openat2)
    {
        struct { uint64_t flags; uint64_t mode; uint64_t resolve; } how;
        int rootfd = open(root, O_PATH | O_DIRECTORY | O_CLOEXEC);

        if (rootfd >= 0)
        {
            how.flags = (uint64_t)(O_RDONLY | O_CLOEXEC);
            how.mode = 0;
            how.resolve = XPOST_RESOLVE_BENEATH | XPOST_RESOLVE_NO_SYMLINKS;
            fd = (int)syscall(SYS_openat2, rootfd, rel, &how, sizeof how);
            close(rootfd);
            /* ENOSYS: kernel older than the syscall; use the portable check */
            if (!(fd < 0 && errno == ENOSYS))
                resolved = 1;
        }
    }
#endif

    if (!resolved)
    {
        char *root_real = xpost_realpath(root);
        char *file_real;
        char full[XPOST_PATH_MAX];
        size_t rl;

        if (!root_real)
            return NULL; /* errno from realpath */
        if (snprintf(full, sizeof full, "%s/%s", root_real, rel) >= (int)sizeof full)
        {
            free(root_real);
            errno = ENAMETOOLONG;
            return NULL;
        }
        file_real = xpost_realpath(full);
        if (!file_real)
        {
            free(root_real);
            return NULL; /* errno from realpath (ENOENT) */
        }
        /* the resolved file must sit strictly beneath the resolved root */
        rl = strlen(root_real);
        if (strncmp(file_real, root_real, rl) != 0 || file_real[rl] != '/')
        {
            free(root_real);
            free(file_real);
            errno = EACCES;
            return NULL;
        }
        fd = open(file_real, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
        free(root_real);
        free(file_real);
    }

    if (fd < 0)
        return NULL; /* errno from open/openat2 */

    {
        FILE *fp = fdopen(fd, "rb");
        if (!fp)
        {
            int e = errno;
            close(fd);
            errno = e;
            return NULL;
        }
        return fp;
    }
}

#if defined(__linux__) && defined(SYS_openat2)
/* translate the XPOST_OPEN_* mask into open(2) flags */
static int
xpost_open_oflags(int access)
{
    int oflags;

    if (access & XPOST_OPEN_RDWR)       oflags = O_RDWR;
    else if (access & XPOST_OPEN_WRITE) oflags = O_WRONLY;
    else                                oflags = O_RDONLY;
    if (access & XPOST_OPEN_CREATE) oflags |= O_CREAT;
    if (access & XPOST_OPEN_TRUNC)  oflags |= O_TRUNC;
    if (access & XPOST_OPEN_APPEND) oflags |= O_APPEND;
    return oflags;
}

/* openat2 with resolution confined beneath rootfd; -1/errno on failure,
   ENOSYS when the running kernel predates the syscall */
static int
xpost_openat2_raw(int rootfd, const char *rel, int oflags, unsigned int cmode)
{
    struct { uint64_t flags; uint64_t mode; uint64_t resolve; } how;

    how.flags = (uint64_t)(oflags | O_CLOEXEC);
    how.mode = (oflags & O_CREAT) ? (uint64_t)cmode : 0;
    how.resolve = XPOST_RESOLVE_BENEATH | XPOST_RESOLVE_NO_SYMLINKS;
    return (int)syscall(SYS_openat2, rootfd, rel, &how, sizeof how);
}
#endif

FILE *
xpost_openat2_beneath(const char *root, const char *rel, const char *mode,
                      int access, int *supported)
{
    *supported = 0;
#if defined(__linux__) && defined(SYS_openat2)
    {
        int rootfd;
        int fd;
        FILE *fp;

        if (!root || !rel || !*rel)
        {
            errno = ENOENT;
            return NULL;
        }
        rootfd = open(root, O_PATH | O_DIRECTORY | O_CLOEXEC);
        if (rootfd < 0)
            return NULL; /* supported stays 0: caller uses the fallback */
        fd = xpost_openat2_raw(rootfd, rel, xpost_open_oflags(access), 0666);
        close(rootfd);
        if (fd < 0 && errno == ENOSYS)
            return NULL; /* supported stays 0 */
        *supported = 1;
        if (fd < 0)
            return NULL; /* a genuine failure; errno already set */
        fp = fdopen(fd, mode);
        if (!fp)
        {
            int e = errno;
            close(fd);
            errno = e;
        }
        return fp;
    }
#else
    (void)root; (void)rel; (void)mode; (void)access;
    errno = ENOSYS;
    return NULL;
#endif
}

int
xpost_fd_realpath(int fd, char *buf, size_t buflen)
{
#if defined(__APPLE__) && defined(F_GETPATH)
    char tmp[PATH_MAX];

    if (fcntl(fd, F_GETPATH, tmp) != 0)
        return 0;
    if (strlen(tmp) >= buflen)
        return 0;
    strcpy(buf, tmp);
    return 1;
#elif defined(__linux__)
    char link[64];
    ssize_t n;

    snprintf(link, sizeof link, "/proc/self/fd/%d", fd);
    n = readlink(link, buf, buflen - 1);
    if (n < 0 || (size_t)n >= buflen)
        return 0;
    buf[n] = '\0';
    /* a since-unlinked file reads back as "<path> (deleted)": not a location
       we can meaningfully test, so report indeterminate */
    if (n >= 10 && strcmp(buf + n - 10, " (deleted)") == 0)
        return 0;
    return 1;
#else
    (void)fd; (void)buf; (void)buflen;
    return 0;
#endif
}

#if defined(__linux__) && defined(SYS_openat2)
/* Open the parent directory of rel beneath root (confined), returning its
   descriptor and, in leaf, the final path component to act on. -1/errno on
   failure; sets *supported per xpost_openat2_beneath. */
static int
xpost_open_parent_beneath(const char *root, const char *rel,
                          char *leaf, size_t leaflen, int *supported)
{
    const char *slash;
    char subdir[XPOST_PATH_MAX];
    int rootfd;
    int dirfd;
    struct { uint64_t flags; uint64_t mode; uint64_t resolve; } how;

    *supported = 0;
    if (!root || !rel || !*rel)
    {
        errno = ENOENT;
        return -1;
    }
    slash = strrchr(rel, '/');
    if (slash)
    {
        size_t dl = (size_t)(slash - rel);

        if (dl == 0 || dl >= sizeof subdir)
        {
            errno = ENOENT;
            return -1;
        }
        memcpy(subdir, rel, dl);
        subdir[dl] = '\0';
        slash++;
    }
    else
    {
        subdir[0] = '.';
        subdir[1] = '\0';
        slash = rel;
    }
    /* a control op must name a real leaf, never "" / "." / ".." */
    if (!*slash || strcmp(slash, ".") == 0 || strcmp(slash, "..") == 0)
    {
        errno = EINVAL;
        return -1;
    }
    if (strlen(slash) >= leaflen)
    {
        errno = ENAMETOOLONG;
        return -1;
    }
    rootfd = open(root, O_PATH | O_DIRECTORY | O_CLOEXEC);
    if (rootfd < 0)
        return -1; /* supported stays 0 */
    how.flags = (uint64_t)(O_PATH | O_DIRECTORY | O_CLOEXEC);
    how.mode = 0;
    how.resolve = XPOST_RESOLVE_BENEATH | XPOST_RESOLVE_NO_SYMLINKS;
    dirfd = (int)syscall(SYS_openat2, rootfd, subdir, &how, sizeof how);
    close(rootfd);
    if (dirfd < 0 && errno == ENOSYS)
        return -1; /* supported stays 0 */
    *supported = 1;
    if (dirfd < 0)
        return -1;
    strcpy(leaf, slash);
    return dirfd;
}
#endif

int
xpost_unlinkat_beneath(const char *root, const char *rel, int *supported)
{
    *supported = 0;
#if defined(__linux__) && defined(SYS_openat2)
    {
        char leaf[XPOST_PATH_MAX];
        int dirfd = xpost_open_parent_beneath(root, rel, leaf, sizeof leaf,
                                              supported);
        int ret;

        if (!*supported || dirfd < 0)
            return -1;
        ret = unlinkat(dirfd, leaf, 0);
        close(dirfd);
        return ret;
    }
#else
    (void)root; (void)rel;
    errno = ENOSYS;
    return -1;
#endif
}

int
xpost_renameat_beneath(const char *oldroot, const char *oldrel,
                       const char *newroot, const char *newrel,
                       int *supported)
{
    *supported = 0;
#if defined(__linux__) && defined(SYS_openat2)
    {
        char oldleaf[XPOST_PATH_MAX];
        char newleaf[XPOST_PATH_MAX];
        int oldfd;
        int newfd;
        int ret;

        oldfd = xpost_open_parent_beneath(oldroot, oldrel, oldleaf,
                                          sizeof oldleaf, supported);
        if (!*supported || oldfd < 0)
            return -1;
        newfd = xpost_open_parent_beneath(newroot, newrel, newleaf,
                                          sizeof newleaf, supported);
        if (!*supported || newfd < 0)
        {
            int e = errno;
            close(oldfd);
            errno = e;
            return -1;
        }
        ret = renameat(oldfd, oldleaf, newfd, newleaf);
        close(oldfd);
        close(newfd);
        return ret;
    }
#else
    (void)oldroot; (void)oldrel; (void)newroot; (void)newrel;
    errno = ENOSYS;
    return -1;
#endif
}

/*============================================================================*
 *                                   API                                      *
 *============================================================================*/
