/*
 * Xpost - a Level-2 Postscript interpreter
 * Copyright (C) 2013, Michael Joshua Ryan
 * Copyright (C) 2013, Thorsten Behrens
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
#include <errno.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xpost_log.h"
#include "xpost_memory.h"  /* Xpost_Memory_File */
#include "xpost_object.h"  /* Xpost_Object */
#include "xpost_stack.h"  /* stack */
#include "../lib/xpost_error.h"

#include "xpost_context.h"
#include "xpost_interpreter.h"  /* access context struct */
#include "xpost_dict.h"  /* access dict objects */
#include "xpost_string.h"  /* access string objects */
#include "xpost_error.h"  /* double-check prototypes */
#include "xpost_name.h"  /* create names */

/*#define EMITONERROR */


char *errorname[] = { ERRORS(XPOST_OBJECT_AS_STR) };


volatile char *errormsg = "";


/* placeholder error function */
/* ultimately, this will do a longjmp back
   to the central loop */
void error(unsigned err,
           char *msg)
{
    Xpost_Context *ctx;
    unsigned int gnad;
    unsigned int lnad;

    errormsg = msg;
    if (!initializing && jbmainloopset && !itpdata->in_onerror) {
        longjmp(jbmainloop, err);
    }

    /* following will become "fallback" code
       if jmpbuf is not set */
    xpost_error_begin_dump();

    XPOST_ERROR_DUMP("\nError: %s", errorname[err]);
    XPOST_ERROR_DUMP("\nObject: ");
    xpost_object_dump(itpdata->ctab[0].currentobject);
    XPOST_ERROR_DUMP("\nExtra: %s", msg);
    XPOST_ERROR_DUMP("\nlast system error:", strerror(errno));

    XPOST_ERROR_DUMP("\nError: %s", errorname[err]);
    XPOST_ERROR_DUMP("\nExtra: %s", msg);

    ctx = &itpdata->ctab[0];
    XPOST_ERROR_DUMP("\nopstack: ");
    xpost_stack_dump(ctx->lo, ctx->os);
    XPOST_ERROR_DUMP("\nexecstack: ");
    xpost_stack_dump(ctx->lo, ctx->es);
    XPOST_ERROR_DUMP("\ndictstack: ");
    xpost_stack_dump(ctx->lo, ctx->ds);
    XPOST_ERROR_DUMP("\nholdstack: ");
    xpost_stack_dump(ctx->lo, ctx->hold);

    XPOST_ERROR_DUMP("\nLocal VM: ");
    xpost_memory_file_dump(ctx->lo);
    xpost_memory_table_dump(ctx->lo);
    XPOST_ERROR_DUMP("\nGlobal VM: ");
    xpost_memory_file_dump(ctx->gl);
    xpost_memory_table_dump(ctx->gl);

    XPOST_ERROR_DUMP("\nGlobal Name Stack: ");
    xpost_memory_table_get_addr(ctx->gl,
            XPOST_MEMORY_TABLE_SPECIAL_NAME_STACK, &gnad);
    xpost_stack_dump(ctx->gl, gnad);
    XPOST_ERROR_DUMP("\nLocal Name Stack: ");
    xpost_memory_table_get_addr(ctx->lo,
            XPOST_MEMORY_TABLE_SPECIAL_NAME_STACK, &lnad);
    xpost_stack_dump(ctx->lo, lnad);

    xpost_error_end_dump();
    exit(EXIT_FAILURE);
}

