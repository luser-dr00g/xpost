
m4_define([v_maj], [0])
m4_define([v_min], [0])
m4_define([v_mic], [1])
m4_define([v_ver], [v_maj.v_min.v_mic])

m4_define([lt_cur], [m4_eval(v_maj + v_min)])
m4_define([lt_rev], [v_mic])
m4_define([lt_age], [v_min])

AC_INIT([xpost], [v_ver], [luser.droog.remove@gmail.com])
AC_PREREQ([2.60])
AC_CONFIG_SRCDIR([configure.ac])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

# AC_PROG_C** defines CFLAGS to -g -O2 if CFLAGS is empty. Let's use our CFLAGS

AC_MSG_CHECKING([whether CFLAGS is empty])
if test  "x$CFLAGS" = "x" ; then
   no_cflags="yes"
else
   no_cflags="no"
fi
AC_MSG_RESULT([${no_cflags}])

CFLAGS_SHELL=${CFLAGS}
CFLAGS=""

AC_USE_SYSTEM_EXTENSIONS

AM_INIT_AUTOMAKE([1.10 dist-bzip2])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

lt_cv_deplibs_check_method=pass_all
LT_INIT([win32-dll])

version_info="lt_cur:lt_rev:lt_age"
AC_SUBST([version_info])

VMAJ=v_maj
AC_SUBST([VMAJ])

AC_DEFINE([XPOST_VERSION_MAJ], [v_maj], [Define to the major version of the library])
AC_DEFINE([XPOST_VERSION_MIN], [v_min], [Define to the minor version of the library])
AC_DEFINE([XPOST_VERSION_MIC], [v_mic], [Define to the micro version of the library])

### Additional options to configure

AC_ARG_ENABLE([release],
   [AS_HELP_STRING([--enable-release], [set release mode @<:@default=no@:>@])],
   [
    if test "x${enableval}" = "xyes" ; then
       enable_release="yes"
    else
       enable_release="no"
    fi
   ],
   [enable_release="no"])

if test "x${enable_release}" = "xyes" ; then
   AC_DEFINE([NDEBUG], [1], [Define to 1 if release mode is wanted])
else
   AC_DEFINE([_DEBUG], [1], [Define to 1 if debug mode is wanted])
fi

AC_ARG_ENABLE([large-object],
   [AS_HELP_STRING([--enable-large-object], [enable use of large object @<:@default=no@:>@])],
   [
    if test "x${enableval}" = "xyes" ; then
       enable_large_object="yes"
    else
       enable_large_object="no"
    fi
   ],
   [enable_large_object="no"])

if test "x${enable_large_object}" = "xyes" ; then
   AC_DEFINE([WANT_LARGE_OBJECT], [1], [Define to 1 if large object are wanted])
fi

AC_ARG_ENABLE([mmap],
   [AS_HELP_STRING([--disable-mmap], [disable use of mmap and use malloc instead @<:@default=yes@:>@])],
   [
    if test "x${enableval}" = "xyes" ; then
       enable_mmap="yes"
    else
       enable_mmap="no"
    fi
   ],
   [enable_mmap="yes"])

have_mmap="no"

AC_ARG_WITH([tests],
   [AS_HELP_STRING([--with-tests=none|regular|coverage], [choose testing method: regular, coverage or none. @<:@default=none@:>@])],
   [build_tests=${withval}],
   [build_tests="none"])

want_coverage="no"
want_tests="no"
case "${build_tests}" in
   regular)
     want_tests="yes"
     ;;
   coverage)
     want_tests="yes"
     want_coverage="yes"
     ;;
   no*)
     ;;
   *)
     AC_MSG_ERROR([Unknown build tests option: --with-tests=${build_tests}])
     ;;
esac

AC_ARG_ENABLE([valgrind],
   [AS_HELP_STRING([--enable-valgrind], [enable running unit tests with valgrind @<:@default=no@:>@])],
   [
    if test "x${enableval}" = "xyes" ; then
       enable_valgrind="yes"
    else
       enable_valgrind="no"
    fi
   ],
   [enable_valgrind="no"])


