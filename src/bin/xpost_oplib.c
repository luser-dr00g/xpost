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

#include <stdlib.h> /* NULL */

#include "xpost_log.h"
#include "xpost_memory.h"
#include "xpost_object.h"
#include "xpost_stack.h"
#include "xpost_context.h"
#include "xpost_name.h"
#include "xpost_dict.h"

#include "xpost_operator.h"
#include "xpost_oplib.h"

#include "xpost_op_stack.h"
#include "xpost_op_string.h"
#include "xpost_op_array.h"
#include "xpost_op_dict.h"
#include "xpost_op_boolean.h"
#include "xpost_op_control.h"
#include "xpost_op_type.h"
#include "xpost_op_token.h"
#include "xpost_op_math.h"
#include "xpost_op_file.h"
#include "xpost_op_save.h"
#include "xpost_op_misc.h"
#include "xpost_op_packedarray.h"
#include "xpost_op_param.h"
#include "xpost_op_matrix.h"
#include "xpost_op_font.h"
#include "xpost_op_context.h"
#include "xpost_dev_generic.h"
#ifdef HAVE_WIN32
# include "xpost_dev_win32.h"
#endif
#ifdef HAVE_XCB
# include "xpost_dev_xcb.h"
#endif
#include "xpost_dev_bgr.h"

/* no-op operator useful as a break target.
   put 'breakhere' in the postscript program,
   run interpreter under gdb,
   gdb> b breakhere
   gdb> run
   will break in the breakhere function (of course),
   which you can follow back to the main loop (gdb> next),
   just as it's about to read the next token.
 */
int breakhere(Xpost_Context *ctx)
{
    (void)ctx;
    return 0;
}

/* create systemdict and call
   all initop?* functions, installing all operators */
int initop(Xpost_Context *ctx)
{
    Xpost_Object op;
    Xpost_Object n;
    Xpost_Object sd;
    Xpost_Memory_Table *tab;
    unsigned ent;
    Xpost_Operator *optab;
    unsigned int optadr;

    sd = xpost_dict_cons (ctx, SDSIZE);
    if (xpost_object_get_type(sd) == nulltype)
    {
        XPOST_LOG_ERR("cannot allocate systemdict");
        return 0;
    }
    xpost_dict_put(ctx, sd, xpost_name_cons(ctx, "systemdict"), sd);
    xpost_stack_push(ctx->lo, ctx->ds, sd); // push systemdict on dictstack
    tab = NULL;
    ent = xpost_object_get_ent(sd);
    xpost_memory_table_find_relative(ctx->gl, &tab, &ent);
    tab->tab[ent].sz = 0; // make systemdict immune to collection

    xpost_memory_table_get_addr(ctx->gl,
            XPOST_MEMORY_TABLE_SPECIAL_OPERATOR_TABLE, &optadr);
    optab = (void *)(ctx->gl->base + optadr);
#ifdef DEBUGOP
    xpost_dict_dump_memory (ctx->gl, sd); fflush(NULL);
    puts("");
#endif

    initops(ctx, sd);

//#ifdef DEBUGOP
    //printf("\nops:\n"); xpost_dict_dump_memory (ctx->gl, sd); fflush(NULL);
//#endif

    op = xpost_operator_cons(ctx, "breakhere", breakhere, 0, 0);
    INSTALL;

    initopst(ctx, sd);
    //printf("\nopst:\n"); xpost_dict_dump_memory (ctx->gl, sd); fflush(NULL);

    xpost_oper_init_array_ops(ctx, sd);
    //printf("\nopar:\n"); xpost_dict_dump_memory (ctx->gl, sd); fflush(NULL);

    xpost_oper_init_dict_ops(ctx, sd);
    //printf("\nopdi:\n"); xpost_dict_dump_memory (ctx->gl, sd); fflush(NULL);

    xpost_oper_init_bool_ops(ctx, sd);
    //printf("\nopb:\n"); xpost_dict_dump_memory (ctx->gl, sd); fflush(NULL);

    xpost_oper_init_control_ops(ctx, sd);
    //printf("\nopc:\n"); xpost_dict_dump_memory (ctx->gl, sd); fflush(NULL);

    initopt(ctx, sd);
    //printf("\nopt:\n"); xpost_dict_dump_memory (ctx->gl, sd); fflush(NULL);

    initoptok(ctx, sd);
    //printf("\noptok:\n"); xpost_dict_dump_memory (ctx->gl, sd); fflush(NULL);

    initopm(ctx, sd);
    //printf("\nopm:\n"); xpost_dict_dump_memory (ctx->gl, sd); fflush(NULL);

    initopf(ctx, sd);
    //printf("\nopf:\n"); xpost_dict_dump_memory (ctx->gl, sd); fflush(NULL);

    initopv(ctx, sd);
    //printf("\nopv:\n"); xpost_dict_dump_memory (ctx->gl, sd); fflush(NULL);

    initopx(ctx, sd);
    //printf("\nopx:\n"); xpost_dict_dump_memory (ctx->gl, sd); fflush(NULL);

    initoppa(ctx, sd);
    initopparam(ctx, sd);
    initopmatrix(ctx, sd);
    initopfont(ctx, sd);
    initdevgenericops(ctx, sd);
#ifdef _WIN32
    initwin32ops(ctx, sd);
#endif
#ifdef HAVE_XCB
    initxcbops(ctx, sd);
    //printf("xcb:\n");
#endif
    initbgrops(ctx, sd);
    xpost_oper_init_context_ops(ctx, sd);


#ifdef DEBUGOP
    printf("final sd:\n");
    xpost_stack_dump(ctx->lo, ctx->ds);
    xpost_dict_dump_memory (ctx->gl, sd); fflush(NULL);
#endif

    return 1;
}

