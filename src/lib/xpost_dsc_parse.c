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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <errno.h>
#include <limits.h>

#include "xpost.h"
#include "xpost_log.h"
#include "xpost_compat.h"

#include "xpost_dsc.h"
#include "xpost_dsc_ctx.h"

/*
 * Postscrip DSC parser
 * x means minimal conforming level 1 DSC
 * a means (atend) is supported
 *
 * header:
 * %%DocumentFonts       x  a  1
 * %%Title                     1
 * %%Creator                   1
 * %%CreationDate              1
 * %%For (!For: Creator lvl 1) 1
 * %%Pages                  a  1
 * %%BoundingBox            a  1
 *
 * %%Routing                   2
 * %%ProofMode                 2
 * %%Requirements              2
 * %%DocumentNeededFonts       2
 * %%DocumentSuppliedFonts     2
 * %%DocumentProcSets       a  2
 * %%DocumentNeededProcSets    2
 * %%DocumentSuppliedProcSets  2
 * %%DocumentNeededFiles       2
 * %%DocumentSuppliedFiles     2
 * %%DocumentPaperSizes        2
 * %%DocumentPaperForms        2
 * %%DocumentPaperColors       2
 * %%DocumentPaperWeights      2
 * %%DocumentPrinterRequired   2
 * %%DocumentProcessColors  a  2
 * %%DocumentCustomColors   a  2
 *
 * %%PageOrder                 3
 *
 * %%EndComments               1
 * %%EndProlog                 1
 *
 * body:
 * %%EndProlog           x     1
 * %%Page                x     1
 * %%PageFonts
 * %%Trailer             x     1
 */


/*============================================================================*
 *                                  Local                                     *
 *============================================================================*/


#define XPOST_DSC_EOL(iter) ((*iter == '\r') || (*iter == '\n'))

#define XPOST_DSC_EOF(cur) ((cur - ctx->base) == ctx->length)

#define XPOST_DSC_CMT(cmt) XPOST_DSC_ ## cmt

#define XPOST_DSC_CMT_LEN(cmt) (sizeof(XPOST_DSC_ ## cmt) - 1)

#define XPOST_DSC_CMT_CHECK(cmt) _xpost_dsc_prefix_cmp(ctx->cur_loc, sz, XPOST_DSC_CMT(cmt))

#define XPOST_DSC_CMT_CHECK_EXACT(cmt) _xpost_dsc_prefix_cmp(ctx->cur_loc, sz, XPOST_DSC_CMT(cmt))

#define XPOST_DSC_CMT_ARG(cmt) \
    ctx->cur_loc + XPOST_DSC_CMT_LEN(cmt) + ((*(ctx->cur_loc + XPOST_DSC_CMT_LEN(cmt)) == ' ') ? 1 : 0)

#define XPOST_DSC_CMT_IS_ATEND(cmt) _xpost_dsc_prefix_cmp_exact(XPOST_DSC_CMT_ARG(cmt), sz, "(atend)")

#define XPOST_DSC_TEXT_GET(buf, cmt) \
    do \
    { \
        if (!ctx->cmt) \
        { \
            const unsigned char *iter; \
            iter = XPOST_DSC_CMT_ARG(cmt); \
            buf = malloc(((end - iter) + 1) * sizeof(char)); \
            if (buf) \
            { \
                memcpy(buf, iter, (end - iter)); \
                buf[end - iter] = '\0'; \
            } \
            ctx->cmt = 1; \
        } \
    } while (0)


/* Comments */

#define XPOST_DSC_HEADER_VERSION "%!PS-Adobe-"

/* level 1 */
#define XPOST_DSC_HEADER_DOCUMENT_FONTS "%%DocumentFonts:"
#define XPOST_DSC_HEADER_TITLE "%%Title:"
#define XPOST_DSC_HEADER_CREATOR "%%Creator:"
#define XPOST_DSC_HEADER_CREATION_DATE "%%CreationDate:"
#define XPOST_DSC_HEADER_FOR "%%For:"
#define XPOST_DSC_HEADER_PAGES "%%Pages:"
#define XPOST_DSC_HEADER_BOUNDING_BOX "%%BoundingBox:"

/* level 2 */
#define XPOST_DSC_HEADER_DOCUMENT_PAPER_SIZES "%%DocumentPaperSizes:"

/* level 3 */
#define XPOST_DSC_HEADER_PAGE_ORDER "%%PageOrder:"