### Default options with respect to host

AC_CANONICAL_HOST

have_win32="no"
have_cygwin="no"
case "$host_os" in
   mingw*)
      have_win32="yes"
   ;;
   cygwin*)
      have_cygwin="yes"
   ;;
esac

AM_CONDITIONAL([HAVE_WIN32], [test "x${have_win32}" = "xyes"])


### Checks for programs

# C compiler
AC_PROG_CC
CFLAGS=${CFLAGS_SHELL}

if test "x${ac_cv_prog_cc_c99}" = "xno" ; then
   AC_MSG_ERROR([Can not find a C99 compliant compiler. Exiting...])
fi

# windres
AC_CHECK_TOOL([RC], [windres], [no])

# pkg-config
PKG_PROG_PKG_CONFIG

# documentation
XPOST_CHECK_DOXYGEN([build_doc="yes"], [build_doc="no"])

# lcov
have_coverage="no"
if test "${want_coverage}" = "yes" ; then
   AC_CHECK_PROG([have_lcov], [lcov], [yes], [no])
   if test "x${have_lcov}" = "xyes" ; then
      XPOST_COV_CFLAGS="--coverage"
      XPOST_COV_LIBS="--coverage"
      have_coverage="yes"
   else
      AC_MSG_ERROR([coverage report requested, but lcov is not found])
   fi
fi

AC_SUBST([XPOST_COV_CFLAGS])
AC_SUBST([XPOST_COV_LIBS])
AM_CONDITIONAL([XPOST_ENABLE_COVERAGE], [test "${have_coverage}" = "yes"])

# splint
AC_CHECK_PROG([SPLINT], [splint], [splint], [none])

AM_CONDITIONAL([HAVE_SPLINT], [test "x${SPLINT}" != "xnone"])

AC_SUBST([SPLINT])

# wc
AC_CHECK_PROG([WC], [wc], [wc], [none])

AM_CONDITIONAL([HAVE_WC], [test "x${WC}" != "xnone"])

AC_SUBST([WC])


### Checks for libraries

AC_CHECK_LIB([Kenny], [life_signs],
   [],
   [echo "  Oh my god, they killed Kenny! You bastards!"])

xpost_requirements_bin_pc=""
xpost_requirements_lib_pc=""
xpost_requirements_lib_libs="-lm"
AC_SUBST([xpost_requirements_lib_pc])
AC_SUBST([xpost_requirements_lib_libs])

# fontconfig
PKG_CHECK_EXISTS([fontconfig],
   [
    have_fontconfig="yes"
    xpost_requirements_lib_pc="${xpost_requirements_lib_pc} fontconfig"
    AC_DEFINE([HAVE_FONTCONFIG], [1], [Define to 1 if Fontconfig is detected])
   ],
   [have_fontconfig="no"])

# freetype
PKG_CHECK_EXISTS([freetype2],
   [
    have_freetype="yes"
    xpost_requirements_lib_pc="${xpost_requirements_lib_pc} freetype2"
    AC_DEFINE([HAVE_FREETYPE2], [1], [Define to 1 if Freetype is detected])
   ],
   [have_freetype="no"])

# libjpeg
AC_CHECK_LIB([jpeg], [jpeg_CreateDecompress],
   [have_libjpeg="yes"], [have_libjpeg="no"])

if test "x${have_libjpeg}" = "xyes" ; then
   AC_CHECK_HEADER([jpeglib.h],
      [
       have_libjpeg="yes"
       AC_DEFINE([HAVE_LIBJPEG], [1], [Define to 1 if libjpeg is detected])
       xpost_requirements_lib_libs="-ljpeg ${xpost_requirements_lib_libs}"
      ],
      [have_libjpeg="no"])
fi

