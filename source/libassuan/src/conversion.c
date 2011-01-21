/* conversion.c - String conversion helper functions.
   Copyright (C) 2000 Werner Koch (dd9jn)
   Copyright (C) 2001, 2002, 2003, 2004, 2007, 2009 g10 Code GmbH
 
   This file is part of Assuan.

   Assuan is free software; you can redistribute it and/or modify it
   under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of
   the License, or (at your option) any later version.
   
   Assuan is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.
   
   You should have received a copy of the GNU Lesser General Public
   License along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include "assuan-defs.h"
#include "debug.h"


/* Convert the number NR to a hexadecimal string.  Returns the tail
   pointer.  */
static char *
_assuan_bytetohex (int nr, char *str)
{
  static char hexdigits[] = "0123456789abcdef";
  int i;

#define NROFHEXDIGITS 2
  for (i = 0; i < NROFHEXDIGITS; i++)
    {
      int digit = (nr >> (i << 2)) & 0xf;
      *(str++) = hexdigits[digit];
    }
  return str;
}


/* Encode the C formatted string SRC and return the malloc'ed result.  */
char *
_assuan_encode_c_string (assuan_context_t ctx, const char *src)
{
  const unsigned char *istr;
  char *res;
  char *ostr;
  
  ostr = _assuan_malloc (ctx, 4 * strlen (src) + 1);
  if (! *ostr)
    return NULL;

  res = ostr;

  for (istr = (const unsigned char *) src; *istr; istr++)
    {
      int c = 0;

      switch (*istr)
	{
	case '\r':
	  c = 'r';
	  break;

	case '\n':
	  c = 'n';
	  break;

	case '\f':
	  c = 'f';
	  break;

	case '\v':
	  c = 'v';
	  break;

	case '\b':
	  c = 'b';
	  break;

	default:
	  if ((isascii (*istr) && isprint (*istr)) || (*istr >= 0x80))
	    *(ostr++) = *istr;
	  else
	    {
	      *(ostr++) = '\\';
	      *(ostr++) = 'x';
	      ostr = _assuan_bytetohex (*istr, ostr);
	    }
	}

      if (c)
	{
	  *(ostr++) = '\\';
	  *(ostr++) = c;
	}
    }
  *(ostr) = '\0';

  return res;
}
