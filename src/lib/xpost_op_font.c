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

#include <stdlib.h> /* NULL strtod */
#include <stddef.h>

#include <assert.h>
#include <math.h> /* sqrt */
#include <stdio.h>
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
#include "xpost_dev_generic.h" /* pdfwrite accumulator access for glyph outlines */

/*
 * FIXME: check if we can factorize show, ashow and kshow a bit.
 * These codes seem quite similar
 */

typedef struct fontdata
{
    void *face;
    void *program;  /* malloc'd font program backing a memory face (Type 42) */
} fontdata;

/* per-text-operator rendering configuration, gathered once from the
   font dictionary, the device dictionary and the graphics state */
typedef struct textstate
{
    Xpost_Object encoding;  /* the font's /Encoding array, or invalid */
    Xpost_Object blendpix;  /* the device's BlendPix method, or invalid */
    int blend;              /* anti-alias: TextAlphaBits > 1 and BlendPix present */
    int vector;             /* the device consumes glyph outlines, not bitmaps */
    int extents;            /* the device consumes glyph ink extents, not marks */
    Xpost_Object fillrect;  /* the device's FillRect, for extent reporting */
    int sepindex;           /* separation registered with the device, or -1 */
    double septint;         /* the separation's tint */
} textstate;

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
    fname = xpost_string_allocate_cstring(ctx, fontstr);

    fontdict = xpost_dict_cons (ctx, 10);
    privatestr = xpost_string_cons(ctx, sizeof data, NULL);
    xpost_dict_put(ctx, fontdict, xpost_name_cons(ctx, "Private"), privatestr);
    xpost_dict_put(ctx, fontdict, xpost_name_cons(ctx, "FontName"), fontname);

    /* initialize font data, with x-scale and y-scale set to 1.
       Faces are cached per name: each face maps the font file and
       holds FreeType state, so creating one per findfont grows the
       process by a mapping per lookup. The face is shared between
       font dictionaries exactly as a FontDirectory-cached dictionary
       already shares it. */
    {
        static struct { char *name; void *face; } face_cache[32];
        static int face_cache_n = 0;
        int fi;
        data.face = NULL;
        data.program = NULL;
        for (fi = 0; fi < face_cache_n; fi++)
        {
            if (strcmp(face_cache[fi].name, fname) == 0)
            {
                data.face = face_cache[fi].face;
                break;
            }
        }
        if (data.face == NULL)
        {
            data.face = xpost_font_face_new_from_name(fname);
            if (data.face != NULL && face_cache_n < 32)
            {
                face_cache[face_cache_n].name = strdup(fname);
                face_cache[face_cache_n].face = data.face;
                face_cache_n++;
            }
        }
    }
    if (data.face == NULL){
        free(fname);
        return invalidfont;
    }

    fontbbox = xpost_array_cons(ctx, 4);
    xpost_font_face_get_bbox(data.face, fontbboxarray);
    xpost_memory_put(xpost_context_select_memory(ctx, fontbbox),
		     xpost_object_get_ent(fontbbox),
		     0, 4 * sizeof(Xpost_Object), fontbboxarray);
    xpost_dict_put(ctx, fontdict, xpost_name_cons(ctx, "FontBBox"), fontbbox);

    xpost_memory_put(xpost_context_select_memory(ctx, privatestr),
            xpost_object_get_ent(privatestr), 0, sizeof data, &data);
    xpost_stack_push(ctx->lo, ctx->os, fontdict);
    free(fname);
    return 0;
#else
    (void)ctx;
    (void)fontname;
    return invalidfont;
#endif
}

/* Load a Type 42 font program: reassemble the /sfnts strings into one
   malloc'd buffer, open it as a memory face, and stash the face in the
   dict's /Private exactly as findfont does for a file face. The buffer
   backs the face for the face's lifetime; like the findfont face cache,
   defined fonts live for the process. */