# libpng
PKG_CHECK_EXISTS([libpng],
   [
    have_libpng="yes"
    xpost_requirements_lib_pc="${xpost_requirements_lib_pc} libpng"
    AC_DEFINE([HAVE_LIBPNG], [1], [Define to 1 if libpng is detected])
   ],
   [have_libpng="no"])

# libxcb
PKG_CHECK_EXISTS([xcb-image xcb-icccm xcb],
   [
    have_xcb="yes"
    xpost_requirements_lib_pc="${xpost_requirements_lib_pc} xcb-image xcb-icccm xcb"
    AC_DEFINE([HAVE_XCB], [1], [Define to 1 if XCB is detected])
   ],
   [have_xcb="no"])

AM_CONDITIONAL([HAVE_XCB], [test "x${have_xcb}" = "xyes"])

if ! test "x${xpost_requirements_lib_pc}" = "x" ; then
   PKG_CHECK_MODULES([XPOST_LIB], [${xpost_requirements_lib_pc}])
fi

if ! test "x${xpost_requirements_bin_pc}" = "x" ; then
   PKG_CHECK_MODULES([XPOST_BIN], [${xpost_requirements_bin_pc}])
fi

# winsock2
if test "x${have_win32}" = "xyes" ; then
   xpost_requirements_lib_libs="-lws2_32 ${xpost_requirements_lib_libs}"
fi

# GDI, OpenGL
if test "x${have_win32}" = "xyes" ; then
   xpost_requirements_lib_libs="-lopengl32 -lgdi32 ${xpost_requirements_lib_libs}"
fi

# check
have_tests="no"
if test "x${want_tests}" = "xyes" ; then
   PKG_CHECK_MODULES([CHECK],
      [check >= 0.9.5],
      [
       have_tests="yes"
       AC_DEFINE([HAVE_CHECK], [1], [Define to 1 if check unit testing library is available])
      ],
      [have_tests="no"])
fi

AC_MSG_CHECKING([whether tests are built])
AC_MSG_RESULT([${have_tests}])

AM_CONDITIONAL(XPOST_ENABLE_TESTS, test "x${have_tests}" = "xyes")

# dladdr
have_dl="no"
if test "x${have_win32}" = "xno" && test "x${have_cygwin}" = "xno" ; then
   AC_MSG_CHECKING([for dladdr() in -ldl])
   LIBS_save="${LIBS}"
   LIBS="${LIBS} -ldl"
   AC_LINK_IFELSE(
      [AC_LANG_PROGRAM(
          [[
#include <stdlib.h>
#include <dlfcn.h>
          ]],
          [[
dladdr(NULL, NULL);
          ]])],
      [
       have_dl="yes"
       xpost_requirements_lib_libs="${xpost_requirements_lib_libs} -ldl"
      ],
      [have_dl="no"])
   LIBS="${LIBS_save}"

   AC_MSG_RESULT([${have_dl}])
   if test "x${have_dl}" = "xno" ; then
      AC_MSG_ERROR([dladdr() function is mandatory, exiting...])
   fi
fi

# valgrind
if test "x${have_tests}" = "xno" ; then
   enable_valgrind="no"
fi

have_valgrind="no"
if test "x${have_tests}" = "xyes" && test "x${enable_valgrind}" = "xyes" ; then
   AC_CHECK_PROG([VALGRIND], [valgrind], [valgrind], [none])
   if test "x${VALGRIND}" != "xnone" ; then
      have_valgrind="yes"
   fi
fi

AM_CONDITIONAL([HAVE_VALGRIND], [test "x${have_valgrind}" = "xyes"])

AC_SUBST([VALGRIND])

# we finalize the setup
AM_CONDITIONAL([HAVE_GRAPHICS], [test "x${have_win32}" = "xyes" || test "x${have_xcb}" = "xyes"])

XPOST_LIB_LIBS="${xpost_requirements_lib_libs} ${XPOST_LIB_LIBS}"

