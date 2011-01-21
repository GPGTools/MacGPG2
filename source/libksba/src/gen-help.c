/* gen-help.c - Helper functions used by build time tools
 * Copyright (C) 2010 g10 Code GmbH
 *
 * This file is part of KSBA.
 *
 * KSBA is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Fountion; either version 3 of the License, or
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

/* No config.h - this file needs to build as plain ISO-C.  */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gen-help.h"


static void
out_of_core(void)
{
  fputs ("\nfatal: out of memory\n", stderr);
  exit (2);
}



/* Implementation of the common xfoo() memory allocation functions */
void *
xmalloc (size_t n )
{
  void *p = malloc (n);
  if (!p)
    out_of_core ();
  return p;
}

void *
xcalloc (size_t n, size_t m)
{
  void *p = calloc (n, m);
  if (!p)
    out_of_core ();
  return p;
}

void *
xrealloc (void *mem, size_t n)
{
  void *p = realloc (mem, n);
  if (!p)
    out_of_core ();
  return p;
}


char *
xstrdup (const char *str)
{
  char *p = strdup (str);
  if (!p)
    out_of_core ();
  return p;
}

void
xfree (void *a)
{
  if (a)
    free (a);
}


/* Our version of stpcpy to avoid conflicts with already availabale
   implementations.  */
char *
gen_help_stpcpy (char *a, const char *b)
{
  while (*b)
    *a++ = *b++;
  *a = 0;
  
  return a;
}


/* Simple replacement function to avoid the need for a build libgpg-error */
const char *
gpg_strerror (int err)
{
  switch (err)
    {
    case 0:                            return "Success";
    case GPG_ERR_GENERAL:              return "General error";
    case GPG_ERR_SYNTAX:               return "Syntax error";
    case GPG_ERR_INV_VALUE:            return "Invalid value";
    case GPG_ERR_BUG:                  return "Bug";
    case GPG_ERR_ELEMENT_NOT_FOUND:    return "Not found";
    case GPG_ERR_IDENTIFIER_NOT_FOUND: return "Identifier not found";
    default:                           return "Unknown error";
    }
}
