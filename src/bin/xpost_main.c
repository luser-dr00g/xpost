/*
 * Xpost - a Level-2 Postscript interpreter
 * Copyright (C) 2013, Michael Joshua Ryan
 * Copyright (C) 2013, Vincent Torri
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

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef HAVE_SIGNAL_H
# include <signal.h>
#endif

#include "xpost.h"
#include "xpost_log.h"
#ifdef _MSC_VER
# include "xpost_compat.h"
#endif

#include "xpost_main.h"


#define XPOST_MAIN_IF_OPT(so, lo, opt)  \
if ((!strcmp(argv[i], so)) || \
   (!strncmp(argv[i], lo, sizeof(lo) - 1))) \
{ \
    if (*(argv[i] + 2) == '\0') \
    { \
        if ((i + 1) < argc) \
        { \
            i++; \
            opt = argv[i]; \
        } \
        else \
        { \
            XPOST_LOG_ERR("missing option value"); \
            _xpost_main_usage(filename); \
            goto quit_xpost; \
        } \
    } \
    else \
    { \
        if (!*(argv[i] + sizeof(lo) - 1)) \
        { \
            XPOST_LOG_ERR("missing option value"); \
            _xpost_main_usage(filename); \
            goto quit_xpost; \
        } \
        else \
        { \
            opt = argv[i] + sizeof(lo) - 1; \
        } \
    } \
}

static const char *_xpost_main_devices[] =
{
    "pgm",
    "ppm",
    "null",
#ifdef _WIN32
    "gdi",
    "gl",
#endif
#ifdef HAVE_XCB
    "xcb",
#endif
    "bgr",
    "raster",
    NULL
};

static void
_xpost_main_license(void)
{
    printf("BSD 3-clause\n");
}

static void
_xpost_main_version(const char *filename)
{
    int maj;
    int min;
    int mic;

    xpost_version_get(&maj, &min, &mic);
    printf("%s %d.%d.%d\n", filename, maj, min, mic);
}

static void
_xpost_main_usage(const char *filename)
{
    printf("Usage: %s [options] [file.ps]\n\n", filename);
    printf("Postscript level 2 interpreter\n\n");
    printf("Options:\n");
    printf("  -o, --output=[FILE]           output file\n");
    printf("  -D, --device-list             device list\n");
    printf("  -d, --device=[STRING]         device name\n");
    printf("  -g, --geometry=WxH{+-}X{+-}Y  geometry specification\n");
    printf("  -q, --quiet                   suppress interpreter messages\n");
    printf("  -v, --verbose                 do not go quiet into that good night\n");
    printf("  -L, --license                 show program license\n");
    printf("  -V, --version                 show program version\n");
    printf("  -h, --help                    show this message\n");
}

static void
_xpost_main_device_list(void)
{
    int i;

    printf("supported devices:\n");
    i = 0;
    while (_xpost_main_devices[i])
    {
        printf("\t%s\n", _xpost_main_devices[i]);
        i++;
    }
}

static int
_xpost_atoi(char *str, int *v, char **endptr)
{
    long val;

    errno = 0;
    val = strtol(str, endptr, 10);;

    if (((errno == ERANGE) &&
         ((val == LONG_MAX) || (val == LONG_MIN))) ||
        ((errno != 0) && (val == 0)))
        return 0;

    if (*endptr == str)
        return 0;

    *v = (int)val;

    return 1;
}

static int
_xpost_geometry_parse(const char *geometry, int *width, int *height, int *xoffset, int *xsign, int *yoffset, int *ysign)
{
    char *str;
    char *endptr;
    int val;

    if (!geometry)
        return 0;

    /* width */
    str = (char *)geometry;
    if (!_xpost_atoi(str, &val, &endptr))
        return 0;

    *width = val;

    if (*endptr != 'x')
        return 0;

    /* height */
    str = endptr + 1;
    if (!_xpost_atoi(str, &val, &endptr))
        return 0;

    *height = val;

    if (*endptr == '+')
        *xsign = 1;
    else if (*endptr == '-')
        *xsign = -1;
    else
        return 0;

    /* xoffset */
    str = endptr + 1;
    if (!_xpost_atoi(str, &val, &endptr))
        return 0;

    *xoffset = val;

    if (*endptr == '+')
        *ysign = 1;
    else if (*endptr == '-')
        *ysign = -1;
    else
        return 0;

    /* yoffset */
    str = endptr + 1;
    if (!_xpost_atoi(str, &val, &endptr))
        return 0;

    *yoffset = val;

    if (*endptr != '\0')
        return 0;

    return 1;
}

