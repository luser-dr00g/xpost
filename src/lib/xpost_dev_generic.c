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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h> /* abs */
#include <stddef.h>

#include <assert.h>
#include <math.h>
#include <string.h>

#ifdef HAVE_ZLIB
# include <zlib.h>
#endif

#include "xpost.h"
#include "xpost_log.h"
#include "xpost_memory.h" /* access memory */
#include "xpost_object.h" /* work with objects */
#include "xpost_stack.h"  /* push results on stack */
#include "xpost_context.h" /* state */
#include "xpost_error.h"
#include "xpost_dict.h" /* get/put values in dicts */
#include "xpost_string.h" /* get/put values in strings */
#include "xpost_array.h"
#include "xpost_name.h" /* create names */

#include "xpost_operator.h" /* create operators */
#include "xpost_op_dict.h" /* call xpost_op_any_load operator for convenience */
#include "xpost_dev_generic.h" /* check prototypes */

struct point
{
    real x, y;
};

/* marks a subpath separator in a point list */
#define SUBPATH_BREAK ((real)-0x7ffffff)

/* FIXME: re-entrancy */
static Xpost_Context *localctx;

static Xpost_Object namewidth;
static Xpost_Object namenativecolorspace;
static Xpost_Object nameDeviceGray;
static Xpost_Object nameDeviceRGB;
static Xpost_Object nameroll;
static Xpost_Object nameDrawLine;
static Xpost_Object nameexec;
static Xpost_Object namerepeat;
static Xpost_Object namecvx;
static Xpost_Object nameRbracket;
static Xpost_Object nameImgData;
static Xpost_Object nameFillRect;
static Xpost_Object namepdfPrivate;

char *xpost_device_get_filename(Xpost_Context *ctx, Xpost_Object devdic)
{
    Xpost_Object filenamestr;
    char *filename;

    filenamestr = xpost_dict_get(ctx, devdic,
                                 xpost_name_cons(ctx, "OutputFileName"));
    filename = malloc(filenamestr.comp_.sz + 1);
    if (filename)
    {
        memcpy(filename, xpost_string_get_pointer(ctx, filenamestr), filenamestr.comp_.sz);
        filename[filenamestr.comp_.sz] = '\0';
    }

    return filename;
}

int xpost_device_set_filename(Xpost_Context *ctx, Xpost_Object devdic, char *filename)
{
    Xpost_Object filenamestr;
    int ret;

    filenamestr = xpost_string_cons(ctx, strlen(filename), filename);
    if ((ret = xpost_dict_put(ctx, devdic, xpost_name_cons(ctx, "OutputFileName"), filenamestr)))
        return ret;
    return 0;
}

static
int _yxcomp(const void *left, const void *right)
{
    const Xpost_Object *lt = left;
    const Xpost_Object *rt = right;
    Xpost_Object leftx, lefty, rightx, righty;
    integer ltx, lty, rtx, rty;

    leftx = xpost_array_get(localctx, *lt, 0);
    lefty = xpost_array_get(localctx, *lt, 1);
    rightx = xpost_array_get(localctx, *rt, 0);
    righty = xpost_array_get(localctx, *rt, 1);
    ltx = xpost_object_get_type(leftx) == realtype ?
        (integer)leftx.real_.val : leftx.int_.val;
    lty = xpost_object_get_type(lefty) == realtype ?
        (integer)lefty.real_.val : lefty.int_.val;
    rtx = xpost_object_get_type(rightx) == realtype ?
        (integer)rightx.real_.val : rightx.int_.val;
    rty = xpost_object_get_type(righty) == realtype ?
        (integer)righty.real_.val : righty.int_.val;
    if (lty == rty)
    {
        if (ltx < rtx)
        {
            return 1;
        }
        else if (ltx > rtx)
        {
            return -1;
        } else
        {
            return 0;
        }
    }
    else
    {
        if (lty < rty)
            return -1;
        else
            return 1;
    }
}

static
int _yxsort (Xpost_Context *ctx, Xpost_Object arr)
{
    unsigned char *arrcontents;
    unsigned int arradr;
    Xpost_Memory_File *mem;

    mem = xpost_context_select_memory(ctx, arr);
    if (!xpost_memory_table_get_addr(mem, xpost_object_get_ent(arr), &arradr))
        return VMerror;
    arrcontents = (mem->base + arradr);

    localctx = ctx;
    qsort(arrcontents, arr.comp_.sz, sizeof(arr), _yxcomp);
    localctx = NULL;

    return 0;
}

/* One boundary-chain passage through a pixel-row band: the x extent
   [lo, hi] the chain covers within the band (row b covers device
   b <= y < b+1) and the chain's y direction (+1 rising, -1 falling) */
struct band_span
{
    int band;
    int dirn;
    real lo, hi;
};

