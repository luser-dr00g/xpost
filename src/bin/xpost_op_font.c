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
#include <stdio.h>
#include <stdlib.h> /* NULL strtod */
#include <string.h>

#include "xpost_log.h"
#include "xpost_memory.h"
#include "xpost_object.h"
#include "xpost_stack.h"
#include "xpost_font.h"

#include "xpost_save.h"
#include "xpost_context.h"
#include "xpost_interpreter.h"
#include "xpost_error.h"
#include "xpost_name.h"
#include "xpost_string.h"
#include "xpost_array.h"
#include "xpost_dict.h"
#include "xpost_operator.h"
#include "xpost_op_font.h"

typedef struct fontdata
{
    void *face;
} fontdata;

static
int _findfont (Xpost_Context *ctx,
               Xpost_Object fontname)
{
#ifdef HAVE_FREETYPE
    Xpost_Object fontstr;
    Xpost_Object fontdict;
    Xpost_Object privatestr;
    struct fontdata data;
    char *fname;

    if (xpost_object_get_type(fontname) == nametype)
        fontstr = strname(ctx, fontname);
    else
        fontstr = fontname;
    fname = alloca(fontstr.comp_.sz + 1);
    memcpy(fname, charstr(ctx, fontstr), fontstr.comp_.sz);
    fname[fontstr.comp_.sz] = '\0';

    fontdict = consbdc(ctx, 10);
    privatestr = consbst(ctx, sizeof data, NULL);
    bdcput(ctx, fontdict, consname(ctx, "Private"), privatestr);

    /* initialize font data, with x-scale and y-scale set to 1 */
    data.face = xpost_font_face_new_from_name(fname);
    if (data.face == NULL)
        return invalidfont;

    xpost_memory_put(xpost_context_select_memory(ctx, privatestr),
            xpost_object_get_ent(privatestr), 0, sizeof data, &data);
    xpost_stack_push(ctx->lo, ctx->os, fontdict);
    return 0;
#else
    (void)ctx;
    (void)fontname;
    return invalidfont;
#endif
}


static
int _scalefont (Xpost_Context *ctx,
                Xpost_Object fontdict,
                Xpost_Object size)
{
    Xpost_Object privatestr;
    struct fontdata data;

    privatestr = bdcget(ctx, fontdict, consname(ctx, "Private"));
    if (xpost_object_get_type(privatestr) == invalidtype)
        return undefined;
    xpost_memory_get(xpost_context_select_memory(ctx, privatestr),
            xpost_object_get_ent(privatestr), 0, sizeof data, &data);

    if (data.face == NULL)
        return invalidfont;

    /* scale x and y sizes by @p size */
    xpost_font_face_scale(data.face, size.real_.val);

    xpost_memory_put(xpost_context_select_memory(ctx, privatestr),
            xpost_object_get_ent(privatestr), 0, sizeof data, &data);
    xpost_stack_push(ctx->lo, ctx->os, fontdict);
    return 0;
}


static
int _setfont (Xpost_Context *ctx,
              Xpost_Object fontdict)
{
    Xpost_Object userdict;
    Xpost_Object gd;
    Xpost_Object gs;

    userdict = xpost_stack_bottomup_fetch(ctx->lo, ctx->ds, 2);
    if (xpost_object_get_type(userdict) != dicttype)
        return dictstackunderflow;
    gd = bdcget(ctx, userdict, consname(ctx, "graphicsdict"));
    gs = bdcget(ctx, gd, consname(ctx, "currgstate"));

    bdcput(ctx, gs, consname(ctx, "currfont"), fontdict);

    return 0;
}

