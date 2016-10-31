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

#ifdef _WIN32
# define FMT_PTRDIFF_T "%Id"
#else
# define FMT_PTRDIFF_T "%td"
#endif

#define PRINT_STR_ARRAY(msg, memb) \
    do { \
        int i; \
        printf("%s : ", msg); \
        for (i = 0; i < dsc.header.memb.nbr; i++) \
            printf("%s ", dsc.header.memb.array[i]); \
        printf("\n"); \
    } while (0)

#define PRINT_INT_ARRAY(msg, memb) \
    do { \
        int i; \
        printf("%s : ", msg); \
        for (i = 0; i < dsc.header.memb.nbr; i++) \
            printf("%d ", dsc.header.memb.array[i]); \
        printf("\n"); \
    } while (0)

int main(int argc, char *argv[])
{
    Xpost_Dsc_File *file;
    Xpost_Dsc dsc;
    unsigned char res;

    if (argc < 2)
    {
        printf("Usage: %s file.ps", argv[0]);
        return -1;
    }

    file = xpost_dsc_file_new_from_file(argv[1]);
    if (!file)
    {
        printf("can not create file from filename %s\n", argv[1]);
    }

    res = xpost_dsc_parse(file, &dsc);
    printf("result : %s\n", res ? "good" : "error");
    if (!res)
    {
        printf("exiting because of errors...");
        xpost_dsc_file_del(file);
        return -1;
    }

    printf("version : %d.%d\n", dsc.ps_vmaj, dsc.ps_vmin);
    PRINT_STR_ARRAY("document fonts", document_fonts);
    printf("title : %s\n", dsc.header.title);
    printf("creator : %s\n", dsc.header.creator);
    printf("creation date : %s\n", dsc.header.creation_date);
    printf("for whom : %s\n", dsc.header.for_whom ? dsc.header.for_whom : "");
    printf("pages : %d\n", dsc.header.pages);
    printf("bounding box : %d %d %d %d\n",
           dsc.header.bounding_box.llx,
           dsc.header.bounding_box.lly,
           dsc.header.bounding_box.urx,
           dsc.header.bounding_box.ury);
    PRINT_STR_ARRAY("paper sizes", document_paper_sizes);
    printf("page order : ");
    switch (dsc.header.page_order)
    {
        case XPOST_DSC_PAGE_ORDER_ASCEND:
            printf("Ascend\n");
            break;
        case XPOST_DSC_PAGE_ORDER_DESCEND:
            printf("Descend\n");
            break;
        case XPOST_DSC_PAGE_ORDER_SPECIAL:
            printf("Special\n");
            break;
        default:
            printf("Unknown\n");
            break;
    }
    if (dsc.pages)
    {
        int i;

        for (i = 0; i < dsc.header.pages; i++)
        {
            printf("page #%d\n", i + 1);
            printf("  start: " FMT_PTRDIFF_T "\n", dsc.pages[i].section.start);
            printf("  end: " FMT_PTRDIFF_T "\n", dsc.pages[i].section.end);
            printf("  label: %s\n", dsc.pages[i].label);
            printf("  ordinal: %d\n", dsc.pages[i].ordinal);

#if 0
            /* Usage */
            {
                int j;
                const unsigned char *iter;

                iter = xpost_dsc_file_base_get(&h) + dsc.pages[i].start;
                printf("-----\n");
                for (iter; iter < xpost_dsc_file_base_get(&h) + dsc.pages[i].end; iter++)
                {
                    printf("%c", *iter);
                }
                printf("\n");
                printf("-----\n");
            }
#endif
        }
    }

    /* display prolog */
    {
        ptrdiff_t iter;

        /* iter = dsc.prolog.start; */
        printf("----- Begin Prolog -----\n");
        for (iter = dsc.prolog.start; iter < dsc.prolog.end; iter++)
        {
            printf("%c", *(xpost_dsc_file_base_get(file) + iter));
        }
        printf("\n");
        printf("----- End Prolog -----\n");
    }

    xpost_dsc_free(&dsc);
    xpost_dsc_file_del(file);

    return 0;
}
