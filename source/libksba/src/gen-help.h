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

/* This header has definitions used by programs which are only run on
   the build platform as part of the build process.  They need to be
   plain ISO C and don't make use of any information gathered from the
   configure run.  */

#ifndef GEN_HELP_H
#define GEN_HELP_H

#ifndef BUILD_GENTOOLS
#error file may only be used for build time tools
#endif


void *xmalloc (size_t n);
void *xcalloc (size_t n, size_t m);
void *xrealloc (void *mem, size_t n);
char *xstrdup (const char *str);
void xfree (void *a);
#define xtrymalloc(a) malloc ((a))

char *gen_help_stpcpy (char *a, const char *b);
#define stpcpy(a, b)  gen_help_stpcpy ((a), (b))




#define DIM(v) (sizeof(v)/sizeof((v)[0]))
#define DIMof(type,member)   DIM(((type *)0)->member)
#ifndef STR
# define STR(v) #v
#endif
#ifndef STR2
# define STR2(v) STR(v)
#endif

#define return_if_fail(expr) do {                          \
    if (!(expr)) {                                         \
      fprintf (stderr, "%s:%d: assertion `%s' failed\n",   \
               __FILE__, __LINE__, #expr );                \
      return;                                              \
    } } while (0)
#define return_null_if_fail(expr) do {                     \
    if (!(expr)) {                                         \
      fprintf (stderr, "%s:%d: assertion `%s' failed\n",   \
               __FILE__, __LINE__, #expr );                \
      return NULL;                                         \
    } } while (0)
#define return_val_if_fail(expr,val) do {                  \
    if (!(expr)) {                                         \
      fprintf (stderr, "%s:%d: assertion `%s' failed\n",   \
               __FILE__, __LINE__, #expr );                \
      return (val);                                        \
    } } while (0)
#define never_reached() do {                                     \
    fprintf (stderr, "%s:%d: oops; should never get here\n",     \
             __FILE__, __LINE__ );                               \
  } while (0)


/* Replacement for gpg_error.h stuff.  */
#define GPG_ERR_GENERAL                 1
#define GPG_ERR_SYNTAX                 29
#define GPG_ERR_INV_VALUE              55
#define GPG_ERR_BUG                    59
#define GPG_ERR_ELEMENT_NOT_FOUND     136
#define GPG_ERR_IDENTIFIER_NOT_FOUND  137

#define gpg_error(a)  (a)
#define gpg_error_from_syserror() (GPG_ERR_GENERAL);
const char *gpg_strerror (int err);

/* Duplicated type definitions from ksba.h.  */
typedef struct ksba_asn_tree_s *ksba_asn_tree_t;


#endif /*GEN_HELP_H*/