#ifdef HAVE_FREETYPE
static
void _draw_bitmap (Xpost_Context *ctx,
              Xpost_Object devdic,
              Xpost_Object putpix,
              const unsigned char *buffer,
              int rows,
              int width,
              int pitch,
              char pixel_mode,
              int xpos,
              int ypos,
              int ncomp,
              Xpost_Object comp1,
              Xpost_Object comp2,
              Xpost_Object comp3)
{
    int i, j;
    const unsigned char *tmp;
    unsigned int pix;

    tmp = buffer;
    XPOST_LOG_INFO("bitmap rows = %d, bitmap width = %d", rows, width);
    XPOST_LOG_INFO("bitmap pitch = %d", pitch);
    XPOST_LOG_INFO("bitmap pixel_mode = %d", pixel_mode);

    for (i = 0; i < rows; i++)
    {
        printf("\n");
        for (j = 0; j < width; j++)
        {
            //pix = tmp[j];
            switch (pixel_mode)
            {
                case XPOST_FONT_PIXEL_MODE_MONO:
                    pix = (tmp[j/8] >> (7 - (j % 8))) & 1;
                    break;
                case XPOST_FONT_PIXEL_MODE_GRAY:
                    pix = tmp[j];
                    break;
                default:
                    XPOST_LOG_ERR("unsupported pixel_mode");
                    return;
            }
            printf("%c", pix? 'X':'_');
            if (pix)
            {
                switch (ncomp)
                {
                    case 1:
                        xpost_stack_push(ctx->lo, ctx->os, comp1);
                        break;
                    case 3:
                        xpost_stack_push(ctx->lo, ctx->os, comp1);
                        xpost_stack_push(ctx->lo, ctx->os, comp2);
                        xpost_stack_push(ctx->lo, ctx->os, comp3);
                        break;
                }
                xpost_stack_push(ctx->lo, ctx->os, xpost_cons_int(xpos+j));
                xpost_stack_push(ctx->lo, ctx->os, xpost_cons_int(ypos+i));
                xpost_stack_push(ctx->lo, ctx->os, devdic);
                if (xpost_object_get_type(putpix) == operatortype)
                    xpost_stack_push(ctx->lo, ctx->es, putpix);
                else
                {
                    xpost_stack_push(ctx->lo, ctx->os, putpix);
                    xpost_stack_push(ctx->lo, ctx->es,
                            consname(ctx, "exec"));
                }
            }
        }
        tmp += pitch;
    }
}
#endif

static
int _show_char(Xpost_Context *ctx,
        Xpost_Object devdic,
        Xpost_Object putpix,
        struct fontdata data,
        real *xpos,
        real *ypos,
        unsigned int ch,
        unsigned int *glyph_previous,
        int has_kerning,
        int ncomp,
        Xpost_Object comp1,
        Xpost_Object comp2,
        Xpost_Object comp3)
{
#ifdef HAVE_FREETYPE
    unsigned int glyph_index;
    unsigned char *buffer;
    int rows;
    int width;
    int pitch;
    char pixel_mode;
    int left;
    int top;
    long advance_x;
    long advance_y;

    glyph_index = xpost_font_face_glyph_index_get(data.face, ch);
    if (has_kerning && *glyph_previous && (glyph_index > 0))
    {
        long delta_x;
        long delta_y;

        if (xpost_font_face_kerning_delta_get(data.face, *glyph_previous, glyph_index,
                                              &delta_x, &delta_y))
        {
            *xpos += delta_x >> 6;
            *ypos += delta_y >> 6;
        }
    }
    if (!xpost_font_face_glyph_render(data.face, glyph_index))
        return 0;
    xpost_font_face_glyph_buffer_get(data.face, &buffer, &rows, &width, &pitch, &pixel_mode, &left, &top, &advance_x, &advance_y);
    _draw_bitmap(ctx, devdic, putpix,
            buffer, rows, width, pitch, pixel_mode,
            *xpos + left, *ypos - top,
            ncomp, comp1, comp2, comp3);
    *xpos += advance_x >> 6;
    *ypos += advance_y >> 6;
    *glyph_previous = glyph_index;
#else
    (void)ctx;
    (void)devdic;
    (void)putpix;
    (void)data;
    (void)xpos;
    (void)ypos;
    (void)ch;
    (void)glyph_previous;
    (void)has_kerning;
    (void)ncomp;
    (void)comp1;
    (void)comp2;
    (void)comp3;
#endif
    return 1;
}

