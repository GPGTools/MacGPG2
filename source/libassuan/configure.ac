# configure.ac - for libassuan
# Copyright (C) 2001-2003, 2006, 2007, 2009 Free Software Foundation, Inc.
# 
# This file is part of Assuan.
#
# Assuan is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation; either version 2.1 of
# the License, or (at your option) any later version.
#
# Assuan is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this program; if not, see <http://www.gnu.org/licenses/>.

# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
min_automake_version="1.10"

# Remember to change the version number immediately *after* a release.
# Set my_issvn to "yes" for non-released code.  Remember to run an
# "svn up" and "autogen.sh" right before creating a distribution.
m4_define([my_version], [2.0.1])
m4_define([my_issvn], [no])

m4_define([svn_revision], m4_esyscmd([printf "%d" $( (svn info 2>/dev/null \
            || echo 'Revision: 0')|sed -n '/^Revision:/ {s/[^0-9]//gp;q;}')]))
AC_INIT([libassuan], my_version[]m4_if(my_issvn,[yes],[-svn[]svn_revision]),
        [bug-libassuan@gnupg.org])

# LT Version numbers, remember to change them just *before* a release.
#   (Code changed:			REVISION++)
#   (Interfaces added/removed/changed:	CURRENT++, REVISION=0)
#   (Interfaces added:			AGE++)
#   (Interfaces removed/changed:	AGE=0)
#
LIBASSUAN_LT_CURRENT=1
LIBASSUAN_LT_AGE=1
LIBASSUAN_LT_REVISION=0

# If the API is changed in an incompatible way: increment the next counter.
LIBASSUAN_CONFIG_API_VERSION=2
##############################################

AC_SUBST(LIBASSUAN_LT_CURRENT)
AC_SUBST(LIBASSUAN_LT_AGE)
AC_SUBST(LIBASSUAN_LT_REVISION)

BUILD_REVISION=svn_revision
PACKAGE=$PACKAGE_NAME
VERSION=$PACKAGE_VERSION

AM_INIT_AUTOMAKE
AM_MAINTAINER_MODE
AC_CONFIG_SRCDIR(src/assuan.h.in)
AC_CONFIG_MACRO_DIR(m4)
AM_CONFIG_HEADER(config.h)
AC_CANONICAL_HOST
AB_INIT

AC_GNU_SOURCE


AC_SUBST(PACKAGE)
AC_SUBST(VERSION)
AC_DEFINE_UNQUOTED(PACKAGE, "$PACKAGE", [Name of this package])
AC_DEFINE_UNQUOTED(VERSION, "$VERSION", [Version of this package])
AC_DEFINE_UNQUOTED(PACKAGE_BUGREPORT, "$PACKAGE_BUGREPORT",
                                        [Bug report address])


# Don't default to build static libs.
LT_PREREQ([2.2.6])
LT_INIT([win32-dll disable-static])
LT_LANG([Windows Resource])

# For now we hardcode the use of version scripts.  It would be better
# to write a test for this or even implement this within libtool.
have_ld_version_script=no
case "${host}" in
    *-*-linux*)
	have_ld_version_script=yes
        ;;
    *-*-gnu*)
	have_ld_version_script=yes
        ;;
    *-apple-darwin*)
        AC_DEFINE(_XOPEN_SOURCE, 500, Activate POSIX interface on MacOS X)
        AC_DEFINE(_DARWIN_C_SOURCE, 1, Activate CMSG_LEN/CMSG_SPACE on MacOS X)
        ;;
esac

AM_CONDITIONAL(HAVE_LD_VERSION_SCRIPT, test "$have_ld_version_script" = "yes")


# Checks for programs.
missing_dir=`cd $ac_aux_dir && pwd`
AM_MISSING_PROG(ACLOCAL, aclocal, $missing_dir)
AM_MISSING_PROG(AUTOCONF, autoconf, $missing_dir)
AM_MISSING_PROG(AUTOMAKE, automake, $missing_dir)
AM_MISSING_PROG(AUTOHEADER, autoheader, $missing_dir)
AM_MISSING_PROG(MAKEINFO, makeinfo, $missing_dir)
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CPP
AM_PROG_CC_C_O
if test "x$ac_cv_prog_cc_c89" = "xno" ; then
  AC_MSG_ERROR([[No C-89 compiler found]])
fi
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
#AC_ARG_PROGRAM

