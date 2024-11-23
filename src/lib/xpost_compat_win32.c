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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>  /* FILE, TMP_MAX */
#include <stdlib.h> /* free, getenv, malloc, */
#include <string.h> /* strlen, memcpy */

#include <windows.h>
#include <io.h> /* _open */
#include <fcntl.h> /* O_CREAT, etc... */
#include <sys/stat.h> /* S_IREAD, S_IWRITE */

#include "xpost_compat.h"


/*============================================================================*
 *                                  Local                                     *
 *============================================================================*/

static BCRYPT_ALG_HANDLE _xpost_bcrypt_provider;

static int
_xpost_mkstemp_fill(char *template)
{
    char *buf;

    buf = template;
    while (*buf)
    {
        unsigned char val;

        if (*buf != 'X')
            return 0;

        /*
         * Only characters from 'a' to 'z' and '0' to '9' are considered
         * because on Windows, file system is case insensitive. That means
         * 36 possible values.
         * To increase randomness, we consider the greatest multiple of 36
         * within 255 : 7*36 = 252, that is, values from 0 to 251 and choose
         * a random value in this interval.
         */
        do {
            BCryptGenRandom(_xpost_bcrypt_provider, &val, sizeof(UCHAR), 0);
        } while (val > 251);

        val = '0' + val % 36;
        if (val > '9')
            val += 'a' - '9' - 1;

        *buf = val;
        buf++;
    }

    return 1;
}

/*============================================================================*
 *                                 Global                                     *
 *============================================================================*/

int
xpost_compat_init(void)
{
    WSADATA wsa_data;

    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
        return 0;

    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&_xpost_bcrypt_provider,
                                                    BCRYPT_RNG_ALGORITHM,
                                                    NULL, 0)))
        return 0;

    return 1;
}

void
xpost_compat_quit(void)
{
    BCryptCloseAlgorithmProvider(_xpost_bcrypt_provider, 0);
    WSACleanup();
}

void
xpost_fpurge(FILE *f)
{
    /* no __fpurge() or fpurge functionos on Windows */
    (void)f;
}

int
xpost_mkstemp(char *template, int *fd)
{
    char *tmpdir;
    char *filename = NULL;
    char *iter;
    size_t len;
    size_t len_tmp;
    int f = -1;
    int count = TMP_MAX;

    if (!template || !*template)
        return 0;

    len = strlen(template);

    if ((tmpdir = getenv("TEMP")) || (tmpdir = getenv("TMP")))
    {
        len_tmp = strlen(tmpdir);
        /* path is $(tmpdir)\xpost_$(template) */
        filename = (char *)malloc(len_tmp + 7 /* \xpost_ */ + len + 1);
        if (filename)
        {
            iter = filename;
            memcpy(iter, tmpdir, len_tmp);
            iter += len_tmp;
            memcpy(iter, "\\xpost_", 7);
            iter += 7;
            memcpy(iter, template, len + 1);
        }
    }
    else if ((tmpdir = getenv("LOCALAPPDATA")))
    {
        len_tmp = strlen(tmpdir);
        /* path is $(tmpdir)\Temp\xpost_$(template) */
        filename = (char *)malloc(len_tmp + 12 /* \Temp\xpost_ */ + len + 1);
        if (filename)
        {
            iter = filename;
            memcpy(iter, tmpdir, len_tmp);
            iter += len_tmp;
            memcpy(iter, "\\Temp\\xpost_", 12);
            iter += 12;
            memcpy(iter, template, len + 1);
        }
    }
    else if ((tmpdir = getenv("USERPROFILE")))
    {
        len_tmp = strlen(tmpdir);
        /* path is $(tmpdir)\xpost_$(template) */
        filename = (char *)malloc(len_tmp + 7 /* \xpost_ */ + len + 1);
        if (filename)
        {
            iter = filename;
            memcpy(iter, tmpdir, len_tmp);
            iter += len_tmp;
            memcpy(iter, "\\xpost_", 7);
            iter += 7;
            memcpy(iter, template, len + 1);
        }
    }

    if (!filename)
        return 0;

    while ((f < 0) && (count-- > 0))
    {
        char *trail;

        CopyMemory(iter, template, len + 1);
        trail = iter + len - 6;

        if (!_xpost_mkstemp_fill(trail))
            break;

        printf("\n*** tmp: '%s'\n\n", filename);
        f = _open(filename,
                   O_CREAT | O_EXCL | O_RDWR | O_BINARY,
                   S_IREAD | S_IWRITE);
        if (f != -1)
            memcpy(template, iter, len + 1);
        else
        {
            if (errno != EEXIST)
                count = 0;
        }
    }

    free(filename);

    if (f == -1)
        return 0;

    *fd = f;

    return 1;
}

char *
xpost_realpath(const char *path)
{
    char *resolved_path;
    DWORD sz = 0UL;

    if (!path || !*path)
        return NULL;

    sz = GetFullPathName(path, 0UL, NULL, NULL);
    if (sz == 0UL)
        return NULL;

    resolved_path = malloc(sz * sizeof(char));
    if (!resolved_path)
        return NULL;

    sz = GetFullPathName(path, sz, resolved_path, NULL);
    if (sz == 0UL)
    {
        free(resolved_path);
        return NULL;
    }

    return resolved_path;
}

/*============================================================================*
 *                                   API                                      *
 *============================================================================*/
