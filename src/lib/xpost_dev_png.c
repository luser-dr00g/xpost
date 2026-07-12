/*
 * Xpost - a Level-2 Postscript interpreter
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

#ifdef HAVE_LIBPNG

#include <stdlib.h>
#include <string.h>
#include <png.h>
#include <setjmp.h>

#include "xpost.h"
#include "xpost_log.h"
#include "xpost_memory.h" /* access memory */
#include "xpost_object.h" /* work with objects */
#include "xpost_stack.h"  /* push results on stack */
#include "xpost_context.h" /* state */
#include "xpost_error.h"
#include "xpost_dict.h" /* get/put values in dicts */
#include "xpost_string.h" /* get/put values in strings */
#include "xpost_array.h"
#include "xpost_name.h" /* create names */

#include "xpost_operator.h" /* create operators */
#include "xpost_op_dict.h" /* call load operator for convenience */
#include "xpost_dev_generic.h" /* get filename */
#include "xpost_dev_png.h" /* check prototypes */

typedef struct
{
    unsigned char red, green, blue, alpha;
} Xpost_Png_Pixel;

typedef struct
{
    int width, height, byte_stride;
    Xpost_Png_Pixel data[1];
} Xpost_Png_Buffer;

typedef struct
{
    int width;
    int height;
    /*
     * add additional members to private struct
     */
    FILE *f;
    png_structp         png_ptr;
    png_infop           info_ptr;
    Xpost_Png_Buffer *buf;
    unsigned int interlaced : 1;
    unsigned int emitted : 1;
    unsigned int alpha : 1;
} PrivateData;

static Xpost_Object namePrivate;
static Xpost_Object namewidth;
static Xpost_Object nameheight;
static Xpost_Object namedotcopydict;
static Xpost_Object namenativecolorspace;
static Xpost_Object nameDeviceRGB;


static unsigned int _create_cont_opcode;

/* create an instance of the device
   using the class .copydict procedure */
static
int _create(Xpost_Context *ctx,
            Xpost_Object width,
            Xpost_Object height,
            Xpost_Object classdic)
{
    xpost_stack_push(ctx->lo, ctx->os, width);
    xpost_stack_push(ctx->lo, ctx->os, height);
    xpost_stack_push(ctx->lo, ctx->os, classdic);
    xpost_dict_put(ctx, classdic, namewidth, width);
    xpost_dict_put(ctx, classdic, nameheight, height);

    /* call device class's ps-level .copydict procedure,
       //call base-class's Create procedure (to initialize ImgData array)
       then call _create_cont, by continuation. */
    if (!xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(_create_cont_opcode)))
        return execstackoverflow;

    if (!xpost_stack_push(ctx->lo, ctx->es, xpost_dict_get(ctx, classdic, namedotcopydict)))
        return execstackoverflow;

    return 0;
}

/* initialize the C-level data
   and define in the device instance */
