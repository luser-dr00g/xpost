
lib_LTLIBRARIES = src/lib/libxpost.la

includes_HEADERS = src/lib/xpost.h
includesdir = $(pkgincludedir)-@VMAJ@

src_lib_libxpost_la_SOURCES = \
src/lib/xpost_main.c \
src/lib/xpost_private.h

src_lib_libxpost_la_CPPFLAGS = \
-DPACKAGE_DATA_DIR=\"$(pkgdatadir)\" \
-DXPOST_BUILD

src_lib_libxpost_la_CFLAGS = \
@XPOST_LIB_CFLAGS@

src_lib_libxpost_la_LIBADD =

src_lib_libxpost_la_LDFLAGS = -no-undefined -version-info @version_info@ @release_info@
