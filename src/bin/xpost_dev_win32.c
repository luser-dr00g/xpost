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

#include <GL/gl.h>

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

typedef enum
{
    RENDER_BACKEND_GDI,
    RENDER_BACKEND_GL,
} Render_Backend;

typedef struct
{
    HINSTANCE instance;
    HWND window;
    int width;
    int height;
} PrivateData;

typedef struct
{
   BITMAPINFOHEADER bih;
   DWORD masks[3];
} BITMAPINFO_XPOST;

typedef struct
{
    BITMAPINFO_XPOST *bitmap_info;
    HBITMAP bitmap;
    unsigned int *buf;
} Render_Data_Gdi;

typedef struct
{
    HGLRC glrc;
    unsigned int changed : 1;
} Render_Data_Gl;

typedef struct
{
    Render_Backend backend_type;
    HDC dc;
    union
    {
        Render_Data_Gdi gdi;
        Render_Data_Gl gl;
    } backend;
} Render_Data;

static unsigned int _event_handler_opcode;
static unsigned int _create_cont_opcode;

static Xpost_Object namePrivate;
static Xpost_Object namewidth;
static Xpost_Object nameheight;
static Xpost_Object namedotcopydict;

static void
_xpost_dev_gl_win32_viewport_set(int width, int height)
{
    /* TODO : checking if size change (for later) */
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, width, 0, height, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glScalef(1, -1, 1);
    glTranslatef(0, -height, 0);
}

