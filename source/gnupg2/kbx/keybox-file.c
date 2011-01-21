/* keybox-file.c - File operations
 *	Copyright (C) 2001, 2003 Free Software Foundation, Inc.
 *
 * This file is part of GnuPG.
 *
 * GnuPG is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * GnuPG is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include "keybox-defs.h"


#if !defined(HAVE_FTELLO) && !defined(ftello)
static off_t
ftello (FILE *stream)
{
  long int off;

  off = ftell (stream);
  if (off == -1)
    return (off_t)-1;
  return off;
}
#endif /* !defined(HAVE_FTELLO) && !defined(ftello) */



/* Read a block at the current postion and return it in r_blob.
   r_blob may be NULL to simply skip the current block */
int
_keybox_read_blob2 (KEYBOXBLOB *r_blob, FILE *fp, int *skipped_deleted)
{
  unsigned char *image;
  size_t imagelen = 0;
  int c1, c2, c3, c4, type;
  int rc;
  off_t off;

  *skipped_deleted = 0;
 again:
  *r_blob = NULL;
  off = ftello (fp);
  if (off == (off_t)-1)
    return gpg_error_from_syserror ();

  if ((c1 = getc (fp)) == EOF
      || (c2 = getc (fp)) == EOF
      || (c3 = getc (fp)) == EOF
      || (c4 = getc (fp)) == EOF
      || (type = getc (fp)) == EOF)
    {
      if ( c1 == EOF && !ferror (fp) )
        return -1; /* eof */
      if (!ferror (fp))
        return gpg_error (GPG_ERR_TOO_SHORT);
      return gpg_error_from_syserror ();
    }

  imagelen = (c1 << 24) | (c2 << 16) | (c3 << 8 ) | c4;
  if (imagelen > 500000) /* Sanity check. */
    return gpg_error (GPG_ERR_TOO_LARGE);
  
  if (imagelen < 5) 
    return gpg_error (GPG_ERR_TOO_SHORT);

  if (!type)
    {
      /* Special treatment for empty blobs. */
      if (fseek (fp, imagelen-5, SEEK_CUR))
        return gpg_error_from_syserror ();
      *skipped_deleted = 1;
      goto again;
    }

  image = xtrymalloc (imagelen);
  if (!image) 
    return gpg_error_from_syserror ();

  image[0] = c1; image[1] = c2; image[2] = c3; image[3] = c4; image[4] = type;
  if (fread (image+5, imagelen-5, 1, fp) != 1)
    {
      gpg_error_t tmperr = gpg_error_from_syserror ();
      xfree (image);
      return tmperr;
    }
  
  rc = r_blob? _keybox_new_blob (r_blob, image, imagelen, off) : 0;
  if (rc || !r_blob)
    xfree (image);
  return rc;
}

int
_keybox_read_blob (KEYBOXBLOB *r_blob, FILE *fp)
{
  int dummy;
  return _keybox_read_blob2 (r_blob, fp, &dummy);
}


/* Write the block to the current file position */
int
_keybox_write_blob (KEYBOXBLOB blob, FILE *fp)
{
  const unsigned char *image;
  size_t length;

  image = _keybox_get_blob_image (blob, &length);
  if (fwrite (image, length, 1, fp) != 1)
    return gpg_error_from_syserror ();
  return 0;
}


/* Write a fresh header type blob. */
int
_keybox_write_header_blob (FILE *fp)
{
  unsigned char image[32];
  u32 val;

  memset (image, 0, sizeof image);
  /* Length of this blob. */
  image[3] = 32;

  image[4] = BLOBTYPE_HEADER;
  image[5] = 1; /* Version */
  
  memcpy (image+8, "KBXf", 4);
  val = time (NULL);
  /* created_at and last maintenance run. */
  image[16]   = (val >> 24);
  image[16+1] = (val >> 16);
  image[16+2] = (val >>  8);
  image[16+3] = (val      );
  image[20]   = (val >> 24);
  image[20+1] = (val >> 16);
  image[20+2] = (val >>  8);
  image[20+3] = (val      );

  if (fwrite (image, 32, 1, fp) != 1)
    return gpg_error_from_syserror ();
  return 0;
}


