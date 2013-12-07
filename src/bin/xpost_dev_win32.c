/*
 * Xpost - a Level-2 Postscript interpreter
 * Copyright (C) 2013, Michael Joshua Ryan
 * Copyright (C) 2013, Vincent Torri
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
#include <stdlib.h> /* malloc free */
#include <string.h>

#ifndef WIN32_LEAN_AND_MEAN
# define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN

#include "xpost_log.h"
#include "xpost_memory.h"  /* save/restore works with mtabs */
#include "xpost_object.h"  /* save/restore examines objects */
#include "xpost_stack.h"  /* save/restore manipulates (internal) stacks */

#include "xpost_error.h"
#include "xpost_context.h"
#include "xpost_dict.h"
#include "xpost_string.h"
#include "xpost_name.h"
#include "xpost_operator.h"
#include "xpost_op_dict.h"
#include "xpost_dev_win32.h"

#ifdef abs
# undef abs
#endif
#define abs(a) ((a) < 0) ? -(a) : (a)

typedef struct _BITMAPINFO_XPOST
{
   BITMAPINFOHEADER bih;
   DWORD masks[3];
} BITMAPINFO_XPOST;

typedef struct
{
    HINSTANCE instance;
    HWND window;
    HDC ctx;
    BITMAPINFO_XPOST *bitmap_info;
    HBITMAP bitmap;
    unsigned int *buf;
    int width;
    int height;
} PrivateData;

static
unsigned int _event_handler_opcode;

static
Xpost_Object namePrivate;
static
Xpost_Object namewidth;
static
Xpost_Object nameheight;

