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
#include <math.h>
#include <stdio.h>
#include <stdlib.h> /* NULL strtod */
#include <string.h>

#include "xpost_log.h"
#include "xpost_memory.h"
#include "xpost_object.h"
#include "xpost_stack.h"
#include "xpost_matrix.h"

#include "xpost_context.h"
#include "xpost_interpreter.h"
#include "xpost_error.h"
#include "xpost_name.h"
#include "xpost_string.h"
#include "xpost_dict.h"
#include "xpost_array.h"
#include "xpost_operator.h"
#include "xpost_op_matrix.h"

#define RAD_PER_DEG (M_PI / 180.0)

static
Xpost_Object _get_ctm(Xpost_Context *ctx)
{
    Xpost_Object userdict;
    Xpost_Object gd;
    Xpost_Object gs;
    Xpost_Object psctm;

    userdict = xpost_stack_bottomup_fetch(ctx->lo, ctx->ds, 2);
    gd = bdcget(ctx, userdict, consname(ctx, "graphicsdict"));
    gs = bdcget(ctx, gd, consname(ctx, "currgstate"));
    psctm = bdcget(ctx, gs, consname(ctx, "currmatrix"));

    return psctm;
}

static
void _psmat2xmat (Xpost_Context *ctx,
                  Xpost_Object psm,
                  Xpost_Matrix *m)
{
    Xpost_Object arr[6];
    int i;
    xpost_memory_get(xpost_context_select_memory(ctx, psm),
            psm.comp_.ent, 0, sizeof arr, arr);
    for (i=0; i < 6; i++)
    {
        if (xpost_object_get_type(arr[i]) == integertype)
            arr[i] = xpost_cons_real(arr[i].int_.val);
    }
    m->xx = arr[0].real_.val;
    m->yx = arr[1].real_.val;
    m->xy = arr[2].real_.val;
    m->yy = arr[3].real_.val;
    m->xz = arr[4].real_.val;
    m->yz = arr[5].real_.val;

    /*
    Xpost_Object el;
    el = barget(ctx, psm, 0);
    m->xx = xpost_object_get_type(el) == integertype ?  el.int_.val : el.real_.val;
    el = barget(ctx, psm, 1);
    m->yx = xpost_object_get_type(el) == integertype ?  el.int_.val : el.real_.val;
    el = barget(ctx, psm, 2);
    m->xy = xpost_object_get_type(el) == integertype ?  el.int_.val : el.real_.val;
    el = barget(ctx, psm, 3);
    m->yy = xpost_object_get_type(el) == integertype ?  el.int_.val : el.real_.val;
    el = barget(ctx, psm, 4);
    m->xz = xpost_object_get_type(el) == integertype ?  el.int_.val : el.real_.val;
    el = barget(ctx, psm, 5);
    m->yz = xpost_object_get_type(el) == integertype ?  el.int_.val : el.real_.val;
    */
}

static
void _xmat2psmat (Xpost_Context *ctx,
                  Xpost_Matrix *m,
                  Xpost_Object psm)
{
    Xpost_Object arr[6] = {
        xpost_cons_real(m->xx),
        xpost_cons_real(m->yx),
        xpost_cons_real(m->xy),
        xpost_cons_real(m->yy),
        xpost_cons_real(m->xz),
        xpost_cons_real(m->yz)
    };
    xpost_memory_put(xpost_context_select_memory(ctx, psm),
            psm.comp_.ent, 0, sizeof arr, arr);

    /*
    barput(ctx, psm, 0, xpost_cons_real(m->xx));
    barput(ctx, psm, 1, xpost_cons_real(m->yx));
    barput(ctx, psm, 2, xpost_cons_real(m->xy));
    barput(ctx, psm, 3, xpost_cons_real(m->yy));
    barput(ctx, psm, 4, xpost_cons_real(m->xz));
    barput(ctx, psm, 5, xpost_cons_real(m->yz));
    */
}

static int _ident_matrix (Xpost_Context *ctx, Xpost_Object psmat);
static int _default_matrix(Xpost_Context *ctx, Xpost_Object psmat);

