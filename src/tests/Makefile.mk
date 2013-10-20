
check_PROGRAMS = src/tests/xpost_suite

src_tests_xpost_suite_SOURCES = \
src/tests/xpost_suite.c \
src/tests/xpost_suite.h

src_tests_xpost_suite_CPPFLAGS = \
@CHECK_CFLAGS@

src_tests_xpost_suite_CFLAGS = \
@XPOST_LIB_CFLAGS@

src_tests_xpost_suite_LDADD = \
$(top_builddir)/src/lib/libxpost.la \
@CHECK_LIBS@
