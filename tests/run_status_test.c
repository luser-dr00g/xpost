/*
 * Embedding-contract test: xpost_run outcomes and error identity.
 *
 * A persistent context serves several programs in sequence; the test
 * checks that completion, showpage-yield, uncaught errors and post-error
 * reuse all report correctly, and that the error getters identify the
 * PostScript error by name.
 */

#include <stdio.h>
#include <string.h>
#include "xpost.h"

static int failures = 0;

static void check(int cond, const char *what)
{
    if (!cond)
    {
        printf("FAIL: %s\n", what);
        failures++;
    }
}

int main(void)
{
    Xpost_Context *ctx;
    Xpost_Run_Status st;

    if (!xpost_init())
    {
        printf("FAIL: xpost_init\n");
        return 1;
    }
    ctx = xpost_create("null", XPOST_OUTPUT_DEFAULT, NULL,
                       XPOST_SHOWPAGE_RETURN, XPOST_OUTPUT_MESSAGE_QUIET,
                       XPOST_USE_SIZE, 100, 100);
    if (!ctx)
    {
        printf("FAIL: xpost_create\n");
        return 1;
    }
    xpost_job_snapshots_set(ctx, 0);

    /* completion */
    st = xpost_run(ctx, XPOST_INPUT_STRING, "/x 1 2 add def", 0);
    check(st == XPOST_RUN_COMPLETE, "simple program completes");
    check(xpost_error_name_get(ctx)[0] == '\0', "no error name after success");

    /* yield at showpage, then resume to completion */
    st = xpost_run(ctx, XPOST_INPUT_STRING, "showpage /y 2 def", 0);
    check(st == XPOST_RUN_YIELDED, "showpage yields");
    st = xpost_run(ctx, XPOST_INPUT_RESUME, "", 0);
    check(st == XPOST_RUN_COMPLETE, "resume runs to completion");

    /* uncaught error is reported and identified */
    st = xpost_run(ctx, XPOST_INPUT_STRING, "1 0 div", 0);
    check(st == XPOST_RUN_ERRORED, "uncaught error reports ERRORED");
    check(strcmp(xpost_error_name_get(ctx), "undefinedresult") == 0,
          "error name identifies the error");

    /* program-raised error with errorinfo, $error-and-stop style */
    st = xpost_run(ctx, XPOST_INPUT_STRING,
        "$error /errorname /sometestfailure put "
        "$error /errorinfo (extra detail) put "
        "$error /newerror true put stop", 0);
    check(st == XPOST_RUN_ERRORED, "program-raised stop reports ERRORED");
    check(strcmp(xpost_error_name_get(ctx), "sometestfailure") == 0,
          "program-raised error name is reported");
    check(strcmp(xpost_error_info_get(ctx), "extra detail") == 0,
          "errorinfo detail is reported");

    /* an error caught by the program is not an errored run */
    st = xpost_run(ctx, XPOST_INPUT_STRING, "{ 1 0 div } stopped pop", 0);
    check(st == XPOST_RUN_COMPLETE, "caught error still completes");
    check(xpost_error_name_get(ctx)[0] == '\0',
          "no error name when the program caught it");

    /* the context stays healthy across all of the above */
    st = xpost_run(ctx, XPOST_INPUT_STRING,
        "x 3 eq { (STATE-OK) print } if flush", 0);
    check(st == XPOST_RUN_COMPLETE, "context reusable after errors");

    xpost_destroy(ctx);
    xpost_quit();

    if (failures == 0)
        printf("SUCCESS\n");
    return failures ? 1 : 0;
}