static
int _create_cont(Xpost_Context *ctx,
                 Xpost_Object w,
                 Xpost_Object h,
                 Xpost_Object devdic)
{
    PrivateData private;
    Xpost_Object privatestr;
    Xpost_Object ud;
    Xpost_Object compression_level_o;
    Xpost_Object interlaced_o;
    char *filename;
    png_color_8 sig_bit;
    integer width = w.int_.val;
    integer height = h.int_.val;
    int compression_level;
    //printf("create_cont\n");

    /* create a string to contain device data structure */
    privatestr = xpost_string_cons(ctx, sizeof(PrivateData), NULL);
    if (xpost_object_get_type(privatestr) == invalidtype)
    {
        XPOST_LOG_ERR("cannot allocat private data structure");
        return unregistered;
    }
    xpost_dict_put(ctx, devdic, namePrivate, privatestr);

    private.width = width;
    private.height = height;
    private.emitted = 0;
    {
        Xpost_Object alpha_o = xpost_dict_get(ctx, devdic,
                                              xpost_name_cons(ctx, "AlphaChannel"));
        private.alpha = xpost_object_get_type(alpha_o) == booleantype
                     && alpha_o.int_.val;
    }

    /*
     *
     * initialize additional members of private struct
     *
     */

    filename = xpost_device_get_filename(ctx, devdic);
    if (!filename)
    {
        XPOST_LOG_ERR("cannot retrieve PNG file name");
        return unregistered;
    }

    private.f = fopen(filename, "wb");
    free(filename);
    if (!private.f)
    {
        XPOST_LOG_ERR("cannot retrieve PNG file name");
        return unregistered;
    }

    ud = xpost_stack_bottomup_fetch(ctx->lo, ctx->ds, 2);
    compression_level_o = xpost_dict_get(ctx, ud,
                                         xpost_name_cons(ctx, "png_compression_level"));
    interlaced_o = xpost_dict_get(ctx, ud,
                                  xpost_name_cons(ctx, "png_interlaced"));

    if (xpost_object_get_type(compression_level_o) == invalidtype)
        compression_level = 3;
    else
        compression_level = compression_level_o.int_.val;
    XPOST_LOG_INFO("PNG compresion level: %d", compression_level);

    if (xpost_object_get_type(interlaced_o) == invalidtype)
        private.interlaced = PNG_INTERLACE_NONE;
    else
    {
        if (interlaced_o.int_.val)
        {
#ifdef PNG_WRITE_INTERLACING_SUPPORTED
            private.interlaced = PNG_INTERLACE_ADAM7;
#else
            private.interlaced = PNG_INTERLACE_NONE;
#endif
        }
        else
            private.interlaced = PNG_INTERLACE_NONE;
    }
    XPOST_LOG_INFO("PNG interlacing: %s",
                   (private.interlaced == PNG_INTERLACE_ADAM7) ? "Adam7" : "none");
    private.info_ptr = NULL;
    private.png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                              NULL, NULL, NULL);
    if (!private.png_ptr)
        goto close_file;

    private.info_ptr = png_create_info_struct(private.png_ptr);
    if (!private.info_ptr)
        goto destroy_png;

    if (setjmp(png_jmpbuf(private.png_ptr)))
        goto destroy_info;

    /* allocate buffer header and array */
    private.buf = malloc(sizeof(Xpost_Png_Buffer) +
                         sizeof(Xpost_Png_Pixel) * width * height);
    if (!private.buf)
    {
        XPOST_LOG_ERR("cannot allocate buffer memory");
        goto destroy_info;
    }

    /* the page starts opaque white; the alpha device starts fully
       transparent, so only marks made by the job carry opacity and an
       erased page is see-through */
    {
        int i;
        Xpost_Png_Pixel init;

        init.red = init.green = init.blue = 255;
        init.alpha = private.alpha ? 0 : 255;
        for (i = 0; i < width * height; i++)
            private.buf->data[i] = init;
    }

	png_init_io(private.png_ptr, private.f);
	png_set_IHDR(private.png_ptr, private.info_ptr,
                 private.width, private.height, 8,
                 private.alpha ? PNG_COLOR_TYPE_RGB_ALPHA : PNG_COLOR_TYPE_RGB,
                 private.interlaced,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    sig_bit.red = 8;
    sig_bit.green = 8;
    sig_bit.blue = 8;
    sig_bit.alpha = 8;
    png_set_sBIT(private.png_ptr, private.info_ptr, &sig_bit);

    png_set_compression_level(private.png_ptr, compression_level);
    png_write_info(private.png_ptr, private.info_ptr);
    png_set_shift(private.png_ptr, &sig_bit);
    png_set_packing(private.png_ptr);
    if (!private.alpha)
        /* rows carry a fourth byte per pixel; skip it when writing RGB */
        png_set_filler(private.png_ptr, 0, PNG_FILLER_AFTER);

    /* save private data struct in string */
    xpost_memory_put(xpost_context_select_memory(ctx, privatestr),
                     xpost_object_get_ent(privatestr), 0,
                     sizeof(private), &private);

    /* return device instance dictionary to ps */
    xpost_stack_push(ctx->lo, ctx->os, devdic);
    return 0;

  destroy_info:
	png_destroy_info_struct(private.png_ptr, (png_infopp) & private.info_ptr);
  destroy_png:
	png_destroy_write_struct(&private.png_ptr,
                             (private.info_ptr) ? (png_infopp)&private.info_ptr : NULL);
  close_file:
    fclose(private.f);

    return unregistered;
}

