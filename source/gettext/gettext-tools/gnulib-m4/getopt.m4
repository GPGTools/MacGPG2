# getopt.m4 serial 28
dnl Copyright (C) 2002-2006, 2008-2010 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

# Request a POSIX compliant getopt function.
AC_DEFUN([gl_FUNC_GETOPT_POSIX],
[
  m4_divert_text([DEFAULTS], [gl_getopt_required=POSIX])
  AC_REQUIRE([gl_UNISTD_H_DEFAULTS])
  gl_GETOPT_IFELSE([
    gl_REPLACE_GETOPT
  ],
  [])
])

# Request a POSIX compliant getopt function with GNU extensions (such as
# options with optional arguments) and the functions getopt_long,
# getopt_long_only.
AC_DEFUN([gl_FUNC_GETOPT_GNU],
[
  m4_divert_text([INIT_PREPARE], [gl_getopt_required=GNU])

  AC_REQUIRE([gl_FUNC_GETOPT_POSIX])
])

# Request the gnulib implementation of the getopt functions unconditionally.
# argp.m4 uses this.
AC_DEFUN([gl_REPLACE_GETOPT],
[
  dnl Arrange for getopt.h to be created.
  gl_GETOPT_SUBSTITUTE_HEADER
  dnl Arrange for unistd.h to include getopt.h.
  GNULIB_UNISTD_H_GETOPT=1
  dnl Arrange to compile the getopt implementation.
  AC_LIBOBJ([getopt])
  AC_LIBOBJ([getopt1])
  gl_PREREQ_GETOPT
])

# emacs' configure.in uses this.
AC_DEFUN([gl_GETOPT_IFELSE],
[
  AC_REQUIRE([gl_GETOPT_CHECK_HEADERS])
  AS_IF([test -n "$gl_replace_getopt"], [$1], [$2])
])

