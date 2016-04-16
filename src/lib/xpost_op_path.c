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

#define _USE_MATH_DEFINES /* needed for M_PI with Visual Studio */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "xpost.h"
#include "xpost_log.h"
#include "xpost_main.h"
#include "xpost_memory.h"
#include "xpost_object.h"
#include "xpost_stack.h"
#include "xpost_context.h"
#include "xpost_error.h"
#include "xpost_name.h"
#include "xpost_string.h"
#include "xpost_array.h"
#include "xpost_dict.h"
#include "xpost_matrix.h"

#include "xpost_operator.h"
#include "xpost_op_dict.h"
#include "xpost_op_path.h"

#undef y0
#undef y1

/*
   % path == <<
   %             0 << subpath0 >>
   %             1 << %subpath1
   %                   0  << elem0 /move >> %first elem must be /move
   %                   1  << elem1 >>
   %               >>
   %         >>
   % A /move element will always start a new subpath
   % Any other element appends to the last subpath
 */

//#define RAD_PER_DEG (M_PI / 180.0)
#define RAD_PER_DEG (0.0174533)

/*name objects*/
static Xpost_Object namegraphicsdict;
static Xpost_Object namecurrgstate;
static Xpost_Object namecurrpath;
static Xpost_Object namecmd;
static Xpost_Object namedata;
static Xpost_Object namemove;
static Xpost_Object nameline;
static Xpost_Object namecurve;
static Xpost_Object nameclose;

/*opcodes*/
static unsigned int _currentpoint_opcode;
static unsigned int _moveto_opcode;
static unsigned int _moveto_cont_opcode;
static unsigned int _rmoveto_cont_opcode;
static unsigned int _lineto_opcode;
static unsigned int _lineto_cont_opcode;
static unsigned int _rlineto_cont_opcode;
static unsigned int _curveto_opcode;
static unsigned int _curveto_cont1_opcode;
static unsigned int _curveto_cont2_opcode;
static unsigned int _curveto_cont3_opcode;
static unsigned int _rcurveto_cont_opcode;

/*matrices*/
static Xpost_Object _mat;
static Xpost_Object _mat1;

static
int _newpath(Xpost_Context *ctx)
{
    Xpost_Object gd, gstate;
    int ret;

    /* graphicsdict /currgstate get /currpath 1 dict put */
    ret = xpost_op_any_load(ctx, namegraphicsdict);
    if (ret) return ret;
    gd = xpost_stack_pop(ctx->lo, ctx->os);
    gstate = xpost_dict_get(ctx, gd, namecurrgstate);
    ret = xpost_dict_put(ctx, gstate,
                         namecurrpath,
                         xpost_dict_cons(ctx, 1));
    if (ret) return ret;
    return 0;
}

static
Xpost_Object _cpath(Xpost_Context *ctx)
{
    Xpost_Object gd, gstate, path;
    int ret;

    /* graphicsdict /currgstate get /currpath get */
    ret = xpost_op_any_load(ctx, namegraphicsdict);
    if (ret) return invalid;
    gd = xpost_stack_pop(ctx->lo, ctx->os);
    if (xpost_object_get_type(gd) == invalidtype)
        return invalid;
    gstate = xpost_dict_get(ctx, gd, namecurrgstate);
    if (xpost_object_get_type(gstate) == invalidtype)
        return invalid;
    path = xpost_dict_get(ctx, gstate, namecurrpath);
    return path;
}

int _currentpoint(Xpost_Context *ctx)
{
    Xpost_Object path, subpath, elem, data;
    int pathlen, subpathlen, datalen;

    /*
    cpath dup length 0 eq {
        pop /currentpoint cvx /nocurrentpoint signalerror
    }{
        dup length 1 sub get % last-subpath
        dup length 1 sub get % last-elem
        /data get dup length 2 sub 2 getinterval % last data pair
        aload pop itransform
    } ifelse
    */
    path = _cpath(ctx);
    if (xpost_object_get_type(path) == invalidtype)
        return nocurrentpoint;
    pathlen = xpost_dict_length_memory(xpost_context_select_memory(ctx, path), path);
    if (pathlen == 0)
        return nocurrentpoint;
    subpath = xpost_dict_get(ctx, path, xpost_int_cons(pathlen - 1));
    subpathlen = xpost_dict_length_memory(xpost_context_select_memory(ctx, subpath), subpath);
    elem = xpost_dict_get(ctx, subpath, xpost_int_cons(subpathlen - 1));
    data = xpost_dict_get(ctx, elem, namedata);
    datalen = data.comp_.sz;
    xpost_stack_push(ctx->lo, ctx->os, xpost_array_get(ctx, data, datalen - 2));
    xpost_stack_push(ctx->lo, ctx->os, xpost_array_get(ctx, data, datalen - 1));
    xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(ctx->opcode_shortcuts.itransform));

    return 0;
}