static
int _putpix(Xpost_Context *ctx,
            Xpost_Object red,
            Xpost_Object green,
            Xpost_Object blue,
            Xpost_Object x,
            Xpost_Object y,
            Xpost_Object devdic)
{
    Xpost_Object privatestr;
    PrivateData private;

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
                     xpost_object_get_ent(privatestr), 0,
                     sizeof(private), &private);

    /* check bounds */
    if ((x.int_.val < 0) ||
        (x.int_.val >= private.width) ||
        (y.int_.val < 0) ||
        (y.int_.val >= private.height))
        return 0;

    {
        Xpost_Png_Pixel pixel;
        pixel.blue = blue.int_.val;
        pixel.green = green.int_.val;
        pixel.red = red.int_.val;
        pixel.alpha = 255;
        private.buf->data[y.int_.val * private.width + x.int_.val] = pixel;
    }

    /* save private data struct in string */
    xpost_memory_put(xpost_context_select_memory(ctx, privatestr),
                     xpost_object_get_ent(privatestr), 0,
                     sizeof(private), &private);

    return 0;
}

/* Blend a coverage-weighted pixel: each channel moves toward the colour
   by cov/255. The text operators use this for glyph edge pixels when the
   device renders anti-aliased text. */
static
int _blendpix(Xpost_Context *ctx,
              Xpost_Object red,
              Xpost_Object green,
              Xpost_Object blue,
              Xpost_Object cov,
              Xpost_Object x,
              Xpost_Object y,
              Xpost_Object devdic)
{
    Xpost_Object privatestr;
    PrivateData private;
    int r, g, b, c, ix, iy;

    r = (int)((xpost_object_get_type(red)   == realtype ? red.real_.val   * 255.0 : red.int_.val   * 255));
    g = (int)((xpost_object_get_type(green) == realtype ? green.real_.val * 255.0 : green.int_.val * 255));
    b = (int)((xpost_object_get_type(blue)  == realtype ? blue.real_.val  * 255.0 : blue.int_.val  * 255));
    c = xpost_object_get_type(cov) == realtype ? (int)cov.real_.val : cov.int_.val;
    ix = xpost_object_get_type(x) == realtype ? (int)x.real_.val : x.int_.val;
    iy = xpost_object_get_type(y) == realtype ? (int)y.real_.val : y.int_.val;

    privatestr = xpost_dict_get(ctx, devdic, namePrivate);
    if (xpost_object_get_type(privatestr) == invalidtype)
        return undefined;
    xpost_memory_get(xpost_context_select_memory(ctx, privatestr),
                     xpost_object_get_ent(privatestr), 0,
                     sizeof(private), &private);

    if ((ix < 0) || (ix >= private.width) ||
        (iy < 0) || (iy >= private.height))
        return 0;

    {
        Xpost_Png_Pixel *p = &private.buf->data[iy * private.width + ix];
        int da = p->alpha;
        int oa = c + (da * (255 - c) + 127) / 255;

        if (oa == 0)
            return 0;
        /* source over: the ink contributes c, the ground its own
           opacity of what c leaves uncovered */
        p->red   = (unsigned char)((r * c + p->red   * da * (255 - c) / 255 + oa / 2) / oa);
        p->green = (unsigned char)((g * c + p->green * da * (255 - c) / 255 + oa / 2) / oa);
        p->blue  = (unsigned char)((b * c + p->blue  * da * (255 - c) / 255 + oa / 2) / oa);
        p->alpha = (unsigned char)oa;
    }

    return 0;
}

/* C fast-path for the base-class PS FillRect: fills the buffer directly
   rather than looping over PutPix per pixel. The only caller is erasepage
   (full-page clear), which dominates page-emission time when done in PS. */
