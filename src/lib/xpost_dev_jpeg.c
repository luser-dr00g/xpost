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

#ifdef HAVE_STDLIB_H
# undef HAVE_STDLIB_H
#endif

#ifdef HAVE_LIBJPEG

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <jpeglib.h>
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
#include "xpost_dev_jpeg.h" /* check prototypes */

typedef struct _JPEG_error_mgr *emptr;
struct _JPEG_error_mgr
{
   struct jpeg_error_mgr pub;
   jmp_buf setjmp_buffer;
};

typedef struct
{
    unsigned char red, green, blue;
} Xpost_Jpeg_Pixel;

typedef struct
{
    int width, height, byte_stride;
    Xpost_Jpeg_Pixel data[1];
} Xpost_Jpeg_Buffer;

typedef struct
{
    int width;
    int height;
    /*
     * add additional members to private struct
     */
    FILE *f;
    Xpost_Jpeg_Buffer *buf;
} PrivateData;

static Xpost_Object namePrivate;
static Xpost_Object namewidth;
static Xpost_Object nameheight;
static Xpost_Object namedotcopydict;
static Xpost_Object namenativecolorspace;
static Xpost_Object nameDeviceRGB;


static unsigned int _create_cont_opcode;

static void
_JPEGFatalErrorHandler(j_common_ptr cinfo)
{
   emptr errmgr;

   errmgr = (emptr) cinfo->err;
   longjmp(errmgr->setjmp_buffer, 1);
   return;
}

static void
_JPEGErrorHandler(j_common_ptr cinfo)
{
   return;
   (void)cinfo;
}

static void
_JPEGErrorHandler2(j_common_ptr cinfo, int msg_level)
{
   return;
   (void)cinfo;
   (void)msg_level;
}

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
    char *filename;
    integer width = w.int_.val;
    integer height = h.int_.val;
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

    /*
     *
     * initialize additional members of private struct
     *
     */

    filename = xpost_device_get_filename(ctx, devdic);
    if (!filename)
    {
        XPOST_LOG_ERR("cannot retrieve JPEG file name");
        return unregistered;
    }

    private.f = fopen(filename, "wb");
    free(filename);
    if (!private.f)
    {
        XPOST_LOG_ERR("cannot retrieve JPEG file name");
        return unregistered;
    }

    /* allocate buffer header and array */
    private.buf = malloc(sizeof(Xpost_Jpeg_Buffer) +
                         sizeof(Xpost_Jpeg_Pixel) * width * height);
    if (!private.buf)
    {
        XPOST_LOG_ERR("cannot allocate buffer memory");
        goto close_file;
    }

    /* save private data struct in string */
    xpost_memory_put(xpost_context_select_memory(ctx, privatestr),
                     xpost_object_get_ent(privatestr), 0,
                     sizeof(private), &private);

    /* return device instance dictionary to ps */
    xpost_stack_push(ctx->lo, ctx->os, devdic);
    return 0;

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
        Xpost_Jpeg_Pixel pixel;
        pixel.blue = blue.int_.val;
        pixel.green = green.int_.val;
        pixel.red = red.int_.val;
        private.buf->data[y.int_.val * private.width + x.int_.val] = pixel;
    }

    /* save private data struct in string */
    xpost_memory_put(xpost_context_select_memory(ctx, privatestr),
                     xpost_object_get_ent(privatestr), 0,
                     sizeof(private), &private);

    return 0;
}