static
int _bandspancomp (const void *left, const void *right)
{
    const struct band_span *lt = left;
    const struct band_span *rt = right;

    if (lt->band != rt->band)
        return lt->band < rt->band ? -1 : 1;
    if (lt->lo != rt->lo)
        return lt->lo < rt->lo ? -1 : 1;
    if (lt->hi != rt->hi)
        return lt->hi < rt->hi ? -1 : 1;
    return lt->dirn - rt->dirn;
}

/* append a span, growing the array as needed; 0 on success */
static
int _span_push(struct band_span **spans, int *cap, int *n,
               int band, int dirn, real lo, real hi)
{
    if (*n == *cap)
    {
        struct band_span *tmp;
        int newcap = *cap ? *cap * 2 : 64;

        tmp = realloc(*spans, newcap * sizeof *tmp);
        if (!tmp)
            return VMerror;
        *spans = tmp;
        *cap = newcap;
    }
    (*spans)[*n].band = band;
    (*spans)[*n].dirn = dirn;
    (*spans)[*n].lo = lo;
    (*spans)[*n].hi = hi;
    ++*n;
    return 0;
}

static
int _fillpoly(Xpost_Context *ctx,
              Xpost_Object poly,
              Xpost_Object devdic)
{
    Xpost_Object colorspace;
    int ncomp;
    Xpost_Object comp1, comp2, comp3;
    int numlines;
    /* Xpost_Object x1, y1, x2, y2; */
    Xpost_Object drawline;
    Xpost_Object fillrect;
    int usefillrect;
    struct point *points;
    struct band_span *spans;
    int nspans, spancap;
    int i;
    //int width;

    //printf("_fillpoly\n");

    //width = xpost_dict_get(ctx, devdic, namewidth).int_.val;
    colorspace = xpost_dict_get(ctx, devdic, namenativecolorspace);
    if (xpost_dict_compare_objects(ctx, colorspace, nameDeviceGray) == 0)
    {
        ncomp = 1;
        comp1 = xpost_stack_pop(ctx->lo, ctx->os);
    }
    else if (xpost_dict_compare_objects(ctx, colorspace, nameDeviceRGB) == 0)
    {
        ncomp = 3;
        comp3 = xpost_stack_pop(ctx->lo, ctx->os);
        comp2 = xpost_stack_pop(ctx->lo, ctx->os);
        comp1 = xpost_stack_pop(ctx->lo, ctx->os);
    }
    else
    {
        XPOST_LOG_ERR("unimplemented device color space");
        return unregistered;
    }

    /* extract polygon vertices from ps array;
       null elements separate subpaths */
    points = malloc(poly.comp_.sz * sizeof *points);
    if (!points)
        return VMerror;
    for (i = 0; i < poly.comp_.sz; i++)
    {
        Xpost_Object pair, x, y;

        pair = xpost_array_get(ctx, poly, i);
        if (xpost_object_get_type(pair) != arraytype)
        {
            points[i].x = SUBPATH_BREAK;
            points[i].y = SUBPATH_BREAK;
            continue;
        }
        x = xpost_array_get(ctx, pair, 0);
        y = xpost_array_get(ctx, pair, 1);
        if (xpost_object_get_type(x) == integertype)
            x = xpost_real_cons((real)x.int_.val);
        if (xpost_object_get_type(y) == integertype)
            y = xpost_real_cons((real)y.int_.val);
        /* quantize to a 1/256 pixel device grid: geometry meant to lie
           on a pixel boundary arrives with accumulated float noise, and
           unsnapped it would classify to the wrong side of the boundary */
        points[i].x = (real)(floor(x.real_.val * 256.0 + 0.5) / 256.0);
        points[i].y = (real)(floor(y.real_.val * 256.0 + 0.5) / 256.0);
    }

    /* Scan-convert under the any-part-of-pixel rule (PLRM 7.5): a
       pixel is painted when the filled region meets its interior.
       Device space divides into unit pixel-row bands (row b covers
       b <= y < b+1). Each subpath boundary is cut into y-monotone
       chains -- walking from a least-y vertex, so a chain never wraps
       the start/end seam -- and each chain deposits, for every band it
       passes through, the x extent of its passage tagged with its y
       direction. Horizontal travel widens the open extent, except
       travel exactly on a band boundary, which meets no band interior
       (an integer-aligned bottom edge must not leak into the band
       below). Sorting each band's extents by left edge and
       accumulating winding numbers then yields the fill spans. */
    spans = NULL;
    nspans = 0;
    spancap = 0;
    i = 0;
    for (;;)
    {
        int s0, nv, base, k;
        int dirn, ib, code;
        real lo, hi, submin, submax;

        while (i < poly.comp_.sz && points[i].x == SUBPATH_BREAK)
            i++;
        if (i == poly.comp_.sz)
            break;
        s0 = i;
        while (i < poly.comp_.sz && points[i].x != SUBPATH_BREAK)
            i++;
        nv = i - s0;

        base = 0;
        for (k = 1; k < nv; k++)
            if (points[s0 + k].y < points[s0 + base].y)
                base = k;

        /* chain state: the open extent, its band, and its direction
           (0 until the first non-horizontal edge; starting at a
           least-y vertex the first direction can only be upward) */
        dirn = 0;
        ib = (int)floor(points[s0 + base].y);
        lo = hi = points[s0 + base].x;
        submin = submax = lo;
        code = 0;

        for (k = 0; k < nv && code == 0; k++)
        {
            struct point P = points[s0 + (base + k) % nv];
            struct point Q = points[s0 + (base + k + 1) % nv];
            int d, eb;

            if (Q.x < submin) submin = Q.x;
            if (Q.x > submax) submax = Q.x;

            if (P.y == Q.y)
            {
                if (P.y == (real)floor(P.y))
                {
                    /* on a band boundary: deposits nothing; until the
                       chain has a direction just track the position */
                    if (dirn == 0)
                        lo = hi = Q.x;
                }
                else
                {
                    if (Q.x < lo) lo = Q.x;
                    if (Q.x > hi) hi = Q.x;
                }
                continue;
            }

            d = Q.y > P.y ? 1 : -1;
            /* the band this edge starts in: a start exactly on a band
               boundary belongs to the band ahead of travel */
            eb = (int)floor(P.y);
            if (d < 0 && (real)eb == P.y)
                eb--;

            if (d != dirn)
            {
                /* direction reversal: the vertex row holds two passages */
                if (dirn != 0)
                {
                    code = _span_push(&spans, &spancap, &nspans, ib, dirn, lo, hi);
                    lo = hi = P.x;
                }
                dirn = d;
                ib = eb;
            }
            else if (eb != ib)
            {
                /* the previous edge ended exactly on our starting boundary */
                code = _span_push(&spans, &spancap, &nspans, ib, dirn, lo, hi);
                lo = hi = P.x;
                ib = eb;
            }

            /* walk the edge band to band, cutting at each boundary */
            while (code == 0)
            {
                real yb = (real)(d > 0 ? ib + 1 : ib);

                if (d > 0 ? Q.y > yb : Q.y < yb)
                {
                    real xb = P.x + (Q.x - P.x) * ((yb - P.y) / (Q.y - P.y));

                    if (xb < lo) lo = xb;
                    if (xb > hi) hi = xb;
                    code = _span_push(&spans, &spancap, &nspans, ib, dirn, lo, hi);
                    ib += d;
                    lo = hi = xb;
                }
                else
                {
                    if (Q.x < lo) lo = Q.x;
                    if (Q.x > hi) hi = Q.x;
                    break;
                }
            }
        }

        if (code == 0)
        {
            if (dirn != 0)
                code = _span_push(&spans, &spancap, &nspans, ib, dirn, lo, hi);
            else
            {
                /* no vertical travel at all: the subpath still meets its
                   row; deposit a balanced pair over its whole x extent */
                code = _span_push(&spans, &spancap, &nspans, ib, 1, submin, submax);
                if (code == 0)
                    code = _span_push(&spans, &spancap, &nspans, ib, -1, submin, submax);
            }
        }
        if (code)
        {
            free(points);
            free(spans);
            return code;
        }
    }

    /* nspans can be zero for a degenerate row, leaving spans NULL; passing a
       null pointer to qsort is undefined even for a zero count, and there is
       nothing to order below two spans anyway */
    if (nspans > 1)
        qsort(spans, nspans, sizeof *spans, _bandspancomp);

    /* A fill scanline is a horizontal span. When the device provides a
       compiled FillRect, render each span through it (the per-pixel plotting
       then happens in C rather than a PostScript DrawLine/PutPix loop);
       otherwise fall back to DrawLine unchanged. Both take the same colour
       components plus four numbers, so the loop body and colour roll below are
       identical either way. */
    fillrect = xpost_dict_get(ctx, devdic, nameFillRect);
    usefillrect = xpost_object_get_type(fillrect) == operatortype;

    /* Walk each band accumulating winding: a span opens at the first
       extent's left edge and closes where the winding count returns to
       zero (or the band runs out), covering the rightmost extent seen.
       Paint columns [floor(lo), ceil(hi)): every pixel whose interior
       the span reaches, and exactly the geometry when the span lies on
       pixel boundaries. FillRect fills the inclusive box [x, x+w] on
       row y (a fill span is height 0); DrawLine plots from its first
       point (included) toward its second (excluded); both therefore
       cover [xlo, xhi-1]. */
    numlines = 0;
    {
        int s = 0;

        while (s < nspans)
        {
            int b = spans[s].band;
            int wind = 0;
            real L = spans[s].lo, R = spans[s].hi;
            integer xlo, xhi;

            do
            {
                if (spans[s].hi > R)
                    R = spans[s].hi;
                wind += spans[s].dirn;
                s++;
            } while (wind != 0 && s < nspans && spans[s].band == b);

            xlo = (integer)floor(L);
            xhi = (integer)ceil(R);
            if (xhi <= xlo)
                continue;
            if (usefillrect)
            {
                xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons(xlo));
                xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons(b));
                xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons(xhi - xlo - 1));
                xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons(0)); /* h */
            }
            else
            {
                xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons(xlo));
                xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons(b));
                xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons(xhi));
                xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons(b));
            }
            numlines++;
        }
    }

    /*call the device's DrawLine generically with continuations.
      each call to DrawLine looks like this

         comp1 (comp2 comp3)? x1 y1 x2 y2 DEVICE >-- DrawLine

     So what we'll do is push all the points on the stack */

    /*for each line: */
    /*
        xpost_stack_push(ctx->lo, ctx->os, x1);
        xpost_stack_push(ctx->lo, ctx->os, y1);
        xpost_stack_push(ctx->lo, ctx->os, x2);
        xpost_stack_push(ctx->lo, ctx->os, y2);
    */

    /*the loop body and continuation are built from operator objects,
     not executable names, so a user definition of /roll or /repeat on
     the dict stack cannot capture them mid-fill */
    /*then we'll use a repeat loop to call DrawLine
     on each set of 4 numbers. But in order to treat the color space
     generically, we construct the loop body dynamically. */

    /*first push the number of elements
     remember we're using a repeat loop which looks like:
         count proc  -repeat-
     so this line places the `count` parameter on the stack
    */
    xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons(numlines));

    /*then push a mark object to begin array construction
     this array is our loop body */
    xpost_stack_push(ctx->lo, ctx->os, mark);

    /*the loop body finds the 4 coordinate numbers on the stack
     and must roll the color values beneath these numbers on the stack  */

    switch (ncomp)
    {
        case 1:
            xpost_stack_push(ctx->lo, ctx->os, comp1);
            xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons(5)); /* total elements to roll */
            xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons(1)); /* color components to move */
            break;
        case 3:
            xpost_stack_push(ctx->lo, ctx->os, comp1);
            xpost_stack_push(ctx->lo, ctx->os, comp2);
            xpost_stack_push(ctx->lo, ctx->os, comp3);
            xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons(7)); /* total elements to roll */
            xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons(3)); /* color components to move */
            break;
    }
    xpost_stack_push(ctx->lo, ctx->os, xpost_operator_cons(ctx, "roll", NULL, 0, 0));

      /*at this point (in constructing the (color-space-generic) loop-body) we have the desired stack picture:

             comp1 (comp2 comp3)? x1 y1 x2 y2

        (with possibly more pairs deeper on the stack, waiting for the next iteration),
        just need to push the devdic (ie. the DEVICE object, in OO-speak) and DrawLine,
        then cinch-off the loop-body procedure (array), make it executable, and call
        the `repeat` operator.
       */

    xpost_stack_push(ctx->lo, ctx->os, devdic);
    if (usefillrect)
    {
        xpost_stack_push(ctx->lo, ctx->os, fillrect);
    }
    else
    {
        drawline = xpost_dict_get(ctx, devdic, nameDrawLine);
        xpost_stack_push(ctx->lo, ctx->os, drawline);

        /*if drawline is a procedure, we also need to call exec */
        if (xpost_object_get_type(drawline) == arraytype)
            xpost_stack_push(ctx->lo, ctx->os, xpost_operator_cons(ctx, "exec", NULL, 0, 0));
    }

    /*--the rest of the code here calls-back to postscript (by "continuation")
        by pushing executable names on the execution-stack, and then returns.
        The (color-space-) generic loop-body is called with the
        `repeat` looping-operator.-------------------------------------------*/

    /*Then construct the loop-body procedure array. Just showing you the line here.
      Read the whole story-line of comments for why we're not just executing it here. */
       //xpost_stack_push(ctx->lo, ctx->es, xpost_object_cvx(xpost_name_cons(ctx, "]")));

    /*Then, after the loop-body array is constructed, we need to call cvx on it. */
       //xpost_stack_push(ctx->lo, ctx->es, xpost_object_cvx(xpost_name_cons(ctx, "cvx")));
    /*"after" means this line, which pushes on the stack, goes *before* the xpost_name_cons("]") line.
     I'll summarize this part again. */

    /*After this, we call `repeat` and we're done. */
        //xpost_stack_push(ctx->lo, ctx->es, xpost_object_cvx(xpost_name_cons(ctx, "repeat")));

    /*Again since these are scheduled on a stack, we need to push them in reverse order
      from the order in which we desire them to execute.
      What we're doing is:

      opstack> xyxy xyxy xyxy ... xyxy numlines [ comp1 5 1 roll DEVICE DrawLine (exec)?
      -or for rgb color values-:
                                   ... numlines [ comp1 comp2 comp3 7 3 roll DEVICE DrawLine (exec)?
      execstack> repeat cvx ]
                            ^ construct array
                         ^ make executable
                   ^ call the loop operator

      So the sequence in C is:
     */

    xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons(ctx, "repeat", NULL, 0, 0));
    xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons(ctx, "cvx", NULL, 0, 0));
    xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons(ctx, "]", NULL, 0, 0));

    /*performance could be increased by factoring-out calls to xpost_name_cons()  ... DONE!
      or using opcode shortcuts for Rbracket & cvx (or just the arrtomark() function) and repeat.
     */
    free(points);
    free(spans);
    return 0;
}

