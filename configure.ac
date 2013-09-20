
m4_define([v_maj], [0])
m4_define([v_min], [0])
m4_define([v_mic], [2])
m4_define([v_ver], [v_maj.v_min.v_mic])

AC_INIT([xpost], [v_ver], [luser.droog.remove@gmail.com])
AC_PREREQ([2.52])
AC_CONFIG_SRCDIR([configure.ac])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.6 dist-bzip2])
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

AC_PROG_CC_C99


### Checks for libraries


### Checks for header files

AC_HEADER_ASSERT
AC_HEADER_STDBOOL
AC_HEADER_TIME


### Checks for types


### Checks for structures


### Checks for compiler characteristics

AM_PROG_CC_C_O

XPOST_CHECK_C_COMPILER_FLAGS([XPOST_BIN], [-Wall -Wextra -Wshadow -Wdeclaration-after-statement -Wmissing-prototypes -Wstrict-prototypes -Wpointer-arith -Wno-missing-field-initializers -fvisibility=hidden -fdata-sections -ffunction-sections])


### Checks for linker characteristics


### Checks for library functions

AC_FUNC_ALLOCA


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
echo "Compilation............: make (or gmake)"
echo "  CPPFLAGS.............: $CPPFLAGS"
echo "  CFLAGS...............: $CFLAGS"
echo "  LDFLAGS..............: $LDFLAGS"
echo
echo "Installation...........: make install (as root if needed, with 'su' or 'sudo')"
echo "  prefix...............: $prefix"