static
int _get_current_point (Xpost_Context *ctx,
                        Xpost_Object gs,
                        real *xpos,
                        real *ypos)
{
    Xpost_Object path;
    Xpost_Object subpath;
    Xpost_Object pathelem;
    Xpost_Object pathelemdata;
    Xpost_Object datax, datay;

    /* get the current pen position */
    /*FIXME if any of these calls fail, should return nocurrentpoint; */
    path = bdcget(ctx, gs, consname(ctx, "currpath"));
    subpath = bdcget(ctx, path, xpost_cons_int(
                diclength(xpost_context_select_memory(ctx,path), path) - 1));
    if (xpost_object_get_type(subpath) == invalidtype)
        return nocurrentpoint;
    pathelem = bdcget(ctx, subpath, xpost_cons_int(
                diclength(xpost_context_select_memory(ctx,subpath), subpath) - 1));
    if (xpost_object_get_type(pathelem) == invalidtype)
        return nocurrentpoint;
    pathelemdata = bdcget(ctx, pathelem, consname(ctx, "data"));
    if (xpost_object_get_type(pathelemdata) == invalidtype)
        return nocurrentpoint;

    datax = barget(ctx, pathelemdata, pathelemdata.comp_.sz - 2);
    datay = barget(ctx, pathelemdata, pathelemdata.comp_.sz - 1);
    if (xpost_object_get_type(datax) == integertype)
        datax = xpost_cons_real(datax.int_.val);
    if (xpost_object_get_type(datay) == integertype)
        datay = xpost_cons_real(datay.int_.val);
    *xpos = datax.real_.val;
    *ypos = datay.real_.val;
    XPOST_LOG_INFO("currentpoint: %f %f", *xpos, *ypos);

    return 0;
}

static
int _show (Xpost_Context *ctx,
           Xpost_Object str)
{
    Xpost_Object userdict;
    Xpost_Object gd;
    Xpost_Object gs;
    Xpost_Object fontdict;
    Xpost_Object privatestr;
    struct fontdata data;
    char *cstr;
    real xpos, ypos;
    char *ch;
    Xpost_Object devdic;
    Xpost_Object putpix;
    Xpost_Object colorspace;
    int ncomp;
    Xpost_Object comp1, comp2, comp3;
    Xpost_Object finalize;
    int ret;

    int has_kerning;
    unsigned int glyph_previous;

    /* load the graphicsdict, current graphics state, and current font */
    userdict = xpost_stack_bottomup_fetch(ctx->lo, ctx->ds, 2);
    if (xpost_object_get_type(userdict) != dicttype)
        return dictstackunderflow;
    gd = bdcget(ctx, userdict, consname(ctx, "graphicsdict"));
    gs = bdcget(ctx, gd, consname(ctx, "currgstate"));
    fontdict = bdcget(ctx, gs, consname(ctx, "currfont"));
    if (xpost_object_get_type(fontdict) == invalidtype)
        return invalidfont;
    XPOST_LOG_INFO("loaded graphicsdict, graphics state, and current font");

    /* load the device and PutPix member function */
    devdic = bdcget(ctx, gs, consname(ctx, "device"));
    putpix = bdcget(ctx, devdic, consname(ctx, "PutPix"));
    XPOST_LOG_INFO("loaded DEVICE and PutPix");

    /* get the font data from the font dict */
    privatestr = bdcget(ctx, fontdict, consname(ctx, "Private"));
    if (xpost_object_get_type(privatestr) == invalidtype)
        return invalidfont;
    xpost_memory_get(xpost_context_select_memory(ctx, privatestr),
            xpost_object_get_ent(privatestr), 0, sizeof data, &data);
    if (data.face == NULL)
    {
        XPOST_LOG_ERR("face is NULL");
        return invalidfont;
    }
    XPOST_LOG_INFO("loaded font data from dict");

    /* get a c-style nul-terminated string */
    cstr = alloca(str.comp_.sz + 1);
    memcpy(cstr, charstr(ctx, str), str.comp_.sz);
    cstr[str.comp_.sz] = '\0';
    XPOST_LOG_INFO("append nul to string");

    ret = _get_current_point(ctx, gs, &xpos, &ypos);
    if (ret)
        return ret;

    colorspace = bdcget(ctx, devdic, consname(ctx, "nativecolorspace"));
    if (objcmp(ctx, colorspace, consname(ctx, "DeviceGray")) == 0)
    {
        ncomp = 1;
        comp1 = bdcget(ctx, gs, consname(ctx, "colorcomp1"));
    }
    else if (objcmp(ctx, colorspace, consname(ctx, "DeviceRGB")) == 0)
    {
        ncomp = 3;
        comp1 = bdcget(ctx, gs, consname(ctx, "colorcomp1"));
        comp2 = bdcget(ctx, gs, consname(ctx, "colorcomp2"));
        comp3 = bdcget(ctx, gs, consname(ctx, "colorcomp3"));
    } else {
        XPOST_LOG_ERR("unimplemented device colorspace");
        return unregistered;
    }
    XPOST_LOG_INFO("ncomp = %d", ncomp);

    finalize = xpost_object_cvx(consbar(ctx, 5));
    /* fill-in final pos before return */
    barput(ctx, finalize, 0, xpost_cons_real(xpos));
    barput(ctx, finalize, 1, xpost_cons_real(ypos));
    barput(ctx, finalize, 2, xpost_object_cvx(consname(ctx, "itransform")));
    barput(ctx, finalize, 3, xpost_object_cvx(consname(ctx, "moveto")));
    barput(ctx, finalize, 4, xpost_object_cvx(consname(ctx, "flushpage")));
    xpost_stack_push(ctx->lo, ctx->es, finalize);

    /* render text in char *cstr  with font data  at pen position xpos ypos */
    has_kerning = xpost_font_face_kerning_has(data.face);
    glyph_previous = 0;
    for (ch = cstr; *ch; ch++) {
        _show_char(ctx, devdic, putpix, data, &xpos, &ypos, *ch, &glyph_previous, has_kerning,
                ncomp, comp1, comp2, comp3);
    }

    /* update current position in the graphics state */
    barput(ctx, finalize, 0, xpost_cons_real(xpos));
    barput(ctx, finalize, 1, xpost_cons_real(ypos));

    return 0;
}

