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

char *errorname[] = { ERRORS(AS_STR) };

volatile char *errormsg;

/* placeholder error function */
/* ultimately, this will do a longjmp back
   to the central loop */
void error(unsigned err, char *msg) {
    errormsg = msg;
    if (!initializing && jbmainloopset) {
        longjmp(jbmainloop, 1);
    }

    /* following will become "fallback" code
       if jmpbuf is not set */
    fprintf(stderr, "\nError: %s", errorname[err]);
    fprintf(stderr, "\nExtra: %s", msg);
    perror("\nlast system error:");

    printf("\nError: %s", errorname[err]);
    printf("\nExtra: %s", msg);
    context *ctx;
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

    printf("\nName Stack: ");
    dumpstack(ctx->gl, adrent(ctx->gl, NAMES));

    exit(EXIT_FAILURE);
}

/* called by itp:loop() after longjmp from error() */
void onerror(context *ctx, unsigned err) {
    object sd;
    object dollarerror;
    char *errmsg; 
    sd = bot(ctx->lo, ctx->ds, 0);
    dollarerror = bdcget(ctx, sd, consname(ctx, "$error"));
    errmsg = errormsg;
    bdcput(ctx, dollarerror,
            consname(ctx, "Extra"),
            consbst(ctx, strlen(errmsg)-1, errmsg));
    push(ctx->lo, ctx->os, ctx->currentobject);
    push(ctx->lo, ctx->os, cvlit(consname(ctx, errorname[err])));
    push(ctx->lo, ctx->es, consname(ctx, "signalerror"));
}

