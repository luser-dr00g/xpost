/*
 * Xpost - a Level-2 Postscript interpreter
 * Copyright (C) 2013-2016, Michael Joshua Ryan
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

#include "xpost.h"
#include "xpost_log.h"
#include "xpost_memory.h"
#include "xpost_object.h"
#include "xpost_stack.h"
#include "xpost_font.h"
#include "xpost_save.h"
#include "xpost_context.h"
#include "xpost_error.h"
#include "xpost_name.h"
#include "xpost_string.h"
#include "xpost_array.h"
#include "xpost_dict.h"

//#include "xpost_interpreter.h"
#include "xpost_operator.h"
#include "xpost_op_font.h"

/*
 * FIXME: check if we can factorize show, ashow and kshow a bit.
 * These codes seem quite similar
 */

typedef struct fontdata
{
    void *face;
} fontdata;

static
int _findfont(Xpost_Context *ctx,
              Xpost_Object fontname)
{
#ifdef HAVE_FREETYPE2
    Xpost_Object fontstr;
    Xpost_Object fontdict;
    Xpost_Object privatestr;
    struct fontdata data;
    char *fname;
    Xpost_Object fontbbox;
    Xpost_Object fontbboxarray[4];

    if (xpost_object_get_type(fontname) == nametype)
        fontstr = xpost_name_get_string(ctx, fontname);
    else
        fontstr = fontname;
    fname = alloca(fontstr.comp_.sz + 1);
    memcpy(fname, xpost_string_get_pointer(ctx, fontstr), fontstr.comp_.sz);
    fname[fontstr.comp_.sz] = '\0';

    fontdict = xpost_dict_cons (ctx, 10);
    privatestr = xpost_string_cons(ctx, sizeof data, NULL);
    xpost_dict_put(ctx, fontdict, xpost_name_cons(ctx, "Private"), privatestr);
    xpost_dict_put(ctx, fontdict, xpost_name_cons(ctx, "FontName"), fontname);

    /* initialize font data, with x-scale and y-scale set to 1 */
    data.face = xpost_font_face_new_from_name(fname);
    if (data.face == NULL)
        return invalidfont;

    fontbbox = xpost_array_cons(ctx, 4);
    xpost_font_face_get_bbox(data.face, fontbboxarray);
    xpost_memory_put(xpost_context_select_memory(ctx, fontbbox),
		     xpost_object_get_ent(fontbbox),
		     0, 4 * sizeof(Xpost_Object), fontbboxarray);
    xpost_dict_put(ctx, fontdict, xpost_name_cons(ctx, "FontBBox"), fontbbox);

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

static int _scalefont(Xpost_Context *ctx, Xpost_Object fontdict, Xpost_Object size);


static
int _makefont(Xpost_Context *ctx,
              Xpost_Object fontdict,
              Xpost_Object psmat)
{
    Xpost_Object privatestr;
    struct fontdata data;

    //_scalefont(ctx, fontdict, xpost_real_cons(1.0));
    privatestr = xpost_dict_get(ctx, fontdict, xpost_name_cons(ctx, "Private"));
    if (xpost_object_get_type(privatestr) == invalidtype)
        return undefined;
    xpost_memory_get(xpost_context_select_memory(ctx, privatestr),
                     xpost_object_get_ent(privatestr), 0, sizeof(data), &data);

    if (data.face == NULL)
        return invalidfont;

    /* apply linear transform from the matrix */
    {
        float mat[6];
        int i;
        for (i = 0; i < 6; i++)
        {
            Xpost_Object el;
            el = xpost_array_get(ctx, psmat, i);
            switch (xpost_object_get_type(el))
            {
                case integertype: mat[i] = (float)el.int_.val; break;
                case realtype: mat[i] = el.real_.val; break;
                default: return typecheck;
            }
        }
        xpost_font_face_transform(data.face, mat);
    }

    xpost_stack_push(ctx->lo, ctx->os, fontdict);
    return 0;
}


static
int _scalefont(Xpost_Context *ctx,
               Xpost_Object fontdict,
               Xpost_Object size)
{
#if 1
    Xpost_Object privatestr;
    struct fontdata data;

    privatestr = xpost_dict_get(ctx, fontdict, xpost_name_cons(ctx, "Private"));
    if (xpost_object_get_type(privatestr) == invalidtype)
        return undefined;
    xpost_memory_get(xpost_context_select_memory(ctx, privatestr),
                     xpost_object_get_ent(privatestr), 0, sizeof data, &data);

    if (data.face == NULL)
        return invalidfont;

    /* scale x and y sizes by @p size */
    xpost_font_face_scale(data.face, size.real_.val);

    /* if face is really a pointer, there's nothing to save back to the string
    xpost_memory_put(xpost_context_select_memory(ctx, privatestr),
            xpost_object_get_ent(privatestr), 0, sizeof data, &data);
            */
    xpost_stack_push(ctx->lo, ctx->os, fontdict);
    return 0;
#else
    Xpost_Object psmat;
    psmat = xpost_array_cons(ctx, 6);
    xpost_array_put(ctx, psmat, 0, size);
    xpost_array_put(ctx, psmat, 1, xpost_real_cons(0.0));
    xpost_array_put(ctx, psmat, 2, xpost_real_cons(0.0));
    xpost_array_put(ctx, psmat, 3, size);
    xpost_array_put(ctx, psmat, 4, xpost_real_cons(0.0));
    xpost_array_put(ctx, psmat, 5, xpost_real_cons(0.0));
    return _makefont(ctx, fontdict, psmat);
#endif
}



static
int _setfont(Xpost_Context *ctx,
             Xpost_Object fontdict)
{
    Xpost_Object userdict;
    Xpost_Object gd;
    Xpost_Object gs;

    userdict = xpost_stack_bottomup_fetch(ctx->lo, ctx->ds, 2);
    if (xpost_object_get_type(userdict) != dicttype)
        return dictstackunderflow;
    gd = xpost_dict_get(ctx, userdict, xpost_name_cons(ctx, "graphicsdict"));
    gs = xpost_dict_get(ctx, gd, xpost_name_cons(ctx, "currgstate"));

    xpost_dict_put(ctx, gs, xpost_name_cons(ctx, "currfont"), fontdict);

    return 0;
}

#ifdef HAVE_FREETYPE2
static
void _draw_bitmap(Xpost_Context *ctx,
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
        //printf("\n");
        for (j = 0; j < width; j++)
        {
            //pix = tmp[j];
            switch (pixel_mode)
            {
                case XPOST_FONT_PIXEL_MODE_MONO:
                    pix = (tmp[j / 8] >> (7 - (j % 8))) & 1;
                    break;
                case XPOST_FONT_PIXEL_MODE_GRAY:
                    pix = tmp[j];
                    break;
                default:
                    XPOST_LOG_ERR("unsupported pixel_mode");
                    return;
            }
            //printf("%c", pix? 'X':'_');
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
                xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons(xpos + j));
                xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons(ypos + i));
                xpost_stack_push(ctx->lo, ctx->os, devdic);
                if (xpost_object_get_type(putpix) == operatortype)
                    xpost_stack_push(ctx->lo, ctx->es, putpix);
                else
                {
                    xpost_stack_push(ctx->lo, ctx->os, putpix);
                    xpost_stack_push(ctx->lo, ctx->es,
                                     xpost_name_cons(ctx, "exec"));
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
#ifdef HAVE_FREETYPE2
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
    //TODO check fontdict's /AutoKern bool
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
    xpost_font_face_glyph_buffer_get(data.face,
                                     &buffer, &rows, &width, &pitch, &pixel_mode,
                                     &left, &top, &advance_x, &advance_y);
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
    path = xpost_dict_get(ctx, gs, xpost_name_cons(ctx, "currpath"));
    subpath = xpost_dict_get(ctx,
                             path,
                             xpost_int_cons(xpost_dict_length_memory(xpost_context_select_memory(ctx,path), path) - 1));
    if (xpost_object_get_type(subpath) == invalidtype)
        return nocurrentpoint;
    pathelem = xpost_dict_get(ctx,
                              subpath, xpost_int_cons(xpost_dict_length_memory(xpost_context_select_memory(ctx,subpath), subpath) - 1));
    if (xpost_object_get_type(pathelem) == invalidtype)
        return nocurrentpoint;
    pathelemdata = xpost_dict_get(ctx, pathelem, xpost_name_cons(ctx, "data"));
    if (xpost_object_get_type(pathelemdata) == invalidtype)
        return nocurrentpoint;

    datax = xpost_array_get(ctx, pathelemdata, pathelemdata.comp_.sz - 2);
    datay = xpost_array_get(ctx, pathelemdata, pathelemdata.comp_.sz - 1);
    if (xpost_object_get_type(datax) == integertype)
        datax = xpost_real_cons((real)datax.int_.val);
    if (xpost_object_get_type(datay) == integertype)
        datay = xpost_real_cons((real)datay.int_.val);
    *xpos = datax.real_.val;
    *ypos = datay.real_.val;
    XPOST_LOG_INFO("currentpoint: %f %f", *xpos, *ypos);

    return 0;
}

static
int _show(Xpost_Context *ctx,
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
    gd = xpost_dict_get(ctx, userdict, xpost_name_cons(ctx, "graphicsdict"));
    gs = xpost_dict_get(ctx, gd, xpost_name_cons(ctx, "currgstate"));
    fontdict = xpost_dict_get(ctx, gs, xpost_name_cons(ctx, "currfont"));
    if (xpost_object_get_type(fontdict) == invalidtype)
        return invalidfont;
    XPOST_LOG_INFO("loaded graphicsdict, graphics state, and current font");

    /* load the device and PutPix member function */
    devdic = xpost_dict_get(ctx, gs, xpost_name_cons(ctx, "device"));
    putpix = xpost_dict_get(ctx, devdic, xpost_name_cons(ctx, "PutPix"));
    XPOST_LOG_INFO("loaded DEVICE and PutPix");

    /* get the font data from the font dict */
    privatestr = xpost_dict_get(ctx, fontdict, xpost_name_cons(ctx, "Private"));
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
    memcpy(cstr, xpost_string_get_pointer(ctx, str), str.comp_.sz);
    cstr[str.comp_.sz] = '\0';
    XPOST_LOG_INFO("append nul to string");

    ret = _get_current_point(ctx, gs, &xpos, &ypos);
    if (ret)
        return ret;

    colorspace = xpost_dict_get(ctx, devdic, xpost_name_cons(ctx, "nativecolorspace"));
    if (xpost_dict_compare_objects(ctx, colorspace, xpost_name_cons(ctx, "DeviceGray")) == 0)
    {
        ncomp = 1;
        comp1 = xpost_dict_get(ctx, gs, xpost_name_cons(ctx, "colorcomp1"));
    }
    else if (xpost_dict_compare_objects(ctx, colorspace, xpost_name_cons(ctx, "DeviceRGB")) == 0)
    {
        ncomp = 3;
        comp1 = xpost_dict_get(ctx, gs, xpost_name_cons(ctx, "colorcomp1"));
        comp2 = xpost_dict_get(ctx, gs, xpost_name_cons(ctx, "colorcomp2"));
        comp3 = xpost_dict_get(ctx, gs, xpost_name_cons(ctx, "colorcomp3"));
    }
    else
    {
        XPOST_LOG_ERR("unimplemented device colorspace");
        return unregistered;
    }
    XPOST_LOG_INFO("ncomp = %d", ncomp);

    finalize = xpost_object_cvx(xpost_array_cons(ctx, 5));
    /* fill-in final pos before return */
    xpost_array_put(ctx, finalize, 0, xpost_real_cons(xpos));
    xpost_array_put(ctx, finalize, 1, xpost_real_cons(ypos));
    xpost_array_put(ctx, finalize, 2, xpost_object_cvx(xpost_name_cons(ctx, "itransform")));
    xpost_array_put(ctx, finalize, 3, xpost_object_cvx(xpost_name_cons(ctx, "moveto")));
    xpost_array_put(ctx, finalize, 4, xpost_object_cvx(xpost_name_cons(ctx, "flushpage")));
    xpost_stack_push(ctx->lo, ctx->es, finalize);

    /* render text in char *cstr  with font data  at pen position xpos ypos */
    has_kerning = xpost_font_face_kerning_has(data.face);
    glyph_previous = 0;
    for (ch = cstr; *ch; ch++) {
        _show_char(ctx, devdic, putpix, data, &xpos, &ypos, *ch, &glyph_previous, has_kerning,
                ncomp, comp1, comp2, comp3);
    }

    /* update current position in the graphics state */
    xpost_array_put(ctx, finalize, 0, xpost_real_cons(xpos));
    xpost_array_put(ctx, finalize, 1, xpost_real_cons(ypos));

    return 0;
}

static
int _ashow(Xpost_Context *ctx,
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
    gd = xpost_dict_get(ctx, userdict, xpost_name_cons(ctx, "graphicsdict"));
    gs = xpost_dict_get(ctx, gd, xpost_name_cons(ctx, "currgstate"));
    fontdict = xpost_dict_get(ctx, gs, xpost_name_cons(ctx, "currfont"));
    if (xpost_object_get_type(fontdict) == invalidtype)
        return invalidfont;
    XPOST_LOG_INFO("loaded graphicsdict, graphics state, and current font");

    /* load the device and PutPix member function */
    devdic = xpost_dict_get(ctx, gs, xpost_name_cons(ctx, "device"));
    putpix = xpost_dict_get(ctx, devdic, xpost_name_cons(ctx, "PutPix"));
    XPOST_LOG_INFO("loaded DEVICE and PutPix");

    /* get the font data from the font dict */
    privatestr = xpost_dict_get(ctx, fontdict, xpost_name_cons(ctx, "Private"));
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
    memcpy(cstr, xpost_string_get_pointer(ctx, str), str.comp_.sz);
    cstr[str.comp_.sz] = '\0';
    XPOST_LOG_INFO("append nul to string");

    ret = _get_current_point(ctx, gs, &xpos, &ypos);
    if (ret)
        return ret;

    colorspace = xpost_dict_get(ctx, devdic, xpost_name_cons(ctx, "nativecolorspace"));
    if (xpost_dict_compare_objects(ctx, colorspace, xpost_name_cons(ctx, "DeviceGray")) == 0)
    {
        ncomp = 1;
        comp1 = xpost_dict_get(ctx, gs, xpost_name_cons(ctx, "colorcomp1"));
    }
    else if (xpost_dict_compare_objects(ctx, colorspace, xpost_name_cons(ctx, "DeviceRGB")) == 0)
    {
        ncomp = 3;
        comp1 = xpost_dict_get(ctx, gs, xpost_name_cons(ctx, "colorcomp1"));
        comp2 = xpost_dict_get(ctx, gs, xpost_name_cons(ctx, "colorcomp2"));
        comp3 = xpost_dict_get(ctx, gs, xpost_name_cons(ctx, "colorcomp3"));
    }
    else
    {
        XPOST_LOG_ERR("unimplemented device colorspace");
        return unregistered;
    }
    XPOST_LOG_INFO("ncomp = %d", ncomp);

    finalize = xpost_object_cvx(xpost_array_cons(ctx, 5));
    /* fill-in final pos before return */
    xpost_array_put(ctx, finalize, 0, xpost_real_cons(xpos));
    xpost_array_put(ctx, finalize, 1, xpost_real_cons(ypos));
    xpost_array_put(ctx, finalize, 2, xpost_object_cvx(xpost_name_cons(ctx, "itransform")));
    xpost_array_put(ctx, finalize, 3, xpost_object_cvx(xpost_name_cons(ctx, "moveto")));
    xpost_array_put(ctx, finalize, 4, xpost_object_cvx(xpost_name_cons(ctx, "flushpage")));
    xpost_stack_push(ctx->lo, ctx->es, finalize);

    /* render text in char *cstr  with font data  at pen position xpos ypos */
    has_kerning = xpost_font_face_kerning_has(data.face);
    glyph_previous = 0;
    for (ch = cstr; *ch; ch++)
    {
        _show_char(ctx, devdic, putpix, data, &xpos, &ypos, *ch, &glyph_previous, has_kerning,
                   ncomp, comp1, comp2, comp3);
        xpos += dx.real_.val;
        ypos += dy.real_.val;
    }

    /* update current position in the graphics state */
    xpost_array_put(ctx, finalize, 0, xpost_real_cons(xpos));
    xpost_array_put(ctx, finalize, 1, xpost_real_cons(ypos));

    return 0;
}

static
int _widthshow(Xpost_Context *ctx,
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
    gd = xpost_dict_get(ctx, userdict, xpost_name_cons(ctx, "graphicsdict"));
    gs = xpost_dict_get(ctx, gd, xpost_name_cons(ctx, "currgstate"));
    fontdict = xpost_dict_get(ctx, gs, xpost_name_cons(ctx, "currfont"));
    if (xpost_object_get_type(fontdict) == invalidtype)
        return invalidfont;
    XPOST_LOG_INFO("loaded graphicsdict, graphics state, and current font");

    /* load the device and PutPix member function */
    devdic = xpost_dict_get(ctx, gs, xpost_name_cons(ctx, "device"));
    putpix = xpost_dict_get(ctx, devdic, xpost_name_cons(ctx, "PutPix"));
    XPOST_LOG_INFO("loaded DEVICE and PutPix");

    /* get the font data from the font dict */
    privatestr = xpost_dict_get(ctx, fontdict, xpost_name_cons(ctx, "Private"));
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
    memcpy(cstr, xpost_string_get_pointer(ctx, str), str.comp_.sz);
    cstr[str.comp_.sz] = '\0';
    XPOST_LOG_INFO("append nul to string");

    ret = _get_current_point(ctx, gs, &xpos, &ypos);
    if (ret)
        return ret;

    colorspace = xpost_dict_get(ctx, devdic, xpost_name_cons(ctx, "nativecolorspace"));
    if (xpost_dict_compare_objects(ctx, colorspace, xpost_name_cons(ctx, "DeviceGray")) == 0)
    {
        ncomp = 1;
        comp1 = xpost_dict_get(ctx, gs, xpost_name_cons(ctx, "colorcomp1"));
    }
    else if (xpost_dict_compare_objects(ctx, colorspace, xpost_name_cons(ctx, "DeviceRGB")) == 0)
    {
        ncomp = 3;
        comp1 = xpost_dict_get(ctx, gs, xpost_name_cons(ctx, "colorcomp1"));
        comp2 = xpost_dict_get(ctx, gs, xpost_name_cons(ctx, "colorcomp2"));
        comp3 = xpost_dict_get(ctx, gs, xpost_name_cons(ctx, "colorcomp3"));
    }
    else
    {
        XPOST_LOG_ERR("unimplemented device colorspace");
        return unregistered;
    }
    XPOST_LOG_INFO("ncomp = %d", ncomp);

    finalize = xpost_object_cvx(xpost_array_cons(ctx, 5));
    /* fill-in final pos before return */
    xpost_array_put(ctx, finalize, 0, xpost_real_cons(xpos));
    xpost_array_put(ctx, finalize, 1, xpost_real_cons(ypos));
    xpost_array_put(ctx, finalize, 2, xpost_object_cvx(xpost_name_cons(ctx, "itransform")));
    xpost_array_put(ctx, finalize, 3, xpost_object_cvx(xpost_name_cons(ctx, "moveto")));
    xpost_array_put(ctx, finalize, 4, xpost_object_cvx(xpost_name_cons(ctx, "flushpage")));
    xpost_stack_push(ctx->lo, ctx->es, finalize);

    /* render text in char *cstr  with font data  at pen position xpos ypos */
    has_kerning = xpost_font_face_kerning_has(data.face);
    glyph_previous = 0;
    for (ch = cstr; *ch; ch++)
    {
        _show_char(ctx, devdic, putpix, data, &xpos, &ypos, *ch, &glyph_previous, has_kerning,
                   ncomp, comp1, comp2, comp3);
        if (*ch == charcode.int_.val)
        {
            xpos += cx.real_.val;
            ypos += cy.real_.val;
        }
    }

    /* update current position in the graphics state */
    xpost_array_put(ctx, finalize, 0, xpost_real_cons(xpos));
    xpost_array_put(ctx, finalize, 1, xpost_real_cons(ypos));

    return 0;
}

static
int _awidthshow(Xpost_Context *ctx,
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
    gd = xpost_dict_get(ctx, userdict, xpost_name_cons(ctx, "graphicsdict"));
    gs = xpost_dict_get(ctx, gd, xpost_name_cons(ctx, "currgstate"));
    fontdict = xpost_dict_get(ctx, gs, xpost_name_cons(ctx, "currfont"));
    if (xpost_object_get_type(fontdict) == invalidtype)
        return invalidfont;
    XPOST_LOG_INFO("loaded graphicsdict, graphics state, and current font");

    /* load the device and PutPix member function */
    devdic = xpost_dict_get(ctx, gs, xpost_name_cons(ctx, "device"));
    putpix = xpost_dict_get(ctx, devdic, xpost_name_cons(ctx, "PutPix"));
    XPOST_LOG_INFO("loaded DEVICE and PutPix");

    /* get the font data from the font dict */
    privatestr = xpost_dict_get(ctx, fontdict, xpost_name_cons(ctx, "Private"));
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
    memcpy(cstr, xpost_string_get_pointer(ctx, str), str.comp_.sz);
    cstr[str.comp_.sz] = '\0';
    XPOST_LOG_INFO("append nul to string");

    ret = _get_current_point(ctx, gs, &xpos, &ypos);
    if (ret)
        return ret;

    colorspace = xpost_dict_get(ctx, devdic, xpost_name_cons(ctx, "nativecolorspace"));
    if (xpost_dict_compare_objects(ctx, colorspace, xpost_name_cons(ctx, "DeviceGray")) == 0)
    {
        ncomp = 1;
        comp1 = xpost_dict_get(ctx, gs, xpost_name_cons(ctx, "colorcomp1"));
    }
    else if (xpost_dict_compare_objects(ctx, colorspace, xpost_name_cons(ctx, "DeviceRGB")) == 0)
    {
        ncomp = 3;
        comp1 = xpost_dict_get(ctx, gs, xpost_name_cons(ctx, "colorcomp1"));
        comp2 = xpost_dict_get(ctx, gs, xpost_name_cons(ctx, "colorcomp2"));
        comp3 = xpost_dict_get(ctx, gs, xpost_name_cons(ctx, "colorcomp3"));
    }
    else
    {
        XPOST_LOG_ERR("unimplemented device colorspace");
        return unregistered;
    }
    XPOST_LOG_INFO("ncomp = %d", ncomp);

    finalize = xpost_object_cvx(xpost_array_cons(ctx, 5));
    /* fill-in final pos before return */
    xpost_array_put(ctx, finalize, 0, xpost_real_cons(xpos));
    xpost_array_put(ctx, finalize, 1, xpost_real_cons(ypos));
    xpost_array_put(ctx, finalize, 2, xpost_object_cvx(xpost_name_cons(ctx, "itransform")));
    xpost_array_put(ctx, finalize, 3, xpost_object_cvx(xpost_name_cons(ctx, "moveto")));
    xpost_array_put(ctx, finalize, 4, xpost_object_cvx(xpost_name_cons(ctx, "flushpage")));
    xpost_stack_push(ctx->lo, ctx->es, finalize);

    /* render text in char *cstr  with font data  at pen position xpos ypos */
    has_kerning = xpost_font_face_kerning_has(data.face);
    glyph_previous = 0;
    for (ch = cstr; *ch; ch++)
    {
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
    xpost_array_put(ctx, finalize, 0, xpost_real_cons(xpos));
    xpost_array_put(ctx, finalize, 1, xpost_real_cons(ypos));

    return 0;
}

static
int _stringwidth(Xpost_Context *ctx,
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
    gd = xpost_dict_get(ctx, userdict, xpost_name_cons(ctx, "graphicsdict"));
    gs = xpost_dict_get(ctx, gd, xpost_name_cons(ctx, "currgstate"));
    fontdict = xpost_dict_get(ctx, gs, xpost_name_cons(ctx, "currfont"));
    if (xpost_object_get_type(fontdict) == invalidtype)
        return invalidfont;
    XPOST_LOG_INFO("loaded graphicsdict, graphics state, and current font");

    /* get the font data from the font dict */
    privatestr = xpost_dict_get(ctx, fontdict, xpost_name_cons(ctx, "Private"));
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
    memcpy(cstr, xpost_string_get_pointer(ctx, str), str.comp_.sz);
    cstr[str.comp_.sz] = '\0';
    XPOST_LOG_INFO("append nul to string");

    /* do everything BUT
       render text in char *cstr  with font data  at pen position xpos ypos */
    has_kerning = xpost_font_face_kerning_has(data.face);
    glyph_previous = 0;
    for (ch = cstr; *ch; ch++)
    {
        /* _show_char(ctx, devdic, putpix, data, &xpos, &ypos, *ch, &glyph_previous, has_kerning,
                ncomp, comp1, comp2, comp3); */

#ifdef HAVE_FREETYPE2
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

    xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons(xpos));
    xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons(ypos));

    return 0;
}

static
int _kshow(Xpost_Context *ctx,
           Xpost_Object proc,
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

    (void) &proc;
    /* load the graphicsdict, current graphics state, and current font */
    userdict = xpost_stack_bottomup_fetch(ctx->lo, ctx->ds, 2);
    if (xpost_object_get_type(userdict) != dicttype)
        return dictstackunderflow;
    gd = xpost_dict_get(ctx, userdict, xpost_name_cons(ctx, "graphicsdict"));
    gs = xpost_dict_get(ctx, gd, xpost_name_cons(ctx, "currgstate"));
    fontdict = xpost_dict_get(ctx, gs, xpost_name_cons(ctx, "currfont"));
    if (xpost_object_get_type(fontdict) == invalidtype)
        return invalidfont;
    XPOST_LOG_INFO("loaded graphicsdict, graphics state, and current font");

    /* load the device and PutPix member function */
    devdic = xpost_dict_get(ctx, gs, xpost_name_cons(ctx, "device"));
    putpix = xpost_dict_get(ctx, devdic, xpost_name_cons(ctx, "PutPix"));
    XPOST_LOG_INFO("loaded DEVICE and PutPix");

    /* get the font data from the font dict */
    privatestr = xpost_dict_get(ctx, fontdict, xpost_name_cons(ctx, "Private"));
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
    memcpy(cstr, xpost_string_get_pointer(ctx, str), str.comp_.sz);
    cstr[str.comp_.sz] = '\0';
    XPOST_LOG_INFO("append nul to string");

    ret = _get_current_point(ctx, gs, &xpos, &ypos);
    if (ret)
        return ret;

    colorspace = xpost_dict_get(ctx, devdic, xpost_name_cons(ctx, "nativecolorspace"));
    if (xpost_dict_compare_objects(ctx, colorspace, xpost_name_cons(ctx, "DeviceGray")) == 0)
    {
        ncomp = 1;
        comp1 = xpost_dict_get(ctx, gs, xpost_name_cons(ctx, "colorcomp1"));
    }
    else if (xpost_dict_compare_objects(ctx, colorspace, xpost_name_cons(ctx, "DeviceRGB")) == 0)
    {
        ncomp = 3;
        comp1 = xpost_dict_get(ctx, gs, xpost_name_cons(ctx, "colorcomp1"));
        comp2 = xpost_dict_get(ctx, gs, xpost_name_cons(ctx, "colorcomp2"));
        comp3 = xpost_dict_get(ctx, gs, xpost_name_cons(ctx, "colorcomp3"));
    }
    else
    {
        XPOST_LOG_ERR("unimplemented device colorspace");
        return unregistered;
    }
    XPOST_LOG_INFO("ncomp = %d", ncomp);

    finalize = xpost_object_cvx(xpost_array_cons(ctx, 5));
    /* fill-in final pos before return */
    xpost_array_put(ctx, finalize, 0, xpost_real_cons(xpos));
    xpost_array_put(ctx, finalize, 1, xpost_real_cons(ypos));
    xpost_array_put(ctx, finalize, 2, xpost_object_cvx(xpost_name_cons(ctx, "itransform")));
    xpost_array_put(ctx, finalize, 3, xpost_object_cvx(xpost_name_cons(ctx, "moveto")));
    xpost_array_put(ctx, finalize, 4, xpost_object_cvx(xpost_name_cons(ctx, "flushpage")));
    xpost_stack_push(ctx->lo, ctx->es, finalize);

    /* render text in char *cstr  with font data  at pen position xpos ypos */
    has_kerning = xpost_font_face_kerning_has(data.face);
    glyph_previous = 0;
    for (ch = cstr; *ch; ch++)
    {
        _show_char(ctx, devdic, putpix, data, &xpos, &ypos, *ch, &glyph_previous, has_kerning,
                ncomp, comp1, comp2, comp3);
    }

    /* update current position in the graphics state */
    xpost_array_put(ctx, finalize, 0, xpost_real_cons(xpos));
    xpost_array_put(ctx, finalize, 1, xpost_real_cons(ypos));

    return 0;
}

int xpost_oper_init_font_ops(Xpost_Context *ctx,
                             Xpost_Object sd)
{
    Xpost_Operator *optab;
    Xpost_Object n,op;
    unsigned int optadr;

    assert(ctx->gl->base);
    //xpost_memory_table_get_addr(ctx->gl, XPOST_MEMORY_TABLE_SPECIAL_OPERATOR_TABLE, &optadr);
    //optab = (void *)(ctx->gl->base + optadr);

    op = xpost_operator_cons(ctx, "findfont", (Xpost_Op_Func)_findfont, 1, 1, nametype);
    INSTALL;
    op = xpost_operator_cons(ctx, "findfont", (Xpost_Op_Func)_findfont, 1, 1, stringtype);
    INSTALL;
    op = xpost_operator_cons(ctx, "scalefont", (Xpost_Op_Func)_scalefont, 1, 2, dicttype, floattype);
    INSTALL;
    op = xpost_operator_cons(ctx, "makefont", (Xpost_Op_Func)_makefont, 1, 2, dicttype, arraytype);
    INSTALL;
    op = xpost_operator_cons(ctx, "setfont", (Xpost_Op_Func)_setfont, 1, 1, dicttype);
    INSTALL;

    op = xpost_operator_cons(ctx, "show", (Xpost_Op_Func)_show, 0, 1, stringtype);
    INSTALL;
    op = xpost_operator_cons(ctx, "ashow", (Xpost_Op_Func)_ashow, 0, 3,
        floattype, floattype, stringtype);
    INSTALL;
    op = xpost_operator_cons(ctx, "widthshow", (Xpost_Op_Func)_widthshow, 0, 4,
        floattype, floattype, integertype, stringtype);
    INSTALL;
    op = xpost_operator_cons(ctx, "awidthshow", (Xpost_Op_Func)_awidthshow, 0, 6,
        floattype, floattype, integertype,
        floattype, floattype, stringtype);
    INSTALL;
    op = xpost_operator_cons(ctx, "stringwidth", (Xpost_Op_Func)_stringwidth, 2, 1, stringtype);
    INSTALL;
    op = xpost_operator_cons(ctx, "kshow", (Xpost_Op_Func)_kshow, 0, 2, proctype, stringtype);
    INSTALL;

    /* xpost_dict_dump_memory (ctx->gl, sd); fflush(NULL);
    xpost_dict_put(ctx, sd, xpost_name_cons(ctx, "mark"), mark); */

    return 0;
}