static
int _addtopath(Xpost_Context *ctx, Xpost_Object elem, Xpost_Object path)
{
    Xpost_Object subpath, cmd, lastelem;
    int pathlen;

    /*
    dup length 0 eq {                       % elem path
        1 index /cmd get /move eq {         % elem path
            %(New Path)=
            << 0 4 3 roll >> % new subpath  % <path> <subpath>
            0 exch put                      %
        }{
            /addtopath cvx /nocurrentpoint signalerror
        } ifelse
    }{                                  % elem path
        1 index /cmd get /move eq {     % elem path
            dup dup length 1 sub get    % elem path last-subpath
            dup length 1 sub get        % elem path last-elem-of-last-subpath
            dup /cmd get /move eq { % elem path last-elem
                %(Merge /move)=
                3 1 roll pop            % last-elem elem
                /data get /data exch put
            }{                          % elem path last-elem
                %(New subpath)=
                pop                     % elem path
                dup length 3 2 roll     % <path> n <elem>
                << 0 3 2 roll >>        % <path> n <<0 <elem>>>        % new subpath
                %pstack()=
                put
            } ifelse
        }{                              % elem path
            %(Append elem)=
            dup length 1 sub get        % elem last-subpath
            dup length                  % elem last-subpath key
            3 2 roll
            %pstack()=
            put
        } ifelse
    } ifelse
    */
    pathlen = xpost_dict_length_memory(xpost_context_select_memory(ctx, path), path);
    if (pathlen == 0)
    {
        cmd = xpost_dict_get(ctx, elem, namecmd);
        if (xpost_dict_compare_objects(ctx, cmd, namemove) == 0)
        {
            /* New Path */
            subpath = xpost_dict_cons(ctx, 1);
            xpost_dict_put(ctx, subpath, xpost_int_cons(0), elem);
            xpost_dict_put(ctx, path, xpost_int_cons(0), subpath);
        }
        else
        {
            return nocurrentpoint;
        }
    }
    else
    {
        cmd = xpost_dict_get(ctx, elem, namecmd);
        if (xpost_dict_compare_objects(ctx, cmd, namemove) == 0)
        {
            int subpathlen;
            subpath = xpost_dict_get(ctx, path, xpost_int_cons(pathlen - 1));
            subpathlen = xpost_dict_length_memory(xpost_context_select_memory(ctx, subpath), subpath);
            lastelem = xpost_dict_get(ctx, subpath, xpost_int_cons(subpathlen - 1));
            cmd = xpost_dict_get(ctx, lastelem, namecmd);
            if (xpost_dict_compare_objects(ctx, cmd, namemove) == 0)
            {
                /* Merge "move" */
                Xpost_Object data;
                data = xpost_dict_get(ctx, elem, namedata);
                xpost_dict_put(ctx, lastelem, namedata, data);
            }
            else
            {
                /* New Sub-path */
                subpath = xpost_dict_cons(ctx, 1);
                xpost_dict_put(ctx, subpath, xpost_int_cons(0), elem);
                xpost_dict_put(ctx, path, xpost_int_cons(pathlen), subpath);
            }
        }
        else
        {
            /* Append elem */
            int subpathlen;
            subpath = xpost_dict_get(ctx, path, xpost_int_cons(pathlen - 1));
            subpathlen = xpost_dict_length_memory(xpost_context_select_memory(ctx, subpath), subpath);
            xpost_dict_put(ctx, subpath, xpost_int_cons(subpathlen), elem);
        }
    }
    return 0;
}

static
int _moveto(Xpost_Context *ctx, Xpost_Object x, Xpost_Object y)
{
    xpost_stack_push(ctx->lo, ctx->os, x);
    xpost_stack_push(ctx->lo, ctx->os, y);
    xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(_moveto_cont_opcode));
    xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(ctx->opcode_shortcuts.transform));
    return 0;
}

static
int _moveto_cont(Xpost_Context *ctx, Xpost_Object x, Xpost_Object y)
{
    Xpost_Object data, elem;
    data = xpost_object_cvlit(xpost_array_cons(ctx, 2));
    xpost_array_put(ctx, data, 0, x);
    xpost_array_put(ctx, data, 1, y);
    elem = xpost_dict_cons(ctx, 2);
    xpost_dict_put(ctx, elem, namecmd, namemove);
    xpost_dict_put(ctx, elem, namedata, data);
    return _addtopath(ctx, elem, _cpath(ctx));
}

static
int _rmoveto(Xpost_Context *ctx, Xpost_Object dx, Xpost_Object dy)
{
    xpost_stack_push(ctx->lo, ctx->os, dx);
    xpost_stack_push(ctx->lo, ctx->os, dy);
    xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(_rmoveto_cont_opcode));
    xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(_currentpoint_opcode));
    return 0;
}

