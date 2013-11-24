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
#include <stdio.h>
#include <stdlib.h> /* NULL strtod */
#include <string.h>

#include "xpost_log.h"
#include "xpost_memory.h"
#include "xpost_object.h"
#include "xpost_stack.h"

#include "xpost_save.h"
#include "xpost_context.h"
#include "xpost_interpreter.h"
#include "xpost_error.h"
#include "xpost_name.h"
#include "xpost_string.h"
#include "xpost_dict.h"
#include "xpost_operator.h"
#include "xpost_op_save.h"

/* -  save  save
   create save object representing vm contents */
static
int Zsave (Xpost_Context *ctx)
{
    if (!xpost_stack_push(ctx->lo, ctx->os, xpost_save_create_snapshot_object(ctx->lo)))
        return stackoverflow;
    printf("save\n");
    return 0;
}

/* save  restore  -
   rewind vm to saved state */
static
int Vrestore (Xpost_Context *ctx,
               Xpost_Object V)
{
    int z;
    unsigned int vs;
    int ret;

    ret = xpost_memory_table_get_addr(ctx->lo,
            XPOST_MEMORY_TABLE_SPECIAL_SAVE_STACK, &vs);
    if (!ret)
    {
        XPOST_LOG_ERR("cannot retrieve address for save stack");
        return VMerror;
    }
    z = xpost_stack_count(ctx->lo, vs);
    while(z > V.save_.lev) {
        xpost_save_restore_snapshot(ctx->lo);
        z--;
    }
    printf("restore\n");
    return 0;
}

/* bool  setglobal  -
   set vm allocation mode in current context. true is global. */
static
int Bsetglobal (Xpost_Context *ctx,
                 Xpost_Object B)
{
    ctx->vmmode = B.int_.val? GLOBAL: LOCAL;
    return 0;
}

/* -  currentglobal  bool
   return vm allocation mode for current context */
static
int Zcurrentglobal (Xpost_Context *ctx)
{
    xpost_stack_push(ctx->lo, ctx->os, xpost_cons_bool(ctx->vmmode==GLOBAL));
    return 0;
}

/* any  gcheck  bool
   check whether value is a legal element of a global compound object */
static
int Agcheck (Xpost_Context *ctx,
              Xpost_Object A)
{
    Xpost_Object r;
    switch(xpost_object_get_type(A)) {
    default:
            r = xpost_cons_bool(0); break;
    case stringtype:
    case nametype:
    case dicttype:
    case arraytype:
            r = xpost_cons_bool((A.tag&XPOST_OBJECT_TAG_DATA_FLAG_BANK)!=0);
    }
    xpost_stack_push(ctx->lo, ctx->os, r);
    return 0;
}

#if 0
/* -  vmstatus  level used max
   return size information for (local) vm */
static
int Zvmstatus (Xpost_Context *ctx)
{
    unsigned int vs;

    xpost_memory_table_get_addr(ctx->lo,
            XPOST_MEMORY_TABLE_SPECIAL_SAVE_STACK, &vs);
    xpost_stack_push(ctx->lo, ctx->os, xpost_cons_int(xpost_stack_count(ctx->lo, vs)));
    xpost_stack_push(ctx->lo, ctx->os, xpost_cons_int(ctx->lo->used));
    xpost_stack_push(ctx->lo, ctx->os, xpost_cons_int(ctx->lo->max));
    return 0;
}
#endif

int initopv(Xpost_Context *ctx,
             Xpost_Object sd)
{
    oper *optab;
    Xpost_Object n,op;
    unsigned int optadr;

    assert(ctx->gl->base);
    xpost_memory_table_get_addr(ctx->gl,
            XPOST_MEMORY_TABLE_SPECIAL_OPERATOR_TABLE, &optadr);
    optab = (void *)(ctx->gl->base + optadr);

    op = consoper(ctx, "save", Zsave, 1, 0); INSTALL;
    op = consoper(ctx, "restore", Vrestore, 0, 1, savetype); INSTALL;
    op = consoper(ctx, "setglobal", Bsetglobal, 0, 1, booleantype); INSTALL;
    op = consoper(ctx, "currentglobal", Zcurrentglobal, 1, 0); INSTALL;
    op = consoper(ctx, "gcheck", Agcheck, 1, 1, anytype); INSTALL;
#if 0
    op = consoper(ctx, "vmstatus", Zvmstatus, 3, 0); INSTALL;
#endif

    /* dumpdic(ctx->gl, sd); fflush(NULL);
    bdcput(ctx, sd, consname(ctx, "mark"), mark); */

    return 0;
}


