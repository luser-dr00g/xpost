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

#define _USE_MATH_DEFINES /* needed for M_PI with Visual Studio */
#include <assert.h>
#include <float.h>
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
#include "xpost_save.h"  /* the current path obeys save/restore */

#include "xpost_operator.h"
#include "xpost_op_dict.h"
#include "xpost_op_path.h"
#include "xpost_dev_generic.h"  /* the vector devices' content accumulator */

#undef y0
#undef y1

/*
   The current path is a packed byte string held at /currpath in the
   graphics state, avoiding a dictionary and an array allocation per
   path element:

       header (32 bytes):
           u32 used       total bytes in use, including this header
           u32 sp_start   offset of the current subpath's move element
           u32 last_elem  offset of the most recent element
           u32 cap        allocated capacity in bytes
           f32 bbox[4]    running minx miny maxx maxy over every point
                          appended (conservative: it may retain points
                          later overwritten by a move-onto-move merge,
                          and it spans curve control hulls)
       element:
           u8  cmd        0 move, 1 line, 2 curve, 3 close
           f32 coords     one point (move, line); three points (curve);
                          the subpath's start point repeated (close)

   Coordinates are stored in device space, already transformed by the
   CTM. The string object is an opaque handle: its sz field is a
   16-bit word, far too small for a large symbol's path, so the true
   extent lives in the header (entity allocations are not so limited)
   and sz holds a fixed nonzero sentinel. The sentinel keeps clear of
   the reserved sz==0 && ent!=0 encoding that luser-dr00g/xpost#40
   earmarks for 65536-byte strings, so a path can never be mistaken
   for one. PostScript code never takes the handle's length; C code
   reaches the bytes through the entity address alone.
   The allocation is oversized and doubled as needed; growth replaces
   /currpath, so a reference snapshotted before newpath stays intact.
   A move following a move overwrites it in place, so a subpath always
   begins with exactly one move element.

   The string's bytes are written directly, so unlike the rest of VM
   the path contents are not unwound by restore; the path is graphics
   state, reverted by grestore's copy, and no drawing sequence relies
   on restore rebuilding a partly constructed path.
 */

//#define RAD_PER_DEG (M_PI / 180.0)
#define RAD_PER_DEG (0.0174533)

/*name objects*/
static Xpost_Object namegraphicsdict;
static Xpost_Object namecurrgstate;
static Xpost_Object namecurrpath;
static Xpost_Object nameclipregion;
static Xpost_Object nameflat;
static Xpost_Object namecurrmatrix;

/*opcodes*/
static unsigned int _currentpoint_opcode;
static unsigned int _moveto_opcode;
static unsigned int _rmoveto_cont_opcode;
static unsigned int _lineto_opcode;
static unsigned int _rlineto_cont_opcode;
static unsigned int _curveto_opcode;
static unsigned int _rcurveto_cont_opcode;

/*matrices*/

#define NUM(x) (xpost_object_get_type(x)==realtype?x.real_.val:(real)x.int_.val)

#define PATH_HDR 32
#define PATH_CMD_MOVE 0
#define PATH_CMD_LINE 1
#define PATH_CMD_CURVE 2
#define PATH_CMD_CLOSE 3

static unsigned int
_path_get_u32(const char *p, unsigned int off)
{
    unsigned int v;
    memcpy(&v, p + off, sizeof v);
    return v;
}

static void
_path_set_u32(char *p, unsigned int off, unsigned int v)
{
    memcpy(p + off, &v, sizeof v);
}

static real
_path_get_f32(const char *p, unsigned int off)
{
    real v;
    memcpy(&v, p + off, sizeof v);
    return v;
}

static void
_path_set_f32(char *p, unsigned int off, real v)
{
    memcpy(p + off, &v, sizeof v);
}

static unsigned int
_path_elem_size(int cmd)
{
    return cmd == PATH_CMD_CURVE ? 1 + 6 * sizeof(real) : 1 + 2 * sizeof(real);
}

static void
_path_get_coords(const char *p, unsigned int elem, real *co, int n)
{
    memcpy(co, p + elem + 1, n * sizeof(real));
}

/* Bytes readable through a path string's data pointer: the backing entity
   less the object's own offset. A path records its extent in its header
   rather than comp_.sz, so an operator handed a program-supplied string
   must bound the header against the allocation, not the object size. */
static unsigned int
_path_avail(Xpost_Context *ctx, Xpost_Object path)
{
    Xpost_Memory_File *mem = xpost_context_select_memory(ctx, path);
    unsigned int ent = xpost_object_get_ent(path);
    unsigned int entsz = mem->table.tab[ent].sz;
    unsigned int off = path.comp_.off;

    return off < entsz ? entsz - off : 0;
}

/* True when a path string's header and element chain lie within its own
   allocation. The current path lives in a program-reachable dictionary and
   can be replaced with a forged string, so a reader that trusts the packed
   layout (extent at offset 0, last-element offset at offset 8, an element
   chain from PATH_HDR) validates it first. */
static int
_path_ok(Xpost_Context *ctx, Xpost_Object path)
{
    unsigned int avail, used, last, o;
    const char *p;

    if (xpost_object_get_type(path) != stringtype)
        return 0;
    avail = _path_avail(ctx, path);
    if (avail < PATH_HDR)
        return 0;
    p = xpost_string_get_pointer(ctx, path);
    used = _path_get_u32(p, 0);
    if (used < PATH_HDR || used > avail)
        return 0;
    for (o = PATH_HDR; o < used; )
    {
        unsigned int esz;

        if (p[o] < PATH_CMD_MOVE || p[o] > PATH_CMD_CLOSE)
            return 0;
        esz = _path_elem_size(p[o]);
        if (esz > used - o)   /* the element must fit the declared content */
            return 0;
        o += esz;
    }
    last = _path_get_u32(p, 8);
    if (used > PATH_HDR && (last < PATH_HDR || last >= used))
        return 0;   /* offset 8 must name an element start within content */
    return 1;
}

/* paths always live in local VM: the graphics state dictionary is
   local, and local objects may not be stored into global composites */
static Xpost_Object
_path_cons(Xpost_Context *ctx, unsigned int cap)
{
    Xpost_Object s;
    unsigned int ent;
    char *p;

    if (!xpost_memory_table_alloc(ctx->lo, cap, stringtype, &ent))
        return invalid;
    /* stamp the ent with the current save level, as the standard
       composite constructors do, so the save/restore guard in
       _path_append can tell a path predating a save from one created
       inside it */
    {
        unsigned int vs, cnt;
        xpost_memory_table_get_addr(ctx->lo, XPOST_MEMORY_TABLE_SPECIAL_SAVE_STACK, &vs);
        cnt = xpost_stack_count(ctx->lo, vs);
        ctx->lo->table.tab[ent].mark =
            (cnt << XPOST_MEMORY_TABLE_MARK_DATA_LOWLEVEL_OFFSET) |
            (cnt << XPOST_MEMORY_TABLE_MARK_DATA_TOPLEVEL_OFFSET);
    }
    s.tag = stringtype |
        (XPOST_OBJECT_TAG_ACCESS_UNLIMITED << XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_OFFSET);
    /* nonzero sentinel: the extent lives in the header, but sz must
       stay clear of the reserved sz==0 encoding (see the note above).
       The header size is always within the allocation, so a stray
       generic read of sz bytes cannot run past the buffer. */
    s.comp_.sz = PATH_HDR;
    s.comp_.off = 0;
    s = xpost_object_set_ent(s, ent);
    s = xpost_object_cvlit(s);
    p = xpost_string_get_pointer(ctx, s);
    _path_set_u32(p, 0, PATH_HDR);
    _path_set_u32(p, 4, 0);
    _path_set_u32(p, 8, 0);
    _path_set_u32(p, 12, cap);
    _path_set_f32(p, 16, FLT_MAX);
    _path_set_f32(p, 20, FLT_MAX);
    _path_set_f32(p, 24, -FLT_MAX);
    _path_set_f32(p, 28, -FLT_MAX);
    return s;
}

