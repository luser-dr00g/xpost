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

#include <stdarg.h>
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
    Xpost_Object charstrings; /* the font's /CharStrings dict, or invalid */
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
        static struct { char *name; void *face; Xpost_Object charstrings; } face_cache[32];
        static int face_cache_n = 0;
        int fi, slot = -1;
        data.face = NULL;
        data.program = NULL;
        for (fi = 0; fi < face_cache_n; fi++)
        {
            if (strcmp(face_cache[fi].name, fname) == 0)
            {
                data.face = face_cache[fi].face;
                slot = fi;
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
                slot = face_cache_n++;
            }
        }
        if (data.face == NULL){
            free(fname);
            return invalidfont;
        }

        /* a base font publishes its glyph complement: programs size
           tables from /CharStrings, test membership with known, and
           re-encode from its keys. Synthesize the name-to-glyph-index
           dictionary from the face's glyph names, once per face and
           shared read-only between every dictionary the name produces,
           in global VM so a restore cannot unwind it from under the
           cache. The values are glyph indices, which the text
           machinery accepts directly; a face without glyph names
           publishes nothing. */
        if (slot >= 0
         && xpost_object_get_type(face_cache[slot].charstrings) == dicttype)
        {
            xpost_dict_put(ctx, fontdict, xpost_name_cons(ctx, "CharStrings"),
                           face_cache[slot].charstrings);
        }
        else
        {
            unsigned int nglyphs = xpost_font_face_glyph_name_count(data.face);
            if (nglyphs)
            {
                Xpost_Object csdict;
                char nbuf[128];
                unsigned int gi;
                unsigned int oldmode = ctx->vmmode;
                ctx->vmmode = GLOBAL;
                csdict = xpost_dict_cons(ctx, nglyphs);
                for (gi = 0; gi < nglyphs; gi++)
                    if (xpost_font_face_glyph_name_get(data.face, gi, nbuf, sizeof nbuf))
                        xpost_dict_put(ctx, csdict,
                                       xpost_name_cons(ctx, nbuf),
                                       xpost_int_cons((integer)gi));
                ctx->vmmode = oldmode;
                csdict = xpost_object_set_access(ctx, csdict,
                                                 XPOST_OBJECT_TAG_ACCESS_READ_ONLY);
                if (slot >= 0)
                    face_cache[slot].charstrings = csdict;
                xpost_dict_put(ctx, fontdict, xpost_name_cons(ctx, "CharStrings"),
                               csdict);
            }
        }
    }

    fontbbox = xpost_array_cons(ctx, 4);
    xpost_font_face_get_bbox(data.face, fontbboxarray, 1000.0);
    xpost_memory_put(xpost_context_select_memory(ctx, fontbbox),
		     xpost_object_get_ent(fontbbox),
		     0, 4 * sizeof(Xpost_Object), fontbboxarray);
    xpost_dict_put(ctx, fontdict, xpost_name_cons(ctx, "FontBBox"), fontbbox);

    /* the base font follows the Type 1 convention: character space
       holds 1000 units per em and FontMatrix maps it to one text-space
       unit, so FontBBox above is in the same 1000-unit space (programs
       read the pair together, e.g. FontBBox dtransformed through
       FontMatrix). scalefont and makefont concatenate onto this in
       dictionary copies, and the text operators derive the face's
       pixel scale from it */
    {
        Xpost_Object fontmatrix = xpost_array_cons(ctx, 6);
        int mi;
        for (mi = 0; mi < 6; mi++)
            xpost_array_put(ctx, fontmatrix, mi,
                            xpost_real_cons(mi == 0 || mi == 3 ? 0.001f : 0.0f));
        xpost_dict_put(ctx, fontdict, xpost_name_cons(ctx, "FontMatrix"), fontmatrix);
    }

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
    /* a Type 42 dictionary maps one em to one character-space unit */
    xpost_font_face_get_bbox(data.face, fontbboxarray, 1.0);
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

/* scalefont and makefont are implemented in font.ps: each returns a
   fresh dictionary with the requested transform concatenated onto the
   font's FontMatrix. No operator mutates the shared face; the text
   operators size it from FontMatrix and the CTM at use time
   (_face_setup below). */



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
    ts.charstrings = xpost_dict_get(ctx, fontdict, xpost_name_cons(ctx, "CharStrings"));
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
   glyph. A Type 42 font's /CharStrings dictionary maps glyph names to
   glyph indices and is authoritative when it holds an integer for the
   name: subset fonts strip the sfnt's own name and character-map
   tables and carry the name-to-index mapping only here. Otherwise the
   name is resolved against the face's glyph names; codes whose entry
   is not a name (the findfont wrapper fills /Encoding with nulls), or
   whose name resolves nowhere, fall back to the face's character map,
   preserving the plain-text behaviour of an unencoded font. */
