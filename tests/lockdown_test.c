/* From-PostScript lockdown: a program permits directories and raises the
   file-access sandbox itself with .lockdown, after which access is confined
   and the permit set is frozen. */

#ifndef _GNU_SOURCE
# define _GNU_SOURCE /* mkdtemp */
#endif

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "xpost.h"

static int failures = 0;

static size_t discard(void *user, const char *buf, size_t len)
{
    (void)user; (void)buf;
    return len;
}

static void check(int cond, const char *what)
{
    if (!cond)
    {
        printf("FAIL: %s\n", what);
        failures++;
    }
}

static int completes(Xpost_Context *ctx, const char *prog)
{
    return xpost_run(ctx, XPOST_INPUT_STRING, prog, 0) == XPOST_RUN_COMPLETE;
}

static int refused(Xpost_Context *ctx, const char *prog)
{
    Xpost_Run_Status st = xpost_run(ctx, XPOST_INPUT_STRING, prog, 0);
    return st == XPOST_RUN_ERRORED &&
           strcmp(xpost_error_name_get(ctx), "invalidfileaccess") == 0;
}

int main(void)
{
    Xpost_Context *ctx;
    char root[] = "/tmp/xpost_lck_XXXXXX";
    char wdir[512];
    char readable[600];
    char outside[600];
    char prog[900];
    FILE *w;

    if (!mkdtemp(root)) { printf("FAIL: mkdtemp\n"); return 1; }
    snprintf(readable, sizeof readable, "%s/readable", root);
    w = fopen(readable, "wb"); if (w) { fputs("DATA", w); fclose(w); }
    snprintf(wdir, sizeof wdir, "%s/wdir", root);
    mkdir(wdir, 0700);
    snprintf(outside, sizeof outside, "%s.outside", root);
    w = fopen(outside, "wb"); if (w) { fputs("SECRET", w); fclose(w); }

    if (!xpost_init()) { printf("FAIL: xpost_init\n"); return 1; }
    ctx = xpost_create("null", XPOST_OUTPUT_DEFAULT, NULL,
                       XPOST_SHOWPAGE_RETURN, XPOST_OUTPUT_MESSAGE_QUIET,
                       XPOST_USE_SIZE, 100, 100);
    if (!ctx) { printf("FAIL: xpost_create\n"); return 1; }
    xpost_job_snapshots_set(ctx, 0);
    xpost_stderr_handler_set(ctx, discard, NULL);

    /* before lockdown, access is unrestricted */
    snprintf(prog, sizeof prog, "(%s) (r) file closefile", outside);
    check(completes(ctx, prog), "read is unrestricted before lockdown");

    /* a trusted prolog permits its working set and locks down */
    snprintf(prog, sizeof prog,
             "(%s) .permitfileread (%s) .permitfilewrite .lockdown", root, wdir);
    check(completes(ctx, prog), "permit + lockdown completes");

    /* permitted read succeeds; outside read is refused */
    snprintf(prog, sizeof prog, "(%s) (r) file closefile", readable);
    check(completes(ctx, prog), "permitted read after lockdown");
    snprintf(prog, sizeof prog, "(%s) (r) file", outside);
    check(refused(ctx, prog), "outside read refused after lockdown");

    /* permitted write succeeds; write elsewhere is refused */
    snprintf(prog, sizeof prog,
             "(%s/out.txt) (w) file dup (hi) writestring closefile", wdir);
    check(completes(ctx, prog), "permitted write after lockdown");
    snprintf(prog, sizeof prog, "(%s/evil.txt) (w) file", root);
    check(refused(ctx, prog), "unpermitted write refused after lockdown");

    /* the permit set is frozen: permitting more after lockdown has no effect */
    snprintf(prog, sizeof prog, "(/tmp) .permitfileread");
    check(completes(ctx, prog), ".permitfileread after lockdown is a no-op call");
    snprintf(prog, sizeof prog, "(%s) (r) file", outside);
    check(refused(ctx, prog), "late permit does not grant access");

    xpost_destroy(ctx);
    xpost_quit();

    snprintf(prog, sizeof prog, "%s/out.txt", wdir); unlink(prog);
    unlink(readable);
    unlink(outside);
    rmdir(wdir);
    rmdir(root);

    if (failures) { printf("%d failure(s)\n", failures); return 1; }
    printf("SUCCESS\n");
    return 0;
}
