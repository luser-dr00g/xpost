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

#undef y0
#undef y1

/*
   The current path is a packed byte string held at /currpath in the
   graphics state, avoiding a dictionary and an array allocation per
   path element:

       header (16 bytes):
           u32 used       total bytes in use, including this header
           u32 sp_start   offset of the current subpath's move element
           u32 last_elem  offset of the most recent element
           u32 cap        allocated capacity in bytes
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
static Xpost_Object namecmd;
static Xpost_Object namedata;
static Xpost_Object namemove;
static Xpost_Object nameline;
static Xpost_Object namecurve;
static Xpost_Object nameclose;
static Xpost_Object nameclipregion;
static Xpost_Object nameflat;

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

#define NUM(x) (xpost_object_get_type(x)==realtype?x.real_.val:(real)x.int_.val)

#define PATH_HDR 16
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

    /* merge a move into an immediately preceding move */
    if (cmd == PATH_CMD_MOVE && used > PATH_HDR)
    {
        unsigned int last = _path_get_u32(p, 8);
        if (p[last] == PATH_CMD_MOVE)
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
    return 0;
}

static
Xpost_Object _gstate(Xpost_Context *ctx)
{
    Xpost_Object gd;
    int ret;

    ret = xpost_op_any_load(ctx, namegraphicsdict);
    if (ret) return invalid;
    gd = xpost_stack_pop(ctx->lo, ctx->os);
    if (xpost_object_get_type(gd) == invalidtype)
        return invalid;
    return xpost_dict_get(ctx, gd, namecurrgstate);
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
    xpost_stack_push(ctx->lo, ctx->os, x);
    xpost_stack_push(ctx->lo, ctx->os, y);
    xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(_moveto_cont_opcode));
    xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(ctx->opcode_shortcuts.transform));
    return 0;
}

static
int _moveto_cont(Xpost_Context *ctx, Xpost_Object x, Xpost_Object y)
{
    Xpost_Object gstate, path;
    real co[2];

    gstate = _gstate(ctx);
    if (xpost_object_get_type(gstate) == invalidtype)
        return undefined;
    path = xpost_dict_get(ctx, gstate, namecurrpath);
    if (xpost_object_get_type(path) != stringtype)
        return unregistered;
    co[0] = NUM(x);
    co[1] = NUM(y);
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
    xpost_stack_push(ctx->lo, ctx->os, x);
    xpost_stack_push(ctx->lo, ctx->os, y);
    xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(_lineto_cont_opcode));
    xpost_stack_push(ctx->lo, ctx->es, xpost_operator_cons_opcode(ctx->opcode_shortcuts.transform));
    return 0;
}

static
int _lineto_cont(Xpost_Context *ctx, Xpost_Object x, Xpost_Object y)
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
    co[0] = NUM(x);
    co[1] = NUM(y);
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
    co[0] = NUM(X1);
    co[1] = NUM(Y1);
    co[2] = NUM(X2);
    co[3] = NUM(Y2);
    co[4] = NUM(X3);
    co[5] = NUM(Y3);
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
   the behaviour of the dictionary-walking predecessors); returns 0 on
   a malformed path or when a curve is present and curves are not
   accepted, 2 on an empty path */
