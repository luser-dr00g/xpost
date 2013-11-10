
lib_LTLIBRARIES = src/lib/libxpost.la

includes_HEADERS = src/lib/xpost.h
includesdir = $(pkgincludedir)-@VMAJ@

src_lib_libxpost_la_SOURCES = \
src/lib/xpost_log.c \
src/lib/xpost_main.c \
src/lib/xpost_memory.c \
src/lib/xpost_free.c \
src/lib/xpost_object.c \
src/lib/xpost_stack.c \
src/lib/xpost_log.h \
src/lib/xpost_main.h \
src/lib/xpost_memory.h \
src/lib/xpost_free.h \
src/lib/xpost_object.h \
src/lib/xpost_stack.h \
src/lib/xpost_private.h

src_lib_libxpost_la_CPPFLAGS = \
-DPACKAGE_DATA_DIR=\"$(pkgdatadir)\" \
-DXPOST_BUILD

src_lib_libxpost_la_CFLAGS = \
@XPOST_LIB_CFLAGS@

src_lib_libxpost_la_LIBADD =

src_lib_libxpost_la_LDFLAGS = -no-undefined -version-info @version_info@
