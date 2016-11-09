/*
 * Xpost DSC - a DSC Postscript parser
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

#ifndef XPOST_DSC_H
#define XPOST_DSC_H

#include <stddef.h> /* for ptrdiff_t */

#ifdef XPAPI
# undef XPAPI
#endif

#ifdef _WIN32
# ifdef XPOST_BUILD
#  ifdef DLL_EXPORT
#   define XPAPI __declspec(dllexport)
#  else
#   define XPAPI
#  endif
# else
#  define XPAPI __declspec(dllimport)
# endif
#else
# ifdef __GNUC__
#  if __GNUC__ >= 4
#   define XPAPI __attribute__ ((visibility("default")))
#  else
#   define XPAPI
#  endif
# else
#  define XPAPI
# endif
#endif

#ifdef __cplusplus
extern "C" {
#endif /* ifdef __cplusplus */


/* File */

typedef struct Xpost_Dsc_File Xpost_Dsc_File;

XPAPI Xpost_Dsc_File *xpost_dsc_file_new_from_address(const unsigned char *base,
                                                      size_t length);

XPAPI Xpost_Dsc_File *xpost_dsc_file_new_from_file(const char *filename);

XPAPI const unsigned char *xpost_dsc_file_base_get(const Xpost_Dsc_File *file);

XPAPI size_t xpost_dsc_file_length_get(const Xpost_Dsc_File *file);

XPAPI void xpost_dsc_file_del(Xpost_Dsc_File *file);

/* DSC */

typedef enum
{
    XPOST_DSC_STATUS_ERROR,
    XPOST_DSC_STATUS_NO_DSC,
    XPOST_DSC_STATUS_SUCCESS
} Xpost_Dsc_Status;

typedef enum
{
    XPOST_DSC_JOB_NONE,
    XPOST_DSC_JOB_EPS,
    XPOST_DSC_JOB_QUERY,
    XPOST_DSC_JOB_EXIT_SERVER,
    XPOST_DSC_JOB_RESOURCE_ENCODING,
    XPOST_DSC_JOB_RESOURCE_FILE,
    XPOST_DSC_JOB_RESOURCE_FONT,
    XPOST_DSC_JOB_RESOURCE_FORM,
    XPOST_DSC_JOB_RESOURCE_PATTERN,
    XPOST_DSC_JOB_RESOURCE_PROCSET
} Xpost_Dsc_Job;

typedef enum
{
    XPOST_DSC_PAGE_ORDER_NONE,
    XPOST_DSC_PAGE_ORDER_ASCEND,
    XPOST_DSC_PAGE_ORDER_DESCEND,
    XPOST_DSC_PAGE_ORDER_SPECIAL
} Xpost_Dsc_Page_Order;

typedef struct
{
    int llx;
    int lly;
    int urx;
    int ury;
} Xpost_Dsc_Bounding_Box;

typedef struct
{
    char **array;
    int nbr;
} Xpost_Dsc_Str_Array;

typedef struct
{
    ptrdiff_t start; /* relative to base address */
    ptrdiff_t end; /* relative to base address */
} Xpost_Dsc_Section;

typedef struct
{
    Xpost_Dsc_Section section;
    char *label;
    int ordinal; /* -1 means '?' with DSC level 1 */
    Xpost_Dsc_Str_Array *fonts;
} Xpost_Dsc_Page;

typedef struct
{
    unsigned char ps_vmaj;
    unsigned char ps_vmin;
    Xpost_Dsc_Job job;
    unsigned char eps_vmaj;
    unsigned char eps_vmin;

    struct
    {
        /* level 1 */
        Xpost_Dsc_Str_Array document_fonts;
        char *title;
        char *creator;
        char *creation_date;
        char *for_whom;
        int pages;
        Xpost_Dsc_Bounding_Box bounding_box;
        /* level 2 */
        Xpost_Dsc_Str_Array document_paper_sizes;
        Xpost_Dsc_Str_Array document_needed_fonts;
        Xpost_Dsc_Str_Array document_supplied_fonts;
        /* level 3 */
        Xpost_Dsc_Page_Order page_order;
    } header;

    Xpost_Dsc_Section prolog;

    Xpost_Dsc_Page *pages;
} Xpost_Dsc;

XPAPI Xpost_Dsc_Status xpost_dsc_parse(const Xpost_Dsc_File *file,
                                       Xpost_Dsc *dsc);

XPAPI void xpost_dsc_free(Xpost_Dsc *dsc);


#ifdef __cplusplus
}
#endif /* ifdef __cplusplus */

#endif