static
int _loadfont42(Xpost_Context *ctx,
                Xpost_Object fontdict)
{
#ifdef HAVE_FREETYPE2
    Xpost_Object sfnts;
    Xpost_Object privatestr;
    Xpost_Object fontbbox;
    Xpost_Object fontbboxarray[4];
    struct fontdata data;
    unsigned char *buf;
    size_t total;
    int i;

    sfnts = xpost_dict_get(ctx, fontdict, xpost_name_cons(ctx, "sfnts"));
    if (xpost_object_get_type(sfnts) != arraytype)
        return invalidfont;
    total = 0;
    for (i = 0; i < sfnts.comp_.sz; i++)
    {
        Xpost_Object s = xpost_array_get(ctx, sfnts, i);
        if (xpost_object_get_type(s) != stringtype)
            return invalidfont;
        total += s.comp_.sz;
    }
    if (total == 0)
        return invalidfont;
    buf = malloc(total);
    if (!buf)
        return VMerror;
    total = 0;
    for (i = 0; i < sfnts.comp_.sz; i++)
    {
        Xpost_Object s = xpost_array_get(ctx, sfnts, i);
        memcpy(buf + total, xpost_string_get_pointer(ctx, s), s.comp_.sz);
        total += s.comp_.sz;
    }

    data.face = xpost_font_face_new_from_memory(buf, total);
    data.program = buf;
    if (data.face == NULL)
    {
        free(buf);
        return invalidfont;
    }

    fontbbox = xpost_array_cons(ctx, 4);
    xpost_font_face_get_bbox(data.face, fontbboxarray);
    xpost_memory_put(xpost_context_select_memory(ctx, fontbbox),
                     xpost_object_get_ent(fontbbox),
                     0, 4 * sizeof(Xpost_Object), fontbboxarray);
    xpost_dict_put(ctx, fontdict, xpost_name_cons(ctx, "FontBBox"), fontbbox);

    privatestr = xpost_string_cons(ctx, sizeof data, NULL);
    xpost_memory_put(xpost_context_select_memory(ctx, privatestr),
                     xpost_object_get_ent(privatestr), 0, sizeof data, &data);
    xpost_dict_put(ctx, fontdict, xpost_name_cons(ctx, "Private"), privatestr);
    return 0;
#else
    (void)ctx;
    (void)fontdict;
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

static
textstate _text_state_get(Xpost_Context *ctx,
                          Xpost_Object fontdict,
                          Xpost_Object devdic,
                          Xpost_Object gs)
{
    textstate ts;
    Xpost_Object tab, vec, sep;

    ts.encoding = xpost_dict_get(ctx, fontdict, xpost_name_cons(ctx, "Encoding"));
    ts.blendpix = xpost_dict_get(ctx, devdic, xpost_name_cons(ctx, "BlendPix"));
    tab = xpost_dict_get(ctx, devdic, xpost_name_cons(ctx, "TextAlphaBits"));
    ts.blend = xpost_object_get_type(ts.blendpix) == operatortype
            && xpost_object_get_type(tab) == integertype
            && tab.int_.val > 1;
    vec = xpost_dict_get(ctx, devdic, xpost_name_cons(ctx, "VectorGlyphs"));
    ts.vector = xpost_object_get_type(vec) == booleantype && vec.int_.val;
    /* an extent-tracking device (the bbox device) needs no glyph
       rasterization: each glyph contributes its ink box through the
       device's FillRect instead */
    vec = xpost_dict_get(ctx, devdic, xpost_name_cons(ctx, "GlyphExtents"));
    ts.extents = xpost_object_get_type(vec) == booleantype && vec.int_.val;
    memset(&ts.fillrect, 0, sizeof ts.fillrect);  /* invalidtype */
    if (ts.extents)
        ts.fillrect = xpost_dict_get(ctx, devdic, xpost_name_cons(ctx, "FillRect"));
    /* a separation the graphics state registered with the device:
       glyph outlines fill in the separation, not the process colour */
    ts.sepindex = -1;
    ts.septint = 0.0;
    sep = xpost_dict_get(ctx, gs, xpost_name_cons(ctx, "sepindex"));
    if (xpost_object_get_type(sep) == integertype)
    {
        Xpost_Object tint = xpost_dict_get(ctx, gs, xpost_name_cons(ctx, "septint"));
        ts.sepindex = sep.int_.val;
        if (xpost_object_get_type(tint) == realtype)
            ts.septint = tint.real_.val;
        else if (xpost_object_get_type(tint) == integertype)
            ts.septint = (double)tint.int_.val;
    }
    return ts;
}

/* Map a character code to a glyph index. When the font carries an
   /Encoding array with a glyph name at this code, the name selects the
   glyph; codes whose entry is not a name (the findfont wrapper fills
   /Encoding with nulls), or whose name the face does not know, fall
   back to the face's character map, preserving the plain-text
   behaviour of an unencoded font. */
static
unsigned int _glyph_index_for_char(Xpost_Context *ctx,
                                   Xpost_Object encoding,
                                   void *face,
                                   unsigned int ch)
{
    if (xpost_object_get_type(encoding) == arraytype
     && ch < (unsigned int)encoding.comp_.sz)
    {
        Xpost_Object en = xpost_array_get(ctx, encoding, ch);
        if (xpost_object_get_type(en) == nametype)
        {
            Xpost_Object str = xpost_name_get_string(ctx, en);
            char *cname = xpost_string_allocate_cstring(ctx, str);
            unsigned int gi = 0;
            if (cname)
            {
                if (strcmp(cname, ".notdef") == 0)
                {
                    free(cname);
                    return 0;
                }
                gi = xpost_font_face_glyph_name_index_get(face, cname);
                free(cname);
            }
            if (gi)
                return gi;
        }
    }
    return xpost_font_face_glyph_index_get(face, (char)ch);
}

#ifdef HAVE_FREETYPE2
/* Give the face the current orientation before using it. The pixel
   size set by scalefont carries the CTM's magnitude (the scalefont
   wrapper in font.ps measures the size through dtransform), so glyphs
   come out the right size but always upright. Rotation, shear and
   anisotropy live in the CTM's linear part: normalize the magnitude
   out and conjugate by the y flip that relates FreeType's y-up glyph
   space to the device's y-down raster, and install the result as the
   face's transform. The face is shared through the font cache and the
   transform is sticky, so each text operator refreshes it from the
   graphics state it runs under. */
static
void _face_transform_from_ctm(Xpost_Context *ctx,
                              Xpost_Object gs,
                              void *face)
{
    Xpost_Object psmat;
    real m[4];
    real q;
    float mat[6] = { 0 };
    int i;

    psmat = xpost_dict_get(ctx, gs, xpost_name_cons(ctx, "currmatrix"));
    if (xpost_object_get_type(psmat) != arraytype || psmat.comp_.sz != 6)
        return;
    for (i = 0; i < 4; i++)
    {
        Xpost_Object el = xpost_array_get(ctx, psmat, i);
        m[i] = xpost_object_get_type(el) == realtype ? el.real_.val
             : (real)el.int_.val;
    }
    q = (real)sqrt(m[0] * m[0] + m[1] * m[1]);
    if (q == 0)
        return;
    mat[0] = (float)( m[0] / q);   /* xx */
    mat[1] = (float)( m[2] / q);   /* xy */
    mat[2] = (float)(-m[1] / q);   /* yx */
    mat[3] = (float)(-m[3] / q);   /* yy */
    xpost_font_face_transform(face, mat);
}

/* Resolve the current colour into the device's native space. Gray and
   RGB devices receive the raw graphics-state components, as the marking
   pipeline has always supplied them. A CMYK device receives a proper
   conversion from the colour's source space (matching the ColorConversion
   table in color.ps, with full black generation and undercolor removal),
   since the raw components are in whatever space the colour was set in.
   Returns 0 on success. */
static
int _device_color(Xpost_Context *ctx,
                  Xpost_Object gs,
                  Xpost_Object devdic,
                  int *ncomp,
                  Xpost_Object comp[4])
{
#define GSREAL(name) (xpost_dict_get(ctx, gs, xpost_name_cons(ctx, name)))
#define OBJVAL(o) (xpost_object_get_type(o) == realtype ? (o).real_.val \
                                                        : (double)(o).int_.val)
    Xpost_Object colorspace;

    colorspace = xpost_dict_get(ctx, devdic, xpost_name_cons(ctx, "nativecolorspace"));
    if (xpost_dict_compare_objects(ctx, colorspace, xpost_name_cons(ctx, "DeviceGray")) == 0)
    {
        *ncomp = 1;
        comp[0] = GSREAL("colorcomp1");
    }
    else if (xpost_dict_compare_objects(ctx, colorspace, xpost_name_cons(ctx, "DeviceRGB")) == 0)
    {
        *ncomp = 3;
        comp[0] = GSREAL("colorcomp1");
        comp[1] = GSREAL("colorcomp2");
        comp[2] = GSREAL("colorcomp3");
    }
    else if (xpost_dict_compare_objects(ctx, colorspace, xpost_name_cons(ctx, "DeviceCMYK")) == 0)
    {
        Xpost_Object srcspace = xpost_dict_get(ctx, gs, xpost_name_cons(ctx, "colorspace"));
        double c, m, y, k;

        if (xpost_dict_compare_objects(ctx, srcspace, xpost_name_cons(ctx, "DeviceCMYK")) == 0)
        {
            c = OBJVAL(GSREAL("colorcomp1"));
            m = OBJVAL(GSREAL("colorcomp2"));
            y = OBJVAL(GSREAL("colorcomp3"));
            k = OBJVAL(GSREAL("colorcomp4"));
        }
        else if (xpost_dict_compare_objects(ctx, srcspace, xpost_name_cons(ctx, "DeviceRGB")) == 0)
        {
            c = 1 - OBJVAL(GSREAL("colorcomp1"));
            m = 1 - OBJVAL(GSREAL("colorcomp2"));
            y = 1 - OBJVAL(GSREAL("colorcomp3"));
            k = c < m ? c : m;
            if (y < k) k = y;
            c -= k; m -= k; y -= k;
        }
        else /* DeviceGray */
        {
            c = m = y = 0;
            k = 1 - OBJVAL(GSREAL("colorcomp1"));
        }
        *ncomp = 4;
        comp[0] = xpost_real_cons((real)c);
        comp[1] = xpost_real_cons((real)m);
        comp[2] = xpost_real_cons((real)y);
        comp[3] = xpost_real_cons((real)k);
    }
    else
    {
        XPOST_LOG_ERR("unimplemented device colorspace");
        return unregistered;
    }
    return 0;
#undef GSREAL
#undef OBJVAL
}