static
int _fillrect(Xpost_Context *ctx,
              Xpost_Object red,
              Xpost_Object green,
              Xpost_Object blue,
              Xpost_Object x,
              Xpost_Object y,
              Xpost_Object w,
              Xpost_Object h,
              Xpost_Object devdic)
{
    Xpost_Object privatestr;
    PrivateData private;
    Xpost_Png_Pixel pixel;
    int ix, iy, x0, y0, x1, y1;

    /* fold numbers as PutPix does: colours scaled to 0..255, coords truncated */
    pixel.red   = (unsigned char)((xpost_object_get_type(red)   == realtype ? red.real_.val   * 255.0 : red.int_.val   * 255));
    pixel.green = (unsigned char)((xpost_object_get_type(green) == realtype ? green.real_.val * 255.0 : green.int_.val * 255));
    pixel.blue  = (unsigned char)((xpost_object_get_type(blue)  == realtype ? blue.real_.val  * 255.0 : blue.int_.val  * 255));
    pixel.alpha = 255;
    x0 = xpost_object_get_type(x) == realtype ? (int)x.real_.val : x.int_.val;
    y0 = xpost_object_get_type(y) == realtype ? (int)y.real_.val : y.int_.val;
    x1 = xpost_object_get_type(w) == realtype ? (int)w.real_.val : w.int_.val;
    y1 = xpost_object_get_type(h) == realtype ? (int)h.real_.val : h.int_.val;

    /* normalise negative extents, then form inclusive end coords (as PS FillRect) */
    if (x1 < 0) { x1 = -x1; x0 -= x1; }
    if (y1 < 0) { y1 = -y1; y0 -= y1; }
    x1 += x0;
    y1 += y0;

    privatestr = xpost_dict_get(ctx, devdic, namePrivate);
    if (xpost_object_get_type(privatestr) == invalidtype)
        return undefined;
    xpost_memory_get(xpost_context_select_memory(ctx, privatestr),
                     xpost_object_get_ent(privatestr), 0,
                     sizeof(private), &private);

    /* clip to device bounds */
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > private.width  - 1) x1 = private.width  - 1;
    if (y1 > private.height - 1) y1 = private.height - 1;

    for (iy = y0; iy <= y1; iy++)
    {
        Xpost_Png_Pixel *row = private.buf->data + iy * private.width;
        for (ix = x0; ix <= x1; ix++)
            row[ix] = pixel;
    }

    return 0;
}

static
int _emit(Xpost_Context *ctx,
          Xpost_Object devdic)
{
    Xpost_Object privatestr;
    PrivateData private;
    unsigned char *data;
    png_bytep row_ptr;
    int num_passes = 1;
    int pass;
    int y;

    /* load private data struct from string */
    privatestr = xpost_dict_get(ctx, devdic, namePrivate);
    if (xpost_object_get_type(privatestr) == invalidtype)
        return undefined;
    xpost_memory_get(xpost_context_select_memory(ctx, privatestr),
            xpost_object_get_ent(privatestr), 0, sizeof private, &private);

    /* libpng reports errors by longjmp: aim it at this call, not at
       the long-gone frame that created the device */
    if (setjmp(png_jmpbuf(private.png_ptr)))
        return ioerror;

#ifdef PNG_WRITE_INTERLACING_SUPPORTED
    num_passes = png_set_interlace_handling(private.png_ptr);
#endif

    for (pass = 0; pass < num_passes; pass++)
    {
        data = (unsigned char *)private.buf->data;
        for (y = 0; y < private.height; y++)
        {
            row_ptr = (png_bytep)data;
            png_write_rows(private.png_ptr, &row_ptr, 1);
            data += 4 * private.width;
        }
    }

    private.emitted = 1;
    xpost_memory_put(xpost_context_select_memory(ctx, privatestr),
                     xpost_object_get_ent(privatestr), 0,
                     sizeof(private), &private);

    /* pass data back to client application */
    {
        Xpost_Object sd, outbufstr;
        sd = xpost_stack_bottomup_fetch(ctx->lo, ctx->ds, 0);
        outbufstr = xpost_dict_get(ctx, sd, xpost_name_cons(ctx, "OutputBufferOut"));
        if (xpost_object_get_type(outbufstr) == stringtype)
        {
            unsigned char **outbuf;
            memcpy(&outbuf, xpost_string_get_pointer(ctx, outbufstr), sizeof(outbuf));
            *outbuf = (unsigned char *)private.buf->data;
        }
    }

    return 0;
}

/* clear the page to fully transparent: the alpha device's erasepage.
   An explicit white fill stays opaque; only the page reset is clear. */