#define XPOST_DSC_HEADER_END_COMMENTS "%%EndComments"

#define XPOST_DSC_BODY_END_PROLOG "%%EndProlog"
#define XPOST_DSC_BODY_PAGE "%%Page:"
#define XPOST_DSC_BODY_PAGE_FONTS "%%PageFonts:"
#define XPOST_DSC_TRAILER "%%Trailer"

static const unsigned char *
_xpost_dsc_line_get(Xpost_Dsc_Ctx *ctx, const unsigned char **end, ptrdiff_t *sz)
{
    const unsigned char *e1;
    const unsigned char *e2;

    e1 = ctx->cur_loc;
    while (((e1 - ctx->base) < ctx->length) && !XPOST_DSC_EOL(e1))
        e1++;
    if (XPOST_DSC_EOF(e1))
    {
        printf(" ** EOF 1\n");
        ctx->eof = 1;
        return NULL;
    }

    e2 = e1;
    while (((e2 - ctx->base) < ctx->length) && XPOST_DSC_EOL(e2))
        e2++;
    if (XPOST_DSC_EOF(e2))
    {
        printf(" ** EOF 2\n");
        ctx->eof = 1;
        return NULL;
    }

    if ((e2 - ctx->cur_loc) > 255)
    {
        printf(" ** line too long\n");
        ctx->line_too_long = 1;
        return NULL;
    }

    *end = e1;
    *sz = e1 - ctx->cur_loc;

    return e2;
}

static unsigned char
_xpost_dsc_prefix_cmp(const unsigned char *iter, ptrdiff_t sz, const char *prefix)
{
    while (*prefix)
    {
        if (*prefix != *iter)
            return 0;
        prefix++;
        iter++;
    }

    return 1;
}

static unsigned char
_xpost_dsc_prefix_cmp_exact(const unsigned char *iter, ptrdiff_t sz, const char *prefix)
{
    while (*prefix)
    {
        if (*prefix != *iter)
            return 0;
        prefix++;
        iter++;
    }
    if (!XPOST_DSC_EOL(iter))
        return 0;

    return 1;
}

static const unsigned char *
_xpost_dsc_integer_get(const unsigned char *cur_loc, int *val)
{
    char buf[20];
    char *endptr;
    const unsigned char *iter;

    iter = cur_loc;
    while ((*iter >= '0') && (*iter <= '9'))
        iter++;
    if ((*iter != ' ') && !XPOST_DSC_EOL(iter))
        return NULL;

    memcpy(buf, cur_loc, iter - cur_loc);
    buf[iter - cur_loc] = '\0';

    *val = strtol(buf, &endptr, 10);
    if (((errno == ERANGE) &&
         ((*val == LONG_MAX) || (*val == LONG_MIN))) ||
        ((errno != 0) && (*val == 0)))
    {
        perror("strtol");
        return NULL;
    }

   if (endptr == buf)
   {
       fprintf(stderr, "No digits were found\n");
       return NULL;
   }

   return iter;
}

static unsigned char
_xpost_dsc_header_bounding_box_get(const unsigned char *iter,
                                   Xpost_Dsc_Bounding_Box *bb)
{
    int val;

    bb->llx = 0.0;
    bb->lly = 0.0;
    bb->urx = 0.0;
    bb->ury = 0.0;
    iter = _xpost_dsc_integer_get(iter, &val);
    if (iter)
    {
        bb->llx = val;
        if (*iter == ' ') iter++;
        else
        {
            printf("error\n");
            return 0;
        }
        iter = _xpost_dsc_integer_get(iter, &val);
        if (iter)
        {
            bb->lly = val;
            if (*iter == ' ') iter++;
            else
            {
                printf("error\n");
                return 0;
            }
            iter = _xpost_dsc_integer_get(iter, &val);
            if (iter)
            {
                bb->urx = val;
                if (*iter == ' ') iter++;
                else
                {
                    printf("error\n");
                    return 0;
                }
                iter = _xpost_dsc_integer_get(iter, &val);
                if (iter)
                {
                    bb->ury = val;
                    if (!XPOST_DSC_EOL(iter))
                    {
                        printf("error\n");
                        return 0;
                    }
                }
            }
        }
    }

    return 1;
}