static
unsigned int _glyph_index_for_char(Xpost_Context *ctx,
                                   Xpost_Object encoding,
                                   Xpost_Object charstrings,
                                   void *face,
                                   unsigned int ch)
{
    if (xpost_object_get_type(encoding) == arraytype
     && ch < (unsigned int)encoding.comp_.sz)
    {
        Xpost_Object en = xpost_array_get(ctx, encoding, ch);
        if (xpost_object_get_type(en) == nametype)
        {
            Xpost_Object str;
            char *cname;
            unsigned int gi = 0;

            if (xpost_object_get_type(charstrings) == dicttype)
            {
                Xpost_Object gid = xpost_dict_get(ctx, charstrings,
                                                  xpost_object_cvlit(en));
                if (xpost_object_get_type(gid) == integertype
                 && gid.int_.val >= 0)
                    return (unsigned int)gid.int_.val;
            }
            str = xpost_name_get_string(ctx, en);
            cname = xpost_string_allocate_cstring(ctx, str);
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

/* Map a glyph name to a glyph index without passing through a
   character code, as glyphshow selects glyphs: the CharStrings
   dictionary decides when it holds an integer for the name, then the
   face's own glyph names; an unknown name selects the notdef glyph,
   there being no code to fall back to the character map with. */
static
unsigned int _glyph_index_for_name(Xpost_Context *ctx,
                                   Xpost_Object charstrings,
                                   void *face,
                                   Xpost_Object gname)
{
    Xpost_Object str;
    char *cname;
    unsigned int gi = 0;

    if (xpost_object_get_type(charstrings) == dicttype)
    {
        Xpost_Object gid = xpost_dict_get(ctx, charstrings,
                                          xpost_object_cvlit(gname));
        if (xpost_object_get_type(gid) == integertype
         && gid.int_.val >= 0)
            return (unsigned int)gid.int_.val;
    }
    str = xpost_name_get_string(ctx, gname);
    cname = xpost_string_allocate_cstring(ctx, str);
    if (cname)
    {
        if (strcmp(cname, ".notdef") != 0)
            gi = xpost_font_face_glyph_name_index_get(face, cname);
        free(cname);
    }
    return gi;
}

#ifdef HAVE_FREETYPE2
/* Prepare the shared face for use under the current graphics state.
   The font dictionary's FontMatrix carries the size (and any rotation,
   shear or anisotropy concatenated by makefont); the CTM carries the
   device mapping. Neither is sticky on the font: scalefont and
   makefont only build dictionaries, so two sizes of one face coexist
   and the CTM matters when the glyphs are marked, not when the font
   was scaled. Compose the two linear parts, split the result into a
   pixel-per-em scale for the face and a unit-magnitude transform
   (conjugated by the y flip that relates FreeType's y-up glyph space
   to the device's y-down raster), and install both. The face is
   shared through the findfont cache, so every text operator must call
   this before touching glyphs. A missing or malformed FontMatrix
   reads as the identity, serving font programs defined without one. */
static
void _face_setup(Xpost_Context *ctx,
                 Xpost_Object gs,
                 Xpost_Object fontdict,
                 void *face)
{
    Xpost_Object psmat;
    real fm[4] = { 1.0, 0.0, 0.0, 1.0 };
    real cm[4];
    real e[4];
    real q;
    real r;
    float mat[6] = { 0 };
    int i;

    psmat = xpost_dict_get(ctx, fontdict, xpost_name_cons(ctx, "FontMatrix"));
    if (xpost_object_get_type(psmat) == arraytype && psmat.comp_.sz == 6)
    {
        for (i = 0; i < 4; i++)
        {
            Xpost_Object el = xpost_array_get(ctx, psmat, i);
            if (xpost_object_get_type(el) == realtype)
                fm[i] = el.real_.val;
            else if (xpost_object_get_type(el) == integertype)
                fm[i] = (real)el.int_.val;
        }
    }

    psmat = xpost_dict_get(ctx, gs, xpost_name_cons(ctx, "currmatrix"));
    if (xpost_object_get_type(psmat) != arraytype || psmat.comp_.sz != 6)
        return;
    for (i = 0; i < 4; i++)
    {
        Xpost_Object el = xpost_array_get(ctx, psmat, i);
        cm[i] = xpost_object_get_type(el) == realtype ? el.real_.val
             : (real)el.int_.val;
    }

    /* text space -> device space: FontMatrix then CTM
       (row convention: x' = a x + c y, y' = b x + d y) */
    e[0] = fm[0] * cm[0] + fm[1] * cm[2];
    e[1] = fm[0] * cm[1] + fm[1] * cm[3];
    e[2] = fm[2] * cm[0] + fm[3] * cm[2];
    e[3] = fm[2] * cm[1] + fm[3] * cm[3];

    q = (real)sqrt(e[0] * e[0] + e[1] * e[1]);
    if (q == 0)
        q = (real)sqrt(e[2] * e[2] + e[3] * e[3]);
    if (q == 0)
        return;

    /* the em in pixels: FontMatrix maps character space to text space,
       so the composed magnitude q is per character-space unit, and the
       units per em are a convention of the font type (1000 for Type 1
       dictionaries, whose FontMatrix carries the 0.001 factor; one for
       Type 42, whose FontMatrix is an identity over the em) */
    {
        Xpost_Object ft = xpost_dict_get(ctx, fontdict,
                                         xpost_name_cons(ctx, "FontType"));
        Xpost_Object cft = xpost_dict_get(ctx, fontdict,
                                          xpost_name_cons(ctx, "CIDFontType"));
        real qem = q;
        if ((xpost_object_get_type(ft) == integertype && ft.int_.val == 1)
         || (xpost_object_get_type(cft) == integertype && cft.int_.val == 0))
        {
            /* a Type 1 character-space unit is usually a thousandth
               of the em -- the convention findfont dictionaries
               declare whatever their face's native units -- but an
               embedded program keeps its design count (a converted
               2048-unit font arrives with a 1/2048 matrix), recorded
               in the dictionary when its face was assembled */
            Xpost_Object emu = xpost_dict_get(ctx, fontdict,
                                              xpost_name_cons(ctx, ".emunits"));
            int units = xpost_object_get_type(emu) == integertype
                      ? emu.int_.val : 1000;

            qem = q * (units > 0 ? units : 1000);
        }

        /* the face serves a well-conditioned base size (an extreme em
           would fail inside FreeType); the residual ratio to the true
           size rides in the transform, which scales outlines, extents
           and linear advances alike */
        r = qem / xpost_font_face_scale(face, qem);
    }

    mat[0] = (float)( e[0] / q * r);   /* xx */
    mat[1] = (float)( e[2] / q * r);   /* xy */
    mat[2] = (float)(-e[1] / q * r);   /* yx */
    mat[3] = (float)(-e[3] / q * r);   /* yy */
    xpost_font_face_transform(face, mat);
}

/* Resolve the current colour into the device's native space, applying
   the same source-to-destination conversions as the ColorConversion
   table in color.ps (gray by NTSC luminosity, CMYK composed by
   additive complement, RGB to CMYK with full black generation and
   undercolor removal), so glyphs mark in exactly the colour a fill
   under the same graphics state would. A device with the /Process
   native space takes each paint in the space it was set in, so the
   source components pass through unconverted. A source space the
   table does not know passes its raw components through. Returns 0 on
   success. */
static
int _device_color(Xpost_Context *ctx,
                  Xpost_Object gs,
                  Xpost_Object devdic,
                  int *ncomp,
                  Xpost_Object comp[4])
{
#define GSCOMP(name) (o = xpost_dict_get(ctx, gs, xpost_name_cons(ctx, name)), \
                      xpost_object_get_type(o) == realtype ? o.real_.val \
                    : xpost_object_get_type(o) == integertype ? (double)o.int_.val \
                    : 0.0)
#define MIN1(x) ((x) < 1.0 ? (x) : 1.0)
    Xpost_Object colorspace, srcspace, o;
    enum { SRC_GRAY, SRC_RGB, SRC_CMYK, SRC_OTHER } src;
    double v[4];

    srcspace = xpost_dict_get(ctx, gs, xpost_name_cons(ctx, "colorspace"));
    if (xpost_dict_compare_objects(ctx, srcspace, xpost_name_cons(ctx, "DeviceGray")) == 0)
        src = SRC_GRAY;
    else if (xpost_dict_compare_objects(ctx, srcspace, xpost_name_cons(ctx, "DeviceRGB")) == 0)
        src = SRC_RGB;
    else if (xpost_dict_compare_objects(ctx, srcspace, xpost_name_cons(ctx, "DeviceCMYK")) == 0)
        src = SRC_CMYK;
    else
        src = SRC_OTHER;
    v[0] = GSCOMP("colorcomp1");
    v[1] = GSCOMP("colorcomp2");
    v[2] = GSCOMP("colorcomp3");
    v[3] = GSCOMP("colorcomp4");

    colorspace = xpost_dict_get(ctx, devdic, xpost_name_cons(ctx, "nativecolorspace"));
    if (xpost_dict_compare_objects(ctx, colorspace, xpost_name_cons(ctx, "DeviceGray")) == 0)
    {
        double g;

        switch (src)
        {
            case SRC_RGB:
                g = 0.3 * v[0] + 0.59 * v[1] + 0.11 * v[2];
                break;
            case SRC_CMYK:
                g = 1.0 - MIN1(0.3 * v[0] + 0.59 * v[1] + 0.11 * v[2] + v[3]);
                break;
            default: /* gray, or an unknown space's first component */
                g = v[0];
                break;
        }
        *ncomp = 1;
        comp[0] = xpost_real_cons((real)g);
    }
    else if (xpost_dict_compare_objects(ctx, colorspace, xpost_name_cons(ctx, "DeviceRGB")) == 0)
    {
        double r, g, b;

        switch (src)
        {
            case SRC_GRAY:
                r = g = b = v[0];
                break;
            case SRC_CMYK:
                r = 1.0 - MIN1(v[0] + v[3]);
                g = 1.0 - MIN1(v[1] + v[3]);
                b = 1.0 - MIN1(v[2] + v[3]);
                break;
            default:
                r = v[0]; g = v[1]; b = v[2];
                break;
        }
        *ncomp = 3;
        comp[0] = xpost_real_cons((real)r);
        comp[1] = xpost_real_cons((real)g);
        comp[2] = xpost_real_cons((real)b);
    }
    else if (xpost_dict_compare_objects(ctx, colorspace, xpost_name_cons(ctx, "DeviceCMYK")) == 0)
    {
        double c, m, y, k;

        switch (src)
        {
            case SRC_GRAY:
                c = m = y = 0;
                k = 1.0 - v[0];
                break;
            case SRC_RGB:
                c = 1.0 - v[0];
                m = 1.0 - v[1];
                y = 1.0 - v[2];
                k = c < m ? c : m;
                if (y < k) k = y;
                c -= k; m -= k; y -= k;
                break;
            default:
                c = v[0]; m = v[1]; y = v[2]; k = v[3];
                break;
        }
        *ncomp = 4;
        comp[0] = xpost_real_cons((real)c);
        comp[1] = xpost_real_cons((real)m);
        comp[2] = xpost_real_cons((real)y);
        comp[3] = xpost_real_cons((real)k);
    }
    else if (xpost_dict_compare_objects(ctx, colorspace, xpost_name_cons(ctx, "Process")) == 0)
    {
        /* the device takes each paint in the space it was set in:
           deliver the source components unconverted */
        switch (src)
        {
            case SRC_GRAY:
                *ncomp = 1;
                comp[0] = xpost_real_cons((real)v[0]);
                break;
            case SRC_CMYK:
                *ncomp = 4;
                comp[0] = xpost_real_cons((real)v[0]);
                comp[1] = xpost_real_cons((real)v[1]);
                comp[2] = xpost_real_cons((real)v[2]);
                comp[3] = xpost_real_cons((real)v[3]);
                break;
            default: /* RGB, or an unknown space's first three components */
                *ncomp = 3;
                comp[0] = xpost_real_cons((real)v[0]);
                comp[1] = xpost_real_cons((real)v[1]);
                comp[2] = xpost_real_cons((real)v[2]);
                break;
        }
    }
    else
    {
        XPOST_LOG_ERR("unimplemented device colorspace");
        return unregistered;
    }
    return 0;
#undef GSCOMP
#undef MIN1
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
    else if (ncomp == 1 && !f.svg)
    {
        /* the paint's space is DeviceGray: the glyph fills in it */
        n = xpost_dev_pdf_fmt_num(t, r);
        memcpy(t + n, " g\n", 3);
        n += 3;
    }
    else if (ncomp == 4 && !f.svg)
    {
        /* the paint's space is DeviceCMYK: the glyph fills in it */
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
int _show_glyph(Xpost_Context *ctx,
                Xpost_Object devdic,
                Xpost_Object putpix,
                struct fontdata data,
                const textstate *ts,
                real *xpos,
                real *ypos,
                unsigned int glyph_index,
                int ncomp,
                Xpost_Object comp1,
                Xpost_Object comp2,
                Xpost_Object comp3,
                Xpost_Object comp4)
{
#ifdef HAVE_FREETYPE2
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
        /* the pen rides at fractional device positions but the glyph
           bitmap sits on the pixel grid: place it at the nearest
           pixel, not the floor, so a pen an epsilon shy of a pixel
           boundary (the linear advance's 16.16 quantization) lands
           where exact arithmetic would put it */
        _draw_bitmap(ctx, devdic, putpix, ts,
                     buffer, rows, width, pitch, pixel_mode,
                     (int)floor(*xpos + left + 0.5),
                     (int)floor(*ypos - top + 0.5),
                     ncomp, comp1, comp2, comp3, comp4);
    }
    /* the face transform leaves the advance in y-up glyph space; the
       pen advances in y-down device space, keeping the fractional part
       (truncating each glyph's advance drifts the line's length) */
    *xpos += (real)(advance_x / 65536.0);
    *ypos -= (real)(advance_y / 65536.0);
#else
    (void)ctx;
    (void)devdic;
    (void)putpix;
    (void)data;
    (void)ts;
    (void)xpos;
    (void)ypos;
    (void)glyph_index;
    (void)ncomp;
    (void)comp1;
    (void)comp2;
    (void)comp3;
    (void)comp4;
#endif
    return 1;
}

static
int _show_char(Xpost_Context *ctx,
               Xpost_Object devdic,
               Xpost_Object putpix,
               struct fontdata data,
               const textstate *ts,
               real *xpos,
               real *ypos,
               unsigned int ch,
               int ncomp,
               Xpost_Object comp1,
               Xpost_Object comp2,
               Xpost_Object comp3,
               Xpost_Object comp4)
{
    /* show does not kern: pair adjustment in PostScript is the
       program's business (kshow, ashow); the advance is the glyph
       widths alone */
    unsigned int glyph_index = _glyph_index_for_char(ctx, ts->encoding,
                                                     ts->charstrings,
                                                     data.face, ch);
    return _show_glyph(ctx, devdic, putpix, data, ts, xpos, ypos,
                       glyph_index, ncomp, comp1, comp2, comp3, comp4);
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
    /* currpath sits in a program-reachable dictionary, so its header may be
       forged; bound the extent and the last-element offset against the
       string's own allocation before dereferencing them */
    {
        Xpost_Memory_File *mem = xpost_context_select_memory(ctx, path);
        unsigned int ent = xpost_object_get_ent(path);
        unsigned int entsz = mem->table.tab[ent].sz;
        unsigned int avail = path.comp_.off < entsz ? entsz - path.comp_.off : 0;

        if (avail < 16)
            return nocurrentpoint;
        p = xpost_string_get_pointer(ctx, path);
        memcpy(&used, p, sizeof used);
        if (used <= 16 || used > avail)
            return nocurrentpoint;
        memcpy(&last, p + 8, sizeof last);
        n = last < used && p[last] == 2 ? 6 : 2; /* curve carries three points */
        if (last >= used || last + 1 + n * sizeof(real) > used)
            return nocurrentpoint;
        memcpy(co, p + last + 1, n * sizeof(real));
        *xpos = co[n - 2];
        *ypos = co[n - 1];
    }
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
    _face_setup(ctx, gs, fontdict, data.face);
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
    for (ch = cstr; *ch; ch++) {
        _show_char(ctx, devdic, putpix, data, &ts, &xpos, &ypos, (unsigned char)*ch,
                ncomp, comp[0], comp[1], comp[2], comp[3]);
    }

    /* update current position in the graphics state */
    xpost_array_put(ctx, finalize, 0, xpost_real_cons(xpos));
    xpost_array_put(ctx, finalize, 1, xpost_real_cons(ypos));

    free(cstr);
    return 0;
}

/* glyphname  .glyphshow  -
   paint the single glyph the name selects, bypassing the encoding,
   and advance the current point by the glyph's width. The
   PostScript-level glyphshow sends Type 3 fonts to their build
   procedures instead of here. */
static
int _glyphshow_common(Xpost_Context *ctx,
                      Xpost_Object gname,
                      int byname,
                      unsigned int gid)
{
    Xpost_Object userdict;
    Xpost_Object gd;
    Xpost_Object gs;
    Xpost_Object fontdict;
    Xpost_Object privatestr;
    struct fontdata data;
    real xpos, ypos;
    Xpost_Object devdic;
    Xpost_Object putpix;
    textstate ts;
    int ncomp;
    Xpost_Object comp[4];
    Xpost_Object finalize;
    unsigned int glyph_index;
    int ret;

    userdict = xpost_stack_bottomup_fetch(ctx->lo, ctx->ds, 2);
    if (xpost_object_get_type(userdict) != dicttype)
        return dictstackunderflow;
    gd = xpost_dict_get(ctx, userdict, xpost_name_cons(ctx, "graphicsdict"));
    gs = xpost_dict_get(ctx, gd, xpost_name_cons(ctx, "currgstate"));
    fontdict = xpost_dict_get(ctx, gs, xpost_name_cons(ctx, "currfont"));
    if (xpost_object_get_type(fontdict) == invalidtype)
        return invalidfont;

    devdic = xpost_dict_get(ctx, gs, xpost_name_cons(ctx, "device"));
    putpix = xpost_dict_get(ctx, devdic, xpost_name_cons(ctx, "PutPix"));
    ts = _text_state_get(ctx, fontdict, devdic, gs);

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
    _face_setup(ctx, gs, fontdict, data.face);

    ret = _get_current_point(ctx, gs, &xpos, &ypos);
    if (ret)
        return ret;

    if (_device_color(ctx, gs, devdic, &ncomp, comp))
        return unregistered;

    finalize = xpost_object_cvx(xpost_array_cons(ctx, 5));
    xpost_array_put(ctx, finalize, 0, xpost_real_cons(xpos));
    xpost_array_put(ctx, finalize, 1, xpost_real_cons(ypos));
    xpost_array_put(ctx, finalize, 2, xpost_object_cvx(xpost_name_cons(ctx, "itransform")));
    xpost_array_put(ctx, finalize, 3, xpost_object_cvx(xpost_name_cons(ctx, "moveto")));
    xpost_array_put(ctx, finalize, 4, xpost_object_cvx(xpost_name_cons(ctx, "flushpage")));
    xpost_stack_push(ctx->lo, ctx->es, finalize);

    glyph_index = byname
        ? _glyph_index_for_name(ctx, ts.charstrings, data.face, gname)
        : gid;
    _show_glyph(ctx, devdic, putpix, data, &ts, &xpos, &ypos,
                glyph_index, ncomp, comp[0], comp[1], comp[2], comp[3]);

    xpost_array_put(ctx, finalize, 0, xpost_real_cons(xpos));
    xpost_array_put(ctx, finalize, 1, xpost_real_cons(ypos));

    return 0;
}

static
int _glyphshow(Xpost_Context *ctx,
               Xpost_Object gname)
{
    return _glyphshow_common(ctx, gname, 1, 0);
}

/* index  .glyphshowidx  -
   paint the single glyph at the given index in the current font's
   face and advance the current point by its width; the composite
   font machinery reaches glyphs by index once a CMap has resolved
   the character code */
static
int _glyphshowidx(Xpost_Context *ctx,
                  Xpost_Object gidx)
{
    if (gidx.int_.val < 0)
        return rangecheck;
    return _glyphshow_common(ctx, null, 0,
                             (unsigned int)gidx.int_.val);
}

/* big-endian field readers over the assembled font program */
static unsigned int _sfnt_u16(const unsigned char *p)
{
    return p[0] << 8 | p[1];
}
static unsigned int _sfnt_u32(const unsigned char *p)
{
    return (unsigned int)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];
}
static void _sfnt_put16(unsigned char *p, unsigned int v)
{
    p[0] = (v >> 8) & 0xff; p[1] = v & 0xff;
}
static void _sfnt_put32(unsigned char *p, unsigned int v)
{
    p[0] = (v >> 24) & 0xff; p[1] = (v >> 16) & 0xff;
    p[2] = (v >> 8) & 0xff; p[3] = v & 0xff;
}


/* ciddict  .loadcidfont0  -
   assemble a working face for a CIDFontType 0 dictionary. The glyph
   programs arrived through StartData as the /GlyphData binary block
   -- the CIDMap, then Type 1 charstrings the FDArray's private
   dictionaries describe. The dictionary is written back out as a
   CIDFont resource file around that block and opened as a memory
   face, which serves glyphs directly by CID. */

static int
_cid_emit(char **buf, size_t *len, size_t *cap, const char *fmt, ...)
{
    va_list ap;
    int n;

    for (;;)
    {
        va_start(ap, fmt);
        n = vsnprintf(*buf + *len, *cap - *len, fmt, ap);
        va_end(ap);
        if (n < 0)
            return -1;
        if (*len + (size_t)n < *cap)
        {
            *len += (size_t)n;
            return 0;
        }
        {
            char *nb = realloc(*buf, *cap * 2);

            if (!nb)
                return -1;
            *buf = nb;
            *cap *= 2;
        }
    }
}

static int
_cid_emit_num(Xpost_Context *ctx, char **buf, size_t *len, size_t *cap,
              Xpost_Object v)
{
    (void)ctx;
    if (xpost_object_get_type(v) == integertype)
        return _cid_emit(buf, len, cap, "%d", v.int_.val);
    if (xpost_object_get_type(v) == realtype)
        return _cid_emit(buf, len, cap, "%g", v.real_.val);
    if (xpost_object_get_type(v) == booleantype)
        return _cid_emit(buf, len, cap, "%s", v.int_.val ? "true" : "false");
    return -1;
}

static int
_cid_emit_entry(Xpost_Context *ctx, char **buf, size_t *len, size_t *cap,
                Xpost_Object d, const char *key)
{
    Xpost_Object v = xpost_dict_get(ctx, d, xpost_name_cons(ctx, key));
    int i;

    if (xpost_object_get_type(v) == invalidtype)
        return 0;
    if (xpost_object_get_type(v) == arraytype)
    {
        if (_cid_emit(buf, len, cap, "/%s [", key)) return -1;
        for (i = 0; i < v.comp_.sz; i++)
        {
            if (_cid_emit(buf, len, cap, i ? " " : "")) return -1;
            if (_cid_emit_num(ctx, buf, len, cap, xpost_array_get(ctx, v, i)))
                return -1;
        }
        return _cid_emit(buf, len, cap, "] def\n");
    }
    if (_cid_emit(buf, len, cap, "/%s ", key)) return -1;
    if (_cid_emit_num(ctx, buf, len, cap, v)) return -1;
    return _cid_emit(buf, len, cap, " def\n");
}

static const char *_cid_private_keys[] = {
    "lenIV", "BlueValues", "OtherBlues", "FamilyBlues", "FamilyOtherBlues",
    "BlueScale", "BlueShift", "BlueFuzz", "StdHW", "StdVW",
    "StemSnapH", "StemSnapV", "LanguageGroup", "ForceBold", "RndStemUp",
    "SubrMapOffset", "SDBytes", "SubrCount",
};

static
int _loadcidfont0(Xpost_Context *ctx,
                  Xpost_Object fontdict)
{
#ifdef HAVE_FREETYPE2
    Xpost_Object gdata, fdarray, privatestr, fontbbox;
    Xpost_Object fontbboxarray[4];
    struct fontdata data;
    char *buf;
    unsigned char *whole;
    size_t len = 0, cap = 8192, glen, gpos;
    int i;
    unsigned int k;

    gdata = xpost_dict_get(ctx, fontdict, xpost_name_cons(ctx, "GlyphData"));
    if (xpost_object_get_type(gdata) == stringtype)
        glen = gdata.comp_.sz;
    else if (xpost_object_get_type(gdata) == arraytype)
    {
        glen = 0;
        for (i = 0; i < gdata.comp_.sz; i++)
        {
            Xpost_Object s = xpost_array_get(ctx, gdata, i);
            if (xpost_object_get_type(s) != stringtype)
                return invalidfont;
            glen += s.comp_.sz;
        }
    }
    else
        return invalidfont;
    fdarray = xpost_dict_get(ctx, fontdict, xpost_name_cons(ctx, "FDArray"));
    if (xpost_object_get_type(fdarray) != arraytype)
        return invalidfont;

    buf = malloc(cap);
    if (!buf)
        return VMerror;
    if (_cid_emit(&buf, &len, &cap,
        "%%!PS-Adobe-3.0 Resource-CIDFont\n"
        "%%%%DocumentNeededResources: ProcSet (CIDInit)\n"
        "%%%%IncludeResource: ProcSet (CIDInit)\n"
        "/CIDInit /ProcSet findresource begin\n"
        "20 dict begin\n"
        "/CIDFontName /X def\n"
        "/CIDFontVersion 1 def\n"
        "/CIDFontType 0 def\n"
        "/CIDSystemInfo 3 dict dup begin\n"
        "  /Registry (Adobe) def\n"
        "  /Ordering (Identity) def\n"
        "  /Supplement 0 def\n"
        "end def\n")) goto fail;
    if (_cid_emit_entry(ctx, &buf, &len, &cap, fontdict, "FontMatrix")) goto fail;
    if (_cid_emit_entry(ctx, &buf, &len, &cap, fontdict, "FontBBox")) goto fail;
    if (_cid_emit_entry(ctx, &buf, &len, &cap, fontdict, "CIDCount")) goto fail;
    if (_cid_emit_entry(ctx, &buf, &len, &cap, fontdict, "FDBytes")) goto fail;
    if (_cid_emit_entry(ctx, &buf, &len, &cap, fontdict, "GDBytes")) goto fail;
    if (_cid_emit_entry(ctx, &buf, &len, &cap, fontdict, "CIDMapOffset")) goto fail;
    if (_cid_emit(&buf, &len, &cap, "/FDArray %d array\n", fdarray.comp_.sz))
        goto fail;
    for (i = 0; i < fdarray.comp_.sz; i++)
    {
        Xpost_Object fd = xpost_array_get(ctx, fdarray, i);
        Xpost_Object priv;

        Xpost_Object topfm, fdfm;
        double m[6] = { 0.001, 0, 0, 0.001, 0, 0 };

        if (xpost_object_get_type(fd) != dicttype)
            goto fail2;
        if (_cid_emit(&buf, &len, &cap,
            "%%ADOBeginFontDict\n"
            "dup %d 10 dict begin\n/FontType 1 def\n", i)) goto fail;
        /* the face carries one matrix per dictionary: the product of
           the font's matrix and the dictionary's own, so the glyph
           space the charstrings draw in reaches CID font space the
           way the two-level dictionary said it should */
        topfm = xpost_dict_get(ctx, fontdict, xpost_name_cons(ctx, "FontMatrix"));
        fdfm = xpost_dict_get(ctx, fd, xpost_name_cons(ctx, "FontMatrix"));
        if (xpost_object_get_type(topfm) == arraytype && topfm.comp_.sz == 6
         && xpost_object_get_type(fdfm) == arraytype && fdfm.comp_.sz == 6)
        {
            double a[6], b[6];
            int j;

            for (j = 0; j < 6; j++)
            {
                Xpost_Object v = xpost_array_get(ctx, fdfm, j);
                a[j] = xpost_object_get_type(v) == realtype ? v.real_.val : v.int_.val;
                v = xpost_array_get(ctx, topfm, j);
                b[j] = xpost_object_get_type(v) == realtype ? v.real_.val : v.int_.val;
            }
            m[0] = a[0]*b[0] + a[1]*b[2];
            m[1] = a[0]*b[1] + a[1]*b[3];
            m[2] = a[2]*b[0] + a[3]*b[2];
            m[3] = a[2]*b[1] + a[3]*b[3];
            m[4] = a[4]*b[0] + a[5]*b[2] + b[4];
            m[5] = a[4]*b[1] + a[5]*b[3] + b[5];
        }
        if (_cid_emit(&buf, &len, &cap,
            "/FontMatrix [%g %g %g %g %g %g] def\n",
            m[0], m[1], m[2], m[3], m[4], m[5])) goto fail;
        if (_cid_emit(&buf, &len, &cap, "/PaintType 0 def\n/Private 32 dict begin\n"))
            goto fail;
        priv = xpost_dict_get(ctx, fd, xpost_name_cons(ctx, "Private"));
        if (xpost_object_get_type(priv) == dicttype)
            for (k = 0; k < sizeof _cid_private_keys / sizeof *_cid_private_keys; k++)
                if (_cid_emit_entry(ctx, &buf, &len, &cap, priv,
                                    _cid_private_keys[k])) goto fail;
        if (_cid_emit(&buf, &len, &cap,
            "currentdict end def\ncurrentdict end put\n"
            "%%ADOEndFontDict\n")) goto fail;
    }
    if (_cid_emit(&buf, &len, &cap, "def\n(Binary) %lu StartData ",
                  (unsigned long)glen)) goto fail;

    whole = malloc(len + glen);
    if (!whole)
        goto fail2;
    memcpy(whole, buf, len);
    gpos = len;
    if (xpost_object_get_type(gdata) == stringtype)
    {
        memcpy(whole + gpos, xpost_string_get_pointer(ctx, gdata), glen);
    }
    else
    {
        for (i = 0; i < gdata.comp_.sz; i++)
        {
            Xpost_Object s = xpost_array_get(ctx, gdata, i);
            memcpy(whole + gpos, xpost_string_get_pointer(ctx, s), s.comp_.sz);
            gpos += s.comp_.sz;
        }
    }
    free(buf);

    /* a rebuilt descendant releases its previous face */
    privatestr = xpost_dict_get(ctx, fontdict, xpost_name_cons(ctx, "Private"));
    if (xpost_object_get_type(privatestr) == stringtype)
    {
        xpost_memory_get(xpost_context_select_memory(ctx, privatestr),
                         xpost_object_get_ent(privatestr), 0, sizeof data, &data);
        if (data.face)
            xpost_font_face_free(data.face);
    }

    data.face = xpost_font_face_new_from_memory(whole, len + glen);
    data.program = whole;
    if (data.face == NULL)
    {
        free(whole);
        return invalidfont;
    }

    fontbbox = xpost_array_cons(ctx, 4);
    xpost_font_face_get_bbox(data.face, fontbboxarray, 1000.0);
    xpost_memory_put(xpost_context_select_memory(ctx, fontbbox),
                     xpost_object_get_ent(fontbbox),
                     0, 4 * sizeof(Xpost_Object), fontbboxarray);
    xpost_dict_put(ctx, fontdict, xpost_name_cons(ctx, "FontBBox"), fontbbox);

    privatestr = xpost_string_cons(ctx, sizeof data, NULL);
    xpost_memory_put(xpost_context_select_memory(ctx, privatestr),
                     xpost_object_get_ent(privatestr), 0, sizeof data, &data);
    xpost_dict_put(ctx, fontdict, xpost_name_cons(ctx, "Private"), privatestr);
    return 0;
fail:
fail2:
    free(buf);
    return invalidfont;
#else
    (void)ctx;
    (void)fontdict;
    return invalidfont;
#endif
}


/* fontdict charstrings-flat subrs  .loadfont1  -
   assemble a working face for a Type 1 font defined by an embedded
   program. The interpreted dictionary is written back out as a font
   program -- cleartext header, then an eexec section carrying the
   private dictionary, the subroutines and the charstrings, whose
   own charstring-level encryption the strings still carry -- and
   opened as a memory face. The charstrings arrive flattened as
   name, string pairs, since only the interpreter can walk its
   dictionaries. */

static void
_t1_encrypt(unsigned char *data, size_t n)
{
    unsigned short r = 55665;
    size_t i;

    for (i = 0; i < n; i++)
    {
        unsigned char p = data[i];
        unsigned char c = p ^ (r >> 8);

        data[i] = c;
        r = (unsigned short)((c + r) * 52845 + 22719);
    }
}

static int
_t1_emit_bin(Xpost_Context *ctx, char **buf, size_t *len, size_t *cap,
             Xpost_Object s)
{
    char *p = xpost_string_get_pointer(ctx, s);

    while (*len + s.comp_.sz + 1 >= *cap)
    {
        char *nb = realloc(*buf, *cap * 2);

        if (!nb)
            return -1;
        *buf = nb;
        *cap *= 2;
    }
    memcpy(*buf + *len, p, s.comp_.sz);
    *len += s.comp_.sz;
    return 0;
}

static
int _loadfont1(Xpost_Context *ctx,
               Xpost_Object fontdict,
               Xpost_Object csflat,
               Xpost_Object subrs)
{
#ifdef HAVE_FREETYPE2
    Xpost_Object priv, privatestr, fontbbox;
    Xpost_Object fontbboxarray[4];
    struct fontdata data;
    char *hdr, *sec;
    unsigned char *whole;
    size_t hlen = 0, hcap = 2048, slen = 0, scap = 16384;
    int i;
    unsigned int k;

    if (xpost_object_get_type(csflat) != arraytype
     || xpost_object_get_type(subrs) != arraytype)
        return invalidfont;
    priv = xpost_dict_get(ctx, fontdict, xpost_name_cons(ctx, "Private"));
    if (xpost_object_get_type(priv) != dicttype)
        return invalidfont;

    hdr = malloc(hcap);
    if (!hdr)
        return VMerror;
    if (_cid_emit(&hdr, &hlen, &hcap,
        "%%!PS-AdobeFont-1.0: X 001.001\n"
        "11 dict begin\n"
        "/FontName /X def\n"
        "/FontType 1 def\n"
        "/PaintType 0 def\n")) goto failh;
    if (_cid_emit_entry(ctx, &hdr, &hlen, &hcap, fontdict, "FontMatrix")) goto failh;
    if (_cid_emit_entry(ctx, &hdr, &hlen, &hcap, fontdict, "FontBBox")) goto failh;
    if (_cid_emit(&hdr, &hlen, &hcap,
        "/Encoding StandardEncoding def\n"
        "currentdict end\n"
        "currentfile eexec\n")) goto failh;

    sec = malloc(scap);
    if (!sec)
        goto failh;
    /* four salt bytes ahead of the program proper */
    if (_cid_emit(&sec, &slen, &scap, "XPT1"
        "dup /Private 16 dict dup begin\n"
        "/RD {string currentfile exch readstring pop} executeonly def\n"
        "/ND {noaccess def} executeonly def\n"
        "/NP {noaccess put} executeonly def\n"
        "/password 5839 def\n"
        "/MinFeature {16 16} def\n")) goto fails;
    for (k = 0; k < sizeof _cid_private_keys / sizeof *_cid_private_keys; k++)
        if (_cid_emit_entry(ctx, &sec, &slen, &scap, priv,
                            _cid_private_keys[k])) goto fails;
    if (subrs.comp_.sz > 0)
    {
        if (_cid_emit(&sec, &slen, &scap, "/Subrs %d array\n", subrs.comp_.sz))
            goto fails;
        for (i = 0; i < subrs.comp_.sz; i++)
        {
            Xpost_Object s = xpost_array_get(ctx, subrs, i);

            if (xpost_object_get_type(s) != stringtype)
                goto fails;
            if (_cid_emit(&sec, &slen, &scap, "dup %d %u RD ", i,
                          (unsigned int)s.comp_.sz)) goto fails;
            if (_t1_emit_bin(ctx, &sec, &slen, &scap, s)) goto fails;
            if (_cid_emit(&sec, &slen, &scap, " NP\n")) goto fails;
        }
        if (_cid_emit(&sec, &slen, &scap, "ND\n")) goto fails;
    }
    if (_cid_emit(&sec, &slen, &scap, "end put\n"
        "dup /CharStrings %d dict dup begin\n", csflat.comp_.sz / 2 + 1))
        goto fails;
    for (i = 0; i + 1 < csflat.comp_.sz; i += 2)
    {
        Xpost_Object nm = xpost_array_get(ctx, csflat, i);
        Xpost_Object s = xpost_array_get(ctx, csflat, i + 1);
        Xpost_Object nstr;
        char nbuf[128];

        if (xpost_object_get_type(s) != stringtype)
            continue;
        if (xpost_object_get_type(nm) != nametype)
            continue;
        nstr = xpost_name_get_string(ctx, nm);
        if (nstr.comp_.sz >= sizeof nbuf)
            continue;
        memcpy(nbuf, xpost_string_get_pointer(ctx, nstr), nstr.comp_.sz);
        nbuf[nstr.comp_.sz] = 0;
        if (_cid_emit(&sec, &slen, &scap, "/%s %u RD ", nbuf,
                      (unsigned int)s.comp_.sz)) goto fails;
        if (_t1_emit_bin(ctx, &sec, &slen, &scap, s)) goto fails;
        if (_cid_emit(&sec, &slen, &scap, " ND\n")) goto fails;
    }
    if (_cid_emit(&sec, &slen, &scap,
        "end end put put\n"
        "dup /FontName get exch definefont pop\n"
        "mark currentfile closefile\n")) goto fails;

    /* a reader decides hex against the first four cipher bytes, so
       the salt must not encrypt to four hexadecimal characters */
    for (;;)
    {
        unsigned char t[4];
        unsigned short r = 55665;
        int j, allhex = 1;

        for (j = 0; j < 4; j++)
        {
            unsigned char cc = (unsigned char)(sec[j] ^ (r >> 8));

            t[j] = cc;
            r = (unsigned short)((cc + r) * 52845 + 22719);
        }
        for (j = 0; j < 4; j++)
            if (!( (t[j] >= '0' && t[j] <= '9')
                || (t[j] >= 'a' && t[j] <= 'f')
                || (t[j] >= 'A' && t[j] <= 'F') ))
                allhex = 0;
        if (!allhex)
            break;
        sec[0]++;   /* different salt, different ciphertext */
    }

    whole = malloc(hlen + slen + 34);
    if (!whole)
        goto fails;
    memcpy(whole, hdr, hlen);
    memcpy(whole + hlen, sec, slen);
    _t1_encrypt(whole + hlen, slen);
    memcpy(whole + hlen + slen, "\n0000000000000000\ncleartomark\n", 30);
    free(hdr);
    free(sec);

    data.face = xpost_font_face_new_from_memory(whole, hlen + slen + 30);
    data.program = whole;
    if (data.face == NULL)
    {
        free(whole);
        return invalidfont;
    }

    fontbbox = xpost_array_cons(ctx, 4);
    xpost_font_face_get_bbox(data.face, fontbboxarray, 1000.0);
    xpost_memory_put(xpost_context_select_memory(ctx, fontbbox),
                     xpost_object_get_ent(fontbbox),
                     0, 4 * sizeof(Xpost_Object), fontbboxarray);
    xpost_dict_put(ctx, fontdict, xpost_name_cons(ctx, "FontBBox"), fontbbox);

    privatestr = xpost_string_cons(ctx, sizeof data, NULL);
    xpost_memory_put(xpost_context_select_memory(ctx, privatestr),
                     xpost_object_get_ent(privatestr), 0, sizeof data, &data);
    xpost_dict_put(ctx, fontdict, xpost_name_cons(ctx, "Private"), privatestr);
    xpost_dict_put(ctx, fontdict, xpost_name_cons(ctx, ".emunits"),
                   xpost_int_cons(xpost_font_face_units(data.face)));
    return 0;
fails:
    free(sec);
failh:
    free(hdr);
    return invalidfont;
#else
    (void)ctx; (void)fontdict; (void)csflat; (void)subrs;
    return invalidfont;
#endif
}

/* ciddict glypharray  .loadcidfont2  -
   assemble a working TrueType face for a CIDFontType 2 dictionary.
   The /sfnts strings supply every table but the outlines: the glyphs
   arrive in /GlyphDirectory, delivered incrementally by glyph index,
   and the caller flattens the directory into an array indexed by
   glyph (null where none has arrived). A fresh glyf table and a
   long-format loca are synthesized around the delivered outlines,
   maxp's glyph count and head's loca format patched to match, and
   the whole reassembled program opened as a memory face stored in
   /Private. Called again after the directory has grown, the previous
   face is released and rebuilt around the larger complement. */
static
int _loadcidfont2(Xpost_Context *ctx,
                  Xpost_Object fontdict,
                  Xpost_Object glyphs)
{
#ifdef HAVE_FREETYPE2
    Xpost_Object sfnts;
    Xpost_Object privatestr;
    Xpost_Object fontbbox;
    Xpost_Object fontbboxarray[4];
    struct fontdata data;
    unsigned char *buf = NULL, *out = NULL;
    size_t total, glyftotal, outtotal, pos;
    unsigned int ntab, nglyphs;
    unsigned int headoff = 0, maxpoff = 0;
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
    if (total < 12)
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

    ntab = _sfnt_u16(buf + 4);
    if (12 + 16 * (size_t)ntab > total)
    {
        free(buf);
        return invalidfont;
    }

    nglyphs = glyphs.comp_.sz;
    glyftotal = 0;
    for (i = 0; i < (int)nglyphs; i++)
    {
        Xpost_Object g = xpost_array_get(ctx, glyphs, i);
        if (xpost_object_get_type(g) == stringtype)
            glyftotal += (g.comp_.sz + 1) & ~(size_t)1;
    }

    /* rebuild the directory: glyf and loca are synthesized (the
       template may omit them entirely, carrying a gdir placeholder
       for the incremental download instead), the hinting programs
       and the placeholder are dropped -- a subset template's
       bytecode does not survive its stripping and fails every glyph
       under the bytecode interpreter -- and everything else is
       carried over */
    {
        int has_glyf = 0, has_loca = 0;
        unsigned int newntab = 0, w = 0;

        out = NULL;
        for (i = 0; i < (int)ntab; i++)
        {
            unsigned int tag = _sfnt_u32(buf + 12 + 16 * i);
            if (tag == 0x63767420 || tag == 0x6670676d
             || tag == 0x70726570 || tag == 0x67646972)
                continue;
            if (tag == 0x676c7966) has_glyf = 1;
            if (tag == 0x6c6f6361) has_loca = 1;
            newntab++;
        }
        newntab += !has_glyf + !has_loca;

        outtotal = 12 + 16 * (size_t)newntab;
        for (i = 0; i < (int)ntab; i++)
        {
            unsigned char *e = buf + 12 + 16 * i;
            unsigned int tag = _sfnt_u32(e);
            size_t len;
            if (tag == 0x63767420 || tag == 0x6670676d
             || tag == 0x70726570 || tag == 0x67646972)
                continue;
            if (tag == 0x676c7966)
                len = glyftotal;
            else if (tag == 0x6c6f6361)
                len = 4 * ((size_t)nglyphs + 1);
            else
                len = _sfnt_u32(e + 12);
            outtotal = (outtotal + 3) & ~(size_t)3;
            outtotal += len;
        }
        if (!has_glyf)
        {
            outtotal = (outtotal + 3) & ~(size_t)3;
            outtotal += glyftotal;
        }
        if (!has_loca)
        {
            outtotal = (outtotal + 3) & ~(size_t)3;
            outtotal += 4 * ((size_t)nglyphs + 1);
        }
        out = malloc(outtotal);
        if (!out)
        {
            free(buf);
            return VMerror;
        }
        memcpy(out, buf, 12);
        _sfnt_put16(out + 4, newntab);
        for (i = 0; i < (int)ntab; i++)
        {
            unsigned char *e = buf + 12 + 16 * i;
            unsigned int tag = _sfnt_u32(e);
            if (tag == 0x63767420 || tag == 0x6670676d
             || tag == 0x70726570 || tag == 0x67646972)
                continue;
            memcpy(out + 12 + 16 * w, e, 16);
            w++;
        }
        pos = 12 + 16 * (size_t)w;
        if (!has_glyf)
        {
            memset(out + pos, 0, 16);
            _sfnt_put32(out + pos, 0x676c7966);
            pos += 16;
            w++;
        }
        if (!has_loca)
        {
            memset(out + pos, 0, 16);
            _sfnt_put32(out + pos, 0x6c6f6361);
            pos += 16;
            w++;
        }
        ntab = newntab;
    }

    pos = 12 + 16 * (size_t)ntab;
    for (i = 0; i < (int)ntab; i++)
    {
        unsigned char *e = out + 12 + 16 * i;
        unsigned int tag = _sfnt_u32(e);
        unsigned int srcoff = _sfnt_u32(e + 8);
        unsigned int srclen = _sfnt_u32(e + 12);

        pos = (pos + 3) & ~(size_t)3;
        if (tag == 0x676c7966)
        {
            size_t gp = 0;
            int gi;
            for (gi = 0; gi < (int)nglyphs; gi++)
            {
                Xpost_Object g = xpost_array_get(ctx, glyphs, gi);
                if (xpost_object_get_type(g) == stringtype)
                {
                    memcpy(out + pos + gp,
                           xpost_string_get_pointer(ctx, g), g.comp_.sz);
                    if (g.comp_.sz & 1)
                        out[pos + gp + g.comp_.sz] = 0;
                    gp += (g.comp_.sz + 1) & ~(size_t)1;
                }
            }
            _sfnt_put32(e + 8, (unsigned int)pos);
            _sfnt_put32(e + 12, (unsigned int)glyftotal);
            pos += glyftotal;
        }
        else if (tag == 0x6c6f6361)
        {
            size_t gp = 0;
            int gi;
            for (gi = 0; gi <= (int)nglyphs; gi++)
            {
                _sfnt_put32(out + pos + 4 * gi, (unsigned int)gp);
                if (gi < (int)nglyphs)
                {
                    Xpost_Object g = xpost_array_get(ctx, glyphs, gi);
                    if (xpost_object_get_type(g) == stringtype)
                        gp += (g.comp_.sz + 1) & ~(size_t)1;
                }
            }
            _sfnt_put32(e + 8, (unsigned int)pos);
            _sfnt_put32(e + 12, 4 * (nglyphs + 1));
            pos += 4 * ((size_t)nglyphs + 1);
        }
        else
        {
            if ((size_t)srcoff + srclen > total)
            {
                free(buf); free(out);
                return invalidfont;
            }
            memcpy(out + pos, buf + srcoff, srclen);
            if (tag == 0x68656164) headoff = (unsigned int)pos;
            if (tag == 0x6d617870) maxpoff = (unsigned int)pos;
            _sfnt_put32(e + 8, (unsigned int)pos);
            pos += srclen;
        }
    }
    free(buf);
    if (headoff && headoff + 52 <= outtotal)
        _sfnt_put16(out + headoff + 50, 1);   /* long loca offsets */
    if (maxpoff && maxpoff + 6 <= outtotal)
        _sfnt_put16(out + maxpoff + 4, nglyphs);

    /* replace any previous face: the directory grows between shows */
    privatestr = xpost_dict_get(ctx, fontdict, xpost_name_cons(ctx, "Private"));
    if (xpost_object_get_type(privatestr) == stringtype)
    {
        xpost_memory_get(xpost_context_select_memory(ctx, privatestr),
                         xpost_object_get_ent(privatestr), 0, sizeof data, &data);
        if (data.face)
            xpost_font_face_free(data.face);
        free(data.program);
    }

    data.face = xpost_font_face_new_from_memory(out, outtotal);
    data.program = out;
    if (data.face == NULL)
    {
        free(out);
        return invalidfont;
    }

    fontbbox = xpost_array_cons(ctx, 4);
    xpost_font_face_get_bbox(data.face, fontbboxarray, 1.0);
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
    (void)glyphs;
    return invalidfont;
#endif
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
    _face_setup(ctx, gs, fontdict, data.face);
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
    for (ch = cstr; *ch; ch++)
    {
        _show_char(ctx, devdic, putpix, data, &ts, &xpos, &ypos, (unsigned char)*ch,
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
    _face_setup(ctx, gs, fontdict, data.face);
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
    for (ch = cstr; *ch; ch++)
    {
        _show_char(ctx, devdic, putpix, data, &ts, &xpos, &ypos, (unsigned char)*ch,
                   ncomp, comp[0], comp[1], comp[2], comp[3]);
        if ((unsigned char)*ch == charcode.int_.val)
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
    _face_setup(ctx, gs, fontdict, data.face);
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
    for (ch = cstr; *ch; ch++)
    {
        _show_char(ctx, devdic, putpix, data, &ts, &xpos, &ypos, (unsigned char)*ch,
                ncomp, comp[0], comp[1], comp[2], comp[3]);
        xpos += dx.real_.val;
        ypos += dy.real_.val;
        if ((unsigned char)*ch == charcode.int_.val)
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
    Xpost_Object charstrings;

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
    _face_setup(ctx, gs, fontdict, data.face);
    encoding = xpost_dict_get(ctx, fontdict, xpost_name_cons(ctx, "Encoding"));
    charstrings = xpost_dict_get(ctx, fontdict, xpost_name_cons(ctx, "CharStrings"));
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

        glyph_index = _glyph_index_for_char(ctx, encoding, charstrings,
                                            data.face, (unsigned char)*ch);
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
        xpos += (real)(advance_x / 65536.0);
        ypos += (real)(advance_y / 65536.0);
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
    Xpost_Object charstrings;
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
    _face_setup(ctx, gs, fontdict, data.face);
    encoding = xpost_dict_get(ctx, fontdict, xpost_name_cons(ctx, "Encoding"));
    charstrings = xpost_dict_get(ctx, fontdict, xpost_name_cons(ctx, "CharStrings"));

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

        glyph_index = _glyph_index_for_char(ctx, encoding, charstrings,
                                            data.face, (unsigned char)*ch);
        sink.moveto = _oc_moveto;
        sink.lineto = _oc_lineto;
        sink.curveto = _oc_curveto;
        sink.closepath = _oc_closepath;
        sink.user = &oc;
        if (!xpost_font_face_glyph_outline(data.face, glyph_index, &sink, &advance_x, &advance_y))
        {
            /* a glyph without an outline cannot contribute a path */
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
        oc.px += advance_x / 65536.0;
        oc.py += advance_y / 65536.0;
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
    op = xpost_operator_cons(ctx, "setfont", (Xpost_Op_Func)_setfont, 1, 1, dicttype);
    INSTALL;

    op = xpost_operator_cons(ctx, "show", (Xpost_Op_Func)_show, 0, 1, stringtype);
    INSTALL;
    op = xpost_operator_cons(ctx, ".glyphshow", (Xpost_Op_Func)_glyphshow, 0, 1, nametype);
    INSTALL;
    op = xpost_operator_cons(ctx, ".glyphshowidx", (Xpost_Op_Func)_glyphshowidx, 0, 1, integertype);
    INSTALL;
    op = xpost_operator_cons(ctx, ".loadcidfont0", (Xpost_Op_Func)_loadcidfont0, 0, 1, dicttype);
    INSTALL;
    op = xpost_operator_cons(ctx, ".loadfont1", (Xpost_Op_Func)_loadfont1, 0, 3,
            dicttype, arraytype, arraytype);
    INSTALL;
    op = xpost_operator_cons(ctx, ".loadcidfont2", (Xpost_Op_Func)_loadcidfont2, 0, 2,
            dicttype, arraytype);
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

    /* xpost_dict_dump_memory (ctx->gl, sd); fflush(NULL);
    xpost_dict_put(ctx, sd, xpost_name_cons(ctx, "mark"), mark); */

    return 0;
}
