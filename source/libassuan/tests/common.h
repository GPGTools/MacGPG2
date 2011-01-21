/* common.h - Common functions for the tests.
 * Copyright (C) 2006 Free Software Foundation, Inc.
 *
 * This file is part of Assuan.
 *
 * Assuan is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of
 * the License, or (at your option) any later version.
 *
 * Assuan is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <stdarg.h>

#if __GNUC__ >= 4 
# define MY_GCC_A_SENTINEL(a) __attribute__ ((sentinel(a)))
#else
# define MY_GCC_A_SENTINEL(a) 
#endif


#ifdef HAVE_W32CE_SYSTEM
#define getpid() GetCurrentProcessId ()
#define getenv(a) (NULL)
#endif

#if HAVE_W32_SYSTEM
#define SOCKET2HANDLE(s) ((void *)(s))
#define HANDLE2SOCKET(h) ((unsigned int)(h))
CRITICAL_SECTION _log_critsect;
#define _log_enter()  do { EnterCriticalSection (&_log_critsect); } while (0)
#define _log_leave()  do { LeaveCriticalSection (&_log_critsect); } while (0)
#else
#define SOCKET2HANDLE(s) (s)
#define HANDLE2SOCKET(h) (h)
#define _log_enter()  do { } while (0)
#define _log_leave()  do { } while (0)
#endif

#define DIM(v)		     (sizeof(v)/sizeof((v)[0]))
#define DIMof(type,member)   DIM(((type *)0)->member)


char *xstrconcat (const char *s1, ...) MY_GCC_A_SENTINEL(0);


static const char *log_prefix;
static int errorcount;
static int verbose;
static int debug;

void *
xmalloc (size_t n)
{
  char *p = malloc (n);
  if (!p)
    {
      if (log_prefix)
        fprintf (stderr, "%s[%u]: ", log_prefix, (unsigned int)getpid ());
      fprintf (stderr, "out of core\n");
      exit (1);
    }
  return p;
}

void *
xcalloc (size_t n, size_t m)
{
  char *p = calloc (n, m);
  if (!p)
    {
      _log_enter ();
      if (log_prefix)
        fprintf (stderr, "%s[%u]: ", log_prefix, (unsigned int)getpid ());
      fprintf (stderr, "out of core\n");
      _log_leave ();
      exit (1);
    }
  return p;
}

void
xfree (void *a)
{
  if (a)
    free (a);
}

void *
xstrdup (const char *string)
{
  char *p = xmalloc (strlen (string) + 1);
  strcpy (p, string);
  return p;
}


void
log_set_prefix (const char *s)
{
#ifdef HAVE_W32_SYSTEM
  InitializeCriticalSection (&_log_critsect);
  log_prefix = strrchr (s, '\\');
#else  
  log_prefix = strrchr (s, '/');
#endif  
  if (log_prefix)
    log_prefix++;
  else
    log_prefix = s;
}


const char *
log_get_prefix (void)
{
  return log_prefix? log_prefix:"";
}


void
log_info (const char *format, ...)
{
  va_list arg_ptr ;

  if (!verbose)
    return;

  va_start (arg_ptr, format) ;
  _log_enter ();
  if (log_prefix)
    fprintf (stderr, "%s[%u]: ", log_prefix, (unsigned int)getpid ());
  vfprintf (stderr, format, arg_ptr );
  _log_leave ();
  va_end (arg_ptr);
}


void
log_error (const char *format, ...)
{
  va_list arg_ptr ;

  va_start (arg_ptr, format) ;
  _log_enter ();
  if (log_prefix)
    fprintf (stderr, "%s[%u]: ", log_prefix, (unsigned int)getpid ());
  vfprintf (stderr, format, arg_ptr );
  _log_leave ();
  va_end (arg_ptr);
  errorcount++;
}


void
log_fatal (const char *format, ...)
{
  va_list arg_ptr ;

  va_start (arg_ptr, format) ;
  _log_enter ();
  if (log_prefix)
    fprintf (stderr, "%s[%u]: ", log_prefix, (unsigned int)getpid ());
  vfprintf (stderr, format, arg_ptr );
  _log_leave ();
  va_end (arg_ptr);
  exit (2);
}


void
log_printhex (const char *text, const void *buffer, size_t length)
{
  const unsigned char *s;

  _log_enter ();
  if (log_prefix)
    fprintf (stderr, "%s[%u]: ", log_prefix, (unsigned int)getpid ());
  fputs (text, stderr);
  for (s=buffer; length; s++, length--)
    fprintf (stderr, "%02X", *s);
  putc ('\n', stderr);
  _log_leave ();
}


/* Prepend FNAME with the srcdir environment variable's value and
   return an allocated filename. */
char *
prepend_srcdir (const char *fname)
{
  static const char *srcdir;
  char *result;

  if (!srcdir && !(srcdir = getenv ("srcdir")))
    srcdir = ".";
  
  result = xmalloc (strlen (srcdir) + 1 + strlen (fname) + 1);
  strcpy (result, srcdir);
  strcat (result, "/");
  strcat (result, fname);
  return result;
}


#ifndef HAVE_STPCPY
#undef __stpcpy
#undef stpcpy
#ifndef weak_alias
# define __stpcpy stpcpy
#endif
char *
__stpcpy (char *a,const char *b)
{
  while (*b)
    *a++ = *b++;
  *a = 0;
  return (char*)a;
}
#ifdef libc_hidden_def
libc_hidden_def (__stpcpy)
#endif
#ifdef weak_alias
weak_alias (__stpcpy, stpcpy)
#endif
#ifdef libc_hidden_builtin_def
libc_hidden_builtin_def (stpcpy)
#endif
#endif


static char *
do_strconcat (const char *s1, va_list arg_ptr)
{
  const char *argv[48];
  size_t argc;
  size_t needed;
  char *buffer, *p;

  argc = 0;
  argv[argc++] = s1;
  needed = strlen (s1);
  while (((argv[argc] = va_arg (arg_ptr, const char *))))
    {
      needed += strlen (argv[argc]);
      if (argc >= DIM (argv)-1)
        {
          fprintf (stderr, "too many args in strconcat\n");
          exit (1);
        }
      argc++;
    }
  needed++;
  buffer = xmalloc (needed);
  if (buffer)
    {
      for (p = buffer, argc=0; argv[argc]; argc++)
        p = stpcpy (p, argv[argc]);
    }
  return buffer;
}


/* Concatenate the string S1 with all the following strings up to a
   NULL.  Returns a malloced buffer or dies on malloc error.  */
char *
xstrconcat (const char *s1, ...)
{
  va_list arg_ptr;
  char *result;

  if (!s1)
    result = xstrdup ("");
  else
    {
      va_start (arg_ptr, s1);
      result = do_strconcat (s1, arg_ptr);
      va_end (arg_ptr);
    }
  return result;
}