AC_CHECK_LIB([Fridge], [mass_quantities_of_bass_ale])
AC_CHECK_LIB([Fridge], [mass_quantities_of_any_ale],
   [],
   [
    echo "Warning: No ales were found in your refrigerator."
    echo "  We highly suggest that you rectify this situation immediately."
   ])



### Checks for header files

AC_HEADER_ASSERT
AC_CHECK_HEADERS([time.h sys/time.h])
AC_SYS_POSIX_TERMIOS

if test "x${ac_cv_sys_posix_termios}" = "xyes" ; then
   AC_DEFINE([HAVE_TERMIOS_H], [1], [Define to 1 if termios headers and functions are available])
fi

if test "x${enable_mmap}" = "xyes" ; then
   AC_CHECK_HEADERS([sys/mman.h], [have_mmap="yes"], [have_mmap="no"])
fi

AC_CHECK_HEADERS([unistd.h signal.h sys/select.h])

### Checks for types


### Checks for structures


### Checks for compiler characteristics

AM_PROG_CC_C_O

xpost_lib_cflags="-std=c99 -pedantic-errors -Wall -Wextra -Wshadow -Wdeclaration-after-statement -Wmissing-prototypes -Wstrict-prototypes -Wpointer-arith -Wno-missing-field-initializers -Winline -Wwrite-strings"

xpost_lib_ldflags="-fdata-sections -ffunction-sections"

if test "x${enable_release}" = "xno" ; then
   xpost_lib_cflags="${xpost_lib_cflags} -g3 -ggdb3 -O2"
fi

AC_SUBST([XPOST_TEST_CPPFLAGS])

if test "x${no_cflags}" = "xyes" ; then
   XPOST_CHECK_C_COMPILER_FLAGS([XPOST_LIB], [${xpost_lib_cflags}])
fi
XPOST_CHECK_LINKER_FLAGS([XPOST_LIB], [${xpost_lib_ldflags}])

if test "x${no_cflags}" = "xyes" ; then
   XPOST_CHECK_C_COMPILER_FLAGS([XPOST_BIN], [-std=c99 -pedantic-errors -Wall -Wextra -Wshadow -Wdeclaration-after-statement -Wmissing-prototypes -Wstrict-prototypes -Wpointer-arith -Wno-missing-field-initializers -Winline])
fi

XPOST_CHECK_LINKER_FLAGS([XPOST_BIN], [-fdata-sections -ffunction-sections])


### Checks for linker characteristics


### Checks for library functions

AC_FUNC_ALLOCA

AC_CHECK_FUNCS([buckets_of_erogenous_nym])

# sysconf
AC_COMPILE_IFELSE(
   [AC_LANG_PROGRAM(
       [[
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
       ]],
       [[
long sz;
sz = sysconf(_SC_PAGESIZE);
       ]])],
   [
    have_sysconf_pagesize="yes"
    AC_DEFINE([HAVE_SYSCONF_PAGESIZE], [1], [Define to 1 if _SC_PAGESIZE is available with sysconf()])
   ],
   [have_sysconf_pagesize="no"])

AC_MSG_CHECKING([whether _SC_PAGESIZE is available with sysconf()])
AC_MSG_RESULT([${have_sysconf_pagesize}])

if test "x${have_sysconf_pagesize}" = "xno" ; then
   AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM(
          [[
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
          ]],
          [[
long sz;
sz = sysconf(_SC_PAGE_SIZE);
          ]])],
      [
       have_sysconf_page_size="yes"
       AC_DEFINE([HAVE_SYSCONF_PAGE_SIZE], [1], [Define to 1 if _SC_PAGE_SIZE is available with sysconf()])
      ],
      [have_sysconf_page_size="no"])

   AC_MSG_CHECKING([whether _SC_PAGE_SIZE is available with sysconf()])
   AC_MSG_RESULT([${have_sysconf_page_size}])
fi

