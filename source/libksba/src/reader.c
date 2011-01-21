/* reader.c - provides the Reader object
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

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include "util.h"

#include "ksba.h"
#include "reader.h"

/**
 * ksba_reader_new:
 * 
 * Create a new but uninitialized ksba_reader_t Object.  Using this
 * reader object in unitialized state does always yield eof.
 * 
 * Return value: ksba_reader_t object or an error code.
 **/
gpg_error_t
ksba_reader_new (ksba_reader_t *r_r)
{
  *r_r = xtrycalloc (1, sizeof **r_r);
  if (!*r_r)
    return gpg_error_from_errno (errno);
  return 0;
}


/**
 * ksba_reader_release:
 * @r: Reader Object (or NULL)
 * 
 * Release this object
 **/
void
ksba_reader_release (ksba_reader_t r)
{
  if (!r)
    return;
  if (r->notify_cb)
    {
      void (*notify_fnc)(void*,ksba_reader_t) = r->notify_cb;

      r->notify_cb = NULL;
      notify_fnc (r->notify_cb_value, r);
    }
  if (r->type == READER_TYPE_MEM)
    xfree (r->u.mem.buffer);
  xfree (r->unread.buf);
  xfree (r);
}


/* Set NOTIFY as function to be called by ksba_reader_release before
   resources are actually deallocated.  NOTIFY_VALUE is passed to the
   called function as its first argument.  Note that only the last
   registered function will be called; passing NULL for NOTIFY removes
   the notification.  */
gpg_error_t
ksba_reader_set_release_notify (ksba_reader_t r, 
                                void (*notify)(void*,ksba_reader_t),
                                void *notify_value)
{
  if (!r)
    return gpg_error (GPG_ERR_INV_VALUE);
  r->notify_cb = notify;
  r->notify_cb_value = notify_value;
  return 0;
}


/* Clear the error and eof indicators for READER, so that it can be
   continued to use.  Also dicards any unread bytes. This is usually
   required if the upper layer want to send to send an EOF to indicate
   the logical end of one part of a file.  If BUFFER and BUFLEN are
   not NULL, possible unread data is copied to a newly allocated
   buffer and this buffer is assigned to BUFFER, BUFLEN will be set to
   the length of the unread bytes. */
gpg_error_t
ksba_reader_clear (ksba_reader_t r, unsigned char **buffer, size_t *buflen)
{
  size_t n;

  if (!r)
    return gpg_error (GPG_ERR_INV_VALUE);
      
  r->eof = 0;
  r->error = 0;
  r->nread = 0;
  n = r->unread.length;
  r->unread.length = 0;

  if (buffer && buflen)
    {
      *buffer = NULL;
      *buflen = 0;
      if (n)
        {
          *buffer = xtrymalloc (n);
          if (!*buffer)
            return gpg_error_from_errno (errno);
          memcpy (*buffer, r->unread.buf, n);
          *buflen = n;
        }
    }
      
  return 0;
}


gpg_error_t
ksba_reader_error (ksba_reader_t r)
{
  return r? gpg_error_from_errno (r->error) : gpg_error (GPG_ERR_INV_VALUE);
}

unsigned long
ksba_reader_tell (ksba_reader_t r)
{
  return r? r->nread : 0;
}


/**
 * ksba_reader_set_mem:
 * @r: Reader object
 * @buffer: Data
 * @length: Length of Data (bytes)
 * 
 * Intialize the reader object with @length bytes from @buffer and set
 * the read position to the beginning.  It is possible to reuse this
 * reader object with another buffer if the reader object has
 * already been initialized using this function.
 * 
 * Return value: 0 on success or an error code.
 **/
