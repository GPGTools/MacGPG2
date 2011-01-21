/* util.c
 *      Copyright (C) 2001, 2009 g10 Code GmbH
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

static void *(*alloc_func)(size_t n) = malloc;
static void *(*realloc_func)(void *p, size_t n) = realloc;
static void (*free_func)(void*) = free;
static gpg_error_t (*hash_buffer_fnc)(void *arg, const char *oid,
                                      const void *buffer, size_t length,
                                      size_t resultsize,
                                      unsigned char *result, size_t *resultlen);
static void *hash_buffer_fnc_arg;



/* Note, that we expect that the free fucntion does not change
   ERRNO. */
void
ksba_set_malloc_hooks ( void *(*new_alloc_func)(size_t n),
                        void *(*new_realloc_func)(void *p, size_t n),
                        void (*new_free_func)(void*) )
{
  alloc_func	    = new_alloc_func;
  realloc_func      = new_realloc_func;
  free_func	    = new_free_func;
}


/* Register a has function for general use by libksba.  This is
   required to avoid dependencies to specific low-level
   crypolibraries.  The function should be used right at the startup
   of the main program, similar to ksba_set_malloc_hooks. 

   The function provided should behave like this:

   gpg_error_t hash_buffer (void *arg, const char *oid,
                            const void *buffer, size_t length,
                            size_t resultsize,
                            unsigned char *result,
                            size_t *resultlen);

   Where ARG is the same pointer as set along with the fucntion, OID
   is an OID string telling the hash algorithm to be used - SHA-1
   shall be used if OID is NULL.  The text to hash is expected in
   BUFFER of LENGTH and the result will be placed into the provided
   buffer RESULT which has been allocated by the caller with at LEAST
   RESULTSIZE bytes; the actual length of the result is put into
   RESULTLEN. 

   The function shall return 0 on success or any other appropriate
   gpg-error.
*/ 
void
ksba_set_hash_buffer_function ( gpg_error_t (*fnc)
                                (void *arg, const char *oid,
                                 const void *buffer, size_t length,
                                 size_t resultsize,
                                 unsigned char *result,
                                 size_t *resultlen),
                                void *fnc_arg)
{
  hash_buffer_fnc = fnc;
  hash_buffer_fnc_arg = fnc_arg;
}

/* Hash BUFFER of LENGTH bytes using the algorithjm denoted by OID,
   where OID may be NULL to demand the use od SHA-1.  The resulting
   digest will be placed in the provided buffer RESULT which must have
   been allocated by the caller with at LEAST RESULTSIZE bytes; the
   actual length of the result is put into RESULTLEN.

   The function shall return 0 on success or any other appropriate
   gpg-error.
*/
gpg_error_t
_ksba_hash_buffer (const char *oid, const void *buffer, size_t length,
                   size_t resultsize, unsigned char *result, size_t *resultlen)
{
  if (!hash_buffer_fnc)
    return gpg_error (GPG_ERR_CONFIGURATION);
  return hash_buffer_fnc (hash_buffer_fnc_arg, oid, buffer, length,
                          resultsize, result, resultlen);
}


/* Wrapper for the common memory allocation functions.  These are here
   so that we can add hooks.  The corresponding macros should be used.
   These macros are not named xfoo() because this name is commonly
   used for function which die on errror.  We use macronames like
   xtryfoo() instead. */

void *
ksba_malloc (size_t n )
{
  return alloc_func (n);
}

void *
ksba_calloc (size_t n, size_t m )
{
  size_t nbytes;
  void *p;

  nbytes = n * m;
  if ( m && nbytes / m != n)
    {
      gpg_err_set_errno (ENOMEM);
      p = NULL;
    }
  else
    p = ksba_malloc (nbytes);
  if (p)
    memset (p, 0, nbytes);
  return p;
}

void *
ksba_realloc (void *mem, size_t n)
{
  return realloc_func (mem, n );
}


char *
ksba_strdup (const char *str)
{
  char *p = ksba_malloc (strlen(str)+1);
  if (p)
    strcpy (p, str);
  return p;
}


void 
ksba_free ( void *a )
{
  if (a)
    free_func (a);
}


static void
out_of_core(void)
{
  fputs ("\nfatal: out of memory\n", stderr );
  exit (2);
}


/* Implementations of the common xfoo() memory allocation functions */
void *
_ksba_xmalloc (size_t n )
{
  void *p = ksba_malloc (n);
  if (!p)
    out_of_core();
  return p;
}

void *
_ksba_xcalloc (size_t n, size_t m )
{
  void *p = ksba_calloc (n,m);
  if (!p)
    out_of_core();
  return p;
}

void *
_ksba_xrealloc (void *mem, size_t n)
{
  void *p = ksba_realloc (mem,n);
  if (!p)
    out_of_core();
  return p;
}


char *
_ksba_xstrdup (const char *str)
{
  char *p = ksba_strdup (str);
  if (!p)
    out_of_core();
  return p;
}


#ifndef HAVE_STPCPY
char *
_ksba_stpcpy (char *a,const char *b)
{
  while (*b)
    *a++ = *b++;
  *a = 0;

  return a;
}
#endif


static inline int 
ascii_toupper (int c)
{
  if (c >= 'a' && c <= 'z')
    c &= ~0x20;
  return c;
}


int
_ksba_ascii_memcasecmp (const void *a_arg, const void *b_arg, size_t n)
{
  const char *a = a_arg;
  const char *b = b_arg;

  if (a == b)
    return 0;
  for ( ; n; n--, a++, b++ )
    {
      if (*a != *b && ascii_toupper (*a) != ascii_toupper (*b))
        return *a == *b? 0 : (ascii_toupper (*a) - ascii_toupper (*b));
    }
  return 0;
}

