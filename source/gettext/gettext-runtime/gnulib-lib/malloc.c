/* malloc() function that is glibc compatible.

   Copyright (C) 1997-1998, 2006-2007, 2009-2010 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

/* written by Jim Meyering and Bruno Haible */

#include <config.h>
/* Only the AC_FUNC_MALLOC macro defines 'malloc' already in config.h.  */
#ifdef malloc
# define NEED_MALLOC_GNU
# undef malloc
#endif

/* Specification.  */
#include <stdlib.h>

#include <errno.h>

/* Call the system's malloc below.  */
#undef malloc

/* Allocate an N-byte block of memory from the heap.
   If N is zero, allocate a 1-byte block.  */

void *
rpl_malloc (size_t n)
{
  void *result;

#ifdef NEED_MALLOC_GNU
  if (n == 0)
    n = 1;
#endif

  result = malloc (n);

#if !HAVE_MALLOC_POSIX
  if (result == NULL)
    errno = ENOMEM;
#endif

  return result;
}