static
int _ashow (Xpost_Context *ctx,
            Xpost_Object dx,
            Xpost_Object dy,
            Xpost_Object str)
{
    Xpost_Object userdict;
    Xpost_Object gd;
    Xpost_Object gs;
    Xpost_Object fontdict;
    Xpost_Object privatestr;
    struct fontdata data;
    char *cstr;
    real xpos, ypos;
    char *ch;
    Xpost_Object devdic;
    Xpost_Object putpix;
    Xpost_Object colorspace;
    int ncomp;
    Xpost_Object comp1, comp2, comp3;
    Xpost_Object finalize;
    int ret;

    int has_kerning;
    unsigned int glyph_previous;

    /* load the graphicsdict, current graphics state, and current font */
    userdict = xpost_stack_bottomup_fetch(ctx->lo, ctx->ds, 2);
    if (xpost_object_get_type(userdict) != dicttype)
        return dictstackunderflow;
    gd = bdcget(ctx, userdict, consname(ctx, "graphicsdict"));
    gs = bdcget(ctx, gd, consname(ctx, "currgstate"));
    fontdict = bdcget(ctx, gs, consname(ctx, "currfont"));
    if (xpost_object_get_type(fontdict) == invalidtype)
        return invalidfont;
    XPOST_LOG_INFO("loaded graphicsdict, graphics state, and current font");

    /* load the device and PutPix member function */
    devdic = bdcget(ctx, gs, consname(ctx, "device"));
    putpix = bdcget(ctx, devdic, consname(ctx, "PutPix"));
    XPOST_LOG_INFO("loaded DEVICE and PutPix");

    /* get the font data from the font dict */
    privatestr = bdcget(ctx, fontdict, consname(ctx, "Private"));
    if (xpost_object_get_type(privatestr) == invalidtype)
        return invalidfont;
    xpost_memory_get(xpost_context_select_memory(ctx, privatestr),
            xpost_object_get_ent(privatestr), 0, sizeof data, &data);
    if (data.face == NULL)
    {
        XPOST_LOG_ERR("face is NULL");
        return invalidfont;
    }
    XPOST_LOG_INFO("loaded font data from dict");

    /* get a c-style nul-terminated string */
    cstr = alloca(str.comp_.sz + 1);
    memcpy(cstr, charstr(ctx, str), str.comp_.sz);
    cstr[str.comp_.sz] = '\0';
    XPOST_LOG_INFO("append nul to string");

    ret = _get_current_point(ctx, gs, &xpos, &ypos);
    if (ret)
        return ret;

    colorspace = bdcget(ctx, devdic, consname(ctx, "nativecolorspace"));
    if (objcmp(ctx, colorspace, consname(ctx, "DeviceGray")) == 0)
    {
        ncomp = 1;
        comp1 = bdcget(ctx, gs, consname(ctx, "colorcomp1"));
    }
    else if (objcmp(ctx, colorspace, consname(ctx, "DeviceRGB")) == 0)
    {
        ncomp = 3;
        comp1 = bdcget(ctx, gs, consname(ctx, "colorcomp1"));
        comp2 = bdcget(ctx, gs, consname(ctx, "colorcomp2"));
        comp3 = bdcget(ctx, gs, consname(ctx, "colorcomp3"));
    } else {
        XPOST_LOG_ERR("unimplemented device colorspace");
        return unregistered;
    }
    XPOST_LOG_INFO("ncomp = %d", ncomp);

    finalize = xpost_object_cvx(consbar(ctx, 5));
    /* fill-in final pos before return */
    barput(ctx, finalize, 0, xpost_cons_real(xpos));
    barput(ctx, finalize, 1, xpost_cons_real(ypos));
    barput(ctx, finalize, 2, xpost_object_cvx(consname(ctx, "itransform")));
    barput(ctx, finalize, 3, xpost_object_cvx(consname(ctx, "moveto")));
    barput(ctx, finalize, 4, xpost_object_cvx(consname(ctx, "flushpage")));
    xpost_stack_push(ctx->lo, ctx->es, finalize);

    /* render text in char *cstr  with font data  at pen position xpos ypos */
    has_kerning = xpost_font_face_kerning_has(data.face);
    glyph_previous = 0;
    for (ch = cstr; *ch; ch++) {
        _show_char(ctx, devdic, putpix, data, &xpos, &ypos, *ch, &glyph_previous, has_kerning,
                ncomp, comp1, comp2, comp3);
        xpos += dx.real_.val;
        ypos += dy.real_.val;
    }

    /* update current position in the graphics state */
    barput(ctx, finalize, 0, xpost_cons_real(xpos));
    barput(ctx, finalize, 1, xpost_cons_real(ypos));

    return 0;
}