/* read a path's capacity from its header */
static unsigned int
_path_cap(Xpost_Context *ctx, Xpost_Object path)
{
    return _path_get_u32(xpost_string_get_pointer(ctx, path), 12);
}

/* append an element, growing the string (and re-seating /currpath in
   the graphics state) when full; *pathp is updated in place */
static int
_path_append(Xpost_Context *ctx, Xpost_Object gstate, Xpost_Object *pathp,
             int cmd, const real *co, int ncoords)
{
    char *p;
    unsigned int used, esz, pent;

    /* the current path is part of the graphics state, which save
       snapshots and restore rewinds. Back up the path storage on its
       first mutation after a save so restore reverts it: one save
       record per save level, not per element. A grow allocates a fresh
       ent whose /currpath slot is separately save-protected, so this
       only matters for in-place appends and the move-onto-move merge.
       save_save_ent may move the memory file, so *pathp's pointer is
       derived afterwards. */
    pent = xpost_object_get_ent(*pathp);
    if (!xpost_save_ent_is_saved(ctx->lo, pent))
        if (!xpost_save_save_ent(ctx->lo, stringtype, 0, pent))
            return VMerror;

    p = xpost_string_get_pointer(ctx, *pathp);
    used = _path_get_u32(p, 0);
    esz = 1 + ncoords * sizeof(real);
    /* the path lives in a program-reachable dictionary; reject a forged
       header before it drives an out-of-bounds append or a grow loop that
       never terminates (a zero capacity doubles to itself) */
    {
        unsigned int avail = _path_avail(ctx, *pathp);
        unsigned int cap;

        if (avail < PATH_HDR)
            return rangecheck;
        cap = _path_get_u32(p, 12);
        if (used < PATH_HDR || used > cap || cap > avail)
            return rangecheck;
    }

    /* merge a move into an immediately preceding move */
    if (cmd == PATH_CMD_MOVE && used > PATH_HDR)
    {
        unsigned int last = _path_get_u32(p, 8);
        if (last >= PATH_HDR && last <= used - esz && p[last] == PATH_CMD_MOVE)
        {
            memcpy(p + last + 1, co, ncoords * sizeof(real));
            return 0;
        }
    }

    if (used + esz > _path_get_u32(p, 12))
    {
        Xpost_Object ns;
        unsigned int newcap = _path_get_u32(p, 12) * 2;
        char *np;
        int ret;

        while (newcap < used + esz)
            newcap *= 2;
        ns = _path_cons(ctx, newcap);
        if (xpost_object_get_type(ns) == invalidtype)
            return VMerror;
        np = xpost_string_get_pointer(ctx, ns);
        p = xpost_string_get_pointer(ctx, *pathp); /* re-derive: cons may move the file */
        memcpy(np, p, used);
        _path_set_u32(np, 12, newcap); /* the copied header overwrote cap */
        ret = xpost_dict_put(ctx, gstate, namecurrpath, ns);
        if (ret)
            return ret;
        *pathp = ns;
        p = xpost_string_get_pointer(ctx, ns);
    }

    p[used] = (char)cmd;
    memcpy(p + used + 1, co, ncoords * sizeof(real));
    if (cmd == PATH_CMD_MOVE)
        _path_set_u32(p, 4, used);
    _path_set_u32(p, 8, used);
    _path_set_u32(p, 0, used + esz);
    {
        int k;
        for (k = 0; k + 1 < ncoords; k += 2)
        {
            if (co[k] < _path_get_f32(p, 16)) _path_set_f32(p, 16, co[k]);
            if (co[k + 1] < _path_get_f32(p, 20)) _path_set_f32(p, 20, co[k + 1]);
            if (co[k] > _path_get_f32(p, 24)) _path_set_f32(p, 24, co[k]);
            if (co[k + 1] > _path_get_f32(p, 28)) _path_set_f32(p, 28, co[k + 1]);
        }
    }
    return 0;
}

/* currgstate is created once when the graphics subsystem loads and is
   only ever mutated in place (setgstate, grestore and gstatecopy copy
   into it, never rebind it), so the resolved dictionary can be cached
   after the first lookup instead of searching the dictionary stack on
   every path operator */
static Xpost_Object _gstate_cache;
static int _gstate_cached = 0;

static
Xpost_Object _gstate(Xpost_Context *ctx)
{
    Xpost_Object gd, gs;
    int ret;

    if (_gstate_cached)
        return _gstate_cache;
    ret = xpost_op_any_load(ctx, namegraphicsdict);
    if (ret) return invalid;
    gd = xpost_stack_pop(ctx->lo, ctx->os);
    if (xpost_object_get_type(gd) == invalidtype)
        return invalid;
    gs = xpost_dict_get(ctx, gd, namecurrgstate);
    if (xpost_object_get_type(gs) == dicttype)
    {
        _gstate_cache = gs;
        _gstate_cached = 1;
    }
    return gs;
}

/* read the CTM's six coefficients, promoting integer entries,
   with arithmetic identical to the transform operator's */
static
int _path_ctm(Xpost_Context *ctx, Xpost_Object gstate, real *m)
{
    Xpost_Object psm;
    Xpost_Object arr[6];
    int i;

    psm = xpost_dict_get(ctx, gstate, namecurrmatrix);
    if (xpost_object_get_type(psm) != arraytype)
        return undefined;
    xpost_memory_get(xpost_context_select_memory(ctx, psm),
                     xpost_object_get_ent(psm), 0, sizeof arr, arr);
    for (i = 0; i < 6; i++)
        m[i] = xpost_object_get_type(arr[i]) == integertype
             ? (real)arr[i].int_.val : arr[i].real_.val;
    return 0;
}

static
int _newpath(Xpost_Context *ctx)
{
    Xpost_Object gstate, path;

    gstate = _gstate(ctx);
    if (xpost_object_get_type(gstate) == invalidtype)
        return undefined;
    path = _path_cons(ctx, 256);
    if (xpost_object_get_type(path) == invalidtype)
        return VMerror;
    return xpost_dict_put(ctx, gstate, namecurrpath, path);
}

/* return a fresh empty path string (for graphics-state templates) */
static
int _newpathstr(Xpost_Context *ctx)
{
    Xpost_Object path;

    path = _path_cons(ctx, 256);
    if (xpost_object_get_type(path) == invalidtype)
        return VMerror;
    xpost_stack_push(ctx->lo, ctx->os, path);
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
    /* the readers below trust the packed layout, so a forged current path
       must not reach them */
    if (xpost_object_get_type(path) == stringtype && !_path_ok(ctx, path))
        return invalid;
    return path;
}

int _currentpoint(Xpost_Context *ctx)
{
    Xpost_Object path;
    char *p;
    unsigned int used, last;
    real co[6];
    int cmd, n;

    path = _cpath(ctx);
    if (xpost_object_get_type(path) != stringtype)
        return nocurrentpoint;
    p = xpost_string_get_pointer(ctx, path);
    used = _path_get_u32(p, 0);
    if (used <= PATH_HDR)
        return nocurrentpoint;
    last = _path_get_u32(p, 8);
    cmd = p[last];
    n = cmd == PATH_CMD_CURVE ? 6 : 2;
    _path_get_coords(p, last, co, n);
    xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons(co[n - 2]));
    xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons(co[n - 1]));
    xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(ctx->opcode_shortcuts.itransform));

    return 0;
}

