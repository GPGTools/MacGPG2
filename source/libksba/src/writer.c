/* writer.c - provides the Writer object
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
#include "writer.h"
#include "asn1-func.h"
#include "ber-help.h"

/**
 * ksba_writer_new:
 * 
 * Create a new but uninitialized ksba_writer_t Object.  Using this
 * write object in unitialized state does always return an error.
 * 
 * Return value: ksba_writer_t object or an error code.
 **/
gpg_error_t
ksba_writer_new (ksba_writer_t *r_w)
{
  *r_w = xtrycalloc (1, sizeof **r_w);
  if (!*r_w)
    return gpg_error_from_errno (errno);

  return 0;
}


/**
 * ksba_writer_release:
 * @w: Writer Object (or NULL)
 * 
 * Release this object
 **/
void
ksba_writer_release (ksba_writer_t w)
{
  if (!w)
    return;
  if (w->notify_cb)
    {
      void (*notify_fnc)(void*,ksba_writer_t) = w->notify_cb;

      w->notify_cb = NULL;
      notify_fnc (w->notify_cb_value, w);
    }
  if (w->type == WRITER_TYPE_MEM)
    xfree (w->u.mem.buffer);
  xfree (w);
}


/* Set NOTIFY as function to be called by ksba_reader_release before
   resources are actually deallocated.  NOTIFY_VALUE is passed to the
   called function as its first argument.  Note that only the last
   registered function will be called; passing NULL for NOTIFY removes
   the notification.  */
gpg_error_t
ksba_writer_set_release_notify (ksba_writer_t w, 
                                void (*notify)(void*,ksba_writer_t),
                                void *notify_value)
{
  if (!w)
    return gpg_error (GPG_ERR_INV_VALUE);
  w->notify_cb = notify;
  w->notify_cb_value = notify_value;
  return 0;
}


int
ksba_writer_error (ksba_writer_t w)
{
  return w? gpg_error_from_errno (w->error) : gpg_error (GPG_ERR_INV_VALUE);
}

unsigned long
ksba_writer_tell (ksba_writer_t w)
{
  return w? w->nwritten : 0;
}


/**
 * ksba_writer_set_fd:
 * @w: Writer object
 * @fd: file descriptor
 * 
 * Initialize the Writer object with a file descriptor, so that write
 * operations on this object are excuted on this file descriptor.
 * 
 * Return value: 
 **/
gpg_error_t
ksba_writer_set_fd (ksba_writer_t w, int fd)
{
  if (!w || fd == -1)
    return gpg_error (GPG_ERR_INV_VALUE);
  if (w->type)
    return gpg_error (GPG_ERR_CONFLICT);

  w->error = 0;
  w->type = WRITER_TYPE_FD;
  w->u.fd = fd;

  return 0;
}

/**
 * ksba_writer_set_file:
 * @w: Writer object
 * @fp: file pointer
 * 
 * Initialize the Writer object with a stdio file pointer, so that write
 * operations on this object are excuted on this stream
 * 
 * Return value: 
 **/
gpg_error_t
ksba_writer_set_file (ksba_writer_t w, FILE *fp)
{
  if (!w || !fp)
    return gpg_error (GPG_ERR_INV_VALUE);
  if (w->type)
    return gpg_error (GPG_ERR_CONFLICT);

  w->error = 0;
  w->type = WRITER_TYPE_FILE;
  w->u.file = fp;
  return 0;
}



/**
 * ksba_writer_set_cb:
 * @w: Writer object
 * @cb: Callback function
 * @cb_value: Value passed to the callback function
 * 
 * Initialize the writer object with a callback function.
 * This callback function is defined as:
 * <literal>
 * typedef int (*cb) (void *cb_value, 
 *                    const void *buffer, size_t count);
 * </literal>
 *
 * The callback is expected to process all @count bytes from @buffer
 * @count should not be 0 and @buffer should not be %NULL
 * The callback should return 0 on success or an %KSBA_xxx error code.
 * 
 * Return value: 0 on success or an error code
 **/
