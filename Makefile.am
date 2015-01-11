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
XPOST_CLEANFILES =

include src/lib/Makefile.mk
include src/bin/Makefile.mk
include data/Makefile.mk
include doc/Makefile.mk

sed_process = \
$(SED) \
-e 's,@VERSION\@,$(VERSION),g' \
-e 's,@VMAJ\@,$(VMAJ),g' \
-e 's,@prefix\@,$(prefix),g' \
-e 's,@exec_prefix\@,$(exec_prefix),g' \
-e 's,@libdir\@,$(libdir),g' \
-e 's,@includedir\@,$(includedir),g' \
-e 's,@pkgincludedir\@,$(pkgincludedir),g' \
-e 's,@xpost_requirements_lib_pc\@,$(xpost_requirements_lib_pc),g' \
-e 's,@xpost_requirements_lib_libs\@,$(xpost_requirements_lib_libs),g' \
< $< > $@ || rm $@

pc_verbose = $(pc_verbose_@AM_V@)
pc_verbose_ = $(pc_verbose_@AM_DEFAULT_V@)
pc_verbose_0 = @echo "  PC      " $@;

xpost.pc: $(top_srcdir)/xpost.pc.in Makefile
	$(AM_V_at)rm -f $@
	$(pc_verbose)$(sed_process)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = xpost.pc

XPOST_CLEANFILES += xpost.pc

# Unit tests

if XPOST_ENABLE_TESTS

include src/tests/Makefile.mk

if HAVE_VALGRIND
XPOST_VAGRIND = $(top_srcdir)/valgrind_wrapper.sh
endif

check-local:
	$(XPOST_VAGRIND) $(top_srcdir)/src/tests/.libs/xpost_suite$(EXEEXT)

else

check-local:
	@echo "reconfigure with --enable-tests"

endif

EXTRA_DIST += \
COMPLIANCE \
autogen.sh \
xpost.pc.in \
m4/xpost.m4 \
valgrind_wrapper.sh \
glibc.supp \
visual_studio/vc10/xpost.sln \
visual_studio/vc10/libxpost/libxpost.vcxproj \
visual_studio/vc10/libxpost/libxpost.vcxproj.filters \
visual_studio/vc10/xpost/xpost.vcxproj \
visual_studio/vc10/xpost/xpost.vcxproj.filters \
visual_studio/vc10/xpost_client/xpost_client.vcxproj \
visual_studio/vc10/xpost_client/xpost_client.vcxproj.filters \
visual_studio/vc10/xpost_suite/xpost_suite.vcxproj \
visual_studio/vc10/xpost_suite/xpost_suite.vcxproj.filters \
visual_studio/vc12visual_studio/vc12/xpost.sln \
visual_studio/vc12/libxpost/libxpost.vcxproj \
visual_studio/vc12/libxpost/libxpost.vcxproj.filters \
visual_studio/vc12/xpost/xpost.vcxproj \
visual_studio/vc12/xpost/xpost.vcxproj.filters \
visual_studio/vc12/xpost_client/xpost_client.vcxproj \
visual_studio/vc12/xpost_client/xpost_client.vcxproj.filters \
visual_studio/vc12/xpost_suite/xpost_suite.vcxproj \
visual_studio/vc12/xpost_suite/xpost_suite.vcxproj.filters

clean-local:
	rm -rf $(XPOST_CLEANFILES)
