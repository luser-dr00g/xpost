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
#include <stdlib.h> /* abs */
#include <string.h>

#include <xcb/xcb.h>
#include <xcb/xcb_image.h>
#include <xcb/xcb_aux.h>
#include <xcb/xcb_icccm.h>

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
#include "xpost_dev_xcb.h" /* check prototypes */

#define XCB_ALL_PLANES ~0

typedef struct {
    xcb_connection_t *c;
    xcb_screen_t *scr;
    xcb_drawable_t win;
    int width, height;
    xcb_pixmap_t img;
    xcb_gcontext_t gc;
    xcb_colormap_t cmap;
} PrivateData;

static int _flush (Xpost_Context *ctx, Xpost_Object devdic);

static
unsigned int _event_handler_opcode;

static Xpost_Object namePrivate;
static Xpost_Object namewidth;
static Xpost_Object nameheight;
static Xpost_Object namedotcopydict;
static Xpost_Object namenativecolorspace;
static Xpost_Object nameDeviceRGB;

static
int _event_handler (Xpost_Context *ctx,
                    Xpost_Object devdic)
{
    Xpost_Object privatestr;
    PrivateData private;
    xcb_generic_event_t *event;


    /* load private data struct from string */
    privatestr = bdcget(ctx, devdic, namePrivate);
    if (xpost_object_get_type(privatestr) == invalidtype)
        return undefined;
    xpost_memory_get(xpost_context_select_memory(ctx, privatestr),
            xpost_object_get_ent(privatestr), 0, sizeof private, &private);

    event = xcb_poll_for_event(private.c);
    if (event)
    {
        switch(event->response_type & ~0x80)
        {
        case XCB_EXPOSE:
            _flush(ctx, devdic);
            break;
        default:
            break;
        }
        free(event);
    }
    else if (xcb_connection_has_error(private.c))
        return unregistered;

    return 0;
}


static
unsigned int _create_cont_opcode;

/* create an instance of the device
   using the class .copydict procedure */
static
int _create (Xpost_Context *ctx,
             Xpost_Object width,
             Xpost_Object height,
             Xpost_Object classdic)
{
    xpost_stack_push(ctx->lo, ctx->os, width);
    xpost_stack_push(ctx->lo, ctx->os, height);
    xpost_stack_push(ctx->lo, ctx->os, classdic);
    bdcput(ctx, classdic, namewidth, width);
    bdcput(ctx, classdic, nameheight, height);

    /* call device class's ps-level .copydict procedure,
       then call _create_cont, by continuation. */
    if (!xpost_stack_push(ctx->lo, ctx->es, operfromcode(_create_cont_opcode)))
        return execstackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->es, bdcget(ctx, classdic,
                    //consname(ctx, ".copydict")
                    namedotcopydict
                    )))
        return execstackoverflow;

    return 0;
}

/* initialize the C-level data
   and define in the device instance */
