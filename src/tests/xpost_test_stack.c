#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>

#include <check.h>

#include "xpost_log.h"
#include "xpost_memory.h"
#include "xpost_object.h"
#include "xpost_stack.h"

#include "xpost_suite.h"

START_TEST(xpost_stack)
{
    Xpost_Memory_File mem;
    unsigned int stack;
    int ret;

    memset(&mem, 0, sizeof(Xpost_Memory_File));
    ret = xpost_memory_file_init(&mem, NULL, -1);
    ck_assert_int_eq (ret, 1);
    ck_assert(mem.base != NULL);

    ret = xpost_stack_init(&mem, &stack);
    ck_assert_int_eq (ret, 1);

    xpost_stack_free (&mem, stack);
    ret = xpost_memory_file_exit(&mem);
    ck_assert_int_eq (ret, 1);
}
END_TEST

START_TEST(xpost_stack_push_pop)
{
    Xpost_Memory_File mem;
    unsigned int stack;
    int segsize = XPOST_STACK_SEGMENT_SIZE;
    int i;
    Xpost_Object obj;
    int ret;

    xpost_log_init();
    memset(&mem, 0, sizeof(Xpost_Memory_File));
    ret = xpost_memory_file_init(&mem, NULL, -1);
    ck_assert_int_eq (ret, 1);
    ck_assert(mem.base != NULL);

    ret = xpost_stack_init(&mem, &stack);
    ck_assert_int_eq (ret, 1);

    for (i = 0; i < 5 + segsize; i++)
    {
        ret = xpost_stack_push(&mem, stack, xpost_cons_int(i));
        XPOST_LOG_INFO("test push integer %d", i);
        ck_assert_int_eq (ret, 1);
    }

    for (i--; i >= 0; i--)
    {
        obj = xpost_stack_pop(&mem, stack);
        XPOST_LOG_INFO("test pop integer %d", i);
        ck_assert_int_eq (xpost_object_get_type(obj), integertype);
        ck_assert_int_eq (obj.int_.val, i);
    }

    xpost_stack_free (&mem, stack);
    ret = xpost_memory_file_exit(&mem);
    ck_assert_int_eq (ret, 1);
}
END_TEST

void xpost_test_stack(TCase *tc)
{
    tcase_add_test(tc, xpost_stack);
    tcase_add_test(tc, xpost_stack_push_pop);
}