/* Fast FillRect for grayscale (DeviceGray) array-of-strings devices such as
   PGMIMAGE. Writes the ImgData row strings directly rather than looping over
   PutPix in PostScript; erasepage clears the whole page through FillRect, so
   the per-pixel interpreter overhead otherwise dominates page emission.
   Mirrors PGMIMAGE's FillRect/PutPix handling exactly: value scaled by 255 and
   truncated to a byte, coordinates floored, negative extents normalised,
   inclusive end coordinates, and bounds clipping (rows via ImgData length,
   columns via each row string's length). */
static
int _fillrectgray(Xpost_Context *ctx,
                  Xpost_Object val,
                  Xpost_Object x,
                  Xpost_Object y,
                  Xpost_Object w,
                  Xpost_Object h,
                  Xpost_Object devdic)
{
    Xpost_Object imgdata, row;
    double dx, dy, dw, dh;
    int height, iy, iy0, iy1, ix0, ix1;
    unsigned char b;

    imgdata = xpost_dict_get(ctx, devdic, nameImgData);
    if (xpost_object_get_type(imgdata) != arraytype)
        return undefined;
    height = imgdata.comp_.sz;

    /* value -> byte, matching PGMIMAGE PutPix "255 mul cvi put" */
    b = (unsigned char)(int)((xpost_object_get_type(val) == realtype
                              ? val.real_.val : (double)val.int_.val) * 255.0);

    dx = xpost_object_get_type(x) == realtype ? x.real_.val : (double)x.int_.val;
    dy = xpost_object_get_type(y) == realtype ? y.real_.val : (double)y.int_.val;
    dw = xpost_object_get_type(w) == realtype ? w.real_.val : (double)w.int_.val;
    dh = xpost_object_get_type(h) == realtype ? h.real_.val : (double)h.int_.val;

    /* normalise negative extents, then form inclusive end coords */
    if (dw < 0) { dw = -dw; dx -= dw; }
    if (dh < 0) { dh = -dh; dy -= dh; }
    ix0 = (int)floor(dx);
    iy0 = (int)floor(dy);
    ix1 = (int)floor(dx + dw);
    iy1 = (int)floor(dy + dh);

    /* clip rows to the device */
    if (iy0 < 0) iy0 = 0;
    if (iy1 > height - 1) iy1 = height - 1;

    for (iy = iy0; iy <= iy1; iy++)
    {
        int width, cx0, cx1;
        row = xpost_array_get(ctx, imgdata, iy);
        width = row.comp_.sz;
        cx0 = ix0 < 0 ? 0 : ix0;
        cx1 = ix1 > width - 1 ? width - 1 : ix1;
        if (cx0 <= cx1)
            memset(xpost_string_get_pointer(ctx, row) + cx0, b,
                   (size_t)(cx1 - cx0 + 1));
    }

    return 0;
}