static
int _create_cont (Xpost_Context *ctx,
                  Xpost_Object w,
                  Xpost_Object h,
                  Xpost_Object devdic)
{
    Xpost_Object privatestr;
    PrivateData private;
    xcb_screen_iterator_t iter;
    xcb_get_geometry_reply_t *geom;
    integer width = w.int_.val;
    integer height = h.int_.val;
    int scrno;
    unsigned char depth;

    /* create a string to contain device data structure */
    privatestr = xpost_string_cons(ctx, sizeof(PrivateData), NULL);
    if (xpost_object_get_type(privatestr) == invalidtype)
    {
        XPOST_LOG_ERR("cannot allocat private data structure");
        return unregistered;
    }
    bdcput(ctx, devdic, namePrivate, privatestr);

    private.width = width;
    private.height = height;

    /* create xcb connection
       and create and map window */
    private.c = xcb_connect(NULL, &scrno);
    if (xcb_connection_has_error(private.c))
    {
        XPOST_LOG_ERR("Fail to connect to the X server");
        return unregistered;
    }

    iter = xcb_setup_roots_iterator(xcb_get_setup(private.c));
    for (; iter.rem; --scrno, xcb_screen_next(&iter))
    {
        if (scrno == 0)
        {
            private.scr = iter.data;
            break;
        }
    }

    geom = xcb_get_geometry_reply (private.c, xcb_get_geometry(private.c, private.scr->root), 0);
    if (!geom)
    {
        XPOST_LOG_ERR("Fail to the geometry of the root window");
        xcb_disconnect(private.c);
        return unregistered;
    }

    depth = geom->depth;
    free(geom);

    private.win = xcb_generate_id(private.c);
    {
        unsigned int value = private.scr->white_pixel;
        xcb_create_window(private.c, XCB_COPY_FROM_PARENT,
                private.win, private.scr->root,
                0, 0,
                width, height,
                5,
                XCB_WINDOW_CLASS_INPUT_OUTPUT,
                private.scr->root_visual,
                XCB_CW_BACK_PIXEL,
                &value);
        xcb_icccm_set_wm_name(private.c, private.win, XCB_ATOM_STRING, 8, strlen("Xpost"), "Xpost");
    }
    xcb_map_window(private.c, private.win);
    xcb_flush(private.c);

    private.img = xcb_generate_id(private.c);
    xcb_create_pixmap(private.c,
            depth, private.img,
            private.win, private.width, private.height);

    /* create graphics context
       and initialize drawing parameters */
    private.gc = xcb_generate_id(private.c);
    {
        unsigned int values[2] = {
            private.scr->black_pixel,
            private.scr->white_pixel
        } ;
        xcb_create_gc(private.c, private.gc, private.win,
                XCB_GC_FOREGROUND | XCB_GC_BACKGROUND,
                values);
    }

    //private.cmap = private.scr->default_colormap;
    /* create colormap */
    private.cmap = xcb_generate_id(private.c);
    xcb_create_colormap(private.c, XCB_COLORMAP_ALLOC_NONE, private.cmap,
            private.win, private.scr->root_visual);

    xpost_context_install_event_handler(ctx,
            operfromcode(_event_handler_opcode),
            devdic);


    /* save private data struct in string */
    xpost_memory_put(xpost_context_select_memory(ctx, privatestr),
            xpost_object_get_ent(privatestr), 0, sizeof private, &private);

    /* return device instance dictionary to ps */
    xpost_stack_push(ctx->lo, ctx->os, devdic);
    return 0;
}

static
int _putpix (Xpost_Context *ctx,
             Xpost_Object red,
             Xpost_Object green,
             Xpost_Object blue,
             Xpost_Object x,
             Xpost_Object y,
             Xpost_Object devdic)
{
    Xpost_Object privatestr;
    PrivateData private;

    /* fold numbers to integertype */
    if (xpost_object_get_type(red) == realtype)
        red = xpost_int_cons(red.real_.val * 65535.0);
    else
        red.int_.val *= 65535;
    if (xpost_object_get_type(green) == realtype)
        green = xpost_int_cons(green.real_.val * 65535.0);
    else
        green.int_.val *= 65535;
    if (xpost_object_get_type(blue) == realtype)
        blue = xpost_int_cons(blue.real_.val * 65535.0);
    else
        blue.int_.val *= 65535;
    if (xpost_object_get_type(x) == realtype)
        x = xpost_int_cons(x.real_.val);
    if (xpost_object_get_type(y) == realtype)
        y = xpost_int_cons(y.real_.val);

    /* load private data struct from string */
    privatestr = bdcget(ctx, devdic, namePrivate);
    if (xpost_object_get_type(privatestr) == invalidtype)
        return undefined;
    xpost_memory_get(xpost_context_select_memory(ctx, privatestr),
            xpost_object_get_ent(privatestr), 0, sizeof private, &private);

    /* check bounds */
    if (x.int_.val < 0 || x.int_.val >= bdcget(ctx, devdic, namewidth).int_.val)
        return 0;
    if (y.int_.val < 0 || y.int_.val >= bdcget(ctx, devdic, nameheight).int_.val)
        return 0;

    {
        xcb_alloc_color_reply_t *rep;
        unsigned int value;
        xcb_point_t p;
        p.x = x.int_.val;
        p.y = y.int_.val;

        rep = xcb_alloc_color_reply(private.c,
                xcb_alloc_color(private.c, private.cmap,
                    red.int_.val,
                    green.int_.val,
                    blue.int_.val),
                0);
        if (!rep)
            return unregistered;

        value = rep->pixel;
        free(rep);
        xcb_change_gc(private.c, private.gc, XCB_GC_FOREGROUND, &value);

        xcb_poly_point(private.c, XCB_COORD_MODE_ORIGIN,
                private.img, private.gc, 1, &p);
    }

    /* save private data struct in string */
    xpost_memory_put(xpost_context_select_memory(ctx, privatestr),
            xpost_object_get_ent(privatestr), 0, sizeof private, &private);

    return 0;
}