static
int _widthshow (Xpost_Context *ctx,
                Xpost_Object cx,
                Xpost_Object cy,
                Xpost_Object charcode,
                Xpost_Object str)
{
    Xpost_Object userdict;
    Xpost_Object gd;
    Xpost_Object gs;
    Xpost_Object fontdict;
    Xpost_Object privatestr;
    struct fontdata data;
    char *cstr;
    real xpos, ypos;
    char *ch;
    Xpost_Object devdic;
    Xpost_Object putpix;
    Xpost_Object colorspace;
    int ncomp;
    Xpost_Object comp1, comp2, comp3;
    Xpost_Object finalize;
    int ret;

    int has_kerning;
    unsigned int glyph_previous;

    /* load the graphicsdict, current graphics state, and current font */
    userdict = xpost_stack_bottomup_fetch(ctx->lo, ctx->ds, 2);
    if (xpost_object_get_type(userdict) != dicttype)
        return dictstackunderflow;
    gd = bdcget(ctx, userdict, consname(ctx, "graphicsdict"));
    gs = bdcget(ctx, gd, consname(ctx, "currgstate"));
    fontdict = bdcget(ctx, gs, consname(ctx, "currfont"));
    if (xpost_object_get_type(fontdict) == invalidtype)
        return invalidfont;
    XPOST_LOG_INFO("loaded graphicsdict, graphics state, and current font");

    /* load the device and PutPix member function */
    devdic = bdcget(ctx, gs, consname(ctx, "device"));
    putpix = bdcget(ctx, devdic, consname(ctx, "PutPix"));
    XPOST_LOG_INFO("loaded DEVICE and PutPix");

    /* get the font data from the font dict */
    privatestr = bdcget(ctx, fontdict, consname(ctx, "Private"));
    if (xpost_object_get_type(privatestr) == invalidtype)
        return invalidfont;
    xpost_memory_get(xpost_context_select_memory(ctx, privatestr),
            xpost_object_get_ent(privatestr), 0, sizeof data, &data);
    if (data.face == NULL)
    {
        XPOST_LOG_ERR("face is NULL");
        return invalidfont;
    }
    XPOST_LOG_INFO("loaded font data from dict");

    /* get a c-style nul-terminated string */
    cstr = alloca(str.comp_.sz + 1);
    memcpy(cstr, charstr(ctx, str), str.comp_.sz);
    cstr[str.comp_.sz] = '\0';
    XPOST_LOG_INFO("append nul to string");

    ret = _get_current_point(ctx, gs, &xpos, &ypos);
    if (ret)
        return ret;

    colorspace = bdcget(ctx, devdic, consname(ctx, "nativecolorspace"));
    if (objcmp(ctx, colorspace, consname(ctx, "DeviceGray")) == 0)
    {
        ncomp = 1;
        comp1 = bdcget(ctx, gs, consname(ctx, "colorcomp1"));
    }
    else if (objcmp(ctx, colorspace, consname(ctx, "DeviceRGB")) == 0)
    {
        ncomp = 3;
        comp1 = bdcget(ctx, gs, consname(ctx, "colorcomp1"));
        comp2 = bdcget(ctx, gs, consname(ctx, "colorcomp2"));
        comp3 = bdcget(ctx, gs, consname(ctx, "colorcomp3"));
    } else {
        XPOST_LOG_ERR("unimplemented device colorspace");
        return unregistered;
    }
    XPOST_LOG_INFO("ncomp = %d", ncomp);

    finalize = xpost_object_cvx(consbar(ctx, 5));
    /* fill-in final pos before return */
    barput(ctx, finalize, 0, xpost_cons_real(xpos));
    barput(ctx, finalize, 1, xpost_cons_real(ypos));
    barput(ctx, finalize, 2, xpost_object_cvx(consname(ctx, "itransform")));
    barput(ctx, finalize, 3, xpost_object_cvx(consname(ctx, "moveto")));
    barput(ctx, finalize, 4, xpost_object_cvx(consname(ctx, "flushpage")));
    xpost_stack_push(ctx->lo, ctx->es, finalize);

    /* render text in char *cstr  with font data  at pen position xpos ypos */
    has_kerning = xpost_font_face_kerning_has(data.face);
    glyph_previous = 0;
    for (ch = cstr; *ch; ch++) {
        _show_char(ctx, devdic, putpix, data, &xpos, &ypos, *ch, &glyph_previous, has_kerning,
                ncomp, comp1, comp2, comp3);
        if (*ch == charcode.int_.val)
        {
            xpos += cx.real_.val;
            ypos += cy.real_.val;
        }
    }

    /* update current position in the graphics state */
    barput(ctx, finalize, 0, xpost_cons_real(xpos));
    barput(ctx, finalize, 1, xpost_cons_real(ypos));

    return 0;
}