# We need to compile and run a program on the build machine.  A
# comment in libgpg-error says that the AC_PROG_CC_FOR_BUILD macro in
# the AC archive is broken for autoconf 2.57.  Given that there is no
# newer version of that macro, we assume that it is also broken for
# autoconf 2.61 and thus we use a simple but usually sufficient
# approach.
AC_MSG_CHECKING(for cc for build)
if test "$cross_compiling" = "yes"; then
  CC_FOR_BUILD="${CC_FOR_BUILD-cc}"
else
  CC_FOR_BUILD="${CC_FOR_BUILD-$CC}"
fi
AC_MSG_RESULT($CC_FOR_BUILD)
AC_ARG_VAR(CC_FOR_BUILD,[build system C compiler])


if test "$GCC" = yes; then
    CFLAGS="$CFLAGS -Wall -Wcast-align -Wshadow -Wstrict-prototypes"

    AC_MSG_CHECKING([if gcc supports -Wpointer-arith])
    _gcc_cflags_save=$CFLAGS
    CFLAGS="-Wpointer-arith"
    AC_COMPILE_IFELSE(AC_LANG_PROGRAM([]),_gcc_wopt=yes,_gcc_wopt=no)
    AC_MSG_RESULT($_gcc_wopt)
    CFLAGS=$_gcc_cflags_save;
    if test x"$_gcc_wopt" = xyes ; then
       CFLAGS="$CFLAGS -Wpointer-arith"
    fi
fi


# 
# Options depending on the host OS.
# 
have_dosish_system=no
have_w32_system=no
have_w32ce_system=no
case "${host}" in
    *-linux*)
        if test "$GCC" = yes; then
          CFLAGS="$CFLAGS -fPIC -DPIC"
        fi
        ;;
    *-mingw32ce*)
        have_dosish_system=yes
        have_w32_system=yes
        have_w32ce_system=yes
        ;;
    *-mingw32*)
        have_dosish_system=yes
        have_w32_system=yes
        ;;
    *-solaris*)
	AC_DEFINE(_XOPEN_SOURCE, 500, Activate extensions on Solaris)
	AC_DEFINE(_XOPEN_SOURCE_EXTENDED, 1, Activate extensions on Solaris)
	AC_DEFINE(__EXTENSIONS__, 1, Activate extensions on Solaris)
	;;
esac

if test "$have_dosish_system" = yes; then
   AC_DEFINE(HAVE_DOSISH_SYSTEM,1,
             [Defined if we run on some of the PCDOS like systems 
              (DOS, Windoze. OS/2) with special properties like
              no file modes])
fi
dnl AM_CONDITIONAL(HAVE_DOSISH_SYSTEM, test "$have_dosish_system" = yes)

if test "$have_w32_system" = yes; then
    AC_DEFINE(HAVE_W32_SYSTEM,1,[Defined if we run on a W32 API based system])
    if test "$have_w32ce_system" = yes; then
       AC_DEFINE(HAVE_W32CE_SYSTEM,1,[Defined if we run on WindowsCE])
    fi
    BUILD_TIMESTAMP=`date --iso-8601=minutes`
    AC_SUBST(BUILD_TIMESTAMP)
    changequote(,)dnl 
    BUILD_FILEVERSION=`echo "$VERSION" | sed 's/\([0-9.]*\).*/\1./;s/\./,/g'`
    changequote([,])dnl
    case "$VERSION" in
      *-svn*) BUILD_FILEVERSION="${BUILD_FILEVERSION}0" ;;
      *-cvs)  BUILD_FILEVERSION="${BUILD_FILEVERSION}0" ;;
      *-rc*)  BUILD_FILEVERSION="${BUILD_FILEVERSION}1" ;;
      *)      BUILD_FILEVERSION="${BUILD_FILEVERSION}2" ;;
    esac
fi
AC_SUBST(BUILD_REVISION)
AC_SUBST(BUILD_TIMESTAMP)
AC_SUBST(BUILD_FILEVERSION)
AM_CONDITIONAL(HAVE_W32_SYSTEM, test "$have_w32_system" = yes)
AM_CONDITIONAL(HAVE_W32CE_SYSTEM, test "$have_w32ce_system" = yes)


# Check for network libraries.  They are needed for tests.
AC_CHECK_FUNC(setsockopt, , AC_CHECK_LIB(socket, setsockopt,
	[NETLIBS="-lsocket $NETLIBS"]))
AC_SUBST(NETLIBS)

if test "$have_w32_system" = yes; then
   if test "$have_w32ce_system" = yes; then
     NETLIBS="-lws2 $NETLIBS"
   else
     # FIXME: Check why we need to use ws2_32 and document that.
     NETLIBS="-lws2_32 $NETLIBS"
   fi
fi


