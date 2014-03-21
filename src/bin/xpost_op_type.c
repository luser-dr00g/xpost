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
#ifdef HAVE_ALLOCA_H
# include <alloca.h>
#elif !defined alloca
# ifdef __GNUC__
#  define alloca __builtin_alloca
# elif defined _AIX
#  define alloca __alloca
# elif defined _MSC_VER
#  include <malloc.h>
#  define alloca _alloca
# elif !defined HAVE_ALLOCA
#  ifdef  __cplusplus
extern "C"
#  endif
void *alloca (size_t);
# endif
#endif

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h> /* NULL strtod */
#include <string.h>

#include "xpost_log.h"
#include "xpost_memory.h"
#include "xpost_object.h"
#include "xpost_stack.h"

#include "xpost_context.h"
#include "xpost_interpreter.h"
#include "xpost_error.h"
#include "xpost_name.h"
#include "xpost_string.h"
#include "xpost_dict.h"
#include "xpost_operator.h"
#include "xpost_op_type.h"

/* any  type  name
   return type of any as a nametype object */
static
int Atype(Xpost_Context *ctx,
           Xpost_Object o)
{
    if (xpost_object_get_type(o) >= XPOST_OBJECT_NTYPES)
        //return unregistered;
        o = invalid; /* normalize to the all-zero invalid object */
    xpost_stack_push(ctx->lo, ctx->os, xpost_object_cvx(consname(ctx, xpost_object_type_names[xpost_object_get_type(o)])));
    return 0;
}

/* obj   cvlit  obj
   set executable attribute in obj to literal (quoted) */
static
int Acvlit(Xpost_Context *ctx,
            Xpost_Object o)
{
    xpost_stack_push(ctx->lo, ctx->os, xpost_object_cvlit(o));
    return 0;
}

/* obj  cvx  obj
   set executable attribute in obj to executable */
static
int Acvx(Xpost_Context *ctx,
          Xpost_Object o)
{
    xpost_stack_push(ctx->lo, ctx->os, xpost_object_cvx(o));
    return 0;
}

/* obj  xcheck  bool
   test executable attribute in obj */
static
int Axcheck(Xpost_Context *ctx,
             Xpost_Object o)
{
    xpost_stack_push(ctx->lo, ctx->os, xpost_bool_cons(xpost_object_is_exe(o)));
    return 0;
}

/* obj  executeonly  obj
   reduce access attribute for obj to execute-only */
static
int Aexecuteonly(Xpost_Context *ctx,
                  Xpost_Object o)
{
    o.tag &= ~XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_MASK;
    o.tag |= (XPOST_OBJECT_TAG_ACCESS_EXECUTE_ONLY << XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_OFFSET);
    xpost_stack_push(ctx->lo, ctx->os, o);
    return 0;
}

/* obj  noaccess  obj
   reduce access attribute for obj to no-access */
static
int Anoaccess(Xpost_Context *ctx,
               Xpost_Object o)
{
    o.tag &= ~XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_MASK;
    o.tag |= (XPOST_OBJECT_TAG_ACCESS_NONE << XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_OFFSET);
    xpost_stack_push(ctx->lo, ctx->os, o);
    return 0;
}

/* obj  readonly  obj
   reduce access attribute for obj to read-only */
static
int Areadonly(Xpost_Context *ctx,
               Xpost_Object o)
{
    o.tag &= ~XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_MASK;
    o.tag |= (XPOST_OBJECT_TAG_ACCESS_READ_ONLY << XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_OFFSET);
    xpost_stack_push(ctx->lo, ctx->os, o);
    return 0;
}

/* obj  rcheck  bool
   test obj for read-access */
static
int Archeck(Xpost_Context *ctx,
             Xpost_Object o)
{
    xpost_stack_push(ctx->lo, ctx->os, xpost_bool_cons( (o.tag & XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_MASK) >> XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_OFFSET >= XPOST_OBJECT_TAG_ACCESS_READ_ONLY ));
    return 0;
}

/* obj  wcheck  bool
   test obj for write-access */
