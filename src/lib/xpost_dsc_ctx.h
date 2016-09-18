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

#ifndef XPOST_DSC_CTX_H
#define XPOST_DSC_CTX_H


#ifdef _WIN32
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
# include <windows.h>
# undef WIN32_LEAN_AND_MEAN
#endif

typedef struct Xpost_Dsc_Ctx Xpost_Dsc_Ctx;

struct Xpost_Dsc_Ctx
{
#ifdef _WIN32
    HANDLE h;
#else
    int fd;
#endif
    const unsigned char *base;
    const unsigned char *cur_loc;
    size_t length;
    /* level 1 */
    /* 0 unset, 1 set, 2 atend (unset) */
    unsigned int HEADER_DOCUMENT_FONTS : 2;
    unsigned int HEADER_TITLE : 1;
    unsigned int HEADER_CREATOR : 1;
    unsigned int HEADER_CREATION_DATE : 1;
    unsigned int HEADER_FOR : 1;
    unsigned int HEADER_PAGES : 2;
    unsigned int HEADER_BOUNDING_BOX : 2;
    unsigned int BODY_PAGE : 1;
    /* level 2 */
    unsigned int HEADER_DOCUMENT_PAPER_SIZES : 1;
    /* level 3 */
    unsigned int HEADER_PAGE_ORDER : 1;
    unsigned int from_file : 1;
    unsigned int line_too_long : 1;
    unsigned int eof : 1;
};

Xpost_Dsc_Ctx *xpost_dsc_ctx_new_from_address(const unsigned char *base,
                                              size_t length);

Xpost_Dsc_Ctx *xpost_dsc_ctx_new_from_file(const char *filename);

void xpost_dsc_ctx_del(Xpost_Dsc_Ctx *ctx);


#endif
