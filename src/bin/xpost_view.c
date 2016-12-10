/*
 * Xpost View - a small Level-2 Postscript viewer
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "xpost.h"
#include "xpost_dsc.h"
#include "xpost_view.h"

static Xpost_Context *ctx = NULL;
static Xpost_Dsc _dsc = { 0 };
static Xpost_Dsc *dsc = &_dsc;
static Xpost_Dsc_File *file = NULL;
static int page_num = 0;
static Xpost_View_Window *win = NULL;
static void *buffer = NULL;

static void
_xpost_view_license(void)
{
    printf("BSD 3-clause\n");
}

static void
_xpost_view_version(const char *progname)
{
    int maj;
    int min;
    int mic;

    xpost_version_get(&maj, &min, &mic);
    printf("%s %d.%d.%d\n", progname, maj, min, mic);
}

static void
_xpost_view_usage(const char *progname)
{
    printf("Usage: %s [options] file.ps\n\n", progname);
    printf("Postscript level 2 interpreter\n\n");
    printf("Options:\n");
    printf("  -q, --quiet            suppress interpreter messages (default)\n");
    printf("  -v, --verbose          do not go quiet into that good night\n");
    printf("  -t, --trace            add additional tracing messages, implies -v\n");
    printf("  -L, --license          show program license\n");
    printf("  -V, --version          show program version\n");
    printf("  -h, --help             show this message\n");
    printf("\n");
}

static int
_xpost_view_options_read(int argc, char *argv[], Xpost_Output_Message *msg, const char **filename)
{
    const char *psfile;
    Xpost_Output_Message output_msg;
    int i;

    psfile = NULL;
    output_msg = XPOST_OUTPUT_MESSAGE_QUIET;

    i = 0;
    while (++i < argc)
    {
        if (*argv[i] == '-')
        {
            if ((!strcmp(argv[i], "-h")) ||
                (!strcmp(argv[i], "--help")))
            {
                _xpost_view_usage(argv[0]);
                return 0;
            }
            else if ((!strcmp(argv[i], "-V")) ||
                     (!strcmp(argv[i], "--version")))
            {
                _xpost_view_version(argv[0]);
                return 0;
            }
            else if ((!strcmp(argv[i], "-L")) ||
                     (!strcmp(argv[i], "--license")))
            {
                _xpost_view_license();
                return 0;
            }
            else if ((!strcmp(argv[i], "-q")) ||
                     (!strcmp(argv[i], "--quiet")))
            {
                output_msg = XPOST_OUTPUT_MESSAGE_QUIET;
            }
            else if ((!strcmp(argv[i], "-v")) ||
                     (!strcmp(argv[i], "--verbose")))
            {
                output_msg = XPOST_OUTPUT_MESSAGE_VERBOSE;
            }
            else if ((!strcmp(argv[i], "-t")) ||
                     (!strcmp(argv[i], "--trace")))
            {
                output_msg = XPOST_OUTPUT_MESSAGE_TRACING;
            }
            else
            {
                printf("unknown option\n");
                _xpost_view_usage(argv[0]);
                return -1;
            }
        }
        else
            psfile = argv[i];
    }

    if (!psfile)
    {
        printf("Postscript file not provided\n");
        _xpost_view_usage(argv[0]);
        return -1;
    }

    *msg = output_msg;
    *filename = psfile;

    return 1;
}

static void
_xpost_view_page_set(void)
{
    xpost_run(ctx, XPOST_INPUT_STRING,
              (void *)(xpost_dsc_file_base_get(file) + dsc->pages[page_num].section.start),
              dsc->pages[page_num].section.end - dsc->pages[page_num].section.start);

    xpost_view_page_display(win, buffer);

    xpost_run(ctx, XPOST_INPUT_RESUME,
              (void *)(xpost_dsc_file_base_get(file) + dsc->pages[page_num].section.start),
              dsc->pages[page_num].section.end - dsc->pages[page_num].section.start);
}

void xpost_view_page_change(int i)
{
    int page = page_num + i;

    if (page < 0) page = 0;
    if (page > (dsc->header.pages - 1)) page = dsc->header.pages - 1;

    if (page != page_num)
    {
        page_num = page;
        _xpost_view_page_set();
    }
}

int main(int argc, char *argv[])
{
    const char *psfile;
    Xpost_Dsc_Status status;
    Xpost_Output_Message output_msg;
    int width;
    int height;
    int ret;

    psfile = NULL;
    output_msg = XPOST_OUTPUT_MESSAGE_QUIET;

    ret = _xpost_view_options_read(argc, argv, &output_msg, &psfile);
    if (ret == -1) return EXIT_FAILURE;
    else if (ret == 0) return EXIT_SUCCESS;

    file = xpost_dsc_file_new_from_file(psfile);
    if (!file)
        return EXIT_FAILURE;

    status = xpost_dsc_parse(file, dsc);

    /*
     * status:
     * XPOST_DSC_STATUS_ERROR: DSC, but ps file not conforming to mandatory DSC
     * XPOST_DSC_STATUS_NO_DSC: no error, but no DSC
     * XPOST_DSC_STATUS_SUCCESS: no error and DSC
     */

    if (status == XPOST_DSC_STATUS_ERROR)
    {
        fprintf(stderr, "File %s not conforming to DSC\n", psfile);
        goto del_file;
    }

    if (status == XPOST_DSC_STATUS_NO_DSC)
    {
        width = 612;
        height = 792;
    }
    else
    {
        width = dsc->header.bounding_box.urx;
        height = dsc->header.bounding_box.ury;
    }

    if (!xpost_init())
    {
        fprintf(stderr, "Xpost failed to initialize\n");
        goto free_dsc;
    }

    ctx = xpost_create("raster:bgra",
                       XPOST_OUTPUT_BUFFEROUT,
                       &buffer,
                       XPOST_SHOWPAGE_RETURN,
                       output_msg,
                       XPOST_USE_SIZE, width, height);
    if (!ctx)
    {
        fprintf(stderr, "Xpost failed to create interpreter context\n");
        goto quit_xpost;
    }

    win = xpost_view_win_new(10, 10, width, height);
    if (!win)
    {
        fprintf(stderr, "Can not create window\n");
        goto destroy_context;
    }

    /* Prolog */
    printf("begin prolog : %d\n", (int)(dsc->prolog.end - dsc->prolog.start));
    ret = xpost_run(ctx, XPOST_INPUT_STRING,
                    (void *)(xpost_dsc_file_base_get(file) + dsc->prolog.start),
                    dsc->prolog.end - dsc->prolog.start);
    printf("end prolog %d\n", ret);

    /* FIXME: manage the case where there is no DSC */
    /* get buffer for the first page */
    _xpost_view_page_set();

    xpost_view_main_loop(win);

    xpost_view_win_del(win);
    xpost_destroy(ctx);
    xpost_quit();
    xpost_dsc_free(dsc);
    xpost_dsc_file_del(file);

    return EXIT_SUCCESS;

  destroy_context:
    xpost_destroy(ctx);
  quit_xpost:
    xpost_quit();
  free_dsc:
    xpost_dsc_free(dsc);
  del_file:
    xpost_dsc_file_del(file);

    return EXIT_FAILURE;
}