static
int Awcheck(Xpost_Context *ctx,
             Xpost_Object o)
{
    xpost_stack_push(ctx->lo, ctx->os, xpost_bool_cons( (o.tag & XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_MASK) >> XPOST_OBJECT_TAG_DATA_FLAG_ACCESS_OFFSET == XPOST_OBJECT_TAG_ACCESS_UNLIMITED ));
    return 0;
}

/* number  cvi  int
   convert number to integer */
static
int Ncvi(Xpost_Context *ctx,
          Xpost_Object n)
{
    if (xpost_object_get_type(n) == realtype)
        n = xpost_int_cons(n.real_.val);
    xpost_stack_push(ctx->lo, ctx->os, n);
    return 0;
}

/* string  cvi  int
   convert initial portion of string to integer */
static
int Scvi(Xpost_Context *ctx,
          Xpost_Object s)
{
    double dbl;
    long num;
    char *t = alloca(s.comp_.sz+1);
    memcpy(t, xpost_string_get_pointer(ctx, s), s.comp_.sz);
    t[s.comp_.sz] = '\0';

    dbl = strtod(t, NULL);
    if ((dbl == HUGE_VAL || dbl -HUGE_VAL) && errno==ERANGE)
        return limitcheck;
    if (dbl >= LONG_MAX || dbl <= LONG_MIN)
        return limitcheck;
    num = dbl;

    /*
    num = strtol(t, NULL, 10);
    if ((num == LONG_MAX || num == LONG_MIN) && errno==ERANGE)
        return limitcheck;
    */

    xpost_stack_push(ctx->lo, ctx->os, xpost_int_cons(num));
    return 0;
}

/* string  cvn  name
   convert string to name */
static
int Scvn(Xpost_Context *ctx,
          Xpost_Object s)
{
    char *t;
    Xpost_Object name;

    t = alloca(s.comp_.sz+1);
    memcpy(t, xpost_string_get_pointer(ctx, s), s.comp_.sz);
    t[s.comp_.sz] = '\0';
    name = consname(ctx, t);
    if (xpost_object_get_type(name) == invalidtype)
        return VMerror;
    if (xpost_object_is_exe(s))
        name = xpost_object_cvx(name);
    else
        name = xpost_object_cvlit(name);
    xpost_stack_push(ctx->lo, ctx->os, name);
    return 0;
}

/* number  cvr  real
   convert number to real */
static
int Ncvr(Xpost_Context *ctx,
          Xpost_Object n)
{
    if (xpost_object_get_type(n) == integertype)
        n = xpost_real_cons(n.int_.val);
    xpost_stack_push(ctx->lo, ctx->os, n);
    return 0;
}

/* string  cvr  real
   convert initial portion of string to real */
static
int Scvr(Xpost_Context *ctx,
          Xpost_Object str)
{
    double num;
    char *s = alloca(str.comp_.sz + 1);
    memcpy(s, xpost_string_get_pointer(ctx, str), str.comp_.sz);
    s[str.comp_.sz] = '\0';
    num = strtod(s, NULL);
    if ((num == HUGE_VAL || num -HUGE_VAL) && errno==ERANGE)
        return limitcheck;
    xpost_stack_push(ctx->lo, ctx->os, xpost_real_cons(num));
    return 0;
}

/* helper function: fill buffer with radix representation of num */
static
int conv_rad(int num,
             int rad,
             char *s,
             int n)
{
    char *vec = "0123456789" "ABCDEFGHIJKLM" "NOPQRSTUVWXYZ";
    int off;
    if (n == 0) return 0;
    if (num < rad) {
        *s = vec[num];
        return 1;
    }
    off = conv_rad(num/rad, rad, s, n);
    if ((off == n) || (off == -1)) return -1;
    s[off] = vec[num%rad];
    return off+1;
}

/* number radix string  cvrs  string
   convert number to a radix representation in string */
