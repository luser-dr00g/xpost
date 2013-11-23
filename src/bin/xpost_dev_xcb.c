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

#include <assert.h>
#include <stdlib.h> /* abs */
#include <string.h>

#include <xcb/xcb.h>
#include <xcb/xcb_image.h>
#include <xcb/xcb_aux.h>

#include "xpost_log.h"
#include "xpost_memory.h" /* access memory */
#include "xpost_object.h" /* work with objects */
#include "xpost_stack.h"  /* push results on stack */

#include "xpost_context.h" /* state */
#include "xpost_error.h"
#include "xpost_dict.h" /* get/put values in dicts */
#include "xpost_string.h" /* get/put values in strings */
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


static
unsigned int _event_handler_opcode;

static
int _event_handler (Xpost_Context *ctx)
{
    Xpost_Object devdic;
    int ret;

    ret = Aload(ctx, consname(ctx, "DEVICE"));
    if (ret)
        return ret;
    devdic = xpost_stack_pop(ctx->lo, ctx->os);

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

    /* call device class's ps-level .copydict procedure,
       then call _create_cont, by continuation. */
    if (!xpost_stack_push(ctx->lo, ctx->es, operfromcode(_create_cont_opcode)))
        return execstackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->es, bdcget(ctx, classdic, consname(ctx, ".copydict"))))
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
    int i;
    unsigned char  depth;

    /* create a string to contain device data structure */
    privatestr = consbst(ctx, sizeof(PrivateData), NULL);
    bdcput(ctx, devdic, consname(ctx, "Private"), privatestr);
    bdcput(ctx, devdic, consname(ctx, "width"), w);
    bdcput(ctx, devdic, consname(ctx, "height"), h);

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
    if (scrno == 0)
    {
        private.scr = iter.data;
        break;
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

    private.cmap = private.scr->default_colormap;
    /* create colormap */
    //private.cmap = xcb_generate_id(private.c);
    //xcb_create_colormap(private.c, XCB_COLORMAP_ALLOC_NONE, private.cmap,
    //        private.win, private.scr->root_visual);

    xpost_context_install_event_handler(ctx, operfromcode(_event_handler_opcode));


    /* save private data struct in string */
    xpost_memory_put(xpost_context_select_memory(ctx, privatestr),
            privatestr.comp_.ent, 0, sizeof private, &private);

    /* return device instance dictionary to ps */
    xpost_stack_push(ctx->lo, ctx->os, devdic);
    return 0;
}

static
int _putpix (Xpost_Context *ctx,
             Xpost_Object val,
             Xpost_Object x,
             Xpost_Object y,
             Xpost_Object devdic)
{
    Xpost_Object privatestr;
    PrivateData private;

    /* fold numbers to integertype */
    if (xpost_object_get_type(val) == realtype)
        val = xpost_cons_int(val.real_.val * 255.0);
    else
        val.int_.val *= 255;
    if (xpost_object_get_type(x) == realtype)
        x = xpost_cons_int(x.real_.val);
    if (xpost_object_get_type(y) == realtype)
        y = xpost_cons_int(y.real_.val);

    /* load private data struct from string */
    privatestr = bdcget(ctx, devdic, consname(ctx, "Private"));
    xpost_memory_get(xpost_context_select_memory(ctx, privatestr),
            privatestr.comp_.ent, 0, sizeof private, &private);

    /* check bounds */
    if (x.int_.val < 0 || x.int_.val >= bdcget(ctx, devdic, consname(ctx, "width")).int_.val)
        return 0;
    if (y.int_.val < 0 || y.int_.val >= bdcget(ctx, devdic, consname(ctx, "height")).int_.val)
        return 0;

    {
        xcb_alloc_color_reply_t *rep;

        xcb_point_t p;
        p.x = x.int_.val;
        p.y = y.int_.val;

        rep = xcb_alloc_color_reply(private.c,
                xcb_alloc_color(private.c, private.cmap,
                    val.int_.val * 257,
                    val.int_.val * 257,
                    val.int_.val * 257),
                0);
        if (!rep)
            return unregistered;

        {
            unsigned int value[] = { rep->pixel };
            xcb_change_gc(private.c, private.gc, XCB_GC_FOREGROUND, value);
        }

        xcb_poly_point(private.c, XCB_COORD_MODE_ORIGIN,
                private.img, private.gc, 1, &p);
    }

    /* save private data struct in string */
    xpost_memory_put(xpost_context_select_memory(ctx, privatestr),
            privatestr.comp_.ent, 0, sizeof private, &private);

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
    privatestr = bdcget(ctx, devdic, consname(ctx, "Private"));
    xpost_memory_get(xpost_context_select_memory(ctx, privatestr),
            privatestr.comp_.ent, 0, sizeof private, &private);

    /* ?? I don't know ...
       make a 1-pixel image and use copy_area?  ... */
    return 0;
}

