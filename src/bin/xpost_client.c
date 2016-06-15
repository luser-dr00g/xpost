/*
 * Xpost - a Level-2 Postscript interpreter
 * Copyright (C) 2013-2016, Michael Joshua Ryan
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

/*
   This is a simple example of a client calling xpost as a library
   with a postscript program, desiring the raster data of the
   generated image.

   The "raster" device can operate in different modes specified with a colon.
   "raster:rgb" (default) 24bit rgb
   "raster:argb" 32big argb
   "raster:bgr" 24bit bgr
   "raster:bgra" 32bit bgra

   The BUFFEROUT output type is currently the only practical output of the buffer.
   The pointer supplied (by reference) is updated by calls to the `showpage` operator.
   The buffer size is currently hardcoded to US Letter dimensions in Postscript units
   1 unit = 1/72 inch.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

#include "xpost.h"


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
            fprintf(stderr, "missing option value"); \
            _xpost_client_usage(filename); \
            goto quit_xpost; \
        } \
    } \
    else \
    { \
        if (!*(argv[i] + sizeof(lo) - 1)) \
        { \
            fprintf(stderr, "missing option value"); \
            _xpost_client_usage(filename); \
            goto quit_xpost; \
        } \
        else \
        { \
            opt = argv[i] + sizeof(lo) - 1; \
        } \
    } \
}

const char *prog =
    "%%BoundingBox: 200 300 400 500\n"
    "0 0 1 setrgbcolor\n"
    "300 400 100 0 360 arc\n"
    "fill\n"
    "0 0 0 setrgbcolor\n"
    "290 390 moveto\n"
    "/Palatino-Roman 20 selectfont\n"
    "(Xpost) show\n"
    "showpage\n";

static const char *_xpost_client_devices[] =
{
    "raster",
#ifdef HAVE_LIBPNG
    "png",
#endif
#ifdef HAVE_LIBJPEG
    "jpeg",
#endif
    NULL
};

static void
_xpost_client_license(void)
{
    printf("BSD 3-clause\n");
}

static void
_xpost_client_version(const char *filename)
{
    int maj;
    int min;
    int mic;

    xpost_version_get(&maj, &min, &mic);
    printf("%s %d.%d.%d\n", filename, maj, min, mic);
}

static void
_xpost_client_usage(const char *filename)
{
    int i;

    printf("Usage: %s [options] [file.png]\n\n", filename);
    printf("Postscript level 2 interpreter\n\n");
    printf("Options:\n");
    printf("  -d, --device=[STRING]  device name (see below) [default=raster]\n");
    printf("  -i, --interlaced       create interlaced PNG [default=disabled]\n");
    printf("  -l, --level=[INT]      compression level for PNG betwwen 0 and 9 [default=0]\n");
    printf("  -Q, --quality=[INT]    quality for JPEG betwwen 0 and 100 [default=90]\n");
    printf("  -q, --quiet            suppress interpreter messages (default)\n");
    printf("  -v, --verbose          do not go quiet into that good night\n");
    printf("  -t, --trace            add additional tracing messages, implies -v\n");
    printf("  -L, --license          show program license\n");
    printf("  -V, --version          show program version\n");
    printf("  -h, --help             show this message\n");
    printf("\n");
    printf("  Supported devices:\n");
    i = 0;
    while (_xpost_client_devices[i])
        printf("\t%s\n", _xpost_client_devices[i++]);
}

int main(int argc, const char *argv[])
{
    Xpost_Context *ctx;
    void *buffer_type_object;
    const char *filename;
    const char *device;
    const void *ptr;
    Xpost_Output_Type output_type;
    Xpost_Showpage_Semantics show_page;
    int ret;
    int output_msg;
    int want_raster;
    int want_png;
    const char *compression_level = NULL;
    int png_interlaced = -1;
    int png_compression_level = 0;
    int want_jpeg;
    const char *quality = NULL;
    long jpeg_quality = -1;
    int i;

    filename = NULL;
    device = "raster";
    output_msg = XPOST_OUTPUT_MESSAGE_QUIET;

    i = 0;
    while (++i < argc)
    {
        if (*argv[i] == '-')
        {
            if ((!strcmp(argv[i], "-h")) ||
                (!strcmp(argv[i], "--help")))
            {
                _xpost_client_usage(argv[0]);
                return EXIT_SUCCESS;
            }
            else if ((!strcmp(argv[i], "-V")) ||
                     (!strcmp(argv[i], "--version")))
            {
                _xpost_client_version(argv[0]);
                return EXIT_SUCCESS;
            }
            else if ((!strcmp(argv[i], "-L")) ||
                     (!strcmp(argv[i], "--license")))
            {
                _xpost_client_license();
                return EXIT_SUCCESS;
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
            else if ((!strcmp(argv[i], "-i")) ||
                     (!strcmp(argv[i], "--interlaced")))
            {
                png_interlaced = 1;
            }
            else XPOST_MAIN_IF_OPT("-d", "--device=", device)
            else XPOST_MAIN_IF_OPT("-l", "--level=", compression_level)
            else XPOST_MAIN_IF_OPT("-Q", "--quality=", quality)
            else
            {
                printf("unknown option\n");
                _xpost_client_usage(argv[0]);
                return EXIT_FAILURE;
            }
        }
        else
            filename = argv[i];
    }

    want_jpeg = 0;
    want_png = 0;
    want_raster = 0;
    if (strcmp(device, "png") == 0)
    {
        if (!filename)
            filename = "xpost_client_out.png";
        device = "png";
        want_png = 1;
    }
    else if (strcmp(device, "jpeg") == 0)
    {
        if (!filename)
            filename = "xpost_client_out.jpeg";
        device = "jpeg";
        want_jpeg = 1;
    }
    else if (strcmp(device, "raster") == 0)
    {
        if (!filename)
            filename = "xpost_client_out.ppm";
        device = "raster:bgr";
        want_raster = 1;
    }

    if (!want_png && ((png_interlaced != -1) || (compression_level)))
    {
        printf("interlaced or compression level are available for PNG device only\n");
        _xpost_client_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (!want_jpeg && quality)
    {
        printf("quality is available for JPEG device only\n");
        _xpost_client_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (want_png)
    {
        output_type = XPOST_OUTPUT_FILENAME;
        show_page = XPOST_SHOWPAGE_NOPAUSE;
        ptr = filename;
        if (png_interlaced == -1) png_interlaced = 0;
        if (compression_level && *compression_level)
        {
            if ((compression_level[1] != '\0') ||
                (compression_level[0] < '0') ||
                (compression_level[0] > '9'))
            {
                _xpost_client_usage(argv[0]);
                return EXIT_FAILURE;
            }
            png_compression_level = compression_level[0] - '0';
        }
    }
    else if (want_jpeg)
    {
        output_type = XPOST_OUTPUT_FILENAME;
        show_page = XPOST_SHOWPAGE_NOPAUSE;
        ptr = filename;
        if (quality && *quality)
        {
            char *endptr;

            jpeg_quality = strtol(quality, &endptr, 10);

            if ((errno == ERANGE &&
                 (jpeg_quality == LONG_MAX || jpeg_quality == LONG_MIN)) ||
                (errno != 0 && jpeg_quality == 0))
            {
                perror("strtol");
                _xpost_client_usage(argv[0]);
                exit(EXIT_FAILURE);
            }

            if (endptr == quality)
            {
                fprintf(stderr, "No digits were found\n");
                _xpost_client_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
        }
        if (jpeg_quality == -1)
            jpeg_quality = 90;
    }
    else
    {
        output_type = XPOST_OUTPUT_BUFFEROUT;
        show_page = XPOST_SHOWPAGE_RETURN;
        ptr = &buffer_type_object;
    }

    xpost_init();

    if (!(ctx = xpost_create(device,
                             output_type,
                             ptr,
                             show_page,
                             output_msg,
                             XPOST_IGNORE_SIZE, 0, 0)))
    {
        fprintf(stderr, "unable to create interpreter context");
        exit(0);
    }

    printf("created interpreter context. executing program...\n");

    if (want_png)
    {
        char buf1[64];
        char buf2[64];
        char *defs[2];

        snprintf(buf1, sizeof(buf1),
                 "compression_level=%d", png_compression_level);
        snprintf(buf2, sizeof(buf2),
                 "interlaced=%d", png_interlaced ? 1 : 0);
        defs[0] = buf1;
        defs[1] = buf2;
        xpost_add_definitions(ctx, 2, defs);
    }

    if (want_jpeg)
    {
        char buf[64];
        char *defs[1];

        snprintf(buf, sizeof(buf), "quality=%d", jpeg_quality);
        defs[0] = buf;
        xpost_add_definitions(ctx, 1, defs);
    }

    ret = xpost_run(ctx, XPOST_INPUT_STRING, prog);
    printf("executed program. xpost_run returned %s\n", ret? "yieldtocaller": "zero");

    if ((!want_png && !want_jpeg) && !ret)
    {
        fprintf(stderr, "error before showpage\n");
    }
    else if (!want_png && !want_jpeg)
    {
        typedef struct { unsigned char blue, green, red; } pixel;
        pixel *buffer;
        int i, j;
        FILE *fp;

        buffer = buffer_type_object;
        fp = fopen(filename, "w");
        fprintf(fp, "P3\n612 792\n255\n");
        for (i = 0; i < 792; i++)
        {
            for (j = 0; j < 612; j++)
            {
                pixel pix = *buffer++;
                fprintf(fp, "%d %d %d ", pix.red, pix.green, pix.blue);
                if ((j % 20) == 0)
                    fprintf(fp, "\n");
            }
            fprintf(fp, "\n");
        }
        fclose(fp);
    }
    xpost_destroy(ctx);
    //free(buffer_type_object);
    xpost_quit();
  quit_xpost:
    return 0;
}

