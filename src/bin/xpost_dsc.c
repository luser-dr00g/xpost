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

#include <stdio.h>

#include <xpost_dsc.h>

#define PRINT_STR_ARRAY(msg, memb) \
    do { \
        int i; \
        printf("%s : ", msg); \
        for (i = 0; i < h.header.memb.nbr; i++) \
            printf("%s ", h.header.memb.array[i]); \
        printf("\n"); \
    } while (0)

#define PRINT_INT_ARRAY(msg, memb) \
    do { \
        int i; \
        printf("%s : ", msg); \
        for (i = 0; i < h.header.memb.nbr; i++) \
            printf("%d ", h.header.memb.array[i]); \
        printf("\n"); \
    } while (0)

int main(int argc, char *argv[])
{
    Xpost_Dsc h;
    unsigned char res;

    if (argc < 2)
    {
        printf("Usage: %s file.ps", argv[0]);
        return -1;
    }

    res = xpost_dsc_parse_from_file(argv[1], &h);
    printf("result : %s\n", res ? "good" : "error");
    if (!res)
    {
        printf("exiting because of errors...");
        return -1;
    }

    printf("version : %d.%d\n", h.ps_version_maj, h.ps_version_min);
    PRINT_STR_ARRAY("document fonts", document_fonts);
    printf("title : %s\n", h.header.title);
    printf("creator : %s\n", h.header.creator);
    printf("creation date : %s\n", h.header.creation_date);
    printf("for whom : %s\n", h.header.for_whom ? h.header.for_whom : "");
    printf("pages : %d\n", h.header.pages);
    printf("bounding box : %d %d %d %d\n",
           h.header.bounding_box.llx,
           h.header.bounding_box.lly,
           h.header.bounding_box.urx,
           h.header.bounding_box.ury);
    PRINT_STR_ARRAY("paper sizes", document_paper_sizes);
    printf("page order : %s\n", h.header.page_order);
    if (h.pages)
    {
        int i;

        for (i = 0; i < h.header.pages; i++)
        {
            printf("page #%d\n", i);
            printf("  label: %s\n", h.pages[i].label);
            printf("  ordinal: %d\n", h.pages[i].ordinal);
        }
    }

    return 0;
}