/* Deflate the concatenation of an array of strings, returning the result as an
   array of <=65535-byte strings (the PostScript string limit) plus a boolean
   that is true when compression happened. Used by the pdfwrite device to write
   a FlateDecode content stream. Without zlib the input is returned unchanged
   with false, so the caller falls back to uncompressed output. */
static
int _flatecompress(Xpost_Context *ctx, Xpost_Object arr)
{
#ifdef HAVE_ZLIB
    z_stream strm;
    unsigned char *out = NULL;
    size_t outlen = 0, outcap = 0;
    unsigned char buf[16384];
    Xpost_Object result;
    int i, n, ret;

    memset(&strm, 0, sizeof strm);
    if (deflateInit(&strm, Z_DEFAULT_COMPRESSION) != Z_OK)
        return unregistered;

    n = arr.comp_.sz;
    for (i = 0; i <= n; i++)   /* the final pass (i == n) flushes */
    {
        int flush = (i == n) ? Z_FINISH : Z_NO_FLUSH;
        if (i < n)
        {
            Xpost_Object s = xpost_array_get(ctx, arr, i);
            strm.next_in = (unsigned char *)xpost_string_get_pointer(ctx, s);
            strm.avail_in = s.comp_.sz;
        }
        else
        {
            strm.next_in = NULL;
            strm.avail_in = 0;
        }
        do
        {
            size_t have;
            strm.next_out = buf;
            strm.avail_out = sizeof buf;
            ret = deflate(&strm, flush);
            if (ret == Z_STREAM_ERROR)
            {
                free(out);
                deflateEnd(&strm);
                return unregistered;
            }
            have = sizeof buf - strm.avail_out;
            if (outlen + have > outcap)
            {
                unsigned char *tmp;
                outcap = (outlen + have) * 2 + 64;
                tmp = realloc(out, outcap);
                if (!tmp)
                {
                    free(out);
                    deflateEnd(&strm);
                    return VMerror;
                }
                out = tmp;
            }
            if (have)
                memcpy(out + outlen, buf, have);
            outlen += have;
        } while (strm.avail_out == 0);
    }
    deflateEnd(&strm);

    {
        size_t pos = 0;
        int nchunks = (int)((outlen + 65534) / 65535);
        if (nchunks == 0)
            nchunks = 1;
        result = xpost_object_cvlit(xpost_array_cons(ctx, nchunks));
        for (i = 0; i < nchunks; i++)
        {
            size_t chunk = outlen - pos;
            if (chunk > 65535)
                chunk = 65535;
            /* cvlit: strings and arrays are executable by default, and this
               binary content must be written, not executed */
            xpost_array_put(ctx, result, i,
                            xpost_object_cvlit(
                                xpost_string_cons(ctx, chunk, (char *)(out + pos))));
            pos += chunk;
        }
    }
    free(out);
    xpost_stack_push(ctx->lo, ctx->os, result);
    xpost_stack_push(ctx->lo, ctx->os, xpost_bool_cons(1));
    return 0;
#else
    xpost_stack_push(ctx->lo, ctx->os, arr);
    xpost_stack_push(ctx->lo, ctx->os, xpost_bool_cons(0));
    return 0;
#endif
}

