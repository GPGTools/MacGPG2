/* funopen.c - Replacement for funopen.
 * Copyright (C) 2003, 2005 Free Software Foundation, Inc.
 *
 * This file is part of Assuan.
 *
 * Assuan is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>


/* Replacement for the *BSD function:

  FILE *funopen (void *cookie,
                 int (*readfn)(void *, char *, int),
                 int (*writefn)(void *, const char *, int),
                 fpos_t (*seekfn)(void *, fpos_t, int),
                 int (*closefn)(void *));

  The functions to provide my either be NULL if not required or
  similar to the unistd function with the exception of using the
  cookie instead of the fiel descripor.
*/


#ifdef HAVE_FOPENCOOKIE
FILE *
_assuan_funopen(void *cookie,
                cookie_read_function_t *readfn,
                cookie_write_function_t *writefn,
                cookie_seek_function_t *seekfn,
                cookie_close_function_t *closefn)
{
  cookie_io_functions_t io;

  io.read = readfn;
  io.write = writefn;
  io.seek = seekfn;
  io.close = closefn;

  return fopencookie (cookie,
		      readfn ? ( writefn ? "rw" : "r" )
		      : ( writefn ? "w" : ""), io);
}
#else
#error No known way to implement funopen.
#endif
