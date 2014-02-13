
bin_PROGRAMS = src/bin/xpost

src_bin_xpost_SOURCES = \
src/bin/xpost_array.c \
src/bin/xpost_dict.c \
src/bin/xpost_file.c \
src/bin/xpost_garbage.c \
src/bin/xpost_context.c \
src/bin/xpost_interpreter.c \
src/bin/xpost_main.c \
src/bin/xpost_name.c \
src/bin/xpost_save.c \
src/bin/xpost_string.c \
src/bin/xpost_operator.c \
src/bin/xpost_op_array.c \
src/bin/xpost_op_boolean.c \
src/bin/xpost_op_control.c \
src/bin/xpost_op_dict.c \
src/bin/xpost_op_file.c \
src/bin/xpost_op_math.c \
src/bin/xpost_op_packedarray.c \
src/bin/xpost_op_stack.c \
src/bin/xpost_op_string.c \
src/bin/xpost_op_type.c \
src/bin/xpost_op_token.c \
src/bin/xpost_op_save.c \
src/bin/xpost_op_misc.c \
src/bin/xpost_op_param.c \
src/bin/xpost_op_matrix.c \
src/bin/xpost_op_font.c \
src/bin/xpost_dev_generic.c \
src/bin/xpost_dev_bgr.c \
src/bin/xpost_pathname.c \
src/bin/xpost_array.h \
src/bin/xpost_dict.h \
src/bin/xpost_file.h \
src/bin/xpost_garbage.h \
src/bin/xpost_context.h \
src/bin/xpost_interpreter.h \
src/bin/xpost_name.h \
src/bin/xpost_op_array.h \
src/bin/xpost_op_boolean.h \
src/bin/xpost_op_control.h \
src/bin/xpost_op_dict.h \
src/bin/xpost_op_file.h \
src/bin/xpost_operator.h \
src/bin/xpost_op_math.h \
src/bin/xpost_op_packedarray.h \
src/bin/xpost_op_stack.h \
src/bin/xpost_op_string.h \
src/bin/xpost_op_type.h \
src/bin/xpost_op_token.h \
src/bin/xpost_op_save.h \
src/bin/xpost_op_misc.h \
src/bin/xpost_op_param.h \
src/bin/xpost_op_matrix.h \
src/bin/xpost_op_font.h \
src/bin/xpost_dev_generic.h \
src/bin/xpost_dev_bgr.h \
src/bin/xpost_pathname.h \
src/bin/xpost_string.h \
src/bin/xpost_save.h

if HAVE_WIN32
src_bin_xpost_SOURCES += \
src/bin/glob.c \
src/bin/glob.h \
src/bin/xpost_dev_win32.c \
src/bin/xpost_dev_win32.h \
src/bin/xpost.rc

windres_verbose = $(windres_verbose_@AM_V@)
windres_verbose_ = $(windres_verbose_@AM_DEFAULT_V@)
windres_verbose_0 = $(AM_V_at)echo "  RC      " $@;

.rc.o:
	$(wc_verbose)windres -o $@ $<
endif

if HAVE_XCB
src_bin_xpost_SOURCES += \
src/bin/xpost_dev_xcb.c \
src/bin/xpost_dev_xcb.h
endif

src_bin_xpost_CPPFLAGS = \
-I$(top_srcdir)/src/lib \
-DPACKAGE_DATA_DIR=\"$(pkgdatadir)\" \
-DPACKAGE_INSTALL_DIR=\"$(prefix)/\" \
@XPOST_BIN_CFLAGS@

src_bin_xpost_LDADD = \
src/lib/libxpost.la \
@XPOST_BIN_LIBS@ \
@XPOST_BIN_LDFLAGS@ \
-lm

if HAVE_SPLINT
splint_process = splint \
+posixlib -boolops -predboolint +ignoresigns -type +charindex \
-nestcomment -noeffect -redef -shiftnegative -castfcnptr \
-shiftimplementation -predboolothers -exportlocal -mustfreefresh \
-preproc \
-Isrc/lib \
$(top_srcdir)/src/{bin,lib}/*.[ch]
else
splint_process = echo "splint not found. Update PATH or install splint."
endif

splint_verbose = $(splint_verbose_@AM_V@)
splint_verbose_ = $(splint_verbose_@AM_DEFAULT_V@)
splint_verbose_0 = $(AM_V_at)echo "  SPLINT  " $@;

splint: $(top_srcdir)/src/bin/*.c $(top_srcdir)/src/bin/*.h
	$(splint_verbose)$(splint_process)

if HAVE_WC
wc_process = @WC@ -l $(top_srcdir)/src/bin/*.c $(top_srcdir)/src/bin/*.h $(top_srcdir)/src/lib/*.c $(top_srcdir)/src/lib/*.h $(top_srcdir)/data/*.ps
else
wc_process = echo "wc not found. Update PATH or install wc."
endif

wc_verbose = $(wc_verbose_@AM_V@)
wc_verbose_ = $(wc_verbose_@AM_DEFAULT_V@)
wc_verbose_0 = $(AM_V_at)echo "  WC      " $@;

count: $(top_srcdir)/src/bin/*.c $(top_srcdir)/src/bin/*.h $(top_srcdir)/src/lib/*.c $(top_srcdir)/src/lib/*.h $(top_srcdir)/data/*.ps
	$(wc_verbose)$(wc_process)

EXTRA_DIST += src/bin/xpostlogo.ico