static
int _erase(Xpost_Context *ctx,
           Xpost_Object devdic)
{
    Xpost_Object privatestr;
    PrivateData private;
    int i;
    Xpost_Png_Pixel init;

    privatestr = xpost_dict_get(ctx, devdic, namePrivate);
    if (xpost_object_get_type(privatestr) == invalidtype)
        return undefined;
    xpost_memory_get(xpost_context_select_memory(ctx, privatestr),
                     xpost_object_get_ent(privatestr), 0,
                     sizeof(private), &private);

    init.red = init.green = init.blue = 255;
    init.alpha = 0;
    for (i = 0; i < private.width * private.height; i++)
        private.buf->data[i] = init;

    return 0;
}

static
int _destroy(Xpost_Context *ctx,
             Xpost_Object devdic)
{
    Xpost_Object privatestr;
    PrivateData private;

    /* load private data struct from string */
    privatestr = xpost_dict_get(ctx, devdic, namePrivate);
    if (xpost_object_get_type(privatestr) == invalidtype)
        return undefined;
    xpost_memory_get(xpost_context_select_memory(ctx, privatestr),
                     xpost_object_get_ent(privatestr), 0,
                     sizeof(private), &private);

    free(private.buf);
    /* a device destroyed without a page emitted (an error ended the
       job first) has no image to finalise, and libpng would reject the
       trailer; aim its longjmp here either way, so a write error while
       finalising cannot jump into the dead frame that created the
       device */
    if (setjmp(png_jmpbuf(private.png_ptr)) == 0)
    {
        if (private.emitted)
            png_write_end(private.png_ptr, private.info_ptr);
    }
    png_destroy_write_struct(&private.png_ptr, (png_infopp) & private.info_ptr);
    png_destroy_info_struct(private.png_ptr, (png_infopp) & private.info_ptr);
    fclose(private.f);

    return 0;
}

/* operator function to instantiate a new window device.
   installed in userdict by calling 'loadXXXdevice'.
 */
static
int newpngdevice(Xpost_Context *ctx,
                 Xpost_Object width,
                 Xpost_Object height)
{
    Xpost_Object classdic;
    int ret;

    xpost_stack_push(ctx->lo, ctx->os, width);
    xpost_stack_push(ctx->lo, ctx->os, height);
    ret = xpost_op_any_load(ctx, xpost_name_cons(ctx, "pngDEVICE"));
    if (ret)
        return ret;
    classdic = xpost_stack_topdown_fetch(ctx->lo, ctx->os, 0);
    if (!xpost_stack_push(ctx->lo, ctx->es, xpost_dict_get(ctx, classdic, xpost_name_cons(ctx, "Create"))))
        return execstackoverflow;

    return 0;
}

static
int newpngalphadevice(Xpost_Context *ctx,
                      Xpost_Object width,
                      Xpost_Object height)
{
    Xpost_Object classdic;
    int ret;

    xpost_stack_push(ctx->lo, ctx->os, width);
    xpost_stack_push(ctx->lo, ctx->os, height);
    ret = xpost_op_any_load(ctx, xpost_name_cons(ctx, "pngalphaDEVICE"));
    if (ret)
        return ret;
    classdic = xpost_stack_topdown_fetch(ctx->lo, ctx->os, 0);
    if (!xpost_stack_push(ctx->lo, ctx->es, xpost_dict_get(ctx, classdic, xpost_name_cons(ctx, "Create"))))
        return execstackoverflow;

    return 0;
}

static
unsigned int _loadpngdevicecont_opcode;
static
unsigned int _loadpngalphadevicecont_opcode;

/* Specializes or sub-classes the PPMIMAGE device class.
   load PPMIMAGE
   load and call ps procedure .copydict which leaves copy on stack
   call loadpngdevicecont by continuation.
 */
static
int loadpngdevice(Xpost_Context *ctx)
{
    Xpost_Object classdic;
    int ret;

    ret = xpost_op_any_load(ctx, xpost_name_cons(ctx, "PPMIMAGE"));
    if (ret)
        return ret;
    classdic = xpost_stack_topdown_fetch(ctx->lo, ctx->os, 0);
    if (!xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(_loadpngdevicecont_opcode)))
        return execstackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->es, xpost_dict_get(ctx, classdic, namedotcopydict)))
        return execstackoverflow;

    return 0;
}