/* Plot a rendered glyph bitmap through the device. An 8-bit coverage
   bitmap is thresholded at half coverage -- the sharp rasterization a
   scan conversion of the outline would produce -- unless the device
   anti-aliases text (ts->blend), in which case fully covered pixels go
   through PutPix and partially covered edge pixels through the
   device's BlendPix with their coverage. */
static
void _draw_bitmap(Xpost_Context *ctx,
                  Xpost_Object devdic,
                  Xpost_Object putpix,
                  const textstate *ts,
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
                  Xpost_Object comp3,
                  Xpost_Object comp4)
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
        for (j = 0; j < width; j++)
        {
            int cov = -1;  /* -1 solid, 0 skip, else blend coverage */

            switch (pixel_mode)
            {
                case XPOST_FONT_PIXEL_MODE_MONO:
                    pix = (tmp[j / 8] >> (7 - (j % 8))) & 1;
                    cov = pix ? -1 : 0;
                    break;
                case XPOST_FONT_PIXEL_MODE_GRAY:
                    pix = tmp[j];
                    if (ts->blend)
                        cov = pix == 255 ? -1 : (int)pix;
                    else
                        cov = pix >= 128 ? -1 : 0;
                    break;
                default:
                    XPOST_LOG_ERR("unsupported pixel_mode");
                    return;
            }
            if (cov)
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
                    case 4:
                        xpost_stack_push(ctx->lo, ctx->os, comp1);
                        xpost_stack_push(ctx->lo, ctx->os, comp2);
                        xpost_stack_push(ctx->lo, ctx->os, comp3);
                        xpost_stack_push(ctx->lo, ctx->os, comp4);
                        break;
                }
                if (cov > 0)
                    xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons(cov));
                xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons(xpos + j));
                xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons(ypos + i));
                xpost_stack_push(ctx->lo, ctx->os, devdic);
                if (cov > 0)
                    xpost_stack_push(ctx->lo, ctx->es, ts->blendpix);
                else if (xpost_object_get_type(putpix) == operatortype)
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

