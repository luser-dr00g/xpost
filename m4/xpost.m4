dnl Copyright (C) 2013 Vincent Torri <vincent dot torri at gmail dot com>
dnl This code is public domain and can be freely used or copied.

dnl Macro that check if compiler of linker flags are available


dnl Macro that checks for a C compiler flag availability
dnl
dnl _XPOST_CHECK_C_COMPILER_FLAG(XPOST, FLAGS)
dnl AC_SUBST : XPOST_CFLAGS (XPOST being replaced by its value)
dnl have_flag: yes or no.
AC_DEFUN([_XPOST_CHECK_C_COMPILER_FLAG],
[dnl
m4_pushdef([UPXPOST], m4_translit([[$1]], [-a-z], [_A-Z]))dnl

dnl store in options -Wfoo if -Wno-foo is passed
option="m4_bpatsubst([[$2]], [-Wno-], [-W])"
CFLAGS_save="${CFLAGS}"
CFLAGS="${CFLAGS} ${option}"
AC_LANG_PUSH([C])

AC_MSG_CHECKING([whether the C compiler supports $2])
AC_COMPILE_IFELSE(
   [AC_LANG_PROGRAM([[]])],
   [have_flag="yes"],
   [have_flag="no"])
AC_MSG_RESULT([${have_flag}])

AC_LANG_POP([C])
CFLAGS="${CFLAGS_save}"
if test "x${have_flag}" = "xyes" ; then
   UPXPOST[_CFLAGS]="${UPXPOST[_CFLAGS]} [$2]"
fi
AC_SUBST(UPXPOST[_CFLAGS])dnl
m4_popdef([UPXPOST])dnl
])

dnl Macro that checks for a C++ compiler flag availability
dnl
dnl _XPOST_CHECK_CXX_COMPILER_FLAG(XPOST, FLAGS)
dnl AC_SUBST : XPOST_CXXFLAGS (XPOST being replaced by its value)
dnl have_flag: yes or no.
AC_DEFUN([_XPOST_CHECK_CXX_COMPILER_FLAG],
[dnl
m4_pushdef([UPXPOST], m4_translit([[$1]], [-a-z], [_A-Z]))dnl

dnl store in options -Wfoo if -Wno-foo is passed
option="m4_bpatsubst([[$2]], [-Wno-], [-W])"
CXXFLAGS_save="${CXXFLAGS}"
CXXFLAGS="${CXXFLAGS} ${option}"
AC_LANG_PUSH([C++])

AC_MSG_CHECKING([whether the C++ compiler supports $2])
AC_COMPILE_IFELSE(
   [AC_LANG_PROGRAM([[]])],
   [have_flag="yes"],
   [have_flag="no"])
AC_MSG_RESULT([${have_flag}])

AC_LANG_POP([C++])
CXXFLAGS="${CXXFLAGS_save}"
if test "x${have_flag}" = "xyes" ; then
   UPXPOST[_CXXFLAGS]="${UPXPOST[_CXXFLAGS]} [$2]"
fi
AC_SUBST(UPXPOST[_CXXFLAGS])dnl
m4_popdef([UPXPOST])dnl
])

dnl XPOST_CHECK_C_COMPILER_FLAGS(XPOST, FLAGS)
dnl Checks if FLAGS are supported and add to XPOST_CLFAGS.
dnl
dnl It will first try every flag at once, if one fails will try them one by one.
AC_DEFUN([XPOST_CHECK_C_COMPILER_FLAGS],
[dnl
_XPOST_CHECK_C_COMPILER_FLAG([$1], [$2])
if test "${have_flag}" != "yes"; then
m4_foreach_w([flag], [$2], [_XPOST_CHECK_C_COMPILER_FLAG([$1], m4_defn([flag]))])
fi
])