static
int _path_walk_bbox(Xpost_Context *ctx, Xpost_Object path,
                    int accept_curves,
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
            if (!any)
            {
                *minx = *maxx = co[k];
                *miny = *maxy = co[k + 1];
                any = 1;
            }
            else
            {
                if (co[k] < *minx) *minx = co[k];
                if (co[k] > *maxx) *maxx = co[k];
                if (co[k + 1] < *miny) *miny = co[k + 1];
                if (co[k + 1] > *maxy) *maxy = co[k + 1];
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

/* build the polygon argument for the device FillPoly procedure in a
   single traversal: a flat array of [x y] point pairs with subpaths
   separated (and terminated) by null. A close element repeats the
   subpath's first point object. Subpaths of fewer than three points
   cannot enclose area and are dropped. */
static
int _fillpolyargs(Xpost_Context *ctx)
{
    Xpost_Object path;
    Xpost_Object result;
    Xpost_Object *pts = NULL;
    int npts = 0, cappts = 0;
    int start = 0;
    char *p;
    unsigned int used, o;
    int i;

    path = _cpath(ctx);
    if (xpost_object_get_type(path) != stringtype)
        return unregistered;
    p = xpost_string_get_pointer(ctx, path);
    used = _path_get_u32(p, 0);

    for (o = PATH_HDR; o < used; o += _path_elem_size(p[o]))
    {
        int cmd = p[o];
        int n, k;
        real co[6];

        if (npts + 5 > cappts)
        {
            Xpost_Object *npts_;
            cappts = cappts ? cappts * 2 : 64;
            npts_ = realloc(pts, cappts * sizeof *pts);
            if (!npts_)
            {
                free(pts);
                return VMerror;
            }
            pts = npts_;
        }
        if (cmd == PATH_CMD_MOVE && npts > start)
        {
            /* flush the finished subpath */
            if (npts - start < 3)
                npts = start;
            else
                pts[npts++] = null;
            start = npts;
        }
        if (cmd == PATH_CMD_CLOSE)
        {
            /* repeat the first point of this subpath */
            if (npts > start)
                pts[npts++] = pts[start];
            continue;
        }
        n = cmd == PATH_CMD_CURVE ? 6 : 2;
        _path_get_coords(p, o, co, n);
        for (k = 0; k + 1 < n; k += 2)
        {
            Xpost_Object pair;
            pair = xpost_object_cvlit(xpost_array_cons(ctx, 2));
            if (xpost_object_get_type(pair) == invalidtype)
            {
                free(pts);
                return VMerror;
            }
            xpost_array_put(ctx, pair, 0, xpost_real_cons(co[k]));
            xpost_array_put(ctx, pair, 1, xpost_real_cons(co[k + 1]));
            /* the string may have moved if array allocation grew the file */
            p = xpost_string_get_pointer(ctx, path);
            pts[npts++] = pair;
        }
    }
    if (npts > start)
    {
        if (npts - start < 3)
            npts = start;
        else
            pts[npts++] = null;
    }

    result = xpost_object_cvlit(xpost_array_cons(ctx, npts));
    if (xpost_object_get_type(result) == invalidtype)
    {
        free(pts);
        return VMerror;
    }
    for (i = 0; i < npts; i++)
        xpost_array_put(ctx, result, i, pts[i]);
    free(pts);

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
    int accept = 0, ret;

    gstate = _gstate(ctx);
    if (xpost_object_get_type(gstate) == invalidtype)
        return undefined;
    path = xpost_dict_get(ctx, gstate, namecurrpath);
    clipregion = xpost_dict_get(ctx, gstate, nameclipregion);

    if (_path_is_rect(ctx, clipregion, &cminx, &cminy, &cmaxx, &cmaxy))
    {
        ret = _path_walk_bbox(ctx, path, 0, &pminx, &pminy, &pmaxx, &pmaxy);
        /* an empty path is accepted: there is nothing to clip */
        accept = ret == 2 ||
                 (ret == 1 &&
                  pminx >= cminx && pmaxx <= cmaxx &&
                  pminy >= cminy && pmaxy <= cmaxy);
    }

    xpost_stack_push(ctx->lo, ctx->os, xpost_bool_cons(accept));
    return 0;
}

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
   bounding box of the current path in device coordinates (matching
   the behaviour of its PostScript predecessor, which did not undo
   the CTM); an empty path yields four zeros */
static
int _pathbbox(Xpost_Context *ctx)
{
    Xpost_Object path;
    real minx = 0, miny = 0, maxx = 0, maxy = 0;
    int ret;

    path = _cpath(ctx);
    if (xpost_object_get_type(path) != stringtype)
        return unregistered;
    ret = _path_walk_bbox(ctx, path, 1, &minx, &miny, &maxx, &maxy);
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
    //y1 = - (((1 - cos_a) * (cos_a - 3)) / (3 * sin_a));
    y1 = (1 - x1*cos_a) / sin_a;
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
               Xpost_Object flat)
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

    if (DIST(x03, y03, x0123, y0123) < NUM(flat))
    {
        real co[2];
        co[0] = x3;
        co[1] = y3;
        return _path_append(ctx, dst->gstate, &dst->path, PATH_CMD_LINE, co, 2);
    }
    else
    {
        int ret;
        ret = _chopcurve(ctx, dst, x0, y0, x01, y01, x012, y012, x0123, y0123, flat);
        if (ret)
            return ret;
        return _chopcurve(ctx, dst, x0123, y0123, x123, y123, x23, y23, x3, y3, flat);
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
    int curved = 0;
    int ret;

    gstate = _gstate(ctx);
    if (xpost_object_get_type(gstate) == invalidtype)
        return undefined;
    flat = xpost_dict_get(ctx, gstate, nameflat);

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
                             flat);
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
    //xpost_memory_table_get_addr(ctx->gl, XPOST_MEMORY_TABLE_SPECIAL_OPERATOR_TABLE, &optadr);
    //optab = (void *)(ctx->gl->base + optadr);

    if (xpost_object_get_type((namegraphicsdict = xpost_name_cons(ctx, "graphicsdict"))) == invalidtype)
        return VMerror;
    if (xpost_object_get_type((namecurrgstate = xpost_name_cons(ctx, "currgstate"))) == invalidtype)
        return VMerror;
    if (xpost_object_get_type((namecurrpath = xpost_name_cons(ctx, "currpath"))) == invalidtype)
        return VMerror;
    if (xpost_object_get_type((namecmd = xpost_name_cons(ctx, "cmd"))) == invalidtype)
        return VMerror;
    if (xpost_object_get_type((namedata = xpost_name_cons(ctx, "data"))) == invalidtype)
        return VMerror;
    if (xpost_object_get_type((namemove = xpost_name_cons(ctx, "move"))) == invalidtype)
        return VMerror;
    if (xpost_object_get_type((nameline = xpost_name_cons(ctx, "line"))) == invalidtype)
        return VMerror;
    if (xpost_object_get_type((namecurve = xpost_name_cons(ctx, "curve"))) == invalidtype)
        return VMerror;
    if (xpost_object_get_type((nameclose = xpost_name_cons(ctx, "close"))) == invalidtype)
        return VMerror;
    if (xpost_object_get_type((nameclipregion = xpost_name_cons(ctx, "clipregion"))) == invalidtype)
        return VMerror;
    if (xpost_object_get_type((nameflat = xpost_name_cons(ctx, "flat"))) == invalidtype)
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