gpg_error_t
ksba_writer_set_cb (ksba_writer_t w, 
                    int (*cb)(void*,const void *,size_t), void *cb_value )
{
  if (!w || !cb)
    return gpg_error (GPG_ERR_INV_VALUE);
  if (w->type)
    return gpg_error (GPG_ERR_CONFLICT);
  
  w->error = 0;
  w->type = WRITER_TYPE_CB;
  w->u.cb.fnc = cb;
  w->u.cb.value = cb_value;

  return 0;
}


gpg_error_t
ksba_writer_set_mem (ksba_writer_t w, size_t initial_size)
{
  if (!w)
    return gpg_error (GPG_ERR_INV_VALUE);
  if (w->type == WRITER_TYPE_MEM)
    ; /* Reuse the buffer (we ignore the initial size)*/
  else
    {
      if (w->type)
        return gpg_error (GPG_ERR_CONFLICT);

      if (!initial_size)
        initial_size = 1024;
      
      w->u.mem.buffer = xtrymalloc (initial_size);
      if (!w->u.mem.buffer)
        return gpg_error (GPG_ERR_ENOMEM);
      w->u.mem.size = initial_size;
      w->type = WRITER_TYPE_MEM;
    }
  w->error = 0;
  w->nwritten = 0;

  return 0;
}

/* Return the pointer to the memory and the size of it.  This pointer
   is valid as long as the writer object is valid and no write
   operations takes place (because they might reallocate the buffer).
   if NBYTES is not NULL, it will receive the number of bytes in this
   buffer which is the same value ksba_writer_tell() returns.
   
   In case of an error NULL is returned.
  */
const void *
ksba_writer_get_mem (ksba_writer_t w, size_t *nbytes)
{
  if (!w || w->type != WRITER_TYPE_MEM || w->error)
    return NULL;
  if (nbytes)
    *nbytes = w->nwritten;
  return w->u.mem.buffer;
}

/* Return the the memory and the size of it.  The writer object is set
   back into the uninitalized state; i.e. one of the
   ksab_writer_set_xxx () must be used before all other operations.
   if NBYTES is not NULL, it will receive the number of bytes in this
   buffer which is the same value ksba_writer_tell() returns.
   
   In case of an error NULL is returned.  */
void *
ksba_writer_snatch_mem (ksba_writer_t w, size_t *nbytes)
{
  void *p;

  if (!w || w->type != WRITER_TYPE_MEM || w->error)
    return NULL;
  if (nbytes)
    *nbytes = w->nwritten;
  p = w->u.mem.buffer;
  w->u.mem.buffer = NULL;
  w->type = 0;
  w->nwritten = 0;
  return p;
}



gpg_error_t
ksba_writer_set_filter (ksba_writer_t w, 
                        gpg_error_t (*filter)(void*,
                                            const void *,size_t, size_t *,
                                            void *, size_t, size_t *),
                        void *filter_arg)
{
  if (!w)
    return gpg_error (GPG_ERR_INV_VALUE);
  
  w->filter = filter;
  w->filter_arg = filter_arg;
  return 0;
}