static
int _matrix (Xpost_Context *ctx)
{
    Xpost_Object psmat;
    psmat = consbar(ctx, 6);
    return _ident_matrix(ctx, psmat);
}

static
int _ident_matrix (Xpost_Context *ctx,
                  Xpost_Object psmat)
{
    Xpost_Matrix mat;
    xpost_matrix_identity(&mat);
    _xmat2psmat(ctx, &mat, psmat);
    xpost_stack_push(ctx->lo, ctx->os, psmat);
    return 0;
}

static
int _init_matrix (Xpost_Context *ctx)
{
    xpost_stack_push(ctx->lo, ctx->es, xpost_object_cvx(consname(ctx, "setmatrix")));
    xpost_stack_push(ctx->lo, ctx->es, xpost_object_cvx(consname(ctx, "defaultmatrix")));
    xpost_stack_push(ctx->lo, ctx->es, xpost_object_cvx(consname(ctx, "matrix")));
    return 0;
}

static
int _default_matrix(Xpost_Context *ctx,
                   Xpost_Object psmat)
{
    Xpost_Object userdict;
    Xpost_Object devdic;
    Xpost_Object defmat;
    userdict = xpost_stack_bottomup_fetch(ctx->lo, ctx->ds, 2);
    devdic = bdcget(ctx, userdict, consname(ctx, "DEVICE"));
    defmat = bdcget(ctx, devdic, consname(ctx, "defaultmatrix"));
    xpost_stack_push(ctx->lo, ctx->os, defmat);
    xpost_stack_push(ctx->lo, ctx->os, psmat);
    xpost_stack_push(ctx->lo, ctx->es, xpost_object_cvx(consname(ctx, "copy")));
    return 0;
}

static
int _current_matrix (Xpost_Context *ctx,
                    Xpost_Object psmat)
{
    Xpost_Object ctm;
    ctm = _get_ctm(ctx);
    xpost_stack_push(ctx->lo, ctx->os, ctm);
    xpost_stack_push(ctx->lo, ctx->os, psmat);
    xpost_stack_push(ctx->lo, ctx->es, xpost_object_cvx(consname(ctx, "copy")));
    return 0;
}

static
int _set_matrix (Xpost_Context *ctx,
                Xpost_Object psmat)
{
    Xpost_Object ctm;
    ctm = _get_ctm(ctx);
    xpost_stack_push(ctx->lo, ctx->os, psmat);
    xpost_stack_push(ctx->lo, ctx->os, ctm);
    xpost_stack_push(ctx->lo, ctx->es, xpost_object_cvx(consname(ctx, "pop")));
    xpost_stack_push(ctx->lo, ctx->es, xpost_object_cvx(consname(ctx, "copy")));
    return 0;
}

static
int _translate (Xpost_Context *ctx,
                Xpost_Object xt,
                Xpost_Object yt)
{
    Xpost_Matrix mat;
    Xpost_Object psmat;
    if (xpost_object_get_type(xt) == integertype)
        xt = xpost_cons_real(xt.int_.val);
    if (xpost_object_get_type(yt) == integertype)
        yt = xpost_cons_real(yt.int_.val);
    xpost_matrix_translate(&mat, xt.real_.val, yt.real_.val);
    _xmat2psmat(ctx, &mat, psmat);
    xpost_stack_push(ctx->lo, ctx->os, psmat);
    xpost_stack_push(ctx->lo, ctx->es, xpost_object_cvx(consname(ctx, "concat")));
    return 0;
}

static
int _mat_translate (Xpost_Context *ctx,
                    Xpost_Object xt,
                    Xpost_Object yt,
                    Xpost_Object psmat)
{
    Xpost_Matrix mat;
    if (xpost_object_get_type(xt) == integertype)
        xt = xpost_cons_real(xt.int_.val);
    if (xpost_object_get_type(yt) == integertype)
        yt = xpost_cons_real(yt.int_.val);
    xpost_matrix_translate(&mat, xt.real_.val, yt.real_.val);
    _xmat2psmat(ctx, &mat, psmat);
    xpost_stack_push(ctx->lo, ctx->os, psmat);
    return 0;
}

