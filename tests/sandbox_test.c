/* File-access sandbox: once engaged, a program may open only files within
   permitted directories. Builds a temporary tree, engages, and checks that
   permitted reads and writes succeed while others are refused. */

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

/* the Windows CRT mkdir takes no mode argument */
#ifdef _WIN32
# define test_mkdir(p) mkdir(p)
#else
# define test_mkdir(p) mkdir((p), 0700)
#endif

#include "xpost.h"

static int failures = 0;

/* swallow the interpreter's error report for the runs meant to fail */
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

/* run a one-line program and report whether it completed */
static int completes(Xpost_Context *ctx, const char *prog)
{
    return xpost_run(ctx, XPOST_INPUT_STRING, prog, 0) == XPOST_RUN_COMPLETE;
}

/* run a program expected to fail, and report the error name */
static int errors_with(Xpost_Context *ctx, const char *prog, const char *name)
{
    Xpost_Run_Status st = xpost_run(ctx, XPOST_INPUT_STRING, prog, 0);
    return st == XPOST_RUN_ERRORED &&
           strcmp(xpost_error_name_get(ctx), name) == 0;
}

int main(void)
{
    Xpost_Context *ctx;
    char root[] = "xpost_sbx_XXXXXX";  /* relative: a native binary need not share /tmp */
    char wdir[512];
    char readable[600];
    char outside[600];
    char prog[900];
    FILE *w;

    if (!mkdtemp(root)) { printf("FAIL: mkdtemp\n"); return 1; }
    snprintf(readable, sizeof readable, "%s/readable", root);
    w = fopen(readable, "wb");
    if (w) { fputs("DATA", w); fclose(w); }
    snprintf(wdir, sizeof wdir, "%s/wdir", root);
    test_mkdir(wdir);
    snprintf(outside, sizeof outside, "%s.outside", root);
    w = fopen(outside, "wb");
    if (w) { fputs("SECRET", w); fclose(w); }

    if (!xpost_init()) { printf("FAIL: xpost_init\n"); return 1; }
    ctx = xpost_create("null", XPOST_OUTPUT_DEFAULT, NULL,
                       XPOST_SHOWPAGE_RETURN, XPOST_OUTPUT_MESSAGE_QUIET,
                       XPOST_USE_SIZE, 100, 100);
    if (!ctx) { printf("FAIL: xpost_create\n"); return 1; }
    xpost_job_snapshots_set(ctx, 0);
    xpost_stderr_handler_set(ctx, discard, NULL);

    /* before engaging, any readable file opens */
    snprintf(prog, sizeof prog, "(%s) (r) file closefile", outside);
    check(completes(ctx, prog), "unrestricted read before engaging");

    /* permit reading the tree and writing one subdirectory, then engage */
    check(xpost_path_permit_read(root) == 1, "permit read accepted");
    check(xpost_path_permit_write(wdir) == 1, "permit write accepted");
    xpost_path_control_engage();

    /* permitted read succeeds */
    snprintf(prog, sizeof prog, "(%s) (r) file closefile", readable);
    check(completes(ctx, prog), "permitted read succeeds");

    /* a read outside the permitted tree is refused */
    snprintf(prog, sizeof prog, "(%s) (r) file", outside);
    check(errors_with(ctx, prog, "invalidfileaccess"), "outside read refused");

    /* write into the permitted subdirectory succeeds */
    snprintf(prog, sizeof prog,
             "(%s/out.txt) (w) file dup (hi) writestring closefile", wdir);
    check(completes(ctx, prog), "permitted write succeeds");

    /* write elsewhere in the tree is refused (only wdir may be written) */
    snprintf(prog, sizeof prog, "(%s/evil.txt) (w) file", root);
    check(errors_with(ctx, prog, "invalidfileaccess"), "unpermitted write refused");

    /* deletefile and renamefile are control operations, not opens: a target
       outside the permitted set is refused rather than escaping the latch */
    snprintf(prog, sizeof prog, "(%s) deletefile", outside);
    check(errors_with(ctx, prog, "invalidfileaccess"), "outside deletefile refused");
    snprintf(prog, sizeof prog, "(%s) (%s/moved) renamefile", outside, wdir);
    check(errors_with(ctx, prog, "invalidfileaccess"), "outside renamefile refused");

    /* deletefile within the permitted write dir succeeds */
    snprintf(prog, sizeof prog, "(%s/out.txt) deletefile", wdir);
    check(completes(ctx, prog), "permitted deletefile succeeds");

    /* directory enumeration hides files the program could not open: the proc
       (which would error) is never reached for an unpermitted match */
    snprintf(prog, sizeof prog,
             "(%s) { pop zzznotanop } 256 string filenameforall", outside);
    check(completes(ctx, prog), "enumeration hides unpermitted files");

    /* the environment is neither read nor written once engaged */
    check(errors_with(ctx, "(PATH) getenv", "invalidaccess"), "getenv refused");
    check(errors_with(ctx, "(X) (y) putenv", "invalidaccess"), "putenv refused");

    /* the latch is one-way: the permit set is frozen after engaging */
    check(xpost_path_permit_read("/") == 0, "permit refused after engage");

    xpost_destroy(ctx);
    xpost_quit();

    /* cleanup (best effort) */
    snprintf(prog, sizeof prog, "%s/out.txt", wdir);
    unlink(prog);
    unlink(readable);
    unlink(outside);
    rmdir(wdir);
    rmdir(root);

    if (failures) { printf("%d failure(s)\n", failures); return 1; }
    printf("SUCCESS\n");
    return 0;
}
