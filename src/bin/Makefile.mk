
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
src/bin/osunix.c \
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
ar.h \
di.h \
err.h \
f.h \
gc.h \
itp.h \
m.h \
nm.h \
ob.h \
opar.h \
opb.h \
opc.h \
opdi.h \
opf.h \
op.h \
opm.h \
oppa.h \
ops.h \
opst.h \
opt.h \
optok.h \
opv.h \
opx.h \
osmswin.h \
osunix.h \
s.h \
st.h \
v.h

src_bin_itp_CPPFLAGS = -DTESTMODULE_ITP -I$(top_srcdir)/src/bin

src_bin_itp_LDADD = -lm