/* write a decimal integer, returning its length */
static int _pdf_fmt_long(char *o, long v)
{
    char t[24];
    int n = 0, neg = 0, len = 0;
    if (v < 0) { neg = 1; v = -v; }
    if (v == 0) t[n++] = '0';
    while (v) { t[n++] = (char)('0' + (v % 10)); v /= 10; }
    if (neg) o[len++] = '-';
    while (n) o[len++] = t[--n];
    return len;
}

/* write a PDF number: an integer when integral, else two decimals (never
   exponential). round(v*100) avoids binary-float print noise. Matches the
   .pdfnum PostScript helper used by the other pdfwrite methods. */
static int _pdf_fmt_num(char *o, double v)
{
    if (v == trunc(v))
        return _pdf_fmt_long(o, (long)v);
    else
    {
        long m = (long)round(v * 100.0);
        long ip, fp;
        int len = 0;
        if (m < 0) { o[len++] = '-'; m = -m; }
        ip = m / 100;
        fp = m % 100;
        len += _pdf_fmt_long(o + len, ip);
        o[len++] = '.';
        o[len++] = (char)('0' + fp / 10);
        o[len++] = (char)('0' + fp % 10);
        return len;
    }
}

/* pdfwrite content accumulator. Held in the device's /Private string and grown
   with malloc/realloc, so the accumulated content lives outside the
   save/restore-managed memory file (like the raster device's pixel buffer) and
   survives a `restore` executed by the job before showpage/Emit. The current
   page's marks are not part of virtual memory, so `restore` must not discard
   them; storing them in the device dict would let it. */
