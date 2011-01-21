/* An interface to write() that retries after interrupts.
   Copyright (C) 2002, 2009-2010 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include <stddef.h>

#define SAFE_WRITE_ERROR ((size_t) -1)

/* Write up to COUNT bytes at BUF to descriptor FD, retrying if interrupted.
   Return the actual number of bytes written, zero for EOF, or SAFE_WRITE_ERROR
   upon error.  */
extern size_t safe_write (int fd, const void *buf, size_t count);
