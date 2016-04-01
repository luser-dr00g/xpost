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
#include <string.h>

#include <check.h>

#include "xpost.h"
#include "xpost_log.h"
#include "xpost_memory.h"

#include "xpost_suite.h"

START_TEST(xpost_memory_init_simple)
{
    Xpost_Memory_File mem;
    int ret;

    xpost_init();

    memset(&mem, 0, sizeof(Xpost_Memory_File));
    ret = xpost_memory_file_init(&mem, NULL, -1, NULL, NULL, NULL);
    ck_assert_int_eq (ret, 1);
    ck_assert(mem.base != NULL);
    ret = xpost_memory_file_exit(&mem);
    ck_assert_int_eq (ret, 1);

    xpost_quit();
}
END_TEST

START_TEST(xpost_memory_init_alloc)
{
    Xpost_Memory_File mem;
    unsigned int addr;
    int ret;

    xpost_init();

    memset(&mem, 0, sizeof(Xpost_Memory_File));
    ret = xpost_memory_file_init(&mem, NULL, -1, NULL, NULL, NULL);
    ck_assert_int_eq (ret, 1);
    ck_assert(mem.base != NULL);
    ret = xpost_memory_file_alloc(&mem, 64, &addr);
    ck_assert_int_eq (ret, 1);
    ck_assert(mem.base != NULL);
    ret = xpost_memory_file_exit(&mem);
    ck_assert_int_eq (ret, 1);

    xpost_quit();
}
END_TEST

START_TEST(xpost_memory_not_init)
{
    Xpost_Memory_File mem = {0};
    unsigned int addr;
    int ret;

    xpost_init();

    mem.base = NULL;
    XPOST_LOG_ERR("you should see an error just below");
    ret = xpost_memory_file_alloc(&mem, 64, &addr);
    ck_assert_int_eq (ret, 0);
    /* ck_assert_int_eq (xpost_memory_file_grow(&mem, 4096), 0); */
    /* ck_assert_int_eq (xpost_memory_file_exit(&mem), 0); */

    xpost_quit();
}
END_TEST

START_TEST(xpost_memory_grow)
{
    char memorypat[] = "preserve this data across grow()";
    Xpost_Memory_File mem = {0};
    unsigned int addr;
    int ret;

    xpost_init();

    ret = xpost_memory_file_init(&mem, NULL, -1, NULL, NULL, NULL);
    ck_assert_int_eq (ret, 1);
    ck_assert(mem.base != NULL);

    ret = xpost_memory_file_alloc(&mem, sizeof memorypat, &addr);
    ck_assert_int_eq (ret, 1);
    strcpy((char *)mem.base + addr, memorypat);

    ret = xpost_memory_file_grow(&mem, 4096);
    ck_assert_int_eq (ret, 1);
    ck_assert(mem.base != NULL);
    ck_assert_str_eq ((char *)mem.base + addr, memorypat);

    ret = xpost_memory_file_exit(&mem);
    ck_assert_int_eq (ret, 1);

    xpost_quit();
}
END_TEST

START_TEST(xpost_memory_tab_init)
{
    Xpost_Memory_File mem = {0};
    unsigned int addr;
    int ret;

    xpost_init();

    ret = xpost_memory_file_init(&mem, NULL, -1, NULL, NULL, NULL);
    ck_assert_int_eq (ret, 1);
    ck_assert(mem.base != NULL);
    ret = xpost_memory_table_init(&mem, &addr);
    ck_assert_int_eq (ret, 1);
    ck_assert_int_eq (addr, 0); /* primary table must be at address 0 */
    ck_assert(mem.base != NULL);
    ret = xpost_memory_file_exit(&mem);
    ck_assert_int_eq (ret, 1);

    xpost_quit();
}
END_TEST

START_TEST(xpost_memory_tab_alloc)
{
    Xpost_Memory_File mem = {0};
    unsigned int addr;
    unsigned int ent;
    char arr[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
    char out[ sizeof arr ];
    unsigned int i;
    int ret;

    xpost_init();

    ret = xpost_memory_file_init(&mem, NULL, -1, NULL, NULL, NULL);
    ck_assert_int_eq (ret, 1);
    ck_assert(mem.base != NULL);
    ret = xpost_memory_table_init(&mem, &addr);
    ck_assert_int_eq (ret, 1);
    ck_assert(mem.base != NULL);
    ret = xpost_memory_table_alloc(&mem, sizeof arr, 0, &ent);
    ck_assert_int_eq (ret, 1);
    ck_assert(mem.base != NULL);
    ret = xpost_memory_put(&mem, ent, 0, sizeof arr, arr);
    ck_assert_int_eq (ret, 1);
    ck_assert(mem.base != NULL);
    ret = xpost_memory_get(&mem, ent, 0, sizeof arr, out);
    ck_assert_int_eq (ret, 1);
    for (i = 0; i < sizeof arr; i++)
        ck_assert_int_eq (arr[i], out[i]);
    ck_assert(mem.base != NULL);
    ret = xpost_memory_file_exit(&mem);
    ck_assert_int_eq (ret, 1);

    xpost_quit();
}
END_TEST

void xpost_test_memory(TCase *tc)
{
    tcase_add_test(tc, xpost_memory_init_simple);
    tcase_add_test(tc, xpost_memory_init_alloc);
    tcase_add_test(tc, xpost_memory_not_init);
    tcase_add_test(tc, xpost_memory_grow);
    tcase_add_test(tc, xpost_memory_tab_init);
    tcase_add_test(tc, xpost_memory_tab_alloc);
}