typedef struct
{
    char *data;
    size_t len;
    size_t cap;
} Pdf_Acc;

static int _pdf_acc_append(Pdf_Acc *a, const char *s, size_t n)
{
    if (a->len + n > a->cap)
    {
        size_t nc = a->cap ? a->cap : 4096;
        char *nd;
        while (nc < a->len + n)
            nc *= 2;
        nd = (char *)realloc(a->data, nc);
        if (!nd)
            return 0;
        a->data = nd;
        a->cap = nc;
    }
    memcpy(a->data + a->len, s, n);
    a->len += n;
    return 1;
}

/* Load/store the accumulator struct via the device's /Private string. The raw
   memory accessors record no save/restore backup, so neither the struct nor the
   malloc'd buffer it points at is reverted by `restore`; the pointer is set once
   at device creation and never re-homed into virtual memory. */
static int _pdf_acc_get(Xpost_Context *ctx, Xpost_Object devdic,
                        Xpost_Object *priv, Pdf_Acc *a)
{
    *priv = xpost_dict_get(ctx, devdic, namepdfPrivate);
    if (xpost_object_get_type(*priv) != stringtype)
        return 0;
    xpost_memory_get(xpost_context_select_memory(ctx, *priv),
                     xpost_object_get_ent(*priv), 0, sizeof(*a), a);
    return 1;
}

