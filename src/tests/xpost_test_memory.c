#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <string.h>

#include <check.h>

#include "xpost_log.h"
#include "xpost_memory.h"

#include "xpost_suite.h"

START_TEST(xpost_init)
{
    Xpost_Memory_File mem;
    int ret;

    memset(&mem, 0, sizeof(Xpost_Memory_File));
    ret = xpost_memory_file_init(&mem, NULL, -1, NULL, NULL, NULL);
    ck_assert_int_eq (ret, 1);
    ck_assert(mem.base != NULL);
    ret = xpost_memory_file_exit(&mem);
    ck_assert_int_eq (ret, 1);
}
END_TEST

START_TEST(xpost_memory_init_)
{
    Xpost_Memory_File mem;
    unsigned int addr;
    int ret;

    memset(&mem, 0, sizeof(Xpost_Memory_File));
    ret = xpost_memory_file_init(&mem, NULL, -1, NULL, NULL, NULL);
    ck_assert_int_eq (ret, 1);
    ck_assert(mem.base != NULL);
    ret = xpost_memory_file_alloc(&mem, 64, &addr);
    ck_assert_int_eq (ret, 1);
    ck_assert(mem.base != NULL);
    ret = xpost_memory_file_exit(&mem);
    ck_assert_int_eq (ret, 1);
}
END_TEST

START_TEST(xpost_memory_not_init)
{
    Xpost_Memory_File mem = {0};
    unsigned int addr;
    int ret;

    mem.base = NULL;
    XPOST_LOG_ERR("you should see an error just below");
    ret = xpost_memory_file_alloc(&mem, 64, &addr);
    ck_assert_int_eq (ret, 0);
    /* ck_assert_int_eq (xpost_memory_file_grow(&mem, 4096), 0); */
    /* ck_assert_int_eq (xpost_memory_file_exit(&mem), 0); */
}
END_TEST

START_TEST(xpost_memory_grow)
{
    char memorypat[] = "preserve this data across grow()";
    Xpost_Memory_File mem = {0};
    unsigned int addr;
    int ret;

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
}
END_TEST

START_TEST(xpost_memory_tab_init)
{
    Xpost_Memory_File mem = {0};
    unsigned int addr;
    int ret;

    ret = xpost_memory_file_init(&mem, NULL, -1, NULL, NULL, NULL);
    ck_assert_int_eq (ret, 1);
    ck_assert(mem.base != NULL);
    ret = xpost_memory_table_init(&mem, &addr);
    ck_assert_int_eq (ret, 1);
    ck_assert_int_eq (addr, 0); /* primary table must be at address 0 */
    ck_assert(mem.base != NULL);
    ret = xpost_memory_file_exit(&mem);
    ck_assert_int_eq (ret, 1);
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
}
END_TEST

void xpost_test_memory(TCase *tc)
{
    tcase_add_test(tc, xpost_init);
    tcase_add_test(tc, xpost_memory_init_);
    tcase_add_test(tc, xpost_memory_not_init);
    tcase_add_test(tc, xpost_memory_grow);
    tcase_add_test(tc, xpost_memory_tab_init);
    tcase_add_test(tc, xpost_memory_tab_alloc);
}