static
int _moveto(Xpost_Context *ctx, Xpost_Object x, Xpost_Object y)
{
    Xpost_Object gstate, path;
    real m[6];
    real co[2];
    int ret;

    gstate = _gstate(ctx);
    if (xpost_object_get_type(gstate) == invalidtype)
        return undefined;
    ret = _path_ctm(ctx, gstate, m);
    if (ret)
        return ret;
    path = xpost_dict_get(ctx, gstate, namecurrpath);
    if (xpost_object_get_type(path) != stringtype)
        return unregistered;
    co[0] = m[0] * x.real_.val + m[2] * y.real_.val + m[4];
    co[1] = m[1] * x.real_.val + m[3] * y.real_.val + m[5];
    return _path_append(ctx, gstate, &path, PATH_CMD_MOVE, co, 2);
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
    Xpost_Object gstate, path;
    char *p;
    real m[6];
    real co[2];
    int ret;

    gstate = _gstate(ctx);
    if (xpost_object_get_type(gstate) == invalidtype)
        return undefined;
    ret = _path_ctm(ctx, gstate, m);
    if (ret)
        return ret;
    path = xpost_dict_get(ctx, gstate, namecurrpath);
    if (xpost_object_get_type(path) != stringtype)
        return unregistered;
    p = xpost_string_get_pointer(ctx, path);
    if (_path_get_u32(p, 0) <= PATH_HDR)
        return nocurrentpoint;
    co[0] = m[0] * x.real_.val + m[2] * y.real_.val + m[4];
    co[1] = m[1] * x.real_.val + m[3] * y.real_.val + m[5];
    return _path_append(ctx, gstate, &path, PATH_CMD_LINE, co, 2);
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
    Xpost_Object gstate, path;
    char *p;
    real m[6];
    real co[6];
    int ret;

    gstate = _gstate(ctx);
    if (xpost_object_get_type(gstate) == invalidtype)
        return undefined;
    ret = _path_ctm(ctx, gstate, m);
    if (ret)
        return ret;
    path = xpost_dict_get(ctx, gstate, namecurrpath);
    if (xpost_object_get_type(path) != stringtype)
        return unregistered;
    p = xpost_string_get_pointer(ctx, path);
    if (_path_get_u32(p, 0) <= PATH_HDR)
        return nocurrentpoint;
    co[0] = m[0] * x1.real_.val + m[2] * y1.real_.val + m[4];
    co[1] = m[1] * x1.real_.val + m[3] * y1.real_.val + m[5];
    co[2] = m[0] * x2.real_.val + m[2] * y2.real_.val + m[4];
    co[3] = m[1] * x2.real_.val + m[3] * y2.real_.val + m[5];
    co[4] = m[0] * x3.real_.val + m[2] * y3.real_.val + m[4];
    co[5] = m[1] * x3.real_.val + m[3] * y3.real_.val + m[5];
    return _path_append(ctx, gstate, &path, PATH_CMD_CURVE, co, 6);
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

/* walk a packed path accumulating the bounding box of every stored
   coordinate pair (curve controls and close repeats included, matching
   the behaviour of the dictionary-walking predecessors); when inv is
   non-NULL it is an affine matrix (PostScript [a b c d tx ty] layout)
   applied to each stored point before accumulation; returns 0 on
   a malformed path or when a curve is present and curves are not
   accepted, 2 on an empty path */
static
int _path_walk_bbox(Xpost_Context *ctx, Xpost_Object path,
                    int accept_curves, const real *inv,
                    real *minx, real *miny, real *maxx, real *maxy)
{
    char *p;
    unsigned int used, o;
    int any = 0;

    if (xpost_object_get_type(path) != stringtype)
        return 0;
    p = xpost_string_get_pointer(ctx, path);
    used = _path_get_u32(p, 0);
    for (o = PATH_HDR; o < used; o += _path_elem_size(p[o]))
    {
        int cmd = p[o];
        int n, k;
        real co[6];

        if (cmd < PATH_CMD_MOVE || cmd > PATH_CMD_CLOSE)
            return 0;
        if (!accept_curves && cmd == PATH_CMD_CURVE)
            return 0;
        n = cmd == PATH_CMD_CURVE ? 6 : 2;
        _path_get_coords(p, o, co, n);
        for (k = 0; k + 1 < n; k += 2)
        {
            real x = co[k], y = co[k + 1];
            if (inv)
            {
                real x_ = inv[0] * x + inv[2] * y + inv[4];
                y = inv[1] * x + inv[3] * y + inv[5];
                x = x_;
            }
            if (!any)
            {
                *minx = *maxx = x;
                *miny = *maxy = y;
                any = 1;
            }
            else
            {
                if (x < *minx) *minx = x;
                if (x > *maxx) *maxx = x;
                if (y < *miny) *miny = y;
                if (y > *maxy) *maxy = y;
            }
        }
    }
    return any ? 1 : 2;
}

/* test whether a path is a single closed axis-aligned rectangle
   and return its bounds */
static
int _path_is_rect(Xpost_Context *ctx, Xpost_Object path,
                  real *minx, real *miny, real *maxx, real *maxy)
{
    char *p;
    unsigned int used, o;
    real px[5], py[5];
    int npts = 0;
    int j;

    if (xpost_object_get_type(path) != stringtype)
        return 0;
    p = xpost_string_get_pointer(ctx, path);
    used = _path_get_u32(p, 0);
    for (o = PATH_HDR; o < used; o += _path_elem_size(p[o]))
    {
        int cmd = p[o];
        real co[2];

        if (cmd == PATH_CMD_CLOSE)
            continue; /* close repeats the start point */
        if (o == PATH_HDR)
        {
            if (cmd != PATH_CMD_MOVE)
                return 0;
        }
        else if (cmd != PATH_CMD_LINE)
            return 0; /* second subpath or curve */
        if (npts >= 5)
            return 0;
        _path_get_coords(p, o, co, 2);
        px[npts] = co[0];
        py[npts] = co[1];
        npts++;
    }
    /* an explicitly repeated start point is equivalent to closure */
    if (npts == 5)
    {
        if (px[4] != px[0] || py[4] != py[0])
            return 0;
        npts = 4;
    }
    if (npts != 4)
        return 0;
    /* every side, including the closing one, must be axis-parallel */
    for (j = 0; j < 4; j++)
    {
        int n = (j + 1) & 3;
        if (px[j] != px[n] && py[j] != py[n])
            return 0;
    }
    *minx = *maxx = px[0];
    *miny = *maxy = py[0];
    for (j = 1; j < 4; j++)
    {
        if (px[j] < *minx) *minx = px[j];
        if (px[j] > *maxx) *maxx = px[j];
        if (py[j] < *miny) *miny = py[j];
        if (py[j] > *maxy) *maxy = py[j];
    }
    return *maxx > *minx && *maxy > *miny;
}

/* allocate an uninitialised array: the caller fills every slot
   directly, so the null prefill and per-put save checks of the
   ordinary constructor are wasted work */
static Xpost_Object
_rawarray_cons(Xpost_Context *ctx, unsigned int sz, Xpost_Object **payload)
{
    Xpost_Memory_File *mem = ctx->lo;
    unsigned int ent, vs, cnt, adr;
    Xpost_Object o;

    if (!xpost_memory_table_alloc(mem, sz * sizeof(Xpost_Object), arraytype, &ent))
        return invalid;
    /* stamp as saved at the current level, as the constructor would */
    xpost_memory_table_get_addr(mem, XPOST_MEMORY_TABLE_SPECIAL_SAVE_STACK, &vs);
    cnt = xpost_stack_count(mem, vs);
    mem->table.tab[ent].mark =
        (cnt << XPOST_MEMORY_TABLE_MARK_DATA_LOWLEVEL_OFFSET) |
        (cnt << XPOST_MEMORY_TABLE_MARK_DATA_TOPLEVEL_OFFSET);
    o.tag = arraytype |
        (XPOST_OBJECT_TAG_ACCESS_UNLIMITED << XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_OFFSET);
    o.comp_.sz = sz;
    o.comp_.off = 0;
    o = xpost_object_set_ent(o, ent);
    o = xpost_object_cvlit(o);
    xpost_memory_table_get_addr(mem, ent, &adr);
    *payload = (Xpost_Object *)(mem->base + adr);
    return o;
}

/* build the polygon argument for the device FillPoly procedure: a flat
   array of [x y] point pairs with subpaths separated (and terminated)
   by null. All pairs are two-object views into one backing array, so
   the whole argument costs two allocations. A close element repeats
   the subpath's first point. Subpaths of fewer than three points
   cannot enclose area and are dropped. */
static
int _fillpolyargs(Xpost_Context *ctx)
{
    Xpost_Object path, backing, result;
    Xpost_Object *bk, *rs;
    char *p;
    unsigned int used, o;
    unsigned int npts = 0, nsp = 0;
    unsigned int bi, ri, spbi, spri;
    int cmd, n, k;
    real co[6];

    path = _cpath(ctx);
    if (xpost_object_get_type(path) != stringtype)
        return unregistered;
    p = xpost_string_get_pointer(ctx, path);
    used = _path_get_u32(p, 0);

    /* first pass: count every point and subpath; area-less subpaths
       are dropped in the second pass after their points are written,
       so the buffers are sized before the drop rule is applied */
    for (o = PATH_HDR; o < used; o += _path_elem_size(p[o]))
    {
        cmd = p[o];
        if (cmd == PATH_CMD_MOVE)
            nsp++;
        if (cmd == PATH_CMD_CLOSE)
            npts += npts ? 1 : 0;
        else
            npts += cmd == PATH_CMD_CURVE ? 3 : 1;
    }

    if (npts == 0)
    {
        result = xpost_object_cvlit(xpost_array_cons(ctx, 0));
        xpost_stack_push(ctx->lo, ctx->os, result);
        return 0;
    }
    if (2 * npts > 65535)
        /* too large for a single backing array (the object size field
           is 16 bits) */
        return limitcheck;

    backing = _rawarray_cons(ctx, 2 * npts, &bk);
    if (xpost_object_get_type(backing) == invalidtype)
        return VMerror;
    xpost_stack_push(ctx->lo, ctx->hold, backing);
    result = _rawarray_cons(ctx, npts + nsp, &rs);
    if (xpost_object_get_type(result) == invalidtype)
        return VMerror;
    xpost_stack_push(ctx->lo, ctx->hold, result);
    /* allocations can move the memory file */
    p = xpost_string_get_pointer(ctx, path);
    {
        unsigned int adr;
        xpost_memory_table_get_addr(ctx->lo, xpost_object_get_ent(backing), &adr);
        bk = (Xpost_Object *)(ctx->lo->base + adr);
    }

    /* second pass: fill the backing coordinates and the pair views,
       rolling an area-less subpath back where it ended */
    bi = ri = spbi = spri = 0;
    for (o = PATH_HDR; o < used; o += _path_elem_size(p[o]))
    {
        cmd = p[o];
        if (cmd == PATH_CMD_MOVE && bi > spbi)
        {
            if (bi - spbi >= 6) rs[ri++] = null;
            else { bi = spbi; ri = spri; }
            spbi = bi;
            spri = ri;
        }
        if (cmd == PATH_CMD_CLOSE)
        {
            if (bi > spbi)
            {
                Xpost_Object v = backing;
                v.comp_.off = spbi;
                v.comp_.sz = 2;
                rs[ri++] = v;
                /* keep the backing fully initialised and count the
                   repeat toward the size rule */
                bk[bi] = bk[spbi];
                bk[bi + 1] = bk[spbi + 1];
                bi += 2;
            }
            continue;
        }
        n = cmd == PATH_CMD_CURVE ? 6 : 2;
        _path_get_coords(p, o, co, n);
        for (k = 0; k + 1 < n; k += 2)
        {
            Xpost_Object v = backing;
            bk[bi] = xpost_real_cons(co[k]);
            bk[bi + 1] = xpost_real_cons(co[k + 1]);
            v.comp_.off = bi;
            v.comp_.sz = 2;
            rs[ri++] = v;
            bi += 2;
        }
    }
    if (bi > spbi)
    {
        if (bi - spbi >= 6) rs[ri++] = null;
        else ri = spri;
    }

    result.comp_.sz = ri; /* dropped subpaths shrink the view */
    xpost_stack_push(ctx->lo, ctx->os, result);
    return 0;
}

/* clip trivial-accept test.
   Push true when the clip region is an axis-aligned rectangle and the
   current path lies entirely inside it: clipping the path against such
   a region passes every point through unchanged, so the caller can skip
   the polygon-clipping machinery. Push false in every uncertain case. */
static
int _cliptrivial(Xpost_Context *ctx)
{
    Xpost_Object gstate, path, clipregion;
    real cminx, cminy, cmaxx, cmaxy;
    real pminx, pminy, pmaxx, pmaxy;
    int accept = 0;

    gstate = _gstate(ctx);
    if (xpost_object_get_type(gstate) == invalidtype)
        return undefined;
    path = xpost_dict_get(ctx, gstate, namecurrpath);
    clipregion = xpost_dict_get(ctx, gstate, nameclipregion);

    if (xpost_object_get_type(path) == stringtype &&
        _path_is_rect(ctx, clipregion, &cminx, &cminy, &cmaxx, &cmaxy))
    {
        /* the header maintains a conservative bounding box (curve
           control hulls contain their curves); an empty path is
           accepted, there being nothing to clip */
        char *p = xpost_string_get_pointer(ctx, path);
        pminx = _path_get_f32(p, 16);
        pminy = _path_get_f32(p, 20);
        pmaxx = _path_get_f32(p, 24);
        pmaxy = _path_get_f32(p, 28);
        accept = _path_get_u32(p, 0) <= PATH_HDR ||
                 (pminx >= cminx && pmaxx <= cmaxx &&
                  pminy >= cminy && pmaxy <= cmaxy);
    }

    xpost_stack_push(ctx->lo, ctx->os, xpost_bool_cons(accept));
    return 0;
}

/* Emit the current path into a vector device's content accumulator with
   curves preserved -- the FillPath hot loop, walking the path string
   directly so arbitrarily many subpaths cost no operand stack. The
   syntax is the device's: PDF path operators or SVG path commands. */
static
int _fillpath_emit(Xpost_Context *ctx,
                   const double *comp,
                   Xpost_Object devdic, int svg)
{
    Xpost_Object gstate, path;
    char *p;
    unsigned int used, o;
    char tmp[192];
    int n, i;

    gstate = _gstate(ctx);
    if (xpost_object_get_type(gstate) == invalidtype)
        return undefined;
    path = xpost_dict_get(ctx, gstate, namecurrpath);
    if (xpost_object_get_type(path) != stringtype)
        return unregistered;
    p = xpost_string_get_pointer(ctx, path);
    used = _path_get_u32(p, 0);
    if (used <= PATH_HDR)
        return 0;

    /* the PDF device emits its fill colour itself, before the walk */
    if (svg)
    {
        n = 0;
        memcpy(tmp + n, "<path fill=\"rgb(", 16); n += 16;
        n += xpost_dev_pdf_fmt_num(tmp + n, comp[0] * 100); tmp[n++] = '%'; tmp[n++] = ',';
        n += xpost_dev_pdf_fmt_num(tmp + n, comp[1] * 100); tmp[n++] = '%'; tmp[n++] = ',';
        n += xpost_dev_pdf_fmt_num(tmp + n, comp[2] * 100); tmp[n++] = '%';
        memcpy(tmp + n, ")\" fill-rule=\"nonzero\" d=\"", 26); n += 26;
        if (!xpost_dev_pdf_append(ctx, devdic, tmp, n))
            return undefined;
    }

    for (o = PATH_HDR; o < used; o += _path_elem_size(p[o]))
    {
        int cmd = p[o];
        int nco = cmd == PATH_CMD_CURVE ? 6 : 2;
        real co[6];

        if (cmd < PATH_CMD_MOVE || cmd > PATH_CMD_CLOSE)
            return unregistered;
        _path_get_coords(p, o, co, nco);
        n = 0;
        if (cmd == PATH_CMD_CLOSE)
        {
            /* the stored coordinates repeat the subpath start */
            if (svg)
                tmp[n++] = 'Z';
            else
                { tmp[n++] = 'h'; tmp[n++] = '\n'; }
        }
        else if (svg)
        {
            tmp[n++] = cmd == PATH_CMD_MOVE ? 'M'
                     : cmd == PATH_CMD_LINE ? 'L' : 'C';
            for (i = 0; i < nco; i++)
            {
                if (i) tmp[n++] = ' ';
                n += xpost_dev_pdf_fmt_num(tmp + n, co[i]);
            }
        }
        else
        {
            for (i = 0; i < nco; i++)
            {
                n += xpost_dev_pdf_fmt_num(tmp + n, co[i]);
                tmp[n++] = ' ';
            }
            tmp[n++] = cmd == PATH_CMD_MOVE ? 'm'
                     : cmd == PATH_CMD_LINE ? 'l' : 'c';
            tmp[n++] = '\n';
        }
        if (n && !xpost_dev_pdf_append(ctx, devdic, tmp, n))
            return undefined;
    }

    if (!xpost_dev_pdf_append(ctx, devdic,
                              svg ? "\"/>\n" : "f\n", svg ? 4 : 2))
        return undefined;
    return 0;
}

#define FPNUMVAL(o) (xpost_object_get_type(o) == realtype ? (o).real_.val \
                                                          : (double)(o).int_.val)
