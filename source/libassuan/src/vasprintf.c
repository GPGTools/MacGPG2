/* Like vsprintf but provides a pointer to malloc'd storage, which must
   be freed by the caller.
   Copyright (C) 1994, 2002 Free Software Foundation, Inc.

This file is part of the libiberty library.
Libiberty is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

Libiberty is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with libiberty; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>


#ifndef va_copy /* According to POSIX, va_copy is a macro. */
#if defined (__GNUC__) && defined (__PPC__) \
     && (defined (_CALL_SYSV) || defined (_WIN32))
#define va_copy(d, s) (*(d) = *(s))
#elif defined (MUST_COPY_VA_BYVAL)
#define va_copy(d, s) ((d) = (s))
#else 
#define va_copy(d, s) memcpy ((d), (s), sizeof (va_list))
#endif 
#endif 


#ifdef TEST
int global_total_width;
#endif

static int int_vasprintf (char **, const char *, va_list *);

static int
int_vasprintf (result, format, args)
     char **result;
     const char *format;
     va_list *args;
{
  const char *p = format;
  /* Add one to make sure that it is never zero, which might cause malloc
     to return NULL.  */
  int total_width = strlen (format) + 1;
  va_list ap;

  va_copy (ap, *args);

  while (*p != '\0')
    {
      if (*p++ == '%')
	{
	  while (strchr ("-+ #0", *p))
	    ++p;
	  if (*p == '*')
	    {
	      ++p;
	      total_width += abs (va_arg (ap, int));
	    }
	  else
	    total_width += strtoul (p, (char **) &p, 10);
	  if (*p == '.')
	    {
	      ++p;
	      if (*p == '*')
		{
		  ++p;
		  total_width += abs (va_arg (ap, int));
		}
	      else
	      total_width += strtoul (p, (char **) &p, 10);
	    }
	  while (strchr ("hlL", *p))
	    ++p;
	  /* Should be big enough for any format specifier except %s and floats.  */
	  total_width += 30;
	  switch (*p)
	    {
	    case 'd':
	    case 'i':
	    case 'o':
	    case 'u':
	    case 'x':
	    case 'X':
	    case 'c':
	      (void) va_arg (ap, int);
	      break;
	    case 'f':
	    case 'e':
	    case 'E':
	    case 'g':
	    case 'G':
	      (void) va_arg (ap, double);
	      /* Since an ieee double can have an exponent of 307, we'll
		 make the buffer wide enough to cover the gross case. */
	      total_width += 307;
	      break;
	    case 's':
              {
                char *tmp = va_arg (ap, char *);
                if (tmp)
                  total_width += strlen (tmp);
                else /* in case the vsprintf does prints a text */
                  total_width += 25; /* e.g. "(null pointer reference)" */
              }
	      break;
	    case 'p':
	    case 'n':
	      (void) va_arg (ap, char *);
	      break;
	    }
	  p++;
	}
    }
#ifdef TEST
  global_total_width = total_width;
#endif
  *result = malloc (total_width);
  if (*result != NULL)
    return vsprintf (*result, format, *args);
  else
    return 0;
}


/* We use the _assuan prefix to keep our name space clean. */
int
_assuan_vasprintf (char **result, const char *format, va_list args)
{
  return int_vasprintf (result, format, &args);
}


int
_assuan_asprintf (char **buf, const char *fmt, ...)
{
  int status;
  va_list ap;

  va_start (ap, fmt);
  status = _assuan_vasprintf (buf, fmt, ap);
  va_end (ap);
  return status;
}


#ifdef TEST

#define asprintf  _assuan_asprintf
#define vasprintf _assuan_vasprintf

void
checkit (const char* format, ...)
{
  va_list args;
  char *result;

  va_start (args, format);
  vasprintf (&result, format, args);
  if (strlen (result) < global_total_width)
    printf ("PASS: ");
  else
    printf ("FAIL: ");
  printf ("%d %s\n", global_total_width, result);
}

int
main (void)
{
  checkit ("%d", 0x12345678);
  checkit ("%200d", 5);
  checkit ("%.300d", 6);
  checkit ("%100.150d", 7);
  checkit ("%s", "jjjjjjjjjiiiiiiiiiiiiiiioooooooooooooooooppppppppppppaa\n\
777777777777777777333333333333366666666666622222222222777777777777733333");
  checkit ("%f%s%d%s", 1.0, "foo", 77, "asdjffffffffffffffiiiiiiiiiiixxxxx");
}
#endif /* TEST */
