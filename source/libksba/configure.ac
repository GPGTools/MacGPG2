# configure.ac - for libksba
# Copyright (C) 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008,
#               2010  g10 Code GmbH
# 
# This file is part of KSBA
# 
# KSBA is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
# 
# KSBA is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <http://www.gnu.org/licenses/>.

# Process this file with autoconf to produce a configure script.
AC_PREREQ(2.61)
min_automake_version="1.10"


# Remember to change the version number immediately *after* a release.
# Set my_issvn to "yes" for non-released code.  Remember to run an
# "svn up" and "autogen.sh" right before creating a distribution.
m4_define([my_version], [1.1.0])
m4_define([my_issvn], [no])

m4_define([svn_revision], m4_esyscmd([printf "%d" $(svn info 2>/dev/null \
          | sed -n '/^Revision:/ s/[^0-9]//gp'|head -1)]))
AC_INIT([libksba],
        [my_version[]m4_if(my_issvn,[yes],[-svn[]svn_revision])],
        [gnupg-devel@gnupg.org])

# LT Version numbers: Remember to change them just *before* a release.
#   (Interfaces removed:    CURRENT++, AGE=0, REVISION=0)
#   (Interfaces added:      CURRENT++, AGE++, REVISION=0)
#   (No interfaces changed:                   REVISION++)
# Please remember to document interface changes in the NEWS file.
LIBKSBA_LT_CURRENT=18
LIBKSBA_LT_AGE=10
LIBKSBA_LT_REVISION=0
#-------------------
# If the API is changed in an incompatible way: increment the next counter.
KSBA_CONFIG_API_VERSION=1


NEED_GPG_ERROR_VERSION=1.8

BUILD_REVISION=svn_revision
PACKAGE=$PACKAGE_NAME
VERSION=$PACKAGE_VERSION

AC_CONFIG_SRCDIR([src/ksba.h])
AM_INIT_AUTOMAKE($PACKAGE, $VERSION)
AM_CONFIG_HEADER(config.h)
AC_CONFIG_MACRO_DIR([m4])
AC_CANONICAL_HOST
AB_INIT

AC_GNU_SOURCE

LT_PREREQ([2.2.6])
LT_INIT([win32-dll disable-static])
LT_LANG([Windows Resource])

AM_MAINTAINER_MODE


AC_SUBST(LIBKSBA_LT_CURRENT)
AC_SUBST(LIBKSBA_LT_AGE)
AC_SUBST(LIBKSBA_LT_REVISION)
AC_SUBST(PACKAGE)
AC_SUBST(VERSION)
AC_DEFINE_UNQUOTED(PACKAGE, "$PACKAGE", [Name of this package])
AC_DEFINE_UNQUOTED(VERSION, "$VERSION", [Version of this package])

AH_TOP([
#ifndef _KSBA_CONFIG_H_INCLUDED
#define _KSBA_CONFIG_H_INCLUDED

/* Enable gpg-error's strerror macro for W32CE.  */
#define GPG_ERR_ENABLE_ERRNO_MACROS 1
])

AH_BOTTOM([

#endif /*_KSBA_CONFIG_H_INCLUDED*/
])



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
gl_EARLY
#AC_ARG_PROGRAM
AC_PROG_YACC
AC_C_INLINE

# We need to compile and run a program on the build machine.
#   The AC_PROG_CC_FOR_BUILD macro in the AC archive is broken for
#   autoconf 2.57.
AC_MSG_CHECKING(for cc for build)
if test "$cross_compiling" = "yes"; then
  CC_FOR_BUILD="${CC_FOR_BUILD-cc}"
else
  CC_FOR_BUILD="${CC_FOR_BUILD-$CC}"
fi
AC_MSG_RESULT($CC_FOR_BUILD)
AC_ARG_VAR(CC_FOR_BUILD,[build system C compiler])


# This is handy for debugging so the compiler doesn't rearrange
# things and eliminate variables.
AC_ARG_ENABLE(optimization,
       AC_HELP_STRING([--disable-optimization],
		      [disable compiler optimization]),
                      [if test $enableval = no ; then
                         CFLAGS=`echo $CFLAGS | sed 's/-O[[0-9]]//'`
                       fi])