static
int _pdffillpath(Xpost_Context *ctx,
                 Xpost_Object devdic)
{
    return _fillpath_emit(ctx, NULL, devdic, 0);
}

static
int _svgfillpath(Xpost_Context *ctx,
                 Xpost_Object r, Xpost_Object g, Xpost_Object b,
                 Xpost_Object devdic)
{
    double comp[3];
    comp[0] = FPNUMVAL(r); comp[1] = FPNUMVAL(g); comp[2] = FPNUMVAL(b);
    return _fillpath_emit(ctx, comp, devdic, 1);
}
#undef FPNUMVAL

static
int _closepath(Xpost_Context *ctx)
{
    Xpost_Object gstate, path;
    char *p;
    unsigned int used, last, sps;
    real co[2];

    gstate = _gstate(ctx);
    if (xpost_object_get_type(gstate) == invalidtype)
        return undefined;
    path = xpost_dict_get(ctx, gstate, namecurrpath);
    if (xpost_object_get_type(path) != stringtype)
        return unregistered;
    p = xpost_string_get_pointer(ctx, path);
    used = _path_get_u32(p, 0);
    if (used <= PATH_HDR)
        return 0;
    last = _path_get_u32(p, 8);
    if (p[last] == PATH_CMD_CLOSE)
        return 0;
    sps = _path_get_u32(p, 4);
    _path_get_coords(p, sps, co, 2);
    return _path_append(ctx, gstate, &path, PATH_CMD_CLOSE, co, 2);
}