static
int _event_handler (Xpost_Context *ctx,
                    Xpost_Object devdic)
{
    Xpost_Object privatestr;
    PrivateData private;
    MSG msg;

    /* load private data struct from string */
    privatestr = bdcget(ctx, devdic, namePrivate);
    if (xpost_object_get_type(privatestr) == invalidtype)
        return undefined;
    xpost_memory_get(xpost_context_select_memory(ctx, privatestr),
                     xpost_object_get_ent(privatestr), 0, sizeof private, &private);

    if (PeekMessage(&msg, private.window, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}


static
unsigned int _create_cont_opcode;

static LRESULT CALLBACK
_xpost_dev_win32_procedure(HWND   window,
                           UINT   message,
                           WPARAM window_param,
                           LPARAM data_param)
{
    switch (message)
    {
        default:
            return DefWindowProc(window, message, window_param, data_param);
    }
}

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
    bdcput(ctx, classdic,
            //consname(ctx, "width")
            namewidth
            , width);
    bdcput(ctx, classdic,
            //consname(ctx, "height")
            nameheight
            , height);

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
    integer width = w.int_.val;
    integer height = h.int_.val;
    WNDCLASSEX wc;
    RECT rect;

    /* create a string to contain device data structure */
    privatestr = consbst(ctx, sizeof(PrivateData), NULL);
    if (xpost_object_get_type(privatestr) == invalidtype)
    {
        XPOST_LOG_ERR("cannot allocate private data structure");
        return unregistered;
    }
    bdcput(ctx, devdic,
            //consname(ctx, "Private")
            namePrivate
            , privatestr);

    /* create and map window */
    private.instance = GetModuleHandle(NULL);

    memset (&wc, 0, sizeof (WNDCLASSEX));
    wc.cbSize = sizeof (WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = _xpost_dev_win32_procedure;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = private.instance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(1 + COLOR_BTNFACE);
    wc.lpszMenuName =  NULL;
    wc.lpszClassName = "XPOST_DEV_WIN32";
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    if(!RegisterClassEx(&wc))
    {
        XPOST_LOG_ERR("RegisterClass() failed");
        goto free_library;
    }

    rect.left = 0;
    rect.top = 0;
    rect.right = width;
    rect.bottom = height;
    if (!AdjustWindowRectEx(&rect, WS_OVERLAPPEDWINDOW | WS_SIZEBOX, FALSE, 0))
    {
        XPOST_LOG_ERR("AdjustWindowRect() failed");
        goto unregister_class;
    }

    private.window = CreateWindow("XPOST_DEV_WIN32", "",
                                  WS_OVERLAPPEDWINDOW | WS_SIZEBOX,
                                  0, 0,
                                  rect.right - rect.left,
                                  rect.bottom - rect.top,
                                  NULL, NULL,
                                  private.instance, NULL);

    if (!private.window)
    {
        XPOST_LOG_ERR("CreateWindowEx() failed");
        goto unregister_class;
    }

    private.width = width;
    private.height = height;

    private.ctx = GetDC(private.window);
    if (!private.ctx)
    {
        XPOST_LOG_ERR("GetDC() failed");
        goto destroy_window;
    }

    private.bitmap_info = (BITMAPINFO_XPOST *)malloc(sizeof(BITMAPINFO_XPOST));
    if (!private.bitmap_info)
    {
        XPOST_LOG_ERR("allocating bitmap info data failed");
        goto release_dc;
    }

    private.bitmap_info->bih.biSize = sizeof(BITMAPINFOHEADER);
    private.bitmap_info->bih.biWidth = width;
    private.bitmap_info->bih.biHeight = -height;
    private.bitmap_info->bih.biPlanes = 1;
    private.bitmap_info->bih.biSizeImage = 4 * width * height;
    private.bitmap_info->bih.biXPelsPerMeter = 0;
    private.bitmap_info->bih.biYPelsPerMeter = 0;
    private.bitmap_info->bih.biClrUsed = 0;
    private.bitmap_info->bih.biClrImportant = 0;
    private.bitmap_info->bih.biBitCount = 32;
    private.bitmap_info->bih.biCompression = BI_BITFIELDS;
    private.bitmap_info->masks[0] = 0x00ff0000;
    private.bitmap_info->masks[1] = 0x0000ff00;
    private.bitmap_info->masks[2] = 0x000000ff;

    private.bitmap = CreateDIBSection(private.ctx,
                                      (const BITMAPINFO *)private.bitmap_info,
                                      DIB_RGB_COLORS,
                                      (void **)(&private.buf),
                                      NULL,
                                      0);
    if (!private.bitmap)
    {
        XPOST_LOG_ERR("CreateDIBSection() failed");
        goto free_bitmap_info;
    }

    ShowWindow(private.window, SW_SHOWNORMAL);
    if (!UpdateWindow(private.window))
    {
        XPOST_LOG_ERR("UpdateWindow() failed");
        goto destroy_window;
    }

    xpost_context_install_event_handler(ctx,
            operfromcode(_event_handler_opcode),
            devdic);

    /* save private data struct in string */
    xpost_memory_put(xpost_context_select_memory(ctx, privatestr),
                     xpost_object_get_ent(privatestr), 0, sizeof(private), &private);

    /* return device instance dictionary to ps */
    xpost_stack_push(ctx->lo, ctx->os, devdic);
    return 0;

  free_bitmap_info:
    free(private.bitmap_info);
  release_dc:
    ReleaseDC(private.window, private.ctx);
  destroy_window:
    DestroyWindow(private.window);
  unregister_class:
    UnregisterClass("XPOST_DEV_WIN32", private.instance);
  free_library:
    FreeLibrary(private.instance);
    return unregistered;
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
    HDC dc;

    /* fold numbers to integertype */
    if (xpost_object_get_type(red) == realtype)
        red = xpost_cons_int(red.real_.val * 255.0);
    else
        red.int_.val *= 255;
    if (xpost_object_get_type(green) == realtype)
        green = xpost_cons_int(green.real_.val * 255.0);
    else
        green.int_.val *= 255;
    if (xpost_object_get_type(blue) == realtype)
        blue = xpost_cons_int(blue.real_.val * 255.0);
    else
        blue.int_.val *= 255;
    if (xpost_object_get_type(x) == realtype)
        x = xpost_cons_int(x.real_.val);
    if (xpost_object_get_type(y) == realtype)
        y = xpost_cons_int(y.real_.val);

    /* load private data struct from string */
    privatestr = bdcget(ctx, devdic, namePrivate);
    if (xpost_object_get_type(privatestr) == invalidtype)
        return undefined;
    xpost_memory_get(xpost_context_select_memory(ctx, privatestr),
                     xpost_object_get_ent(privatestr), 0, sizeof private, &private);

    /* check bounds */
    if (x.int_.val < 0 || x.int_.val >= bdcget(ctx, devdic,
                //consname(ctx, "width")
                namewidth
                ).int_.val)
        return 0;
    if (y.int_.val < 0 || y.int_.val >= bdcget(ctx, devdic,
                //consname(ctx, "height")
                nameheight
                ).int_.val)
        return 0;

    private.buf[y.int_.val * private.width + x.int_.val] =
        red.int_.val << 16 | green.int_.val << 8 | blue.int_.val;

    dc = CreateCompatibleDC(private.ctx);
    SelectObject(dc, private.bitmap);
    BitBlt(private.ctx, x.int_.val, y.int_.val, 1, 1,
           dc, x.int_.val, y.int_.val, SRCCOPY);
    DeleteDC(dc);

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

    xpost_stack_push(ctx->lo, ctx->os,
        xpost_cons_int( (private.buf[y.int_.val * private.width + x.int_.val]
                >> 16) & 0xFF));
    xpost_stack_push(ctx->lo, ctx->os,
        xpost_cons_int( (private.buf[y.int_.val * private.width + x.int_.val]
            >> 8) & 0xFF));
    xpost_stack_push(ctx->lo, ctx->os,
        xpost_cons_int( private.buf[y.int_.val * private.width + x.int_.val]
            & 0xFF));
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
    HDC dc;
    int _x1;
    int _x2;
    int _y1;
    int _y2;
    int deltax;
    int deltay;
    int x;
    int y;
    int s1;
    int s2;
    int interchange;
    int err;
    int i;

    /* fold numbers to integertype */
    if (xpost_object_get_type(red) == realtype)
        red = xpost_cons_int(red.real_.val * 255.0);
    else
        red.int_.val *= 255;
    if (xpost_object_get_type(green) == realtype)
        green = xpost_cons_int(green.real_.val * 255.0);
    else
        green.int_.val *= 255;
    if (xpost_object_get_type(blue) == realtype)
        blue = xpost_cons_int(blue.real_.val * 255.0);
    else
        blue.int_.val *= 255;
    if (xpost_object_get_type(x1) == realtype)
        x1 = xpost_cons_int(x1.real_.val);
    if (xpost_object_get_type(y1) == realtype)
        y1 = xpost_cons_int(y1.real_.val);
    if (xpost_object_get_type(x2) == realtype)
        x2 = xpost_cons_int(x2.real_.val);
    if (xpost_object_get_type(y2) == realtype)
        y2 = xpost_cons_int(y2.real_.val);

    /* load private data struct from string */
    privatestr = bdcget(ctx, devdic, namePrivate);
    if (xpost_object_get_type(privatestr) == invalidtype)
        return undefined;
    xpost_memory_get(xpost_context_select_memory(ctx, privatestr),
            xpost_object_get_ent(privatestr), 0, sizeof private, &private);

    _x1 = x1.int_.val;
    _x2 = x2.int_.val;
    _y1 = y1.int_.val;
    _y2 = y2.int_.val;

    XPOST_LOG_INFO("_drawline(%d, %d, %d, %d)",
            _x1, _y1, _x2, _y2);

    if (_x1 == _x2)
    {
        if (_y1 > _y2)
        {
            int tmp;

            tmp = _y1;
            _y1 = _y2;
            _y2 = tmp;
        }
        for (y = _y1; y <= _y2; y++)
            private.buf[y * private.width + _x1] =
                red.int_.val << 16 | green.int_.val << 8 | blue.int_.val;

        dc = CreateCompatibleDC(private.ctx);
        SelectObject(dc, private.bitmap);
        BitBlt(private.ctx, _x1, _y1, 1, _y2 - _y1 + 1,
               dc, _x1, _y1, SRCCOPY);
        DeleteDC(dc);

        return 0;
    }

    if (_y1 == _y2)
    {
        if (_x1 > _x2)
        {
            int tmp;

            tmp = _x1;
            _x1 = _x2;
            _x2 = tmp;
        }
        for (x = _x1; x <= _x2; x++)
            private.buf[_y1 * private.width + x] =
                red.int_.val << 16 | green.int_.val << 8 | blue.int_.val;

        dc = CreateCompatibleDC(private.ctx);
        SelectObject(dc, private.bitmap);
        BitBlt(private.ctx, _x1, _y1, _x2 - _x1 + 1, 1,
               dc, _x1, _y1, SRCCOPY);
        DeleteDC(dc);

        return 0;
    }

    x = _x1;
    y = _y1;
    deltax = abs(_x2 - _x1);
    s1 = ((_x2 - _x1) < 0) ? - 1 : 1;
    deltay = abs(_y2 - _y1);
    s2 = ((_y2 - _y1) < 0) ? -1 : 1;
    interchange = (deltay > deltax);
    if (interchange)
    {
        int tmp;

        tmp = deltax;
        deltax = deltay;
        deltay = tmp;
    }
    err = 2 * deltay - deltax;
    for (i = 1; i <= deltax; ++i)
    {
        private.buf[y * private.width + x] =
            red.int_.val << 16 | green.int_.val << 8 | blue.int_.val;
        while (err >= 0)
        {
            if (interchange)
                x += s1;
            else
                y += s2;
            err -= 2 * deltax;
        }
        if (interchange)
            y += s2;
        else
            x += s1;
        err += 2 * deltay;
    }

    if (_x1 > _x2)
    {
        int tmp;

        tmp = _x1;
        _x1 = _x2;
        _x2 = tmp;
    }

    if (_y1 > _y2)
    {
        int tmp;

        tmp = _y1;
        _y1 = _y2;
        _y2 = tmp;
    }


    dc = CreateCompatibleDC(private.ctx);
    SelectObject(dc, private.bitmap);
    BitBlt(private.ctx, _x1, _y1, _x2 - _x1 + 1, _y2 - _y1 + 1,
           dc, _x1, _y1, SRCCOPY);
    DeleteDC(dc);

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
    HDC dc;
    int w;
    int h;
    int i;
    int j;

	if (strcmp(ctx->device_str, "gl") == 0)
		/* do opengl stuff */;

    /* fold numbers to integertype */
    if (xpost_object_get_type(red) == realtype)
        red = xpost_cons_int(red.real_.val * 255.0);
    else
        red.int_.val *= 255;
    if (xpost_object_get_type(green) == realtype)
        green = xpost_cons_int(green.real_.val * 255.0);
    else
        green.int_.val *= 255;
    if (xpost_object_get_type(blue) == realtype)
        blue = xpost_cons_int(blue.real_.val * 255.0);
    else
        blue.int_.val *= 255;
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
    privatestr = bdcget(ctx, devdic, namePrivate);
    if (xpost_object_get_type(privatestr) == invalidtype)
        return undefined;
    xpost_memory_get(xpost_context_select_memory(ctx, privatestr),
            xpost_object_get_ent(privatestr), 0, sizeof private, &private);
    w = bdcget(ctx, devdic,
            //consname(ctx,"width")
            namewidth
            ).int_.val;
    h = bdcget(ctx, devdic,
            //consname(ctx,"height")
            nameheight
            ).int_.val;

    if (x.int_.val >= w || y.int_.val >= h)
        return 0;
    if (x.int_.val + width.int_.val > w)
        width.int_.val = w - x.int_.val;
    if (y.int_.val + height.int_.val > h)
        height.int_.val = h - y.int_.val;

    for (i = 0; i < height.int_.val; i++)
    {
        for (j = 0; j < width.int_.val; j++)
        {
            private.buf[(y.int_.val + i) * private.width + x.int_.val + j] =
                red.int_.val << 16 | green.int_.val << 8 | blue.int_.val;
        }
    }

    dc = CreateCompatibleDC(private.ctx);
    SelectObject(dc, private.bitmap);
    BitBlt(private.ctx, x.int_.val, y.int_.val, width.int_.val, height.int_.val,
           dc, x.int_.val, y.int_.val, SRCCOPY);
    DeleteDC(dc);

    return 0;
}

static
int _emit (Xpost_Context *ctx,
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

    UpdateWindow(private.window);

    return 0;
}

static
int _destroy (Xpost_Context *ctx,
              Xpost_Object devdic)
{
    Xpost_Object privatestr;
    PrivateData private;

    /* load private data struct from string */
    privatestr = bdcget(ctx, devdic, namePrivate);
    if (xpost_object_get_type(privatestr) == invalidtype)
        return undefined;
    xpost_memory_get(xpost_context_select_memory(ctx, privatestr), xpost_object_get_ent(privatestr), 0,
                     sizeof(private), &private);

    xpost_context_install_event_handler(ctx, null, null);

    free(private.bitmap_info);
    ReleaseDC(private.window, private.ctx);
    DestroyWindow(private.window);

    if (!UnregisterClass("XPOST_DEV_WIN32", private.instance))
        XPOST_LOG_INFO("UnregisterClass() failed");

    if (!FreeLibrary(private.instance))
        XPOST_LOG_INFO("FreeLibrary() failed");

    return 0;
}

/* operator function to instantiate a new window device.
   installed in userdict by calling 'loadXXXdevice'.
*/
static
int newwin32device (Xpost_Context *ctx,
                    Xpost_Object width,
                    Xpost_Object height)
{
    Xpost_Object classdic;
    int ret;

    xpost_stack_push(ctx->lo, ctx->os, width);
    xpost_stack_push(ctx->lo, ctx->os, height);
    ret = Aload(ctx, consname(ctx, "win32DEVICE"));
    if (ret)
        return ret;
    classdic = xpost_stack_topdown_fetch(ctx->lo, ctx->os, 0);
    if (!xpost_stack_push(ctx->lo, ctx->es, bdcget(ctx, classdic, consname(ctx, "Create"))))
        return execstackoverflow;

    return 0;
}

static
unsigned int _loadwin32devicecont_opcode;

/* Specializes or sub-classes the PPMIMAGE device class.
   load PPMIMAGE
   load and call ps procedure .copydict which leaves copy on stack
   call loadXXXdevicecont by continuation.
*/
static
int loadwin32device (Xpost_Context *ctx)
{
    Xpost_Object classdic;
    int ret;

    ret = Aload(ctx, consname(ctx, "PPMIMAGE"));
    if (ret)
        return ret;
    classdic = xpost_stack_topdown_fetch(ctx->lo, ctx->os, 0);
    if (!xpost_stack_push(ctx->lo, ctx->es, operfromcode(_loadwin32devicecont_opcode)))
        return execstackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->es, bdcget(ctx, classdic, consname(ctx, ".copydict"))))
        return execstackoverflow;

    return 0;
}

