#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "m.h"
#include "ob.h"
#include "s.h"
#include "itp.h"

/* placeholder error function */
/* ultimately, this will do a longjmp back to the central loop */
void error(char *msg) {
    fprintf(stderr, "\nError: %s", msg);
    perror("\nlast system error:");

    printf("\nError: %s", msg);
    context *ctx;
    ctx = &itpdata->ctab[0];
    printf("\nopstack: ");
    dumpstack(ctx->lo, ctx->os);
    printf("\nexecstack: ");
    dumpstack(ctx->lo, ctx->es);
    printf("\ndictstack: ");
    dumpstack(ctx->lo, ctx->ds);

    dumpmfile(ctx->lo);
    dumpmtab(ctx->lo, 0);
    dumpmfile(ctx->gl);
    dumpmtab(ctx->gl, 0);

    dumpstack(ctx->gl, adrent(ctx->gl, NAMES));

    exit(EXIT_FAILURE);
}