static
int _rmoveto_cont(Xpost_Context *ctx,
                  Xpost_Object dx, Xpost_Object dy,
                  Xpost_Object x, Xpost_Object y)
{
    x.real_.val += dx.real_.val;
    y.real_.val += dy.real_.val;
    return _moveto(ctx, x, y);
}

static
int _lineto(Xpost_Context *ctx, Xpost_Object x, Xpost_Object y)
{
    xpost_stack_push(ctx->lo, ctx->os, x);
    xpost_stack_push(ctx->lo, ctx->os, y);
    xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(_lineto_cont_opcode));
    xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(ctx->opcode_shortcuts.transform));
    return 0;
}

static
int _lineto_cont(Xpost_Context *ctx, Xpost_Object x, Xpost_Object y)
{
    Xpost_Object data, elem;
    data = xpost_object_cvlit(xpost_array_cons(ctx, 2));
    xpost_array_put(ctx, data, 0, x);
    xpost_array_put(ctx, data, 1, y);
    elem = xpost_dict_cons(ctx, 2);
    xpost_dict_put(ctx, elem, namecmd, nameline);
    xpost_dict_put(ctx, elem, namedata, data);
    return _addtopath(ctx, elem, _cpath(ctx));
}

static
int _rlineto(Xpost_Context *ctx, Xpost_Object dx, Xpost_Object dy)
{
    xpost_stack_push(ctx->lo, ctx->os, dx);
    xpost_stack_push(ctx->lo, ctx->os, dy);
    xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(_rlineto_cont_opcode));
    xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(_currentpoint_opcode));
    return 0;
}

static
int _rlineto_cont(Xpost_Context *ctx,
                  Xpost_Object dx, Xpost_Object dy,
                  Xpost_Object x, Xpost_Object y)
{
    x.real_.val += dx.real_.val;
    y.real_.val += dy.real_.val;
    return _lineto(ctx, x, y);
}

static
int _curveto(Xpost_Context *ctx,
             Xpost_Object x1, Xpost_Object y1,
             Xpost_Object x2, Xpost_Object y2,
             Xpost_Object x3, Xpost_Object y3)
{
    xpost_stack_push(ctx->lo, ctx->os, x1);
    xpost_stack_push(ctx->lo, ctx->os, y1);
    xpost_stack_push(ctx->lo, ctx->os, x2);
    xpost_stack_push(ctx->lo, ctx->os, y2);
    xpost_stack_push(ctx->lo, ctx->os, x3);
    xpost_stack_push(ctx->lo, ctx->os, y3);
    xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(_curveto_cont1_opcode));
    xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(ctx->opcode_shortcuts.transform));
    return 0;
}

static
int _curveto_cont1(Xpost_Context *ctx,
                   Xpost_Object x1, Xpost_Object y1,
                   Xpost_Object x2, Xpost_Object y2,
                   Xpost_Object X3, Xpost_Object Y3)
{
    xpost_stack_push(ctx->lo, ctx->os, X3);
    xpost_stack_push(ctx->lo, ctx->os, Y3);
    xpost_stack_push(ctx->lo, ctx->os, x1);
    xpost_stack_push(ctx->lo, ctx->os, y1);
    xpost_stack_push(ctx->lo, ctx->os, x2);
    xpost_stack_push(ctx->lo, ctx->os, y2);
    xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(_curveto_cont2_opcode));
    xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(ctx->opcode_shortcuts.transform));
    return 0;
}

static
int _curveto_cont2(Xpost_Context *ctx,
                   Xpost_Object X3, Xpost_Object Y3,
                   Xpost_Object x1, Xpost_Object y1,
                   Xpost_Object X2, Xpost_Object Y2)
{
    xpost_stack_push(ctx->lo, ctx->os, X2);
    xpost_stack_push(ctx->lo, ctx->os, Y2);
    xpost_stack_push(ctx->lo, ctx->os, X3);
    xpost_stack_push(ctx->lo, ctx->os, Y3);
    xpost_stack_push(ctx->lo, ctx->os, x1);
    xpost_stack_push(ctx->lo, ctx->os, y1);
    xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(_curveto_cont3_opcode));
    xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(ctx->opcode_shortcuts.transform));
    return 0;
}