static
int NRScvrs (Xpost_Context *ctx,
              Xpost_Object num,
              Xpost_Object rad,
              Xpost_Object str)
{
    int r, n;
    if (xpost_object_get_type(num) == realtype) num = xpost_int_cons(num.real_.val);
    r = rad.int_.val;
    if (r < 2 || r > 36)
        return rangecheck;
    n = conv_rad(num.int_.val, r, xpost_string_get_pointer(ctx, str), str.comp_.sz);
    if (n == -1)
        return rangecheck;
    if (n < str.comp_.sz) str.comp_.sz = n;
    xpost_stack_push(ctx->lo, ctx->os, str);
    return 0;
}

/* helper function: fill string with integer */
static
int conv_integ(real num,
               char *s,
               int n)
{
    int off;
    if (num < 10.0) {
        *s = ((int)num) + '0';
        return 1;
    }
    off = conv_integ(num/10.0, s, n);
    if ((off == n) || (off == -1)) return -1;
    s[off] = (((int)num)%10) + '0';
    return off + 1;
}

/* helper function: fill string with fraction */
static
int conv_frac (real num,
               char *s,
               int n)
{
    real integ, frac;
    num *= 10.0;
    integ = floor(num);
    frac = num - integ;
    *s = (int)integ + '0';
    //if (num == 0.0) return 1;
    if (num < 0.0001) return 1;
    return 1 + conv_frac(frac, s+1, n-1);
}

/* helper function: fill string with real decimal representation */
static
int conv_real (real num,
               char *s,
               int n)
{
    int off = 0;
    real integ, frac;
    if (n == 0) return -1;
    if (isinf(num)){
        if (n < 3) return -1;
        memcpy(s, "inf", 3);
        return 3;
    }
    if (isnan(num)){
        if (n < 3) return -1;
        memcpy(s, "nan", 3);
        return 3;
    }
    if (num < 0) {
        num = fabs(num);
        s[off++] = '-';
    }
    integ = floor(num);
    frac = num - integ;
    off += conv_integ(integ, s+off, n);
    s[off++] = '.';
    off += conv_frac(frac, s+off, n-off);
    return off;
}

/* any string  cvs  string
   convert any object to string representation */
static
int AScvs (Xpost_Context *ctx,
            Xpost_Object any,
            Xpost_Object str)
{
    char nostringval[] = "-nostringval-";
    char strue[] = "true";
    char sfalse[] = "false";
    char smark[] = "-mark-";
    char ssave[] = "-save-";
    int n;
    int ret;

    switch(xpost_object_get_type(any)) {
    default:
        if (str.comp_.sz < sizeof(nostringval)-1)
            return rangecheck;
        memcpy(xpost_string_get_pointer(ctx, str), nostringval, sizeof(nostringval)-1);
        str.comp_.sz = sizeof(nostringval)-1;
        break;

    case savetype:
        if (str.comp_.sz < sizeof(ssave)-1)
            return rangecheck;
        memcpy(xpost_string_get_pointer(ctx, str), ssave, sizeof(ssave)-1);
        str.comp_.sz = sizeof(ssave)-1;
        break;

    case marktype:
        if (str.comp_.sz < sizeof(smark)-1)
            return rangecheck;
        memcpy(xpost_string_get_pointer(ctx, str), smark, sizeof(smark)-1);
        str.comp_.sz = sizeof(smark)-1;
        break;

    case booleantype:
        {
            if (any.int_.val) {
                if (str.comp_.sz < sizeof(strue)-1)
                    return rangecheck;
                memcpy(xpost_string_get_pointer(ctx, str), strue, sizeof(strue)-1);
                str.comp_.sz = sizeof(strue)-1;
            } else {
                if (str.comp_.sz < sizeof(sfalse)-1)
                    return rangecheck;
                memcpy(xpost_string_get_pointer(ctx, str), sfalse, sizeof(sfalse)-1);
                str.comp_.sz = sizeof(sfalse)-1;
            }
        }
        break;
    case integertype:
        {
            //n = conv_rad(any.int_.val, 10, xpost_string_get_pointer(ctx, str), str.comp_.sz);
            char *s = xpost_string_get_pointer(ctx, str);
            int sz = str.comp_.sz;
            n = 0;
            if (any.int_.val < 0) {
                s[n++] = '-';
                any.int_.val = abs(any.int_.val);
                --sz;
            }
            n += conv_integ(any.int_.val, s + n, sz);
            if (n == -1)
                return rangecheck;
            if (n < str.comp_.sz) str.comp_.sz = n;
            break;
        }
    case realtype:
        n = conv_real(any.real_.val, xpost_string_get_pointer(ctx, str), str.comp_.sz);
        if (n == -1)
            return rangecheck;
        if (n < str.comp_.sz) str.comp_.sz = n;
        break;

    case operatortype:
        {
            unsigned int optadr;
            oper *optab;
            oper op;
            Xpost_Object_Mark nm;
            ret = xpost_memory_table_get_addr(ctx->gl,
                    XPOST_MEMORY_TABLE_SPECIAL_OPERATOR_TABLE, &optadr);
            if (!ret)
            {
                XPOST_LOG_ERR("cannot load optab!");
                return VMerror;
            }
            optab = (void *)(ctx->gl->base + optadr);
            op = optab[any.mark_.padw];
            nm.tag = nametype | XPOST_OBJECT_TAG_DATA_FLAG_BANK;
            nm.pad0 = 0;
            nm.padw = op.name;
            any.mark_ = nm;
        }
        /*@fallthrough@*/
    case nametype:
        any = strname(ctx, any);
        /*@fallthrough@*/
    case stringtype:
        if (any.comp_.sz > str.comp_.sz)
            return rangecheck;
        if (any.comp_.sz < str.comp_.sz) str.comp_.sz = any.comp_.sz;
        memcpy(xpost_string_get_pointer(ctx, str), xpost_string_get_pointer(ctx, any), any.comp_.sz);
        break;
    }

    xpost_stack_push(ctx->lo, ctx->os, str);
    return 0;
}