static
int _awidthshow (Xpost_Context *ctx,
                 Xpost_Object cx,
                 Xpost_Object cy,
                 Xpost_Object charcode,
                 Xpost_Object dx,
                 Xpost_Object dy,
                 Xpost_Object str)
{
    Xpost_Object userdict;
    Xpost_Object gd;
    Xpost_Object gs;
    Xpost_Object fontdict;
    Xpost_Object privatestr;
    struct fontdata data;
    char *cstr;
    real xpos, ypos;
    char *ch;
    Xpost_Object devdic;
    Xpost_Object putpix;
    Xpost_Object colorspace;
    int ncomp;
    Xpost_Object comp1, comp2, comp3;
    Xpost_Object finalize;
    int ret;

    int has_kerning;
    unsigned int glyph_previous;

    /* load the graphicsdict, current graphics state, and current font */
    userdict = xpost_stack_bottomup_fetch(ctx->lo, ctx->ds, 2);
    if (xpost_object_get_type(userdict) != dicttype)
        return dictstackunderflow;
    gd = bdcget(ctx, userdict, consname(ctx, "graphicsdict"));
    gs = bdcget(ctx, gd, consname(ctx, "currgstate"));
    fontdict = bdcget(ctx, gs, consname(ctx, "currfont"));
    if (xpost_object_get_type(fontdict) == invalidtype)
        return invalidfont;
    XPOST_LOG_INFO("loaded graphicsdict, graphics state, and current font");

    /* load the device and PutPix member function */
    devdic = bdcget(ctx, gs, consname(ctx, "device"));
    putpix = bdcget(ctx, devdic, consname(ctx, "PutPix"));
    XPOST_LOG_INFO("loaded DEVICE and PutPix");

    /* get the font data from the font dict */
    privatestr = bdcget(ctx, fontdict, consname(ctx, "Private"));
    if (xpost_object_get_type(privatestr) == invalidtype)
        return invalidfont;
    xpost_memory_get(xpost_context_select_memory(ctx, privatestr),
            xpost_object_get_ent(privatestr), 0, sizeof data, &data);
    if (data.face == NULL)
    {
        XPOST_LOG_ERR("face is NULL");
        return invalidfont;
    }
    XPOST_LOG_INFO("loaded font data from dict");

    /* get a c-style nul-terminated string */
    cstr = alloca(str.comp_.sz + 1);
    memcpy(cstr, charstr(ctx, str), str.comp_.sz);
    cstr[str.comp_.sz] = '\0';
    XPOST_LOG_INFO("append nul to string");

    ret = _get_current_point(ctx, gs, &xpos, &ypos);
    if (ret)
        return ret;

    colorspace = bdcget(ctx, devdic, consname(ctx, "nativecolorspace"));
    if (objcmp(ctx, colorspace, consname(ctx, "DeviceGray")) == 0)
    {
        ncomp = 1;
        comp1 = bdcget(ctx, gs, consname(ctx, "colorcomp1"));
    }
    else if (objcmp(ctx, colorspace, consname(ctx, "DeviceRGB")) == 0)
    {
        ncomp = 3;
        comp1 = bdcget(ctx, gs, consname(ctx, "colorcomp1"));
        comp2 = bdcget(ctx, gs, consname(ctx, "colorcomp2"));
        comp3 = bdcget(ctx, gs, consname(ctx, "colorcomp3"));
    } else {
        XPOST_LOG_ERR("unimplemented device colorspace");
        return unregistered;
    }
    XPOST_LOG_INFO("ncomp = %d", ncomp);

    finalize = xpost_object_cvx(consbar(ctx, 5));
    /* fill-in final pos before return */
    barput(ctx, finalize, 0, xpost_cons_real(xpos));
    barput(ctx, finalize, 1, xpost_cons_real(ypos));
    barput(ctx, finalize, 2, xpost_object_cvx(consname(ctx, "itransform")));
    barput(ctx, finalize, 3, xpost_object_cvx(consname(ctx, "moveto")));
    barput(ctx, finalize, 4, xpost_object_cvx(consname(ctx, "flushpage")));
    xpost_stack_push(ctx->lo, ctx->es, finalize);

    /* render text in char *cstr  with font data  at pen position xpos ypos */
    has_kerning = xpost_font_face_kerning_has(data.face);
    glyph_previous = 0;
    for (ch = cstr; *ch; ch++) {
        _show_char(ctx, devdic, putpix, data, &xpos, &ypos, *ch, &glyph_previous, has_kerning,
                ncomp, comp1, comp2, comp3);
        xpos += dx.real_.val;
        ypos += dy.real_.val;
        if (*ch == charcode.int_.val)
        {
            xpos += cx.real_.val;
            ypos += cy.real_.val;
        }
    }

    /* update current position in the graphics state */
    barput(ctx, finalize, 0, xpost_cons_real(xpos));
    barput(ctx, finalize, 1, xpost_cons_real(ypos));

    return 0;
}

