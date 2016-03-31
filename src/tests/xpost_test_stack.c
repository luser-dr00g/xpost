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
    ret = xpost_memory_file_init(&mem, NULL, -1, NULL, NULL, NULL);
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

    memset(&mem, 0, sizeof(Xpost_Memory_File));
    ret = xpost_memory_file_init(&mem, NULL, -1, NULL, NULL, NULL);
    ck_assert_int_eq (ret, 1);
    ck_assert(mem.base != NULL);

    ret = xpost_stack_init(&mem, &stack);
    ck_assert_int_eq (ret, 1);

    for (i = 0; i < 5 + segsize; i++)
    {
        ret = xpost_stack_push(&mem, stack, xpost_int_cons(i));
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
