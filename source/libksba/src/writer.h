/* writer.h - internl definitions for the writer object.
 *      Copyright (C) 2001, 2010 g10 Code GmbH
 *
 * This file is part of KSBA.
 *
 * KSBA is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * KSBA is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef WRITER_H
#define WRITER_H 1

#include <stdio.h>

enum writer_type {
  WRITER_TYPE_NONE = 0,
  WRITER_TYPE_FD,
  WRITER_TYPE_FILE,
  WRITER_TYPE_CB,
  WRITER_TYPE_MEM
};


struct ksba_writer_s {
  int error;
  unsigned long nwritten;
  enum writer_type type;
  int ndef_is_open;

  gpg_error_t (*filter)(void*,
                      const void *,size_t, size_t *,
                      void *, size_t, size_t *);
  void *filter_arg;

  union {
    int fd;  /* for WRITER_TYPE_FD */
    FILE *file; /* for WRITER_TYPE_FILE */
    struct {
      int (*fnc)(void*,const void *,size_t);
      void *value;
    } cb;   /* for WRITER_TYPE_CB */
    struct {
      unsigned char *buffer;
      size_t size;
    } mem;   /* for WRITER_TYPE_MEM */
  } u;
  void (*notify_cb)(void*,ksba_writer_t);
  void *notify_cb_value;
};




#endif /*WRITER_H*/