if test "$GCC" = yes; then
    CFLAGS="$CFLAGS -Wall -Wcast-align -Wshadow -Wstrict-prototypes"

    if test "$USE_MAINTAINER_MODE" = "yes"; then
        CFLAGS="$CFLAGS -Wformat -Wno-format-y2k -Wformat-security"

        # We use -W only if -Wno-missing-field-initializers is supported.
        # -W is important because it detects errors like "if (foo);"
        AC_MSG_CHECKING([if gcc supports -Wno-missing-field-initializers])
        _gcc_cflags_save=$CFLAGS
        CFLAGS="-Wno-missing-field-initializers"
        AC_COMPILE_IFELSE(AC_LANG_PROGRAM([]),_gcc_wopt=yes,_gcc_wopt=no)
        AC_MSG_RESULT($_gcc_wopt)
        CFLAGS=$_gcc_cflags_save;
        if test x"$_gcc_wopt" = xyes ; then
          CFLAGS="$CFLAGS -W -Wno-sign-compare -Wno-missing-field-initializers"
        fi

        AC_MSG_CHECKING([if gcc supports -Wdeclaration-after-statement])
        _gcc_cflags_save=$CFLAGS
        CFLAGS="-Wdeclaration-after-statement"
        AC_COMPILE_IFELSE(AC_LANG_PROGRAM([]),_gcc_wopt=yes,_gcc_wopt=no)
        AC_MSG_RESULT($_gcc_wopt)
        CFLAGS=$_gcc_cflags_save;
        if test x"$_gcc_wopt" = xyes ; then
          CFLAGS="$CFLAGS -Wdeclaration-after-statement"
        fi
    fi

    AC_MSG_CHECKING([if gcc supports -Wpointer-arith])
    _gcc_cflags_save=$CFLAGS
    CFLAGS="-Wpointer-arith"
    AC_COMPILE_IFELSE(AC_LANG_PROGRAM([]),_gcc_wopt=yes,_gcc_wopt=no)
    AC_MSG_RESULT($_gcc_wopt)
    CFLAGS=$_gcc_cflags_save;
    if test x"$_gcc_wopt" = xyes ; then
       CFLAGS="$CFLAGS -Wpointer-arith"
    fi

    AC_MSG_CHECKING([if gcc supports -Wno-pointer-sign])
    _gcc_cflags_save=$CFLAGS
    CFLAGS="-Wno-pointer-sign"
    AC_COMPILE_IFELSE(AC_LANG_PROGRAM([]),_gcc_psign=yes,_gcc_psign=no)
    AC_MSG_RESULT($_gcc_psign)
    CFLAGS=$_gcc_cflags_save;
    if test x"$_gcc_psign" = xyes ; then
       CFLAGS="$CFLAGS -Wno-pointer-sign"
    fi

fi

# Setup some stuff depending on host.
have_w32_system=no
have_w32ce_system=no
case "${host}" in
    *-*-mingw32ce*)
      have_w32_system=yes
      have_w32ce_system=yes
      ;;
    *-*-mingw32*)
      have_w32_system=yes
      ;;
    *)
      ;;
esac
if test "$have_w32_system" = yes; then
   AC_DEFINE(HAVE_W32_SYSTEM,1, [Defined if we run on a W32 API based system])
   if test "$have_w32ce_system" = yes; then
     AC_DEFINE(HAVE_W32CE_SYSTEM,1,[Defined if we run on WindowsCE])
   fi
fi
AM_CONDITIONAL(HAVE_W32_SYSTEM, test "$have_w32_system" = yes)
AM_CONDITIONAL(HAVE_W32CE_SYSTEM, test "$have_w32ce_system" = yes)


# For some systems we know that we have ld_version scripts.  
# Use it then as default.
have_ld_version_script=no
case "${host}" in
    *-*-linux*)
	have_ld_version_script=yes
        ;;
    *-*-gnu*)
	have_ld_version_script=yes
        ;;
esac
AC_ARG_ENABLE([ld-version-script],
              AC_HELP_STRING([--enable-ld-version-script],
                             [enable/disable use of linker version script.
                              (default is system dependent)]),
              [have_ld_version_script=$enableval],
              [ : ] )
AM_CONDITIONAL(HAVE_LD_VERSION_SCRIPT, test "$have_ld_version_script" = "yes")


#            
# Check for ELF visibility support.
#
AC_CACHE_CHECK(whether the visibility attribute is supported,
       ksba_cv_visibility_attribute,
       [ksba_cv_visibility_attribute=no
        AC_LANG_CONFTEST([AC_LANG_SOURCE(
          [[int foo __attribute__ ((visibility ("hidden"))) = 1;
            int bar __attribute__ ((visibility ("protected"))) = 1;
          ]])])
        
        if ${CC-cc} -Werror -S conftest.c -o conftest.s \
                  1>&AS_MESSAGE_LOG_FD 2>&AS_MESSAGE_LOG_FD ; then
            if grep '\.hidden.*foo' conftest.s >/dev/null 2>&1 ; then
                if grep '\.protected.*bar' conftest.s >/dev/null 2>&1; then
                    ksba_cv_visibility_attribute=yes
                fi
            fi  
        fi  
       ])
if test "$ksba_cv_visibility_attribute" = "yes"; then
    AC_CACHE_CHECK(for broken visibility attribute,
       ksba_cv_broken_visibility_attribute,
       [ksba_cv_broken_visibility_attribute=yes
        AC_LANG_CONFTEST([AC_LANG_SOURCE(
          [[int foo (int x);
            int bar (int x) __asm__ ("foo")
                            __attribute__ ((visibility ("hidden")));
            int bar (int x) { return x; }
          ]])])
                  
        if ${CC-cc} -Werror -S conftest.c -o conftest.s \
                  1>&AS_MESSAGE_LOG_FD 2>&AS_MESSAGE_LOG_FD ; then
           if grep '\.hidden@<:@ 	_@:>@foo' conftest.s >/dev/null 2>&1;
            then
               ksba_cv_broken_visibility_attribute=no
           fi
        fi
       ])
