/*
 * Xpost - a Level-2 Postscript interpreter
 * Copyright (C) 2013, Michael Joshua Ryan
 * Copyright (C) 2013, Thorsten Behrens
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

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_LIBGEN_H
# include <libgen.h>
#endif

#ifdef _WIN32
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
# include <windows.h>
# include <direct.h>
# undef WIN32_LEAN_AND_MEAN
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
# include "../lib/xpost_compat.h"
#endif
#include "xpost_main.h"
#include "xpost_pathname.h"

/*
 * check if path-to-executable is where it should be installed
 */
static
int checkexepath (const char *exepath)
{
#ifdef _MSC_VER
    char buf[MAX_PATH];
    char drive[_MAX_DRIVE];
    char dir[_MAX_DIR];
#endif
    char *exedir, *orig;
    int is_installed = 0;

#ifdef _WIN32
    char *slash;

    /* replace windows's \\  so we can compare paths */
    slash = (char *)exepath;
    while (*slash++)
    {
        if (*slash == '\\') *slash = '/';
    }
#endif

    /*TODO: remove: no longer needed */
    /* global exedir is set in ps systemdict as /EXE_DIR */
    exedir = orig = strdup(exepath);
#ifdef _MSC_VER
    _splitpath(exedir, drive, dir, NULL, NULL);
    snprintf(buf, sizeof(buf), "%s/%s", drive, dir);
    buf[sizeof(buf) - 1] = '\0';
    memcpy(exedir, buf, strlen(buf));
#else
    exedir = dirname(exedir);
#endif

#ifdef DEBUG_PATHNAME
    printf("exepath: %s\n", exepath);
    printf("dirname: %s\n", exedir);
    printf("PACKAGE_INSTALL_DIR: %s\n", PACKAGE_INSTALL_DIR);
#endif

    /* if path-to-executable is where it should be installed */
    is_installed = strstr(exepath, PACKAGE_INSTALL_DIR) != NULL;

#ifdef DEBUG_PATHNAME
    printf("is_installed: %d\n", is_installed);
#endif

    if (exedir != orig)
        free(orig);
    return is_installed;
}

/*
   append the argument (assumed to be a relative path)
   to the current working directory
 */
static
char *appendtocwd (const char *relpath)
{
    char buf[1024];
    if (getcwd(buf, sizeof buf) == NULL)
    {
        perror("getcwd() error");
        return NULL;
    }
    strcat(buf, "/");
    strcat(buf, relpath);
    return strdup(buf);
}

/*
   if all else fails in determining the path to the executable,
   ... (search $PATH variable, maybe??)
 */
static
int searchpathforargv0(const char *argv0)
{
    (void)argv0;
    /*
       not implemented.
       This function may be necessary on some obscure unices.
       It is called if there is no other path information,
       ie. argv[0] is a bare name,
       and no /proc/???/exe links are present
    */
    return checkexepath(".");
}

/*
   inspect argv[0] for a (relative?) path
 */
static
int checkargv0 (const char *argv0)
{
#ifdef _WIN32
    if (argv0[1] == ':' &&
        (argv0[2] == '/' || argv0[2] == '\\'))
#else
    if (argv0[0] == '/') /* absolute path */
#endif
    {
        return checkexepath(argv0);
    }
    else if (strchr(argv0, '/') != 0) /* relative path */
    {
        char *tmp;
        int ret;
        tmp = appendtocwd(argv0);
        ret = checkexepath(tmp);
        free(tmp);
        return ret;
    }
    else /* no path info: search $PATH */
        return searchpathforargv0(argv0);
}

/*
 * Check if xpost is running in "installed" mode 
 * or "not-installed" mode. 
 * This is used to determine if xpost can trust the
 * PACKAGE_INSTALL_DIR string to find its postscript files
 * or otherwise use a fallback strategy such as an
 * environment variable.
 */
int xpost_is_installed (const char *argv0)
{
    char buf[1024];
    ssize_t len;
    char *libsptr;
    char *exedir = NULL;
    int ret;

    (void)len; // len and buf are used in some, but not all, compilation paths
    (void)buf;
    //printf("argv0: %s\n", argv0);

    /* hack for cygwin and mingw.
       there's this unfortunate ".libs" in the path.
       remove it.
    */
    if ((libsptr = strstr(argv0, ".libs/"))) {
        printf("removing '.libs' from pathname\n");
        memmove(libsptr, libsptr+6, strlen(libsptr+6)+1);
        printf("argv0: %s\n", argv0);
        ret = checkargv0(argv0);
        if (exedir) free(exedir);
        return ret;
    }

#ifdef _WIN32
    ret = checkargv0(argv0);
    if (exedir) free(exedir);
    return ret;

    /*
      len = GetModuleFileName(NULL, buf, 1024);
      buf[len] = '\0';
      if (len == 0)
      return -1;
      else
      return checkexepath(buf);
    */
#else

    if ((len = readlink("/proc/self/exe", buf, sizeof buf)) != -1)
    {
        buf[len] = '\0';
    }

    if (len == -1)
        if ((len = readlink("/proc/curproc/exe", buf, sizeof buf)) != -1)
            buf[len] = '\0';

    if (len == -1)
        if ((len = readlink("/proc/self/path/a.out", buf, sizeof buf)) != -1)
            buf[len] = '\0';

    if (len == -1) {
        ret = checkargv0(argv0);
        if (exedir) free(exedir);
        return ret;
    }

    ret = checkexepath(buf);
    if (exedir) free(exedir);
    return ret;
#endif
}