static
int _stringwidth (Xpost_Context *ctx,
                  Xpost_Object str)
{
    Xpost_Object userdict;
    Xpost_Object gd;
    Xpost_Object gs;
    Xpost_Object fontdict;
    Xpost_Object privatestr;
    struct fontdata data;
    char *cstr;
    real xpos = 0, ypos = 0;
    char *ch;

    int has_kerning;
    unsigned int glyph_previous;

    /* load the graphicsdict, current graphics state, and current font */
    userdict = xpost_stack_bottomup_fetch(ctx->lo, ctx->ds, 2);
    if (xpost_object_get_type(userdict) != dicttype)
        return dictstackunderflow;
    gd = bdcget(ctx, userdict, consname(ctx, "graphicsdict"));
    gs = bdcget(ctx, gd, consname(ctx, "currgstate"));
    fontdict = bdcget(ctx, gs, consname(ctx, "currfont"));
    if (xpost_object_get_type(fontdict) == invalidtype)
        return invalidfont;
    XPOST_LOG_INFO("loaded graphicsdict, graphics state, and current font");

    /* get the font data from the font dict */
    privatestr = bdcget(ctx, fontdict, consname(ctx, "Private"));
    if (xpost_object_get_type(privatestr) == invalidtype)
        return invalidfont;
    xpost_memory_get(xpost_context_select_memory(ctx, privatestr),
            xpost_object_get_ent(privatestr), 0, sizeof data, &data);
    if (data.face == NULL)
    {
        XPOST_LOG_ERR("face is NULL");
        return invalidfont;
    }
    XPOST_LOG_INFO("loaded font data from dict");

    /* get a c-style nul-terminated string */
    cstr = alloca(str.comp_.sz + 1);
    memcpy(cstr, charstr(ctx, str), str.comp_.sz);
    cstr[str.comp_.sz] = '\0';
    XPOST_LOG_INFO("append nul to string");

    /* do everything BUT
       render text in char *cstr  with font data  at pen position xpos ypos */
    has_kerning = xpost_font_face_kerning_has(data.face);
    glyph_previous = 0;
    for (ch = cstr; *ch; ch++) {
        /* _show_char(ctx, devdic, putpix, data, &xpos, &ypos, *ch, &glyph_previous, has_kerning,
                ncomp, comp1, comp2, comp3); */

#ifdef HAVE_FREETYPE
        unsigned int glyph_index;
        unsigned char *buffer;
        int rows;
        int width;
        int pitch;
        char pixel_mode;
        int left;
        int top;
        long advance_x;
        long advance_y;

        glyph_index = xpost_font_face_glyph_index_get(data.face, *ch);
        if (has_kerning && glyph_previous && (glyph_index > 0))
        {
            long delta_x;
            long delta_y;

            if (xpost_font_face_kerning_delta_get(data.face, glyph_previous, glyph_index,
                                                  &delta_x, &delta_y))
            {
                xpos += delta_x >> 6;
                ypos += delta_y >> 6;
            }
        }
        if (!xpost_font_face_glyph_render(data.face, glyph_index))
            return unregistered;
        xpost_font_face_glyph_buffer_get(data.face, &buffer, &rows, &width, &pitch, &pixel_mode, &left, &top, &advance_x, &advance_y);
        /*
        _draw_bitmap(ctx, devdic, putpix,
                buffer, rows, width, pitch, pixel_mode,
                *xpos + left, *ypos - top,
                ncomp, comp1, comp2, comp3);
                */
        xpos += advance_x >> 6;
        ypos += advance_y >> 6;
        glyph_previous = glyph_index;
#endif

    }

    xpost_stack_push(ctx->lo, ctx->os, xpost_cons_real(xpos));
    xpost_stack_push(ctx->lo, ctx->os, xpost_cons_real(ypos));

    return 0;
}