fi
if test "$ksba_cv_visibility_attribute" = "yes"; then
    AC_CACHE_CHECK(for broken alias attribute,
       ksba_cv_broken_alias_attribute,
       [ksba_cv_broken_alias_attribute=yes
        AC_LANG_CONFTEST([AC_LANG_SOURCE(
          [[extern int foo (int x) __asm ("xyzzy");
            int bar (int x) { return x; }
            extern __typeof (bar) foo __attribute ((weak, alias ("bar")));
            extern int dfoo;
            extern __typeof (dfoo) dfoo __asm ("abccb");
            int dfoo = 1;
          ]])])

        if ${CC-cc} -Werror -S conftest.c -o conftest.s \
                  1>&AS_MESSAGE_LOG_FD 2>&AS_MESSAGE_LOG_FD ; then
           if grep 'xyzzy' conftest.s >/dev/null 2>&1 && \
              grep 'abccb' conftest.s >/dev/null 2>&1; then
              ksba_cv_broken_alias_attribute=no
           fi
        fi
        ])
fi
if test "$ksba_cv_visibility_attribute" = "yes"; then
    AC_CACHE_CHECK(if gcc supports -fvisibility=hidden,
       ksba_cv_gcc_has_f_visibility,
       [ksba_cv_gcc_has_f_visibility=no
        _gcc_cflags_save=$CFLAGS
        CFLAGS="-fvisibility=hidden"
        AC_COMPILE_IFELSE(AC_LANG_PROGRAM([]),
                          ksba_cv_gcc_has_f_visibility=yes)
        CFLAGS=$_gcc_cflags_save;
       ])
fi
if test "$ksba_cv_visibility_attribute" = "yes" \
   && test "$ksba_cv_broken_visibility_attribute" != "yes" \
   && test "$ksba_cv_broken_alias_attribute" != "yes" \
   && test "$ksba_cv_gcc_has_f_visibility" = "yes"  
 then
   AC_DEFINE(KSBA_USE_VISIBILITY, 1,
               [Define to use the GNU C visibility attribute.])
   CFLAGS="$CFLAGS -fvisibility=hidden"
fi


#
# Checks for libraries.
#
AM_PATH_GPG_ERROR("$NEED_GPG_ERROR_VERSION")
if test "x$GPG_ERROR_LIBS" = "x"; then
  AC_MSG_ERROR([libgpg-error is needed.
                See ftp://ftp.gnupg.org/gcrypt/libgpg-error/ .])
fi

AC_DEFINE(GPG_ERR_SOURCE_DEFAULT, GPG_ERR_SOURCE_KSBA,
          [The default error source for libksba.])


# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([string.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST

AC_C_BIGENDIAN

AC_CHECK_SIZEOF(unsigned int)
AC_CHECK_SIZEOF(unsigned long)
GNUPG_CHECK_TYPEDEF(u32, HAVE_U32_TYPEDEF)


# Checks for library functions.
AC_CHECK_FUNCS([memmove strchr strtol strtoul stpcpy gmtime_r getenv])


# GNUlib checks
gl_SOURCE_BASE(gl)
gl_M4_BASE(gl/m4)
gl_MODULES(alloca)
gl_INIT

# To be used in ksba-config
KSBA_CONFIG_LIBS="-lksba"
KSBA_CONFIG_CFLAGS=""
AC_SUBST(KSBA_CONFIG_LIBS)
AC_SUBST(KSBA_CONFIG_CFLAGS)
AC_SUBST(KSBA_CONFIG_API_VERSION)

# The Makefiles need to know about cross compiling
AM_CONDITIONAL(CROSS_COMPILING, test x$cross_compiling != xno)


# Generate extended version information for W32.
if test "$have_w32_system" = yes; then
   BUILD_TIMESTAMP=`date --iso-8601=minutes`
   changequote(,)dnl 
   BUILD_FILEVERSION=`echo "$VERSION" | sed 's/\([0-9.]*\).*/\1./;s/\./,/g'`
   changequote([,])dnl
   BUILD_FILEVERSION="${BUILD_FILEVERSION}${BUILD_REVISION}"
fi
AC_SUBST(BUILD_REVISION)
AC_SUBST(BUILD_TIMESTAMP)
AC_SUBST(BUILD_FILEVERSION)
AC_DEFINE_UNQUOTED(BUILD_REVISION, "$BUILD_REVISION", 
                   [Subversion revision used to build this package])


AC_CONFIG_FILES([
Makefile
m4/Makefile
gl/Makefile
src/Makefile
src/ksba-config
src/versioninfo.rc
tests/Makefile
doc/Makefile
])
AC_OUTPUT


echo "
        Libksba v${VERSION} has been configured as follows:
        
        Platform:  $host

"
