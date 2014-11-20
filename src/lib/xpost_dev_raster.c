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

#include <assert.h>
#include <stdlib.h> /* abs */
//#include <stdio.h>
#include <string.h>

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
#include "xpost_dev_raster.h" /* check prototypes */

#define FAST_C_BUFFER

typedef struct
{
    int width, height;
    /*
     * add additional members to private struct
     */
#ifdef FAST_C_BUFFER
    Xpost_Raster_Buffer *buf;
#endif
} PrivateData;

static int _flush (Xpost_Context *ctx, Xpost_Object devdic);


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
int _create (Xpost_Context *ctx,
             Xpost_Object width,
             Xpost_Object height,
             Xpost_Object classdic)
{
    xpost_stack_push(ctx->lo, ctx->os, width);
    xpost_stack_push(ctx->lo, ctx->os, height);
    xpost_stack_push(ctx->lo, ctx->os, classdic);
    xpost_dict_put(ctx, classdic, namewidth, width);
    xpost_dict_put(ctx, classdic, nameheight, height);

    //printf("create\n");
    //fflush(0);
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
int _create_cont (Xpost_Context *ctx,
                  Xpost_Object w,
                  Xpost_Object h,
                  Xpost_Object devdic)
{
    Xpost_Object privatestr;
    PrivateData private;
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

#ifdef FAST_C_BUFFER
    {
        /* allocate buffer header and array */
        private.buf = malloc(sizeof(Xpost_Raster_Buffer) + sizeof(Xpost_Raster_Pixel)*width*height);
    }
#else
    { /*
         initialize the PS-level raster buffer, 
         an array of arrays of ints, each holding a 24-bit rgb value.
         This allows us to re-use most of the PPM base-class functions,
         and just grab the buffer in _emit() which overrides the device's
         /Emit member-function which is called as the action of `showpage`.
       */
        int i, j;
        Xpost_Object imgdata;
        Xpost_Object row;
        Xpost_Object *rowdata;

        //printf("creating row of %d integers\n", width);
        rowdata = malloc(width * sizeof(Xpost_Object));
        for (j = 0; j < width; j++)
        {
            rowdata[j] = xpost_int_cons(0);
        }

        //printf("creating array of %d rows\n", height);
        imgdata = xpost_object_cvlit(xpost_array_cons(ctx, height));
        for (i = 0; i < height; i++)
        {
            row = xpost_object_cvlit(xpost_array_cons(ctx, width));
            xpost_array_put(ctx, imgdata, i, row);
            xpost_memory_put(xpost_context_select_memory(ctx, row), 
                    xpost_object_get_ent(row),
                    0,
                    width * sizeof(Xpost_Object),
                    rowdata);
        }
        xpost_dict_put(ctx, devdic, xpost_name_cons(ctx, "ImgData"), imgdata);

        free(rowdata);
    }
#endif

    /* save private data struct in string */
    xpost_memory_put(xpost_context_select_memory(ctx, privatestr),
            xpost_object_get_ent(privatestr), 0, sizeof private, &private);

    /* return device instance dictionary to ps */
    xpost_stack_push(ctx->lo, ctx->os, devdic);
    return 0;
}

#ifdef FAST_C_BUFFER

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

    {
        Xpost_Raster_Pixel pixel;
        pixel.blue = blue.int_.val;
        pixel.green = green.int_.val;
        pixel.red = red.int_.val;
        private.buf->data[y.int_.val * private.buf->width + x.int_.val] = pixel;
    }

    /* save private data struct in string */
    xpost_memory_put(xpost_context_select_memory(ctx, privatestr),
            xpost_object_get_ent(privatestr), 0, sizeof private, &private);

    return 0;
}

#endif

static
int _flush (Xpost_Context *ctx,
           Xpost_Object devdic)
{
    Xpost_Object privatestr;
    PrivateData private;

    /* load private data struct from string */
    privatestr = xpost_dict_get(ctx, devdic, namePrivate);
    if (xpost_object_get_type(privatestr) == invalidtype)
        return undefined;
    xpost_memory_get(xpost_context_select_memory(ctx, privatestr),
            xpost_object_get_ent(privatestr), 0, sizeof private, &private);

    return 0;
}


