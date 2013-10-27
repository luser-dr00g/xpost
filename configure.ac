
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
   [AC_HELP_STRING([--enable-tests], [enable tests @<:@default=disabled@:>@])],
   [
    if test "x${enableval}" = "xyes" ; then
       enable_tests="yes"
    else
       enable_tests="no"
    fi
   ],
   [enable_tests="no"])


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


### Checks for header files

AC_HEADER_ASSERT
AC_HEADER_STDBOOL
AC_HEADER_TIME

if test "x${enable_mmap}" = "xyes" ; then
   AC_CHECK_HEADERS([sys/mman.h], [have_mmap="yes"], [have_mmap="no"])
fi

AC_CHECK_HEADERS([libgen.h])

### Checks for types


### Checks for structures


### Checks for compiler characteristics

AM_PROG_CC_C_O

XPOST_CHECK_C_COMPILER_FLAGS([XPOST_LIB], [-Wall -Wextra -Wshadow -Wdeclaration-after-statement -Wmissing-prototypes -Wstrict-prototypes -Wpointer-arith -Wno-missing-field-initializers -fdata-sections -ffunction-sections])

XPOST_CHECK_C_COMPILER_FLAGS([XPOST_BIN], [-Wall -Wextra -Wshadow -Wdeclaration-after-statement -Wmissing-prototypes -Wpointer-arith -Wno-missing-field-initializers -fdata-sections -ffunction-sections])


### Checks for linker characteristics


### Checks for library functions

AC_FUNC_ALLOCA

AC_CHECK_FUNCS([gettimeofday dirname])

if ! test "x${ac_cv_func_dirname}" = "xyes" ; then
   AC_MSG_ERROR([dirname() function is mandatory, exiting...])
fi

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
echo
echo "Compilation............: make (or gmake)"
echo "  CPPFLAGS.............: $CPPFLAGS"
echo "  CFLAGS...............: $CFLAGS"
echo "  LDFLAGS..............: $LDFLAGS"
echo
echo "Installation...........: make install (as root if needed, with 'su' or 'sudo')"
echo "  prefix...............: $prefix"
