/*
 * Xpost View - a small Level-2 Postscript viewer
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

#ifndef WIN32_LEAN_AND_MEAN
# define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN

#include "xpost.h"
#include "xpost_dsc.h"
#include "xpost_view.h"

typedef struct
{
    BITMAPINFOHEADER bih;
    DWORD masks[3];
} BITMAPINFO_XPOST;

struct _Xpost_View_Window
{
    HBITMAP bitmap;
    BITMAPINFO_XPOST *bitmap_info;
    HINSTANCE instance;
    HWND window;
    HDC dc;
    unsigned int *buf;
    int width;
    int height;
};

static LRESULT CALLBACK
_xpost_view_win32_procedure(HWND   window,
                            UINT   message,
                            WPARAM window_param,
                            LPARAM data_param)
{
    Xpost_View_Window *win;

    win = (Xpost_View_Window *)GetWindowLongPtr(window, GWLP_USERDATA);

    switch (message)
    {
        case WM_CREATE:
            printf("create\n");
            return 0;
        case WM_CLOSE:
            printf("close\n");
            xpost_view_win_del(win);
            return 0;
        case WM_KEYUP:
            if (window_param == VK_RIGHT)
                printf(" right !!\n");
            if (window_param == VK_LEFT)
                printf(" left !!\n");
            return 0;
        case WM_DESTROY:
            printf("destroy window message\n");
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProc(window, message, window_param, data_param);
    }
}

const char *xpost_view_device_get(void)
{
    return "gdi";
}

Xpost_View_Window *
xpost_view_win_new(int xorig, int yorig, int width, int height)
{
    WNDCLASSEX wc;
    RECT rect;
    Xpost_View_Window *win;
    HICON icon = NULL;
    HICON icon_sm = NULL;

    win = (Xpost_View_Window *)malloc(sizeof(Xpost_View_Window));
    if (!win)
        return NULL;

    win->instance = GetModuleHandle(NULL);
    if (!win->instance)
        goto free_win;

    icon = LoadImage(win->instance,
                     MAKEINTRESOURCE(101),
                     IMAGE_ICON,
                     GetSystemMetrics(SM_CXICON),
                     GetSystemMetrics(SM_CYICON),
                     LR_DEFAULTCOLOR);
    if (!icon)
        icon = LoadIcon(NULL, IDI_APPLICATION);

    icon_sm = LoadImage(win->instance,
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
    wc.lpfnWndProc = _xpost_view_win32_procedure;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = win->instance;
    wc.hIcon = icon;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(1 + COLOR_BTNFACE);
    wc.lpszMenuName =  NULL;
    wc.lpszClassName = "XPOST_VIEW";
    wc.hIconSm = icon_sm;

    if(!RegisterClassEx(&wc))
        goto free_library;

    rect.left = 0;
    rect.top = 0;
    rect.right = width;
    rect.bottom = height;
    if (!AdjustWindowRectEx(&rect, WS_OVERLAPPEDWINDOW | WS_SIZEBOX, FALSE, 0))
        goto unregister_class;

    win->window = CreateWindowEx(0, "XPOST_VIEW", "Xpost viewer",
                                 WS_OVERLAPPEDWINDOW | WS_SIZEBOX,
                                 xorig, yorig,
                                 rect.right - rect.left,
                                 rect.bottom - rect.top,
                                 NULL, NULL,
                                 win->instance, NULL);

    if (!win->window)
        goto unregister_class;

    win->dc =  GetDC(win->window);
    if (!win->dc)
        goto destroy_window;

    win->bitmap_info = (BITMAPINFO_XPOST *)malloc(sizeof(BITMAPINFO_XPOST));
    if (!win->bitmap_info)
    {
        fprintf(stderr, "allocating bitmap info data failed\n");
        goto release_dc;
    }

    win->bitmap_info->bih.biSize = sizeof(BITMAPINFOHEADER);
    win->bitmap_info->bih.biWidth = width;
    win->bitmap_info->bih.biHeight = -height;
    win->bitmap_info->bih.biPlanes = 1;
    win->bitmap_info->bih.biSizeImage = 4 * width * height;
    win->bitmap_info->bih.biXPelsPerMeter = 0;
    win->bitmap_info->bih.biYPelsPerMeter = 0;
    win->bitmap_info->bih.biClrUsed = 0;
    win->bitmap_info->bih.biClrImportant = 0;
    win->bitmap_info->bih.biBitCount = 32;
    win->bitmap_info->bih.biCompression = BI_BITFIELDS;
    win->bitmap_info->masks[0] = 0x00ff0000;
    win->bitmap_info->masks[1] = 0x0000ff00;
    win->bitmap_info->masks[2] = 0x000000ff;

    win->bitmap = CreateDIBSection(win->dc,
                                   (const BITMAPINFO *)win->bitmap_info,
                                   DIB_RGB_COLORS,
                                   (void **)(&win->buf),
                                   NULL,
                                   0);
    if (!win->bitmap)
    {
        fprintf(stderr, "CreateDIBSection() failed\n");
        goto free_bitmap_info;
    }

    ShowWindow(win->window, SW_SHOWNORMAL);
    if (!UpdateWindow(win->window))
        goto delete_bitmap;

    win->width = width;
    win->height = height;

    SetLastError(0);
    if (!SetWindowLongPtr(win->window, GWLP_USERDATA, (LONG_PTR)win) &&
        (GetLastError() != 0))
    {
        fprintf(stderr, "SetWindowLongPtr() failed\n");
        goto delete_bitmap;
    }

    return win;

  delete_bitmap:
    DeleteObject(win->bitmap);
  free_bitmap_info:
    free(win->bitmap_info);
  release_dc:
    ReleaseDC(win->window, win->dc);
  destroy_window:
    DestroyWindow(win->window);
  unregister_class:
    UnregisterClass(TEXT("XPOST_VIEW"), win->instance);
  free_library:
    FreeLibrary(win->instance);
  free_win:
    free(win);

    return NULL;
}

void
xpost_view_win_del(Xpost_View_Window *win)
{
    if (!win)
        return;

    DeleteObject(win->bitmap);
    free(win->bitmap_info);
    ReleaseDC(win->window, win->dc);
    DestroyWindow(win->window);
    UnregisterClass(TEXT("XPOST_VIEW"), win->instance);
    FreeLibrary(win->instance);
}

void
xpost_view_main_loop(Xpost_View_Window *win)
{
    while(1)
    {
        MSG msg;
        BOOL ret;

        ret = PeekMessage(&msg, NULL, 0, 0, PM_REMOVE);
        if (ret)
        {
            do
            {
                if (msg.message == WM_QUIT)
                    return;
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            } while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE));
        }
    }
}