static
int _curveto_cont3(Xpost_Context *ctx,
                   Xpost_Object X2, Xpost_Object Y2,
                   Xpost_Object X3, Xpost_Object Y3,
                   Xpost_Object X1, Xpost_Object Y1)
{
    Xpost_Object data, elem;
    data = xpost_object_cvlit(xpost_array_cons(ctx, 6));
    xpost_array_put(ctx, data, 0, X1);
    xpost_array_put(ctx, data, 1, Y1);
    xpost_array_put(ctx, data, 2, X2);
    xpost_array_put(ctx, data, 3, Y2);
    xpost_array_put(ctx, data, 4, X3);
    xpost_array_put(ctx, data, 5, Y3);
    elem = xpost_dict_cons(ctx, 2);
    xpost_dict_put(ctx, elem, namecmd, namecurve);
    xpost_dict_put(ctx, elem, namedata, data);
    return _addtopath(ctx, elem, _cpath(ctx));
}

static
int _rcurveto(Xpost_Context *ctx,
              Xpost_Object x1, Xpost_Object y1,
              Xpost_Object x2, Xpost_Object y2,
              Xpost_Object x3, Xpost_Object y3)
{
    xpost_stack_push(ctx->lo, ctx->os, x1);
    xpost_stack_push(ctx->lo, ctx->os, y1);
    xpost_stack_push(ctx->lo, ctx->os, x2);
    xpost_stack_push(ctx->lo, ctx->os, y2);
    xpost_stack_push(ctx->lo, ctx->os, x3);
    xpost_stack_push(ctx->lo, ctx->os, y3);
    xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(_rcurveto_cont_opcode));
    xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(_currentpoint_opcode));
    return 0;
}

static
int _rcurveto_cont(Xpost_Context *ctx,
                   Xpost_Object x1, Xpost_Object y1,
                   Xpost_Object x2, Xpost_Object y2,
                   Xpost_Object x3, Xpost_Object y3,
                   Xpost_Object x, Xpost_Object y)
{
    x1.real_.val += x.real_.val;
    y1.real_.val += y.real_.val;
    x2.real_.val += x.real_.val;
    y2.real_.val += y.real_.val;
    x3.real_.val += x.real_.val;
    y3.real_.val += y.real_.val;
    return _curveto(ctx, x1, y1, x2, y2, x3, y3);
}

static
int _closepath(Xpost_Context *ctx)
{
    Xpost_Object path;
    int pathlen;

    /*
    cpath length 0 gt {
        cpath dup length 1 sub get % subpath
        dup dup length 1 sub get % subpath last-elem
        /cmd get /close eq { % subpath
            pop
        }{                   % subpath
            0 get /data get % subpath [data]
            << /data 3 2 roll /cmd /close >> cpath addtopath
        } ifelse
    } if
    */
    path = _cpath(ctx);
    pathlen = xpost_dict_length_memory(xpost_context_select_memory(ctx, path), path);
    if (pathlen)
    {
        Xpost_Object subpath, lastelem, firstelem, cmd, elem, data;
        int subpathlen;

        subpath = xpost_dict_get(ctx, path, xpost_int_cons(pathlen - 1));
        subpathlen = xpost_dict_length_memory(xpost_context_select_memory(ctx, subpath), subpath);
        lastelem = xpost_dict_get(ctx, subpath, xpost_int_cons(subpathlen - 1));
        cmd = xpost_dict_get(ctx, lastelem, namecmd);
        if (xpost_dict_compare_objects(ctx, cmd, nameclose) != 0)
        {
            firstelem = xpost_dict_get(ctx, subpath, xpost_int_cons(0));
            data = xpost_dict_get(ctx, firstelem, namedata);
            elem = xpost_dict_cons(ctx, 2);
            xpost_dict_put(ctx, elem, namecmd, nameclose);
            xpost_dict_put(ctx, elem, namedata, data);
            return _addtopath(ctx, elem, _cpath(ctx));
        }
    }
    return 0;
}

/*
% packs the center-point, radius and center-angle in a matrix
% then performs the simpler task of calculating a bezier
% for the arc that is symmetrical about the x-axis
% formula derived from http://www.tinaja.com/glib/bezarc1.pdf
/arcbez { % draw single bezier % x y r angle1 angle2  .  x1 y1 x2 y2 x3 y3 x0 y0
    DICT
    %5 dict
    begin
    %/mat matrix def
    5 3 roll mat translate pop                         % r angle1 angle2
    3 2 roll dup mat1 scale mat mat concatmatrix pop % angle1 angle2
    2 copy exch sub /da exch def                       % da=a2-a1
    add 2 div mat1 rotate mat mat concatmatrix pop
    /da_2 da 2 div def
    /sin_a da_2 sin def
    /cos_a da_2 cos def
    4 cos_a sub 3 div % x1
    1 cos_a sub cos_a 3 sub mul
    3 sin_a mul div   % x1 y1
    neg
    1 index           % x1 y1 x2(==x1)
    1 index neg       % x1 y1 x2 y2(==-y1)
    cos_a sin_a neg   % x1 y1 x2 y2 x3 y3
    cos_a sin_a       %               ... x0 y0
    4 { 8 2 roll mat transform } repeat
    %pstack()=
    end
}
dup 0 10 dict
    dup /mat matrix put
    dup /mat1 matrix put
put
bind
def
*/