# Determine whether to replace the entire getopt facility.
AC_DEFUN([gl_GETOPT_CHECK_HEADERS],
[
  AC_REQUIRE([AC_CANONICAL_HOST]) dnl for cross-compiles

  dnl Persuade Solaris <unistd.h> to declare optarg, optind, opterr, optopt.
  AC_REQUIRE([AC_USE_SYSTEM_EXTENSIONS])

  gl_CHECK_NEXT_HEADERS([getopt.h])
  AC_CHECK_HEADERS_ONCE([getopt.h])
  if test $ac_cv_header_getopt_h = yes; then
    HAVE_GETOPT_H=1
  else
    HAVE_GETOPT_H=0
  fi
  AC_SUBST([HAVE_GETOPT_H])

  gl_replace_getopt=

  dnl Test whether <getopt.h> is available.
  if test -z "$gl_replace_getopt" && test $gl_getopt_required = GNU; then
    AC_CHECK_HEADERS([getopt.h], [], [gl_replace_getopt=yes])
  fi

  dnl Test whether the function getopt_long is available.
  if test -z "$gl_replace_getopt" && test $gl_getopt_required = GNU; then
    AC_CHECK_FUNCS([getopt_long_only], [], [gl_replace_getopt=yes])
  fi

  dnl BSD getopt_long uses an incompatible method to reset option processing.
  dnl Existence of the variable, in and of itself, is not a reason to replace
  dnl getopt, but knowledge of the variable is needed to determine how to
  dnl reset and whether a reset reparses the environment.
  dnl Solaris supports neither optreset nor optind=0, but keeps no state that
  dnl needs a reset beyond setting optind=1; detect Solaris by getopt_clip.
  if test -z "$gl_replace_getopt"; then
    AC_CHECK_DECLS([optreset], [],
      [AC_CHECK_DECLS([getopt_clip], [], [],
        [[#include <getopt.h>]])
      ],
      [[#include <getopt.h>]])
  fi

  dnl mingw's getopt (in libmingwex.a) does weird things when the options
  dnl strings starts with '+' and it's not the first call.  Some internal state
  dnl is left over from earlier calls, and neither setting optind = 0 nor
  dnl setting optreset = 1 get rid of this internal state.
  dnl POSIX is silent on optind vs. optreset, so we allow either behavior.
  dnl POSIX 2008 does not specify leading '+' behavior, but see
  dnl http://austingroupbugs.net/view.php?id=191 for a recommendation on
  dnl the next version of POSIX.  For now, we only guarantee leading '+'
  dnl behavior with getopt-gnu.
  if test -z "$gl_replace_getopt"; then
    AC_CACHE_CHECK([whether getopt is POSIX compatible],
      [gl_cv_func_getopt_posix],
      [
        dnl This test fails on mingw and succeeds on all other platforms.
        AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#if !HAVE_DECL_OPTRESET && !HAVE_DECL_GETOPT_CLIP
# define OPTIND_MIN 0
#else
# define OPTIND_MIN 1
#endif

int
main ()
{
  {
    int argc = 0;
    char *argv[10];
    int c;

    argv[argc++] = "program";
    argv[argc++] = "-a";
    argv[argc++] = "foo";
    argv[argc++] = "bar";
    argv[argc] = NULL;
    optind = OPTIND_MIN;
    opterr = 0;

    c = getopt (argc, argv, "ab");
    if (!(c == 'a'))
      return 1;
    c = getopt (argc, argv, "ab");
    if (!(c == -1))
      return 2;
    if (!(optind == 2))
      return 3;
  }
  /* Some internal state exists at this point.  */
  {
    int argc = 0;
    char *argv[10];
    int c;

    argv[argc++] = "program";
    argv[argc++] = "donald";
    argv[argc++] = "-p";
    argv[argc++] = "billy";
    argv[argc++] = "duck";
    argv[argc++] = "-a";
    argv[argc++] = "bar";
    argv[argc] = NULL;
    optind = OPTIND_MIN;
    opterr = 0;

    c = getopt (argc, argv, "+abp:q:");
    if (!(c == -1))
      return 4;
    if (!(strcmp (argv[0], "program") == 0))
      return 5;
    if (!(strcmp (argv[1], "donald") == 0))
      return 6;
    if (!(strcmp (argv[2], "-p") == 0))
      return 7;
    if (!(strcmp (argv[3], "billy") == 0))
      return 8;
    if (!(strcmp (argv[4], "duck") == 0))
      return 9;
    if (!(strcmp (argv[5], "-a") == 0))
      return 10;
    if (!(strcmp (argv[6], "bar") == 0))
      return 11;
    if (!(optind == 1))
      return 12;
  }
  /* Detect MacOS 10.5 bug.  */
  {
    char *argv[3] = { "program", "-ab", NULL };
    optind = OPTIND_MIN;
    opterr = 0;
    if (getopt (2, argv, "ab:") != 'a')
      return 13;
    if (getopt (2, argv, "ab:") != '?')
      return 14;
    if (optopt != 'b')
      return 15;
    if (optind != 2)
      return 16;
  }

  return 0;
}
]])],
          [gl_cv_func_getopt_posix=yes], [gl_cv_func_getopt_posix=no],
          [case "$host_os" in
             mingw*) gl_cv_func_getopt_posix="guessing no";;
             darwin*) gl_cv_func_getopt_posix="guessing no";;
             *)      gl_cv_func_getopt_posix="guessing yes";;
           esac
          ])
      ])
    case "$gl_cv_func_getopt_posix" in
      *no) gl_replace_getopt=yes ;;
    esac
  fi

  if test -z "$gl_replace_getopt" && test $gl_getopt_required = GNU; then
    AC_CACHE_CHECK([for working GNU getopt function], [gl_cv_func_getopt_gnu],
      [# Even with POSIXLY_CORRECT, the GNU extension of leading '-' in the
       # optstring is necessary for programs like m4 that have POSIX-mandated
       # semantics for supporting options interspersed with files.
       # Also, since getopt_long is a GNU extension, we require optind=0.
       gl_had_POSIXLY_CORRECT=${POSIXLY_CORRECT:+yes}
       POSIXLY_CORRECT=1
       export POSIXLY_CORRECT
       AC_RUN_IFELSE(
        [AC_LANG_PROGRAM([[#include <getopt.h>
                           #include <stddef.h>
                           #include <string.h>
           ]], [[
             /* This code succeeds on glibc 2.8, OpenBSD 4.0, Cygwin, mingw,
                and fails on MacOS X 10.5, AIX 5.2, HP-UX 11, IRIX 6.5,
                OSF/1 5.1, Solaris 10.  */
             {
               char *myargv[3];
               myargv[0] = "conftest";
               myargv[1] = "-+";
               myargv[2] = 0;
               opterr = 0;
               if (getopt (2, myargv, "+a") != '?')
                 return 1;
             }
             /* This code succeeds on glibc 2.8, mingw,
                and fails on MacOS X 10.5, OpenBSD 4.0, AIX 5.2, HP-UX 11,
                IRIX 6.5, OSF/1 5.1, Solaris 10, Cygwin 1.5.x.  */
             {
               char *argv[] = { "program", "-p", "foo", "bar", NULL };

               optind = 1;
               if (getopt (4, argv, "p::") != 'p')
                 return 2;
               if (optarg != NULL)
                 return 3;
               if (getopt (4, argv, "p::") != -1)
                 return 4;
               if (optind != 2)
                 return 5;
             }
             /* This code succeeds on glibc 2.8 and fails on Cygwin 1.7.0.  */
             {
               char *argv[] = { "program", "foo", "-p", NULL };
               optind = 0;
               if (getopt (3, argv, "-p") != 1)
                 return 6;
               if (getopt (3, argv, "-p") != 'p')
                 return 7;
             }
             /* This code fails on glibc 2.11.  */
             {
               char *argv[] = { "program", "-b", "-a", NULL };
               optind = opterr = 0;
               if (getopt (3, argv, "+:a:b") != 'b')
                 return 8;
               if (getopt (3, argv, "+:a:b") != ':')
                 return 9;
             }
             return 0;
           ]])],
        [gl_cv_func_getopt_gnu=yes],
        [gl_cv_func_getopt_gnu=no],
        [dnl Cross compiling. Guess based on host and declarations.
         case $host_os:$ac_cv_have_decl_optreset in
           *-gnu*:* | mingw*:*) gl_cv_func_getopt_gnu=no;;
           *:yes)               gl_cv_func_getopt_gnu=no;;
           *)                   gl_cv_func_getopt_gnu=yes;;
         esac
        ])
       if test "$gl_had_POSIXLY_CORRECT" != yes; then
         AS_UNSET([POSIXLY_CORRECT])
       fi
      ])
    if test "$gl_cv_func_getopt_gnu" = "no"; then
      gl_replace_getopt=yes
    fi
  fi
])

# emacs' configure.in uses this.
AC_DEFUN([gl_GETOPT_SUBSTITUTE_HEADER],
[
  GETOPT_H=getopt.h
  AC_DEFINE([__GETOPT_PREFIX], [[rpl_]],
    [Define to rpl_ if the getopt replacement functions and variables
     should be used.])
  AC_SUBST([GETOPT_H])
])

# Prerequisites of lib/getopt*.
# emacs' configure.in uses this.
AC_DEFUN([gl_PREREQ_GETOPT],
[
  AC_CHECK_DECLS_ONCE([getenv])
])
