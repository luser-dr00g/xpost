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
#include <string.h>

#include <xcb/xcb.h>

#include "xpost_log.h"
#include "xpost_memory.h"  /* save/restore works with mtabs */
#include "xpost_object.h"  /* save/restore examines objects */
#include "xpost_stack.h"  /* save/restore manipulates (internal) stacks */

#include "xpost_context.h"
#include "xpost_dict.h"
#include "xpost_string.h"
#include "xpost_name.h"
#include "xpost_operator.h"
#include "xpost_op_dict.h"
#include "xpost_dev_xcb.h"

typedef struct {
    xcb_connection_t *c;
    int screenNum;
    xcb_screen_t *screen;
    xcb_window_t window;
    xcb_gcontext_t gc;
    xcb_colormap_t colormap;
} PrivateData;


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
    xpost_stack_push(ctx->lo, ctx->es,
            operfromcode(_create_cont_opcode));
    xpost_stack_push(ctx->lo, ctx->es,
            bdcget(ctx, classdic, consname(ctx, ".copydict")));

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
    integer width = w.int_.val;
    integer height = h.int_.val;
    int i;
    const xcb_setup_t *setup;
    xcb_screen_iterator_t iter;

    privatestr = consbst(ctx, sizeof(PrivateData), NULL);
    bdcput(ctx, devdic, consname(ctx, "Private"), privatestr);
    bdcput(ctx, devdic, consname(ctx, "width"), w);
    bdcput(ctx, devdic, consname(ctx, "height"), h);

    private.c = xcb_connect(NULL, &private.screenNum);
    setup = xcb_get_setup(private.c);
    iter = xcb_setup_roots_iterator(setup);

    for (i=0; i < private.screenNum; ++i)
        xcb_screen_next(&iter);

    private.screen = iter.data;
    XPOST_LOG_INFO("screen->root_depth: %d", private.screen->root_depth);

    private.window = xcb_generate_id(private.c);
    {
        unsigned int values = private.screen->white_pixel;
        xcb_create_window(private.c, XCB_COPY_FROM_PARENT,
                private.window, private.screen->root,
                0, 0,
                width, height,
                5,
                XCB_WINDOW_CLASS_INPUT_OUTPUT,
                private.screen->root_visual,
                XCB_CW_BACK_PIXEL,
                &values);
    }
    xcb_map_window(private.c, private.window);
    xcb_flush(private.c);

    private.gc = xcb_generate_id(private.c);
    {
        unsigned int values[2] = {
            private.screen->black_pixel,
            private.screen->white_pixel
        } ;
        xcb_create_gc(private.c, private.gc, private.window,
                XCB_GC_FOREGROUND | XCB_GC_BACKGROUND,
                values);
    }

    private.colormap = private.screen->default_colormap;

    xpost_memory_put(xpost_context_select_memory(ctx, privatestr),
            privatestr.comp_.ent, 0, sizeof private, &private);

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

    if (xpost_object_get_type(val) == realtype)
        val = xpost_cons_int(val.real_.val);
    if (xpost_object_get_type(x) == realtype)
        x = xpost_cons_int(x.real_.val);
    if (xpost_object_get_type(y) == realtype)
        y = xpost_cons_int(y.real_.val);

    privatestr = bdcget(ctx, devdic, consname(ctx, "Private"));
    xpost_memory_get(xpost_context_select_memory(ctx, privatestr),
            privatestr.comp_.ent, 0, sizeof private, &private);

    {
        xcb_point_t p;
        p.x = x.int_.val;
        p.y = y.int_.val;

        xcb_poly_point(private.c,
                XCB_COORD_MODE_ORIGIN,
                private.window,
                private.gc,
                1, 
                &p);
    }

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

    privatestr = bdcget(ctx, devdic, consname(ctx, "Private"));
    xpost_memory_get(xpost_context_select_memory(ctx, privatestr),
            privatestr.comp_.ent, 0, sizeof private, &private);

    /* ?? I don't know ...
       make a 1-pixel image and use copy_area?  ... */
    return 0;
}

static
int _emit (Xpost_Context *ctx,
           Xpost_Object devdic)
{
    Xpost_Object privatestr;
    PrivateData private;

    privatestr = bdcget(ctx, devdic, consname(ctx, "Private"));
    xpost_memory_get(xpost_context_select_memory(ctx, privatestr),
            privatestr.comp_.ent, 0, sizeof private, &private);

    xcb_flush(private.c);

    return 0;
}

static
int _destroy (Xpost_Context *ctx,
              Xpost_Object devdic)
{
    Xpost_Object privatestr;
    PrivateData private;

    privatestr = bdcget(ctx, devdic, consname(ctx, "Private"));
    xpost_memory_get(xpost_context_select_memory(ctx, privatestr), privatestr.comp_.ent, 0,
            sizeof private, &private);

    xcb_disconnect(private.c);

    return 0;
}


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
    xpost_stack_push(ctx->lo, ctx->es, bdcget(ctx, classdic, consname(ctx, "Create")));

    return 0;
}

static
unsigned int _loadxcbdevicecont_opcode;

/* load PGMIMAGE
   load and call .copydict
   leaves copy on stack */
static
int loadxcbdevice (Xpost_Context *ctx)
{
    Xpost_Object classdic;
    int ret;

    ret = Aload(ctx, consname(ctx, "PGMIMAGE"));
    if (ret)
        return ret;
    classdic = xpost_stack_topdown_fetch(ctx->lo, ctx->os, 0);
    xpost_stack_push(ctx->lo, ctx->es, operfromcode(_loadxcbdevicecont_opcode));
    xpost_stack_push(ctx->lo, ctx->es, bdcget(ctx, classdic, consname(ctx, ".copydict")));

    return 0;
}

/* replace procedures with operators */
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

    op = consoper(ctx, "xcbEmit", _emit, 0, 1, dicttype);
    bdcput(ctx, classdic, consname(ctx, "Emit"), op);

    op = consoper(ctx, "xcbDestroy", _destroy, 0, 1, dicttype);
    bdcput(ctx, classdic, consname(ctx, "Destroy"), op);

    userdict = xpost_stack_bottomup_fetch(ctx->lo, ctx->ds, 2);

    bdcput(ctx, userdict, consname(ctx, "xcbDEVICE"), classdic);

    op = consoper(ctx, "newxcbdevice", newxcbdevice, 1, 2, integertype, integertype);
    bdcput(ctx, userdict, consname(ctx, "newxcbdevice"), op);

    return 0;
}

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