static
void _transform(Xpost_Matrix mat, real x, real y, real *xres, real *yres)
{
    *xres = mat.xx * x + mat.xy * y + mat.xz;
    *yres = mat.yx * x + mat.yy * y + mat.yz;
}

static
Xpost_Object _arc_start_proc;

static
int _arcbez(Xpost_Context *ctx,
            Xpost_Object x, Xpost_Object y, Xpost_Object r,
            Xpost_Object angle1, Xpost_Object angle2)
{
    Xpost_Matrix mat1, mat2, mat3;
    real da_2, sin_a, cos_a;
    real x0, y0, x1, y1, x2, y2, x3, y3;

    xpost_matrix_scale(&mat1, r.real_.val, r.real_.val);
    xpost_matrix_translate(&mat2, x.real_.val, y.real_.val);
    xpost_matrix_mult(&mat2, &mat1, &mat3);
    xpost_matrix_rotate(&mat2, (real)(((angle1.real_.val + angle2.real_.val) / 2.0) * RAD_PER_DEG));
    xpost_matrix_mult(&mat3, &mat2, &mat1);

    da_2 = (real)(((angle2.real_.val - angle1.real_.val) / 2.0) * RAD_PER_DEG);
    sin_a = (real)sin(da_2);
    cos_a = (real)cos(da_2);
    x0 = cos_a;
    y0 = sin_a;
    x1 = (real)((4 - cos_a) / 3.0);
    y1 = - (((1 - cos_a) * (cos_a - 3)) / (3 * sin_a));
    x2 = x1;
    y2 = -y1;
    x3 = cos_a;
    y3 = -sin_a;
    _transform(mat1, x0, y0, &x0, &y0);
    _transform(mat1, x1, y1, &x1, &y1);
    _transform(mat1, x2, y2, &x2, &y2);
    _transform(mat1, x3, y3, &x3, &y3);
    xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons(x1));
    xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons(y1));
    xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons(x2));
    xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons(y2));
    xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons(x3));
    xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons(y3));
    xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons(x0));
    xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons(y0));
    return 0;
}

static
int _arc(Xpost_Context *ctx,
         Xpost_Object x, Xpost_Object y, Xpost_Object r,
         Xpost_Object angle1, Xpost_Object angle2)
{
    double a1 = angle1.real_.val;
    double a2 = angle2.real_.val;
    while (a2 < a1)
    {
        double t;
        t = a2 + 360;
        a2 = t;
    }
    if ((a2 - a1) > 90)
    {
        _arc(ctx, x, y, r, xpost_real_cons(a1), xpost_real_cons((real)(a2 - ((a2 - a1)/2.0))));
        _arc(ctx, x, y, r, xpost_real_cons((real)(a1 + ((a2 - a1)/2.0))), xpost_real_cons(a2));
    }
    else
    {
        //Xpost_Object path = _cpath(ctx);
        //int pathlen = xpost_dict_length_memory(xpost_context_select_memory(ctx, path), path);
        _arcbez(ctx, x, y, r, xpost_real_cons(a1), xpost_real_cons(a2));
        xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(_curveto_opcode));
        xpost_stack_push(ctx->lo, ctx->es, _arc_start_proc);
        /*
        if (pathlen)
            xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(_lineto_opcode));
        else
            xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(_moveto_opcode));
            */
    }
    return 0;
}

static
int _arcn(Xpost_Context *ctx,
          Xpost_Object x, Xpost_Object y, Xpost_Object r,
          Xpost_Object angle1, Xpost_Object angle2)
{
    real a1 = angle1.real_.val;
    real a2 = angle2.real_.val;
    while (a2 > a1)
    {
        double t;
        t = a2 - 360;
        a2 = t;
    }
    if ((a1 - a2) > 90)
    {
        _arcn(ctx, x, y, r, xpost_real_cons(a1), xpost_real_cons(a2 + (real)((a1 - a2)/2.0)));
        _arcn(ctx, x, y, r, xpost_real_cons(a1 - (real)((a1 - a2)/2.0)), xpost_real_cons(a2));
    }
    else
    {
        //Xpost_Object path = _cpath(ctx);
        //int pathlen = xpost_dict_length_memory(xpost_context_select_memory(ctx, path), path);
        _arcbez(ctx, x, y, r, xpost_real_cons(a1), xpost_real_cons(a2));
        xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(_curveto_opcode));
        xpost_stack_push(ctx->lo, ctx->es, _arc_start_proc);
        /*
        if (pathlen)
            xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(_lineto_opcode));
        else
            xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(_moveto_opcode));
            */
    }
    return 0;
}

