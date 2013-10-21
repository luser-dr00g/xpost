#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>

#include <check.h>

#include "xpost_suite.h"

START_TEST(xpost_init)
{
}
END_TEST

void xpost_test_memory(TCase *tc)
{
    tcase_add_test(tc, xpost_init);
}
