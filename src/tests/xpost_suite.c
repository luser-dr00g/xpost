#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>

#include <check.h>

#include "xpost_suite.h"

typedef struct
{
    const char *test_case;
    void (*build)(TCase *tc);
} Xpost_Test_Case;

static const Xpost_Test_Case _tests[] = {
    { "Memory", xpost_test_memory },
    { NULL, NULL }
};

static void
_xpost_suite_list(void)
{
    const Xpost_Test_Case *iter = _tests;

    fprintf(stderr, "Available Test Cases:\n");
    for (; iter->test_case; iter++)
        fprintf(stderr, "\t%s\n", iter->test_case);
}

static int
_xpost_suite_test_use(int argc, const char **argv, const char *test_case)
{
    if (argc < 1)
        return 1;

    for (; argc > 0; argc++, argv--)
    {
        if (strcmp(*argv, test_case) == 0)
            return 1;
    }

    return 0;
}

static Suite *
_xpost_suite_build(int argc, const char **argv)
{
    TCase *tc;
    Suite *s;
    int i;

    s = suite_create("Xpost");

    for (i = 0; _tests[i].test_case; ++i)
    {
        if (!_xpost_suite_test_use(argc, argv, _tests[i].test_case))
            continue;

        tc = tcase_create(_tests[i].test_case);
        _tests[i].build(tc);
        suite_add_tcase(s, tc);
        tcase_set_timeout(tc, 0);
    }

    return s;
}

int
main(int argc, char **argv)
{
    Suite *s;
    SRunner *sr;
    int failed_count;
    int i;

    for (i = 1; i < argc; i++)
        if ((strcmp(argv[i], "-h") == 0) ||
            (strcmp(argv[i], "--help") == 0))
        {
            fprintf(stderr, "Usage:\n\t%s [test_case1 .. [test_caseN]]\n",
                    argv[0]);
            _xpost_suite_list();
            return 0;
        }
        else if ((strcmp(argv[i], "-l") == 0) ||
                 (strcmp(argv[i], "--list") == 0))
        {
            _xpost_suite_list();
            return 0;
        }

    s = _xpost_suite_build(argc - 1, (const char **)argv + 1);
    sr = srunner_create(s);

    srunner_run_all(sr, CK_ENV);
    failed_count = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (failed_count == 0) ? 0 : 255;
}
