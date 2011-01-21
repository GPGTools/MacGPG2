/* t-common.h - Common functions for the tests.
 *      Copyright (C) 2002, 2003 g10 Code GmbH
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

/*-- sha1.c --*/
void sha1_hash_buffer (char *outbuf, const char *buffer, size_t length);



#define digitp(p)   (*(p) >= '0' && *(p) <= '9')

#define fail_if_err(a) do { if(a) {                                       \
                              fprintf (stderr, "%s:%d: KSBA error: %s\n", \
                              __FILE__, __LINE__, gpg_strerror(a));   \
                              exit (1); }                              \
                           } while(0)


#define fail_if_err2(f, a) do { if(a) {\
            fprintf (stderr, "%s:%d: KSBA error on file `%s': %s\n", \
                       __FILE__, __LINE__, (f), gpg_strerror(a));   \
                            exit (1); }                              \
                           } while(0)

#define fail(s)  do { fprintf (stderr, "%s:%d: %s\n", __FILE__,__LINE__, (s));\
                      exit (1); } while(0)

#define xfree(a)  ksba_free (a)


void *
xmalloc (size_t n)
{
  char *p = ksba_malloc (n);
  if (!p)
    {
      fprintf (stderr, "out of core\n");
      exit (1);
    }
  return p;
}


/* Prepend FNAME with the srcdir environment variable's value and
   retrun an allocated filename. */
char *
prepend_srcdir (const char *fname)
{
  static const char *srcdir;
  char *result;

  if (!srcdir)
    if(!(srcdir = getenv ("srcdir")))
      srcdir = ".";
  
  result = xmalloc (strlen (srcdir) + 1 + strlen (fname) + 1);
  strcpy (result, srcdir);
  strcat (result, "/");
  strcat (result, fname);
  return result;
}



void
print_hex (const unsigned char *p, size_t n)
{
  if (!p)
    fputs ("none", stdout);
  else
    {
      for (; n; n--, p++)
        printf ("%02X", *p);
    }
}


void
print_sexp (ksba_const_sexp_t p)
{
  int level = 0;

  if (!p)
    fputs ("[none]", stdout);
  else
    {
      for (;;)
        {
          if (*p == '(')
            {
              putchar (*p);
              p++;
              level++;
            }
          else if (*p == ')')
            {
              putchar (*p);
              if (--level <= 0 )
                return;
            }
          else if (!digitp (p))
            {
              fputs ("[invalid s-exp]", stdout);
              return;
            }
          else
            {
              char *endp;
              const unsigned char *s;
              unsigned long len, n;

              len = strtoul (p, &endp, 10);
              p = endp;
              if (*p != ':')
                {
                  fputs ("[invalid s-exp]", stdout);
                  return;
                }
              p++;
              for (s=p,n=0; n < len; n++, s++)
                if ( !((*s >= 'a' && *s <= 'z')
                       || (*s >= 'A' && *s <= 'Z')
                       || (*s >= '0' && *s <= '9')
                       || *s == '-' || *s == '.'))
                  break;
              if (n < len)
                {
                  putchar('#');
                  for (n=0; n < len; n++, p++)
                    printf ("%02X", *p);
                  putchar('#');
                }
              else
                {
                  for (n=0; n < len; n++, p++)
                    putchar (*p);
                }
            }
        }
    }
}

/* Variant of print_sexp which forces printing the values in hex.  */
void
print_sexp_hex (ksba_const_sexp_t p)
{
  int level = 0;

  if (!p)
    fputs ("[none]", stdout);
  else
    {
      for (;;)
        {
          if (*p == '(')
            {
              putchar (*p);
              p++;
              level++;
            }
          else if (*p == ')')
            {
              putchar (*p);
              if (--level <= 0 )
                return;
            }
          else if (!digitp (p))
            {
              fputs ("[invalid s-exp]", stdout);
              return;
            }
          else
            {
              char *endp;
              unsigned long len, n;

              len = strtoul (p, &endp, 10);
              p = endp;
              if (*p != ':')
                {
                  fputs ("[invalid s-exp]", stdout);
                  return;
                }
              p++;
              putchar('#');
              for (n=0; n < len; n++, p++)
                printf ("%02X", *p);
              putchar('#');
            }
        }
    }
}


void
print_dn (char *p)
{
  if (!p)
    fputs ("error", stdout);
  else
    printf ("`%s'", p);
}


void
print_time (ksba_isotime_t t)
{
  if (!t || !*t)
    fputs ("none", stdout);
  else
    printf ("%.4s-%.2s-%.2s %.2s:%.2s:%s", t, t+4, t+6, t+9, t+11, t+13);
}

