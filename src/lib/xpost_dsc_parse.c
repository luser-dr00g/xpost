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


#define XPOST_DSC_ERROR_TEST(test, msg) \
    do \
    { \
        if (test) \
        { \
            XPOST_LOG_ERR(msg); \
            break; \
        } \
    } while (0)

#define XPOST_DSC_HEADER_ERROR_TEST(cmt) \
    XPOST_DSC_ERROR_TEST(!in_header, cmt " comment not in header")

#define XPOST_DSC_EOL(iter) ((*iter == '\r') || (*iter == '\n'))

#define XPOST_DSC_EOF(cur) ((cur - ctx->base) == ctx->length)

#define XPOST_DSC_CMT(cmt) XPOST_DSC_ ## cmt

#define XPOST_DSC_CMT_LEN(cmt) (sizeof(XPOST_DSC_ ## cmt) - 1)

#define XPOST_DSC_CMT_CHECK(cmt) _xpost_dsc_prefix_cmp(ctx->cur_loc, sz, XPOST_DSC_CMT(cmt))

#define XPOST_DSC_CMT_CHECK_EXACT(cmt) _xpost_dsc_prefix_cmp_exact(ctx->cur_loc, sz, XPOST_DSC_CMT(cmt))

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
        XPOST_LOG_ERR("Line too long");
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

static unsigned char
_xpost_dsc_intger_get_from_string(const unsigned char *str, int *val)
{
    char *endptr;

    *val = strtol(str, &endptr, 10);
    if (((errno == ERANGE) &&
         ((*val == LONG_MAX) || (*val == LONG_MIN))) ||
        ((errno != 0) && (*val == 0)))
    {
        perror("strtol");
        return 0;
    }

    if (endptr == (char *)str)
    {
        XPOST_LOG_ERR("No digits were found");
        return 0;
    }

   return 1;
}

