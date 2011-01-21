# serial 56

# Copyright (C) 1996, 1997, 1998, 1999, 2000, 2001, 2003, 2004, 2005, 2006,
# 2007, 2008, 2009, 2010 Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

dnl Initially derived from code in GNU grep.
dnl Mostly written by Jim Meyering.

AC_PREREQ([2.50])

AC_DEFUN([gl_REGEX],
[
  AC_CHECK_HEADERS_ONCE([locale.h])

  AC_ARG_WITH([included-regex],
    [AS_HELP_STRING([--without-included-regex],
                    [don't compile regex; this is the default on systems
                     with recent-enough versions of the GNU C Library
                     (use with caution on other systems).])])

  case $with_included_regex in #(
  yes|no) ac_use_included_regex=$with_included_regex
        ;;
  '')
    # If the system regex support is good enough that it passes the
    # following run test, then default to *not* using the included regex.c.
    # If cross compiling, assume the test would fail and use the included
    # regex.c.
    AC_CACHE_CHECK([for working re_compile_pattern],
                   [gl_cv_func_re_compile_pattern_working],
      [AC_RUN_IFELSE(
        [AC_LANG_PROGRAM(
          [AC_INCLUDES_DEFAULT[
           #if HAVE_LOCALE_H
            #include <locale.h>
           #endif
           #include <limits.h>
           #include <regex.h>
           ]],
          [[static struct re_pattern_buffer regex;
            unsigned char folded_chars[UCHAR_MAX + 1];
            int i;
            const char *s;
            struct re_registers regs;

            #if HAVE_LOCALE_H
              /* http://sourceware.org/ml/libc-hacker/2006-09/msg00008.html
                 This test needs valgrind to catch the bug on Debian
                 GNU/Linux 3.1 x86, but it might catch the bug better
                 on other platforms and it shouldn't hurt to try the
                 test here.  */
              if (setlocale (LC_ALL, "en_US.UTF-8"))
                {
                  static char const pat[] = "insert into";
                  static char const data[] =
                    "\xFF\0\x12\xA2\xAA\xC4\xB1,K\x12\xC4\xB1*\xACK";
                  re_set_syntax (RE_SYNTAX_GREP | RE_HAT_LISTS_NOT_NEWLINE
                                 | RE_ICASE);
                  memset (&regex, 0, sizeof regex);
                  s = re_compile_pattern (pat, sizeof pat - 1, &regex);
                  if (s)
                    return 1;
                  if (re_search (&regex, data, sizeof data - 1,
                                 0, sizeof data - 1, &regs)
                      != -1)
                    return 1;
                  if (! setlocale (LC_ALL, "C"))
                    return 1;
                }
            #endif

            /* This test is from glibc bug 3957, reported by Andrew Mackey.  */
            re_set_syntax (RE_SYNTAX_EGREP | RE_HAT_LISTS_NOT_NEWLINE);
            memset (&regex, 0, sizeof regex);
            s = re_compile_pattern ("a[^x]b", 6, &regex);
            if (s)
              return 1;

            /* This should fail, but succeeds for glibc-2.5.  */
            if (re_search (&regex, "a\nb", 3, 0, 3, &regs) != -1)
              return 1;

            /* This regular expression is from Spencer ere test number 75
               in grep-2.3.  */
            re_set_syntax (RE_SYNTAX_POSIX_EGREP);
            memset (&regex, 0, sizeof regex);
            for (i = 0; i <= UCHAR_MAX; i++)
              folded_chars[i] = i;
            regex.translate = folded_chars;
            s = re_compile_pattern ("a[[:@:>@:]]b\n", 11, &regex);
            /* This should fail with _Invalid character class name_ error.  */
            if (!s)
              return 1;

            /* Ensure that [b-a] is diagnosed as invalid, when
               using RE_NO_EMPTY_RANGES. */
            re_set_syntax (RE_SYNTAX_POSIX_EGREP | RE_NO_EMPTY_RANGES);
            memset (&regex, 0, sizeof regex);
            s = re_compile_pattern ("a[b-a]", 6, &regex);
            if (s == 0)
              return 1;

            /* This should succeed, but does not for glibc-2.1.3.  */
            memset (&regex, 0, sizeof regex);
            s = re_compile_pattern ("{1", 2, &regex);

            if (s)
              return 1;

            /* The following example is derived from a problem report
               against gawk from Jorge Stolfi <stolfi@ic.unicamp.br>.  */
            memset (&regex, 0, sizeof regex);
            s = re_compile_pattern ("[an\371]*n", 7, &regex);
            if (s)
              return 1;

            /* This should match, but does not for glibc-2.2.1.  */
            if (re_match (&regex, "an", 2, 0, &regs) != 2)
              return 1;

            memset (&regex, 0, sizeof regex);
            s = re_compile_pattern ("x", 1, &regex);
            if (s)
              return 1;

            /* glibc-2.2.93 does not work with a negative RANGE argument.  */
            if (re_search (&regex, "wxy", 3, 2, -2, &regs) != 1)
              return 1;

            /* The version of regex.c in older versions of gnulib
               ignored RE_ICASE.  Detect that problem too.  */
            re_set_syntax (RE_SYNTAX_EMACS | RE_ICASE);
            memset (&regex, 0, sizeof regex);
            s = re_compile_pattern ("x", 1, &regex);
            if (s)
              return 1;

            if (re_search (&regex, "WXY", 3, 0, 3, &regs) < 0)
              return 1;

            /* Catch a bug reported by Vin Shelton in
               http://lists.gnu.org/archive/html/bug-coreutils/2007-06/msg00089.html
               */
            re_set_syntax (RE_SYNTAX_POSIX_BASIC
                           & ~RE_CONTEXT_INVALID_DUP
                           & ~RE_NO_EMPTY_RANGES);
            memset (&regex, 0, sizeof regex);
            s = re_compile_pattern ("[[:alnum:]_-]\\\\+$", 16, &regex);
            if (s)
              return 1;

            /* REG_STARTEND was added to glibc on 2004-01-15.
               Reject older versions.  */
            if (! REG_STARTEND)
              return 1;

            /* Reject hosts whose regoff_t values are too narrow.
               These include glibc 2.3.5 on hosts with 64-bit ptrdiff_t
               and 32-bit int.  */
            if (sizeof (regoff_t) < sizeof (ptrdiff_t)
                || sizeof (regoff_t) < sizeof (ssize_t))
              return 1;

            return 0;]])],
       [gl_cv_func_re_compile_pattern_working=yes],
       [gl_cv_func_re_compile_pattern_working=no],
       dnl When crosscompiling, assume it is not working.
       [gl_cv_func_re_compile_pattern_working=no])])
    case $gl_cv_func_re_compile_pattern_working in #(
    yes) ac_use_included_regex=no;; #(
    no) ac_use_included_regex=yes;;
    esac
    ;;
  *) AC_MSG_ERROR([Invalid value for --with-included-regex: $with_included_regex])
    ;;
  esac

  if test $ac_use_included_regex = yes; then
    AC_DEFINE([_REGEX_LARGE_OFFSETS], [1],
      [Define if you want regoff_t to be at least as wide POSIX requires.])
    AC_DEFINE([re_syntax_options], [rpl_re_syntax_options],
      [Define to rpl_re_syntax_options if the replacement should be used.])
    AC_DEFINE([re_set_syntax], [rpl_re_set_syntax],
      [Define to rpl_re_set_syntax if the replacement should be used.])
    AC_DEFINE([re_compile_pattern], [rpl_re_compile_pattern],
      [Define to rpl_re_compile_pattern if the replacement should be used.])
    AC_DEFINE([re_compile_fastmap], [rpl_re_compile_fastmap],
      [Define to rpl_re_compile_fastmap if the replacement should be used.])
    AC_DEFINE([re_search], [rpl_re_search],
      [Define to rpl_re_search if the replacement should be used.])
    AC_DEFINE([re_search_2], [rpl_re_search_2],
      [Define to rpl_re_search_2 if the replacement should be used.])
    AC_DEFINE([re_match], [rpl_re_match],
      [Define to rpl_re_match if the replacement should be used.])
    AC_DEFINE([re_match_2], [rpl_re_match_2],
      [Define to rpl_re_match_2 if the replacement should be used.])
    AC_DEFINE([re_set_registers], [rpl_re_set_registers],
      [Define to rpl_re_set_registers if the replacement should be used.])
    AC_DEFINE([re_comp], [rpl_re_comp],
      [Define to rpl_re_comp if the replacement should be used.])
    AC_DEFINE([re_exec], [rpl_re_exec],
      [Define to rpl_re_exec if the replacement should be used.])
    AC_DEFINE([regcomp], [rpl_regcomp],
      [Define to rpl_regcomp if the replacement should be used.])
    AC_DEFINE([regexec], [rpl_regexec],
      [Define to rpl_regexec if the replacement should be used.])
    AC_DEFINE([regerror], [rpl_regerror],
      [Define to rpl_regerror if the replacement should be used.])
    AC_DEFINE([regfree], [rpl_regfree],
      [Define to rpl_regfree if the replacement should be used.])
    AC_LIBOBJ([regex])
    gl_PREREQ_REGEX
  fi
])

# Prerequisites of lib/regex.c and lib/regex_internal.c.
AC_DEFUN([gl_PREREQ_REGEX],
[
  AC_REQUIRE([AC_USE_SYSTEM_EXTENSIONS])
  AC_REQUIRE([AC_C_INLINE])
  AC_REQUIRE([AC_C_RESTRICT])
  AC_REQUIRE([AC_TYPE_MBSTATE_T])
  AC_CHECK_HEADERS([libintl.h])
  AC_CHECK_FUNCS_ONCE([isblank iswctype wcscoll])
  AC_CHECK_DECLS([isblank], [], [], [#include <ctype.h>])
])
