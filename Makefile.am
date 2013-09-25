AUTOMAKE_OPTIONS = subdir-objects

ACLOCAL_AMFLAGS = -I m4

MAINTAINERCLEANFILES = \
Makefile.in \
aclocal.m4 \
compile \
config.guess \
config.h.in \
config.h.in~ \
config.sub \
configure \
depcomp \
install-sh \
missing \
$(DIST_ARCHIVES)

EXTRA_DIST =

include src/bin/Makefile.mk
include data/Makefile.mk
include doc/Makefile.mk

EXTRA_DIST += \
m4/xpost.m4
