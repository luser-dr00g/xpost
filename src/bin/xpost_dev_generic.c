/*
 * Xpost - a Level-2 Postscript interpreter
 * Copyright (C) 2013, Michael Joshua Ryan
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

#ifdef STDC_HEADERS
# include <stdlib.h>
# include <stddef.h>
#else
# ifdef HAVE_STDLIB_H
#  include <stdlib.h>
# endif
#endif
#ifdef HAVE_ALLOCA_H
# include <alloca.h>
#elif !defined alloca
# ifdef __GNUC__
#  define alloca __builtin_alloca
# elif defined _AIX
#  define alloca __alloca
# elif defined _MSC_VER
#  include <malloc.h>
#  define alloca _alloca
# elif !defined HAVE_ALLOCA
#  ifdef  __cplusplus
extern "C"
#  endif
void *alloca (size_t);
# endif
#endif

#include <assert.h>
#include <math.h>
#include <stdlib.h> /* abs */
#include <string.h>

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
#include "xpost_op_dict.h" /* call Aload operator for convenience */
#include "xpost_dev_generic.h" /* check prototypes */

struct point {
    real x, y;
};

/* FIXME: re-entrancy */
static
Xpost_Context *localctx;

static
int _yxcomp (const void *left, const void *right)
{
    const Xpost_Object *lt = left;
    const Xpost_Object *rt = right;
    Xpost_Object leftx, lefty, rightx, righty;
    integer ltx, lty, rtx, rty;
    leftx = barget(localctx, *lt, 0);
    lefty = barget(localctx, *lt, 1);
    rightx = barget(localctx, *rt, 0);
    righty = barget(localctx, *rt, 1);
    ltx = xpost_object_get_type(leftx) == realtype ?
        leftx.real_.val : leftx.int_.val;
    lty = xpost_object_get_type(lefty) == realtype ?
        lefty.real_.val : lefty.int_.val;
    rtx = xpost_object_get_type(rightx) == realtype ?
        rightx.real_.val : rightx.int_.val;
    rty = xpost_object_get_type(righty) == realtype ?
        righty.real_.val : righty.int_.val;
    if (lty == rty) {
        if (ltx < rtx) {
            return 1;
        } else if (ltx > rtx) {
            return -1;
        } else {
            return 0;
        }
    } else {
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

    //arrcontents = alloca(arr.comp_.sz * sizeof arr);
    //if (!xpost_memory_get(xpost_context_select_memory(ctx, arr),
    //            xpost_object_get_ent(arr), 0, arr.comp_.sz * sizeof arr, arrcontents))
    //    return VMerror;
    mem = xpost_context_select_memory(ctx, arr);
    if (!xpost_memory_table_get_addr(mem, xpost_object_get_ent(arr), &arradr))
        return VMerror;
    arrcontents = (mem->base + arradr);

    localctx = ctx;
    qsort(arrcontents, arr.comp_.sz, sizeof arr, _yxcomp);
    localctx = NULL;

    //if (!xpost_memory_put(xpost_context_select_memory(ctx, arr),
    //            xpost_object_get_ent(arr), 0, arr.comp_.sz * sizeof arr, arrcontents))
    //    return VMerror;

    return 0;
}

static
int feq (double left, double right) {
    if (fabs(left-right) < 0.00001)
        return 1;
    return 0;
}

static
int _intersect (double ax, double ay,  double bx, double by,
                double cx, double cy,  double dx, double dy,
                int *rx, int *ry)
{
    double distAB;
    double theCos;
    double theSin;
    double newX;
    double ABpos;

    //printf("%f %f  %f %f  %f %f  %f %f\n",
    //        ax, ay,  bx, by,  cx, cy,  dx, dy);

    /* reject degenerate line */
    if ((feq(ax, bx) && feq(ay, by)) ||
        (feq(cx, dx) && feq(cy, dy)))
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
    if ((feq(ax, cx) && feq(ay, cy)) ||
        (feq(bx, cx) && feq(by, cy)) ||
        (feq(ax, dx) && feq(ay, dy)) ||
        (feq(bx, dx) && feq(by, dy)))
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

    /* length of AB */
    distAB = sqrt(bx * bx + by * by);

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
    if ((cy < 0 && dy < 0) || (cy > 0 && dy > 0))
        return 0;

    if (feq(dy, cy)) return 0;
    ABpos = dx + ((cx - dx) * dy) / (dy - cy);
    if (ABpos < 0 || ABpos > distAB)
        return 0;

    *rx = ax + ABpos * theCos;
    *ry = ay + ABpos * theSin;

    XPOST_LOG_INFO(">< %d %d", *rx, *ry);
    return 1;
}

static
int _cyxcomp (const void *left, const void *right)
{
    const struct point *lt = left;
    const struct point *rt = right;
    if (feq(lt->y, rt->y)) {
        if (lt->x < rt->x) {
            return 1;
        } else if (lt->x > rt->x) {
            return -1;
        } else {
            return 0;
        }
    } else {
        if (lt->y < rt->y)
            return -1;
        else
            return 1;
    }
}

static
int _fillpoly (Xpost_Context *ctx,
               Xpost_Object poly,
               Xpost_Object devdic)
{
    Xpost_Object colorspace;
    int ncomp;
    Xpost_Object comp1, comp2, comp3;
    int numlines;
    /* Xpost_Object x1, y1, x2, y2; */
    Xpost_Object drawline;
    struct point *points, *intersections;
    int i, j;
    real yscan;
    real minx = 0x7ffffff;
    real miny = minx;
    real maxx = -minx;
    real maxy = maxx;
    int width;

    width = bdcget(ctx, devdic, consname(ctx, "width")).int_.val;
    colorspace = bdcget(ctx, devdic, consname(ctx, "nativecolorspace"));
    if (objcmp(ctx, colorspace, consname(ctx, "DeviceGray")) == 0)
    {
        ncomp = 1;
        comp1 = xpost_stack_pop(ctx->lo, ctx->os);
    }
    else if (objcmp(ctx, colorspace, consname(ctx, "DeviceRGB")) == 0)
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

    /* extract polygon vertices from ps array */
    points = alloca(poly.comp_.sz * sizeof *points);
    for (i=0; i < poly.comp_.sz; i++)
    {
        Xpost_Object pair, x, y;
        pair = barget(ctx, poly, i);
        x = barget(ctx, pair, 0);
        y = barget(ctx, pair, 1);
        if (xpost_object_get_type(x) == integertype)
            x = xpost_cons_real(x.int_.val);
        if (xpost_object_get_type(y) == integertype)
            y = xpost_cons_real(y.int_.val);

        points[i].x = floor(x.real_.val);
        points[i].y = floor(y.real_.val);
    }

    /* find bounding box */
    for (i = 0; i < poly.comp_.sz; i++){
        if (points[i].x < minx)
            minx = points[i].x;
        if (points[i].x > maxx)
            maxx = points[i].x;
        if (points[i].y < miny)
            miny = points[i].y;
        if (points[i].y > maxy)
            maxy = points[i].y;
    }

    intersections = alloca((maxy - miny) * 2 * 2 * sizeof *intersections);

    /* intersect polygon edges with scanlines */
    for (i = 0, j = 0; i < poly.comp_.sz - 1; i++){
        int rx, ry;
        for (yscan = miny + 0.5; yscan <= maxy; yscan += 1.0){
            if (_intersect(points[i].x, points[i].y,
                        points[i+1].x, points[i+1].y,
                        minx - 0.5, yscan,
                        maxx + 0.5, yscan,
                        &rx, &ry)){
                intersections[j].x = rx;
                intersections[j].y = ry;
                j++;
            }
        }
    }
    numlines = j / 2;

    /* sort intersection points */
    qsort(intersections, j, sizeof *intersections, _cyxcomp);

    /* arrange ((x1,y1),(x2,y2)) pairs */
    for (i = 0; i < numlines * 2; i += 2) {
        xpost_stack_push(ctx->lo, ctx->os, xpost_cons_int(floor(intersections[i].x)));
        xpost_stack_push(ctx->lo, ctx->os, xpost_cons_int(floor(intersections[i].y - 0.5)));
        xpost_stack_push(ctx->lo, ctx->os, xpost_cons_int(floor(intersections[i+1].x)));
        xpost_stack_push(ctx->lo, ctx->os, xpost_cons_int(floor(intersections[i+1].y - 0.5)));
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
     we're using a repeat loop which looks like:
         count proc  -repeat- 
    */ 
    xpost_stack_push(ctx->lo, ctx->os, xpost_cons_int(numlines));

    /*then push a mark object to begin array construction
     this array is our loop body */
    xpost_stack_push(ctx->lo, ctx->os, mark);

    /*the loop body finds the 4 coordinate numbers on the stack
     and must roll the color values beneath these numbers on the stack  */

    switch (ncomp) {
    case 1:
        xpost_stack_push(ctx->lo, ctx->os, comp1);
        xpost_stack_push(ctx->lo, ctx->os, xpost_cons_int(5)); /* total elements to roll */
        xpost_stack_push(ctx->lo, ctx->os, xpost_cons_int(1)); /* color components to move */
        break;
    case 3:
        xpost_stack_push(ctx->lo, ctx->os, comp1);
        xpost_stack_push(ctx->lo, ctx->os, comp2);
        xpost_stack_push(ctx->lo, ctx->os, comp3);
        xpost_stack_push(ctx->lo, ctx->os, xpost_cons_int(7)); /* total elements to roll */
        xpost_stack_push(ctx->lo, ctx->os, xpost_cons_int(3)); /* color components to move */
        break;
    }
    xpost_stack_push(ctx->lo, ctx->os, xpost_object_cvx(consname(ctx, "roll")));

      /*at this point we have the desired stack picture:
        
             comp1 (comp2 comp3)? x1 y1 x2 y2
        
        just need to push the devdic and DrawLine  */
     
    xpost_stack_push(ctx->lo, ctx->os, devdic);
    drawline = bdcget(ctx, devdic, consname(ctx, "DrawLine"));
    xpost_stack_push(ctx->lo, ctx->os, drawline);

    /*if drawline is a procedure, we also need to call exec */
    if (xpost_object_get_type(drawline) == arraytype)
        xpost_stack_push(ctx->lo, ctx->os, consname(ctx, "exec"));

    /*Then construct the loop-body procedure array. */
       //xpost_stack_push(ctx->lo, ctx->es, xpost_object_cvx(consname(ctx, "]")));

    /*Then, after the loop-body array is constructed, we need to call cvx on it. */
       //xpost_stack_push(ctx->lo, ctx->es, xpost_object_cvx(consname(ctx, "cvx")));
    /*"after" means this line, which pushes on the stack, goes *before* the consname("]") line.
     I'll summarize this part again. */

    /*After this, we call `repeat` and we're done. */
        //xpost_stack_push(ctx->lo, ctx->es, xpost_object_cvx(consname(ctx, "repeat")));

    /*Again since these are scheduled on a stack, we need to push them in reverse order
      from the order in which we desire them to execute.
      What we're doing is:

      opstack> xyxy xyxy xyxy ... xyxy numlines [ comp1 5 1 roll DEVICE DrawLine (exec)?
                                       numlines [ comp1 comp2 comp3 7 3 roll DEVICE DrawLine (exec)?
      execstack> repeat cvx ]
                            ^ construct array
                         ^ make executable
                   ^ call the loop operator
    */

    /*So the sequence in C should be: */

    xpost_stack_push(ctx->lo, ctx->es, xpost_object_cvx(consname(ctx, "repeat")));
    xpost_stack_push(ctx->lo, ctx->es, xpost_object_cvx(consname(ctx, "cvx")));
    xpost_stack_push(ctx->lo, ctx->es, xpost_object_cvx(consname(ctx, "]")));

    /*performance could be increased by factoring-out calls to consname()
      or using opcode shortcuts.
     */
    return 0;
}

int initdevgenericops (Xpost_Context *ctx,
                Xpost_Object sd)
{
    unsigned int optadr;
    oper *optab;
    Xpost_Object n,op;

    xpost_memory_table_get_addr(ctx->gl,
            XPOST_MEMORY_TABLE_SPECIAL_OPERATOR_TABLE, &optadr);
    optab = (oper *)(ctx->gl->base + optadr);

    op = consoper(ctx, ".yxsort", _yxsort, 0, 1, arraytype); INSTALL;
    op = consoper(ctx, ".fillpoly", _fillpoly, 0, 2, arraytype, dicttype); INSTALL;

    return 0;
}