static
int _emit (Xpost_Context *ctx,
           Xpost_Object devdic)
{
    Xpost_Object privatestr;
    PrivateData private;
    Xpost_Object imgdata;

    unsigned char *data;
    int stride;
    int height;

    /* load private data struct from string */
    privatestr = xpost_dict_get(ctx, devdic, namePrivate);
    if (xpost_object_get_type(privatestr) == invalidtype)
        return undefined;
    xpost_memory_get(xpost_context_select_memory(ctx, privatestr),
            xpost_object_get_ent(privatestr), 0, sizeof private, &private);

    stride = private.width;
    height = private.height;

#ifdef FAST_C_BUFFER
    data = (unsigned char *)private.buf->data;
#else

    data = malloc(stride * height * 3); 
    imgdata = xpost_dict_get(ctx, devdic, xpost_name_cons(ctx, "ImgData"));
    if (xpost_object_get_type(imgdata) == invalidtype)
        return undefined;

    {
        int i,j;
        Xpost_Object row;
        Xpost_Object *rowdata;
        unsigned int rowaddr;
        Xpost_Memory_File *mem;
        unsigned char *iter = data;

        mem = xpost_context_select_memory(ctx, imgdata);

        for (i=0; i < height; i++)
        {
            row = xpost_array_get_memory(mem, imgdata, i);
            //row = xpost_array_get(ctx, imgdata, i);
            xpost_memory_table_get_addr(mem, xpost_object_get_ent(row), &rowaddr);
            rowdata = (Xpost_Object *)(mem->base + rowaddr);
            //printf("%d\n", i);

            for (j=0; j < stride; j++)
            {
                unsigned int val;
                val = rowdata[j].int_.val; /* r|g|b 0x00RRGGGBB */
                *iter++ = (val) & 0xFF;     /* b */
                *iter++ = (val>>8) & 0xFF;  /* g */
                *iter++ = (val>>16) & 0xFF; /* r */
            }
        }
    }
#endif

    /*pass data back to client application */
    {
        Xpost_Object sd, outbufstr;
        sd = xpost_stack_bottomup_fetch(ctx->lo, ctx->ds, 0);
        outbufstr = xpost_dict_get(ctx, sd, xpost_name_cons(ctx, "OutputBufferOut"));
        if (xpost_object_get_type(outbufstr) == stringtype){
            unsigned char **outbuf;
            memcpy(&outbuf, xpost_string_get_pointer(ctx, outbufstr), sizeof(outbuf));
            *outbuf = data;
        }
    }

    return 0;
}



/* operator function to instantiate a new window device.
   installed in userdict by calling 'loadXXXdevice'.
 */
static
int newrasterdevice (Xpost_Context *ctx,
                  Xpost_Object width,
                  Xpost_Object height)
{
    Xpost_Object classdic;
    int ret;

    xpost_stack_push(ctx->lo, ctx->os, width);
    xpost_stack_push(ctx->lo, ctx->os, height);
    ret = xpost_op_any_load(ctx, xpost_name_cons(ctx, "rasterDEVICE"));
    if (ret)
        return ret;
    classdic = xpost_stack_topdown_fetch(ctx->lo, ctx->os, 0);
    if (!xpost_stack_push(ctx->lo, ctx->es, xpost_dict_get(ctx, classdic, xpost_name_cons(ctx, "Create"))))
        return execstackoverflow;

    return 0;
}

static
unsigned int _loadrasterdevicecont_opcode;

/* Specializes or sub-classes the PPMIMAGE device class.
   load PPMIMAGE
   load and call ps procedure .copydict which leaves copy on stack
   call loadrasterdevicecont by continuation.
 */
static
int loadrasterdevice (Xpost_Context *ctx)
{
    Xpost_Object classdic;
    int ret;

    ret = xpost_op_any_load(ctx, xpost_name_cons(ctx, "PPMIMAGE"));
    if (ret)
        return ret;
    classdic = xpost_stack_topdown_fetch(ctx->lo, ctx->os, 0);
    if (!xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(_loadrasterdevicecont_opcode)))
        return execstackoverflow;
    if (!xpost_stack_push(ctx->lo, ctx->es, xpost_dict_get(ctx, classdic, namedotcopydict)))
        return execstackoverflow;

    return 0;
}

/* replace procedures in the class with newly created special operators.
   defines the device class rasterDEVICE in userdict.
   defines a new operator in userdict: newrasterdevice
 */
