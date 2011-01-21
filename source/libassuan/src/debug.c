/* debug.c - helpful output in desperate situations
   Copyright (C) 2000 Werner Koch (dd9jn)
   Copyright (C) 2001, 2002, 2003, 2004, 2005, 2007, 2009 g10 Code GmbH
 
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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, 
   MA 02110-1301, USA.  */

#if HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#ifndef HAVE_DOSISH_SYSTEM
#  include <sys/types.h>
#  include <sys/stat.h>
#  include <fcntl.h>
#endif
#include <assert.h>

#include "assuan-defs.h"
#include "debug.h"


/* Log the formatted string FORMAT at debug category CAT higher.  */
void
_assuan_debug (assuan_context_t ctx, unsigned int cat, const char *format, ...)
{
  va_list arg_ptr;
  int saved_errno;
  char *msg;
  int res;

  if (!ctx || !ctx->log_cb)
    return;

  saved_errno = errno;
  va_start (arg_ptr, format);
  res = vasprintf (&msg, format, arg_ptr);
  va_end (arg_ptr);
  if (res < 0)
    return;
  ctx->log_cb (ctx, ctx->log_cb_data, cat, msg);
  free (msg);
  gpg_err_set_errno (saved_errno);
}


/* Start a new debug line in *LINE, logged at level LEVEL or higher,
   and starting with the formatted string FORMAT.  */
void
_assuan_debug_begin (assuan_context_t ctx,
		     void **line, unsigned int cat, const char *format, ...)
{
  va_list arg_ptr;
  int res;

  *line = NULL;
  /* Probe if this wants to be logged based on category.  */
  if (! ctx 
      || ! ctx->log_cb 
      || ! (*ctx->log_cb) (ctx, ctx->log_cb_data, cat, NULL))
    return;
  
  va_start (arg_ptr, format);
  res = vasprintf ((char **) line, format, arg_ptr);
  va_end (arg_ptr);
  if (res < 0)
    *line = NULL;
}


/* Add the formatted string FORMAT to the debug line *LINE.  */
void
_assuan_debug_add (assuan_context_t ctx, void **line, const char *format, ...)
{
  va_list arg_ptr;
  char *toadd;
  char *result;
  int res;

  if (!*line)
    return;

  va_start (arg_ptr, format);
  res = vasprintf (&toadd, format, arg_ptr);
  va_end (arg_ptr);
  if (res < 0)
    {
      free (*line);
      *line = NULL;
    }
  res = asprintf (&result, "%s%s", *(char **) line, toadd);
  free (toadd);
  free (*line);
  if (res < 0)
    *line = NULL;
  else
    *line = result;
}


/* Finish construction of *LINE and send it to the debug output
   stream.  */
void
_assuan_debug_end (assuan_context_t ctx, void **line, unsigned int cat)
{
  if (!*line)
    return;

  /* Force logging here by using category ~0.  */
  _assuan_debug (ctx, ~0, "%s", *line);
  free (*line);
  *line = NULL;
}


#define TOHEX(val) (((val) < 10) ? ((val) + '0') : ((val) - 10 + 'a'))

void
_assuan_debug_buffer (assuan_context_t ctx, unsigned int cat,
		      const char *const fmt, const char *const func,
		      const char *const tagname, void *tag,
		      const char *const buffer, size_t len)
{
  int idx = 0;
  int j;

  /* Probe if this wants to be logged based on category.  */
  if (!ctx 
      || ! ctx->log_cb 
      || ! (*ctx->log_cb) (ctx, ctx->log_cb_data, cat, NULL))
    return;

  while (idx < len)
    {
      char str[51];
      char *strp = str;
      char *strp2 = &str[34];
      
      for (j = 0; j < 16; j++)
	{
	  unsigned char val;
	  if (idx < len)
	    {
	      val = buffer[idx++];
	      *(strp++) = TOHEX (val >> 4);
	      *(strp++) = TOHEX (val % 16);
	      *(strp2++) = isprint (val) ? val : '.';
	    }
	  else
	    {
	      *(strp++) = ' ';
	      *(strp++) = ' ';
	    }
	  if (j == 7)
	    *(strp++) = ' ';
	}
      *(strp++) = ' ';
      *(strp2++) = '\n';
      *(strp2) = '\0';
      
      _assuan_debug (ctx, cat, fmt, func, tagname, tag, str);
    }
}