static
int _getpix (Xpost_Context *ctx,
             Xpost_Object x,
             Xpost_Object y,
             Xpost_Object devdic)
{
    Xpost_Object privatestr;
    PrivateData private;

    /* load private data struct from string */
    privatestr = bdcget(ctx, devdic, namePrivate);
    if (xpost_object_get_type(privatestr) == invalidtype)
        return undefined;
    xpost_memory_get(xpost_context_select_memory(ctx, privatestr),
            xpost_object_get_ent(privatestr), 0, sizeof private, &private);

    /* ?? I don't know ...
       make a 1-pixel image and use copy_area?  ... */
    return 0;
}

static
int _drawline (Xpost_Context *ctx,
               Xpost_Object red,
               Xpost_Object green,
               Xpost_Object blue,
               Xpost_Object x1,
               Xpost_Object y1,
               Xpost_Object x2,
               Xpost_Object y2,
               Xpost_Object devdic)
{
    Xpost_Object privatestr;
    PrivateData private;

    /* fold numbers to integertype */
    if (xpost_object_get_type(red) == realtype)
        red = xpost_int_cons(red.real_.val * 65535.0);
    else
        red.int_.val *= 65535;
    if (xpost_object_get_type(green) == realtype)
        green = xpost_int_cons(green.real_.val * 65535.0);
    else
        green.int_.val *= 65535;
    if (xpost_object_get_type(blue) == realtype)
        blue = xpost_int_cons(blue.real_.val * 65535.0);
    else
        blue.int_.val *= 65535;
    if (xpost_object_get_type(x1) == realtype)
        x1 = xpost_int_cons(x1.real_.val);
    if (xpost_object_get_type(y1) == realtype)
        y1 = xpost_int_cons(y1.real_.val);
    if (xpost_object_get_type(x2) == realtype)
        x2 = xpost_int_cons(x2.real_.val);
    if (xpost_object_get_type(y2) == realtype)
        y2 = xpost_int_cons(y2.real_.val);

    XPOST_LOG_INFO("_drawline(%d, %d, %d, %d)",
            x1.int_.val, y1.int_.val, x2.int_.val, y2.int_.val);

    /* load private data struct from string */
    privatestr = bdcget(ctx, devdic, namePrivate);
    if (xpost_object_get_type(privatestr) == invalidtype)
        return undefined;
    xpost_memory_get(xpost_context_select_memory(ctx, privatestr),
            xpost_object_get_ent(privatestr), 0, sizeof private, &private);

    {
        xcb_alloc_color_reply_t *rep;
        unsigned int value;

        rep = xcb_alloc_color_reply(private.c,
                xcb_alloc_color(private.c, private.cmap,
                    red.int_.val,
                    green.int_.val,
                    blue.int_.val),
                0);
        if (!rep)
            return unregistered;

        value = rep->pixel;
        free(rep);
        xcb_change_gc(private.c, private.gc, XCB_GC_FOREGROUND, &value);
    }

    {
        xcb_point_t points[] = {
            { x1.int_.val, y1.int_.val },
            { x2.int_.val, y2.int_.val }
        };
        xcb_poly_line(private.c, XCB_COORD_MODE_ORIGIN,
                private.img, private.gc, 2, points);
    }

    return 0;
}

