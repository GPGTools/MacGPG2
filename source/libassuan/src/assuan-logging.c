/* assuan-logging.c - Default logging function.
   Copyright (C) 2002, 2003, 2004, 2007, 2009,
                 2010 Free Software Foundation, Inc.

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
   License along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#ifdef HAVE_W32_SYSTEM
#include <windows.h>
#endif /*HAVE_W32_SYSTEM*/
#include <errno.h>
#include <ctype.h>

#include "assuan-defs.h"


/* The default log handler is useful for global logging, but it should
   only be used by one user of libassuan at a time.  Libraries that
   use libassuan can register their own log handler.  */

/* A common prefix for all log messages.  */
static char prefix_buffer[80];

/* A global flag read from the environment to check if to enable full
   logging of buffer data.  This is also used by custom log
   handlers.  */
static int full_logging;

/* A bitfield that specifies the categories to log.  */
static int log_cats;
#define TEST_LOG_CAT(x) (!! (log_cats & (1 << (x - 1))))

static FILE *_assuan_log;


void
_assuan_init_log_envvars (void)
{
  char *flagstr;

  full_logging = !!getenv ("ASSUAN_FULL_LOGGING");
  flagstr = getenv ("ASSUAN_DEBUG");
  if (flagstr)
    log_cats = atoi (flagstr);
  else /* Default to log the control channel.  */
    log_cats = (1 << (ASSUAN_LOG_CONTROL - 1));

  _assuan_sysutils_blurb (); /* Make sure this code gets linked in.  */
}


void
assuan_set_assuan_log_stream (FILE *fp)
{
  _assuan_log = fp;

  _assuan_init_log_envvars ();
}


/* Set the per context log stream.  Also enable the default log stream
   if it has not been set.  */
void
assuan_set_log_stream (assuan_context_t ctx, FILE *fp)
{
  if (ctx)
    {
      if (ctx->log_fp)
        fflush (ctx->log_fp);
      ctx->log_fp = fp;
      if (! _assuan_log)
	assuan_set_assuan_log_stream (fp);
    }
}


/* Set the prefix to be used for logging to TEXT or resets it to the
   default if TEXT is NULL. */
void
assuan_set_assuan_log_prefix (const char *text)
{
  if (text)
    {
      strncpy (prefix_buffer, text, sizeof (prefix_buffer)-1);
      prefix_buffer[sizeof (prefix_buffer)-1] = 0;
    }
  else
    *prefix_buffer = 0;
}


/* Get the prefix to be used for logging.  */
const char *
assuan_get_assuan_log_prefix (void)
{
  return prefix_buffer;
}


/* Default log handler.  */
int
_assuan_log_handler (assuan_context_t ctx, void *hook, unsigned int cat,
		     const char *msg)
{
  FILE *fp;
  const char *prf;
  int saved_errno = errno;

  /* For now.  */
  if (msg == NULL)
    return TEST_LOG_CAT (cat);

  if (! TEST_LOG_CAT (cat))
    return 0;

  fp = ctx->log_fp ? ctx->log_fp : _assuan_log;
  if (!fp)
    return 0;

  prf = assuan_get_assuan_log_prefix ();
  if (*prf)
    fprintf (fp, "%s[%u]: ", prf, (unsigned int)getpid ());

  fprintf (fp, "%s", msg);
  /* If the log stream is a file, the output would be buffered.  This
     is bad for debugging, thus we flush the stream if FORMAT ends
     with a LF.  */ 
  if (msg && *msg && msg[strlen (msg) - 1] == '\n')
    fflush (fp);
  gpg_err_set_errno (saved_errno);

  return 0;
}



/* Log a control channel message.  This is either a STRING with a
   diagnostic or actual data in (BUFFER1,LENGTH1) and
   (BUFFER2,LENGTH2).  If OUTBOUND is true the data is intended for
   the peer.  */