static const unsigned char *
_xpost_dsc_integer_get(const unsigned char *cur_loc, int *val)
{
    char buf[20];
    const unsigned char *iter;

    iter = cur_loc;
    while ((*iter >= '0') && (*iter <= '9'))
        iter++;
    if ((*iter != ' ') && !XPOST_DSC_EOL(iter))
        return NULL;

    memcpy(buf, cur_loc, iter - cur_loc);
    buf[iter - cur_loc] = '\0';

    if (!_xpost_dsc_intger_get_from_string(buf, val))
        return NULL;

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
    int page_idx = 0;
    unsigned char in_header = 0;
    unsigned char in_script = 0;
    unsigned char in_trailer = 0;
    unsigned char in_font = 0;

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
            XPOST_DSC_ERROR_TEST(in_header || in_trailer,
                                 "EndProlog comment in header or trailer");

            in_script = 1;
        }

        if (XPOST_DSC_CMT_CHECK_EXACT(TRAILER))
        {
            XPOST_LOG_INFO("Trailer.");
            XPOST_DSC_ERROR_TEST(!in_script, "Trailer comment not in script");
            if (page_idx > 0)
                h->pages[page_idx - 1].end = ctx->cur_loc - ctx->base;

            in_trailer = 1;
        }

        /* Header comments */
        {
            char *txt = NULL;

            /* Level 1 */
            if (XPOST_DSC_CMT_CHECK(HEADER_TITLE))
            {
                /* %%Title can be found in %%BeginFont */
                XPOST_DSC_ERROR_TEST(!in_header && !in_font,
                                     "Title comment not in header or in font");
                if (in_header)
                {
                    XPOST_DSC_TEXT_GET(txt, HEADER_TITLE);
                    h->header.title = txt;
                }
            }
            else if (XPOST_DSC_CMT_CHECK(HEADER_CREATOR))
            {
                /* %%Creator can be found in %%BeginFont */
                XPOST_DSC_ERROR_TEST(!in_header && !in_font,
                                     "Creator comment not in header or in font");
                if (in_header)
                {
                    XPOST_DSC_TEXT_GET(txt, HEADER_CREATOR);
                    h->header.creator = txt;
                }
            }
            else if (XPOST_DSC_CMT_CHECK(HEADER_CREATION_DATE))
            {
                /* %%CreationDate can be found in %%BeginFont */
                XPOST_DSC_ERROR_TEST(!in_header && !in_font,
                                     "CreationDate comment not in header or in font");
                if (in_header)
                {
                    XPOST_DSC_TEXT_GET(txt, HEADER_CREATION_DATE);
                    h->header.creation_date = txt;
                }
            }
            else if (XPOST_DSC_CMT_CHECK(HEADER_FOR))
            {
                XPOST_DSC_HEADER_ERROR_TEST("For");
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
                    {
                        h->header.pages = val;
                        h->pages = (Xpost_Dsc_Page *)calloc(val, sizeof(Xpost_Dsc_Page));
                    }
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
            /* Level 2 */
            else if (XPOST_DSC_CMT_CHECK(HEADER_DOCUMENT_PAPER_SIZES))
            {
                XPOST_DSC_HEADER_ERROR_TEST("DocumentPaperSizes");
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
            /* Level 3 */
            else if (XPOST_DSC_CMT_CHECK(HEADER_PAGE_ORDER))
            {
                XPOST_DSC_HEADER_ERROR_TEST("PageOrder");
                if (h->ps_version_maj > 2)
                {
                    if ((in_trailer) && (ctx->HEADER_PAGE_ORDER != 2))
                    {
                        XPOST_LOG_ERR("%%PageOrder is in trailer "
                                      "but not atend, exiting.");
                        return 0;
                    }

                    if (!in_trailer)
                    {
                        if (!ctx->HEADER_PAGE_ORDER)
                        {
                            if (XPOST_DSC_CMT_IS_ATEND(HEADER_PAGE_ORDER))
                                ctx->HEADER_PAGE_ORDER = 2;
                            else
                                goto get_page_order;
                        }
                    }
                    else
                    {
                        const unsigned char *iter;

                      get_page_order:
                        iter = XPOST_DSC_CMT_ARG(HEADER_PAGE_ORDER);
                        if (_xpost_dsc_prefix_cmp_exact(iter, sz, "Ascend"))
                            h->header.page_order = XPOST_DSC_PAGE_ORDER_ASCEND;
                        else if (_xpost_dsc_prefix_cmp_exact(iter, sz, "Descend"))
                            h->header.page_order = XPOST_DSC_PAGE_ORDER_DESCEND;
                        else if (_xpost_dsc_prefix_cmp_exact(iter, sz, "Special"))
                            h->header.page_order = XPOST_DSC_PAGE_ORDER_SPECIAL;
                        else
                        {
                            XPOST_LOG_ERR("%%PageOrder value is invalid");
                            return 0;
                        }

                    }
                }
                else
                {
                    XPOST_LOG_INFO("Comment not allowed in version %d.",
                                   h->ps_version_maj);
                }
            }
        } /* end of management of header comments */

        /* script comments */
        {
            if (XPOST_DSC_CMT_CHECK(BODY_PAGE))
            {
                const unsigned char *iter;
                const unsigned char *iter_next;
                char *label;
                char *ordinal_str;
                int ordinal;

                XPOST_DSC_ERROR_TEST(!in_script, "Page comment not in script");
                iter = XPOST_DSC_CMT_ARG(BODY_PAGE);
                if (*iter == ' ')
                    iter++;

                iter_next = iter;
                while (!XPOST_DSC_EOL(iter_next))
                {
                    if (*iter_next == ' ')
                        break;
                    iter_next++;
                }

                label = (char *)malloc(iter_next - iter + 1);
                if (label)
                {
                    memcpy(label, iter, iter_next - iter);
                    label[iter_next - iter]= '\0';
                }

                iter = ++iter_next;
                while (!XPOST_DSC_EOL(iter_next))
                {
                    if (*iter_next == ' ')
                        break;
                    iter_next++;
                }

                if (!XPOST_DSC_EOL(iter_next))
                {
                    XPOST_LOG_ERR("EOL not reached in %%PAGE comment");
                    break;
                }

                ordinal_str = (char *)malloc(iter_next - iter + 1);
                if (ordinal_str)
                {
                    memcpy(ordinal_str, iter, iter_next - iter);
                    ordinal_str[iter_next - iter]= '\0';
                }

                if (((h->ps_version_maj == 1)) &&
                    ((iter_next - iter) == 1) &&
                    (*iter == '?'))
                    ordinal = -1;

                if (!_xpost_dsc_intger_get_from_string(iter, &ordinal))
                {
                    free(ordinal_str);
                    break;
                }

                free(ordinal_str);
                if (ordinal < 1)
                    break;

                if (h->pages && (page_idx < h->header.pages))
                {
                    h->pages[page_idx].start = next - ctx->base;
                    if (page_idx > 0)
                        h->pages[page_idx - 1].end = ctx->cur_loc - ctx->base;
                    h->pages[page_idx].label = label;
                    h->pages[page_idx].ordinal = ordinal;
                }
                page_idx++;
            }
        } /* end of management of script */

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
xpost_dsc_parse_from_file(const Xpost_Dsc_File *file, Xpost_Dsc *h)
{
    Xpost_Dsc_Ctx ctx;
    unsigned char res;

    if (!file)
    {
        XPOST_LOG_ERR("Invalid file");
        return 0;
    }

    memset(h, 0, sizeof(Xpost_Dsc));
    memset(&ctx, 0, sizeof(Xpost_Dsc_Ctx));

    ctx.base = xpost_dsc_file_base_get(file);
    ctx.cur_loc = ctx.base;
    ctx.length = xpost_dsc_file_length_get(file);

    res = _xpost_dsc_parse(&ctx, h);

    return res;
}

XPAPI void
xpost_dsc_free(Xpost_Dsc *h)
{
    int i;

    if (h->header.title)
        free(h->header.title);
    if (h->header.creator)
        free(h->header.creator);
    if (h->header.creation_date)
        free(h->header.creation_date);
    if (h->header.for_whom)
        free(h->header.for_whom);

    for (i = 0; i < h->header.document_fonts.nbr; i++)
        free(h->header.document_fonts.array[i]);
    free(h->header.document_fonts.array);

    for (i = 0; i < h->header.document_paper_sizes.nbr; i++)
        free(h->header.document_paper_sizes.array[i]);
    free(h->header.document_paper_sizes.array);

    for (i = 0; i < h->header.pages; i++)
    {
        if (h->pages[i].label)
            free(h->pages[i].label);
    }

    if (h->pages)
        free(h->pages);
}
