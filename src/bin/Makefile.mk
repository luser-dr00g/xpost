
bin_PROGRAMS = src/bin/itp

src_bin_itp_SOURCES = \
src/bin/ar.c \
src/bin/di.c \
src/bin/err.c \
src/bin/f.c \
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
src/bin/ar.h \
src/bin/di.h \
src/bin/err.h \
src/bin/f.h \
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
src/bin/osmswin.h
else
src_bin_itp_SOURCES += \
src/bin/osunix.c \
src/bin/osunix.h
endif

src_bin_itp_CPPFLAGS = -DTESTMODULE_ITP

src_bin_itp_CFLAGS = @XPOST_BIN_CFLAGS@

src_bin_itp_LDADD = -lm