static
int _scale (Xpost_Context *ctx,
            Xpost_Object xs,
            Xpost_Object ys)
{
    Xpost_Matrix mat;
    Xpost_Object psmat;
    if (xpost_object_get_type(xs) == integertype)
        xs = xpost_cons_real(xs.int_.val);
    if (xpost_object_get_type(ys) == integertype)
        ys = xpost_cons_real(ys.int_.val);
    xpost_matrix_scale(&mat, xs.real_.val, ys.real_.val);
    _xmat2psmat(ctx, &mat, psmat);
    xpost_stack_push(ctx->lo, ctx->os, psmat);
    xpost_stack_push(ctx->lo, ctx->es, xpost_object_cvx(consname(ctx, "concat")));
    return 0;
}

static
int _mat_scale (Xpost_Context *ctx,
            Xpost_Object xs,
            Xpost_Object ys,
            Xpost_Object psmat)
{
    Xpost_Matrix mat;
    if (xpost_object_get_type(xs) == integertype)
        xs = xpost_cons_real(xs.int_.val);
    if (xpost_object_get_type(ys) == integertype)
        ys = xpost_cons_real(ys.int_.val);
    xpost_matrix_scale(&mat, xs.real_.val, ys.real_.val);
    _xmat2psmat(ctx, &mat, psmat);
    xpost_stack_push(ctx->lo, ctx->os, psmat);
    return 0;
}

static
int _rotate (Xpost_Context *ctx,
             Xpost_Object angle)
{
    Xpost_Matrix mat;
    Xpost_Object psmat;
    if (xpost_object_get_type(angle) == integertype)
        angle = xpost_cons_real(angle.int_.val);
    xpost_matrix_rotate(&mat, RAD_PER_DEG * angle.real_.val);
    _xmat2psmat(ctx, &mat, psmat);
    xpost_stack_push(ctx->lo, ctx->os, psmat);
    xpost_stack_push(ctx->lo, ctx->es, xpost_object_cvx(consname(ctx, "concat")));
    return 0;
}

static
int _mat_rotate (Xpost_Context *ctx,
                 Xpost_Object angle,
                 Xpost_Object psmat)
{
    Xpost_Matrix mat;
    if (xpost_object_get_type(angle) == integertype)
        angle = xpost_cons_real(angle.int_.val);
    xpost_matrix_rotate(&mat, RAD_PER_DEG * angle.real_.val);
    _xmat2psmat(ctx, &mat, psmat);
    xpost_stack_push(ctx->lo, ctx->os, psmat);
    return 0;
}

static
int _concat (Xpost_Context *ctx,
             Xpost_Object psmat)
{
    Xpost_Matrix mat;
    Xpost_Object psctm;
    Xpost_Matrix ctm;
    Xpost_Matrix result;

    _psmat2xmat(ctx, psmat, &mat);
    //fetch CTM from graphics state
    psctm = _get_ctm(ctx);
    //xpost_matrix_mult
    _psmat2xmat(ctx, psctm, &ctm);
    xpost_matrix_mult(&ctm, &mat, &result);
    //replace CTM
    _xmat2psmat(ctx, &result, psctm);
    return 0;
}

static
int _concat_matrix (Xpost_Context *ctx,
                    Xpost_Object psmat1,
                    Xpost_Object psmat2,
                    Xpost_Object psmat3)
{
    Xpost_Matrix mat1, mat2, mat3;
    _psmat2xmat(ctx, psmat1, &mat1);
    _psmat2xmat(ctx, psmat2, &mat2);
    xpost_matrix_mult(&mat2, &mat1, &mat3);
    _xmat2psmat(ctx, &mat3, psmat3);
    xpost_stack_push(ctx->lo, ctx->os, psmat3);
    return 0;
}