gpg_error_t
ksba_reader_set_mem (ksba_reader_t r, const void *buffer, size_t length)
{
  if (!r || !buffer)
    return gpg_error (GPG_ERR_INV_VALUE);
  if (r->type == READER_TYPE_MEM)
    { /* Reuse this reader */
      xfree (r->u.mem.buffer);
      r->type = 0;
    }
  if (r->type)
    return gpg_error (GPG_ERR_CONFLICT);

  r->u.mem.buffer = xtrymalloc (length);
  if (!r->u.mem.buffer)
    return gpg_error (GPG_ERR_ENOMEM);
  memcpy (r->u.mem.buffer, buffer, length);
  r->u.mem.size = length;
  r->u.mem.readpos = 0;
  r->type = READER_TYPE_MEM;
  r->eof = 0;

  return 0;
}


/**
 * ksba_reader_set_fd:
 * @r: Reader object
 * @fd: file descriptor
 * 
 * Initialize the Reader object with a file descriptor, so that read
 * operations on this object are excuted on this file descriptor.
 * 
 * Return value: 
 **/
gpg_error_t
ksba_reader_set_fd (ksba_reader_t r, int fd)
{
  if (!r || fd == -1)
    return gpg_error (GPG_ERR_INV_VALUE);
  if (r->type)
    return gpg_error (GPG_ERR_CONFLICT);

  r->eof = 0;
  r->type = READER_TYPE_FD;
  r->u.fd = fd;

  return 0;
}

/**
 * ksba_reader_set_file:
 * @r: Reader object
 * @fp: file pointer
 * 
 * Initialize the Reader object with a stdio file pointer, so that read
 * operations on this object are excuted on this stream
 * 
 * Return value: 
 **/
gpg_error_t
ksba_reader_set_file (ksba_reader_t r, FILE *fp)
{
  if (!r || !fp)
    return gpg_error (GPG_ERR_INV_VALUE);
  if (r->type)
    return gpg_error (GPG_ERR_CONFLICT);

  r->eof = 0;
  r->type = READER_TYPE_FILE;
  r->u.file = fp;
  return 0;
}



/**
 * ksba_reader_set_cb:
 * @r: Reader object
 * @cb: Callback function
 * @cb_value: Value passed to the callback function
 * 
 * Initialize the reader object with a callback function.
 * This callback function is defined as:
 * <literal>
 * typedef int (*cb) (void *cb_value, 
 *                    char *buffer, size_t count,
 *                    size_t *nread);
 * </literal>
 *
 * The callback should return a maximium of @count bytes in @buffer
 * and the number actually read in @nread.  It may return 0 in @nread
 * if there are no bytes currently available.  To indicate EOF the
 * callback should return with an error code of GPG_ERR_EOF and set @nread to
 * 0.  The callback may support passing %NULL for @buffer and @nread
 * and %0 for count as an indication to reset its internal read
 * pointer.
 * 
 * Return value: 0 on success or an error code
 **/
gpg_error_t
ksba_reader_set_cb (ksba_reader_t r, 
                    int (*cb)(void*,char *,size_t,size_t*), void *cb_value )
{
  if (!r || !cb)
    return gpg_error (GPG_ERR_INV_VALUE);
  if (r->type)
    return gpg_error (GPG_ERR_CONFLICT);
  
  r->eof = 0;
  r->type = READER_TYPE_CB;
  r->u.cb.fnc = cb;
  r->u.cb.value = cb_value;

  return 0;
}


/**
 * ksba_reader_read:
 * @r: Readder object
 * @buffer: A buffer for returning the data
 * @length: The length of this buffer
 * @nread:  Number of bytes actually read.
 * 
 * Read data from the current read position to the supplied @buffer,
 * max. @length bytes are read and the actual number of bytes read are
 * returned in @nread.  If there are no more bytes available %GPG_ERR_EOF is
 * returned and @nread is set to 0.
 *
 * If a @buffer of NULL is specified, the function does only return
 * the number of bytes available and does not move the read pointer.
 * This does only work for objects initialized from memory; if the
 * object is not capable of this it will return the error
 * GPG_ERR_NOT_IMPLEMENTED
 * 
 * Return value: 0 on success, GPG_ERR_EOF or another error code
 **/