static
int loadrasterdevicecont (Xpost_Context *ctx,
                       Xpost_Object classdic)
{
    Xpost_Object userdict;
    Xpost_Object op;
    int ret;

    ret = xpost_dict_put(ctx, classdic, namenativecolorspace, nameDeviceRGB);

    op = xpost_operator_cons(ctx, "rasterCreateCont", (Xpost_Op_Func)_create_cont, 1, 3, integertype, integertype, dicttype);
    _create_cont_opcode = op.mark_.padw;
    op = xpost_operator_cons(ctx, "rasterCreate", (Xpost_Op_Func)_create, 1, 3, integertype, integertype, dicttype);
    ret = xpost_dict_put(ctx, classdic, xpost_name_cons(ctx, "Create"), op);
    if (ret)
        return ret;

#ifdef FAST_C_BUFFER
    op = xpost_operator_cons(ctx, "rasterPutPix", (Xpost_Op_Func)_putpix, 0, 6,
            numbertype, numbertype, numbertype,
            numbertype, numbertype,
            dicttype);
    ret = xpost_dict_put(ctx, classdic, xpost_name_cons(ctx, "PutPix"), op);
    if (ret)
        return ret;
#endif

    op = xpost_operator_cons(ctx, "rasterEmit", (Xpost_Op_Func)_emit, 0, 1, dicttype);
    ret = xpost_dict_put(ctx, classdic, xpost_name_cons(ctx, "Emit"), op);
    if (ret)
        return ret;

    op = xpost_operator_cons(ctx, "rasterFlush", (Xpost_Op_Func)_flush, 0, 1, dicttype);
    ret = xpost_dict_put(ctx, classdic, xpost_name_cons(ctx, "Flush"), op);
    if (ret)
        return ret;

    userdict = xpost_stack_bottomup_fetch(ctx->lo, ctx->ds, 2);

    ret = xpost_dict_put(ctx, userdict, xpost_name_cons(ctx, "rasterDEVICE"), classdic);
    if (ret)
        return ret;

    op = xpost_operator_cons(ctx, "newrasterdevice", (Xpost_Op_Func)newrasterdevice, 1, 2, integertype, integertype);
    ret = xpost_dict_put(ctx, userdict, xpost_name_cons(ctx, "newrasterdevice"), op);
    if (ret)
        return ret;

    return 0;
}

/*
   install the loadXXXdevice which may be called during graphics initialization
   to produce the operator newXXXdevice which instantiates the device dictionary.
*/
int xpost_oper_init_raster_device_ops (Xpost_Context *ctx,
                Xpost_Object sd)
{
    unsigned int optadr;
    Xpost_Operator *optab;
    Xpost_Object n,op;

    /* factor-out name lookups from the operators (optimization) */
    if (xpost_object_get_type(namePrivate = xpost_name_cons(ctx, "Private")) == invalidtype)
        return VMerror;
    if (xpost_object_get_type(namewidth = xpost_name_cons(ctx, "width")) == invalidtype)
        return VMerror;
    if (xpost_object_get_type(nameheight = xpost_name_cons(ctx, "height")) == invalidtype)
        return VMerror;
    if (xpost_object_get_type(namedotcopydict = xpost_name_cons(ctx, ".copydict")) == invalidtype)
        return VMerror;
    if (xpost_object_get_type(namenativecolorspace = xpost_name_cons(ctx, "nativecolorspace")) == invalidtype)
        return VMerror;
    if (xpost_object_get_type(nameDeviceRGB = xpost_name_cons(ctx, "DeviceRGB")) == invalidtype)
        return VMerror;

    xpost_memory_table_get_addr(ctx->gl,
            XPOST_MEMORY_TABLE_SPECIAL_OPERATOR_TABLE, &optadr);
    optab = (Xpost_Operator *)(ctx->gl->base + optadr);
    op = xpost_operator_cons(ctx, "loadrasterdevice", (Xpost_Op_Func)loadrasterdevice, 1, 0); INSTALL;
    op = xpost_operator_cons(ctx, "loadrasterdevicecont", (Xpost_Op_Func)loadrasterdevicecont, 1, 1, dicttype);
    _loadrasterdevicecont_opcode = op.mark_.padw;

    return 0;
}
