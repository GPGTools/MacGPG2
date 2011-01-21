/* ber-dump.c - Tool to dump BER encoded data
 *      Copyright (C) 2001 g10 Code GmbH
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
#include <errno.h>
#include <stdarg.h>

#include "visibility.h"
#include "ksba.h"
#include "ber-decoder.h"

#define PGMNAME "ber-dump"

#if (__GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 5 ))
# define  ATTR_PRINTF(a,b)  __attribute__ ((format (printf,a,b)))
#else
# define  ATTR_PRINTF(a,b) 
#endif

/* keep track of parsing error */
static int error_counter;


static void print_error (const char *fmt, ... )  ATTR_PRINTF(1,2);



static void
print_error (const char *fmt, ... )  
{
  va_list arg_ptr ;

  va_start (arg_ptr, fmt);
  fputs (PGMNAME ": ", stderr);
  vfprintf (stderr, fmt, arg_ptr);
  va_end (arg_ptr);
  error_counter++;
}

static void
fatal (const char *fmt, ... )  
{
  va_list arg_ptr ;

  va_start (arg_ptr, fmt);
  fputs (PGMNAME ": fatal: ", stderr);
  vfprintf (stderr, fmt, arg_ptr);
  va_end (arg_ptr);
  exit (1);
}


static void
one_file (FILE *fp, const char *fname, ksba_asn_tree_t asn_tree)
{
  gpg_error_t err;
  ksba_reader_t r;
  BerDecoder d;

  (void)fname;  /* Not yet used in error messages.  */

  err = ksba_reader_new (&r);
  if (err)
    fatal ("out of core\n");
  err = ksba_reader_set_file (r, fp);
  if (err)
    fatal ("ksba_reader_set_file failed: rc=%d\n", err);

  d = _ksba_ber_decoder_new ();
  if (!d)
    fatal ("out of core\n");
  err = _ksba_ber_decoder_set_reader (d, r);
  if (err)
    fatal ("ksba_ber_decoder_set_reader failed: rc=%d\n", err);
  
  if (asn_tree)
    {
      err = _ksba_ber_decoder_set_module (d, asn_tree);
      if (err)
        fatal ("ksba_ber_decoder_set_module failed: rc=%d\n", err);
    }

  err = _ksba_ber_decoder_dump (d, stdout);
  if (err)
    print_error ("ksba_ber_decoder_dump failed: rc=%d\n", err);
  
  _ksba_ber_decoder_release (d);
  ksba_reader_release (r);
}


static void
usage (int exitcode)
{
  fputs ("usage: ber-dump [--module asnfile] [files]\n", stderr);
  exit (exitcode);
}

int
main (int argc, char **argv)
{
  const char *asnfile = NULL;
  ksba_asn_tree_t asn_tree = NULL;
  int rc;

  if (!argc || (argc > 1 &&
                (!strcmp (argv[1],"--help") || !strcmp (argv[1],"-h"))) )
    usage (0);
  
  argc--; argv++;
  if (argc && !strcmp (*argv,"--module"))
    {
      argc--; argv++;
      if (!argc)
        usage (1);
      asnfile = *argv;
      argc--; argv++;
    }

  if (asnfile)
    {
      rc = ksba_asn_parse_file (asnfile, &asn_tree, 0);
      if (rc)
        {
          print_error ("parsing `%s' failed: rc=%d\n", asnfile, rc);
          exit (1);
        }
    }

  
  if (!argc)
    one_file (stdin, "-", asn_tree);
  else
    {
      for (; argc; argc--, argv++) 
        {
          FILE *fp;
          
          fp = fopen (*argv, "r");
          if (!fp)
              print_error ("can't open `%s': %s\n", *argv, strerror (errno));
          else
            {
              one_file (fp, *argv, asn_tree);
              fclose (fp);
            }
        }
    }
  
  ksba_asn_tree_release (asn_tree);

  return error_counter? 1:0;
}

