#include <assert.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "m.h"
#include "ob.h"
#include "s.h"
#include "itp.h"
#include "di.h"
#include "st.h"
#include "err.h"
#include "nm.h"

//#define EMITONERROR

char *errorname[] = { ERRORS(AS_STR) };

volatile char *errormsg;

static int in_onerror;

/* placeholder error function */
/* ultimately, this will do a longjmp back
   to the central loop */
void error(unsigned err, char *msg) {
    context *ctx;
    errormsg = msg;
    if (!initializing && jbmainloopset && !in_onerror) {
        longjmp(jbmainloop, err);
    }

    /* following will become "fallback" code
       if jmpbuf is not set */
    fprintf(stderr, "\nError: %s", errorname[err]);
    fprintf(stderr, "\nObject: ");
    dumpobject(itpdata->ctab[0].currentobject);
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


/* called by itp:loop() after longjmp from error() */
void onerror(context *ctx, unsigned err) {
    object sd;
    object dollarerror;
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

    //printf("1\n");
    sd = bot(ctx->lo, ctx->ds, 0);
    //printf("2\n");

    dollarerror = bdcget(ctx, sd, consname(ctx, "$error"));
    printf("3\n");
    errmsg = errormsg;
    printf("4\n");
    bdcput(ctx, dollarerror,
            consname(ctx, "Extra"),
            consbst(ctx, strlen(errmsg), errmsg));
    printf("5\n");

    push(ctx->lo, ctx->os, ctx->currentobject);
    //printf("6\n");
    push(ctx->lo, ctx->os, cvlit(consname(ctx, errorname[err])));
    //printf("7\n");
    push(ctx->lo, ctx->es, consname(ctx, "signalerror"));
    //printf("8\n");

    in_onerror = false;
}