static gpg_error_t
do_writer_write (ksba_writer_t w, const void *buffer, size_t length)
{
  if (!w->type)
    {
      w->error = EINVAL;
      return gpg_error_from_errno (w->error);
    }
  else if (w->type == WRITER_TYPE_MEM)
    {
      if (w->error == ENOMEM)
        return gpg_error (GPG_ERR_ENOMEM); /* it does not make sense to proceed then */

      if (w->nwritten + length > w->u.mem.size)
        {
          size_t newsize = w->nwritten + length;
          char *p;

          newsize = ((newsize + 4095)/4096)*4096;
          if (newsize < 16384)
            newsize += 4096;
          else
            newsize += 16384;
          
          p = xtryrealloc (w->u.mem.buffer, newsize);
          if (!p)
            {
              /* Keep an error flag so that the user does not need to
                 check the return code of a write but instead use
                 ksba_writer_error() to check for it or even figure
                 this state out when using ksba_writer_get_mem() */
              w->error = ENOMEM;
              return gpg_error (GPG_ERR_ENOMEM);
            }
          w->u.mem.buffer = p;
          w->u.mem.size = newsize;
          /* Better check again in case of an overwrap. */
          if (w->nwritten + length > w->u.mem.size)
            return gpg_error (GPG_ERR_ENOMEM); 
        }
      memcpy (w->u.mem.buffer + w->nwritten, buffer, length);
      w->nwritten += length;
    }
  else if (w->type == WRITER_TYPE_FILE)
    {
      if (!length)
        return 0;

      if ( fwrite (buffer, length, 1, w->u.file) == 1)
        {
          w->nwritten += length;
        }
      else
        {
          w->error = errno;
          return gpg_error_from_errno (errno);
        }
    }
  else if (w->type == WRITER_TYPE_CB)
    {
      int err = w->u.cb.fnc (w->u.cb.value, buffer, length);
      if (err)
        return err;
      w->nwritten += length;
    }
  else 
    return gpg_error (GPG_ERR_BUG);
  
  return 0;
} 

/**
 * ksba_writer_write:
 * @w: Writer object
 * @buffer: A buffer with the data to be written
 * @length: The length of this buffer
 * 
 * Write @length bytes from @buffer.
 *
 * Return value: 0 on success or an error code
 **/
gpg_error_t
ksba_writer_write (ksba_writer_t w, const void *buffer, size_t length)
{
  gpg_error_t err=0;

  if (!w)
    return gpg_error (GPG_ERR_INV_VALUE);

  if (!buffer)
      return gpg_error (GPG_ERR_NOT_IMPLEMENTED);

  if (w->filter)
    {
      char outbuf[4096];
      size_t nin, nout;
      const char *p = buffer;

      while (length)
        {
          err = w->filter (w->filter_arg, p, length, &nin,
                           outbuf, sizeof (outbuf), &nout);
          if (err)
            break;
          if (nin > length || nout > sizeof (outbuf))
            return gpg_error (GPG_ERR_BUG); /* tsss, someone else made an error */
          err = do_writer_write (w, outbuf, nout);
          if (err)
            break;
          length -= nin;
          p += nin;
        }
    }
  else
    {
      err = do_writer_write (w, buffer, length);
    }
  
  return err;
} 

/* Write LENGTH bytes of BUFFER to W while encoding it as an BER
   encoded octet string.  With FLUSH set to 1 the octet stream will be
   terminated.  If the entire octet string is available in BUFFER it
   is a good idea to set FLUSH to 1 so that the function does not need
   to encode the string partially. */
gpg_error_t
ksba_writer_write_octet_string (ksba_writer_t w,
                                const void *buffer, size_t length, int flush)
{
  gpg_error_t err = 0;

  if (!w)
    return gpg_error (GPG_ERR_INV_VALUE);

  if (buffer && length)
    {
      if (!w->ndef_is_open && !flush)
        {
          err = _ksba_ber_write_tl (w, TYPE_OCTET_STRING,
                                    CLASS_UNIVERSAL, 1, 0);
          if (err)
            return err;
          w->ndef_is_open = 1;
        }

      err = _ksba_ber_write_tl (w, TYPE_OCTET_STRING,
                                CLASS_UNIVERSAL, 0, length);
      if (!err)
        err = ksba_writer_write (w, buffer, length);
    }

  if (!err && flush && w->ndef_is_open) /* write an end tag */
      err = _ksba_ber_write_tl (w, 0, 0, 0, 0);

  if (flush) /* Reset it even in case of an error. */
    w->ndef_is_open = 1;

  return err;
}



