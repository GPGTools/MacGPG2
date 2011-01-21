/* t-crl-parser.c - basic test for the CRl parser.
 *      Copyright (C) 2002, 2004, 2005 g10 Code GmbH
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

#undef ENABLE_HASH_LOGGING

#ifdef ENABLE_HASH_LOGGING
#define _GNU_SOURCE 1
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <errno.h>

#include "../src/ksba.h"

#include "t-common.h"
#include "oidtranstbl.h"

static void
my_hasher (void *arg, const void *buffer, size_t length)
{
  FILE *fp = arg;

  if (fp)
    {
      if ( fwrite (buffer, length, 1, fp) != 1 )
        fail ("error writing to-be-hashed data");
    }
}


/* Return the description for OID; if no description is available 
   NULL is returned. */
static const char *
get_oid_desc (const char *oid)
{
  int i;

  if (oid)
    for (i=0; oidtranstbl[i].oid; i++)
      if (!strcmp (oidtranstbl[i].oid, oid))
        return oidtranstbl[i].desc;
  return NULL;
}


static void
print_names (int indent, ksba_name_t name)
{
  int idx;
  const char *s;
  int indent_all;

  if ((indent_all = (indent < 0)))
    indent = - indent;

  if (!name)
    {
      fputs ("none\n", stdout);
      return;
    }
  
  for (idx=0; (s = ksba_name_enum (name, idx)); idx++)
    {
      char *p = ksba_name_get_uri (name, idx);
      printf ("%*s%s\n", idx||indent_all?indent:0, "", p?p:s);
      xfree (p);
    }
}



