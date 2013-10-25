#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>

#include <check.h>
#include "../lib/xpost_memory.h"

#include "xpost_suite.h"

START_TEST(xpost_init)
{
    Xpost_Memory_File mem;
    ck_assert_int_eq (xpost_memory_file_init(&mem, "", -1), 1);
    ck_assert_int_eq (xpost_memory_file_grow(&mem, 4096), 1);
    ck_assert_int_eq (xpost_memory_file_exit(&mem), 1);
}
END_TEST

void xpost_test_memory(TCase *tc)
{
    tcase_add_test(tc, xpost_init);
}
