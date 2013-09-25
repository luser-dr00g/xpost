
m4_define([v_maj], [0])
m4_define([v_min], [0])
m4_define([v_mic], [1])
m4_define([v_ver], [v_maj.v_min.v_mic])

AC_INIT([xpost], [v_ver], [luser.droog.remove@gmail.com])
AC_PREREQ([2.60])
AC_CONFIG_SRCDIR([configure.ac])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_USE_SYSTEM_EXTENSIONS

AM_INIT_AUTOMAKE([1.10 dist-bzip2])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])


### Additional options to configure


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


### Checks for header files

AC_HEADER_ASSERT
AC_HEADER_STDBOOL
AC_HEADER_TIME

AC_CHECK_HEADERS([sys/mman.h])


### Checks for types


### Checks for structures


### Checks for compiler characteristics

AM_PROG_CC_C_O

XPOST_CHECK_C_COMPILER_FLAGS([XPOST_BIN], [-Wall -Wextra -Wshadow -Wdeclaration-after-statement -Wmissing-prototypes -Wstrict-prototypes -Wpointer-arith -Wno-missing-field-initializers -fdata-sections -ffunction-sections])


### Checks for linker characteristics


### Checks for library functions

AC_FUNC_ALLOCA

AC_CHECK_FUNCS([gettimeofday mmap mremap])

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
echo
echo "Compilation............: make (or gmake)"
echo "  CPPFLAGS.............: $CPPFLAGS"
echo "  CFLAGS...............: $CFLAGS"
echo "  LDFLAGS..............: $LDFLAGS"
echo
echo "Installation...........: make install (as root if needed, with 'su' or 'sudo')"
echo "  prefix...............: $prefix"