/* -  .pathempty  bool
   report whether the current path has no elements */
static
int _pathempty(Xpost_Context *ctx)
{
    Xpost_Object path;
    int empty = 1;

    path = _cpath(ctx);
    if (xpost_object_get_type(path) == stringtype)
    {
        char *p = xpost_string_get_pointer(ctx, path);
        empty = _path_get_u32(p, 0) <= PATH_HDR;
    }
    xpost_stack_push(ctx->lo, ctx->os, xpost_bool_cons(empty));
    return 0;
}

/* x y  .devmoveto  -
   append a move element in device coordinates, bypassing the CTM */
static
int _devmoveto(Xpost_Context *ctx, Xpost_Object x, Xpost_Object y)
{
    Xpost_Object gstate, path;
    real co[2];

    gstate = _gstate(ctx);
    if (xpost_object_get_type(gstate) == invalidtype)
        return undefined;
    path = xpost_dict_get(ctx, gstate, namecurrpath);
    if (xpost_object_get_type(path) != stringtype)
        return unregistered;
    co[0] = x.real_.val;
    co[1] = y.real_.val;
    return _path_append(ctx, gstate, &path, PATH_CMD_MOVE, co, 2);
}

/* x y  .devlineto  -
   append a line element in device coordinates, bypassing the CTM */
static
int _devlineto(Xpost_Context *ctx, Xpost_Object x, Xpost_Object y)
{
    Xpost_Object gstate, path;
    char *p;
    real co[2];

    gstate = _gstate(ctx);
    if (xpost_object_get_type(gstate) == invalidtype)
        return undefined;
    path = xpost_dict_get(ctx, gstate, namecurrpath);
    if (xpost_object_get_type(path) != stringtype)
        return unregistered;
    p = xpost_string_get_pointer(ctx, path);
    if (_path_get_u32(p, 0) <= PATH_HDR)
        return nocurrentpoint;
    co[0] = x.real_.val;
    co[1] = y.real_.val;
    return _path_append(ctx, gstate, &path, PATH_CMD_LINE, co, 2);
}

/* x1 y1 x2 y2 x3 y3  .devcurveto  -
   append a curve element in device coordinates, bypassing the CTM */
static
int _devcurveto(Xpost_Context *ctx,
                Xpost_Object x1, Xpost_Object y1,
                Xpost_Object x2, Xpost_Object y2,
                Xpost_Object x3, Xpost_Object y3)
{
    Xpost_Object gstate, path;
    char *p;
    real co[6];

    gstate = _gstate(ctx);
    if (xpost_object_get_type(gstate) == invalidtype)
        return undefined;
    path = xpost_dict_get(ctx, gstate, namecurrpath);
    if (xpost_object_get_type(path) != stringtype)
        return unregistered;
    p = xpost_string_get_pointer(ctx, path);
    if (_path_get_u32(p, 0) <= PATH_HDR)
        return nocurrentpoint;
    co[0] = x1.real_.val;
    co[1] = y1.real_.val;
    co[2] = x2.real_.val;
    co[3] = y2.real_.val;
    co[4] = x3.real_.val;
    co[5] = y3.real_.val;
    return _path_append(ctx, gstate, &path, PATH_CMD_CURVE, co, 6);
}