/* Emit one glyph's outline into the pdfwrite device's content
   accumulator as filled path segments: "r g b rg", the contours as
   m/l/c/h operators, and a nonzero-winding fill. Coordinates arrive
   from the face in y-up pixels relative to the pen and are placed at
   the y-down device pen position, exactly where the bitmap path puts
   the rendered glyph. */
typedef struct glyphfrag
{
    char *d;
    size_t len, cap;
    double px, py;
    int has;   /* any contour emitted */
    int oom;
    int svg;   /* emit SVG path commands instead of PDF operators */
} glyphfrag;

static int _frag_put(glyphfrag *f, const char *s, size_t n)
{
    if (f->len + n > f->cap)
    {
        size_t nc = f->cap ? f->cap * 2 : 256;
        char *nd;
        while (nc < f->len + n)
            nc *= 2;
        nd = realloc(f->d, nc);
        if (!nd)
        {
            f->oom = 1;
            return 1;
        }
        f->d = nd;
        f->cap = nc;
    }
    memcpy(f->d + f->len, s, n);
    f->len += n;
    return 0;
}

static int _frag_xy(glyphfrag *f, double x, double y)
{
    char t[64];
    int n;

    n = xpost_dev_pdf_fmt_num(t, f->px + x);
    t[n++] = ' ';
    n += xpost_dev_pdf_fmt_num(t + n, f->py - y);
    t[n++] = ' ';
    return _frag_put(f, t, n);
}

/* PDF and SVG spell the same commands differently: PDF postfixes the
   operator ("x y m"), SVG prefixes it ("M x y"). */
static int _frag_cmd_xy(glyphfrag *f, const char *pdfop, const char *svgop, double x, double y)
{
    if (f->svg)
        return _frag_put(f, svgop, 1) || _frag_xy(f, x, y);
    return _frag_xy(f, x, y) || _frag_put(f, pdfop, 2);
}

static int _frag_moveto(void *user, double x, double y)
{
    glyphfrag *f = user;
    f->has = 1;
    return _frag_cmd_xy(f, "m\n", "M", x, y);
}

static int _frag_lineto(void *user, double x, double y)
{
    glyphfrag *f = user;
    return _frag_cmd_xy(f, "l\n", "L", x, y);
}

static int _frag_curveto(void *user, double x1, double y1, double x2, double y2, double x3, double y3)
{
    glyphfrag *f = user;
    if (f->svg)
        return _frag_put(f, "C", 1)
            || _frag_xy(f, x1, y1) || _frag_xy(f, x2, y2) || _frag_xy(f, x3, y3);
    return _frag_xy(f, x1, y1) || _frag_xy(f, x2, y2) || _frag_xy(f, x3, y3)
        || _frag_put(f, "c\n", 2);
}

static int _frag_closepath(void *user)
{
    glyphfrag *f = user;
    if (f->svg)
        return _frag_put(f, "Z", 1);
    return _frag_put(f, "h\n", 2);
}

#define COMPVAL(o) (xpost_object_get_type(o) == realtype ? (o).real_.val \
                                                         : (double)(o).int_.val)

static
int _show_char_outline(Xpost_Context *ctx,
                       Xpost_Object devdic,
                       const textstate *ts,
                       void *face,
                       unsigned int glyph_index,
                       real xpos,
                       real ypos,
                       int ncomp,
                       Xpost_Object comp1,
                       Xpost_Object comp2,
                       Xpost_Object comp3,
                       Xpost_Object comp4,
                       long *advance_x,
                       long *advance_y)
{
    glyphfrag f;
    Xpost_Font_Outline_Sink sink;
    double r, g, b;
    char t[96];
    int n;

    memset(&f, 0, sizeof f);
    f.px = xpos;
    f.py = ypos;

    /* the target syntax is the device's choice: PDF operators unless the
       device declares /VectorSyntax /svg */
    {
        Xpost_Object syn = xpost_dict_get(ctx, devdic, xpost_name_cons(ctx, "VectorSyntax"));
        if (xpost_object_get_type(syn) == nametype)
        {
            Xpost_Object ss = xpost_name_get_string(ctx, syn);
            f.svg = ss.comp_.sz == 3
                 && memcmp(xpost_string_get_pointer(ctx, ss), "svg", 3) == 0;
        }
    }

    r = COMPVAL(comp1);
    g = ncomp >= 3 ? COMPVAL(comp2) : r;
    b = ncomp >= 3 ? COMPVAL(comp3) : r;
    if (ts->sepindex >= 0 && !f.svg)
    {
        /* the fill colour is a separation registered with the device:
           paint in its /CS<i> resource space at the recorded tint */
        memcpy(t, "/CS", 3); n = 3;
        n += xpost_dev_pdf_fmt_num(t + n, (double)ts->sepindex);
        memcpy(t + n, " cs ", 4); n += 4;
        n += xpost_dev_pdf_fmt_num(t + n, ts->septint);
        memcpy(t + n, " scn\n", 5); n += 5;
    }
    else if (ncomp == 4 && !f.svg)
    {
        /* the device's process model is CMYK: the glyph fills in it */
        n = xpost_dev_pdf_fmt_num(t, r);
        t[n++] = ' ';
        n += xpost_dev_pdf_fmt_num(t + n, g);
        t[n++] = ' ';
        n += xpost_dev_pdf_fmt_num(t + n, b);
        t[n++] = ' ';
        n += xpost_dev_pdf_fmt_num(t + n, COMPVAL(comp4));
        memcpy(t + n, " k\n", 3);
        n += 3;
    }
    else if (f.svg)
    {
        memcpy(t, "<path fill=\"rgb(", 16); n = 16;
        n += xpost_dev_pdf_fmt_num(t + n, r * 100); t[n++] = '%'; t[n++] = ',';
        n += xpost_dev_pdf_fmt_num(t + n, g * 100); t[n++] = '%'; t[n++] = ',';
        n += xpost_dev_pdf_fmt_num(t + n, b * 100); t[n++] = '%';
        memcpy(t + n, ")\" d=\"", 6); n += 6;
    }
    else
    {
        n = xpost_dev_pdf_fmt_num(t, r);
        t[n++] = ' ';
        n += xpost_dev_pdf_fmt_num(t + n, g);
        t[n++] = ' ';
        n += xpost_dev_pdf_fmt_num(t + n, b);
        memcpy(t + n, " rg\n", 4);
        n += 4;
    }
    _frag_put(&f, t, n);

    sink.moveto = _frag_moveto;
    sink.lineto = _frag_lineto;
    sink.curveto = _frag_curveto;
    sink.closepath = _frag_closepath;
    sink.user = &f;
    if (!xpost_font_face_glyph_outline(face, glyph_index, &sink, advance_x, advance_y))
    {
        free(f.d);
        return 0;
    }
    /* a blank glyph (e.g. space) decomposes to nothing: advance only */
    if (f.has && !f.oom)
    {
        if (f.svg)
            _frag_put(&f, "\"/>\n", 4);   /* glyphs fill nonzero: SVG's default rule */
        else
            _frag_put(&f, "f\n", 2);
        if (!f.oom)
            xpost_dev_pdf_append(ctx, devdic, f.d, f.len);
    }
    free(f.d);
    return 1;
}
#endif