/* replace procedures in the class with newly created special operators.
   defines the device class XXXDEVICE in userdict.
   defines a new operator in userdict: newXXXdevice
*/
static
int loadwin32devicecont (Xpost_Context *ctx,
                         Xpost_Object classdic)
{
    Xpost_Object userdict;
    Xpost_Object op;

    bdcput(ctx, classdic, consname(ctx, "nativecolorspace"), consname(ctx, "DeviceRGB"));

    op = consoper(ctx, "win32CreateCont", _create_cont, 1, 3, integertype, integertype, dicttype);
    _create_cont_opcode = op.mark_.padw;
    op = consoper(ctx, "win32Create", _create, 1, 3, integertype, integertype, dicttype);
    bdcput(ctx, classdic, consname(ctx, "Create"), op);

    op = consoper(ctx, "win32PutPix", _putpix, 0, 6,
            numbertype, numbertype, numbertype, /* r g b color values */
            numbertype, numbertype, /* x y coords */
            dicttype); /* devdic */
    bdcput(ctx, classdic, consname(ctx, "PutPix"), op);

    op = consoper(ctx, "win32GetPix", _getpix, 3, 3,
            numbertype, numbertype, dicttype);
    bdcput(ctx, classdic, consname(ctx, "GetPix"), op);

    op = consoper(ctx, "win32DrawLine", _drawline, 0, 8,
            numbertype, numbertype, numbertype, /* r g b color values */
            numbertype, numbertype, /* x1 y1 */
            numbertype, numbertype, /* x2 y2 */
            dicttype); /* devdic */
    bdcput(ctx, classdic, consname(ctx, "DrawLine"), op);

    op = consoper(ctx, "win32FillRect", _fillrect, 0, 8,
            numbertype, numbertype, numbertype, /* r g b color values */
            numbertype, numbertype, /* x y coords */
            numbertype, numbertype, /* width height */
            dicttype); /* devdic */
    bdcput(ctx, classdic, consname(ctx, "FillRect"), op);

    op = consoper(ctx, "win32Emit", _emit, 0, 1, dicttype);
    bdcput(ctx, classdic, consname(ctx, "Emit"), op);

    op = consoper(ctx, "win32Destroy", _destroy, 0, 1, dicttype);
    bdcput(ctx, classdic, consname(ctx, "Destroy"), op);

    userdict = xpost_stack_bottomup_fetch(ctx->lo, ctx->ds, 2);

    bdcput(ctx, userdict, consname(ctx, "win32DEVICE"), classdic);

    op = consoper(ctx, "newwin32device", newwin32device, 1, 2, integertype, integertype);
    bdcput(ctx, userdict, consname(ctx, "newwin32device"), op);

    op = consoper(ctx, "win32EventHandler", _event_handler, 0, 1, dicttype);
    _event_handler_opcode = op.mark_.padw;

    return 0;
}

/*
   install the loadXXXdevice which may be called during graphics initialization
   to produce the operator newXXXdevice
   which creates the device instance dictionary.
*/
int initwin32ops (Xpost_Context *ctx,
                  Xpost_Object sd)
{
    unsigned int optadr;
    oper *optab;
    Xpost_Object n,op;

    namePrivate = consname(ctx, "Private");
    namewidth = consname(ctx, "width");
    nameheight = consname(ctx, "height");

    xpost_memory_table_get_addr(ctx->gl,
                                XPOST_MEMORY_TABLE_SPECIAL_OPERATOR_TABLE,
                                &optadr);
    optab = (oper *)(ctx->gl->base + optadr);
	op = consoper(ctx, "loadwin32device", loadwin32device, 1, 0); INSTALL;
	op = consoper(ctx, "loadwin32devicecont", loadwin32devicecont, 1, 1, dicttype);
	_loadwin32devicecont_opcode = op.mark_.padw;

    return 0;
}
