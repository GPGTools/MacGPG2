# locale-tr.m4 serial 6
dnl Copyright (C) 2003, 2005-2010 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl From Bruno Haible.

dnl Determine the name of a turkish locale with UTF-8 encoding.
AC_DEFUN([gt_LOCALE_TR_UTF8],
[
  AC_REQUIRE([AM_LANGINFO_CODESET])
  AC_CACHE_CHECK([for a turkish Unicode locale], [gt_cv_locale_tr_utf8], [
    AC_LANG_CONFTEST([AC_LANG_SOURCE([
changequote(,)dnl
#include <locale.h>
#include <time.h>
#if HAVE_LANGINFO_CODESET
# include <langinfo.h>
#endif
#include <stdlib.h>
#include <string.h>
struct tm t;
char buf[16];
int main () {
  /* On BeOS, locales are not implemented in libc.  Rather, libintl
     imitates locale dependent behaviour by looking at the environment
     variables, and all locales use the UTF-8 encoding.  But BeOS does not
     implement the Turkish upper-/lowercase mappings.  Therefore, let this
     program return 1 on BeOS.  */
  /* Check whether the given locale name is recognized by the system.  */
  if (setlocale (LC_ALL, "") == NULL) return 1;
  /* Check whether nl_langinfo(CODESET) is nonempty and not "ASCII" or "646".
     On MacOS X 10.3.5 (Darwin 7.5) in the tr_TR locale, nl_langinfo(CODESET)
     is empty, and the behaviour of Tcl 8.4 in this locale is not useful.
     On OpenBSD 4.0, when an unsupported locale is specified, setlocale()
     succeeds but then nl_langinfo(CODESET) is "646". In this situation,
     some unit tests fail.  */
#if HAVE_LANGINFO_CODESET
  {
    const char *cs = nl_langinfo (CODESET);
    if (cs[0] == '\0' || strcmp (cs, "ASCII") == 0 || strcmp (cs, "646") == 0)
      return 1;
  }
#endif
#ifdef __CYGWIN__
  /* On Cygwin, avoid locale names without encoding suffix, because the
     locale_charset() function relies on the encoding suffix.  Note that
     LC_ALL is set on the command line.  */
  if (strchr (getenv ("LC_ALL"), '.') == NULL) return 1;
#endif
  /* Check whether in the abbreviation of the eighth month, the second
     character (should be U+011F: LATIN SMALL LETTER G WITH BREVE) is
     two bytes long, with UTF-8 encoding.  */
  t.tm_year = 1992 - 1900; t.tm_mon = 8 - 1; t.tm_mday = 19;
  if (strftime (buf, sizeof (buf), "%b", &t) < 4
      || buf[1] != (char) 0xc4 || buf[2] != (char) 0x9f)
    return 1;
  /* Check whether the upper-/lowercase mappings are as expected for
     Turkish.  */
  if (towupper ('i') != 0x0130 || towlower (0x0130) != 'i'
      || towupper(0x0131) != 'I' || towlower ('I') != 0x0131)
    return 1;
  return 0;
}
changequote([,])dnl
      ])])
    if AC_TRY_EVAL([ac_link]) && test -s conftest$ac_exeext; then
      # Setting LC_ALL is not enough. Need to set LC_TIME to empty, because
      # otherwise on MacOS X 10.3.5 the LC_TIME=C from the beginning of the
      # configure script would override the LC_ALL setting. Likewise for
      # LC_CTYPE, which is also set at the beginning of the configure script.
      # Test for the usual locale name.
      if (LC_ALL=tr_TR LC_TIME= LC_CTYPE= ./conftest; exit) 2>/dev/null; then
        gt_cv_locale_tr_utf8=tr_TR
      else
        # Test for the locale name with explicit encoding suffix.
        if (LC_ALL=tr_TR.UTF-8 LC_TIME= LC_CTYPE= ./conftest; exit) 2>/dev/null; then
          gt_cv_locale_tr_utf8=tr_TR.UTF-8
        else
          # Test for the Solaris 7 locale name.
          if (LC_ALL=tr.UTF-8 LC_TIME= LC_CTYPE= ./conftest; exit) 2>/dev/null; then
            gt_cv_locale_tr_utf8=tr.UTF-8
          else
            # None found.
            gt_cv_locale_tr_utf8=none
          fi
        fi
      fi
    else
      gt_cv_locale_tr_utf8=none
    fi
    rm -fr conftest*
  ])
  LOCALE_TR_UTF8=$gt_cv_locale_tr_utf8
  AC_SUBST([LOCALE_TR_UTF8])
])