#define NUM(x) (xpost_object_get_type(x)==realtype?x.real_.val:(real)x.int_.val)

static
int _chopcurve(Xpost_Context *ctx,
               real x0, real y0,
               real x1, real y1,
               real x2, real y2,
               real x3, real y3,
               Xpost_Object flat)
{
    real x01, y01, x12, y12, x23, y23,
         x012, y012, x123, y123,
         x0123, y0123;
    real x03, y03;

    //printf("%f %f %f %f %f %f %f %f\n", x0, y0, x1, y1, x2, y2, x3, y3);

#define UGLY
#ifdef UGLY

#define MEDIAN(x, y, xA, yA, xB, yB) \
    x = (real)(((xA)+(xB))/2.0); \
    y = (real)(((yA)+(yB))/2.0);

    MEDIAN(x01, y01, x0, y0, x1, y1)
    MEDIAN(x12, y12, x1, y1, x2, y2)
    MEDIAN(x23, y23, x2, y2, x3, y3)
    MEDIAN(x012, y012, x01, y01, x12, y12)
    MEDIAN(x123, y123, x12, y12, x23, y23)
    MEDIAN(x0123, y0123, x012, y012, x123, y123)

    MEDIAN(x03, y03, x0, y0, x3, y3)

#elif defined UNREADABLE

#define MED(Z, A, B) \
    x##Z = ((x##A)+(x##B))/2.0; \
    y##Z = ((y##A)+(y##B))/2.0;

    MED(01, 0, 1)
    MED(12, 1, 2)
    MED(23, 2, 3)
    MED(012, 01, 12)
    MED(123, 12, 23)
    MED(0123, 012, 123)

    MED(03, 0, 3)

#endif
#undef UGLY

#define DIST(xA, yA, xB, yB) \
    sqrt((xB-xA)*(xB-xA) + (yB-yA)*(yB-yA))

    //printf("%f %f\n", DIST(x03, y03, x0123, y0123), NUM(flat));
    if (DIST(x03, y03, x0123, y0123) < NUM(flat))
    {
        Xpost_Object elem, data;
        elem = xpost_dict_cons(ctx, 2);
        xpost_dict_put(ctx, elem, namecmd, nameline);
        data = xpost_object_cvlit(xpost_array_cons(ctx, 2));
        xpost_array_put(ctx, data, 0, xpost_real_cons(x3));
        xpost_array_put(ctx, data, 1, xpost_real_cons(y3));
        xpost_dict_put(ctx, elem, namedata, data);
        _addtopath(ctx, elem, _cpath(ctx));
    }
    else
    {
        _chopcurve(ctx, x0, y0, x01, y01, x012, y012, x0123, y0123, flat);
        _chopcurve(ctx, x0123, y0123, x123, y123, x23, y23, x3, y3, flat);
    }

    return 0;
}

