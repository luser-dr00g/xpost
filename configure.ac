
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
AC_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_USE_SYSTEM_EXTENSIONS

AM_INIT_AUTOMAKE([1.10 dist-bzip2])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

LT_INIT([win32-dll])

version_info="lt_cur:lt_rev:lt_age"
AC_SUBST([version_info])

VMAJ=v_maj
AC_SUBST([VMAJ])

AC_DEFINE([XPOST_VERSION_MAJ], [v_maj], [Define to the major version of the library])
AC_DEFINE([XPOST_VERSION_MIN], [v_min], [Define to the minor version of the library])
AC_DEFINE([XPOST_VERSION_MIC], [v_mic], [Define to the micro version of the library])

### Additional options to configure

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

AC_ARG_ENABLE([tests],
   [AC_HELP_STRING([--enable-tests], [enable tests @<:@default=no@:>@])],
   [
    if test "x${enableval}" = "xyes" ; then
       enable_tests="yes"
    else
       enable_tests="no"
    fi
   ],
   [enable_tests="no"])

AC_ARG_ENABLE([valgrind],
   [AC_HELP_STRING([--enable-valgrind], [enable running unit tests with valgrind @<:@default=no@:>@])],
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
case "$host_os" in
   mingw*)
      have_win32="yes"
   ;;
esac

if test "x${have_win32}" = "xyes" ; then
   AC_DEFINE([HAVE_WIN32], [1], [Define to 1 on Windows OS])
fi

AM_CONDITIONAL([HAVE_WIN32], [test "x${have_win32}" = "xyes"])


### Checks for programs

# C compiler
AC_PROG_CC_C99

if test "x${ac_cv_prog_cc_c99}" = "xno" ; then
   AC_MSG_ERROR([Can not find a C99 compliant compiler. Exiting...])
fi

# documentation
XPOST_CHECK_DOXYGEN([build_doc="yes"], [build_doc="no"])

# splint
AC_CHECK_PROG([SPLINT], [splint], [splint], [none])

AM_CONDITIONAL([HAVE_SPLINT], [test "x${SPLINT}" != "xnone"])

AC_SUBST([SPLINT])

# wc
AC_CHECK_PROG([WC], [wc], [wc], [none])

AM_CONDITIONAL([HAVE_WC], [test "x${WC}" != "xnone"])

AC_SUBST([WC])


### Checks for libraries

xpost_requirements_bin_pc=""
xpost_requirements_lib_pc=""
AC_SUBST([xpost_requirements_lib_pc])

# fontconfig
PKG_CHECK_EXISTS([fontconfig],
   [
    have_fontconfig="yes"
    xpost_requirements_lib_pc="${xpost_requirements_lib_pc} fontconfig"
    AC_DEFINE([HAVE_FONTCONFIG], [1], [Define to 1 if Fontconfig is detected])
   ],
   [have_fontconfig="no"])

# libxcb
PKG_CHECK_EXISTS([xcb-image xcb],
   [
    have_xcb="yes"
    xpost_requirements_bin_pc="${xpost_requirements_bin_pc} xcb-image xcb"
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

# check
have_tests="no"
if test "x${enable_tests}" = "xyes" ; then
   PKG_CHECK_MODULES([CHECK],
      [check >= 0.9.5],
      [have_tests="yes"],
      [have_tests="no"])
fi

AC_MSG_CHECKING([whether tests are built])
AC_MSG_RESULT([${have_tests}])

AM_CONDITIONAL(XPOST_ENABLE_TESTS, test "x${have_tests}" = "xyes")

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


### Checks for header files

AC_HEADER_ASSERT
AC_HEADER_TIME
AC_SYS_POSIX_TERMIOS

if test "x${ac_cv_sys_posix_termios}" = "xyes" ; then
   AC_DEFINE([HAVE_TERMIOS_H], [1], [Define to 1 if termios headers and functions are available])
fi

if test "x${enable_mmap}" = "xyes" ; then
   AC_CHECK_HEADERS([sys/mman.h], [have_mmap="yes"], [have_mmap="no"])
fi

AC_CHECK_HEADERS([libgen.h unistd.h])

### Checks for types


### Checks for structures


### Checks for compiler characteristics

AM_PROG_CC_C_O

xpost_lib_cflags="-Wall -Wextra -Wshadow -Wdeclaration-after-statement -Wmissing-prototypes -Wstrict-prototypes -Wpointer-arith -Wno-missing-field-initializers"

xpost_lib_ldflags="-fdata-sections -ffunction-sections"

if test "x${have_tests}" = "xyes" ; then
   xpost_lib_flags="${xpost_lib_flags} -g3 -ggdb3 -O2"
   XPOST_TEST_CPPFLAGS="-DDEBUG"
else
   XPOST_TEST_CPPFLAGS="-DNDEBUG"
fi

AC_SUBST([XPOST_TEST_CPPFLAGS])

XPOST_CHECK_C_COMPILER_FLAGS([XPOST_LIB], [${xpost_lib_cflags}])
XPOST_CHECK_LINKER_FLAGS([XPOST_LIB], [${xpost_lib_ldflags}])

XPOST_CHECK_C_COMPILER_FLAGS([XPOST_BIN], [-Wall -Wextra -Wshadow -Wdeclaration-after-statement -Wmissing-prototypes -Wpointer-arith -Wno-missing-field-initializers])

XPOST_CHECK_LINKER_FLAGS([XPOST_BIN], [-fdata-sections -ffunction-sections])


### Checks for linker characteristics


### Checks for library functions

AC_FUNC_ALLOCA

AC_CHECK_FUNCS([gettimeofday dirname])

if ! test "x${ac_cv_func_dirname}" = "xyes" ; then
   AC_MSG_ERROR([dirname() function is mandatory, exiting...])
fi

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
if test "x${have_mmap}" = "xyes" ; then
echo "  mmap support.........: ${have_mmap} (mremap: ${have_mremap})"
else
echo "  mmap support.........: no"
fi
echo "  Fontconfig support...: ${have_fontconfig}"
echo "  Devices:"
echo "    PGM image..........: always"
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
echo "  Valgrind.............: ${have_valgrind}"
echo
echo "Compilation............: make (or gmake)"
echo "  CPPFLAGS.............: $CPPFLAGS"
echo "  CFLAGS...............: $CFLAGS"
echo "  LDFLAGS..............: $LDFLAGS"
echo
echo "Installation...........: make install (as root if needed, with 'su' or 'sudo')"
echo "  prefix...............: $prefix"