static
int _fillrect (Xpost_Context *ctx,
               Xpost_Object red,
               Xpost_Object green,
               Xpost_Object blue,
               Xpost_Object x,
               Xpost_Object y,
               Xpost_Object width,
               Xpost_Object height,
               Xpost_Object devdic)
{
    Xpost_Object privatestr;
    PrivateData private;
    int w,h;
    int i,j;

    /* fold numbers to integertype */
    if (xpost_object_get_type(red) == realtype)
        red = xpost_int_cons(red.real_.val * 65535.0);
    else
        red.int_.val *= 65535;
    if (xpost_object_get_type(green) == realtype)
        green = xpost_int_cons(green.real_.val * 65535.0);
    else
        green.int_.val *= 65535;
    if (xpost_object_get_type(blue) == realtype)
        blue = xpost_int_cons(blue.real_.val * 65535.0);
    else
        blue.int_.val *= 65535;
    if (xpost_object_get_type(x) == realtype)
        x = xpost_int_cons(x.real_.val);
    if (xpost_object_get_type(y) == realtype)
        y = xpost_int_cons(y.real_.val);
    if (xpost_object_get_type(width) == realtype)
        width = xpost_int_cons(width.real_.val);
    if (xpost_object_get_type(height) == realtype)
        height = xpost_int_cons(height.real_.val);

    /* adjust ranges */
    if (width.int_.val < 0)
    {
        width.int_.val = abs(width.int_.val);
        x.int_.val -= width.int_.val;
    }
    if (height.int_.val < 0)
    {
        height.int_.val = abs(height.int_.val);
        y.int_.val -= height.int_.val;
    }
    if (x.int_.val < 0) x.int_.val = 0;
    if (y.int_.val < 0) y.int_.val = 0;

    /* load private data struct from string */
    privatestr = bdcget(ctx, devdic, namePrivate);
    if (xpost_object_get_type(privatestr) == invalidtype)
        return undefined;
    xpost_memory_get(xpost_context_select_memory(ctx, privatestr),
            xpost_object_get_ent(privatestr), 0, sizeof private, &private);
    w = bdcget(ctx, devdic, namewidth).int_.val;
    h = bdcget(ctx, devdic, nameheight).int_.val;

    if (x.int_.val >= w || y.int_.val >= h)
        return 0;
    if (x.int_.val + width.int_.val > w)
        width.int_.val = w - x.int_.val;
    if (y.int_.val + height.int_.val > h)
        height.int_.val = h - y.int_.val;

    {
        xcb_alloc_color_reply_t *rep;
        unsigned int value;

        rep = xcb_alloc_color_reply(private.c,
                xcb_alloc_color(private.c, private.cmap,
                    red.int_.val,
                    green.int_.val,
                    blue.int_.val),
                0);
        if (!rep)
            return unregistered;

        value = rep->pixel;
        free(rep);
        xcb_change_gc(private.c, private.gc, XCB_GC_FOREGROUND, &value);

        for (i=0; i < height.int_.val; i++)
        {
            for (j=0; j < width.int_.val; j++)
            {
                xcb_point_t p;
                p.x = x.int_.val + j;
                p.y = y.int_.val + i;

                xcb_poly_point(private.c, XCB_COORD_MODE_ORIGIN,
                        private.img, private.gc, 1, &p);
            }
        }
    }
    return 0;
}

static
int _fillpoly (Xpost_Context *ctx,
               Xpost_Object red,
               Xpost_Object green,
               Xpost_Object blue,
               Xpost_Object poly,
               Xpost_Object devdic)
{
    Xpost_Object privatestr;
    PrivateData private;

    /* fold numbers to integertype */
    if (xpost_object_get_type(red) == realtype)
        red = xpost_int_cons(red.real_.val * 65535.0);
    else
        red.int_.val *= 65535;
    if (xpost_object_get_type(green) == realtype)
        green = xpost_int_cons(green.real_.val * 65535.0);
    else
        green.int_.val *= 65535;
    if (xpost_object_get_type(blue) == realtype)
        blue = xpost_int_cons(blue.real_.val * 65535.0);
    else
        blue.int_.val *= 65535;

    /* load private data struct from string */
    privatestr = bdcget(ctx, devdic, namePrivate);
    xpost_memory_get(xpost_context_select_memory(ctx, privatestr),
            xpost_object_get_ent(privatestr), 0, sizeof private, &private);

    {
        xcb_point_t *points;
        int i;
        xcb_alloc_color_reply_t *rep;
        unsigned int value;

        rep = xcb_alloc_color_reply(private.c,
                xcb_alloc_color(private.c, private.cmap,
                    red.int_.val,
                    green.int_.val,
                    blue.int_.val),
                0);
        if (!rep)
            return unregistered;

        value = rep->pixel;
        free(rep);
        xcb_change_gc(private.c, private.gc, XCB_GC_FOREGROUND, &value);

        points = alloca((poly.comp_.sz //+ 1
                    ) * sizeof *points);
        for (i=0; i < poly.comp_.sz; i++)
        {
            Xpost_Object pair, x, y;
            pair = barget(ctx, poly, i);
            x = barget(ctx, pair, 0);
            y = barget(ctx, pair, 1);
            if (xpost_object_get_type(x) == realtype)
                x = xpost_int_cons(x.real_.val);
            if (xpost_object_get_type(y) == realtype)
                y = xpost_int_cons(y.real_.val);

            points[i].x = x.int_.val;
            points[i].y = y.int_.val;
        }
        //points[i].x = points[0].x;
        //points[i].y = points[0].y;

        xcb_fill_poly(private.c, private.img, private.gc,
                XCB_POLY_SHAPE_NONCONVEX,
                XCB_COORD_MODE_ORIGIN,
                poly.comp_.sz //+ 1
                , points);
    }

    return 0;
}