static void _pdf_acc_put(Xpost_Context *ctx, Xpost_Object priv, Pdf_Acc *a)
{
    xpost_memory_put(xpost_context_select_memory(ctx, priv),
                     xpost_object_get_ent(priv), 0, sizeof(*a), a);
}

/* Create the content accumulator and stash it in the device's /Private. Called
   from the device Create method, before any user save/restore. */
static int _pdfinit(Xpost_Context *ctx, Xpost_Object devdic)
{
    Pdf_Acc a;
    Xpost_Object priv;

    a.data = (char *)malloc(4096);
    a.len = 0;
    a.cap = a.data ? 4096 : 0;
    priv = xpost_object_cvlit(xpost_string_cons(ctx, sizeof(a), NULL));
    _pdf_acc_put(ctx, priv, &a);
    xpost_dict_put(ctx, devdic, namepdfPrivate, priv);
    return 0;
}

/* append a string's bytes to the accumulator (the marking methods' .put) */
static int _pdfput(Xpost_Context *ctx, Xpost_Object str, Xpost_Object devdic)
{
    Pdf_Acc a;
    Xpost_Object priv;

    if (!_pdf_acc_get(ctx, devdic, &priv, &a))
        return undefined;
    if (!_pdf_acc_append(&a, (char *)xpost_string_get_pointer(ctx, str), str.comp_.sz))
        return VMerror;
    _pdf_acc_put(ctx, priv, &a);
    return 0;
}

/* Emit the content-stream operators for a filled path into the accumulator: the
   colour ("r g b rg"), the flattened subpaths ("x y m" / "x y l", closed with
   "h"), and an even-odd fill ("f*"). This is the per-coordinate hot loop of the
   pdfwrite FillPoly, in C. */
static int _pdffillpoly(Xpost_Context *ctx,
                        Xpost_Object r, Xpost_Object g, Xpost_Object b,
                        Xpost_Object poly, Xpost_Object devdic)
{
#define PDFNUMVAL(o) (xpost_object_get_type(o) == realtype ? (o).real_.val \
                                                           : (double)(o).int_.val)
    Pdf_Acc a;
    Xpost_Object priv;
    char tmp[96];
    int i, n, len, needmove = 1;

    if (!_pdf_acc_get(ctx, devdic, &priv, &a))
        return undefined;

    len = 0;
    len += _pdf_fmt_num(tmp + len, PDFNUMVAL(r)); tmp[len++] = ' ';
    len += _pdf_fmt_num(tmp + len, PDFNUMVAL(g)); tmp[len++] = ' ';
    len += _pdf_fmt_num(tmp + len, PDFNUMVAL(b));
    memcpy(tmp + len, " rg\n", 4); len += 4;
    _pdf_acc_append(&a, tmp, len);

    n = poly.comp_.sz;
    for (i = 0; i < n; i++)
    {
        Xpost_Object e = xpost_array_get(ctx, poly, i);
        if (xpost_object_get_type(e) == arraytype && e.comp_.sz == 2)
        {
            double x = PDFNUMVAL(xpost_array_get(ctx, e, 0));
            double y = PDFNUMVAL(xpost_array_get(ctx, e, 1));
            len = 0;
            len += _pdf_fmt_num(tmp + len, x); tmp[len++] = ' ';
            len += _pdf_fmt_num(tmp + len, y); tmp[len++] = ' ';
            tmp[len++] = needmove ? 'm' : 'l';
            tmp[len++] = '\n';
            needmove = 0;
            _pdf_acc_append(&a, tmp, len);
        }
        else if (!needmove)   /* null subpath separator: close the subpath */
        {
            _pdf_acc_append(&a, "h\n", 2);
            needmove = 1;
        }
    }
    if (!needmove)
        _pdf_acc_append(&a, "h\n", 2);
    _pdf_acc_append(&a, "f*\n", 3);

    _pdf_acc_put(ctx, priv, &a);
    return 0;
#undef PDFNUMVAL
}

/* Return the accumulated content as an array of <=65535-byte strings (the
   PostScript string limit) for the Emit method to compress and write. The
   malloc'd source buffer is stable across the string allocations. */
static int _pdfchunks(Xpost_Context *ctx, Xpost_Object devdic)
{
    Pdf_Acc a;
    Xpost_Object priv, result;
    size_t pos = 0;
    int nchunks, i;

    if (!_pdf_acc_get(ctx, devdic, &priv, &a))
        return undefined;
    nchunks = (int)((a.len + 65534) / 65535);
    if (nchunks == 0)
        nchunks = 1;
    result = xpost_object_cvlit(xpost_array_cons(ctx, nchunks));
    for (i = 0; i < nchunks; i++)
    {
        size_t chunk = a.len - pos;
        if (chunk > 65535)
            chunk = 65535;
        xpost_array_put(ctx, result, i,
                        xpost_object_cvlit(
                            xpost_string_cons(ctx, chunk, a.data + pos)));
        pos += chunk;
    }
    xpost_stack_push(ctx->lo, ctx->os, result);
    return 0;
}