static
int _show_char(Xpost_Context *ctx,
               Xpost_Object devdic,
               Xpost_Object putpix,
               struct fontdata data,
               const textstate *ts,
               real *xpos,
               real *ypos,
               unsigned int ch,
               unsigned int *glyph_previous,
               int ncomp,
               Xpost_Object comp1,
               Xpost_Object comp2,
               Xpost_Object comp3,
               Xpost_Object comp4)
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
    long bx0, by0, bx1, by1;

    /* show does not kern: pair adjustment in PostScript is the
       program's business (kshow, ashow), and the reference
       interpreter advances by the glyph widths alone */
    glyph_index = _glyph_index_for_char(ctx, ts->encoding, data.face, ch);
    if (ts->vector)
    {
        if (!_show_char_outline(ctx, devdic, ts, data.face, glyph_index,
                                *xpos, *ypos, ncomp, comp1, comp2, comp3, comp4,
                                &advance_x, &advance_y))
            return 0;
    }
    else if (ts->extents
             && xpost_font_face_glyph_extents(data.face, glyph_index,
                                              &bx0, &by0, &bx1, &by1,
                                              &advance_x, &advance_y))
    {
        /* an extent-tracking device needs no glyph rasterization (whose
           cost grows with the square of the resolution): the glyph
           contributes its ink box through the device's FillRect. The
           box is 26.6 glyph space, y-up around the pen; the device is
           y-down. An empty box (a space) advances only. A glyph with
           no outline takes the rendering path below instead. */
        if (bx1 > bx0 && by1 > by0)
        {
            switch (ncomp)
            {
                case 4:
                    xpost_stack_push(ctx->lo, ctx->os, comp1);
                    xpost_stack_push(ctx->lo, ctx->os, comp2);
                    xpost_stack_push(ctx->lo, ctx->os, comp3);
                    xpost_stack_push(ctx->lo, ctx->os, comp4);
                    break;
                case 3:
                    xpost_stack_push(ctx->lo, ctx->os, comp1);
                    xpost_stack_push(ctx->lo, ctx->os, comp2);
                    xpost_stack_push(ctx->lo, ctx->os, comp3);
                    break;
                default:
                    xpost_stack_push(ctx->lo, ctx->os, comp1);
                    break;
            }
            xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons((real)(*xpos + bx0 / 64.0)));
            xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons((real)(*ypos - by1 / 64.0)));
            xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons((real)((bx1 - bx0) / 64.0)));
            xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons((real)((by1 - by0) / 64.0)));
            xpost_stack_push(ctx->lo, ctx->os, devdic);
            if (xpost_object_get_type(ts->fillrect) == operatortype)
                xpost_stack_push(ctx->lo, ctx->es, ts->fillrect);
            else
            {
                xpost_stack_push(ctx->lo, ctx->os, ts->fillrect);
                xpost_stack_push(ctx->lo, ctx->es, xpost_name_cons(ctx, "exec"));
            }
        }
    }
    else
    {
        if (!xpost_font_face_glyph_render(data.face, glyph_index))
            return 0;
        xpost_font_face_glyph_buffer_get(data.face,
                                         &buffer, &rows, &width, &pitch, &pixel_mode,
                                         &left, &top, &advance_x, &advance_y);
        _draw_bitmap(ctx, devdic, putpix, ts,
                     buffer, rows, width, pitch, pixel_mode,
                     *xpos + left, *ypos - top,
                     ncomp, comp1, comp2, comp3, comp4);
    }
    /* the face transform leaves the advance in y-up glyph space; the
       pen advances in y-down device space, keeping the fractional part
       (truncating each glyph's advance drifts the line's length) */
    *xpos += (real)advance_x / 64;
    *ypos -= (real)advance_y / 64;
    *glyph_previous = glyph_index;