static char **
_xpost_dsc_header_string_array_get(const unsigned char *cur_loc, int *count)
{
    const unsigned char *iter;
    char **array;
    int nbr;

    nbr = 0;
    iter = cur_loc;
    while (!XPOST_DSC_EOL(iter))
    {
        if (*iter == ' ')
            nbr++;
        iter++;
    }
    nbr++;

    array = (char **)calloc(nbr, sizeof(char *));
    if (!array)
    {
        *count = 0;
        return NULL;
    }

    *count = nbr;

    nbr = 0;
    iter = cur_loc;
    while (!XPOST_DSC_EOL(iter))
    {
        if (*iter == ' ')
        {
            array[nbr] = (char *)malloc((iter - cur_loc + 1) * sizeof(char));
            if (array[nbr])
            {
                memcpy(array[nbr], cur_loc, iter - cur_loc);
                array[nbr][iter - cur_loc] = '\0';
            }
            nbr++;
            cur_loc = iter + 1;
        }
        iter++;
    }

    array[nbr] = (char *)malloc((iter - cur_loc + 1) * sizeof(char));
    if (array[nbr])
    {
        memcpy(array[nbr], cur_loc, iter - cur_loc);
        array[nbr][iter - cur_loc] = '\0';
    }

    return array;
}

static const unsigned char *
_xpost_dsc_header_version_get(Xpost_Dsc_Ctx *ctx, Xpost_Dsc *h, const unsigned char **end, ptrdiff_t *size)
{
    const unsigned char *next;
    unsigned char v_maj;
    unsigned char v_min;
    size_t len;
    ptrdiff_t sz;

    next = _xpost_dsc_line_get(ctx, end, size);
    if (!next)
        return NULL;

    sz = *size;
    if ((*end - ctx->cur_loc) < 14)
    {
        XPOST_LOG_ERR("First comment erronoeus, size insufficient.");
        return NULL;
    }

    if (!XPOST_DSC_CMT_CHECK(HEADER_VERSION))
    {
        XPOST_LOG_ERR("First comment erronoeus.");
        return NULL;
    }

    len = XPOST_DSC_CMT_LEN(HEADER_VERSION);
    v_maj = *(ctx->cur_loc + len);
    if ((v_maj < '0') || (v_maj > '9'))
    {
        XPOST_LOG_ERR("First comment erronoeus (invalid vmaj) %c.", *(ctx->cur_loc + len));
        return NULL;
    }

    if (*(ctx->cur_loc + len + 1) != '.')
    {
        XPOST_LOG_ERR("First comment erronoeus (invalid version) %c.", *(ctx->cur_loc + len + 1));
        return NULL;
    }

    v_min = *(ctx->cur_loc + len + 2);
    if ((v_min < '0') || (v_min > '9'))
    {
        XPOST_LOG_ERR("First comment erronoeus (invalid vmin) %c.", *(ctx->cur_loc + len + 2));
        return NULL;
    }

    h->ps_version_maj = v_maj - '0';
    h->ps_version_min = v_min - '0';

    if ((h->ps_version_maj == 0) || (h->ps_version_maj > 3))
    {
        XPOST_LOG_ERR("First comment erronoeus (invalid vmaj).");
        return NULL;
    }

    if (((h->ps_version_maj == 2) && (h->ps_version_min > 1)) ||
        (h->ps_version_min > 0))
    {
        XPOST_LOG_ERR("First comment erronoeus (invalid vmin).");
        return NULL;
    }

    if ((h->ps_version_maj == 1) &&
        !XPOST_DSC_EOL((ctx->cur_loc + sizeof(XPOST_DSC_CMT(HEADER_VERSION)) + 2)))
    {
        XPOST_LOG_ERR("First comment erronoeus (level 1).");
        return NULL;

    }

    /* FIXME: do the options after the version of maj >= 2 */

    return next;
}

