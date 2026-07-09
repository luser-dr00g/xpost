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

/*
   feq is applied to determine if two pixel coordinates
   are "close enough" to be considered equal.
   It is used to reject cases in _intersect,
   and to control the checking of both coordinates
   when sorting (x,y) pairs in a y|x sort.
   These values are device-space points derived from user-input,
   so they are ultimately quantized to integers to address the raster,
   but here we consider them quantized to a small fraction of unity,
   somewhere between 1 and the true floating-point epsilon.
 */
#ifdef _WANT_LARGE_OBJECT
# define PIXEL_TOLERANCE 0.0001
#else
# define PIXEL_TOLERANCE 0.0001f
#endif

static inline
int feq(real dif)
{
#ifdef _WANT_LARGE_OBJECT
    if (fabs(dif) < PIXEL_TOLERANCE)
        return 1;
#else
    if (fabsf(dif) < PIXEL_TOLERANCE)
        return 1;
#endif
    return 0;
}

static
int _intersect(real ax, real ay,  real bx, real by,
               real cx, real cy,  real dx, real dy,
               real *rx, real *ry)
{
    real distAB;
    real theCos;
    real theSin;
    real newX;
    real ABpos;

    //printf("%f %f  %f %f  %f %f  %f %f\n",
    //        ax, ay,  bx, by,  cx, cy,  dx, dy);

    /* reject degenerate line */
    if ((feq(ax - bx) && feq(ay - by)) ||
        (feq(cx - dx) && feq(cy - dy)))
    {
        return 0;
        /*
        if (ax == cx && ay == cy && ax != 0.0 && ay != 0.0)
        {
            *rx = ax;
            *ry = ay;
            return 1;
        }
        */
    }

    /* reject coinciding endpoints */
    if ((feq(ax - cx) && feq(ay - cy)) ||
        (feq(bx - cx) && feq(by - cy)) ||
        (feq(ax - dx) && feq(ay - dy)) ||
        (feq(bx - dx) && feq(by - dy)))
    {
        return 0;
        /*
        *rx = ax;
        *ry = ay;
        return 1;
        */
    }

    /* translate by -ax, -ay */
    bx -= ax;  by -= ay;
    cx -= ax;  cy -= ay;
    dx -= ax;  dy -= ay;

    distAB = (real)sqrt(bx * bx + by * by);

    /* rotate AB to x-axis */
    theCos = bx / distAB;
    theSin = by / distAB;
    newX = cx * theCos + cy * theSin;
    cy = cy * theCos - cx * theSin;
    cx = newX;
    newX = dx * theCos + dy * theSin;
    dy = dy * theCos - dx * theSin;
    dx = newX;

    /* no intersection */
    if (((cy < 0) && (dy < 0)) || ((cy > 0) && (dy > 0)))
        return 0;

    if (feq(dy - cy)) return 0;

    ABpos = dx + ((cx - dx) * dy) / (dy - cy);
    if ((ABpos < 0) || (ABpos > distAB))
        return 0;

    *rx = ax + ABpos * theCos;
    *ry = ay + ABpos * theSin;

    XPOST_LOG_INFO(">< %f %f", *rx, *ry);

    return 1;
}

