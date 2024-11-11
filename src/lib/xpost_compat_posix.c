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

#include <stdlib.h> /* malloc, free realpath, ,mkstemp */
#include <string.h> /* strlen, memcpy */

// This prototype isn't visible under cygwin
char *realpath(const char *restrict file_name, char *restrict resolved_name);

#include "xpost_compat.h"


/*============================================================================*
 *                                  Local                                     *
 *============================================================================*/

/*============================================================================*
 *                                 Global                                     *
 *============================================================================*/

int
xpost_compat_init(void)
{
    return 1;
}

void
xpost_compat_quit(void)
{
}

char *
xpost_realpath(const char *path)
{
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
}

int
xpost_mkstemp(char *template, int *fd)
{
    char *tmpdir = NULL;
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

/*============================================================================*
 *                                   API                                      *
 *============================================================================*/
