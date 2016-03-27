
bin_PROGRAMS = src/bin/xpost src/bin/xpost_client

src_bin_xpost_client_SOURCES = \
src/bin/xpost_client.c

src_bin_xpost_client_CPPFLAGS = \
-I$(top_srcdir)/src/lib

src_bin_xpost_client_LDADD = \
src/lib/libxpost.la

src_bin_xpost_SOURCES = \
src/bin/xpost_main.c

if HAVE_WIN32
src_bin_xpost_SOURCES += \
src/bin/xpost.rc

windres_verbose = $(windres_verbose_@AM_V@)
windres_verbose_ = $(windres_verbose_@AM_DEFAULT_V@)
windres_verbose_0 = $(AM_V_at)echo "  RC      " $@;

.rc.o:
	$(windres_verbose)$(RC) -o $@ $<
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
