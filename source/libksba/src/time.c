/* time.c - UTCTime and GeneralizedTime helper
 *      Copyright (C) 2001, 2003, 2005 g10 Code GmbH
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
#include <time.h>
#include <assert.h>

#include "util.h"
#include "convert.h"


/* Converts an UTCTime or GeneralizedTime to ISO format.  Sets the
   returns string to empty on error and returns the error code. The
   function figures automagically the right format.  fixme: Currently
   we only zupport Zulu time and no timezone which is sufficient for
   DER encoding.  It IS_UTCTIME is true, the function assumes that the
   time is in UTCTime and thus allows to parse UTCTimes without
   seconds (which is legal ASN.1; however Gutmann claims that the
   rules changed in 1996 to always require seconds; OTOH, Dubuisson's
   reference book from 2001 doesn't say so). */
gpg_error_t
_ksba_asntime_to_iso (const char *buffer, size_t length, int is_utctime,
                      ksba_isotime_t timebuf)
{ 
  const char *s;
  size_t n;
  int year;
  
  *timebuf = 0;
  for (s=buffer, n=0; n < length && digitp (s); n++, s++)
    ;
  if (is_utctime)
    {
      if ((n != 10 && n != 12) || *s != 'Z')
        return gpg_error (GPG_ERR_INV_TIME);
    }
  else if ((n != 12 && n != 14) || *s != 'Z')
    return gpg_error (GPG_ERR_INV_TIME);
  
  s = buffer;
  if (n==12 || n == 10 ) /* UTCTime with or without seconds. */
    {
      year = atoi_2 (s);
      timebuf[0] = year < 50? '2': '1';
      timebuf[1] = year < 50? '0': '9';
      memcpy (timebuf+2, s, 6);
      s += 6;
    }
  else
    {
      memcpy (timebuf, s, 8);
      s += 8;
    }
  timebuf[8] = 'T';
  if (n == 10) /* UTCTime w/0 seconds. */
    {
      memcpy (timebuf+9, s, 4);
      timebuf[13] = timebuf[14] = '0';
    }
  else
    {
      memcpy (timebuf+9, s, 6);
    }
  timebuf[15] = 0;

  return 0;
}


/* Return 0 if ATIME has the proper format (e.g. "19660205T131415"). */
gpg_error_t
_ksba_assert_time_format (const ksba_isotime_t atime)
{
  int i;
  const char *s;

  if (!*atime)
    return gpg_error (GPG_ERR_NO_VALUE);
  
  for (s=atime, i=0; i < 8; i++, s++)
    if (!digitp (s))
      return gpg_error (GPG_ERR_BUG);
  if (*s != 'T')
      return gpg_error (GPG_ERR_BUG);
  for (s++, i=9; i < 15; i++, s++)
    if (!digitp (s))
      return gpg_error (GPG_ERR_BUG);
  if (*s)
      return gpg_error (GPG_ERR_BUG);
  return 0;
}


/* Copy ISO time S to D.  This is a function so that we can detect
   faulty time formats. */
void
_ksba_copy_time (ksba_isotime_t d, const ksba_isotime_t s)
{
  if (!*s)
    memset (d, 0, 16);
  else if ( _ksba_assert_time_format (s) )
    {
      fprintf (stderr, "BUG: invalid isotime buffer\n");
      abort ();
    }
  else  
    strcpy (d, s);
}


/* Compare the time strings A and B. Return 0 if they show the very
   same time, return 1 if A is newer than B and -1 if A is older than
   B. */
int 
_ksba_cmp_time (const ksba_isotime_t a, const ksba_isotime_t b) 
{
  return strcmp (a, b);
}

/* Fill the TIMEBUF with the current time (UTC of course). */
void
_ksba_current_time (ksba_isotime_t timebuf)
{
  time_t epoch = time (NULL);
  struct tm *tp;
#ifdef HAVE_GMTIME_R
  struct tm tmbuf;
    
  tp = gmtime_r ( &epoch, &tmbuf);
#else
  tp = gmtime ( &epoch );
#endif
  sprintf (timebuf,"%04d%02d%02dT%02d%02d%02d",
           1900 + tp->tm_year, tp->tm_mon+1, tp->tm_mday,
           tp->tm_hour, tp->tm_min, tp->tm_sec);
}

