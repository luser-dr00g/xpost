/*
 * Xpost - a Level-2 Postscript interpreter
 * Copyright (C) 2013, Michael Joshua Ryan
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

#include <stdio.h>
#include <stdlib.h>

#include "xpost_log.h"
#include "xpost_error.h"

static FILE *_xpost_error_dump_file = NULL;

int xpost_error_begin_dump(void)
{
    char dump_filename[] = "xdumpXXXXXX";
    int fd;
    FILE *fp;

    fd = mkstemp(dump_filename);
    fp = fdopen(fd, "w");

    XPOST_LOG_ERR("Error dump requested.\n"
                  "Writing interpreter state dump to %s",
                  dump_filename);

    _xpost_error_dump_file = fp;

    xpost_log_print_cb_set(xpost_error_print_cb_file, NULL);

    return 1;
}

int xpost_error_end_dump(void)
{
    return 1;
}

void xpost_error_print_cb_file(Xpost_Log_Level level,
                               const char *file,
                               const char *fct,
                               int line,
                               const char *fmt,
                               void *data,
                               va_list args)
{
    FILE *fp = _xpost_error_dump_file;

    //if (fp)
        vfprintf(fp, fmt, args);
}