static unsigned char
_xpost_dsc_parse(Xpost_Dsc_Ctx *ctx, Xpost_Dsc *h)
{
    const unsigned char *next;
    const unsigned char *end;
    ptrdiff_t sz;
    unsigned char in_header = 0;
    unsigned char in_script = 0;
    unsigned char in_trailer = 0;

    /* first line is for version info */
    next = _xpost_dsc_header_version_get(ctx, h, &end, &sz);
    if (!next)
        return 0;

    ctx->cur_loc = next;
    in_header = 1;

    while (1)
    {
        next = _xpost_dsc_line_get(ctx, &end, &sz);
        if (!next)
        {
            XPOST_LOG_INFO("Can not get line: %s.",
                           ctx->eof ? " EOF" : ctx->line_too_long ? "too long" : "");
            break;
        }

        if (in_header)
        {
            if ((sz >= 2) && (ctx->cur_loc[0] == '%') && (ctx->cur_loc[1] == '%'))
            {
                char *txt = NULL;

                if (XPOST_DSC_CMT_CHECK_EXACT(HEADER_END_COMMENTS))
                {
                    XPOST_LOG_INFO("End of header (EndComments).");
                    in_header = 0;
                    continue;
                }
            }
            else
            {
                XPOST_LOG_INFO("End of header (no %%%%).");
                in_header = 0;
                continue;
            }
        }

        if (XPOST_DSC_CMT_CHECK_EXACT(BODY_END_PROLOG))
        {
            XPOST_LOG_INFO("EndProlog.");
            in_trailer = 0;
            in_script = 1;
        }

        if (in_header)
        {
//            if ((sz >= 2) && (ctx->cur_loc[0] == '%') && (ctx->cur_loc[1] == '%'))
            {
                char *txt = NULL;

                /* we check which comment we have */
                if (XPOST_DSC_CMT_CHECK_EXACT(TRAILER))
                {
                    XPOST_LOG_INFO("Trailer.");
                    in_trailer = 1;
                }
                else if (XPOST_DSC_CMT_CHECK(HEADER_TITLE))
                {
                    XPOST_DSC_TEXT_GET(txt, HEADER_TITLE);
                    h->header.title = txt;
                }
                else if (XPOST_DSC_CMT_CHECK(HEADER_CREATOR))
                {
                    XPOST_DSC_TEXT_GET(txt, HEADER_CREATOR);
                    h->header.creator = txt;
                }
                else if (XPOST_DSC_CMT_CHECK(HEADER_CREATION_DATE))
                {
                    XPOST_DSC_TEXT_GET(txt, HEADER_CREATION_DATE);
                    h->header.creation_date = txt;
                }
                else if (XPOST_DSC_CMT_CHECK(HEADER_FOR))
                {
                    XPOST_DSC_TEXT_GET(txt, HEADER_FOR);
                    h->header.for_whom = txt;
                }
                else if (XPOST_DSC_CMT_CHECK(HEADER_PAGES))
                {
                    if ((in_trailer) && (ctx->HEADER_PAGES != 2))
                    {
                        XPOST_LOG_ERR("%%PAGES is in trailer "
                                      "but not atend, exiting.");
                        return 0;
                    }

                    if (!in_trailer)
                    {
                        if (!ctx->HEADER_PAGES)
                        {
                            if (XPOST_DSC_CMT_IS_ATEND(HEADER_PAGES))
                                ctx->HEADER_PAGES = 2;
                            else
                                goto get_pages;
                        }
                    }
                    else
                    {
                        const unsigned char *iter;
                        int val;

                      get_pages:
                        iter = ctx->cur_loc + XPOST_DSC_CMT_LEN(HEADER_PAGES);
                        if (*iter == ' ')
                            iter++;

                        if (_xpost_dsc_integer_get(iter, &val))
                            h->header.pages = val;
                        ctx->HEADER_PAGES = 1;
                    }
                }
                else if (XPOST_DSC_CMT_CHECK(HEADER_BOUNDING_BOX))
                {
                    if ((in_trailer) && (ctx->HEADER_BOUNDING_BOX != 2))
                    {
                        XPOST_LOG_ERR("%%BoundingBox is in trailer "
                                      "but not atend, exiting.");
                        return 0;
                    }

                    if (!in_trailer)
                    {
                        if (!ctx->HEADER_BOUNDING_BOX)
                        {
                            if (XPOST_DSC_CMT_IS_ATEND(HEADER_BOUNDING_BOX))
                                ctx->HEADER_BOUNDING_BOX = 2;
                            else
                                goto get_bounding_box;
                        }
                    }
                    else
                    {
                        Xpost_Dsc_Bounding_Box bb;
                        const unsigned char *iter;

                      get_bounding_box:
                        iter = ctx->cur_loc + XPOST_DSC_CMT_LEN(HEADER_BOUNDING_BOX);
                        if (*iter == ' ')
                            iter++;

                        if (_xpost_dsc_header_bounding_box_get(iter, &bb))
                            h->header.bounding_box = bb;
                        ctx->HEADER_BOUNDING_BOX = 1;
                    }
                }
                else if (XPOST_DSC_CMT_CHECK(HEADER_DOCUMENT_FONTS))
                {
                    if ((in_trailer) && (ctx->HEADER_DOCUMENT_FONTS != 2))
                    {
                        XPOST_LOG_ERR("%%DocumentFonts is in trailer "
                                      "but not atend, exiting.");
                        return 0;
                    }

                    if (!in_trailer)
                    {
                        if (!ctx->HEADER_DOCUMENT_FONTS)
                        {
                            if (XPOST_DSC_CMT_IS_ATEND(HEADER_DOCUMENT_FONTS))
                                ctx->HEADER_DOCUMENT_FONTS = 2;
                            else
                                goto get_document_fonts;
                        }
                    }
                    else
                    {
                        const unsigned char *iter;
                        char **array;
                        int nbr;

                      get_document_fonts:
                        iter = ctx->cur_loc + XPOST_DSC_CMT_LEN(HEADER_DOCUMENT_FONTS);
                        if (*iter == ' ')
                            iter++;

                        array = _xpost_dsc_header_string_array_get(iter, &nbr);
                        h->header.document_fonts.array = array;
                        h->header.document_fonts.nbr = nbr;
                        ctx->HEADER_DOCUMENT_FONTS = 1;
                    }
                }
                else if (XPOST_DSC_CMT_CHECK(HEADER_DOCUMENT_PAPER_SIZES))
                {
                    if (h->ps_version_maj > 1)
                    {
                        const unsigned char *iter;
                        char **array;
                        int nbr;

                        if (!ctx->HEADER_DOCUMENT_PAPER_SIZES)
                        {
                            iter = ctx->cur_loc + XPOST_DSC_CMT_LEN(HEADER_DOCUMENT_PAPER_SIZES);
                            if (*iter == ' ')
                                iter++;

                            array = _xpost_dsc_header_string_array_get(iter, &nbr);
                            h->header.document_paper_sizes.array = array;
                            h->header.document_paper_sizes.nbr = nbr;
                            ctx->HEADER_DOCUMENT_PAPER_SIZES = 1;
                        }
                    }
                    else
                    {
                        XPOST_LOG_INFO("Comment not allowed in version %d.",
                                       h->ps_version_maj);
                    }
                }
                else if (XPOST_DSC_CMT_CHECK(HEADER_PAGE_ORDER))
                {
                    if (h->ps_version_maj > 2)
                    {
                        XPOST_DSC_TEXT_GET(txt, HEADER_PAGE_ORDER);
                        h->header.page_order = txt;
                    }
                    else
                    {
                        XPOST_LOG_INFO("Comment not allowed in version %d.",
                                       h->ps_version_maj);
                    }
                }
                else
                {
                    XPOST_LOG_INFO("Comment not allowed in header.");
                }
            }
        }

        if (in_script)
        {
            char *txt = NULL;

            if (XPOST_DSC_CMT_CHECK(BODY_PAGE))
            {
                XPOST_DSC_TEXT_GET(txt, BODY_PAGE);
                h->header.page_order = txt;
            }
        }

        ctx->cur_loc = next;
    }

    /*
     * DSC level 1 says that if there is no For comment, the intended
     * recipient is assumed to be the same as the value of Creator.
     *
     * FIXME: the test must be on the presence of For or not, not on its value.
     */
    if ((!h->header.for_whom) && (h->header.creator)&& (h->ps_version_maj == 1))
        h->header.for_whom = strdup(h->header.creator);

    return 1;
}


/*============================================================================*
 *                                 Global                                     *
 *============================================================================*/


/*============================================================================*
 *                                   API                                      *
 *============================================================================*/


XPAPI unsigned char
xpost_dsc_parse_from_file(const char *filename, Xpost_Dsc *h)
{
    Xpost_Dsc_Ctx *ctx;
    unsigned char res;

    if (!filename || !*filename)
    {
        printf("ERR: file empty\n");
        return 0;
    }

    memset(h, 0, sizeof(Xpost_Dsc));

    ctx = xpost_dsc_ctx_new_from_file(filename);
    if (!ctx)
    {
        printf("ERR: can not create context\n");
        return 0;
    }

    res = _xpost_dsc_parse(ctx, h);

    xpost_dsc_ctx_del(ctx);

    return res;
}
