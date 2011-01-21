/* util.h 
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

#ifndef UTIL_H
#define UTIL_H

#ifdef BUILD_GENTOOLS
#error file may not be be used for build time tools
#endif


#include "visibility.h"


gpg_error_t _ksba_hash_buffer (const char *oid,
                               const void *buffer, size_t length,
                               size_t resultsize,
                               unsigned char *result, size_t *resultlen);


void *_ksba_xmalloc (size_t n );
void *_ksba_xcalloc (size_t n, size_t m );
void *_ksba_xrealloc (void *p, size_t n);
char *_ksba_xstrdup (const char *p);

#define xtrymalloc(a)    ksba_malloc((a))
#define xtrycalloc(a,b)  ksba_calloc((a),(b))
#define xtryrealloc(a,b) ksba_realloc((a),(b))
#define xtrystrdup(a)    ksba_strdup((a))
#define xfree(a)         ksba_free((a))

#define xmalloc(a)       _ksba_xmalloc((a))
#define xcalloc(a,b)     _ksba_xcalloc((a),(b))
#define xrealloc(a,b)    _ksba_xrealloc((a),(b))
#define xstrdup(a)       _ksba_xstrdup((a))


#define DIM(v) (sizeof(v)/sizeof((v)[0]))
#define DIMof(type,member)   DIM(((type *)0)->member)
#ifndef STR
# define STR(v) #v
#endif
#ifndef STR2
# define STR2(v) STR(v)
#endif

#define return_if_fail(expr) do {                        \
    if (!(expr)) {                                       \
        fprintf (stderr, "%s:%d: assertion `%s' failed\n", \
                 __FILE__, __LINE__, #expr );            \
        return;	                                         \
    } } while (0)
#define return_null_if_fail(expr) do {                   \
    if (!(expr)) {                                       \
        fprintf (stderr, "%s:%d: assertion `%s' failed\n", \
                 __FILE__, __LINE__, #expr );            \
        return NULL;	                                 \
    } } while (0)
#define return_val_if_fail(expr,val) do {                \
    if (!(expr)) {                                       \
        fprintf (stderr, "%s:%d: assertion `%s' failed\n", \
                 __FILE__, __LINE__, #expr );            \
        return (val);	                                 \
    } } while (0)
#define never_reached() do {                                   \
        fprintf (stderr, "%s:%d: oops; should never get here\n", \
                 __FILE__, __LINE__ );                         \
    } while (0)


#ifndef HAVE_STPCPY
char *_ksba_stpcpy (char *a, const char *b);
#define stpcpy(a,b) _ksba_stpcpy ((a), (b))
#endif

int _ksba_ascii_memcasecmp (const void *a_arg, const void *b_arg, size_t n);
#define ascii_memcasecmp(a,b,n) _ksba_ascii_memcasecmp ((a),(b),(n))

/* some macros to replace ctype ones and avoid locale problems */
#define spacep(p)   (*(p) == ' ' || *(p) == '\t')
#define digitp(p)   (*(p) >= '0' && *(p) <= '9')
#define hexdigitp(a) (digitp (a)                     \
                      || (*(a) >= 'A' && *(a) <= 'F')  \
                      || (*(a) >= 'a' && *(a) <= 'f'))
/* the atoi macros assume that the buffer has only valid digits */
#define atoi_1(p)   (*(p) - '0' )
#define atoi_2(p)   ((atoi_1(p) * 10) + atoi_1((p)+1))
#define atoi_4(p)   ((atoi_2(p) * 100) + atoi_2((p)+2))
#define xtoi_1(p)   (*(p) <= '9'? (*(p)- '0'): \
                     *(p) <= 'F'? (*(p)-'A'+10):(*(p)-'a'+10))
#define xtoi_2(p)   ((xtoi_1(p) * 16) + xtoi_1((p)+1))

#endif /* UTIL_H */