int initopfont (Xpost_Context *ctx,
                Xpost_Object sd)
{
    oper *optab;
    Xpost_Object n,op;
    unsigned int optadr;

    assert(ctx->gl->base);
    xpost_memory_table_get_addr(ctx->gl,
            XPOST_MEMORY_TABLE_SPECIAL_OPERATOR_TABLE, &optadr);
    optab = (void *)(ctx->gl->base + optadr);

    op = consoper(ctx, "findfont", _findfont, 1, 1, nametype); INSTALL;
    op = consoper(ctx, "findfont", _findfont, 1, 1, stringtype); INSTALL;
    op = consoper(ctx, "scalefont", _scalefont, 1, 2, dicttype, floattype); INSTALL;
    //op = consoper(ctx, "makefont", _makefont, 1, 2, dicttype, arraytype); INSTALL;
    op = consoper(ctx, "setfont", _setfont, 1, 1, dicttype); INSTALL;

    op = consoper(ctx, "show", _show, 0, 1, stringtype); INSTALL;
    op = consoper(ctx, "ashow", _ashow, 0, 3,
        floattype, floattype, stringtype); INSTALL;
    op = consoper(ctx, "widthshow", _widthshow, 0, 4,
        floattype, floattype, integertype, stringtype); INSTALL;
    op = consoper(ctx, "awidthshow", _awidthshow, 0, 6,
        floattype, floattype, integertype,
        floattype, floattype, stringtype); INSTALL;
    op = consoper(ctx, "stringwidth", _stringwidth, 2, 1, stringtype); INSTALL;
    /*
    op = consoper(ctx, "kshow", _kshow, 0, 2,
        proctype, stringtype); INSTALL;
    */

    /* dumpdic(ctx->gl, sd); fflush(NULL);
    bdcput(ctx, sd, consname(ctx, "mark"), mark); */

    return 0;
}