# getpagesize
if test "x${have_sysconf_pagesize}" = "xno" && test "x${have_sysconf_page_size}" = "xno" ; then
   AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM(
          [[
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
          ]],
          [[
long sz;
sz = getpagesize();
          ]])],
      [
       have_getpagesize="yes"
       AC_DEFINE([HAVE_GETPAGESIZE], [1], [Define to 1 if getpagesize() is available])
      ],
      [have_getpagesize="no"])

   AC_MSG_CHECKING([whether getpagesize() is available])
   AC_MSG_RESULT([${have_getpagesize}])
fi

AC_CHECK_FUNCS([buttox])

# mremap
have_mremap="no"
if test "x${have_mmap}" = "xyes" ; then
   AC_CHECK_FUNCS([mmap mremap])
   if ! test "x${ac_cv_func_mmap}" = "xyes" ; then
      have_mmap="no"
   fi
   if test "x${ac_cv_func_mremap}" = "xyes" ; then
      have_mremap="yes"
   fi
fi

### Output

AC_CONFIG_FILES([
Makefile
])

AC_OUTPUT

echo
echo "$PACKAGE $VERSION"
echo
echo "Configuration Options Summary:"
echo
echo "  OS...................: ${host_os}"
echo
echo "  Release mode.........: ${enable_release}"
if test "x${have_mmap}" = "xyes" ; then
echo "  mmap support.........: ${have_mmap} (mremap: ${have_mremap})"
else
echo "  mmap support.........: no"
fi
echo "  Freetype support.....: ${have_freetype}"
echo "  Fontconfig support...: ${have_fontconfig}"
echo "  Devices:"
echo "    PGM image..........: always"
echo "    JPEG image.........: ${have_libjpeg}"
echo "    PNG image..........: ${have_libpng}"
echo "    XCB................: ${have_xcb}"
echo "    Win32..............: ${have_win32}"
echo "  Documentation........: ${enable_doc} (make doc)"
if test "x${SPLINT}" = "xnone" ; then
echo "  splint...............: no"
else
echo "  splint...............: yes (make splint)"
fi
if test "x${WC}" = "xnone" ; then
echo "  wc...................: no"
else
echo "  wc...................: yes (make count)"
fi
if test "x${have_tests}" = "xyes" ; then
echo "  Tests................: yes (make check)"
else
echo "  Tests................: no"
fi
if test "x${have_coverage}" = "xyes" ; then
echo "  Coverage.............: yes (make lcov-check)"
else
echo "  Coverage.............: no"
fi
echo "  Valgrind.............: ${have_valgrind}"
echo
echo "Binaries...............:"
echo "  xpost................: always"
echo "  xpost_client.........: always"
echo "  xpost_dsc............: always"
if test "x${have_win32}" = "xyes" || test "x${have_xcb}" = "xyes" ; then
echo "  xpost_view...........: yes"
else
echo "  xpost_view...........: no"

fi
echo
echo "Compilation............: make (or gmake)"
echo "  Environment flags....:"
echo "    CPPFLAGS...........: $CPPFLAGS"
echo "    CFLAGS.............: $CFLAGS"
echo "    LDFLAGS............: ${LDFLAGS}"
echo "  Xpost flags..........:"
echo "    XPOST_LIB_CFLAGS...: ${XPOST_LIB_CFLAGS}"
echo "    XPOST_LIB_LIBS.....: $XPOST_LIB_LIBS"
echo "    XPOST_LIB_LDFLAGS..: $XPOST_LIB_LDFLAGS"
echo "    XPOST_BIN_CFLAGS...: $XPOST_BIN_CFLAGS"
echo "    XPOST_BIN_LIBS.....: $XPOST_BIN_LIBS"
echo "    XPOST_BIN_LDFLAGS..: $XPOST_BIN_LDFLAGS"
echo
echo "Installation...........: make install (as root if needed, with 'su' or 'sudo')"
echo "  prefix...............: $prefix"