static
int _cyxcomp (const void *left, const void *right)
{
    const struct point *lt = left;
    const struct point *rt = right;

    if (feq(lt->y - rt->y))
    {
        if (lt->x < rt->x)
        {
            return 1;
        }
        else if (lt->x > rt->x)
        {
            return -1;
        }
        else
        {
            return 0;
        }
    }
    else
    {
        if (lt->y < rt->y)
            return -1;
        else
            return 1;
    }
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
    struct point *points, *intersections, *tmp;
    int i, j;
    int cap;
    real yscan;
    real minx = (real)0x7ffffff;
    real miny = minx;
    real maxx = -minx;
    real maxy = maxx;
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

        //points[i].x = x.real_.val;
        //points[i].y = y.real_.val;
        points[i].x = (real)floor(x.real_.val + 0.5);
        points[i].y = (real)floor(y.real_.val + 0.5);
    }

    /* find bounding box */
    for (i = 0; i < poly.comp_.sz; i++)
    {
        if (points[i].x == SUBPATH_BREAK)
            continue;
        if (points[i].x < minx)
            minx = points[i].x;
        if (points[i].x > maxx)
            maxx = points[i].x;
        if (points[i].y < miny)
            miny = points[i].y;
        if (points[i].y > maxy)
            maxy = points[i].y;
    }

    /* a complex polygon may cross a scanline many times; grow as needed */
    cap = 4 * ((int)(maxy - miny) + 1);
    intersections = calloc(cap, sizeof *intersections);
    if (!intersections)
    {
        free(points);
        return VMerror;
    }

    /* intersect polygon edges with scanlines */
    for (i = 0, j = 0; i < poly.comp_.sz - 1; i++)
    {
        real rx, ry;

        if (points[i].x == SUBPATH_BREAK || points[i+1].x == SUBPATH_BREAK)
            continue;
        for (yscan = (real)(miny + 0.5); yscan < maxy; yscan += 1.0)
        {
            if (_intersect(points[i].x, points[i].y,
                           points[i+1].x, points[i+1].y,
                           (real)(minx - 0.5), yscan,
                           (real)(maxx + 0.5), yscan,
                           &rx, &ry))
            {
                if (j == cap)
                {
                    cap *= 2;
                    tmp = realloc(intersections, cap * sizeof *intersections);
                    if (!tmp)
                    {
                        free(points);
                        free(intersections);
                        return VMerror;
                    }
                    intersections = tmp;
                }
                intersections[j].x = rx;
                intersections[j].y = ry;
                j++;
            }
        }
    }
    numlines = j / 2;

    /* sort intersection points */
    qsort(intersections, j, sizeof *intersections, _cyxcomp);

    /* A fill scanline is a horizontal span. When the device provides a
       compiled FillRect, render each span through it (the per-pixel plotting
       then happens in C rather than a PostScript DrawLine/PutPix loop);
       otherwise fall back to DrawLine unchanged. Both take the same colour
       components plus four numbers, so the loop body and colour roll below are
       identical either way. */
    fillrect = xpost_dict_get(ctx, devdic, nameFillRect);
    usefillrect = xpost_object_get_type(fillrect) == operatortype;

    if (usefillrect)
    {
        /* DrawLine plots a span from its first point (included) toward its
           second (excluded), so it covers |x1-x2| pixels: [x1, x2-1] when
           x1 < x2, or [x2+1, x1] when x1 > x2, and nothing when x1 == x2.
           FillRect fills the inclusive box [x, x+w] on row y (a fill span is
           height 0), so pass the low end as x and |x1-x2|-1 as w, dropping
           empty spans, to match DrawLine's pixels exactly. */
        int emitted = 0;
        for (i = 0; i < numlines * 2; i += 2)
        {
            integer x1 = (integer)floor(intersections[i].x);
            integer y1 = (integer)floor(intersections[i].y);
            integer x2 = (integer)floor(intersections[i+1].x);
            integer xlo;
            if (x1 == x2)
                continue;
            xlo = x1 < x2 ? x1 : x2 + 1;
            xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons(xlo));
            xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons(y1));
            xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons((x1 < x2 ? x2 - x1 : x1 - x2) - 1)); /* w */
            xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons(0));                                 /* h */
            emitted++;
        }
        numlines = emitted;
    }
    else
    {
        /* arrange ((x1,y1),(x2,y2)) pairs */
        for (i = 0; i < numlines * 2; i += 2)
        {
            xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons((integer)floor(intersections[i].x)));
            xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons((integer)floor(intersections[i].y)));
            xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons((integer)floor(intersections[i+1].x)));
            xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons((integer)floor(intersections[i+1].y)));
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
    xpost_stack_push(ctx->lo, ctx->os, xpost_object_cvx( nameroll));

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
            xpost_stack_push(ctx->lo, ctx->os, nameexec);
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

    xpost_stack_push(ctx->lo, ctx->es, xpost_object_cvx( namerepeat));
    xpost_stack_push(ctx->lo, ctx->es, xpost_object_cvx( namecvx));
    xpost_stack_push(ctx->lo, ctx->es, xpost_object_cvx( nameRbracket));

    /*performance could be increased by factoring-out calls to xpost_name_cons()  ... DONE!
      or using opcode shortcuts for Rbracket & cvx (or just the arrtomark() function) and repeat.
     */
    free(points);
    free(intersections);
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
    if (xpost_object_get_type((namewidth = xpost_name_cons(ctx, "width"))) == invalidtype)
        return VMerror;
    if (xpost_object_get_type((nameImgData = xpost_name_cons(ctx, "ImgData"))) == invalidtype)
        return VMerror;
    if (xpost_object_get_type((nameFillRect = xpost_name_cons(ctx, "FillRect"))) == invalidtype)
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