static
int _flattenpath (Xpost_Context *ctx)
{
    Xpost_Object gd, gstate, flat;
    Xpost_Object path, the_new_path;
    Xpost_Object cp;
    Xpost_Object num;
    real x0, y0, x1, y1, x2, y2, x3, y3;
    int pathlen;
    int ret;
    int i;

    ret = xpost_op_any_load(ctx, namegraphicsdict);
    if (ret) return ret;
    gd = xpost_stack_pop(ctx->lo, ctx->os);
    gstate = xpost_dict_get(ctx, gd, namecurrgstate);
    flat = xpost_dict_get(ctx, gstate, xpost_name_cons(ctx, "flat"));

    path = _cpath(ctx);
    pathlen = xpost_dict_length_memory(xpost_context_select_memory(ctx, path), path);
    ret = _newpath(ctx);
    if (ret)
        return ret;
    the_new_path = _cpath(ctx);
    for (i = 0; i < pathlen; i++)
    {
        Xpost_Object subpath;
        int subpathlen;
        int j;

        subpath = xpost_dict_get(ctx, path, xpost_int_cons(i));
        if (xpost_object_get_type(subpath) == invalidtype)
        {
            XPOST_LOG_ERR("subpath %d not found in path (size %d)", i, pathlen);
            return undefined;
        }
        subpathlen = xpost_dict_length_memory(xpost_context_select_memory(ctx, subpath), subpath);
        for (j = 0; j < subpathlen; j++)
        {
            Xpost_Object elem;
            Xpost_Object cmd;

            elem = xpost_dict_get(ctx, subpath, xpost_int_cons(j));
            if (xpost_object_get_type(elem) == invalidtype)
            {
                XPOST_LOG_ERR("elem %d not found in subpath %d (size %d)", j, i, subpathlen);
                return undefined;
            }
            cmd = xpost_dict_get(ctx, elem, namecmd);
            if (xpost_object_get_type(cmd) == invalidtype)
            {
                XPOST_LOG_ERR("/cmd not found in elem %d of subpath %d", j, i);
                return undefined;
            }
            if (cmd.mark_.padw == namemove.mark_.padw)
            {
                cp = xpost_dict_get(ctx, elem, namedata);
                ret = _addtopath(ctx, elem, the_new_path);
                if (ret)
                    return ret;
            }
            else if (cmd.mark_.padw == nameline.mark_.padw)
            {
                cp = xpost_dict_get(ctx, elem, namedata);
                ret = _addtopath(ctx, elem, the_new_path);
                if (ret)
                    return ret;
            }
            else if (cmd.mark_.padw == namecurve.mark_.padw)
            {

                Xpost_Object data;
                num = xpost_array_get(ctx, cp, 0);
                x0 = NUM(num);
                num = xpost_array_get(ctx, cp, 1);
                y0 = NUM(num);
                data = xpost_dict_get(ctx, elem, namedata);
                num = xpost_array_get(ctx, data, 0);
                x1 = NUM(num);
                num = xpost_array_get(ctx, data, 1);
                y1 = NUM(num);
                num = xpost_array_get(ctx, data, 2);
                x2 = NUM(num);
                num = xpost_array_get(ctx, data, 3);
                y2 = NUM(num);
                num = xpost_array_get(ctx, data, 4);
                x3 = NUM(num);
                num = xpost_array_get(ctx, data, 5);
                y3 = NUM(num);
                //printf("%f %f %f %f %f %f %f %f\n", x0, y0, x1, y1, x2, y2, x3, y3);

                _chopcurve(ctx, x0, y0, x1, y1, x2, y2, x3, y3, flat);
            }
            else if (cmd.mark_.padw == nameclose.mark_.padw)
            {
                cp = xpost_dict_get(ctx, elem, namedata);
                ret = _addtopath(ctx, elem, the_new_path);
                if (ret)
                    return ret;
            }
        }
    }

    return 0;
}