dnl XPOST_CHECK_CXX_COMPILER_FLAGS(XPOST, FLAGS)
dnl Checks if FLAGS are supported and add to XPOST_CXXLFAGS.
dnl
dnl It will first try every flag at once, if one fails will try them one by one.
AC_DEFUN([XPOST_CHECK_CXX_COMPILER_FLAGS],
[dnl
_XPOST_CHECK_CXX_COMPILER_FLAG([$1], [$2])
if test "${have_flag}" != "yes"; then
m4_foreach_w([flag], [$2], [_XPOST_CHECK_CXX_COMPILER_FLAG([$1], m4_defn([flag]))])
fi
])


dnl Macro that checks for a linker flag availability
dnl
dnl _XPOST_CHECK_LINKER_FLAGS(XPOST, FLAGS)
dnl AC_SUBST : XPOST_LDFLAGS (XPOST being replaced by its value)
dnl have_flag: yes or no
AC_DEFUN([_XPOST_CHECK_LINKER_FLAGS],
[dnl
m4_pushdef([UPXPOST], m4_translit([[$1]], [-a-z], [_A-Z]))dnl

LDFLAGS_save="${LDFLAGS}"
LDFLAGS="${LDFLAGS} $2"
AC_LANG_PUSH([C])

AC_MSG_CHECKING([whether the linker supports $2])
AC_LINK_IFELSE(
   [AC_LANG_PROGRAM([[]])],
   [have_flag="yes"],
   [have_flag="no"])
AC_MSG_RESULT([${have_flag}])

AC_LANG_POP([C])
LDFLAGS="${LDFLAGS_save}"
if test "x${have_flag}" = "xyes" ; then
   UPXPOST[_LDFLAGS]="${UPXPOST[_LDFLAGS]} [$2]"
fi
AC_SUBST(UPXPOST[_LDFLAGS])dnl
m4_popdef([UPXPOST])dnl
])

dnl XPOST_CHECK_LINKER_FLAGS(XPOST, FLAGS)
dnl Checks if FLAGS are supported and add to XPOST_CLFAGS.
dnl
dnl It will first try every flag at once, if one fails will try them one by one.
AC_DEFUN([XPOST_CHECK_LINKER_FLAGS],
[dnl
_XPOST_CHECK_LINKER_FLAGS([$1], [$2])
if test "${have_flag}" != "yes"; then
m4_foreach_w([flag], [$2], [_XPOST_CHECK_LINKER_FLAGS([$1], m4_defn([flag]))])
fi
])dnl

dnl XPOST_CHECK_DOXYGEN([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl Test for the doxygen program
dnl Defines DOXYGEN
dnl Defines the automake conditionnal XPOST_BUILD_DOC
dnl
AC_DEFUN([XPOST_CHECK_DOXYGEN],
[

DOXYGEN="doxygen"

AC_ARG_ENABLE([doc],
   AS_HELP_STRING(
      [--disable-doc],
      [Disable the build of the documentation @<:@default=yes@:>@]),
   [if test "${disable_doc}" = "yes" ; then
       enable_doc="no"
    else
       enable_doc="yes"
    fi],
   [enable_doc="yes"]
)

AC_ARG_WITH([doxygen],
   [AS_HELP_STRING(
      [--with-doxygen=FILE],
      [doxygen program to use @<:@default=doxygen@:>@])],
   [DOXYGEN=${withval}],
   [DOXYGEN="doxygen"])

if test "x${enable_doc}" = "xyes" ; then
   AC_CHECK_PROG([BUILD_DOCS], [${DOXYGEN}], [${DOXYGEN}], [none])
   if test "x${BUILD_DOCS}" = "xnone" ; then
      AC_MSG_WARN([doxygen requested but not found. Please check PATH or Doxygen program name])
   fi
fi


dnl
dnl Substitution
dnl
AC_SUBST([DOXYGEN])

AM_CONDITIONAL([XPOST_BUILD_DOC], [test "x${BUILD_DOCS}" != "xnone"])

AS_IF([test "x${BUILD_DOCS}" != "xnone"], [$1], [$2])

])
