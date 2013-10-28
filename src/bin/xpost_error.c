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

#ifdef HAVE_STDBOOL_H
# include <stdbool.h>
#else
# ifndef HAVE__BOOL
#  ifdef __cplusplus
typedef bool _Bool;
#  else
#   define _Bool signed char
#  endif
# endif
# define bool _Bool
# define false 0
# define true 1
# define __bool_true_false_are_defined 1
#endif

#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xpost_memory.h"  // mfile
#include "xpost_object.h"  // object
#include "xpost_stack.h"  // stack
#include "xpost_interpreter.h"  // access context struct
#include "xpost_dict.h"  // access dict objects
#include "xpost_string.h"  // access string objects
#include "xpost_error.h"  // double-check prototypes
#include "xpost_name.h"  // create names

//#define EMITONERROR

char *errorname[] = { ERRORS(XPOST_OBJECT_AS_STR) };

volatile char *errormsg = "";

static int in_onerror;

/* placeholder error function */
/* ultimately, this will do a longjmp back
   to the central loop */
void error(unsigned err,
        char *msg)
{
    context *ctx;


    errormsg = msg;
    if (!initializing && jbmainloopset && !in_onerror) {
        longjmp(jbmainloop, err);
    }

    /* following will become "fallback" code
       if jmpbuf is not set */
    fprintf(stderr, "\nError: %s", errorname[err]);
    fprintf(stderr, "\nObject: ");
    xpost_object_dump(itpdata->ctab[0].currentobject);
    fprintf(stderr, "\nExtra: %s", msg);
    perror("\nlast system error:");

    printf("\nError: %s", errorname[err]);
    printf("\nExtra: %s", msg);

    ctx = &itpdata->ctab[0];
    printf("\nopstack: ");
    dumpstack(ctx->lo, ctx->os);
    printf("\nexecstack: ");
    dumpstack(ctx->lo, ctx->es);
    printf("\ndictstack: ");
    dumpstack(ctx->lo, ctx->ds);

    printf("\nLocal VM: ");
    dumpmfile(ctx->lo);
    dumpmtab(ctx->lo, 0);
    printf("\nGlobal VM: ");
    dumpmfile(ctx->gl);
    dumpmtab(ctx->gl, 0);

    printf("\nGlobal Name Stack: ");
    dumpstack(ctx->gl, adrent(ctx->gl, NAMES));
    printf("\nLocal Name Stack: ");
    dumpstack(ctx->lo, adrent(ctx->lo, NAMES));

    exit(EXIT_FAILURE);
}


/* called by itp:loop() after longjmp from error()
   pushes postscript-level error procedures
   and resumes normal execution.
 */
void onerror(context *ctx,
        unsigned err)
{
    Xpost_Object sd;
    Xpost_Object dollarerror;
    char *errmsg;

    assert(ctx);
    assert(ctx->gl);
    assert(ctx->gl->base);
    assert(ctx->lo);
    assert(ctx->lo->base);

    in_onerror = true;

#ifdef EMITONERROR
    fprintf(stderr, "err: %s\n", errorname[err]);
#endif

    //reset stack
    if (xpost_object_get_type(ctx->currentobject) == operatortype
            && ctx->currentobject.tag & XPOST_OBJECT_TAG_DATA_FLAG_OPARGSINHOLD) {
        int n = ctx->currentobject.mark_.pad0;
        int i;
        for (i=0; i < n; i++) {
            push(ctx->lo, ctx->os, bot(ctx->lo, ctx->hold, i));
        }
    }

    //printf("1\n");
    sd = bot(ctx->lo, ctx->ds, 0);
    //printf("2\n");

    dollarerror = bdcget(ctx, sd, consname(ctx, "$error"));
    //printf("3\n");
    /* FIXME: does errormsg need to be volatile ?? If no, below cast is useless */
    errmsg = (char *)errormsg;
    //printf("4\n");
    if (err == VMerror) {
        bdcput(ctx, dollarerror,
                consname(ctx, "Extra"),
                null);
    } else {
        unsigned mode = ctx->vmmode;
        ctx->vmmode = GLOBAL;
        bdcput(ctx, dollarerror,
                consname(ctx, "Extra"),
                consbst(ctx, strlen(errmsg), errmsg));
        ctx->vmmode = mode;
    }
    //printf("5\n");

    push(ctx->lo, ctx->os, ctx->currentobject);
    //printf("6\n");
    push(ctx->lo, ctx->os, xpost_object_cvlit(consname(ctx, errorname[err])));
    //printf("7\n");
    push(ctx->lo, ctx->es, consname(ctx, "signalerror"));
    //printf("8\n");

    in_onerror = false;
}