gpg_error_t
ksba_reader_read (ksba_reader_t r, char *buffer, size_t length, size_t *nread)
{
  size_t nbytes;

  if (!r || !nread)
    return gpg_error (GPG_ERR_INV_VALUE);


  if (!buffer)
    {
      if (r->type != READER_TYPE_MEM)
        return gpg_error (GPG_ERR_NOT_IMPLEMENTED);
      *nread = r->u.mem.size - r->u.mem.readpos;
      if (r->unread.buf)
        *nread += r->unread.length - r->unread.readpos;
      return *nread? 0 : gpg_error (GPG_ERR_EOF);
    }

  *nread = 0;

  if (r->unread.buf && r->unread.length)
    {
      nbytes = r->unread.length - r->unread.readpos;
      if (!nbytes)
        return gpg_error (GPG_ERR_BUG);
      
      if (nbytes > length)
        nbytes = length;
      memcpy (buffer, r->unread.buf + r->unread.readpos, nbytes);
      r->unread.readpos += nbytes;
      if (r->unread.readpos == r->unread.length)
        r->unread.readpos = r->unread.length = 0;
      *nread = nbytes;
      r->nread += nbytes;
      return 0;
    }


  if (!r->type)
    {
      r->eof = 1;
      return gpg_error (GPG_ERR_EOF);
    }
  else if (r->type == READER_TYPE_MEM)
    {
      nbytes = r->u.mem.size - r->u.mem.readpos;
      if (!nbytes)
        {
          r->eof = 1;
          return gpg_error (GPG_ERR_EOF);
        }
      
      if (nbytes > length)
        nbytes = length;
      memcpy (buffer, r->u.mem.buffer + r->u.mem.readpos, nbytes);
      *nread = nbytes;
      r->nread += nbytes;
      r->u.mem.readpos += nbytes;
    }
  else if (r->type == READER_TYPE_FILE)
    {
      int n;

      if (r->eof)
        return gpg_error (GPG_ERR_EOF);
      
      if (!length)
        {
          *nread = 0;
          return 0;
        }

      n = fread (buffer, 1, length, r->u.file);
      if (n > 0)
        {
          r->nread += n;
          *nread = n;
        }
      else
        *nread = 0;
      if (n < length)
        {
          if (ferror(r->u.file))
              r->error = errno;
          r->eof = 1;
          if (n <= 0)
            return gpg_error (GPG_ERR_EOF);
        }
    }
  else if (r->type == READER_TYPE_CB)
    {
      if (r->eof)
        return gpg_error (GPG_ERR_EOF);
      
      if (r->u.cb.fnc (r->u.cb.value, buffer, length, nread))
        {
          *nread = 0;
          r->eof = 1;
          return gpg_error (GPG_ERR_EOF);
        }
      r->nread += *nread;
    }
  else 
    return gpg_error (GPG_ERR_BUG);

  return 0;
} 

gpg_error_t
ksba_reader_unread (ksba_reader_t r, const void *buffer, size_t count)
{
  if (!r || !buffer)
    return gpg_error (GPG_ERR_INV_VALUE);
  if (!count)
    return 0;

  /* Make sure that we do not push more bytes back than we have read.
     Otherwise r->nread won't have a clear semantic. */
  if (r->nread < count)
    return gpg_error (GPG_ERR_CONFLICT);
  
  if (!r->unread.buf)
    {
      r->unread.size = count + 100;
      r->unread.buf = xtrymalloc (r->unread.size);
      if (!r->unread.buf)
        return gpg_error (GPG_ERR_ENOMEM);
      r->unread.length = count;
      r->unread.readpos = 0;
      memcpy (r->unread.buf, buffer, count);
      r->nread -= count;
    }
  else if (r->unread.length + count < r->unread.size)
    {
      memcpy (r->unread.buf+r->unread.length, buffer, count);
      r->unread.length += count;
      r->nread -= count;
    }
  else
    return gpg_error (GPG_ERR_NOT_IMPLEMENTED); /* fixme: easy to do */

  return 0;
}



