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

#include <stdio.h> /* fprintf printf */
#include <stdlib.h> /* EXIT_FAILURE */
#include <string.h> /* free */

#include "xpost_pathname.h" /* xpost_is_installed exedir */
#include "xpost_memory.h" /* Xpost_Memory_File */
#include "xpost_object.h" /* Xpost_Object */
#include "xpost_context.h" /* Xpost_Context */
#include "xpost_interpreter.h" /* xpost_create */
#include "xpost_log.h" /* XPOST_LOG_ERR */

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

static void
_xpost_main_license(void)
{
    printf("BSD 3-clause\n");
}

static void
_xpost_main_version(const char *filename)
{
    printf("%s " PACKAGE_VERSION "\n", filename);
}

static void
_xpost_main_usage(const char *filename)
{
    printf("Usage: %s [options] [file.ps]\n\n", filename);
    printf("Postscript level 2 interpreter\n\n");
    printf("Options:\n");
    printf("  -o, --output=[FILE]    output file\n");
    printf("  -d, --device=[STRING]  string device\n");
    printf("  -L, --license          show program license\n");
    printf("  -V, --version          show program version\n");
    printf("  -h, --help             show this message\n");
}

static void
_xpost_main_device_list(void)
{
    printf("supported devices:\n");
    printf("\tPGM\n");
#ifdef HAVE_XCB
    printf("\tXCB\n");
#endif
#ifdef _WIN32
    printf("\tGDI\n");
#endif
}

int main(int argc, char *argv[])
{
    const char *output_file = NULL;
    const char *device = NULL;
    const char *ps_file = NULL;
    const char *filename = argv[0];
    int i;

    printf("EXTRA_BITS_SIZE = %d\n", XPOST_OBJECT_TAG_EXTRA_BITS_SIZE);
    printf("COMP_MAX_ENT = %d\n", XPOST_OBJECT_COMP_MAX_ENT);

#ifdef _WIN32
    device = "GDI";
#elif defined HAVE_XCB
    device = "XCB";
#else
    device = "PGM";
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
            else XPOST_MAIN_IF_OPT("-o", "--output=", output_file)
            else XPOST_MAIN_IF_OPT("-d", "--device=", device)
            else
            {
                printf("unknown option\n");
                _xpost_main_usage(filename);
                return -1;
            }
        }
        else
        {
            ps_file = argv[i];
        }
    }

    xpost_is_installed(filename); /* mallocs char* exedir */

    if (!xpost_create())
    {
        XPOST_LOG_ERR("Failed to initialize.");
        goto quit_xpost;
    }

    xpost_run();
    xpost_destroy();
    free(exedir);

    xpost_quit();

    return EXIT_SUCCESS;

  quit_xpost:
    xpost_quit();

    return EXIT_FAILURE;
}
