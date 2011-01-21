/* version.c - Version checking
 *      Copyright (C) 2001, 2002 g10 Code GmbH
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

#include "util.h"

static const char*
parse_version_number (const char *s, int *number)
{
  int val = 0;

  if (*s == '0' && digitp (s+1))
    return NULL;  /* Leading zeros are not allowed.  */
  for (; digitp (s); s++)
    {
      val *= 10;
      val += *s - '0';
    }
  *number = val;
  return val < 0 ? NULL : s;
}

static const char *
parse_version_string (const char *s, int *major, int *minor, int *micro)
{
  s = parse_version_number (s, major);
  if (!s || *s != '.')
    return NULL;
  s++;
  s = parse_version_number (s, minor);
  if (!s || *s != '.')
    return NULL;
  s++;
  s = parse_version_number (s, micro);
  if (!s)
    return NULL;
  return s;  /* Patchlevel.  */
}

static const char *
compare_versions (const char *my_version, const char *req_version)
{
  int my_major, my_minor, my_micro;
  int rq_major, rq_minor, rq_micro;
  const char *my_plvl, *rq_plvl;

  if (!req_version)
    return my_version;
  if (!my_version)
    return NULL;

  my_plvl = parse_version_string (my_version, &my_major, &my_minor, &my_micro);
  if (!my_plvl)
    return NULL;	/* Very strange: our own version is bogus.  */
  rq_plvl = parse_version_string(req_version,
				 &rq_major, &rq_minor, &rq_micro);
  if (!rq_plvl)
    return NULL;	/* Requested version string is invalid.  */

  if (my_major > rq_major
	|| (my_major == rq_major && my_minor > rq_minor)
      || (my_major == rq_major && my_minor == rq_minor 
	  && my_micro > rq_micro)
      || (my_major == rq_major && my_minor == rq_minor
	  && my_micro == rq_micro
	  && strcmp( my_plvl, rq_plvl ) >= 0))
    {
      return my_version;
    }
  return NULL;
}

/**
 * ksba_check_version:
 * @req_version: A string with a version
 * 
 * Check that the the version of the library is at minimum the requested one
 * and return the version string; return NULL if the condition is not
 * met.  If a NULL is passed to this function, no check is done and
 * the version string is simply returned.  It is a pretty good idea to
 * run this function as soon as possible, because it also intializes 
 * some subsystems.  In a multithreaded environment if should be called
 * before the first thread is created.
 * 
 * Return value: The version string or NULL
 **/
const char *
ksba_check_version (const char *req_version)
{
  /* fixme: if we need global initializations.
     Note that these the malloc hook might not have been run yet */
  return compare_versions (VERSION, req_version);
}
