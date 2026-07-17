/* Path-component validation and beneath-root confinement.
   Exercises xpost_path_safe_leaf() and xpost_diskfile_fopen_beneath()
   directly, including traversal and symlink-escape attempts. */

#ifndef _GNU_SOURCE
# define _GNU_SOURCE /* mkdtemp */
#endif

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stddef.h> /* size_t */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
# include <unistd.h>
# include <sys/stat.h>
#endif

/* Declared in xpost_file.h; forward-declared here so this focused test
   need not pull in the interpreter object headers. A signature drift
   would fail to link. */
int xpost_path_safe_leaf(const char *s, size_t len);
FILE *xpost_diskfile_fopen_beneath(const char *root, const char *rel, int *err);

static int failures = 0;

static void check(int cond, const char *what)
{
    if (!cond)
    {
        printf("FAIL: %s\n", what);
        failures++;
    }
}

static void test_safe_leaf(void)
{
    /* accepted: identifiers and dotted category names */
    check(xpost_path_safe_leaf("qrcode", 6), "accept qrcode");
    check(xpost_path_safe_leaf("uk.co.terryburton.bwipp", 23), "accept dotted");
    check(xpost_path_safe_leaf("A_b-9.x", 7), "accept mixed charset");
    check(xpost_path_safe_leaf("COM10", 5), "accept COM10 (not reserved)");

    /* rejected: empty, dot components */
    check(!xpost_path_safe_leaf("", 0), "reject empty");
    check(!xpost_path_safe_leaf(".", 1), "reject dot");
    check(!xpost_path_safe_leaf("..", 2), "reject dotdot");

    /* rejected: separators and drive/stream punctuation */
    check(!xpost_path_safe_leaf("a/b", 3), "reject slash");
    check(!xpost_path_safe_leaf("a\\b", 3), "reject backslash");
    check(!xpost_path_safe_leaf("a:b", 3), "reject colon");

    /* rejected: leading/trailing dot or space */
    check(!xpost_path_safe_leaf(".hidden", 7), "reject leading dot");
    check(!xpost_path_safe_leaf(" lead", 5), "reject leading space");
    check(!xpost_path_safe_leaf("trail.", 6), "reject trailing dot");
    check(!xpost_path_safe_leaf("trail ", 6), "reject trailing space");

    /* rejected: NUL and control/non-ASCII bytes (counted, not terminated) */
    check(!xpost_path_safe_leaf("a\0b", 3), "reject embedded NUL");
    check(!xpost_path_safe_leaf("a\x01" "b", 3), "reject control byte");
    check(!xpost_path_safe_leaf("caf\xe9", 4), "reject non-ASCII byte");

    /* rejected: reserved Windows device names, any case, with extension */
    check(!xpost_path_safe_leaf("CON", 3), "reject CON");
    check(!xpost_path_safe_leaf("con", 3), "reject con");
    check(!xpost_path_safe_leaf("con.txt", 7), "reject con.txt");
    check(!xpost_path_safe_leaf("LPT1", 4), "reject LPT1");
    check(!xpost_path_safe_leaf("nul", 3), "reject nul");
}

#ifndef _WIN32
static void test_beneath(void)
{
    char tmpl[] = "/tmp/xpost_conf_XXXXXX";
    char *root;
    char catdir[512];
    char path[512];
    char link[512];
    char outside[560];
    int err;
    FILE *fp;
    FILE *w;

    root = mkdtemp(tmpl);
    check(root != NULL, "mkdtemp");
    if (!root)
        return;

    snprintf(catdir, sizeof catdir, "%s/Category", root);
    check(mkdir(catdir, 0700) == 0, "mkdir Category");
    snprintf(path, sizeof path, "%s/Category/name", root);
    w = fopen(path, "wb");
    check(w != NULL, "create instance file");
    if (w) { fputs("INSTANCE", w); fclose(w); }

    /* a real secret outside the root, reachable only via a symlink */
    snprintf(outside, sizeof outside, "%s.secret", root);
    w = fopen(outside, "wb");
    if (w) { fputs("SECRET", w); fclose(w); }

    /* valid: opens and reads the instance */
    fp = xpost_diskfile_fopen_beneath(root, "Category/name", &err);
    check(fp != NULL, "beneath opens a valid instance");
    if (fp)
    {
        char b[16] = {0};
        size_t n = fread(b, 1, 8, fp);
        check(n == 8 && memcmp(b, "INSTANCE", 8) == 0, "beneath reads content");
        fclose(fp);
    }

    /* traversal: refused */
    fp = xpost_diskfile_fopen_beneath(root, "Category/../../nonexistent", &err);
    check(fp == NULL, "beneath refuses traversal");
    if (fp) fclose(fp);

    /* symlink to an existing file outside the root: refused */
    snprintf(link, sizeof link, "%s/Category/evil", root);
    check(symlink(outside, link) == 0, "create escaping symlink");
    fp = xpost_diskfile_fopen_beneath(root, "Category/evil", &err);
    check(fp == NULL, "beneath refuses symlink escape");
    if (fp) fclose(fp);

    /* missing: reported, not opened */
    fp = xpost_diskfile_fopen_beneath(root, "Category/missing", &err);
    check(fp == NULL && err != 0, "beneath reports a missing instance");
    if (fp) fclose(fp);

    unlink(link);
    unlink(path);
    unlink(outside);
    rmdir(catdir);
    rmdir(root);
}
#endif

int main(void)
{
    test_safe_leaf();
#ifndef _WIN32
    test_beneath();
#endif
    if (failures)
    {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("ok\n");
    return 0;
}