void
_assuan_log_control_channel (assuan_context_t ctx, int outbound,
                             const char *string,
                             const void *buffer1, size_t length1,
                             const void *buffer2, size_t length2)
{
  int res;
  char *outbuf;
  int saved_errno;

  /* Check whether logging is enabled and do a quick check to see
     whether the callback supports our category.  */
  if (!ctx || !ctx->log_cb 
      || !(*ctx->log_cb) (ctx, ctx->log_cb_data, ASSUAN_LOG_CONTROL, NULL))
    return;

  saved_errno = errno;

  /* Note that we use the inbound channel fd as the printed channel
     number for both directions.  */
#ifdef HAVE_W32_SYSTEM
# define CHANNEL_FMT "%p"
#else
# define CHANNEL_FMT "%d"
#endif
#define TOHEX(val) (((val) < 10) ? ((val) + '0') : ((val) - 10 + 'a'))

  if (!buffer1 && buffer2)
    {
      buffer1 = buffer2;
      length1 = length2;
      buffer2 = NULL;
      length2 = 0;
    }

  if (ctx->flags.confidential && !string && buffer1)
    string = "[Confidential data not shown]";

  if (string)
    {
      /* Print the diagnostic.  */
      res = asprintf (&outbuf, "chan_" CHANNEL_FMT " %s [%s]\n",
                      ctx->inbound.fd, outbound? "->":"<-", string);
    }
  else if (buffer1)
    {
      /* Print the control channel data.  */
      const unsigned char *s;
      unsigned int n, x;

      for (n = length1, s = buffer1; n; n--, s++)
        if ((!isascii (*s) || iscntrl (*s) || !isprint (*s) || !*s)
            && !(*s >= 0x80))
          break;
      if (!n && buffer2)
        {
          for (n = length2, s = buffer2; n; n--, s++)
            if ((!isascii (*s) || iscntrl (*s) || !isprint (*s) || !*s)
                && !(*s >= 0x80))
              break;
        }
      if (!buffer2)
        length2 = 0;

      if (!n && (length1 && *(const char*)buffer1 != '['))
        {
          /* No control characters and not starting with our error
             message indicator.  Log it verbatim.  */
          res = asprintf (&outbuf, "chan_" CHANNEL_FMT " %s %.*s%.*s\n",
                          ctx->inbound.fd, outbound? "->":"<-",
                          (int)length1, (const char*)buffer1,
                          (int)length2, buffer2? (const char*)buffer2:"");
        }
      else
        {
          /* The buffer contains control characters - do a hex dump.
             Even in full logging mode we limit the line length -
             however this is no real limit because the provided
             buffers will never be larger than the maximum assuan line
             length. */
          char *hp;
          unsigned int nbytes;
          unsigned int maxbytes = full_logging? (2*LINELENGTH) : 16;
            
          nbytes = length1 + length2;
          if (nbytes > maxbytes)
            nbytes = maxbytes;

          if (!(outbuf = malloc (50 + 3*nbytes + 60 + 3 + 1)))
            res = -1;
          else
            {
              res = 0;
              hp = outbuf;
              snprintf (hp, 50, "chan_" CHANNEL_FMT " %s [",
                        ctx->inbound.fd, outbound? "->":"<-");
              hp += strlen (hp);
              n = 0;
              for (s = buffer1, x = 0; x < length1 && n < nbytes; x++, n++)
                {
                  *hp++ = ' ';
                  *hp++ = TOHEX (*s >> 4);
                  *hp++ = TOHEX (*s & 0x0f);
                  s++;
                }
              for (s = buffer2, x = 0; x < length2 && n < nbytes; x++, n++)
                {
                  *hp++ = ' ';
                  *hp++ = TOHEX (*s >> 4);
                  *hp++ = TOHEX (*s & 0x0f);
                  s++;
                }
              if (nbytes < length1 + length2)
                {
                  snprintf (hp, 60, " ...(%u byte(s) skipped)",
                            (unsigned int)((length1+length2) - nbytes));
                  hp += strlen (hp);
                }
              strcpy (hp, " ]\n");
            }
        }
    }
  else
    {
      res = 0;
      outbuf = NULL;
    }
  if (res < 0)
    ctx->log_cb (ctx, ctx->log_cb_data, ASSUAN_LOG_CONTROL,
                 "[libassuan failed to format the log message]");
  else if (outbuf)
    {
      ctx->log_cb (ctx, ctx->log_cb_data, ASSUAN_LOG_CONTROL, outbuf);
      free (outbuf);
    }
#undef TOHEX
#undef CHANNEL_FMT
  gpg_err_set_errno (saved_errno);
}
