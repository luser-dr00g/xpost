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

#include <assert.h>
#include <stdio.h>
#include <string.h>


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

#include "xpost_operator.h"
#include "xpost_op_dict.h"
#include "xpost_op_path.h"
    
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

static Xpost_Object namegraphicsdict;
static Xpost_Object namecurrgstate;
static Xpost_Object namecurrpath;
static Xpost_Object namecmd;
static Xpost_Object namedata;
static Xpost_Object namemove;
static Xpost_Object nameline;
static Xpost_Object namecurve;
static Xpost_Object nameclose;

static unsigned int _currentpoint_opcode;
static unsigned int _moveto_cont_opcode;
static unsigned int _rmoveto_cont_opcode;
static unsigned int _lineto_cont_opcode;
static unsigned int _rlineto_cont_opcode;
static unsigned int _curveto_cont1_opcode;
static unsigned int _curveto_cont2_opcode;
static unsigned int _curveto_cont3_opcode;
static unsigned int _rcurveto_cont_opcode;

static
int _newpath (Xpost_Context *ctx)
{
    Xpost_Object gd, gstate;
    int ret;

    /* graphicsdict /currgstate get /currpath 1 dict put */
    ret = xpost_op_any_load(ctx, namegraphicsdict /*xpost_name_cons(ctx, "graphicsdict")*/);
    if (ret) return ret;
    gd = xpost_stack_pop(ctx->lo, ctx->os);
    gstate = xpost_dict_get(ctx, gd, namecurrgstate /*xpost_name_cons(ctx, "currgstate")*/);
    ret = xpost_dict_put(ctx, gstate,
            namecurrpath /*xpost_name_cons(ctx, "currpath")*/,
            xpost_dict_cons(ctx, 1));
    if (ret) return ret;
    return 0;
}

static
Xpost_Object _cpath (Xpost_Context *ctx)
{
    Xpost_Object gd, gstate, path;
    int ret;

    /* graphicsdict /currgstate get /currpath get */
    ret = xpost_op_any_load(ctx, namegraphicsdict /*xpost_name_cons(ctx, "graphicsdict")*/);
    if (ret) return invalid;
    gd = xpost_stack_pop(ctx->lo, ctx->os);
    gstate = xpost_dict_get(ctx, gd, namecurrgstate /*xpost_name_cons(ctx, "currgstate")*/);
    path = xpost_dict_get(ctx, gstate, namecurrpath /*xpost_name_cons(ctx, "currpath")*/);
    return path;
}

int _currentpoint (Xpost_Context *ctx)
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
        return undefined;
    pathlen = xpost_dict_length_memory(xpost_context_select_memory(ctx, path), path);
    if (pathlen == 0)
        return nocurrentpoint;
    subpath = xpost_dict_get(ctx, path, xpost_int_cons(pathlen - 1));
    subpathlen = xpost_dict_length_memory(xpost_context_select_memory(ctx, subpath), subpath);
    elem = xpost_dict_get(ctx, subpath, xpost_int_cons(subpathlen - 1));
    data = xpost_dict_get(ctx, elem, namedata /*xpost_name_cons(ctx, "data")*/);
    datalen = data.comp_.sz;
    xpost_stack_push(ctx->lo, ctx->os, xpost_array_get(ctx, data, datalen - 2));
    xpost_stack_push(ctx->lo, ctx->os, xpost_array_get(ctx, data, datalen - 1));
    //xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons(ctx, "itransform", NULL,0,0));
    xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(ctx->opcode_shortcuts.itransform));
    
    return 0;
}