int main(int argc, char *argv[])
{
    Xpost_Context *ctx;
    const char *geometry = NULL;
    const char *output_file = NULL;
    const char *device = NULL;
    const char *ps_file = NULL;
    const char *filename = argv[0];
    int quiet = 0;
    int have_device;
    int width = -1;
    int height = -1;
    int xoffset = 0;
    int yoffset = 0;
    int xsign = 1;
    int ysign = 1;
    int have_geometry = 0;
    int i;
#ifdef HAVE_SIGACTION
    struct sigaction sa, oldsa;

    sa.sa_handler = SIG_IGN;
    sigaction(SIGTRAP, &sa, &oldsa);
#endif

#ifdef DEBUG_ENTS
    printf("EXTRA_BITS_SIZE = %u\n", (unsigned int)XPOST_OBJECT_TAG_EXTRA_BITS_SIZE);
    printf("COMP_MAX_ENT = %u\n", (unsigned int)XPOST_OBJECT_COMP_MAX_ENT);
#endif

#ifdef _WIN32
    device = "gdi";
#elif defined HAVE_XCB
    device = "xcb";
#else
    device = "pgm";
#endif

    if (!xpost_init())
    {
        fprintf(stderr, "Fail to initialize xpost\n");
        return -1;
    }

    if (argc == 1)
    {
        /* XPOST_LOG_INFO("FIXME"); */
    }
    else
    {
        /* XPOST_LOG_INFO("FIXME"); */
    }

    i = 0;
    while (++i < argc)
    {
        if (*argv[i] == '-')
        {
            if ((!strcmp(argv[i], "-h")) ||
                (!strcmp(argv[i], "--help")))
            {
                _xpost_main_usage(filename);
                return EXIT_SUCCESS;
            }
            else if ((!strcmp(argv[i], "-V")) ||
                     (!strcmp(argv[i], "--version")))
            {
                _xpost_main_version(filename);
                return EXIT_SUCCESS;
            }
            else if ((!strcmp(argv[i], "-L")) ||
                     (!strcmp(argv[i], "--license")))
            {
                _xpost_main_license();
                return EXIT_SUCCESS;
            }
            else if ((!strcmp(argv[i], "-D")) ||
                     (!strcmp(argv[i], "--device-list")))
            {
                _xpost_main_device_list();
                return EXIT_SUCCESS;
            }
            else if ((!strcmp(argv[i], "-q")) ||
                     (!strcmp(argv[i], "--quiet")))
            {
                quiet = 1;
            }
            else if ((!strcmp(argv[i], "-v")) ||
                     (!strcmp(argv[i], "--verbose")))
            {
                quiet = 0;
            }
            else XPOST_MAIN_IF_OPT("-o", "--output=", output_file)
            else XPOST_MAIN_IF_OPT("-d", "--device=", device)
            else XPOST_MAIN_IF_OPT("-g", "--geometry=", geometry)
            else
            {
                printf("unknown option\n");
                _xpost_main_usage(filename);
                goto quit_xpost;
            }
        }
        else
        {
            ps_file = argv[i];
        }
    }

    /* parse geometry if any */
    if (!quiet)
    {
        printf("geom 1 : %s\n", geometry);
    }
    have_geometry = _xpost_geometry_parse(geometry, &width, &height, &xoffset, &xsign, &yoffset, &ysign);
    if (have_geometry)
    {
        XPOST_LOG_ERR("bad formatted geometry");
        goto quit_xpost;
    }
    if (!quiet)
    {
        printf("geom 2 : %dx%d%c%d%c%d\n",
               width, height,
               (xsign == 1) ? '+' : '-', xoffset,
               (ysign == 1) ? '+' : '-', yoffset);
    }

    {
        char *devstr = strdup(device);
        char *subdevice;
        if ((subdevice=strchr(devstr,':')))
            *subdevice++='\0';
        /* check devices */
        have_device = 0;
        i = 0;
        while (_xpost_main_devices[i])
        {
            if (strcmp(_xpost_main_devices[i], devstr) == 0)
            {
                have_device = 1;
                break;
            }
            i++;
        }
        free(devstr);
    }

    if (!have_device)
    {
        XPOST_LOG_ERR("wrong device.");
        _xpost_main_usage(filename);
        goto quit_xpost;
    }

    if (!(ctx = xpost_create(device,
                      XPOST_OUTPUT_FILENAME,
                      output_file,
                      XPOST_SHOWPAGE_DEFAULT,
                      quiet,
                      XPOST_IGNORE_SIZE, 0, 0)))
    {
        XPOST_LOG_ERR("Failed to initialize.");
        goto quit_xpost;
    }

    xpost_run(ctx, XPOST_INPUT_FILENAME, ps_file);
    xpost_destroy(ctx);

    xpost_quit();

    return EXIT_SUCCESS;

  quit_xpost:
    xpost_quit();

    return EXIT_FAILURE;
}
