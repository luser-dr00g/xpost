/*
 * Xpost - a Level-2 Postscript interpreter
 * Copyright (C) 2013-2016, Michael Joshua Ryan
 * Copyright (C) 2013-2016, Vincent Torri
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
# include "config.h"
#endif

#include <stdio.h>

#include <check.h>

#include "xpost.h"
#include "xpost_suite.h"

typedef struct
{
    const char *test_case;
    void (*build)(TCase *tc);
} Xpost_Test_Case;

static const Xpost_Test_Case _tests[] = {
    { "Main", xpost_test_main },
    { "Memory", xpost_test_memory },
    { "Stack", xpost_test_stack },
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

    if (!xpost_init())
    {
        fprintf(stderr, "Fail to initialize the xpost library\n");
        return -1;
    }

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

    xpost_quit();

    return (failed_count == 0) ? 0 : 255;
}
