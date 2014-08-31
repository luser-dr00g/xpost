
lib_LTLIBRARIES = src/lib/libxpost.la

includes_HEADERS = src/lib/xpost.h
includesdir = $(pkgincludedir)-@VMAJ@

src_lib_libxpost_la_SOURCES = \
src/lib/xpost_compat.c \
src/lib/xpost_context.c \
src/lib/xpost_error.c \
src/lib/xpost_font.c \
src/lib/xpost_free.c \
src/lib/xpost_log.c \
src/lib/xpost_main.c \
src/lib/xpost_matrix.c \
src/lib/xpost_memory.c \
src/lib/xpost_object.c \
src/lib/xpost_save.c \
src/lib/xpost_stack.c \
src/lib/xpost_string.c \
src/lib/xpost_array.c \
src/lib/xpost_dict.c \
src/lib/xpost_name.c \
src/lib/xpost_file.c \
src/lib/xpost_garbage.c \
src/lib/xpost_operator.c \
src/lib/xpost_op_array.c \
src/lib/xpost_op_boolean.c \
src/lib/xpost_op_control.c \
src/lib/xpost_op_dict.c \
src/lib/xpost_op_file.c \
src/lib/xpost_op_math.c \
src/lib/xpost_op_packedarray.c \
src/lib/xpost_op_stack.c \
src/lib/xpost_op_string.c \
src/lib/xpost_op_type.c \
src/lib/xpost_op_token.c \
src/lib/xpost_op_save.c \
src/lib/xpost_op_misc.c \
src/lib/xpost_op_param.c \
src/lib/xpost_op_matrix.c \
src/lib/xpost_op_font.c \
src/lib/xpost_op_context.c \
src/lib/xpost_compat.h \
src/lib/xpost_context.h \
src/lib/xpost_error.h \
src/lib/xpost_font.h \
src/lib/xpost_free.h \
src/lib/xpost_log.h \
src/lib/xpost_main.h \
src/lib/xpost_matrix.h \
src/lib/xpost_memory.h \
src/lib/xpost_object.h \
src/lib/xpost_save.h \
src/lib/xpost_stack.h \
src/lib/xpost_string.h \
src/lib/xpost_array.h \
src/lib/xpost_dict.h \
src/lib/xpost_name.h \
src/lib/xpost_file.h \
src/lib/xpost_garbage.h \
src/lib/xpost_operator.h \
src/lib/xpost_op_array.h \
src/lib/xpost_op_boolean.h \
src/lib/xpost_op_control.h \
src/lib/xpost_op_dict.h \
src/lib/xpost_op_file.h \
src/lib/xpost_op_math.h \
src/lib/xpost_op_packedarray.h \
src/lib/xpost_op_stack.h \
src/lib/xpost_op_string.h \
src/lib/xpost_op_type.h \
src/lib/xpost_op_token.h \
src/lib/xpost_op_save.h \
src/lib/xpost_op_misc.h \
src/lib/xpost_op_param.h \
src/lib/xpost_op_matrix.h \
src/lib/xpost_op_font.h \
src/lib/xpost_op_context.h \
src/lib/xpost_private.h

if HAVE_WIN32
src_lib_libxpost_la_SOURCES += \
src/lib/glob.c \
src/lib/glob.h
endif

src_lib_libxpost_la_CPPFLAGS = \
-DPACKAGE_DATA_DIR=\"$(pkgdatadir)\" \
-DXPOST_BUILD

src_lib_libxpost_la_CFLAGS = \
@XPOST_LIB_CFLAGS@

src_lib_libxpost_la_LIBADD = \
@XPOST_LIB_LDFLAGS@ \
@XPOST_LIB_LIBS@

src_lib_libxpost_la_LDFLAGS = -no-undefined -version-info @version_info@