static
int loadpngalphadevice(Xpost_Context *ctx)
{
    Xpost_Object classdic;
    int ret;

    ret = xpost_op_any_load(ctx, xpost_name_cons(ctx, "PPMIMAGE"));
    if (ret)
        return ret;
    classdic = xpost_stack_topdown_fetch(ctx->lo, ctx->os, 0);
    if (!xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(_loadpngalphadevicecont_opcode)))
        return execstackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->es, xpost_dict_get(ctx, classdic, namedotcopydict)))
        return execstackoverflow;

    return 0;
}

/* replace procedures in the class with newly created special operators.
   defines the device class (pngDEVICE or pngalphaDEVICE) in userdict
   and the matching newXXXdevice operator. The alpha class carries
   /AlphaChannel for Create and an /Erase method for erasepage. */
static
int _loaddevicecont_common(Xpost_Context *ctx,
                           Xpost_Object classdic,
                           int alpha)
{
    Xpost_Object userdict;
    Xpost_Object op;
    int ret;

    ret = xpost_dict_put(ctx, classdic, namenativecolorspace, nameDeviceRGB);

    op = xpost_operator_cons(ctx, "pngCreateCont", (Xpost_Op_Func)_create_cont, 1, 3, integertype, integertype, dicttype);
    _create_cont_opcode = op.mark_.padw;
    op = xpost_operator_cons(ctx, "pngCreate", (Xpost_Op_Func)_create, 1, 3, integertype, integertype, dicttype);
    ret = xpost_dict_put(ctx, classdic, xpost_name_cons(ctx, "Create"), op);
    if (ret)
        return ret;

    op = xpost_operator_cons(ctx, "pngPutPix", (Xpost_Op_Func)_putpix, 0, 6,
            numbertype, numbertype, numbertype,
            numbertype, numbertype,
            dicttype);
    ret = xpost_dict_put(ctx, classdic, xpost_name_cons(ctx, "PutPix"), op);
    if (ret)
        return ret;

    op = xpost_operator_cons(ctx, "pngFillRect", (Xpost_Op_Func)_fillrect, 0, 8,
            numbertype, numbertype, numbertype,
            numbertype, numbertype, numbertype, numbertype,
            dicttype);
    ret = xpost_dict_put(ctx, classdic, xpost_name_cons(ctx, "FillRect"), op);
    if (ret)
        return ret;

    op = xpost_operator_cons(ctx, "pngBlendPix", (Xpost_Op_Func)_blendpix, 0, 7,
            numbertype, numbertype, numbertype,
            numbertype, numbertype, numbertype,
            dicttype);
    ret = xpost_dict_put(ctx, classdic, xpost_name_cons(ctx, "BlendPix"), op);
    if (ret)
        return ret;

    op = xpost_operator_cons(ctx, "pngEmit", (Xpost_Op_Func)_emit, 0, 1, dicttype);
    ret = xpost_dict_put(ctx, classdic, xpost_name_cons(ctx, "Emit"), op);
    if (ret)
        return ret;

    op = xpost_operator_cons(ctx, "pngDestroy", (Xpost_Op_Func)_destroy, 0, 1, dicttype);
    ret = xpost_dict_put(ctx, classdic, xpost_name_cons(ctx, "Destroy"), op);
    if (ret)
        return ret;

    if (alpha)
    {
        ret = xpost_dict_put(ctx, classdic, xpost_name_cons(ctx, "AlphaChannel"), xpost_bool_cons(1));
        if (ret)
            return ret;
        op = xpost_operator_cons(ctx, "pngErase", (Xpost_Op_Func)_erase, 0, 1, dicttype);
        ret = xpost_dict_put(ctx, classdic, xpost_name_cons(ctx, "Erase"), op);
        if (ret)
            return ret;
    }

    userdict = xpost_stack_bottomup_fetch(ctx->lo, ctx->ds, 2);

    ret = xpost_dict_put(ctx, userdict,
                         xpost_name_cons(ctx, alpha ? "pngalphaDEVICE" : "pngDEVICE"),
                         classdic);
    if (ret)
        return ret;

    if (alpha)
        op = xpost_operator_cons(ctx, "newpngalphadevice", (Xpost_Op_Func)newpngalphadevice, 1, 2, integertype, integertype);
    else
        op = xpost_operator_cons(ctx, "newpngdevice", (Xpost_Op_Func)newpngdevice, 1, 2, integertype, integertype);
    ret = xpost_dict_put(ctx, userdict,
                         xpost_name_cons(ctx, alpha ? "newpngalphadevice" : "newpngdevice"),
                         op);
    if (ret)
        return ret;

    return 0;
}