static
int _emit(Xpost_Context *ctx,
          Xpost_Object devdic)
{
    struct jpeg_compress_struct cinfo;
    struct _JPEG_error_mgr jerr;
    Xpost_Object ud;
    Xpost_Object quality_o;
    Xpost_Object privatestr;
    PrivateData private;
    unsigned char *data;
    JSAMPROW *jbuf;
    int quality;

    /* load private data struct from string */
    privatestr = xpost_dict_get(ctx, devdic, namePrivate);
    if (xpost_object_get_type(privatestr) == invalidtype)
        return undefined;
    xpost_memory_get(xpost_context_select_memory(ctx, privatestr),
                     xpost_object_get_ent(privatestr), 0,
                     sizeof(private), &private);

    ud = xpost_stack_bottomup_fetch(ctx->lo, ctx->ds, 2);
    quality_o = xpost_dict_get(ctx, ud, xpost_name_cons(ctx, "jpeg_quality"));

    if (xpost_object_get_type(quality_o) == invalidtype)
        quality = 90;
    else
        quality = quality_o.int_.val;
    XPOST_LOG_INFO("JPEG quality: %d", quality);

    memset(&cinfo, 0, sizeof(cinfo));
    cinfo.err = jpeg_std_error(&(jerr.pub));
    jerr.pub.error_exit = _JPEGFatalErrorHandler;
    jerr.pub.emit_message = _JPEGErrorHandler2;
    jerr.pub.output_message = _JPEGErrorHandler;
    if (setjmp(jerr.setjmp_buffer))
    {
        jpeg_destroy_compress(&cinfo);
        return undefined;
    }
    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, private.f);
    cinfo.image_width = private.width;
    cinfo.image_height = private.height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;
    cinfo.optimize_coding = FALSE;
    cinfo.dct_method = JDCT_ISLOW; /* JDCT_FLOAT JDCT_IFAST(quality loss) */
    if (quality < 60)
        cinfo.dct_method = JDCT_IFAST;
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);
    if (quality >= 90)
    {
        cinfo.comp_info[0].h_samp_factor = 1;
        cinfo.comp_info[0].v_samp_factor = 1;
        cinfo.comp_info[1].h_samp_factor = 1;
        cinfo.comp_info[1].v_samp_factor = 1;
        cinfo.comp_info[2].h_samp_factor = 1;
        cinfo.comp_info[2].v_samp_factor = 1;
    }
    jpeg_start_compress(&cinfo, TRUE);
    data = (unsigned char *)private.buf->data;
    while (cinfo.next_scanline < cinfo.image_height)
    {
        jbuf = (JSAMPROW *) (&data);
        jpeg_write_scanlines(&cinfo, jbuf, 1);
        data += 3 * private.width;
    }
    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

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
    fclose(private.f);

    return 0;
}

/* operator function to instantiate a new window device.
   installed in userdict by calling 'loadXXXdevice'.
 */
static
int newjpegdevice(Xpost_Context *ctx,
                  Xpost_Object width,
                  Xpost_Object height)
{
    Xpost_Object classdic;
    int ret;

    xpost_stack_push(ctx->lo, ctx->os, width);
    xpost_stack_push(ctx->lo, ctx->os, height);
    ret = xpost_op_any_load(ctx, xpost_name_cons(ctx, "jpegDEVICE"));
    if (ret)
        return ret;
    classdic = xpost_stack_topdown_fetch(ctx->lo, ctx->os, 0);
    if (!xpost_stack_push(ctx->lo, ctx->es, xpost_dict_get(ctx, classdic, xpost_name_cons(ctx, "Create"))))
        return execstackoverflow;

    return 0;
}

static
unsigned int _loadjpegdevicecont_opcode;

/* Specializes or sub-classes the PPMIMAGE device class.
   load PPMIMAGE
   load and call ps procedure .copydict which leaves copy on stack
   call loadjpegdevicecont by continuation.
 */
static
int loadjpegdevice(Xpost_Context *ctx)
{
    Xpost_Object classdic;
    int ret;

    ret = xpost_op_any_load(ctx, xpost_name_cons(ctx, "PPMIMAGE"));
    if (ret)
        return ret;
    classdic = xpost_stack_topdown_fetch(ctx->lo, ctx->os, 0);
    if (!xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(_loadjpegdevicecont_opcode)))
        return execstackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->es, xpost_dict_get(ctx, classdic, namedotcopydict)))
        return execstackoverflow;

    return 0;
}