#else
    (void)ctx;
    (void)devdic;
    (void)putpix;
    (void)data;
    (void)ts;
    (void)xpos;
    (void)ypos;
    (void)ch;
    (void)glyph_previous;
    (void)ncomp;
    (void)comp1;
    (void)comp2;
    (void)comp3;
    (void)comp4;
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
    char *p;
    unsigned int used, last;
    real co[6];
    int n;

    /* get the current pen position from the packed path string
       (device coordinates; layout as described in xpost_op_path.c) */
    path = xpost_dict_get(ctx, gs, xpost_name_cons(ctx, "currpath"));
    if (xpost_object_get_type(path) != stringtype)
        return nocurrentpoint;
    p = xpost_string_get_pointer(ctx, path);
    memcpy(&used, p, sizeof used);
    if (used <= 16)
        return nocurrentpoint;
    memcpy(&last, p + 8, sizeof last);
    n = p[last] == 2 ? 6 : 2; /* curve carries three points */
    memcpy(co, p + last + 1, n * sizeof(real));
    *xpos = co[n - 2];
    *ypos = co[n - 1];
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
    textstate ts;
    int ncomp;
    Xpost_Object comp[4];
    Xpost_Object finalize;
    int ret;

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
    ts = _text_state_get(ctx, fontdict, devdic, gs);

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
    _face_transform_from_ctm(ctx, gs, data.face);
    XPOST_LOG_INFO("loaded font data from dict");

    /* get a c-style nul-terminated string */
    cstr = xpost_string_allocate_cstring(ctx, str);
    XPOST_LOG_INFO("append nul to string");

    ret = _get_current_point(ctx, gs, &xpos, &ypos);
    if (ret){
        free(cstr);
        return ret;
    }

    if (_device_color(ctx, gs, devdic, &ncomp, comp))
    {
        free(cstr);
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
    glyph_previous = 0;
    for (ch = cstr; *ch; ch++) {
        _show_char(ctx, devdic, putpix, data, &ts, &xpos, &ypos, (unsigned char)*ch, &glyph_previous,
                ncomp, comp[0], comp[1], comp[2], comp[3]);
    }

    /* update current position in the graphics state */
    xpost_array_put(ctx, finalize, 0, xpost_real_cons(xpos));
    xpost_array_put(ctx, finalize, 1, xpost_real_cons(ypos));

    free(cstr);
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
    textstate ts;
    int ncomp;
    Xpost_Object comp[4];
    Xpost_Object finalize;
    int ret;

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
    ts = _text_state_get(ctx, fontdict, devdic, gs);

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
    _face_transform_from_ctm(ctx, gs, data.face);
    XPOST_LOG_INFO("loaded font data from dict");

    /* get a c-style nul-terminated string */
    cstr = xpost_string_allocate_cstring(ctx, str);
    XPOST_LOG_INFO("append nul to string");

    ret = _get_current_point(ctx, gs, &xpos, &ypos);
    if (ret){
        free(cstr);
        return ret;
    }

    if (_device_color(ctx, gs, devdic, &ncomp, comp))
    {
        free(cstr);
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
    glyph_previous = 0;
    for (ch = cstr; *ch; ch++)
    {
        _show_char(ctx, devdic, putpix, data, &ts, &xpos, &ypos, (unsigned char)*ch, &glyph_previous,
                   ncomp, comp[0], comp[1], comp[2], comp[3]);
        xpos += dx.real_.val;
        ypos += dy.real_.val;
    }

    /* update current position in the graphics state */
    xpost_array_put(ctx, finalize, 0, xpost_real_cons(xpos));
    xpost_array_put(ctx, finalize, 1, xpost_real_cons(ypos));

    free(cstr);
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
    textstate ts;
    int ncomp;
    Xpost_Object comp[4];
    Xpost_Object finalize;
    int ret;

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
    ts = _text_state_get(ctx, fontdict, devdic, gs);

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
    _face_transform_from_ctm(ctx, gs, data.face);
    XPOST_LOG_INFO("loaded font data from dict");

    /* get a c-style nul-terminated string */
    cstr = xpost_string_allocate_cstring(ctx, str);
    XPOST_LOG_INFO("append nul to string");

    ret = _get_current_point(ctx, gs, &xpos, &ypos);
    if (ret){
        free(cstr);
        return ret;
    }

    if (_device_color(ctx, gs, devdic, &ncomp, comp))
    {
        free(cstr);
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
    glyph_previous = 0;
    for (ch = cstr; *ch; ch++)
    {
        _show_char(ctx, devdic, putpix, data, &ts, &xpos, &ypos, (unsigned char)*ch, &glyph_previous,
                   ncomp, comp[0], comp[1], comp[2], comp[3]);
        if (*ch == charcode.int_.val)
        {
            xpos += cx.real_.val;
            ypos += cy.real_.val;
        }
    }

    /* update current position in the graphics state */
    xpost_array_put(ctx, finalize, 0, xpost_real_cons(xpos));
    xpost_array_put(ctx, finalize, 1, xpost_real_cons(ypos));

    free(cstr);
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
    textstate ts;
    int ncomp;
    Xpost_Object comp[4];
    Xpost_Object finalize;
    int ret;

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
    ts = _text_state_get(ctx, fontdict, devdic, gs);

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
    _face_transform_from_ctm(ctx, gs, data.face);
    XPOST_LOG_INFO("loaded font data from dict");

    /* get a c-style nul-terminated string */
    cstr = xpost_string_allocate_cstring(ctx, str);
    XPOST_LOG_INFO("append nul to string");

    ret = _get_current_point(ctx, gs, &xpos, &ypos);
    if (ret){
        free(cstr);
        return ret;
    }

    if (_device_color(ctx, gs, devdic, &ncomp, comp))
    {
        free(cstr);
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
    glyph_previous = 0;
    for (ch = cstr; *ch; ch++)
    {
        _show_char(ctx, devdic, putpix, data, &ts, &xpos, &ypos, (unsigned char)*ch, &glyph_previous,
                ncomp, comp[0], comp[1], comp[2], comp[3]);
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

    free(cstr);
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
    Xpost_Object encoding;

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
    _face_transform_from_ctm(ctx, gs, data.face);
    encoding = xpost_dict_get(ctx, fontdict, xpost_name_cons(ctx, "Encoding"));
    XPOST_LOG_INFO("loaded font data from dict");

    /* get a c-style nul-terminated string */
    cstr = xpost_string_allocate_cstring(ctx, str);
    XPOST_LOG_INFO("append nul to string");

    /* accumulate the advances without rendering: the outline metrics
       carry the advance; a glyph with no outline (a bitmap strike)
       renders as a fallback */
    for (ch = cstr; *ch; ch++)
    {
#ifdef HAVE_FREETYPE2
        unsigned int glyph_index;
        long bx0, by0, bx1, by1;
        long advance_x;
        long advance_y;

        glyph_index = _glyph_index_for_char(ctx, encoding, data.face, (unsigned char)*ch);
        if (!xpost_font_face_glyph_extents(data.face, glyph_index,
                                           &bx0, &by0, &bx1, &by1,
                                           &advance_x, &advance_y))
        {
            unsigned char *buffer;
            int rows, width, pitch, left, top;
            char pixel_mode;

            if (!xpost_font_face_glyph_render(data.face, glyph_index))
            {
                free(cstr);
                return unregistered;
            }
            xpost_font_face_glyph_buffer_get(data.face, &buffer, &rows, &width,
                                             &pitch, &pixel_mode, &left, &top,
                                             &advance_x, &advance_y);
        }
        xpos += (real)advance_x / 64;
        ypos += (real)advance_y / 64;
#endif

    }

    /* the advances accumulate in the face's y-up glyph space, sized and
       oriented through the CTM; stringwidth must report the distance in
       user space, so flip to the device's y-down convention and map back
       through the inverse of the CTM's linear part */
    ypos = -ypos;
    {
        Xpost_Object psmat = xpost_dict_get(ctx, gs, xpost_name_cons(ctx, "currmatrix"));
        if (xpost_object_get_type(psmat) == arraytype && psmat.comp_.sz == 6)
        {
            real m[4], det;
            int i;
            for (i = 0; i < 4; i++)
            {
                Xpost_Object el = xpost_array_get(ctx, psmat, i);
                m[i] = xpost_object_get_type(el) == realtype ? el.real_.val
                     : (real)el.int_.val;
            }
            det = m[0] * m[3] - m[1] * m[2];
            if (det != 0)
            {
                real ux = (m[3] * xpos - m[2] * ypos) / det;
                real uy = (-m[1] * xpos + m[0] * ypos) / det;
                xpos = ux;
                ypos = uy;
            }
        }
    }

    xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons(xpos));
    xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons(ypos));

    free(cstr);
    return 0;
}

/* str  .stringoutline  array
   the string's glyph outlines as a flat array of path segments in the
   face's y-up glyph space (device-magnitude pixels, oriented by the
   face transform), relative to the pen start: coordinates followed by
   a tag, /m /l /c (cubic) or /h. charpath (in font.ps) maps each
   point to user space about the current point and appends it to the
   current path. Blank glyphs contribute advance only. */
typedef struct outlinecollect
{
    Xpost_Context *ctx;
    Xpost_Object *objs;
    size_t len, cap;
    double px, py;
    int err;
    Xpost_Object nm, nl, nc, nh;
} outlinecollect;

static
int _oc_push(outlinecollect *oc, Xpost_Object o)
{
    if (oc->len == oc->cap)
    {
        Xpost_Object *tmp;
        size_t ncap = oc->cap ? oc->cap * 2 : 256;

        tmp = realloc(oc->objs, ncap * sizeof *tmp);
        if (!tmp)
        {
            oc->err = VMerror;
            return 1;
        }
        oc->objs = tmp;
        oc->cap = ncap;
    }
    oc->objs[oc->len++] = o;
    return 0;
}

static
int _oc_xy(outlinecollect *oc, double x, double y)
{
    return _oc_push(oc, xpost_real_cons((real)(oc->px + x)))
        || _oc_push(oc, xpost_real_cons((real)(oc->py + y)));
}

static
int _oc_moveto(void *user, double x, double y)
{
    outlinecollect *oc = user;
    return _oc_xy(oc, x, y) || _oc_push(oc, oc->nm);
}

static
int _oc_lineto(void *user, double x, double y)
{
    outlinecollect *oc = user;
    return _oc_xy(oc, x, y) || _oc_push(oc, oc->nl);
}

static
int _oc_curveto(void *user, double x1, double y1, double x2, double y2, double x3, double y3)
{
    outlinecollect *oc = user;
    return _oc_xy(oc, x1, y1) || _oc_xy(oc, x2, y2) || _oc_xy(oc, x3, y3)
        || _oc_push(oc, oc->nc);
}

static
int _oc_closepath(void *user)
{
    outlinecollect *oc = user;
    return _oc_push(oc, oc->nh);
}

static
int _stringoutline(Xpost_Context *ctx,
                   Xpost_Object str)
{
    Xpost_Object userdict;
    Xpost_Object gd;
    Xpost_Object gs;
    Xpost_Object fontdict;
    Xpost_Object privatestr;
    struct fontdata data;
    Xpost_Object encoding;
    char *cstr;
    char *ch;
    outlinecollect oc;
    Xpost_Object arr;
    size_t i;

    userdict = xpost_stack_bottomup_fetch(ctx->lo, ctx->ds, 2);
    if (xpost_object_get_type(userdict) != dicttype)
        return dictstackunderflow;
    gd = xpost_dict_get(ctx, userdict, xpost_name_cons(ctx, "graphicsdict"));
    gs = xpost_dict_get(ctx, gd, xpost_name_cons(ctx, "currgstate"));
    fontdict = xpost_dict_get(ctx, gs, xpost_name_cons(ctx, "currfont"));
    if (xpost_object_get_type(fontdict) == invalidtype)
        return invalidfont;

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
    _face_transform_from_ctm(ctx, gs, data.face);
    encoding = xpost_dict_get(ctx, fontdict, xpost_name_cons(ctx, "Encoding"));

    cstr = xpost_string_allocate_cstring(ctx, str);
    if (!cstr)
        return VMerror;

    memset(&oc, 0, sizeof oc);
    oc.ctx = ctx;
    oc.nm = xpost_object_cvlit(xpost_name_cons(ctx, "m"));
    oc.nl = xpost_object_cvlit(xpost_name_cons(ctx, "l"));
    oc.nc = xpost_object_cvlit(xpost_name_cons(ctx, "c"));
    oc.nh = xpost_object_cvlit(xpost_name_cons(ctx, "h"));

    for (ch = cstr; *ch; ch++)
    {
#ifdef HAVE_FREETYPE2
        unsigned int glyph_index;
        long advance_x, advance_y;
        Xpost_Font_Outline_Sink sink;

        glyph_index = _glyph_index_for_char(ctx, encoding, data.face, (unsigned char)*ch);
        sink.moveto = _oc_moveto;
        sink.lineto = _oc_lineto;
        sink.curveto = _oc_curveto;
        sink.closepath = _oc_closepath;
        sink.user = &oc;
        if (!xpost_font_face_glyph_outline(data.face, glyph_index, &sink, &advance_x, &advance_y))
        {
            /* a glyph without an outline leaves no path; skip it */
            free(oc.objs);
            free(cstr);
            return invalidfont;
        }
        if (oc.err)
        {
            free(oc.objs);
            free(cstr);
            return oc.err;
        }
        oc.px += (double)advance_x / 64;
        oc.py += (double)advance_y / 64;
#endif
    }
    free(cstr);

    if (oc.len > 65535)
    {
        free(oc.objs);
        return limitcheck;
    }
    arr = xpost_object_cvlit(xpost_array_cons(ctx, (unsigned int)oc.len));
    if (xpost_object_get_type(arr) == nulltype)
    {
        free(oc.objs);
        return VMerror;
    }
    for (i = 0; i < oc.len; i++)
        xpost_array_put(ctx, arr, (integer)i, oc.objs[i]);
    free(oc.objs);
    xpost_stack_push(ctx->lo, ctx->os, arr);
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
    textstate ts;
    int ncomp;
    Xpost_Object comp[4];
    Xpost_Object finalize;
    int ret;

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
    ts = _text_state_get(ctx, fontdict, devdic, gs);

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
    _face_transform_from_ctm(ctx, gs, data.face);
    XPOST_LOG_INFO("loaded font data from dict");

    /* get a c-style nul-terminated string */
    cstr = xpost_string_allocate_cstring(ctx, str);
    XPOST_LOG_INFO("append nul to string");

    ret = _get_current_point(ctx, gs, &xpos, &ypos);
    if (ret){
        free(cstr);
        return ret;
    }

    if (_device_color(ctx, gs, devdic, &ncomp, comp))
    {
        free(cstr);
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
    glyph_previous = 0;
    for (ch = cstr; *ch; ch++)
    {
        _show_char(ctx, devdic, putpix, data, &ts, &xpos, &ypos, (unsigned char)*ch, &glyph_previous,
                ncomp, comp[0], comp[1], comp[2], comp[3]);
    }

    /* update current position in the graphics state */
    xpost_array_put(ctx, finalize, 0, xpost_real_cons(xpos));
    xpost_array_put(ctx, finalize, 1, xpost_real_cons(ypos));

    free(cstr);
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
    op = xpost_operator_cons(ctx, ".loadfont42", (Xpost_Op_Func)_loadfont42, 0, 1, dicttype);
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
    op = xpost_operator_cons(ctx, ".stringoutline", (Xpost_Op_Func)_stringoutline, 1, 1, stringtype);
    INSTALL;
    op = xpost_operator_cons(ctx, "kshow", (Xpost_Op_Func)_kshow, 0, 2, proctype, stringtype);
    INSTALL;

    /* xpost_dict_dump_memory (ctx->gl, sd); fflush(NULL);
    xpost_dict_put(ctx, sd, xpost_name_cons(ctx, "mark"), mark); */

    return 0;
}