int initopt (Xpost_Context *ctx,
              Xpost_Object sd)
{
    oper *optab;
    Xpost_Object n,op;
    unsigned int optadr;

    assert(ctx->gl->base);
    xpost_memory_table_get_addr(ctx->gl,
            XPOST_MEMORY_TABLE_SPECIAL_OPERATOR_TABLE, &optadr);
    optab = (void *)(ctx->gl->base + optadr);

    op = consoper(ctx, "type", Atype, 1, 1, anytype); INSTALL;
    op = consoper(ctx, "cvlit", Acvlit, 1, 1, anytype); INSTALL;
    op = consoper(ctx, "cvx", Acvx, 1, 1, anytype); INSTALL;
    ctx->opcode_shortcuts.cvx = op.mark_.padw;
    op = consoper(ctx, "xcheck", Axcheck, 1, 1, anytype); INSTALL;
    op = consoper(ctx, "executeonly", Aexecuteonly, 1, 1, anytype); INSTALL;
    op = consoper(ctx, "noaccess", Anoaccess, 1, 1, anytype); INSTALL;
    op = consoper(ctx, "readonly", Areadonly, 1, 1, anytype); INSTALL;
    op = consoper(ctx, "rcheck", Archeck, 1, 1, anytype); INSTALL;
    op = consoper(ctx, "wcheck", Awcheck, 1, 1, anytype); INSTALL;
    op = consoper(ctx, "cvi", Ncvi, 1, 1, numbertype); INSTALL;
    op = consoper(ctx, "cvi", Scvi, 1, 1, stringtype); INSTALL;
    op = consoper(ctx, "cvn", Scvn, 1, 1, stringtype); INSTALL;
    op = consoper(ctx, "cvr", Ncvr, 1, 1, numbertype); INSTALL;
    op = consoper(ctx, "cvr", Scvr, 1, 1, stringtype); INSTALL;
    op = consoper(ctx, "cvrs", NRScvrs, 1, 3, numbertype, integertype, stringtype); INSTALL;
    op = consoper(ctx, "cvs", AScvs, 1, 2, anytype, stringtype); INSTALL;

    /* xpost_dict_dump_memory (ctx->gl, sd); fflush(NULL);
     */
    return 0;
}


