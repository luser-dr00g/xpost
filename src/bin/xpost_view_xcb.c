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
# include <xcb/xcb.h>
# include <xcb/xcb_image.h>
# include <xcb/xcb_event.h>
#endif

#include "xpost.h"
#include "xpost_dsc.h"
#include "xpost_view.h"

struct _Xpost_View_Window
{
    xcb_connection_t *c;
    xcb_screen_t *scr;
    xcb_image_t *image;
    xcb_drawable_t window;
    xcb_drawable_t pixmap;
    int width, height, depth;
    xcb_gcontext_t gc;
};


Xpost_View_Window *
xpost_view_win_new(int xorig, int yorig, int width, int height)
{
    xcb_screen_iterator_t iter;
    xcb_rectangle_t rect;
    xcb_get_geometry_reply_t *geom;
    Xpost_View_Window *win;
    int scrno;
    unsigned int values[3];
    unsigned int mask;

    win = (Xpost_View_Window *)calloc(1, sizeof(Xpost_View_Window));
    if (!win)
        return NULL;

    win->width = width;
    win->height = height;

    /* open a connection */
    win->c = xcb_connect(NULL, &scrno);
    if (xcb_connection_has_error(win->c))
    {
        fprintf(stderr, "Fail to connect to the X server\n");
        goto free_win;
    }

    /* get the screen */
    iter = xcb_setup_roots_iterator(xcb_get_setup(win->c));
    for (; iter.rem; --scrno, xcb_screen_next(&iter))
    {
        if (scrno == 0)
        {
            win->scr = iter.data;
            break;
        }
    }

    /* get the depth of the screen */
    geom = xcb_get_geometry_reply(win->c,
                                  xcb_get_geometry(win->c, win->scr->root), 0);
    if (!geom)
    {
        fprintf(stderr, "Fail to the geometry of the root window\n");
        goto disconnect_c;
    }

    win->depth = geom->depth;
    free(geom);

    /* create the window */
    win->window = xcb_generate_id(win->c);
    mask = XCB_CW_BACK_PIXMAP | XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    values[0] = XCB_NONE;
    values[1] = win->scr->white_pixel;
    values[2] = XCB_EVENT_MASK_EXPOSURE |
                XCB_EVENT_MASK_BUTTON_PRESS |
                XCB_EVENT_MASK_KEY_RELEASE;
    xcb_create_window(win->c, XCB_COPY_FROM_PARENT,
                      win->window, win->scr->root,
                      xorig, yorig,
                      width, height,
                      5,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      win->scr->root_visual,
                      mask,
                      values);

    /* set title of the window */
    xcb_change_property(win->c,
                        XCB_PROP_MODE_REPLACE,
                        win->window,
                        XCB_ATOM_WM_NAME,
                        XCB_ATOM_STRING,
                        8,
                        sizeof("Xpost viewer") - 1,
                        "Xpost viewer");

    /* set background context */
    win->gc = xcb_generate_id(win->c);
    values[0] = win->scr->white_pixel;
    values[1] = 0;
    xcb_create_gc(win->c, win->gc, win->window,
                  XCB_GC_BACKGROUND | XCB_GC_GRAPHICS_EXPOSURES,
                  values);

    /* set background pixmap */
    rect.x = 0;
    rect.y = 0;
    rect.width = width;
    rect.height = height;
    win->pixmap = xcb_generate_id(win->c);
    xcb_create_pixmap(win->c,
                      win->depth, win->pixmap,
                      win->window, width, height);
    xcb_poly_fill_rectangle(win->c, win->pixmap, win->gc, 1, &rect);

    xcb_map_window(win->c, win->window);
    xcb_flush(win->c);

    return win;

  disconnect_c:
    xcb_disconnect(win->c);
  free_win:
    free(win);

    return NULL;
}

void
xpost_view_win_del(Xpost_View_Window *win)
{
    if (!win)
        return;

    xcb_disconnect(win->c);
    free(win);
}

void
xpost_view_page_display(Xpost_View_Window *win,
                        const void *buffer)
{
    xcb_image_t *image;

    image = xcb_image_create_native(win->c, win->width, win->height,
                                    XCB_IMAGE_FORMAT_Z_PIXMAP,
                                    win->depth, (void *)buffer,
                                    4 * win->width * win->height, NULL);
    xcb_image_put(win->c, win->window, win->gc, image, 0, 0, 0);
    win->image = image;
}

void
xpost_view_main_loop(const Xpost_View_Window *win)
{
    xcb_intern_atom_cookie_t cookie1;
    xcb_intern_atom_cookie_t cookie2;
    xcb_intern_atom_reply_t* reply1;
    xcb_intern_atom_reply_t* reply2;
    int finished;

    /*
     * Listen to X client messages in order to be able to pickup
     * the "delete window" message that is generated for example
     * when someone clicks the top-right X button within the window
     * manager decoration (or when user hits ALT-F4).
     */
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
                    xcb_image_put(win->c, win->window, win->gc, win->image, 0, 0, 0);
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
                case XCB_KEY_RELEASE:
                {
                    xcb_key_release_event_t *event;

                    event = (xcb_key_release_event_t *)e;
                    if (event->detail == 113)
                        xpost_view_page_change(-1);
                    if (event->detail == 114)
                        xpost_view_page_change(1);
                    break;
                }
            }
            free (e);
        }

        xcb_flush (win->c);
    }

    free(reply2);
    free(reply1);
}