static
int _addtopath (Xpost_Context *ctx, Xpost_Object elem, Xpost_Object path)
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
        cmd = xpost_dict_get(ctx, elem, namecmd /*xpost_name_cons(ctx, "cmd")*/);
        if (xpost_dict_compare_objects(ctx, cmd, namemove /*xpost_name_cons(ctx, "move")*/) == 0)
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
        cmd = xpost_dict_get(ctx, elem, namecmd /*xpost_name_cons(ctx, "cmd")*/);
        if (xpost_dict_compare_objects(ctx, cmd, namemove /*xpost_name_cons(ctx, "move")*/) == 0)
        {
            int subpathlen;
            subpath = xpost_dict_get(ctx, path, xpost_int_cons(pathlen - 1));
            subpathlen = xpost_dict_length_memory(xpost_context_select_memory(ctx, subpath), subpath);
            lastelem = xpost_dict_get(ctx, subpath, xpost_int_cons(subpathlen - 1));
            cmd = xpost_dict_get(ctx, lastelem, namecmd /*xpost_name_cons(ctx, "cmd")*/);
            if (xpost_dict_compare_objects(ctx, cmd, namemove /*xpost_name_cons(ctx, "move")*/) == 0)
            {
                /* Merge "move" */
                Xpost_Object data;
                data = xpost_dict_get(ctx, elem, namedata /*xpost_name_cons(ctx, "data")*/);
                xpost_dict_put(ctx, lastelem, namedata /*xpost_name_cons(ctx, "data")*/, data);
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
int _moveto (Xpost_Context *ctx, Xpost_Object x, Xpost_Object y)
{
    xpost_stack_push(ctx->lo, ctx->os, x);
    xpost_stack_push(ctx->lo, ctx->os, y);
    //xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons(ctx, "moveto_cont", NULL,0,0));
    xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(_moveto_cont_opcode));
    //xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons(ctx, "transform", NULL,0,0));
    xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(ctx->opcode_shortcuts.transform));
    return 0;
}

static
int _moveto_cont (Xpost_Context *ctx, Xpost_Object x, Xpost_Object y)
{
    Xpost_Object data, elem;
    data = xpost_array_cons(ctx, 2);
    xpost_array_put(ctx, data, 0, x);
    xpost_array_put(ctx, data, 1, y);
    elem = xpost_dict_cons(ctx, 2);
    xpost_dict_put(ctx, elem, namecmd /*xpost_name_cons(ctx, "cmd")*/, namemove /*xpost_name_cons(ctx, "move")*/);
    xpost_dict_put(ctx, elem, namedata /*xpost_name_cons(ctx, "data")*/, data);
    return _addtopath(ctx, elem, _cpath(ctx));
}

static
int _rmoveto (Xpost_Context *ctx, Xpost_Object dx, Xpost_Object dy)
{
    xpost_stack_push(ctx->lo, ctx->os, dx);
    xpost_stack_push(ctx->lo, ctx->os, dy);
    //xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons(ctx, "rmoveto_cont", NULL,0,0));
    xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(_rmoveto_cont_opcode));
    //xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons(ctx, "currentpoint", NULL,0,0));
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
int _lineto (Xpost_Context *ctx, Xpost_Object x, Xpost_Object y)
{
    xpost_stack_push(ctx->lo, ctx->os, x);
    xpost_stack_push(ctx->lo, ctx->os, y);
    //xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons(ctx, "lineto_cont", NULL,0,0));
    xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(_lineto_cont_opcode));
    //xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons(ctx, "transform", NULL,0,0));
    xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(ctx->opcode_shortcuts.transform));
    return 0;
}

static
int _lineto_cont (Xpost_Context *ctx, Xpost_Object x, Xpost_Object y)
{
    Xpost_Object data, elem;
    data = xpost_array_cons(ctx, 2);
    xpost_array_put(ctx, data, 0, x);
    xpost_array_put(ctx, data, 1, y);
    elem = xpost_dict_cons(ctx, 2);
    xpost_dict_put(ctx, elem, namecmd /*xpost_name_cons(ctx, "cmd")*/, nameline /*xpost_name_cons(ctx, "line")*/);
    xpost_dict_put(ctx, elem, namedata /*xpost_name_cons(ctx, "data")*/, data);
    return _addtopath(ctx, elem, _cpath(ctx));
}

static
int _rlineto (Xpost_Context *ctx, Xpost_Object dx, Xpost_Object dy)
{
    xpost_stack_push(ctx->lo, ctx->os, dx);
    xpost_stack_push(ctx->lo, ctx->os, dy);
    //xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons(ctx, "rlineto_cont", NULL,0,0));
    xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(_rlineto_cont_opcode));
    //xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons(ctx, "currentpoint", NULL,0,0));
    xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(_currentpoint_opcode));
    return 0;
}

static
int _rlineto_cont (Xpost_Context *ctx,
                   Xpost_Object dx, Xpost_Object dy,
                   Xpost_Object x, Xpost_Object y)
{
    x.real_.val += dx.real_.val;
    y.real_.val += dy.real_.val;
    return _lineto(ctx, x, y);
}

static
int _curveto (Xpost_Context *ctx,
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
    //xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons(ctx, "curveto_cont1", NULL,0,0));
    xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(_curveto_cont1_opcode));
    //xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons(ctx, "transform", NULL,0,0));
    xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(ctx->opcode_shortcuts.transform));
    return 0;
}