/* replace procedures in the class with newly created special operators.
   defines the device class jpegDEVICE in userdict.
   defines a new operator in userdict: newjpegdevice
 */
static
int loadjpegdevicecont(Xpost_Context *ctx,
                      Xpost_Object classdic)
{
    Xpost_Object userdict;
    Xpost_Object op;
    int ret;

    ret = xpost_dict_put(ctx, classdic, namenativecolorspace, nameDeviceRGB);

    op = xpost_operator_cons(ctx, "jpegCreateCont", (Xpost_Op_Func)_create_cont, 1, 3, integertype, integertype, dicttype);
    _create_cont_opcode = op.mark_.padw;
    op = xpost_operator_cons(ctx, "jpegCreate", (Xpost_Op_Func)_create, 1, 3, integertype, integertype, dicttype);
    ret = xpost_dict_put(ctx, classdic, xpost_name_cons(ctx, "Create"), op);
    if (ret)
        return ret;

    op = xpost_operator_cons(ctx, "jpegPutPix", (Xpost_Op_Func)_putpix, 0, 6,
            numbertype, numbertype, numbertype,
            numbertype, numbertype,
            dicttype);
    ret = xpost_dict_put(ctx, classdic, xpost_name_cons(ctx, "PutPix"), op);
    if (ret)
        return ret;

    op = xpost_operator_cons(ctx, "jpegEmit", (Xpost_Op_Func)_emit, 0, 1, dicttype);
    ret = xpost_dict_put(ctx, classdic, xpost_name_cons(ctx, "Emit"), op);
    if (ret)
        return ret;

    op = xpost_operator_cons(ctx, "jpegDestroy", (Xpost_Op_Func)_destroy, 0, 1, dicttype);
    ret = xpost_dict_put(ctx, classdic, xpost_name_cons(ctx, "Destroy"), op);
    if (ret)
        return ret;

    userdict = xpost_stack_bottomup_fetch(ctx->lo, ctx->ds, 2);

    ret = xpost_dict_put(ctx, userdict, xpost_name_cons(ctx, "jpegDEVICE"), classdic);
    if (ret)
        return ret;

    op = xpost_operator_cons(ctx, "newjpegdevice", (Xpost_Op_Func)newjpegdevice, 1, 2, integertype, integertype);
    ret = xpost_dict_put(ctx, userdict, xpost_name_cons(ctx, "newjpegdevice"), op);
    if (ret)
        return ret;

    return 0;
}

/*
   install the loadXXXdevice which may be called during graphics initialization
   to produce the operator newXXXdevice which instantiates the device dictionary.
*/
int xpost_oper_init_jpeg_device_ops(Xpost_Context *ctx,
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
    op = xpost_operator_cons(ctx, "loadjpegdevice", (Xpost_Op_Func)loadjpegdevice, 1, 0); INSTALL;
    op = xpost_operator_cons(ctx, "loadjpegdevicecont", (Xpost_Op_Func)loadjpegdevicecont, 1, 1, dicttype);
    _loadjpegdevicecont_opcode = op.mark_.padw;

    return 0;
}

XPAPI void
xpost_dev_jpeg_options_set(Xpost_Context *ctx, int quality)
{
    char buf[32];
    char *def[1];

    if ((quality < 0) || (quality > 100))
    {
        XPOST_LOG_ERR("wrong quality value for the JPEG device (%d)",
                      quality);
        return;
    }

    snprintf(buf, sizeof(buf), "jpeg_quality=%d", quality);
    def[0] = buf;
    xpost_add_definitions(ctx, 1, def);
}

#else /* ! HAVE_LIBJPEG */

#include "xpost.h"

XPAPI void
xpost_dev_jpeg_options_set(Xpost_Context *ctx, int quality)
{
    (void)ctx;
    (void)quality;
}

#endif