static
int loadpngdevicecont(Xpost_Context *ctx,
                      Xpost_Object classdic)
{
    return _loaddevicecont_common(ctx, classdic, 0);
}

static
int loadpngalphadevicecont(Xpost_Context *ctx,
                           Xpost_Object classdic)
{
    return _loaddevicecont_common(ctx, classdic, 1);
}

/*
   install the loadXXXdevice which may be called during graphics initialization
   to produce the operator newXXXdevice which instantiates the device dictionary.
*/
int xpost_oper_init_png_device_ops(Xpost_Context *ctx,
                                   Xpost_Object sd)
{
    unsigned int optadr;
    Xpost_Operator *optab;
    Xpost_Object n,op;

    /* factor-out name lookups from the operators (optimization) */
    if (xpost_object_get_type((namePrivate = xpost_name_cons(ctx, "Private"))) == invalidtype)
        return VMerror;
    if (xpost_object_get_type((namewidth = xpost_name_cons(ctx, "width"))) == invalidtype)
        return VMerror;
    if (xpost_object_get_type((nameheight = xpost_name_cons(ctx, "height"))) == invalidtype)
        return VMerror;
    if (xpost_object_get_type((namedotcopydict = xpost_name_cons(ctx, ".copydict"))) == invalidtype)
        return VMerror;
    if (xpost_object_get_type((namenativecolorspace = xpost_name_cons(ctx, "nativecolorspace"))) == invalidtype)
        return VMerror;
    if (xpost_object_get_type((nameDeviceRGB = xpost_name_cons(ctx, "DeviceRGB"))) == invalidtype)
        return VMerror;

    xpost_memory_table_get_addr(ctx->gl,
                                XPOST_MEMORY_TABLE_SPECIAL_OPERATOR_TABLE,
                                &optadr);
    optab = (Xpost_Operator *)(ctx->gl->base + optadr);
    op = xpost_operator_cons(ctx, "loadpngdevice", (Xpost_Op_Func)loadpngdevice, 1, 0); INSTALL;
    op = xpost_operator_cons(ctx, "loadpngdevicecont", (Xpost_Op_Func)loadpngdevicecont, 1, 1, dicttype);
    _loadpngdevicecont_opcode = op.mark_.padw;
    op = xpost_operator_cons(ctx, "loadpngalphadevice", (Xpost_Op_Func)loadpngalphadevice, 1, 0); INSTALL;
    op = xpost_operator_cons(ctx, "loadpngalphadevicecont", (Xpost_Op_Func)loadpngalphadevicecont, 1, 1, dicttype);
    _loadpngalphadevicecont_opcode = op.mark_.padw;

    return 0;
}

XPAPI void
xpost_dev_png_options_set(Xpost_Context *ctx,
                          int compression_level,
                          int interlaced)
{
    char buf1[32];
    char buf2[32];
    char *defs[2];

    if ((compression_level < 0) || (compression_level > 9))
    {
        XPOST_LOG_ERR("wrong compression level for the PNG device (%d)",
                      compression_level);
        return;
    }

    snprintf(buf1, sizeof(buf1),
             "png_compression_level=%d", compression_level);
    snprintf(buf2, sizeof(buf2),
             "png_interlaced=%d", interlaced ? 1 : 0);
    defs[0] = buf1;
    defs[1] = buf2;
    xpost_add_definitions(ctx, 2, defs);
}

#else /* ! HAVE_LIBPNG */

#include "xpost.h"

XPAPI void
xpost_dev_png_options_set(Xpost_Context *ctx,
                          int compression_level,
                          int interlaced)
{
    (void)ctx;
    (void)compression_level;
    (void)interlaced;
}

#endif
