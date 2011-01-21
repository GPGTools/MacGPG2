/* mkheader.c - Create a header file for libassuan.
 * Copyright (C) 2010 Free Software Foundation, Inc.
 *
 * This file is free software; as a special exception the author gives
 * unlimited permission to copy and/or distribute it, with or without
 * modifications, as long as this notice is preserved.
 * 
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define PGM "mkheader"

#define LINESIZE 1024

static const char *host_os;
static char *srcdir;


/* Include the file NAME form the source directory.  The included file
   is not further expanded.  It may have comments indicated by a
   double hash mark at the begin of a line.  */
static void
include_file (const char *fname, int lnr, const char *name)
{
  FILE *fp;
  char *incfname;
  char line[LINESIZE];

  incfname = malloc (strlen (srcdir) + strlen (name) + 1);
  if (!incfname)
    {
      fputs (PGM ": out of core\n", stderr);
      exit (1);
    }
  strcpy (incfname, srcdir);
  strcat (incfname, name);

  fp = fopen (incfname, "r");
  if (!fp)
    {
      fprintf (stderr, "%s:%d: error including `%s': %s\n", 
               fname, lnr, incfname, strerror (errno));
      exit (1);
    }

  while (fgets (line, LINESIZE, fp))
    {
      if (line[0] == '#' && line[1] == '#')
        {
          if (!strncmp (line+2, "EOF##", 5))
            break; /* Forced EOF.  */
        }
      else
        fputs (line, stdout);
    }
  if (ferror (fp))
    {
      fprintf (stderr, "%s:%d: error reading `%s': %s\n", 
               fname, lnr, incfname, strerror (errno));
      exit (1);
    }
  fclose (fp);
  free (incfname);
}


static int
write_special (const char *fname, int lnr, const char *tag)
{
  if (!strcmp (tag, "include:includes"))
    {
      if (strstr (host_os, "mingw32"))
        include_file (fname, lnr, "w32-includes.inc.h");
      else
        include_file (fname, lnr, "posix-includes.inc.h");
    }
  else if (!strcmp (tag, "include:types"))
    {
      if (strstr (host_os, "mingw32"))
        include_file (fname, lnr, "w32-types.inc.h");
      else
        include_file (fname, lnr, "posix-types.inc.h");
    }
  else if (!strcmp (tag, "include:fd-t"))
    {
      if (!strcmp (host_os, "mingw32ce"))
        include_file (fname, lnr, "w32ce-fd-t.inc.h");
      else if (strstr (host_os, "mingw32"))
        include_file (fname, lnr, "w32-fd-t.inc.h");
      else
        include_file (fname, lnr, "posix-fd-t.inc.h");
    }
  else if (!strcmp (tag, "include:sock-nonce"))
    {
      if (strstr (host_os, "mingw32"))
        include_file (fname, lnr, "w32-sock-nonce.inc.h");
      else
        include_file (fname, lnr, "posix-sock-nonce.inc.h");
    }
  else if (!strcmp (tag, "include:sys-pth-impl"))
    {
      if (strstr (host_os, "mingw32"))
        include_file (fname, lnr, "w32-sys-pth-impl.h");
      else
        include_file (fname, lnr, "posix-sys-pth-impl.h");
    }
  else if (!strcmp (tag, "include:w32ce-add"))
    {
      if (!strcmp (host_os, "mingw32ce"))
        include_file (fname, lnr, "w32ce-add.h");
    }
  else
    return 0; /* Unknown tag.  */

  return 1; /* Tag processed.  */
}


int 
main (int argc, char **argv)
{
  FILE *fp;
  char line[LINESIZE];
  int lnr = 0;
  const char *fname, *s;
  char *p1, *p2;

  if (argc)
    {
      argc--; argv++;
    }
 
  if (argc != 2)
    {
      fputs ("usage: " PGM " host_os template.h\n", stderr);
      return 1;
    }
  host_os = argv[0];
  fname = argv[1];

  srcdir = malloc (strlen (fname) + 2 + 1);
  if (!srcdir)
    {
      fputs (PGM ": out of core\n", stderr);
      return 1;
    }
  strcpy (srcdir, fname);
  p1 = strrchr (srcdir, '/');
  if (p1)
    p1[1] = 0;
  else
    strcpy (srcdir, "./");

  fp = fopen (fname, "r");
  if (!fp)
    {
      fprintf (stderr, "%s:%d: can't open file: %s",
               fname, lnr, strerror (errno));
      return 1;
    }
  
  while (fgets (line, LINESIZE, fp))
    {
      size_t n = strlen (line);

      lnr++;
      if (!n || line[n-1] != '\n')
        {
          fprintf (stderr,
                   "%s:%d: trailing linefeed missing, line too long or "
                   "embedded Nul character", fname, lnr);
          break;
        }
      line[--n] = 0;
      
      p1 = strchr (line, '@');
      p2 = p1? strchr (p1+1, '@') : NULL;
      if (!p1 || !p2 || p2-p1 == 1)
        {
          puts (line);
          continue;
        }
      *p1++ = 0;
      *p2++ = 0;
      fputs (line, stdout);

      if (!strcmp (p1, "configure_input"))
        {
          s = strrchr (fname, '/');
          printf ("Do not edit.  Generated from %s by %s for %s.", 
                  s? s+1 : fname, PGM, host_os);
          fputs (p2, stdout);
          putchar ('\n');
        }
      else if (!write_special (fname, lnr, p1))
        {
          putchar ('@');
          fputs (p1, stdout);
          putchar ('@');
          fputs (p2, stdout);
          putchar ('\n');
        }
    }

  if (ferror (fp))
    {
      fprintf (stderr, "%s:%d: error reading file: %s\n", 
               fname, lnr, strerror (errno));
      return 1;
    }

  fputs ("/*\n"
         "Loc" "al Variables:\n"
         "buffer-read-only: t\n"
         "End:\n"
         "*/\n", stdout);

  if (ferror (stdout))
    {
      fprintf (stderr, PGM ": error writing stdout: %s\n", strerror (errno));
      return 1;
    }
  
  fclose (fp);

  return 0;
}