static
int _drawline (Xpost_Context *ctx,
               Xpost_Object val,
               Xpost_Object x1,
               Xpost_Object y1,
               Xpost_Object x2,
               Xpost_Object y2,
               Xpost_Object devdic)
{
    Xpost_Object privatestr;
    PrivateData private;

    /* fold numbers to integertype */
    if (xpost_object_get_type(val) == realtype)
        val = xpost_cons_int(val.real_.val * 255.0);
    else
        val.int_.val *= 255;
    if (xpost_object_get_type(x1) == realtype)
        x1 = xpost_cons_int(x1.real_.val);
    if (xpost_object_get_type(y1) == realtype)
        y1 = xpost_cons_int(y1.real_.val);
    if (xpost_object_get_type(x2) == realtype)
        x2 = xpost_cons_int(x2.real_.val);
    if (xpost_object_get_type(y2) == realtype)
        y2 = xpost_cons_int(y2.real_.val);

    /* load private data struct from string */
    privatestr = bdcget(ctx, devdic, consname(ctx, "Private"));
    xpost_memory_get(xpost_context_select_memory(ctx, privatestr),
            privatestr.comp_.ent, 0, sizeof private, &private);

    {
        xcb_alloc_color_reply_t *rep;

        rep = xcb_alloc_color_reply(private.c,
                xcb_alloc_color(private.c, private.cmap,
                    val.int_.val * 257,
                    val.int_.val * 257,
                    val.int_.val * 257),
                0);
        if (!rep)
            return unregistered;

        {
            unsigned int value[] = { rep->pixel };
            xcb_change_gc(private.c, private.gc, XCB_GC_FOREGROUND, value);
        }
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
               Xpost_Object val,
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
    if (xpost_object_get_type(val) == realtype)
        val = xpost_cons_int(val.real_.val * 255.0);
    else
        val.int_.val *= 255;
    if (xpost_object_get_type(x) == realtype)
        x = xpost_cons_int(x.real_.val);
    if (xpost_object_get_type(y) == realtype)
        y = xpost_cons_int(y.real_.val);
    if (xpost_object_get_type(width) == realtype)
        width = xpost_cons_int(width.real_.val);
    if (xpost_object_get_type(height) == realtype)
        height = xpost_cons_int(height.real_.val);

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
    privatestr = bdcget(ctx, devdic, consname(ctx, "Private"));
    xpost_memory_get(xpost_context_select_memory(ctx, privatestr),
            privatestr.comp_.ent, 0, sizeof private, &private);
    w = bdcget(ctx, devdic, consname(ctx,"width")).int_.val;
    h = bdcget(ctx, devdic, consname(ctx,"height")).int_.val;

    if (x.int_.val >= w || y.int_.val >= h)
        return 0;
    if (x.int_.val + width.int_.val > w)
        width.int_.val = w - x.int_.val;
    if (y.int_.val + height.int_.val > h)
        height.int_.val = h - y.int_.val;

    {
        xcb_alloc_color_reply_t *rep;

        rep = xcb_alloc_color_reply(private.c,
                xcb_alloc_color(private.c, private.cmap,
                    val.int_.val * 257,
                    val.int_.val * 257,
                    val.int_.val * 257),
                0);
        if (!rep)
            return unregistered;

        {
            unsigned int value[] = { rep->pixel };
            xcb_change_gc(private.c, private.gc, XCB_GC_FOREGROUND, value);
        }

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
int _flush (Xpost_Context *ctx,
           Xpost_Object devdic)
{
    Xpost_Object privatestr;
    PrivateData private;

    /* load private data struct from string */
    privatestr = bdcget(ctx, devdic, consname(ctx, "Private"));
    xpost_memory_get(xpost_context_select_memory(ctx, privatestr),
            privatestr.comp_.ent, 0, sizeof private, &private);

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

    privatestr = bdcget(ctx, devdic, consname(ctx, "Private"));
    xpost_memory_get(xpost_context_select_memory(ctx, privatestr), privatestr.comp_.ent, 0,
            sizeof private, &private);

    xpost_context_install_event_handler(ctx, null);

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

/* Specializes or sub-classes the PGMIMAGE device class.
   load PGMIMAGE
   load and call ps procedure .copydict which leaves copy on stack
   call loadxcbdevicecont by continuation.
 */
static
int loadxcbdevice (Xpost_Context *ctx)
{
    Xpost_Object classdic;
    int ret;

    ret = Aload(ctx, consname(ctx, "PGMIMAGE"));
    if (ret)
        return ret;
    classdic = xpost_stack_topdown_fetch(ctx->lo, ctx->os, 0);
    if (!xpost_stack_push(ctx->lo, ctx->es, operfromcode(_loadxcbdevicecont_opcode)))
        return execstackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->es, bdcget(ctx, classdic, consname(ctx, ".copydict"))))
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

    op = consoper(ctx, "xcbCreateCont", _create_cont, 1, 3, integertype, integertype, dicttype);
    _create_cont_opcode = op.mark_.padw;
    op = consoper(ctx, "xcbCreate", _create, 1, 3, integertype, integertype, dicttype);
    bdcput(ctx, classdic, consname(ctx, "Create"), op);

    op = consoper(ctx, "xcbPutPix", _putpix, 0, 4, numbertype, numbertype, numbertype, dicttype);
    bdcput(ctx, classdic, consname(ctx, "PutPix"), op);

    op = consoper(ctx, "xcbGetPix", _getpix, 1, 3, numbertype, numbertype, dicttype);
    bdcput(ctx, classdic, consname(ctx, "GetPix"), op);

    op = consoper(ctx, "xcbDrawLine", _drawline, 0, 6, numbertype, numbertype, numbertype,
       numbertype, numbertype, dicttype);
    bdcput(ctx, classdic, consname(ctx, "DrawLine"), op);

    op = consoper(ctx, "xcbFillRect", _fillrect, 0, 6,
            numbertype, numbertype, numbertype, numbertype, numbertype, dicttype);
    bdcput(ctx, classdic, consname(ctx, "FillRect"), op);

    op = consoper(ctx, "xcbEmit", _emit, 0, 1, dicttype);
    bdcput(ctx, classdic, consname(ctx, "Emit"), op);

    op = consoper(ctx, "xcbFlush", _flush, 0, 1, dicttype);
    bdcput(ctx, classdic, consname(ctx, "Flush"), op);

    op = consoper(ctx, "xcbDestroy", _destroy, 0, 1, dicttype);
    bdcput(ctx, classdic, consname(ctx, "Destroy"), op);

    userdict = xpost_stack_bottomup_fetch(ctx->lo, ctx->ds, 2);

    bdcput(ctx, userdict, consname(ctx, "xcbDEVICE"), classdic);

    op = consoper(ctx, "newxcbdevice", newxcbdevice, 1, 2, integertype, integertype);
    bdcput(ctx, userdict, consname(ctx, "newxcbdevice"), op);

    op = consoper(ctx, "xcbEventHandler", _event_handler, 0, 0);
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

    xpost_memory_table_get_addr(ctx->gl,
            XPOST_MEMORY_TABLE_SPECIAL_OPERATOR_TABLE, &optadr);
    optab = (oper *)(ctx->gl->base + optadr);
    op = consoper(ctx, "loadxcbdevice", loadxcbdevice, 1, 0); INSTALL;
    op = consoper(ctx, "loadxcbdevicecont", loadxcbdevicecont, 1, 1, dicttype);
    _loadxcbdevicecont_opcode = op.mark_.padw;

    return 0;
}
