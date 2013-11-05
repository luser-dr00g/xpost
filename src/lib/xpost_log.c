/*
 * Xpost - a Level-2 Postscript interpreter
 * Copyright (C) 2013, Michael Joshua Ryan
 * Copyright (C) 2013, Vincent Torri
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
#include <errno.h>
#include <limits.h>

#ifdef _WIN32
# include <windows.h>
#endif

#include "xpost_log.h"


/*============================================================================*
 *                                  Local                                     *
 *============================================================================*/

static const char *_xpost_log_level_names[] =
{
    "ERR",
    "WRN",
    "INF",
    "DBG"
};

#ifdef DEBUG
static Xpost_Log_Level _xpost_log_level = XPOST_LOG_LEVEL_DBG;
#else
static Xpost_Log_Level _xpost_log_level = XPOST_LOG_LEVEL_ERR;
#endif

static Xpost_Log_Print_Cb _xpost_log_print_cb = xpost_log_print_cb_stderr;
static void *_xpost_log_print_cb_data = NULL;

#ifdef _WIN32
static void
_xpost_log_print_prefix_func(FILE *stream,
                             Xpost_Log_Level level,
                             const char *file,
                             const char *fct,
                             int line)
{
    CONSOLE_SCREEN_BUFFER_INFO scbi;
    HANDLE std_handle;
    DWORD console;
    WORD color;
    char *str;
    DWORD res;
    int s;

    console = (stream == stderr) ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE;
    std_handle = GetStdHandle(console);
    if (std_handle == INVALID_HANDLE_VALUE)
        return;

    if (!GetConsoleScreenBufferInfo(std_handle, &scbi))
        return;

    s = snprintf(NULL, 0, "%s", _xpost_log_level_names[level]);
    if (s == -1)
        return;

    str = (char *)malloc((s + 1) * sizeof(char));
    if (!str)
        return;

    s = snprintf(str, s + 1, "%s", _xpost_log_level_names[level]);
    if (s == -1)
        goto free_str;

    str[s] = '\0';

    switch (level)
    {
        case XPOST_LOG_LEVEL_ERR:
            color = FOREGROUND_INTENSITY | FOREGROUND_RED;
            break;
        case XPOST_LOG_LEVEL_WARN:
            color = FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN;
            break;
        case XPOST_LOG_LEVEL_INFO:
            color = FOREGROUND_INTENSITY | FOREGROUND_GREEN;
            break;
        case XPOST_LOG_LEVEL_DBG:
            color = FOREGROUND_INTENSITY | FOREGROUND_BLUE;
            break;
        default:
            color = FOREGROUND_INTENSITY | FOREGROUND_BLUE;
            break;
    }

    SetConsoleTextAttribute(std_handle, color);
    if (!WriteConsole(std_handle, str, s, &res, NULL))
    {
        SetConsoleTextAttribute(std_handle, scbi.wAttributes);
        goto free_str;
    }

    free(str);

    if ((int)res != s)
        fprintf(stderr, "ERROR: %s(): want to write %d bytes, %ld written\n", __FUNCTION__, s, res);

    SetConsoleTextAttribute(std_handle, scbi.wAttributes);

    s = snprintf(NULL, 0, ": %s:%d ", file, line);
    if (s == -1)
        return;

    str = (char *)malloc((s + 1) * sizeof(char));
    if (!str)
        return;

    s = snprintf(str, s + 1, ": %s:%d ", file, line);
    if (s == -1)
        goto free_str;

    str[s] = '\0';

    if (!WriteConsole(std_handle, str, s, &res, NULL))
    {
        goto free_str;
    }

    free(str);

    if ((int)res != s)
        fprintf(stderr, "ERROR: %s(): want to write %d bytes, %ld written\n", __FUNCTION__, s, res);

    s = snprintf(NULL, 0, "%s() ", fct);
    if (s == -1)
        return;

    str = (char *)malloc((s + 1) * sizeof(char));
    if (!str)
        return;

    s = snprintf(str, s + 1, "%s() ", fct);
    if (s == -1)
        goto free_str;

    str[s] = '\0';

    SetConsoleTextAttribute(std_handle, FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    if (!WriteConsole(std_handle, str, s, &res, NULL))
    {
        SetConsoleTextAttribute(std_handle, scbi.wAttributes);
        goto free_str;
    }

    free(str);

    SetConsoleTextAttribute(std_handle, scbi.wAttributes);

    if ((int)res != s)
        fprintf(stderr, "ERROR: %s(): want to write %d bytes, %ld written\n", __FUNCTION__, s, res);

    return;

  free_str:
    free(str);
}
#else
static void
_xpost_log_print_prefix_func(FILE *stream,
                             Xpost_Log_Level level,
                             const char *file,
                             const char *fct,
                             int line)
{
    const char *color;

    switch (level)
    {
        case XPOST_LOG_LEVEL_ERR:
            color = "\033[31;1m";
            break;
        case XPOST_LOG_LEVEL_WARN:
            color = "\033[33;1m";
            break;
        case XPOST_LOG_LEVEL_DBG:
	  color = "\033[34;1m";
            break;
        case XPOST_LOG_LEVEL_INFO:
            color = "\033[32;1m";
            break;
        default:
            color = "\033[34m";
            break;
    }

    fprintf(stream, "%s%s" "\033[0m" ": %s:%d "
            "\033[1m" "%s()" "\033[0m" " ",
            color, _xpost_log_level_names[level], file, line, fct);
}
#endif

static void
_xpost_log_fprint_cb(FILE *stream,
                     Xpost_Log_Level level,
                     const char *file,
                     const char *fct,
                     int line,
                     const char *fmt,
                     void *data, /* later for XML output */
                     va_list args)
{
    char *str;
    int res;
    int s;

    s = vsnprintf(NULL, 0, fmt, args);
    if (s == -1)
        return;

    str = (char *)malloc((s + 2) * sizeof(char));
    if (!str)
        return;

    s = vsnprintf(str, s + 1, fmt, args);
    if (s == -1)
    {
        free(str);
        return;
    }
    str[s] = '\n';
    str[s + 1] = '\0';

    _xpost_log_print_prefix_func(stream, level, file, fct, line);
    res = fprintf(stream, str);
    free(str);

    if (res < 0)
        return;

    if ((int)res != (s + 1))
        fprintf(stderr, "ERROR: %s(): want to write %d bytes, %d written\n", __FUNCTION__, s + 1, res);

    (void)data;
}


/*============================================================================*
 *                                 Global                                     *
 *============================================================================*/

void
xpost_log_init(void)
{
    char *endptr;
    const char *level;
    long l;

    level = getenv("XPOST_LOG_LEVEL");
    if (level)
    {
        l = strtol(level, &endptr, 10);
        if (!((errno == ERANGE && (l == LONG_MAX || l == LONG_MIN)) ||
              (errno != 0 && l == 0) ||
              (endptr == level)))
            _xpost_log_level = (int)l;
    }
}

void
xpost_log_print_cb_set(Xpost_Log_Print_Cb cb, void *data)
{
    _xpost_log_print_cb = cb;
    _xpost_log_print_cb_data = data;
}

void
xpost_log_print_cb_stderr(Xpost_Log_Level level,
                          const char *file,
                          const char *fct,
                          int line,
                          const char *fmt,
                          void *data,
                          va_list args)
{
    _xpost_log_fprint_cb(stderr, level, file, fct, line, fmt, data, args);
}

void
xpost_log_print_cb_stdout(Xpost_Log_Level level,
                          const char *file,
                          const char *fct,
                          int line,
                          const char *fmt,
                          void *data,
                          va_list args)
{
    _xpost_log_fprint_cb(stdout, level, file, fct, line, fmt, data, args);
}

void
xpost_log_print(Xpost_Log_Level level,
                const char *file,
                const char *fct,
                int line,
                const char *fmt, ...)
{
    va_list args;

    if (!fmt)
    {
        fprintf(stderr, "ERROR: %s() fmt == NULL\n", __FUNCTION__);
        return;
    }

    if (level > _xpost_log_level)
        return;

    va_start(args, fmt);
    _xpost_log_print_cb(level, file, fct, line, fmt, _xpost_log_print_cb_data, args);
    va_end(args);
}