/* path  .copypath  path'
   value copy of a packed path (for gsave) */
static
int _copypath(Xpost_Context *ctx, Xpost_Object path)
{
    Xpost_Object np;
    char *p, *q;
    unsigned int used;

    if (xpost_object_get_type(path) != stringtype)
        return typecheck;
    if (_path_avail(ctx, path) < PATH_HDR)
        return rangecheck;
    p = xpost_string_get_pointer(ctx, path);
    used = _path_get_u32(p, 0);
    /* offset 0 is the byte extent copied and offset 12 the capacity the
       destination is sized to; both must fit the source allocation so the
       copy neither reads past the string nor writes past the new path */
    if (used < PATH_HDR || used > _path_avail(ctx, path)
            || used > _path_get_u32(p, 12))
        return rangecheck;
    np = _path_cons(ctx, _path_cap(ctx, path));
    if (xpost_object_get_type(np) == invalidtype)
        return VMerror;
    q = xpost_string_get_pointer(ctx, np);
    p = xpost_string_get_pointer(ctx, path); /* re-derive after cons */
    {
        unsigned int cap = _path_get_u32(q, 12);
        memcpy(q, p, used);
        _path_set_u32(q, 12, cap); /* keep the copy's own capacity */
    }
    xpost_stack_push(ctx->lo, ctx->os, np);
    return 0;
}

/* -  .retagclose  -
   convert a final line element whose endpoint the caller has verified
   equals the subpath start into a close element, in place */
static
int _retagclose(Xpost_Context *ctx)
{
    Xpost_Object path;
    char *p;
    unsigned int used, last;

    path = _cpath(ctx);
    if (xpost_object_get_type(path) != stringtype)
        return unregistered;
    /* an in-place mutation of the current path: back it up for restore
       as _path_append does (see the note there) */
    {
        unsigned int pent = xpost_object_get_ent(path);
        if (!xpost_save_ent_is_saved(ctx->lo, pent))
            if (!xpost_save_save_ent(ctx->lo, stringtype, 0, pent))
                return VMerror;
    }
    p = xpost_string_get_pointer(ctx, path);
    used = _path_get_u32(p, 0);
    if (used <= PATH_HDR)
        return 0;
    last = _path_get_u32(p, 8);
    if (p[last] == PATH_CMD_LINE)
        p[last] = PATH_CMD_CLOSE;
    return 0;
}

/* str off  .pathnext  coords... cmd nextoff true
   str off  .pathnext  false
   step a packed path enumeration: pushes the element at byte offset
   off (its point for move and line, three points for curve, nothing
   for close), the command code, and the offset of the following
   element, or false when off is past the end */
static
int _pathnext(Xpost_Context *ctx, Xpost_Object str, Xpost_Object off)
{
    char *p;
    unsigned int used, o;
    int cmd, n, k;
    real co[6];

    if (xpost_object_get_type(str) != stringtype)
        return typecheck;
    if (_path_avail(ctx, str) < PATH_HDR)
        return rangecheck;
    p = xpost_string_get_pointer(ctx, str);
    used = _path_get_u32(p, 0);
    if (used > _path_avail(ctx, str))
        return rangecheck;
    o = off.int_.val < PATH_HDR ? PATH_HDR : (unsigned int)off.int_.val;
    if (o >= used)
    {
        xpost_stack_push(ctx->lo, ctx->os, xpost_bool_cons(0));
        return 0;
    }
    cmd = p[o];
    if (cmd < PATH_CMD_MOVE || cmd > PATH_CMD_CLOSE)
        return unregistered;
    if (cmd != PATH_CMD_CLOSE)
    {
        n = cmd == PATH_CMD_CURVE ? 6 : 2;
        if (o + _path_elem_size(cmd) > used)
            return rangecheck;
        _path_get_coords(p, o, co, n);
        for (k = 0; k < n; k++)
            xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons(co[k]));
    }
    xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons(cmd));
    xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons(o + _path_elem_size(cmd)));
    xpost_stack_push(ctx->lo, ctx->os, xpost_bool_cons(1));
    return 0;
}

/* -  pathbbox  llx lly urx ury
   bounding box of the current path in user space (PLRM): the stored
   device-space points are mapped back through the inverse CTM before
   accumulation; an empty path yields four zeros */
static
int _pathbbox(Xpost_Context *ctx)
{
    Xpost_Object path;
    Xpost_Object userdict, gd, gs, psmat;
    real m[6], inv[6], det;
    const real *invp = NULL;
    real minx = 0, miny = 0, maxx = 0, maxy = 0;
    int i, ret;

    /* fetch the CTM and build its inverse; on any irregularity fall
       back to the raw device-space box rather than erroring */
    userdict = xpost_stack_bottomup_fetch(ctx->lo, ctx->ds, 2);
    gd = xpost_dict_get(ctx, userdict, xpost_name_cons(ctx, "graphicsdict"));
    gs = xpost_dict_get(ctx, gd, xpost_name_cons(ctx, "currgstate"));
    psmat = xpost_dict_get(ctx, gs, xpost_name_cons(ctx, "currmatrix"));
    if (xpost_object_get_type(psmat) == arraytype && psmat.comp_.sz == 6)
    {
        for (i = 0; i < 6; i++)
        {
            Xpost_Object e = xpost_array_get(ctx, psmat, i);
            m[i] = xpost_object_get_type(e) == realtype ? e.real_.val
                 : (real)e.int_.val;
        }
        det = m[0] * m[3] - m[1] * m[2];
        if (det != 0)
        {
            inv[0] = m[3] / det;
            inv[1] = -m[1] / det;
            inv[2] = -m[2] / det;
            inv[3] = m[0] / det;
            inv[4] = (m[2] * m[5] - m[3] * m[4]) / det;
            inv[5] = (m[1] * m[4] - m[0] * m[5]) / det;
            invp = inv;
        }
    }

    path = _cpath(ctx);
    if (xpost_object_get_type(path) != stringtype)
        return unregistered;
    ret = _path_walk_bbox(ctx, path, 1, invp, &minx, &miny, &maxx, &maxy);
    if (ret == 1)
    {
        xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons(minx));
        xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons(miny));
        xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons(maxx));
        xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons(maxy));
    }
    else
    {
        xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons(0));
        xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons(0));
        xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons(0));
        xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons(0));
    }
    return 0;
}


static
void _transform(Xpost_Matrix mat, real x, real y, real *xres, real *yres)
{
    *xres = mat.xx * x + mat.xy * y + mat.xz;
    *yres = mat.yx * x + mat.yy * y + mat.yz;
}

static
Xpost_Object _arc_start_proc;

/* Push one circular-arc segment's cubic Bezier onto the operand stack in
   user-space coordinates, forward: the four points run from the angle1
   end to the angle2 end (negative da sweeps clockwise). Push order is
   x1 y1 x2 y2 x3 y3, ready for the curveto the caller schedules; the
   control offsets follow the standard circular approximation. */