# For src/libassuan-config.in
LIBASSUAN_CONFIG_LIB="-lassuan"
LIBASSUAN_CONFIG_CFLAGS=""
LIBASSUAN_CONFIG_EXTRA_LIBS=
if test x"$NETLIBS" != x; then
  LIBASSUAN_CONFIG_EXTRA_LIBS="$LIBASSUAN_CONFIG_EXTRA_LIBS $NETLIBS"
fi
AC_SUBST(LIBASSUAN_CONFIG_LIB)
AC_SUBST(LIBASSUAN_CONFIG_CFLAGS)
AC_SUBST(LIBASSUAN_CONFIG_API_VERSION)
AC_SUBST(LIBASSUAN_CONFIG_EXTRA_LIBS)

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([string.h locale.h sys/uio.h stdint.h inttypes.h])
AC_TYPE_UINTPTR_T
AC_TYPE_UINT16_T

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T
AC_TYPE_SIGNAL
AC_DECL_SYS_SIGLIST

gl_HEADER_SYS_SOCKET
gl_TYPE_SOCKLEN_T

AC_CHECK_MEMBER(struct cmsghdr.cmsg_len,
                [use_descriptor_passing=yes],
                [use_descriptor_passing=no
                 AC_MSG_WARN([
***
*** Data structure for sending ancillary data missing.
*** Descriptor passing won't work.
***])],[
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#if HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif
#include <unistd.h>
       ])
if test "$use_descriptor_passing" = "yes"; then
 AC_DEFINE(USE_DESCRIPTOR_PASSING, 1,
            [Defined if descriptor passing is supported])
fi
AM_CONDITIONAL(USE_DESCRIPTOR_PASSING, test "$use_descriptor_passing" = "yes")


# Checking for libgpg-error.
AM_PATH_GPG_ERROR(1.8,, AC_MSG_ERROR([libgpg-error was not found]))

#
# Checks for library functions.
#
AC_CHECK_FUNCS([flockfile funlockfile inet_pton])

# On some systems (e.g. Solaris) nanosleep requires linking to librl.
# Given that we use nanosleep only as an optimization over a select
# based wait function we want it only if it is available in libc.
_save_libs="$LIBS"
AC_SEARCH_LIBS([nanosleep], [],
               [AC_DEFINE(HAVE_NANOSLEEP,1,
                [Define to 1 if you have the `nanosleep' function in libc.])])
LIBS="$_save_libs"


# Check for funopen
AC_CHECK_FUNCS(funopen)
if test $ac_cv_func_funopen != yes; then
    # No funopen but we can implement that in terms of fopencookie.
    AC_CHECK_FUNCS(fopencookie)
    if test $ac_cv_func_fopencookie = yes; then
        AC_LIBOBJ([funopen])
    else
        AC_MSG_WARN([
***
*** No implementation of fopencookie or funopen available.
*** The assuan_get_data_fp function won't work; see the
*** manual for details.  GnuPG does not require this feature.
***])
    fi
fi


AC_REPLACE_FUNCS(isascii)
AC_REPLACE_FUNCS(putc_unlocked)
AC_REPLACE_FUNCS(memrchr)
AC_REPLACE_FUNCS(stpcpy)
AC_CHECK_HEADERS(unistd.h)
AC_REPLACE_FUNCS(setenv)
AC_REPLACE_FUNCS(vasprintf)


#
# Check for the getsockopt SO_PEERCRED
#
AC_MSG_CHECKING(for SO_PEERCRED)
AC_CACHE_VAL(assuan_cv_sys_so_peercred,
      [AC_TRY_COMPILE([#include <sys/socket.h>], 
         [struct ucred cr; 
          int cl = sizeof cr;
          getsockopt (1, SOL_SOCKET, SO_PEERCRED, &cr, &cl);],
          assuan_cv_sys_so_peercred=yes,
          assuan_cv_sys_so_peercred=no)
       ])
AC_MSG_RESULT($assuan_cv_sys_so_peercred) 
if test $assuan_cv_sys_so_peercred = yes; then
  AC_DEFINE(HAVE_SO_PEERCRED, 1,
            [Defined if SO_PEERCRED is supported (Linux specific)])
fi


# Create the config files.
AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([m4/Makefile])
AC_CONFIG_FILES([src/Makefile])
AC_CONFIG_FILES([doc/Makefile])
AC_CONFIG_FILES([tests/Makefile])
AC_CONFIG_FILES([src/libassuan-config], [chmod +x src/libassuan-config])
AC_CONFIG_FILES([src/versioninfo.rc])

AC_OUTPUT
