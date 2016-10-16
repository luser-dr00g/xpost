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

#ifdef HAVE_XCB
#include <xcb/xcb.h>
#include <xcb/xcb_image.h>
#include <xcb/xcb_event.h>
#endif

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
# define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#endif

#include "xpost.h"
#include "xpost_dsc.h"

typedef struct _Xpost_View_Window Xpost_View_Window;

#ifdef HAVE_XCB

struct _Xpost_View_Window
{
    xcb_connection_t *c;
    xcb_screen_t *scr;
    xcb_drawable_t window;
    int width, height;
    xcb_pixmap_t img;
    xcb_gcontext_t gc;
    xcb_colormap_t cmap;
};

static Xpost_View_Window *
_xpost_view_win_new(int xorig, int yorig, int width, int height)
{
    xcb_screen_iterator_t iter;
    xcb_get_geometry_reply_t *geom;
    Xpost_View_Window *win;
    int scrno;
    unsigned int values[3];
    unsigned int mask;
    unsigned char depth;

    win = (Xpost_View_Window *)malloc(sizeof(Xpost_View_Window));
    if (!win)
        return NULL;

    win->c = xcb_connect(NULL, &scrno);
    if (xcb_connection_has_error(win->c))
    {
        fprintf(stderr, "Fail to connect to the X server\n");
        goto free_win;
    }

    iter = xcb_setup_roots_iterator(xcb_get_setup(win->c));
    for (; iter.rem; --scrno, xcb_screen_next(&iter))
    {
        if (scrno == 0)
        {
            win->scr = iter.data;
            break;
        }
    }

    geom = xcb_get_geometry_reply(win->c,
                                  xcb_get_geometry(win->c, win->scr->root), 0);
    if (!geom)
    {
        fprintf(stderr, "Fail to the geometry of the root window\n");
        goto disconnect_c;
    }

    depth = geom->depth;
    free(geom);

    win->window = xcb_generate_id(win->c);
    mask = XCB_CW_BACK_PIXMAP | XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    values[0] = XCB_NONE;
    values[1] = win->scr->white_pixel;
    values[2] = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS;
    xcb_create_window(win->c, XCB_COPY_FROM_PARENT,
                      win->window, win->scr->root,
                      0, 0,
                      width, height,
                      5,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      win->scr->root_visual,
                      mask,
                      values);

    /* set title */
    xcb_change_property(win->c,
                        XCB_PROP_MODE_REPLACE,
                        win->window,
                        XCB_ATOM_WM_NAME,
                        XCB_ATOM_STRING,
                        8,
                        sizeof("Xpost") - 1,
                        "Xpost");
    xcb_map_window(win->c, win->window);
    xcb_flush(win->c);

    win->img = xcb_generate_id(win->c);
    xcb_create_pixmap(win->c,
                      depth, win->img,
                      win->window, win->width, win->height);

    win->gc = xcb_generate_id(win->c);
    values[0] = win->scr->black_pixel;
    values[1] = win->scr->white_pixel;
    xcb_create_gc(win->c, win->gc, win->window,
                  XCB_GC_FOREGROUND | XCB_GC_BACKGROUND,
                  values);

    win->cmap = xcb_generate_id(win->c);
    xcb_create_colormap(win->c, XCB_COLORMAP_ALLOC_NONE, win->cmap,
                        win->window, win->scr->root_visual);

    win->width = width;
    win->height = height;

    return win;

  disconnect_c:
    xcb_disconnect(win->c);
  free_win:
    free(win);

    return NULL;
}

static void
_xpost_view_win_del(Xpost_View_Window *win)
{
    if (!win)
        return;

    xcb_disconnect(win->c);
    free(win);
}

static void
_xpost_view_main_loop(Xpost_View_Window *win)
{
    xcb_intern_atom_cookie_t cookie1;
    xcb_intern_atom_cookie_t cookie2;
    xcb_intern_atom_reply_t* reply1;
    xcb_intern_atom_reply_t* reply2;
    xcb_gcontext_t gc = { 0 };
    int finished;

    /*
     * Listen to X client messages in order to be able to pickup
     * the "delete window" message that is generated for example
     * when someone clicks the top-right X button within the window
     * manager decoration (or when user hits ALT-F4). */
    cookie1 = xcb_intern_atom(win->c, 1,
                              sizeof("WM_DELETE_WINDOW") - 1, "WM_DELETE_WINDOW");
    cookie2 = xcb_intern_atom(win->c, 1,
                              sizeof("WM_PROTOCOLS") - 1, "WM_PROTOCOLS");
    reply1 = xcb_intern_atom_reply(win->c, cookie1, 0);
    reply2 = xcb_intern_atom_reply(win->c, cookie2, 0);
    xcb_change_property(win->c, XCB_PROP_MODE_REPLACE, win->window, reply2->atom, 4, 32, 1,
                        &reply1->atom);

    finished = 0;
    while (!finished)
    {
        xcb_generic_event_t *e;

        if ((e = xcb_poll_for_event(win->c)))
        {
            switch (XCB_EVENT_RESPONSE_TYPE(e))
            {
                case XCB_EXPOSE:
                    /* xcb_copy_area(win->c, win->pixmap, win->window, gc, */
                    /*               0, 0, 0, 0, win->width, win->height); */
                    xcb_flush(win->c);
                    break;
                case XCB_CLIENT_MESSAGE:
                {
                    xcb_client_message_event_t *event;

                    printf("client message\n");
                    event = (xcb_client_message_event_t *)e;
                    if (event->data.data32[0] == reply1->atom)
                        finished = 1;
                    break;
                }
                case XCB_BUTTON_PRESS:
                    printf("button pressed\n");
                    finished = 1;
                    break;
            }
            free (e);
        }

        xcb_flush (win->c);
    }

    free(reply2);
    free(reply1);
}