static
void _arcbezseg(Xpost_Context *ctx,
                double cx, double cy, double r,
                double a1, double a2)
{
    Xpost_Matrix mat1, mat2, mat3;
    real da_2, sin_a, cos_a;
    real x1, y1, x2, y2, x3, y3;

    xpost_matrix_scale(&mat1, (real)r, (real)r);
    xpost_matrix_translate(&mat2, (real)cx, (real)cy);
    xpost_matrix_mult(&mat2, &mat1, &mat3);
    xpost_matrix_rotate(&mat2, (real)(((a1 + a2) / 2.0) * RAD_PER_DEG));
    xpost_matrix_mult(&mat3, &mat2, &mat1);

    da_2 = (real)(((a2 - a1) / 2.0) * RAD_PER_DEG);
    sin_a = (real)sin(da_2);
    cos_a = (real)cos(da_2);
    x1 = (real)((4 - cos_a) / 3.0);
    y1 = (1 - x1*cos_a) / sin_a;
    x2 = x1;
    x3 = cos_a;
    y2 = y1;
    y1 = -y1;
    y3 = sin_a;
    _transform(mat1, x1, y1, &x1, &y1);
    _transform(mat1, x2, y2, &x2, &y2);
    _transform(mat1, x3, y3, &x3, &y3);
    xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons(x1));
    xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons(y1));
    xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons(x2));
    xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons(y2));
    xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons(x3));
    xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons(y3));
}

/* Append a circular arc counterclockwise (dir=1) or clockwise (dir=-1)
   from a1 to a2, split at the quadrant boundaries as the reference
   interpreter splits, emitted forward: the first point (at a1) reaches
   the path through moveto-or-lineto and each segment through curveto,
   leaving the current point at a2 as the language requires. The
   scheduled operators transform the user-space coordinates through the
   CTM as they run; the exec stack runs last-pushed first, so segments
   are pushed in reverse. */
static
int _arc_append(Xpost_Context *ctx,
                Xpost_Object x, Xpost_Object y, Xpost_Object r,
                Xpost_Object angle1, Xpost_Object angle2, int dir)
{
    double cx = x.real_.val;
    double cy = y.real_.val;
    double rr = r.real_.val;
    double a1 = angle1.real_.val;
    double a2 = angle2.real_.val;
    double segs[66][2];
    double cur, b;
    int n = 0, i;
    real sx, sy;

    if (dir > 0)
        while (a2 < a1) a2 += 360;
    else
        while (a2 > a1) a2 -= 360;
    /* a runaway sweep would otherwise fill the operand stack a segment
       at a time; sixteen revolutions is beyond any drawing's use */
    if (fabs(a2 - a1) > 5760)
        a2 = a1 + dir * 5760;

    cur = a1;
    b = dir > 0 ? (floor(a1 / 90) + 1) * 90 : (ceil(a1 / 90) - 1) * 90;
    while (n < 65 && (dir > 0 ? b < a2 : b > a2))
    {
        segs[n][0] = cur; segs[n][1] = b; n++;
        cur = b;
        b += dir * 90;
    }
    if (fabs(a2 - cur) > 1e-9)
    {
        segs[n][0] = cur; segs[n][1] = a2; n++;
    }

    for (i = n; i-- > 0; )
        _arcbezseg(ctx, cx, cy, rr, segs[i][0], segs[i][1]);
    sx = (real)(cx + rr * cos(a1 * RAD_PER_DEG));
    sy = (real)(cy + rr * sin(a1 * RAD_PER_DEG));
    xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons(sx));
    xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons(sy));

    for (i = 0; i < n; i++)
        xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(_curveto_opcode));
    xpost_stack_push(ctx->lo, ctx->es, _arc_start_proc);
    return 0;
}

static
int _arc(Xpost_Context *ctx,
         Xpost_Object x, Xpost_Object y, Xpost_Object r,
         Xpost_Object angle1, Xpost_Object angle2)
{
    return _arc_append(ctx, x, y, r, angle1, angle2, 1);
}

static
int _arcn(Xpost_Context *ctx,
          Xpost_Object x, Xpost_Object y, Xpost_Object r,
          Xpost_Object angle1, Xpost_Object angle2)
{
    return _arc_append(ctx, x, y, r, angle1, angle2, -1);
}

/* destination for flattening: appends must be able to re-seat
   /currpath in the graphics state when the path string grows */
typedef struct
{
    Xpost_Object gstate;
    Xpost_Object path;
} _flatten_dst;

static
int _chopcurve(Xpost_Context *ctx, _flatten_dst *dst,
               real x0, real y0,
               real x1, real y1,
               real x2, real y2,
               real x3, real y3,
               real tol)
{
    real x01, y01, x12, y12, x23, y23,
         x012, y012, x123, y123,
         x0123, y0123;
    real x03, y03;

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

#define DIST(xA, yA, xB, yB) \
    sqrt((xB-xA)*(xB-xA) + (yB-yA)*(yB-yA))

    if (DIST(x03, y03, x0123, y0123) < tol)
    {
        real co[2];
        co[0] = x3;
        co[1] = y3;
        return _path_append(ctx, dst->gstate, &dst->path, PATH_CMD_LINE, co, 2);
    }
    else
    {
        int ret;
        ret = _chopcurve(ctx, dst, x0, y0, x01, y01, x012, y012, x0123, y0123, tol);
        if (ret)
            return ret;
        return _chopcurve(ctx, dst, x0123, y0123, x123, y123, x23, y23, x3, y3, tol);
    }
}

