/*
 * Xpost - a Level-2 Postscript interpreter
 * Copyright (C) 2013, Michael Joshua Ryan
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

#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#ifdef _WIN32
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
# include <winsock2.h> /* WSAStartup WSACleanup */
# undef WIN32_LEAN_AND_MEAN
#endif

#include "xpost.h"
#include "xpost_log.h"
#include "xpost_compat.h"
#include "xpost_object.h"
#include "xpost_memory.h"
#include "xpost_font.h"
#include "xpost_main.h"
#include "xpost_private.h"


/*============================================================================*
 *                                  Local                                     *
 *============================================================================*/

static int _xpost_init_count = 0;
static double _xpost_start_time = 0.0;
static char _xpost_lib_dir[XPOST_PATH_MAX];
static char _xpost_data_dir[XPOST_PATH_MAX];

/*============================================================================*
 *                                 Global                                     *
 *============================================================================*/

double
xpost_start_time_get(void)
{
    return _xpost_start_time;
}

/*============================================================================*
 *                                   API                                      *
 *============================================================================*/

XPAPI int
xpost_init(void)
{
    char tmp1[XPOST_PATH_MAX];
    char tmp2[XPOST_PATH_MAX];
#ifdef HAVE_GETTIMEOFDAY
    struct timeval tv;
#endif
#ifdef _WIN32
    WSADATA wsa_data;
#endif
    size_t l;

    if (++_xpost_init_count != 1)
        return _xpost_init_count;

    if (!xpost_log_init())
        return --_xpost_init_count;

    if (!xpost_module_path_get(xpost_init, _xpost_lib_dir, XPOST_PATH_MAX))
        return --_xpost_init_count;

    snprintf(tmp1, sizeof(tmp1), "%s/../share/xpost", _xpost_lib_dir);
    xpost_realpath(tmp1, tmp2);
    l = strlen(tmp2) + 1;
    memcpy(_xpost_data_dir, tmp2, l);

    if (!xpost_memory_init())
        return --_xpost_init_count;

    if (!xpost_font_init())
        return --_xpost_init_count;

#ifdef _WIN32
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
        return --_xpost_init_count;
#endif

#ifdef HAVE_GETTIMEOFDAY
    gettimeofday(&tv, NULL);
    _xpost_start_time = (((long)tv.tv_sec) * 1000) + ((long)tv.tv_usec / 1000);
#else
    _xpost_start_time = time(NULL) * 1000.0;
#endif

    return _xpost_init_count;
}

XPAPI int
xpost_quit(void)
{
    if (_xpost_init_count <= 0)
    {
        XPOST_LOG_ERR("Init count not greater than 0 in shutdown.");
        return 0;
    }

    if (--_xpost_init_count != 0)
        return _xpost_init_count;

#ifdef _WIN32
    WSACleanup();
#endif

    xpost_font_quit();
    xpost_log_quit();

    return _xpost_init_count;
}

XPAPI void
xpost_version_get(int *maj, int *min, int *mic)
{
    if (maj) *maj = XPOST_VERSION_MAJ;
    if (min) *min = XPOST_VERSION_MIN;
    if (mic) *mic = XPOST_VERSION_MIC;
}

XPAPI const char *
xpost_lib_dir_get(void)
{
    return _xpost_lib_dir;
}

XPAPI const char *
xpost_data_dir_get(void)
{
    return _xpost_data_dir;
}