/* free the accumulator's malloc'd buffer (device Destroy) */
static int _pdffree(Xpost_Context *ctx, Xpost_Object devdic)
{
    Pdf_Acc a;
    Xpost_Object priv;

    if (!_pdf_acc_get(ctx, devdic, &priv, &a))
        return 0;
    free(a.data);
    a.data = NULL;
    a.len = 0;
    a.cap = 0;
    _pdf_acc_put(ctx, priv, &a);
    return 0;
}

int xpost_oper_init_generic_device_ops(Xpost_Context *ctx,
                                       Xpost_Object sd)
{
    unsigned int optadr;
    Xpost_Operator *optab;
    Xpost_Object n,op;

    xpost_memory_table_get_addr(ctx->gl,
                                XPOST_MEMORY_TABLE_SPECIAL_OPERATOR_TABLE,
                                &optadr);
    optab = (Xpost_Operator *)(ctx->gl->base + optadr);

    op = xpost_operator_cons(ctx, ".yxsort", (Xpost_Op_Func)_yxsort, 0, 1, arraytype); INSTALL;
    op = xpost_operator_cons(ctx, ".fillpoly", (Xpost_Op_Func)_fillpoly, 0, 2, arraytype, dicttype); INSTALL;
    op = xpost_operator_cons(ctx, ".fillrectgray", (Xpost_Op_Func)_fillrectgray, 0, 6,
            numbertype, numbertype, numbertype, numbertype, numbertype, dicttype); INSTALL;
    op = xpost_operator_cons(ctx, ".flatecompress", (Xpost_Op_Func)_flatecompress, 2, 1, arraytype); INSTALL;
    op = xpost_operator_cons(ctx, ".pdffillpoly", (Xpost_Op_Func)_pdffillpoly, 0, 5,
            numbertype, numbertype, numbertype, arraytype, dicttype); INSTALL;
    op = xpost_operator_cons(ctx, ".pdfinit", (Xpost_Op_Func)_pdfinit, 0, 1, dicttype); INSTALL;
    op = xpost_operator_cons(ctx, ".pdfput", (Xpost_Op_Func)_pdfput, 0, 2, stringtype, dicttype); INSTALL;
    op = xpost_operator_cons(ctx, ".pdfchunks", (Xpost_Op_Func)_pdfchunks, 1, 1, dicttype); INSTALL;
    op = xpost_operator_cons(ctx, ".pdffree", (Xpost_Op_Func)_pdffree, 0, 1, dicttype); INSTALL;
    if (xpost_object_get_type((namewidth = xpost_name_cons(ctx, "width"))) == invalidtype)
        return VMerror;
    if (xpost_object_get_type((nameImgData = xpost_name_cons(ctx, "ImgData"))) == invalidtype)
        return VMerror;
    if (xpost_object_get_type((nameFillRect = xpost_name_cons(ctx, "FillRect"))) == invalidtype)
        return VMerror;
    if (xpost_object_get_type((namepdfPrivate = xpost_name_cons(ctx, "Private"))) == invalidtype)
        return VMerror;
    if (xpost_object_get_type((namenativecolorspace = xpost_name_cons(ctx, "nativecolorspace"))) == invalidtype)
        return VMerror;
    if (xpost_object_get_type((nameDeviceGray = xpost_name_cons(ctx, "DeviceGray"))) == invalidtype)
        return VMerror;
    if (xpost_object_get_type((nameDeviceRGB = xpost_name_cons(ctx, "DeviceRGB"))) == invalidtype)
        return VMerror;
    if (xpost_object_get_type((nameroll = xpost_name_cons(ctx, "roll"))) == invalidtype)
        return VMerror;
    if (xpost_object_get_type((nameDrawLine = xpost_name_cons(ctx, "DrawLine"))) == invalidtype)
        return VMerror;
    if (xpost_object_get_type((nameexec = xpost_name_cons(ctx, "exec"))) == invalidtype)
        return VMerror;
    if (xpost_object_get_type((namerepeat = xpost_name_cons(ctx, "repeat"))) == invalidtype)
        return VMerror;
    if (xpost_object_get_type((namecvx = xpost_name_cons(ctx, "cvx"))) == invalidtype)
        return VMerror;
    if (xpost_object_get_type((nameRbracket = xpost_name_cons(ctx, "]"))) == invalidtype)
        return VMerror;

    return 0;
}