static
int _event_handler (Xpost_Context *ctx,
                    Xpost_Object devdic)
{
    Xpost_Object privatestr;
    PrivateData private;
    MSG msg;

    /* load private data struct from string */
    privatestr = xpost_dict_get(ctx, devdic, namePrivate);
    if (xpost_object_get_type(privatestr) == invalidtype)
        return undefined;
    xpost_memory_get(xpost_context_select_memory(ctx, privatestr),
                     xpost_object_get_ent(privatestr), 0, sizeof private, &private);

    while (PeekMessage(&msg, private.window, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}



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
    int ret;

    xpost_stack_push(ctx->lo, ctx->os, width);
    xpost_stack_push(ctx->lo, ctx->os, height);
    xpost_stack_push(ctx->lo, ctx->os, classdic);
    ret = xpost_dict_put(ctx, classdic, namewidth, width);
    if (ret)
        return ret;
    ret = xpost_dict_put(ctx, classdic, nameheight, height);
    if (ret)
        return ret;

     /* call device class's ps-level .copydict procedure,
        then call _create_cont, by continuation. */
    if (!xpost_stack_push(ctx->lo, ctx->es, operfromcode(_create_cont_opcode)))
        return execstackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->es, xpost_dict_get(ctx, classdic,
                    namedotcopydict)))
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
    Render_Data *rd;
    integer width = w.int_.val;
    integer height = h.int_.val;
    WNDCLASSEX wc;
    RECT rect;
    HICON icon = NULL;
    HICON icon_sm = NULL;
    int ret;

    /* create a string to contain device data structure */
    privatestr = xpost_string_cons(ctx, sizeof(PrivateData), NULL);
    if (xpost_object_get_type(privatestr) == invalidtype)
    {
        XPOST_LOG_ERR("cannot allocate private data structure");
        return unregistered;
    }
    ret = xpost_dict_put(ctx, devdic, namePrivate, privatestr);
    if (ret)
        return ret;

    /* create and map window */
    private.instance = GetModuleHandle(NULL);
    if (!private.instance)
        return unregistered;

    icon = LoadImage(private.instance,
                     MAKEINTRESOURCE(101),
                     IMAGE_ICON,
                     GetSystemMetrics(SM_CXICON),
                     GetSystemMetrics(SM_CYICON),
                     LR_DEFAULTCOLOR);
    if (!icon)
        icon = LoadIcon(NULL, IDI_APPLICATION);

    icon_sm = LoadImage(private.instance,
                        MAKEINTRESOURCE(101),
                        IMAGE_ICON,
                        GetSystemMetrics(SM_CXSMICON),
                        GetSystemMetrics(SM_CYSMICON),
                        LR_DEFAULTCOLOR);
    if (!icon_sm)
        icon_sm = LoadIcon(NULL, IDI_APPLICATION);

    memset (&wc, 0, sizeof (WNDCLASSEX));
    wc.cbSize = sizeof (WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = _xpost_dev_win32_procedure;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = private.instance;
    wc.hIcon = icon;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(1 + COLOR_BTNFACE);
    wc.lpszMenuName =  NULL;
    wc.lpszClassName = "XPOST_DEV_WIN32";
    wc.hIconSm = icon_sm;

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

    SetWindowText(private.window, "Xpost");

    rd = (Render_Data *)malloc(sizeof(Render_Data_Gdi));
    if (!rd)
    {
        XPOST_LOG_ERR("allocation of memory failed");
        goto destroy_window;
    }

    rd->dc = GetDC(private.window);
    if (!rd->dc)
    {
        XPOST_LOG_ERR("GetDC() failed");
        goto free_rd;
    }

    if (strcmp(ctx->device_str, "gl") == 0)
    {
        PIXELFORMATDESCRIPTOR pfd;
        HGLRC glrc;
        int pixel_format;
        LONG_PTR res;

        ZeroMemory(&pfd, sizeof (pfd));
        pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 24;
        pfd.cDepthBits = 32;
        pfd.iLayerType = PFD_MAIN_PLANE;

        pixel_format = ChoosePixelFormat(rd->dc, &pfd);
        if (!pixel_format)
        {
            XPOST_LOG_ERR("ChoosePixelFormat() failed");
            goto release_dc;
        }

        if (!SetPixelFormat(rd->dc, pixel_format, &pfd))
        {
            XPOST_LOG_ERR("SetPixelFormat() failed");
            goto release_dc;
        }

        glrc = wglCreateContext(rd->dc);
        if (!glrc)
        {
            XPOST_LOG_ERR("wglCreateContext() failed %ld", GetLastError());
            goto release_dc;
        }

        if (!wglMakeCurrent(rd->dc, glrc))
        {
            XPOST_LOG_ERR("wglMakeCurrent() failed");
            wglDeleteContext(glrc);
            goto release_dc;
        }

        rd->backend_type = RENDER_BACKEND_GL;
        rd->backend.gl.glrc = glrc;
        rd->backend.gl.changed = 0;

        SetLastError(0);
        res = SetWindowLongPtr(private.window, GWLP_USERDATA, (LONG_PTR)rd);
        if ((res == 0) && (GetLastError() != 0))
        {
            XPOST_LOG_ERR("SetWindowLongPtr() failed %ld", GetLastError());
            wglDeleteContext(glrc);
            goto release_dc;
        }

        /* set the viewport to be a 2D rectangle of size width and height */
        _xpost_dev_gl_win32_viewport_set(width, height);
    }
    else
    {
        BITMAPINFO_XPOST *bitmap_info;
        HBITMAP bitmap;
        unsigned int *buf;
        LONG_PTR res;

        bitmap_info = (BITMAPINFO_XPOST *)malloc(sizeof(BITMAPINFO_XPOST));
        if (!bitmap_info)
        {
            XPOST_LOG_ERR("allocating bitmap info data failed");
            goto release_dc;
        }

        bitmap_info->bih.biSize = sizeof(BITMAPINFOHEADER);
        bitmap_info->bih.biWidth = width;
        bitmap_info->bih.biHeight = -height;
        bitmap_info->bih.biPlanes = 1;
        bitmap_info->bih.biSizeImage = 4 * width * height;
        bitmap_info->bih.biXPelsPerMeter = 0;
        bitmap_info->bih.biYPelsPerMeter = 0;
        bitmap_info->bih.biClrUsed = 0;
        bitmap_info->bih.biClrImportant = 0;
        bitmap_info->bih.biBitCount = 32;
        bitmap_info->bih.biCompression = BI_BITFIELDS;
        bitmap_info->masks[0] = 0x00ff0000;
        bitmap_info->masks[1] = 0x0000ff00;
        bitmap_info->masks[2] = 0x000000ff;

        bitmap = CreateDIBSection(rd->dc,
                                  (const BITMAPINFO *)bitmap_info,
                                  DIB_RGB_COLORS,
                                  (void **)(&buf),
                                  NULL,
                                  0);
        if (!bitmap)
        {
            XPOST_LOG_ERR("CreateDIBSection() failed");
            free(bitmap_info);
            goto release_dc;
        }

        rd->backend_type = RENDER_BACKEND_GDI;
        rd->backend.gdi.bitmap_info = bitmap_info;
        rd->backend.gdi.bitmap = bitmap;
        rd->backend.gdi.buf = buf;

        SetLastError(0);
        res = SetWindowLongPtr(private.window, GWLP_USERDATA, (LONG_PTR)rd);
        if ((res == 0) && (GetLastError() != 0))
        {
            XPOST_LOG_ERR("SetWindowLongPtr() failed %ld", GetLastError());
            DeleteObject(rd->backend.gdi.bitmap);
            free(bitmap_info);
            goto release_dc;
        }
    }

    private.width = width;
    private.height = height;

    ShowWindow(private.window, SW_SHOWNORMAL);
    if (!UpdateWindow(private.window))
    {
        XPOST_LOG_ERR("UpdateWindow() failed");
        goto free_rd;
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

  release_dc:
    ReleaseDC(private.window, rd->dc);
  free_rd:
    free(rd);
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
    Render_Data *rd;

    /* fold numbers to integertype */
    if (xpost_object_get_type(red) == realtype)
        red = xpost_int_cons(red.real_.val * 255.0);
    else
        red.int_.val *= 255;
    if (xpost_object_get_type(green) == realtype)
        green = xpost_int_cons(green.real_.val * 255.0);
    else
        green.int_.val *= 255;
    if (xpost_object_get_type(blue) == realtype)
        blue = xpost_int_cons(blue.real_.val * 255.0);
    else
        blue.int_.val *= 255;
    if (xpost_object_get_type(x) == realtype)
        x = xpost_int_cons(x.real_.val);
    if (xpost_object_get_type(y) == realtype)
        y = xpost_int_cons(y.real_.val);

    /* load private data struct from string */
    privatestr = xpost_dict_get(ctx, devdic, namePrivate);
    if (xpost_object_get_type(privatestr) == invalidtype)
        return undefined;
    xpost_memory_get(xpost_context_select_memory(ctx, privatestr),
                     xpost_object_get_ent(privatestr), 0, sizeof private, &private);

    /* check bounds */
    if (x.int_.val < 0 || x.int_.val >= xpost_dict_get(ctx, devdic, namewidth).int_.val)
        return 0;
    if (y.int_.val < 0 || y.int_.val >= xpost_dict_get(ctx, devdic, nameheight).int_.val)
        return 0;

    rd = (Render_Data *)GetWindowLongPtr(private.window, GWLP_USERDATA);
    if (!rd)
        return 0;

    switch (rd->backend_type)
    {
        case RENDER_BACKEND_GDI:
        {
            HDC cdc;

            rd->backend.gdi.buf[y.int_.val * private.width + x.int_.val] =
                red.int_.val << 16 | green.int_.val << 8 | blue.int_.val;

            cdc = CreateCompatibleDC(rd->dc);
            SelectObject(cdc, rd->backend.gdi.bitmap);
            BitBlt(rd->dc, x.int_.val, y.int_.val, 1, 1,
                   cdc, x.int_.val, y.int_.val, SRCCOPY);
            DeleteDC(cdc);
            break;
        }
        case RENDER_BACKEND_GL:
            glBegin(GL_POINTS);
            glColor4f(red.int_.val / 255.0f, green.int_.val / 255.0f, blue.int_.val / 255.0f, 1.0f);
            glVertex2f(x.int_.val, y.int_.val);
            glEnd();
            rd->backend.gl.changed = 1;
            break;
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
    Render_Data *rd;

    /* load private data struct from string */
    privatestr = xpost_dict_get(ctx, devdic, namePrivate);
    if (xpost_object_get_type(privatestr) == invalidtype)
        return undefined;
    xpost_memory_get(xpost_context_select_memory(ctx, privatestr),
                     xpost_object_get_ent(privatestr), 0, sizeof private, &private);

    rd = (Render_Data *)GetWindowLongPtr(private.window, GWLP_USERDATA);
    if (!rd)
        return 0;

    switch (rd->backend_type)
    {
        case RENDER_BACKEND_GDI:
            xpost_stack_push(ctx->lo, ctx->os,
                             xpost_int_cons( (rd->backend.gdi.buf[y.int_.val * private.width + x.int_.val]
                                              >> 16) & 0xFF));
            xpost_stack_push(ctx->lo, ctx->os,
                             xpost_int_cons( (rd->backend.gdi.buf[y.int_.val * private.width + x.int_.val]
                                              >> 8) & 0xFF));
            xpost_stack_push(ctx->lo, ctx->os,
                             xpost_int_cons(rd->backend.gdi.buf[y.int_.val * private.width + x.int_.val]
                                            & 0xFF));
            break;
        default:
            break;
    }

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
    Render_Data *rd;
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
        red = xpost_int_cons(red.real_.val * 255.0);
    else
        red.int_.val *= 255;
    if (xpost_object_get_type(green) == realtype)
        green = xpost_int_cons(green.real_.val * 255.0);
    else
        green.int_.val *= 255;
    if (xpost_object_get_type(blue) == realtype)
        blue = xpost_int_cons(blue.real_.val * 255.0);
    else
        blue.int_.val *= 255;
    if (xpost_object_get_type(x1) == realtype)
        x1 = xpost_int_cons(x1.real_.val);
    if (xpost_object_get_type(y1) == realtype)
        y1 = xpost_int_cons(y1.real_.val);
    if (xpost_object_get_type(x2) == realtype)
        x2 = xpost_int_cons(x2.real_.val);
    if (xpost_object_get_type(y2) == realtype)
        y2 = xpost_int_cons(y2.real_.val);

    /* load private data struct from string */
    privatestr = xpost_dict_get(ctx, devdic, namePrivate);
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

    rd = (Render_Data *)GetWindowLongPtr(private.window, GWLP_USERDATA);
    if (!rd)
        return 0;

    switch (rd->backend_type)
    {
        case RENDER_BACKEND_GDI:
        {
            HDC cdc;

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
                    rd->backend.gdi.buf[y * private.width + _x1] =
                        red.int_.val << 16 | green.int_.val << 8 | blue.int_.val;

                cdc = CreateCompatibleDC(rd->dc);
                SelectObject(cdc, rd->backend.gdi.bitmap);
                BitBlt(rd->dc, _x1, _y1, 1, _y2 - _y1 + 1,
                       cdc, _x1, _y1, SRCCOPY);
                DeleteDC(cdc);

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
                    rd->backend.gdi.buf[_y1 * private.width + x] =
                        red.int_.val << 16 | green.int_.val << 8 | blue.int_.val;

                cdc = CreateCompatibleDC(rd->dc);
                SelectObject(cdc, rd->backend.gdi.bitmap);
                BitBlt(rd->dc, _x1, _y1, _x2 - _x1 + 1, 1,
                       cdc, _x1, _y1, SRCCOPY);
                DeleteDC(cdc);

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
                rd->backend.gdi.buf[y * private.width + x] =
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

            cdc = CreateCompatibleDC(rd->dc);
            SelectObject(cdc, rd->backend.gdi.bitmap);
            BitBlt(rd->dc, _x1, _y1, _x2 - _x1 + 1, _y2 - _y1 + 1,
                   cdc, _x1, _y1, SRCCOPY);
            DeleteDC(cdc);
            break;
        }
        case RENDER_BACKEND_GL:
            glBegin(GL_LINES);
            glColor4f(red.int_.val / 255.0f, green.int_.val / 255.0f, blue.int_.val / 255.0f, 1.0f);
            glVertex2f(_x1, _y1);
            glVertex2f(_x2, _y2);
            glEnd();
            rd->backend.gl.changed = 1;
            break;
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
    Render_Data *rd;
    int w;
    int h;

    /* fold numbers to integertype */
    if (xpost_object_get_type(red) == realtype)
        red = xpost_int_cons(red.real_.val * 255.0);
    else
        red.int_.val *= 255;
    if (xpost_object_get_type(green) == realtype)
        green = xpost_int_cons(green.real_.val * 255.0);
    else
        green.int_.val *= 255;
    if (xpost_object_get_type(blue) == realtype)
        blue = xpost_int_cons(blue.real_.val * 255.0);
    else
        blue.int_.val *= 255;
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
    privatestr = xpost_dict_get(ctx, devdic, namePrivate);
    if (xpost_object_get_type(privatestr) == invalidtype)
        return undefined;
    xpost_memory_get(xpost_context_select_memory(ctx, privatestr),
            xpost_object_get_ent(privatestr), 0, sizeof private, &private);
    w = xpost_dict_get(ctx, devdic, namewidth).int_.val;
    h = xpost_dict_get(ctx, devdic, nameheight).int_.val;

    if (x.int_.val >= w || y.int_.val >= h)
        return 0;
    if (x.int_.val + width.int_.val > w)
        width.int_.val = w - x.int_.val;
    if (y.int_.val + height.int_.val > h)
        height.int_.val = h - y.int_.val;

    rd = (Render_Data *)GetWindowLongPtr(private.window, GWLP_USERDATA);
    if (!rd)
        return 0;

    switch (rd->backend_type)
    {
        case RENDER_BACKEND_GDI:
        {
            HDC cdc;
            int i;
            int j;

            for (i = 0; i < height.int_.val; i++)
            {
                for (j = 0; j < width.int_.val; j++)
                {
                    rd->backend.gdi.buf[(y.int_.val + i) * private.width + x.int_.val + j] =
                        red.int_.val << 16 | green.int_.val << 8 | blue.int_.val;
                }
            }

            cdc = CreateCompatibleDC(rd->dc);
            SelectObject(cdc, rd->backend.gdi.bitmap);
            BitBlt(rd->dc, x.int_.val, y.int_.val, width.int_.val, height.int_.val,
                   cdc, x.int_.val, y.int_.val, SRCCOPY);
            DeleteDC(cdc);
            break;
        }
        case RENDER_BACKEND_GL:
            glBegin(GL_QUADS);
            glColor4f(red.int_.val / 255.0f, green.int_.val / 255.0f, blue.int_.val / 255.0f, 1.0f);
            glVertex2f(x.int_.val, y.int_.val);
            glVertex2f(x.int_.val + width.int_.val, y.int_.val);
            glVertex2f(x.int_.val + width.int_.val, y.int_.val + height.int_.val);
            glVertex2f(x.int_.val, y.int_.val + height.int_.val);
            glEnd();
            rd->backend.gl.changed = 1;
            break;
    }

    return 0;
}

static
int _flush (Xpost_Context *ctx,
            Xpost_Object devdic)
{
    Xpost_Object privatestr;
    PrivateData private;
    Render_Data *rd;

    /* load private data struct from string */
    privatestr = xpost_dict_get(ctx, devdic, namePrivate);
    if (xpost_object_get_type(privatestr) == invalidtype)
        return undefined;
    xpost_memory_get(xpost_context_select_memory(ctx, privatestr),
                     xpost_object_get_ent(privatestr), 0, sizeof private, &private);

    rd = (Render_Data *)GetWindowLongPtr(private.window, GWLP_USERDATA);
    if (!rd)
        return 0;

    switch (rd->backend_type)
    {
        case RENDER_BACKEND_GDI:
            UpdateWindow(private.window);
            break;
        case RENDER_BACKEND_GL:
            if (rd->backend.gl.changed)
            {
                wglMakeCurrent(rd->dc, rd->backend.gl.glrc);
                SwapBuffers(rd->dc);
                rd->backend.gl.changed = 0;
            }
            break;
    }

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
    Render_Data *rd;

    /* load private data struct from string */
    privatestr = xpost_dict_get(ctx, devdic, namePrivate);
    if (xpost_object_get_type(privatestr) == invalidtype)
        return undefined;
    xpost_memory_get(xpost_context_select_memory(ctx, privatestr), xpost_object_get_ent(privatestr), 0,
                     sizeof(private), &private);

    xpost_context_install_event_handler(ctx, null, null);

    rd = (Render_Data *)GetWindowLongPtr(private.window, GWLP_USERDATA);
    if (!rd)
        return 0;

    switch (rd->backend_type)
    {
        case RENDER_BACKEND_GDI:
            DeleteObject(rd->backend.gdi.bitmap);
            free(rd->backend.gdi.bitmap_info);
            break;
        case RENDER_BACKEND_GL:
            wglMakeCurrent(NULL, NULL);
            wglDeleteContext(rd->backend.gl.glrc);
            break;
    }

    ReleaseDC(private.window, rd->dc);
    free(rd);
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

    /* note:
       an invalid name should cause an undefined error to propagate
       with extra handling here */

    ret = xpost_op_any_load(ctx, xpost_name_cons(ctx, "win32DEVICE"));
    if (ret)
        return ret;
    classdic = xpost_stack_topdown_fetch(ctx->lo, ctx->os, 0);

    /* xpost_stack_push will also throw an error upon an invalid object
       return from xpost_dict_get */
    if (!xpost_stack_push(ctx->lo, ctx->es, xpost_dict_get(ctx, classdic, xpost_name_cons(ctx, "Create"))))
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

    /* see note in newwin32device above */
    ret = xpost_op_any_load(ctx, xpost_name_cons(ctx, "PPMIMAGE"));
    if (ret)
        return ret;
    classdic = xpost_stack_topdown_fetch(ctx->lo, ctx->os, 0);
    if (!xpost_stack_push(ctx->lo, ctx->es, operfromcode(_loadwin32devicecont_opcode)))
        return execstackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->es, xpost_dict_get(ctx, classdic, namedotcopydict)))
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
    int ret;

    ret = xpost_dict_put(ctx, classdic, xpost_name_cons(ctx, "nativecolorspace"), xpost_name_cons(ctx, "DeviceRGB"));
    if (ret)
        return ret;

    op = consoper(ctx, "win32CreateCont", _create_cont, 1, 3, integertype, integertype, dicttype);
    _create_cont_opcode = op.mark_.padw;
    op = consoper(ctx, "win32Create", _create, 1, 3, integertype, integertype, dicttype);
    ret = xpost_dict_put(ctx, classdic, xpost_name_cons(ctx, "Create"), op);
    if (ret)
        return ret;

    op = consoper(ctx, "win32PutPix", _putpix, 0, 6,
            numbertype, numbertype, numbertype, /* r g b color values */
            numbertype, numbertype, /* x y coords */
            dicttype); /* devdic */
    ret = xpost_dict_put(ctx, classdic, xpost_name_cons(ctx, "PutPix"), op);
    if (ret)
        return ret;

    op = consoper(ctx, "win32GetPix", _getpix, 3, 3,
            numbertype, numbertype, dicttype);
    ret = xpost_dict_put(ctx, classdic, xpost_name_cons(ctx, "GetPix"), op);
    if (ret)
        return ret;

    op = consoper(ctx, "win32DrawLine", _drawline, 0, 8,
            numbertype, numbertype, numbertype, /* r g b color values */
            numbertype, numbertype, /* x1 y1 */
            numbertype, numbertype, /* x2 y2 */
            dicttype); /* devdic */
    ret = xpost_dict_put(ctx, classdic, xpost_name_cons(ctx, "DrawLine"), op);
    if (ret)
        return ret;

    op = consoper(ctx, "win32FillRect", _fillrect, 0, 8,
            numbertype, numbertype, numbertype, /* r g b color values */
            numbertype, numbertype, /* x y coords */
            numbertype, numbertype, /* width height */
            dicttype); /* devdic */
    ret = xpost_dict_put(ctx, classdic, xpost_name_cons(ctx, "FillRect"), op);
    if (ret)
        return ret;

    op = consoper(ctx, "win32Emit", _emit, 0, 1, dicttype);
    ret = xpost_dict_put(ctx, classdic, xpost_name_cons(ctx, "Emit"), op);
    if (ret)
        return ret;

    op = consoper(ctx, "win32Flush", _flush, 0, 1, dicttype);
    ret = xpost_dict_put(ctx, classdic, xpost_name_cons(ctx, "Flush"), op);
    if (ret)
        return ret;

    op = consoper(ctx, "win32Destroy", _destroy, 0, 1, dicttype);
    ret = xpost_dict_put(ctx, classdic, xpost_name_cons(ctx, "Destroy"), op);
    if (ret)
        return ret;

    userdict = xpost_stack_bottomup_fetch(ctx->lo, ctx->ds, 2);

    ret = xpost_dict_put(ctx, userdict, xpost_name_cons(ctx, "win32DEVICE"), classdic);
    if (ret)
        return ret;

    op = consoper(ctx, "newwin32device", newwin32device, 1, 2, integertype, integertype);
    ret = xpost_dict_put(ctx, userdict, xpost_name_cons(ctx, "newwin32device"), op);
    if (ret)
        return ret;

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

    if (xpost_object_get_type(namePrivate = xpost_name_cons(ctx, "Private")) == invalidtype)
        return VMerror;
    if (xpost_object_get_type(namewidth = xpost_name_cons(ctx, "width")) == invalidtype)
        return VMerror;
    if (xpost_object_get_type(nameheight = xpost_name_cons(ctx, "height")) == invalidtype)
        return VMerror;
    if (xpost_object_get_type(namedotcopydict = xpost_name_cons(ctx, ".copydict")) == invalidtype)
        return VMerror;

    xpost_memory_table_get_addr(ctx->gl,
                                XPOST_MEMORY_TABLE_SPECIAL_OPERATOR_TABLE,
                                &optadr);
    optab = (oper *)(ctx->gl->base + optadr);
    op = consoper(ctx, "loadwin32device", loadwin32device, 1, 0); INSTALL;
    op = consoper(ctx, "loadwin32devicecont", loadwin32devicecont, 1, 1, dicttype);
    _loadwin32devicecont_opcode = op.mark_.padw;

    return 0;
}