static
int _flattenpath (Xpost_Context *ctx)
{
    Xpost_Object gstate, flat;
    Xpost_Object path;
    _flatten_dst dst;
    char *p;
    unsigned int used, o;
    real cp[2] = { 0, 0 };
    real tol;
    int curved = 0;
    int ret;

    gstate = _gstate(ctx);
    if (xpost_object_get_type(gstate) == invalidtype)
        return undefined;
    flat = xpost_dict_get(ctx, gstate, nameflat);
    /* the flatness value bounds the error in device pixels; subdivide
       well inside it so a curve's polygonization classifies the same
       boundary pixels as a renderer that meets the bound exactly */
    tol = NUM(flat) * 0.25;

    path = xpost_dict_get(ctx, gstate, namecurrpath);
    if (xpost_object_get_type(path) != stringtype)
        return unregistered;
    p = xpost_string_get_pointer(ctx, path);
    used = _path_get_u32(p, 0);

    /* a path without curves is already flat: leave it untouched
       rather than rebuild an identical copy */
    for (o = PATH_HDR; o < used; o += _path_elem_size(p[o]))
    {
        if (p[o] < PATH_CMD_MOVE || p[o] > PATH_CMD_CLOSE)
            return unregistered;
        if (p[o] == PATH_CMD_CURVE)
        {
            curved = 1;
            break;
        }
    }
    if (!curved)
        return 0;

    xpost_stack_push(ctx->lo, ctx->hold, path);
    ret = _newpath(ctx);
    if (ret)
        return ret;
    /* _newpath allocates, so the source string may have moved -- refresh the
       pointer before the loop reads it, as the loop already does after each
       append below */
    p = xpost_string_get_pointer(ctx, path);
    dst.gstate = gstate;
    dst.path = xpost_dict_get(ctx, gstate, namecurrpath);

    for (o = PATH_HDR; o < used; o += _path_elem_size(p[o]))
    {
        int cmd = p[o];
        real co[6];

        _path_get_coords(p, o, co, cmd == PATH_CMD_CURVE ? 6 : 2);
        if (cmd == PATH_CMD_CURVE)
        {
            ret = _chopcurve(ctx, &dst,
                             cp[0], cp[1],
                             co[0], co[1], co[2], co[3], co[4], co[5],
                             tol);
            if (ret)
                return ret;
            cp[0] = co[4];
            cp[1] = co[5];
        }
        else
        {
            ret = _path_append(ctx, dst.gstate, &dst.path, cmd, co, 2);
            if (ret)
                return ret;
            cp[0] = co[0];
            cp[1] = co[1];
        }
        /* appends allocate: the source string may have moved */
        p = xpost_string_get_pointer(ctx, path);
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

    _gstate_cached = 0;
    //xpost_memory_table_get_addr(ctx->gl, XPOST_MEMORY_TABLE_SPECIAL_OPERATOR_TABLE, &optadr);
    //optab = (void *)(ctx->gl->base + optadr);

    if (xpost_object_get_type((namegraphicsdict = xpost_name_cons(ctx, "graphicsdict"))) == invalidtype)
        return VMerror;
    if (xpost_object_get_type((namecurrgstate = xpost_name_cons(ctx, "currgstate"))) == invalidtype)
        return VMerror;
    if (xpost_object_get_type((namecurrpath = xpost_name_cons(ctx, "currpath"))) == invalidtype)
        return VMerror;
    if (xpost_object_get_type((nameclipregion = xpost_name_cons(ctx, "clipregion"))) == invalidtype)
        return VMerror;
    if (xpost_object_get_type((nameflat = xpost_name_cons(ctx, "flat"))) == invalidtype)
        return VMerror;
    if (xpost_object_get_type((namecurrmatrix = xpost_name_cons(ctx, "currmatrix"))) == invalidtype)
        return VMerror;


    op = xpost_operator_cons(ctx, "newpath", (Xpost_Op_Func)_newpath, 0, 0);
    INSTALL;
    op = xpost_operator_cons(ctx, "currentpoint", (Xpost_Op_Func)_currentpoint, 0, 0);
    _currentpoint_opcode = op.mark_.padw;
    INSTALL;

    op = xpost_operator_cons(ctx, "moveto", (Xpost_Op_Func)_moveto, 0, 2, floattype, floattype);
    _moveto_opcode = op.mark_.padw;
    INSTALL;

    op = xpost_operator_cons(ctx, "rmoveto", (Xpost_Op_Func)_rmoveto, 0, 2, floattype, floattype);
    INSTALL;
    op = xpost_operator_cons(ctx, "rmoveto_cont", (Xpost_Op_Func)_rmoveto_cont, 0, 4,
                             floattype, floattype, floattype, floattype);
    _rmoveto_cont_opcode = op.mark_.padw;

    op = xpost_operator_cons(ctx, "lineto", (Xpost_Op_Func)_lineto, 0, 2, floattype, floattype);
    _lineto_opcode = op.mark_.padw;
    INSTALL;

    op = xpost_operator_cons(ctx, "rlineto", (Xpost_Op_Func)_rlineto, 0, 2, floattype, floattype);
    INSTALL;
    op = xpost_operator_cons(ctx, "rlineto_cont", (Xpost_Op_Func)_rlineto_cont, 0, 4,
                             floattype, floattype, floattype, floattype);
    _rlineto_cont_opcode = op.mark_.padw;

    op = xpost_operator_cons(ctx, "curveto", (Xpost_Op_Func)_curveto, 0, 6,
                             floattype, floattype, floattype, floattype, floattype, floattype);
    _curveto_opcode = op.mark_.padw;
    INSTALL;

    op = xpost_operator_cons(ctx, "rcurveto", (Xpost_Op_Func)_rcurveto, 0, 6,
                             floattype, floattype, floattype, floattype, floattype, floattype);
    INSTALL;
    op = xpost_operator_cons(ctx, "rcurveto_cont", (Xpost_Op_Func)_rcurveto_cont, 0, 8,
                             floattype, floattype, floattype, floattype, floattype, floattype, floattype, floattype);
    _rcurveto_cont_opcode = op.mark_.padw;

    op = xpost_operator_cons(ctx, "closepath", (Xpost_Op_Func)_closepath, 0, 0);
    INSTALL;

    op = xpost_operator_cons(ctx, ".cliptrivial", (Xpost_Op_Func)_cliptrivial, 1, 0);
    INSTALL;

    op = xpost_operator_cons(ctx, ".fillpolyargs", (Xpost_Op_Func)_fillpolyargs, 1, 0);
    INSTALL;

    op = xpost_operator_cons(ctx, ".newpathstr", (Xpost_Op_Func)_newpathstr, 1, 0);
    INSTALL;
    op = xpost_operator_cons(ctx, ".pathempty", (Xpost_Op_Func)_pathempty, 1, 0);
    INSTALL;
    op = xpost_operator_cons(ctx, ".devmoveto", (Xpost_Op_Func)_devmoveto, 0, 2, floattype, floattype);
    INSTALL;
    op = xpost_operator_cons(ctx, ".devlineto", (Xpost_Op_Func)_devlineto, 0, 2, floattype, floattype);
    INSTALL;
    op = xpost_operator_cons(ctx, ".devcurveto", (Xpost_Op_Func)_devcurveto, 0, 6,
                             floattype, floattype, floattype, floattype, floattype, floattype);
    INSTALL;
    op = xpost_operator_cons(ctx, ".retagclose", (Xpost_Op_Func)_retagclose, 0, 0);
    INSTALL;
    op = xpost_operator_cons(ctx, ".copypath", (Xpost_Op_Func)_copypath, 1, 1, stringtype);
    INSTALL;
    op = xpost_operator_cons(ctx, ".pathnext", (Xpost_Op_Func)_pathnext, 1, 2, stringtype, integertype);
    INSTALL;
    op = xpost_operator_cons(ctx, "pathbbox", (Xpost_Op_Func)_pathbbox, 4, 0);
    INSTALL;

    op = xpost_operator_cons(ctx, "arc", (Xpost_Op_Func)_arc, 0, 5,
                             floattype, floattype, floattype, floattype, floattype);
    INSTALL;
    op = xpost_operator_cons(ctx, "arcn", (Xpost_Op_Func)_arcn, 0, 5,
                             floattype, floattype, floattype, floattype, floattype);
    INSTALL;

    op = xpost_operator_cons(ctx, "flattenpath", (Xpost_Op_Func)_flattenpath, 0, 0);
    INSTALL;

    op = xpost_operator_cons(ctx, ".pdffillpath", (Xpost_Op_Func)_pdffillpath, 0, 1,
            dicttype);
    INSTALL;
    op = xpost_operator_cons(ctx, ".svgfillpath", (Xpost_Op_Func)_svgfillpath, 0, 4,
            numbertype, numbertype, numbertype, dicttype);
    INSTALL;

    _arc_start_proc = xpost_array_cons(ctx, 4);
    xpost_array_put(ctx, _arc_start_proc, 0, xpost_object_cvx(xpost_name_cons(ctx, ".pathempty")));
    {
        Xpost_Object true_clause = xpost_object_cvx(xpost_array_cons(ctx, 1));
        xpost_array_put(ctx, true_clause, 0, xpost_object_cvx(xpost_name_cons(ctx, "moveto")));
        xpost_array_put(ctx, _arc_start_proc, 1, true_clause);
    }
    {
        Xpost_Object false_clause = xpost_object_cvx(xpost_array_cons(ctx, 1));
        xpost_array_put(ctx, false_clause, 0, xpost_object_cvx(xpost_name_cons(ctx, "lineto")));
        xpost_array_put(ctx, _arc_start_proc, 2, false_clause);
    }
    xpost_array_put(ctx, _arc_start_proc, 3, xpost_object_cvx(xpost_name_cons(ctx, "ifelse")));

    return 0;
}