static
int _curveto_cont1 (Xpost_Context *ctx,
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
    //xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons(ctx, "curveto_cont2", NULL,0,0));
    xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(_curveto_cont2_opcode));
    //xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons(ctx, "transform", NULL,0,0));
    xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(ctx->opcode_shortcuts.transform));
    return 0;
}

static
int _curveto_cont2 (Xpost_Context *ctx,
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
    //xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons(ctx, "curveto_cont3", NULL,0,0));
    xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(_curveto_cont3_opcode));
    //xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons(ctx, "transform", NULL,0,0));
    xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(ctx->opcode_shortcuts.transform));
    return 0;
}

static
int _curveto_cont3 (Xpost_Context *ctx,
        Xpost_Object X2, Xpost_Object Y2,
        Xpost_Object X3, Xpost_Object Y3,
        Xpost_Object X1, Xpost_Object Y1)
{
    Xpost_Object data, elem;
    data = xpost_array_cons(ctx, 6);
    xpost_array_put(ctx, data, 0, X1);
    xpost_array_put(ctx, data, 1, Y1);
    xpost_array_put(ctx, data, 2, X2);
    xpost_array_put(ctx, data, 3, Y2);
    xpost_array_put(ctx, data, 4, X3);
    xpost_array_put(ctx, data, 5, Y3);
    elem = xpost_dict_cons(ctx, 2);
    xpost_dict_put(ctx, elem, namecmd /*xpost_name_cons(ctx, "cmd")*/, namecurve /*xpost_name_cons(ctx, "curve")*/);
    xpost_dict_put(ctx, elem, namedata /*xpost_name_cons(ctx, "data")*/, data);
    return _addtopath(ctx, elem, _cpath(ctx));
}

static
int _rcurveto (Xpost_Context *ctx,
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
    //xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons(ctx, "rcurveto_cont", NULL,0,0));
    xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(_rcurveto_cont_opcode));
    //xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons(ctx, "currentpoint", NULL,0,0));
    xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(_currentpoint_opcode));
    return 0;
}

static
int _rcurveto_cont (Xpost_Context *ctx,
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
int _closepath (Xpost_Context *ctx)
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
        cmd = xpost_dict_get(ctx, lastelem, namecmd /*xpost_name_cons(ctx, "cmd")*/);
        if (xpost_dict_compare_objects(ctx, cmd, nameclose /*xpost_name_cons(ctx, "close")*/) != 0)
        {
            firstelem = xpost_dict_get(ctx, subpath, xpost_int_cons(0));
            data = xpost_dict_get(ctx, firstelem, namedata /*xpost_name_cons(ctx, "data")*/);
            elem = xpost_dict_cons(ctx, 2);
            xpost_dict_put(ctx, elem, namecmd /*xpost_name_cons(ctx, "cmd")*/, nameclose /*xpost_name_cons(ctx, "close")*/);
            xpost_dict_put(ctx, elem, namedata /*xpost_name_cons(ctx, "data")*/, data);
            return _addtopath(ctx, elem, _cpath(ctx));
        }
    }
    return 0;
}

int xpost_oper_init_path_ops (Xpost_Context *ctx,
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

    op = xpost_operator_cons(ctx, "newpath", (Xpost_Op_Func)_newpath, 0, 0);
    INSTALL;
    op = xpost_operator_cons(ctx, "currentpoint", (Xpost_Op_Func)_currentpoint, 0, 0);
    _currentpoint_opcode = op.mark_.padw;
    INSTALL;

    op = xpost_operator_cons(ctx, "moveto", (Xpost_Op_Func)_moveto, 0, 2, numbertype, numbertype);
    INSTALL;
    op = xpost_operator_cons(ctx, "moveto_cont", (Xpost_Op_Func)_moveto_cont, 0, 2, numbertype, numbertype);
    _moveto_cont_opcode = op.mark_.padw;

    op = xpost_operator_cons(ctx, "rmoveto", (Xpost_Op_Func)_rmoveto, 0, 2, floattype, floattype);
    INSTALL;
    op = xpost_operator_cons(ctx, "rmoveto_cont", (Xpost_Op_Func)_rmoveto_cont, 0, 4,
            floattype, floattype, floattype, floattype);
    _rmoveto_cont_opcode = op.mark_.padw;

    op = xpost_operator_cons(ctx, "lineto", (Xpost_Op_Func)_lineto, 0, 2, numbertype, numbertype);
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

    return 0;
}


