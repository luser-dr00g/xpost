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
ltmain.sh \
m4/libtool.m4 \
m4/ltoptions.m4 \
m4/ltsugar.m4 \
m4/ltversion.m4 \
m4/lt~obsolete.m4 \
missing \
$(DIST_ARCHIVES)

EXTRA_DIST =

include src/lib/Makefile.mk
include src/bin/Makefile.mk
include data/Makefile.mk
include doc/Makefile.mk

# Unit tests

if XPOST_ENABLE_TESTS

include src/tests/Makefile.mk

check-local:
	src/tests/.libs/xpost_suite.exe

else

check-local:
	@echo "reconfigure with --enable-tests"

endif

EXTRA_DIST += \
m4/xpost.m4