static void
one_file (const char *fname)
{
  gpg_error_t err;
  FILE *fp;
  ksba_reader_t r;
  ksba_crl_t crl;
  ksba_stop_reason_t stopreason;
  int count = 0;
  FILE *hashlog = NULL;

#ifdef ENABLE_HASH_LOGGING
    {
      char *buf;

      if (asprintf (&buf, "%s.hash.log", fname) < 0)
        fail ("asprintf failed");
      hashlog = fopen (buf, "wb");
      if (!hashlog)
        fail ("can't create log file");
      free (buf);
    }
#endif

  printf ("*** checking `%s' ***\n", fname);
  fp = fopen (fname, "r");
  if (!fp)
    {
      fprintf (stderr, "%s:%d: can't open `%s': %s\n", 
               __FILE__, __LINE__, fname, strerror (errno));
      exit (1);
    }

  err = ksba_reader_new (&r);
  if (err)
    fail_if_err (err);
  err = ksba_reader_set_file (r, fp);
  fail_if_err (err);

  err = ksba_crl_new (&crl);
  if (err)
    fail_if_err (err);

  err = ksba_crl_set_reader (crl, r);
  fail_if_err (err);

  if (hashlog)
    ksba_crl_set_hash_function (crl, my_hasher, hashlog);

  do 
    {
      err = ksba_crl_parse (crl, &stopreason);
      fail_if_err2 (fname, err);
      switch (stopreason)
        {
        case KSBA_SR_BEGIN_ITEMS:
          {
            const char *algoid;
            char *issuer;
            ksba_isotime_t this, next;

            algoid = ksba_crl_get_digest_algo (crl);
            printf ("digest algo: %s\n", algoid? algoid : "[none]");

            err = ksba_crl_get_issuer (crl, &issuer);
            fail_if_err2 (fname, err);
            printf ("issuer: ");
            print_dn (issuer);
            xfree (issuer);
            putchar ('\n');
            err = ksba_crl_get_update_times (crl, this, next);
            if (gpg_err_code (err) != GPG_ERR_INV_TIME)
              fail_if_err2 (fname, err);
            printf ("thisUpdate: ");
            print_time (this);
            putchar ('\n');
            printf ("nextUpdate: ");
            print_time (next);
            putchar ('\n');
          }
          break;

        case KSBA_SR_GOT_ITEM:
          {
            ksba_sexp_t serial;
            ksba_isotime_t rdate;
            ksba_crl_reason_t reason;

            err = ksba_crl_get_item (crl, &serial, rdate, &reason);
            fail_if_err2 (fname, err);
            printf ("CRL entry %d: s=", ++count);
            print_sexp_hex (serial);
            printf (", t=");
            print_time (rdate);
            printf (", r=%x\n", reason);
            xfree (serial);
          }
          break;

        case KSBA_SR_END_ITEMS:
          break;

        case KSBA_SR_READY:
          break;

        default:
          fail ("unknown stop reason");
        }

    }
  while (stopreason != KSBA_SR_READY);

  if ( !ksba_crl_get_digest_algo (crl))
    fail ("digest algorithm mismatch");

  {
    ksba_name_t name1;
    ksba_sexp_t serial;
    ksba_sexp_t keyid;

    err = ksba_crl_get_auth_key_id (crl, &keyid, &name1, &serial);
    if (!err || gpg_err_code (err) == GPG_ERR_NO_DATA)
      {
        fputs ("AuthorityKeyIdentifier: ", stdout);
        if (gpg_err_code (err) == GPG_ERR_NO_DATA)
          fputs ("none\n", stdout);
        else
          {
            if (name1)
              {
                print_names (24, name1);
                ksba_name_release (name1);
                fputs ("                serial: ", stdout);
                print_sexp_hex (serial);
                ksba_free (serial);
              }
            putchar ('\n');
            if (keyid)
              {
                fputs ("         keyIdentifier: ", stdout);
                print_sexp (keyid);
                ksba_free (keyid);
                putchar ('\n');
              }
          }
      }
    else
      fail_if_err (err);
  }

  {
    ksba_sexp_t serial;

    err = ksba_crl_get_crl_number (crl, &serial);
    if (!err || gpg_err_code (err) == GPG_ERR_NO_DATA)
      {
        fputs ("crlNumber: ", stdout);
        if (gpg_err_code (err) == GPG_ERR_NO_DATA)
          fputs ("none", stdout);
        else
          {
            print_sexp (serial);
            ksba_free (serial);
          }
        putchar ('\n');
      }
    else
      fail_if_err (err);
  }


  {
    int idx, crit;
    const char *oid;
    size_t derlen;

    for (idx=0; !(err=ksba_crl_get_extension (crl, idx,
                                              &oid, &crit,
                                              NULL, &derlen)); idx++)
      {
        const char *s = get_oid_desc (oid);
        printf ("%sExtn: %s%s%s%s   (%lu octets)\n",
                crit? "Crit":"", 
                s?" (":"", s?s:"", s?")":"",
                oid, (unsigned long)derlen);
      }
    if (err && gpg_err_code (err) != GPG_ERR_EOF 
        && gpg_err_code (err) != GPG_ERR_NO_DATA )
      fail_if_err (err);
  }      
  

  {
    ksba_sexp_t sigval;

    sigval = ksba_crl_get_sig_val (crl);
    if (!sigval)
      fail ("signature value missing");
    print_sexp (sigval);
    putchar ('\n');
    xfree (sigval);
  }


  ksba_crl_release (crl);
  ksba_reader_release (r);
  fclose (fp);
  if (hashlog)
    fclose (hashlog);
}




int 
main (int argc, char **argv)
{
  const char *srcdir = getenv ("srcdir");
  
  if (!srcdir)
    srcdir = ".";

  if (argc > 1)
    {
      for (argc--, argv++; argc; argc--, argv++)
        one_file (*argv);
    }
  else
    {
      const char *files[] = {
        "crl_testpki_testpca.der",
        NULL 
      };
      int idx;
      
      for (idx=0; files[idx]; idx++)
        {
          char *fname;

          fname = xmalloc (strlen (srcdir) + 1 + strlen (files[idx]) + 1);
          strcpy (fname, srcdir);
          strcat (fname, "/");
          strcat (fname, files[idx]);
          one_file (fname);
          xfree (fname);
        }
    }

  return 0;
}

