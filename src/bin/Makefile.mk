
bin_PROGRAMS = src/bin/itp

src_bin_itp_SOURCES = \
src/bin/xpost_array.c \
src/bin/xpost_dict.c \
src/bin/err.c \
src/bin/xpost_file.c \
src/bin/gc.c \
src/bin/itp.c \
src/bin/m.c \
src/bin/nm.c \
src/bin/s.c \
src/bin/v.c \
src/bin/st.c \
src/bin/ob.c \
src/bin/op.c \
src/bin/opar.c \
src/bin/opb.c \
src/bin/opc.c \
src/bin/opdi.c \
src/bin/opf.c \
src/bin/opm.c \
src/bin/oppa.c \
src/bin/ops.c \
src/bin/opst.c \
src/bin/opt.c \
src/bin/optok.c \
src/bin/opv.c \
src/bin/opx.c \
src/bin/xpost_array.h \
src/bin/xpost_dict.h \
src/bin/err.h \
src/bin/xpost_file.h \
src/bin/gc.h \
src/bin/itp.h \
src/bin/m.h \
src/bin/nm.h \
src/bin/ob.h \
src/bin/opar.h \
src/bin/opb.h \
src/bin/opc.h \
src/bin/opdi.h \
src/bin/opf.h \
src/bin/op.h \
src/bin/opm.h \
src/bin/oppa.h \
src/bin/ops.h \
src/bin/opst.h \
src/bin/opt.h \
src/bin/optok.h \
src/bin/opv.h \
src/bin/opx.h \
src/bin/s.h \
src/bin/st.h \
src/bin/v.h

if HAVE_WIN32
src_bin_itp_SOURCES += \
src/bin/osmswin.c \
src/bin/osmswin.h \
src/bin/glob.c \
src/bin/glob.h
else
src_bin_itp_SOURCES += \
src/bin/osunix.c \
src/bin/osunix.h
endif

src_bin_itp_CPPFLAGS = \
-DPACKAGE_DATA_DIR=\"$(pkgdatadir)\" \
-DTESTMODULE_ITP

src_bin_itp_CFLAGS = @XPOST_BIN_CFLAGS@

src_bin_itp_LDADD = -lm

if HAVE_SPLINT
splint_process = splint \
+posixlib -boolops -predboolint +ignoresigns -type +charindex \
-nestcomment -noeffect -redef -shiftnegative -castfcnptr \
-shiftimplementation -predboolothers -exportlocal -mustfreefresh \
$(top_srcdir)/src/bin/*.c
else
splint_process = echo "splint not found. Update PATH or install splint."
endif

splint_verbose = $(splint_verbose_@AM_V@)
splint_verbose_ = $(splint_verbose_@AM_DEFAULT_V@)
splint_verbose_0 = $(AM_V_at)echo "  SPLINT  " $@;

splint: $(top_srcdir)/src/bin/*.c $(top_srcdir)/src/bin/*.h
	$(splint_verbose)$(splint_process)

if HAVE_WC
wc_process = @WC@ -l $(top_srcdir)/src/bin/*.c $(top_srcdir)/src/bin/*.h
else
wc_process = echo "wc not found. Update PATH or install wc."
endif

wc_verbose = $(wc_verbose_@AM_V@)
wc_verbose_ = $(wc_verbose_@AM_DEFAULT_V@)
wc_verbose_0 = $(AM_V_at)echo "  WC      " $@;

count: $(top_srcdir)/src/bin/*.c $(top_srcdir)/src/bin/*.h
	$(wc_verbose)$(wc_process)
