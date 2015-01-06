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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>

#ifdef HAVE_TERMIOS_H
# include <termios.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef _WIN32
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
# include <windows.h>
# include <io.h>
#include <stdlib.h>
# undef WIN32_LEAN_AND_MEAN
#else
# include <glob.h>
#endif

#include "xpost_compat.h"

/*
   Enable keyboard echo on terminal (console) input
 */
void echoon(FILE *f)
{
#ifdef _WIN32
    if (f == stdin)
    {
        HANDLE h;
        DWORD mode;

        h = GetStdHandle(STD_INPUT_HANDLE);
        if (GetConsoleMode(h, &mode))
        {
            mode |= ENABLE_ECHO_INPUT;
            SetConsoleMode(h, mode);
        }
    }
#elif defined HAVE_TERMIOS_H
    struct termios ts;

    tcgetattr(fileno(f), &ts);
    ts.c_lflag |= ECHO;
    tcsetattr(fileno(f), TCSANOW, &ts);
#else
    (void)f;
#endif
}

/*
   Disable keyboard echoing on terminal (console) input
 */
void echooff(FILE *f)
{
#ifdef _WIN32
    if (f == stdin)
    {
        HANDLE h;
        DWORD mode;

        h = GetStdHandle(STD_INPUT_HANDLE);
        if (GetConsoleMode(h, &mode))
        {
            mode &= ~ENABLE_ECHO_INPUT;
            SetConsoleMode(h, mode);
        }
    }
#elif defined HAVE_TERMIOS_H
    struct termios ts;

    tcgetattr(fileno(f), &ts);
    ts.c_lflag &= ~ECHO;
    tcsetattr(fileno(f), TCSANOW, &ts);
#else
    (void)f;
#endif
}

int xpost_isatty(int fd)
{
#ifdef _WIN32
    DWORD st;
    HANDLE h;

    if (!_isatty(fd))
        return 0;

    h = (HANDLE)_get_osfhandle(fd);
    if (h == INVALID_HANDLE_VALUE)
        return 0;

    if (!GetConsoleMode(h, &st))
        return 0;

    return 1;
#else
    return isatty(fd);
#endif
}

#ifdef _WIN32

int mkstemp(char *template)
{
    char *temp;

    temp = _mktemp(template);
    if (!temp)
        return -1;

    return _open(temp, _O_CREAT | _O_TEMPORARY | _O_EXCL | _O_RDWR, _S_IREAD | _S_IWRITE);
}

#endif

int
xpost_glob(const char *pattern, glob_t *pglob)
{
#ifdef _WIN32
    /* code based on the implementation in unixem (http://synesis.com.au/software/unixem.html) */
    /* BSD license */
    char path[MAX_PATH];
    WIN32_FIND_DATA fd;
    HANDLE f;
    char *buffer;
    char *iter;
    char *file;
    int result;
    size_t max_matches;

    if (!pattern || !pglob)
        return -1;

    /* path and file parts */
    file = NULL;
    iter = (char *)pattern;
    while (*iter)
    {
        if ((*iter == '/') || (*iter == '\\'))
            file = iter;
        iter++;
    }

    if (!file)
    {
        file = (char *)pattern;
        path[0] = '\0';
    }
    else
    {
        memcpy(path, pattern, file - pattern);
        path[file - pattern] = '\0';
        file++;
    }

    buffer = NULL;

    pglob->gl_pathc = 0;
    pglob->gl_pathv = NULL;
    pglob->gl_offs = 0;

    f = FindFirstFile(pattern, &fd);

    if (f == INVALID_HANDLE_VALUE)
    {
        if ((strpbrk(pattern, "?*") != NULL) && file)
            result = 0;
        else
            result = -1;
    }
    else
    {
        size_t current_len;
        size_t size;
        size_t matches;

        current_len = 0;
        size = 0;
        matches = 0;
        max_matches = ~(size_t)(0);
        result = 0;

        do
        {
            size_t len;
            size_t new_size;

            if ((file == strpbrk(file, "?*")) && (fd.cFileName[0] == '.'))
                continue;

            len = strlen(fd.cFileName);
            if (file)
                len += (file - pattern);

            new_size = current_len + len + 1;
            if (new_size > size)
            {
                char *new_buffer;

                new_size *= 2;
                new_size = (new_size + 31) & ~(31);
                new_buffer = (char *)realloc(buffer, new_size);
                if (!new_buffer)
                {
                    result = -1;
                    free(buffer);
                    buffer = NULL;
                    break;
                }

                buffer = new_buffer;
                size = new_size;
            }
            lstrcpyn(buffer + current_len, path, (file - pattern) + 1);
            lstrcat(buffer + current_len, fd.cFileName);
            current_len += len + 1;
            matches ++;
        } while (FindNextFile(f, &fd) && (matches != max_matches));

        FindClose(f);

        if (result == 0)
        {
            size_t nbr_pointers;
            char *new_buffer;

            nbr_pointers = (matches + pglob->gl_offs + 1) * sizeof(char *);
            new_buffer = (char *)realloc(buffer, size + nbr_pointers);
            if (!new_buffer)
            {
                result = -2;
                free(buffer);
            }
            else
            {
                char**  pp;
                char**  begin;
                char**  end;
                char*   next_str;

                buffer = new_buffer;
                memmove(new_buffer + nbr_pointers, new_buffer, size);
                begin = (char**)new_buffer;
                end = begin + pglob->gl_offs;
                for (; begin != end; ++begin)
                    *begin = NULL;

                pp = (char**)new_buffer + pglob->gl_offs;
                begin = pp;
                end = begin + matches;
                for (begin = pp, next_str = (buffer + nbr_pointers); begin != end; ++begin)
                {
                    *begin = next_str;
                    next_str += 1 + strlen(next_str);
                }

                *begin = NULL;
                pglob->gl_pathc = (int)matches;
                pglob->gl_pathv = (char**)new_buffer;
            }
        }

        if (matches == 0)
            result = -1;
    }

    if (result == 0)
    {
        if ((size_t)pglob->gl_pathc == max_matches)
            result = -1;
    }

    return (result == 0) ? 0 : -1;
#else
    return glob(pattern, 0, NULL, pglob);
#endif
}

void
xpost_glob_free(glob_t *pglob)
{
#ifdef _WIN32
    if (pglob)
    {
        free(pglob->gl_pathv);
        pglob->gl_pathc = 0;
        pglob->gl_pathv = NULL;
    }
#else
    globfree(pglob);
#endif
}