static
int _flush (Xpost_Context *ctx,
           Xpost_Object devdic)
{
    Xpost_Object privatestr;
    PrivateData private;

    /* load private data struct from string */
    privatestr = bdcget(ctx, devdic, namePrivate);
    if (xpost_object_get_type(privatestr) == invalidtype)
        return undefined;
    xpost_memory_get(xpost_context_select_memory(ctx, privatestr),
            xpost_object_get_ent(privatestr), 0, sizeof private, &private);

    xcb_copy_area(private.c, private.img, private.win, private.gc,
            0, 0, 0, 0, private.width, private.height);
    xcb_flush(private.c);

    return 0;
}

/* Emit here is the same as Flush
   But Flush is called (if available) by all raster operators
   for smoother previewing.
 */
static
int (*_emit) (Xpost_Context *ctx,
           Xpost_Object devdic) = _flush;

static
int _destroy (Xpost_Context *ctx,
              Xpost_Object devdic)
{
    Xpost_Object privatestr;
    PrivateData private;

    privatestr = bdcget(ctx, devdic, namePrivate);
    if (xpost_object_get_type(privatestr) == invalidtype)
        return undefined;
    xpost_memory_get(xpost_context_select_memory(ctx, privatestr), xpost_object_get_ent(privatestr), 0,
            sizeof private, &private);

    xpost_context_install_event_handler(ctx, null, null);

    xcb_disconnect(private.c);

    return 0;
}



/* operator function to instantiate a new window device.
   installed in userdict by calling 'loadXXXdevice'.
 */
static
int newxcbdevice (Xpost_Context *ctx,
                  Xpost_Object width,
                  Xpost_Object height)
{
    Xpost_Object classdic;
    int ret;

    xpost_stack_push(ctx->lo, ctx->os, width);
    xpost_stack_push(ctx->lo, ctx->os, height);
    ret = Aload(ctx, consname(ctx, "xcbDEVICE"));
    if (ret)
        return ret;
    classdic = xpost_stack_topdown_fetch(ctx->lo, ctx->os, 0);
    if (!xpost_stack_push(ctx->lo, ctx->es, bdcget(ctx, classdic, consname(ctx, "Create"))))
        return execstackoverflow;

    return 0;
}

static
unsigned int _loadxcbdevicecont_opcode;

/* Specializes or sub-classes the PPMIMAGE device class.
   load PPMIMAGE
   load and call ps procedure .copydict which leaves copy on stack
   call loadxcbdevicecont by continuation.
 */
static
int loadxcbdevice (Xpost_Context *ctx)
{
    Xpost_Object classdic;
    int ret;

    ret = Aload(ctx, consname(ctx, "PPMIMAGE"));
    if (ret)
        return ret;
    classdic = xpost_stack_topdown_fetch(ctx->lo, ctx->os, 0);
    if (!xpost_stack_push(ctx->lo, ctx->es, operfromcode(_loadxcbdevicecont_opcode)))
        return execstackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->es, bdcget(ctx, classdic,
                    //consname(ctx, ".copydict")
                    namedotcopydict
                    )))
        return execstackoverflow;

    return 0;
}

/* replace procedures in the class with newly created special operators.
   defines the device class xcbDEVICE in userdict.
   defines a new operator in userdict: newxcbdevice
 */