static
int _mat_transform (Xpost_Context *ctx,
                    Xpost_Object x,
                    Xpost_Object y,
                    Xpost_Object psmat)
{
    Xpost_Matrix mat;
    real xres, yres;
    _psmat2xmat(ctx, psmat, &mat);
    xres = mat.xx * x.real_.val + mat.xy * y.real_.val + mat.xz;
    yres = mat.yx * x.real_.val + mat.yy * y.real_.val + mat.yz;
    xpost_stack_push(ctx->lo, ctx->os, xpost_cons_real(xres));
    xpost_stack_push(ctx->lo, ctx->os, xpost_cons_real(yres));
    return 0;
}

static
int _transform (Xpost_Context *ctx,
                Xpost_Object x,
                Xpost_Object y)
{
    Xpost_Object psctm;
    psctm = _get_ctm(ctx);
    return _mat_transform(ctx, x, y, psctm);
}

static
int _mat_dtransform (Xpost_Context *ctx,
                    Xpost_Object x,
                    Xpost_Object y,
                    Xpost_Object psmat)
{
    Xpost_Matrix mat;
    real xres, yres;
    _psmat2xmat(ctx, psmat, &mat);
    xres = mat.xx * x.real_.val + mat.xy * y.real_.val;
    yres = mat.yx * x.real_.val + mat.yy * y.real_.val;
    xpost_stack_push(ctx->lo, ctx->os, xpost_cons_real(xres));
    xpost_stack_push(ctx->lo, ctx->os, xpost_cons_real(yres));
    return 0;
}

static
int _dtransform (Xpost_Context *ctx,
                Xpost_Object x,
                Xpost_Object y)
{
    Xpost_Object psctm;
    psctm = _get_ctm(ctx);
    return _mat_dtransform(ctx, x, y, psctm);
}

static
int _mat_itransform (Xpost_Context *ctx,
                    Xpost_Object x,
                    Xpost_Object y,
                    Xpost_Object psmat)
{
    Xpost_Matrix mat;
    real xres, yres;
    real disc;
    real invdet;
    _psmat2xmat(ctx, psmat, &mat);
    disc = mat.xx * mat.yy - mat.yx * mat.xy;
    if (disc == 0)
        return undefinedresult;
    invdet = 1 / disc;
    xres = (mat.yy * x.real_.val - mat.yx * y.real_.val 
            + mat.xy * mat.xz - mat.yy * mat.xz) * invdet;
    yres = ( -mat.xy * x.real_.val + mat.xx * y.real_.val
            + mat.yx * mat.xz - mat.xx * mat.yz) * invdet;
    xpost_stack_push(ctx->lo, ctx->os, xpost_cons_real(xres));
    xpost_stack_push(ctx->lo, ctx->os, xpost_cons_real(yres));
    return 0;
}

static
int _itransform (Xpost_Context *ctx,
                Xpost_Object x,
                Xpost_Object y)
{
    Xpost_Object psctm;
    psctm = _get_ctm(ctx);
    return _mat_itransform(ctx, x, y, psctm);
}

static
int _mat_idtransform (Xpost_Context *ctx,
                    Xpost_Object x,
                    Xpost_Object y,
                    Xpost_Object psmat)
{
    Xpost_Matrix mat;
    real xres, yres;
    real disc;
    real invdet;
    _psmat2xmat(ctx, psmat, &mat);
    disc = mat.xx * mat.yy - mat.yx * mat.xy;
    if (disc == 0)
        return undefinedresult;
    invdet = 1 / disc;
    xres = (mat.yy * x.real_.val - mat.yx * y.real_.val) * invdet;
    yres = ( -mat.xy * x.real_.val + mat.xx * y.real_.val) * invdet;
    xpost_stack_push(ctx->lo, ctx->os, xpost_cons_real(xres));
    xpost_stack_push(ctx->lo, ctx->os, xpost_cons_real(yres));
    return 0;
}

static
int _idtransform (Xpost_Context *ctx,
                Xpost_Object x,
                Xpost_Object y)
{
    Xpost_Object psctm;
    psctm = _get_ctm(ctx);
    return _mat_idtransform(ctx, x, y, psctm);
}

