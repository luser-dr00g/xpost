/* Implicit resource loading: findresource, on a miss, loads a named
   instance from the resource search path, confined and leaf-validated.
   Builds a temporary resource tree on disk and drives findresource. */

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
static char out_buf[256];
static size_t out_len = 0;

static size_t out_sink(void *user, const char *buf, size_t len)
{
    (void)user;
    if (out_len + len < sizeof out_buf)
    {
        memcpy(out_buf + out_len, buf, len);
        out_len += len;
    }
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

int main(void)
{
    Xpost_Context *ctx;
    Xpost_Run_Status st;
    char root[] = "xpost_res_XXXXXX";  /* relative: a native binary need not share /tmp */
    char dir[512];
    char file[600];
    FILE *w;

    if (!mkdtemp(root))
    {
        printf("FAIL: mkdtemp\n");
        return 1;
    }
    snprintf(dir, sizeof dir, "%s/TestCategory", root);
    if (test_mkdir(dir) != 0)
    {
        printf("FAIL: mkdir\n");
        return 1;
    }
    snprintf(file, sizeof file, "%s/TestCategory/testinstance", root);
    w = fopen(file, "wb");
    if (!w)
    {
        printf("FAIL: write instance\n");
        return 1;
    }
    fputs("/testinstance (RESOURCE-OK) /TestCategory defineresource pop\n", w);
    fclose(w);

    /* a second category whose implementation and instance both live on
       disk, exercising category-on-demand loading (the layout used by a
       resource-tree distribution: Category/<name> and <name>/<instance>) */
    snprintf(dir, sizeof dir, "%s/Category", root);
    test_mkdir(dir);
    snprintf(file, sizeof file, "%s/Category/DiskCat", root);
    w = fopen(file, "wb");
    if (w)
    {
        fputs("/DiskCat << /Category /DiskCat >> /Category defineresource pop\n", w);
        fclose(w);
    }
    snprintf(dir, sizeof dir, "%s/DiskCat", root);
    test_mkdir(dir);
    snprintf(file, sizeof file, "%s/DiskCat/diskinst", root);
    w = fopen(file, "wb");
    if (w)
    {
        fputs("/diskinst (DISK-CAT-OK) /DiskCat defineresource pop\n", w);
        fclose(w);
    }

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
    xpost_stdout_handler_set(ctx, out_sink, NULL);

    /* point the search path at the temporary tree via the C API */
    check(xpost_add_resource_dir(ctx, root) == 1, "add resource dir");

    /* create the category (in global VM, like the shared instance table) */
    st = xpost_run(ctx, XPOST_INPUT_STRING,
        "currentglobal true setglobal "
        "/TestCategory << /Category /TestCategory >> /Category defineresource pop "
        "setglobal", 0);
    check(st == XPOST_RUN_COMPLETE, "setup completes");

    /* a miss loads the instance from disk, then resolves it */
    out_len = 0;
    st = xpost_run(ctx, XPOST_INPUT_STRING,
        "/testinstance /TestCategory findresource print flush", 0);
    check(st == XPOST_RUN_COMPLETE, "findresource of a disk instance completes");
    out_buf[out_len] = '\0';
    check(strcmp(out_buf, "RESOURCE-OK") == 0, "loaded instance resolves");

    /* a resolved instance is cached: a second lookup does not error */
    st = xpost_run(ctx, XPOST_INPUT_STRING,
        "/testinstance /TestCategory findresource pop", 0);
    check(st == XPOST_RUN_COMPLETE, "second lookup is served from VM");

    /* an unknown category is itself loaded on demand, then its instance */
    out_len = 0;
    st = xpost_run(ctx, XPOST_INPUT_STRING,
        "/diskinst /DiskCat findresource print flush", 0);
    check(st == XPOST_RUN_COMPLETE, "category-on-demand load completes");
    out_buf[out_len] = '\0';
    check(strcmp(out_buf, "DISK-CAT-OK") == 0, "on-demand category resolves");

    /* a non-leaf key cannot name a file: refused, reported undefinedresource */
    st = xpost_run(ctx, XPOST_INPUT_STRING,
        "(../../nonexistent) /TestCategory findresource", 0);
    check(st == XPOST_RUN_ERRORED, "traversal key errors");
    check(strcmp(xpost_error_name_get(ctx), "undefinedresource") == 0,
          "traversal key is undefinedresource");

    /* an absent instance is reported, not fabricated */
    st = xpost_run(ctx, XPOST_INPUT_STRING,
        "/nope /TestCategory findresource", 0);
    check(st == XPOST_RUN_ERRORED, "absent instance errors");
    check(strcmp(xpost_error_name_get(ctx), "undefinedresource") == 0,
          "absent instance is undefinedresource");

    /* a category that is neither in VM nor on disk errors */
    st = xpost_run(ctx, XPOST_INPUT_STRING,
        "/x /NoSuchCategory findresource", 0);
    check(st == XPOST_RUN_ERRORED, "unknown category errors");
    check(strcmp(xpost_error_name_get(ctx), "undefinedresource") == 0,
          "unknown category is undefinedresource");

    xpost_destroy(ctx);
    xpost_quit();

    snprintf(file, sizeof file, "%s/TestCategory/testinstance", root); unlink(file);
    snprintf(file, sizeof file, "%s/Category/DiskCat", root); unlink(file);
    snprintf(file, sizeof file, "%s/DiskCat/diskinst", root); unlink(file);
    snprintf(dir, sizeof dir, "%s/TestCategory", root); rmdir(dir);
    snprintf(dir, sizeof dir, "%s/Category", root); rmdir(dir);
    snprintf(dir, sizeof dir, "%s/DiskCat", root); rmdir(dir);
    rmdir(root);

    if (failures)
    {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("SUCCESS\n");
    return 0;
}