static
int loadxcbdevicecont (Xpost_Context *ctx,
                       Xpost_Object classdic)
{
    Xpost_Object userdict;
    Xpost_Object op;
    int ret;

    ret = bdcput(ctx, classdic,
            //consname(ctx, "nativecolorspace"),
            namenativecolorspace,
            //consname(ctx, "DeviceRGB")
            nameDeviceRGB
            );

    op = consoper(ctx, "xcbCreateCont", _create_cont, 1, 3, integertype, integertype, dicttype);
    _create_cont_opcode = op.mark_.padw;
    op = consoper(ctx, "xcbCreate", _create, 1, 3, integertype, integertype, dicttype);
    ret = bdcput(ctx, classdic, consname(ctx, "Create"), op);
    if (ret)
        return ret;

    op = consoper(ctx, "xcbPutPix", _putpix, 0, 6,
            numbertype, numbertype, numbertype, /* r g b color values */
            numbertype, numbertype, /* x y coords */
            dicttype); /* devdic */
    ret = bdcput(ctx, classdic, consname(ctx, "PutPix"), op);
    if (ret)
        return ret;

    op = consoper(ctx, "xcbGetPix", _getpix, 3, 3, numbertype, numbertype, dicttype);
    ret = bdcput(ctx, classdic, consname(ctx, "GetPix"), op);
    if (ret)
        return ret;

    op = consoper(ctx, "xcbDrawLine", _drawline, 0, 8,
            numbertype, numbertype, numbertype, /* r g b color values */
            numbertype, numbertype, /* x1 y1 */
            numbertype, numbertype, /* x2 y2 */
            dicttype); /* devdic */
    ret = bdcput(ctx, classdic, consname(ctx, "DrawLine"), op);
    if (ret)
        return ret;

    op = consoper(ctx, "xcbFillRect", _fillrect, 0, 8,
            numbertype, numbertype, numbertype, /* r g b color values */
            numbertype, numbertype, /* x y */
            numbertype, numbertype, /* width height */
            dicttype); /* devdic */
    ret = bdcput(ctx, classdic, consname(ctx, "FillRect"), op);
    if (ret)
        return ret;

    op = consoper(ctx, "xcbFillPoly", _fillpoly, 0, 5,
            numbertype, numbertype, numbertype,
            arraytype, dicttype);
    ret = bdcput(ctx, classdic, consname(ctx, "FillPoly"), op);
    if (ret)
        return ret;

    op = consoper(ctx, "xcbEmit", _emit, 0, 1, dicttype);
    ret = bdcput(ctx, classdic, consname(ctx, "Emit"), op);
    if (ret)
        return ret;

    op = consoper(ctx, "xcbFlush", _flush, 0, 1, dicttype);
    ret = bdcput(ctx, classdic, consname(ctx, "Flush"), op);
    if (ret)
        return ret;

    op = consoper(ctx, "xcbDestroy", _destroy, 0, 1, dicttype);
    ret = bdcput(ctx, classdic, consname(ctx, "Destroy"), op);
    if (ret)
        return ret;

    userdict = xpost_stack_bottomup_fetch(ctx->lo, ctx->ds, 2);

    ret = bdcput(ctx, userdict, consname(ctx, "xcbDEVICE"), classdic);
    if (ret)
        return ret;

    op = consoper(ctx, "newxcbdevice", newxcbdevice, 1, 2, integertype, integertype);
    ret = bdcput(ctx, userdict, consname(ctx, "newxcbdevice"), op);
    if (ret)
        return ret;

    op = consoper(ctx, "xcbEventHandler", _event_handler, 0, 1, dicttype);
    _event_handler_opcode = op.mark_.padw;

    return 0;
}

/*
   install the loadXXXdevice which may be called during graphics initialization
   to produce the operator newXXXdevice which instantiates the device dictionary.
*/
int initxcbops (Xpost_Context *ctx,
                Xpost_Object sd)
{
    unsigned int optadr;
    oper *optab;
    Xpost_Object n,op;

    if (xpost_object_get_type(namePrivate = consname(ctx, "Private")) == invalidtype)
        return VMerror;
    if (xpost_object_get_type(namewidth = consname(ctx, "width")) == invalidtype)
        return VMerror;
    if (xpost_object_get_type(nameheight = consname(ctx, "height")) == invalidtype)
        return VMerror;
    if (xpost_object_get_type(namedotcopydict = consname(ctx, ".copydict")) == invalidtype)
        return VMerror;
    if (xpost_object_get_type(namenativecolorspace = consname(ctx, "nativecolorspace")) == invalidtype)
        return VMerror;
    if (xpost_object_get_type(nameDeviceRGB = consname(ctx, "DeviceRGB")) == invalidtype)
        return VMerror;

    xpost_memory_table_get_addr(ctx->gl,
            XPOST_MEMORY_TABLE_SPECIAL_OPERATOR_TABLE, &optadr);
    optab = (oper *)(ctx->gl->base + optadr);
    op = consoper(ctx, "loadxcbdevice", loadxcbdevice, 1, 0); INSTALL;
    op = consoper(ctx, "loadxcbdevicecont", loadxcbdevicecont, 1, 1, dicttype);
    _loadxcbdevicecont_opcode = op.mark_.padw;

    return 0;
}