int xpost_oper_init_path_ops(Xpost_Context *ctx,
                             Xpost_Object sd)
{
    Xpost_Operator *optab;
    Xpost_Object n,op;
    unsigned int optadr;

    assert(ctx->gl->base);
    xpost_memory_table_get_addr(ctx->gl,
            XPOST_MEMORY_TABLE_SPECIAL_OPERATOR_TABLE, &optadr);
    optab = (void *)(ctx->gl->base + optadr);

    if (xpost_object_get_type(namegraphicsdict = xpost_name_cons(ctx, "graphicsdict")) == invalidtype)
        return VMerror;
    if (xpost_object_get_type(namecurrgstate = xpost_name_cons(ctx, "currgstate")) == invalidtype)
        return VMerror;
    if (xpost_object_get_type(namecurrpath = xpost_name_cons(ctx, "currpath")) == invalidtype)
        return VMerror;
    if (xpost_object_get_type(namecmd = xpost_name_cons(ctx, "cmd")) == invalidtype)
        return VMerror;
    if (xpost_object_get_type(namedata = xpost_name_cons(ctx, "data")) == invalidtype)
        return VMerror;
    if (xpost_object_get_type(namemove = xpost_name_cons(ctx, "move")) == invalidtype)
        return VMerror;
    if (xpost_object_get_type(nameline = xpost_name_cons(ctx, "line")) == invalidtype)
        return VMerror;
    if (xpost_object_get_type(namecurve = xpost_name_cons(ctx, "curve")) == invalidtype)
        return VMerror;
    if (xpost_object_get_type(nameclose = xpost_name_cons(ctx, "close")) == invalidtype)
        return VMerror;

    _mat = xpost_object_cvlit(xpost_array_cons(ctx, 6));
    _mat1 = xpost_object_cvlit(xpost_array_cons(ctx, 6));

    op = xpost_operator_cons(ctx, "newpath", (Xpost_Op_Func)_newpath, 0, 0);
    INSTALL;
    op = xpost_operator_cons(ctx, "currentpoint", (Xpost_Op_Func)_currentpoint, 0, 0);
    _currentpoint_opcode = op.mark_.padw;
    INSTALL;

    op = xpost_operator_cons(ctx, "moveto", (Xpost_Op_Func)_moveto, 0, 2, numbertype, numbertype);
    _moveto_opcode = op.mark_.padw;
    INSTALL;
    op = xpost_operator_cons(ctx, "moveto_cont", (Xpost_Op_Func)_moveto_cont, 0, 2, numbertype, numbertype);
    _moveto_cont_opcode = op.mark_.padw;

    op = xpost_operator_cons(ctx, "rmoveto", (Xpost_Op_Func)_rmoveto, 0, 2, floattype, floattype);
    INSTALL;
    op = xpost_operator_cons(ctx, "rmoveto_cont", (Xpost_Op_Func)_rmoveto_cont, 0, 4,
                             floattype, floattype, floattype, floattype);
    _rmoveto_cont_opcode = op.mark_.padw;

    op = xpost_operator_cons(ctx, "lineto", (Xpost_Op_Func)_lineto, 0, 2, numbertype, numbertype);
    _lineto_opcode = op.mark_.padw;
    INSTALL;
    op = xpost_operator_cons(ctx, "lineto_cont", (Xpost_Op_Func)_lineto_cont, 0, 2, numbertype, numbertype);
    _lineto_cont_opcode = op.mark_.padw;

    op = xpost_operator_cons(ctx, "rlineto", (Xpost_Op_Func)_rlineto, 0, 2, floattype, floattype);
    INSTALL;
    op = xpost_operator_cons(ctx, "rlineto_cont", (Xpost_Op_Func)_rlineto_cont, 0, 4,
                             floattype, floattype, floattype, floattype);
    _rlineto_cont_opcode = op.mark_.padw;

    op = xpost_operator_cons(ctx, "curveto", (Xpost_Op_Func)_curveto, 0, 6,
                             numbertype, numbertype, numbertype, numbertype, numbertype, numbertype);
    _curveto_opcode = op.mark_.padw;
    INSTALL;
    op = xpost_operator_cons(ctx, "curveto_cont1", (Xpost_Op_Func)_curveto_cont1, 0, 6,
                             numbertype, numbertype, numbertype, numbertype, numbertype, numbertype);
    _curveto_cont1_opcode = op.mark_.padw;

    op = xpost_operator_cons(ctx, "curveto_cont2", (Xpost_Op_Func)_curveto_cont2, 0, 6,
                             numbertype, numbertype, numbertype, numbertype, numbertype, numbertype);
    _curveto_cont2_opcode = op.mark_.padw;

    op = xpost_operator_cons(ctx, "curveto_cont3", (Xpost_Op_Func)_curveto_cont3, 0, 6,
                             numbertype, numbertype, numbertype, numbertype, numbertype, numbertype);
    _curveto_cont3_opcode = op.mark_.padw;

    op = xpost_operator_cons(ctx, "rcurveto", (Xpost_Op_Func)_rcurveto, 0, 6,
                             floattype, floattype, floattype, floattype, floattype, floattype);
    INSTALL;
    op = xpost_operator_cons(ctx, "rcurveto_cont", (Xpost_Op_Func)_rcurveto_cont, 0, 8,
                             floattype, floattype, floattype, floattype, floattype, floattype, floattype, floattype);
    _rcurveto_cont_opcode = op.mark_.padw;

    op = xpost_operator_cons(ctx, "closepath", (Xpost_Op_Func)_closepath, 0, 0);
    INSTALL;

    op = xpost_operator_cons(ctx, "arc", (Xpost_Op_Func)_arc, 0, 5,
                             floattype, floattype, floattype, floattype, floattype);
    INSTALL;
    op = xpost_operator_cons(ctx, "arcn", (Xpost_Op_Func)_arcn, 0, 5,
                             floattype, floattype, floattype, floattype, floattype);
    INSTALL;

    op = xpost_operator_cons(ctx, "flattenpath", (Xpost_Op_Func)_flattenpath, 0, 0);
    INSTALL;

    _arc_start_proc = xpost_array_cons(ctx, 7);
    xpost_array_put(ctx, _arc_start_proc, 0, xpost_object_cvx(xpost_name_cons(ctx, "cpath")));
    xpost_array_put(ctx, _arc_start_proc, 1, xpost_object_cvx(xpost_name_cons(ctx, "length")));
    xpost_array_put(ctx, _arc_start_proc, 2, xpost_int_cons(0));
    xpost_array_put(ctx, _arc_start_proc, 3, xpost_object_cvx(xpost_name_cons(ctx, "gt")));
    {
        Xpost_Object true_clause = xpost_object_cvx(xpost_array_cons(ctx, 1));
        xpost_array_put(ctx, true_clause, 0, xpost_object_cvx(xpost_name_cons(ctx, "lineto")));
        xpost_array_put(ctx, _arc_start_proc, 4, true_clause);
    }
    {
        Xpost_Object false_clause = xpost_object_cvx(xpost_array_cons(ctx, 1));
        xpost_array_put(ctx, false_clause, 0, xpost_object_cvx(xpost_name_cons(ctx, "moveto")));
        xpost_array_put(ctx, _arc_start_proc, 5, false_clause);
    }
    xpost_array_put(ctx, _arc_start_proc, 6, xpost_object_cvx(xpost_name_cons(ctx, "ifelse")));

    return 0;
}