static
int _invert_matrix (Xpost_Context *ctx,
                    Xpost_Object psmat1,
                    Xpost_Object psmat2)
{
    Xpost_Matrix mat1, mat2;
    real disc;
    real invdet;
    _psmat2xmat(ctx, psmat1, &mat1);
    disc = mat1.xx * mat1.yy - mat1.yx * mat1.xy;
    if (disc == 0)
        return undefinedresult;
    invdet = 1 / disc;
    mat2.xx = mat1.yy * invdet;
    mat2.yx = -mat1.yx * invdet;
    mat2.xy = -mat1.xy * invdet;
    mat2.yy = mat1.xx * invdet;
    mat2.xz = (mat1.xy * mat1.yz - mat1.yy * mat1.xz) * invdet;
    mat2.yz = (mat1.yx * mat1.xz - mat1.xx * mat1.yz) * invdet;
    _xmat2psmat(ctx, &mat2, psmat2);
    return 0;
}


int initopmatrix(Xpost_Context *ctx,
             Xpost_Object sd)
{
    oper *optab;
    Xpost_Object n,op;
    unsigned int optadr;

    assert(ctx->gl->base);
    xpost_memory_table_get_addr(ctx->gl,
            XPOST_MEMORY_TABLE_SPECIAL_OPERATOR_TABLE, &optadr);
    optab = (void *)(ctx->gl->base + optadr);

    op = consoper(ctx, "matrix", _matrix, 1, 0); INSTALL;

    op = consoper(ctx, "initmatrix", _init_matrix, 0, 0); INSTALL;

    op = consoper(ctx, "identmatrix", _ident_matrix, 1, 1, arraytype); INSTALL;
    op = consoper(ctx, "defaultmatrix", _default_matrix, 1, 1, arraytype); INSTALL;

    op = consoper(ctx, "currentmatrix", _current_matrix, 1, 1, arraytype); INSTALL;
    op = consoper(ctx, "setmatrix", _set_matrix, 0, 1, arraytype); INSTALL;

    op = consoper(ctx, "translate", _translate, 0, 2, floattype, floattype); INSTALL;
    op = consoper(ctx, "translate", _mat_translate, 1, 3, floattype, floattype, arraytype); INSTALL;

    op = consoper(ctx, "scale", _scale, 0, 2, floattype, floattype); INSTALL;
    op = consoper(ctx, "scale", _mat_scale, 1, 3, floattype, floattype, arraytype); INSTALL;

    op = consoper(ctx, "rotate", _rotate, 0, 2, floattype, floattype); INSTALL;
    op = consoper(ctx, "rotate", _mat_rotate, 1, 3, floattype, floattype, arraytype); INSTALL;

    op = consoper(ctx, "concat", _concat, 0, 1, arraytype); INSTALL;
    op = consoper(ctx, "concatmatrix", _concat_matrix, 1, 3,
            arraytype, arraytype, arraytype); INSTALL;

    op = consoper(ctx, "transform", _transform, 2, 2, floattype, floattype); INSTALL;
    op = consoper(ctx, "transform", _mat_transform, 2, 3, floattype, floattype, arraytype); INSTALL;
    op = consoper(ctx, "dtransform", _dtransform, 2, 2, floattype, floattype); INSTALL;
    op = consoper(ctx, "dtransform", _mat_dtransform, 2, 3, floattype, floattype, arraytype); INSTALL;
    op = consoper(ctx, "itransform", _itransform, 2, 2, floattype, floattype); INSTALL;
    op = consoper(ctx, "itransform", _mat_itransform, 2, 3, floattype, floattype, arraytype); INSTALL;
    op = consoper(ctx, "idtransform", _idtransform, 2, 2, floattype, floattype); INSTALL;
    op = consoper(ctx, "idtransform", _mat_idtransform, 2, 3, floattype, floattype, arraytype); INSTALL;

    op = consoper(ctx, "invertmatrix", _invert_matrix, 1, 2, arraytype, arraytype); INSTALL;

    /* dumpdic(ctx->gl, sd); fflush(NULL);
    bdcput(ctx, sd, consname(ctx, "mark"), mark); */

    return 0;
}