#elif defined _WIN32

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

static void _xpost_view_win_del(Xpost_View_Window *win);

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
            _xpost_view_win_del(win);
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

static Xpost_View_Window *
_xpost_view_win_new(int xorig, int yorig, int width, int height)
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

    win->window = CreateWindowEx(0, "XPOST_VIEW", "test",
                                 WS_OVERLAPPEDWINDOW | WS_SIZEBOX,
                                 xorig, yorig,
                                 rect.right - rect.left,
                                 rect.bottom - rect.top,
                                 NULL, NULL,
                                 win->instance, NULL);

    if (!win->window)
        goto unregister_class;

    SetWindowText(win->window, "Xpost");

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

static void
_xpost_view_win_del(Xpost_View_Window *win)
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

static void
_xpost_view_main_loop(Xpost_View_Window *win)
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

#endif

static void
_xpost_view_license(void)
{
    printf("BSD 3-clause\n");
}

static void
_xpost_view_version(const char *progname)
{
    int maj;
    int min;
    int mic;

    xpost_version_get(&maj, &min, &mic);
    printf("%s %d.%d.%d\n", progname, maj, min, mic);
}

static void
_xpost_view_usage(const char *progname)
{
    printf("Usage: %s [options] file.ps\n\n", progname);
    printf("Postscript level 2 interpreter\n\n");
    printf("Options:\n");
    printf("  -q, --quiet            suppress interpreter messages (default)\n");
    printf("  -v, --verbose          do not go quiet into that good night\n");
    printf("  -t, --trace            add additional tracing messages, implies -v\n");
    printf("  -L, --license          show program license\n");
    printf("  -V, --version          show program version\n");
    printf("  -h, --help             show this message\n");
    printf("\n");
}

static int
_xpost_view_options_read(int argc, char *argv[], int *msg, const char **file)
{
    const char *psfile;
    int output_msg;
    int i;

    psfile = NULL;
    output_msg = XPOST_OUTPUT_MESSAGE_QUIET;

    i = 0;
    while (++i < argc)
    {
        if (*argv[i] == '-')
        {
            if ((!strcmp(argv[i], "-h")) ||
                (!strcmp(argv[i], "--help")))
            {
                _xpost_view_usage(argv[0]);
                return 0;
            }
            else if ((!strcmp(argv[i], "-V")) ||
                     (!strcmp(argv[i], "--version")))
            {
                _xpost_view_version(argv[0]);
                return 0;
            }
            else if ((!strcmp(argv[i], "-L")) ||
                     (!strcmp(argv[i], "--license")))
            {
                _xpost_view_license();
                return 0;
            }
            else if ((!strcmp(argv[i], "-q")) ||
                     (!strcmp(argv[i], "--quiet")))
            {
                output_msg = XPOST_OUTPUT_MESSAGE_QUIET;
            }
            else if ((!strcmp(argv[i], "-v")) ||
                     (!strcmp(argv[i], "--verbose")))
            {
                output_msg = XPOST_OUTPUT_MESSAGE_VERBOSE;
            }
            else if ((!strcmp(argv[i], "-t")) ||
                     (!strcmp(argv[i], "--trace")))
            {
                output_msg = XPOST_OUTPUT_MESSAGE_TRACING;
            }
            else
            {
                printf("unknown option\n");
                _xpost_view_usage(argv[0]);
                return -1;
            }
        }
        else
            psfile = argv[i];
    }

    if (!psfile)
    {
        printf("Postscript file not provided\n");
        _xpost_view_usage(argv[0]);
        return -1;
    }

    *msg = output_msg;
    *file = psfile;

    return 1;
}

int main(int argc, char *argv[])
{
    Xpost_View_Window *win;
    Xpost_Context *ctx;
    const char *device;
    const char *psfile;
    int output_msg;
    int ret;

#ifdef HAVE_XCB
    device = "xcb";
#elif defined _WIN32
    device = "gdi";
#else
# error "System not supported"
#endif

    psfile = NULL;
    output_msg = XPOST_OUTPUT_MESSAGE_QUIET;

    ret = _xpost_view_options_read(argc, argv, &output_msg, &psfile);
    if (ret == -1) return EXIT_FAILURE;
    else if (ret == 0) return EXIT_SUCCESS;

    /* if (!xpost_init()) */
    /* { */
    /*     fprintf(stderr, "Xpost failed to initialize\n"); */
    /*     return EXIT_FAILURE; */
    /* } */

    /* ctx = xpost_create(device); */
    /* if (!xpost_init()) */
    /* { */
    /*     fprintf(stderr, "Xpost failed to create interpreter\n"); */
    /*     goto quit_xpost; */
    /* } */


    win = _xpost_view_win_new(0, 0, 480, 640);
    if (!win)
        return 0;

    _xpost_view_main_loop(win);

    /* xpost_quit(); */

    return EXIT_SUCCESS;

  quit_xpost:
    printf("err\n");
    return EXIT_FAILURE;
}
