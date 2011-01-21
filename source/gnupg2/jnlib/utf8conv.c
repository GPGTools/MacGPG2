/* utf8conf.c -  UTF8 character set conversion
 * Copyright (C) 1994, 1998, 1999, 2000, 2001,
 *               2003, 2006, 2008  Free Software Foundation, Inc.
 *
 * This file is part of JNLIB.
 *
 * JNLIB is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of
 * the License, or (at your option) any later version.
 *
 * JNLIB is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#ifdef HAVE_LANGINFO_CODESET
#include <langinfo.h>
#endif
#include <errno.h>
#ifndef HAVE_W32_SYSTEM
# include <iconv.h>
#endif

#include "libjnlib-config.h"
#include "stringhelp.h"
#include "dynload.h"
#include "utf8conv.h"

#ifndef MB_LEN_MAX
#define MB_LEN_MAX 16
#endif

static const char *active_charset_name = "iso-8859-1";
static int no_translation;     /* Set to true if we let simply pass through. */
static int use_iconv;          /* iconv comversion fucntions required. */


/* Under W32 we dlopen the iconv dll and don't require any iconv
   related headers at all.  However we need to define some stuff.  */
#ifdef HAVE_W32_SYSTEM
typedef void *iconv_t;
#ifndef ICONV_CONST
#define ICONV_CONST const 
#endif
static iconv_t (* __stdcall iconv_open) (const char *tocode,
                                         const char *fromcode);
static size_t  (* __stdcall iconv) (iconv_t cd,
                                    const char **inbuf, size_t *inbytesleft,
                                    char **outbuf, size_t *outbytesleft);
static int     (* __stdcall iconv_close) (iconv_t cd);

static int 
load_libiconv (void)
{
  static int done;
  
  if (!done)
    {
      void *handle;

      done = 1; /* Do it right now because we might get called recursivly
                   through gettext.  */
    
      handle = dlopen ("iconv.dll", RTLD_LAZY);
      if (handle)
        {
          iconv_open  = dlsym (handle, "libiconv_open");
          if (iconv_open)
            iconv = dlsym (handle, "libiconv");
          if (iconv)    
            iconv_close = dlsym (handle, "libiconv_close");
        }
      if (!handle || !iconv_close)
        {
          log_info (_("error loading `%s': %s\n"),
                     "iconv.dll",  dlerror ());
          log_info (_("please see %s for more information\n"),
                    "http://www.gnupg.org/download/iconv.html");
          iconv_open = NULL;
          iconv = NULL;
          iconv_close = NULL;
          if (handle)
            dlclose (handle);
        }
    }
  return iconv_open? 0: -1;
}    
#endif /*HAVE_W32_SYSTEM*/


/* Error handler for iconv failures. This is needed to not clutter the
   output with repeated diagnostics about a missing conversion. */
static void
handle_iconv_error (const char *to, const char *from, int use_fallback)
{
  if (errno == EINVAL)
    {
      static int shown1, shown2;
      int x;

      if (to && !strcmp (to, "utf-8"))
        {
          x = shown1;
          shown1 = 1;
        }
      else
        {
          x = shown2;
          shown2 = 1;
        }

      if (!x)
        log_info (_("conversion from `%s' to `%s' not available\n"),
                  from, to);
    }
  else
    {
      static int shown;

      if (!shown)
        log_info (_("iconv_open failed: %s\n"), strerror (errno));
      shown = 1;
    }

  if (use_fallback)
    {
      /* To avoid further error messages we fallback to Latin-1 for the
         native encoding.  This is justified as one can expect that on a
         utf-8 enabled system nl_langinfo() will work and thus we won't
         never get to here.  Thus Latin-1 seems to be a reasonable
         default.  */
      active_charset_name = "iso-8859-1";
      no_translation = 0;
      use_iconv = 0;
    }
}



int
set_native_charset (const char *newset)
{
  const char *full_newset;

  if (!newset) 
    {
#ifdef HAVE_W32_SYSTEM
      static char codepage[30];
      unsigned int cpno;
      const char *aliases;
      
      /* We are a console program thus we need to use the
         GetConsoleOutputCP function and not the the GetACP which
         would give the codepage for a GUI program.  Note this is not
         a bulletproof detection because GetConsoleCP might return a
         different one for console input.  Not sure how to cope with
         that.  If the console Code page is not known we fall back to
         the system code page.  */
      cpno = GetConsoleOutputCP ();
      if (!cpno)
        cpno = GetACP ();
      sprintf (codepage, "CP%u", cpno );
      /* Resolve alias.  We use a long string string and not the usual
         array to optimize if the code is taken to a DSO.  Taken from
         libiconv 1.9.2. */
      newset = codepage;
      for (aliases = ("CP936"   "\0" "GBK" "\0"
                      "CP1361"  "\0" "JOHAB" "\0"
                      "CP20127" "\0" "ASCII" "\0"
                      "CP20866" "\0" "KOI8-R" "\0"
                      "CP21866" "\0" "KOI8-RU" "\0"
                      "CP28591" "\0" "ISO-8859-1" "\0"
                      "CP28592" "\0" "ISO-8859-2" "\0"
                      "CP28593" "\0" "ISO-8859-3" "\0"
                      "CP28594" "\0" "ISO-8859-4" "\0"
                      "CP28595" "\0" "ISO-8859-5" "\0"
                      "CP28596" "\0" "ISO-8859-6" "\0"
                      "CP28597" "\0" "ISO-8859-7" "\0"
                      "CP28598" "\0" "ISO-8859-8" "\0"
                      "CP28599" "\0" "ISO-8859-9" "\0"
                      "CP28605" "\0" "ISO-8859-15" "\0"
                      "CP65001" "\0" "UTF-8" "\0");
           *aliases;
           aliases += strlen (aliases) + 1, aliases += strlen (aliases) + 1)
        {
          if (!strcmp (codepage, aliases) ||(*aliases == '*' && !aliases[1]))
            {
              newset = aliases + strlen (aliases) + 1;
              break;
            }
        }

#else /*!HAVE_W32_SYSTEM*/
      
#ifdef HAVE_LANGINFO_CODESET
      newset = nl_langinfo (CODESET);
#else /*!HAVE_LANGINFO_CODESET*/
      /* Try to get the used charset from environment variables.  */
      static char codepage[30];
      const char *lc, *dot, *mod;

      strcpy (codepage, "iso-8859-1");
      lc = getenv ("LC_ALL");
      if (!lc || !*lc)
        {
          lc = getenv ("LC_CTYPE");
          if (!lc || !*lc)
            lc = getenv ("LANG");
        }
      if (lc && *lc)
        {
          dot = strchr (lc, '.');
          if (dot)
            {
              mod = strchr (++dot, '@');
              if (!mod)
                mod = dot + strlen (dot);
              if (mod - dot < sizeof codepage && dot != mod) 
                {
                  memcpy (codepage, dot, mod - dot);
                  codepage [mod - dot] = 0;
                }
            }
        }
      newset = codepage;
#endif /*!HAVE_LANGINFO_CODESET*/
#endif /*!HAVE_W32_SYSTEM*/
    }

  full_newset = newset;
  if (strlen (newset) > 3 && !ascii_memcasecmp (newset, "iso", 3))
    {
      newset += 3;
      if (*newset == '-' || *newset == '_')
        newset++;
    }

  /* Note that we silently assume that plain ASCII is actually meant
     as Latin-1.  This makes sense because many Unix system don't have
     their locale set up properly and thus would get annoying error
     messages and we have to handle all the "bug" reports. Latin-1 has
     always been the character set used for 8 bit characters on Unix
     systems. */
  if ( !*newset
       || !ascii_strcasecmp (newset, "8859-1" )
       || !ascii_strcasecmp (newset, "646" )
       || !ascii_strcasecmp (newset, "ASCII" )
       || !ascii_strcasecmp (newset, "ANSI_X3.4-1968" )
       )
    {
      active_charset_name = "iso-8859-1";
      no_translation = 0;
      use_iconv = 0;
    }
  else if ( !ascii_strcasecmp (newset, "utf8" )
            || !ascii_strcasecmp(newset, "utf-8") )
    {
      active_charset_name = "utf-8";
      no_translation = 1;
      use_iconv = 0;
    }
  else
    {
      iconv_t cd;
      
#ifdef HAVE_W32_SYSTEM
      if (load_libiconv ())
        return -1;
#endif /*HAVE_W32_SYSTEM*/      

      cd = iconv_open (full_newset, "utf-8");
      if (cd == (iconv_t)-1) 
        {
          handle_iconv_error (full_newset, "utf-8", 0);
          return -1;
        }
      iconv_close (cd);
      cd = iconv_open ("utf-8", full_newset);
      if (cd == (iconv_t)-1) 
        {
          handle_iconv_error ("utf-8", full_newset, 0);
          return -1;
        }
      iconv_close (cd);
      active_charset_name = full_newset;
      no_translation = 0;
      use_iconv = 1;
    }
  return 0;
}

const char *
get_native_charset ()
{
  return active_charset_name;
}

/* Return true if the native charset is utf-8.  */
int 
is_native_utf8 (void)
{
  return no_translation;
}


/* Convert string, which is in native encoding to UTF8 and return a
   new allocated UTF-8 string.  This function terminates the process
   on memory shortage.  */
char *
native_to_utf8 (const char *orig_string)
{
  const unsigned char *string = (const unsigned char *)orig_string;
  const unsigned char *s;
  char *buffer;
  unsigned char *p;
  size_t length = 0;

  if (no_translation)
    {
      /* Already utf-8 encoded. */
      buffer = jnlib_xstrdup (orig_string);
    }
  else if (!use_iconv)
    {
      /* For Latin-1 we can avoid the iconv overhead. */
      for (s = string; *s; s++)
	{
	  length++;
	  if (*s & 0x80)
	    length++;
	}
      buffer = jnlib_xmalloc (length + 1);
      for (p = (unsigned char *)buffer, s = string; *s; s++)
	{
	  if ( (*s & 0x80 ))
	    {
	      *p++ = 0xc0 | ((*s >> 6) & 3);
	      *p++ = 0x80 | (*s & 0x3f);
	    }
	  else
	    *p++ = *s;
	}
      *p = 0;
    }
  else
    { 
      /* Need to use iconv.  */
      iconv_t cd;
      const char *inptr;
      char *outptr;
      size_t inbytes, outbytes;
     
      cd = iconv_open ("utf-8", active_charset_name);
      if (cd == (iconv_t)-1)
        {
          handle_iconv_error ("utf-8", active_charset_name, 1);
          return native_to_utf8 (string);
        }

      for (s=string; *s; s++ ) 
        {
          length++;
          if ((*s & 0x80))
            length += 5; /* We may need up to 6 bytes for the utf8 output. */
        }
      buffer = jnlib_xmalloc (length + 1);
      
      inptr = string;
      inbytes = strlen (string);
      outptr = buffer;
      outbytes = length;
      if ( iconv (cd, (ICONV_CONST char **)&inptr, &inbytes,
                  &outptr, &outbytes) == (size_t)-1)
        {
          static int shown;

          if (!shown)
            log_info (_("conversion from `%s' to `%s' failed: %s\n"),
                      active_charset_name, "utf-8", strerror (errno));
          shown = 1;
          /* We don't do any conversion at all but use the strings as is. */
          strcpy (buffer, string);
        }
      else /* Success.  */
        {
          *outptr = 0;
          /* We could realloc the buffer now but I doubt that it makes
             much sense given that it will get freed anyway soon
             after.  */
        }
      iconv_close (cd);
    }
  return buffer;
}



static char *
do_utf8_to_native (const char *string, size_t length, int delim,
                   int with_iconv)
{
  int nleft;
  int i;
  unsigned char encbuf[8];
  int encidx;
  const unsigned char *s;
  size_t n;
  char *buffer = NULL;
  char *p = NULL;
  unsigned long val = 0;
  size_t slen;
  int resync = 0;

  /* First pass (p==NULL): count the extended utf-8 characters.  */
  /* Second pass (p!=NULL): create string.  */
  for (;;)
    {
      for (slen = length, nleft = encidx = 0, n = 0,
             s = (const unsigned char *)string;
           slen;
	   s++, slen--)
	{
	  if (resync)
	    {
	      if (!(*s < 128 || (*s >= 0xc0 && *s <= 0xfd)))
		{
		  /* Still invalid. */
		  if (p)
		    {
		      sprintf (p, "\\x%02x", *s);
		      p += 4;
		    }
		  n += 4;
		  continue;
		}
	      resync = 0;
	    }
	  if (!nleft)
	    {
	      if (!(*s & 0x80))
		{	
                  /* Plain ascii. */
		  if ( delim != -1
                       && (*s < 0x20 || *s == 0x7f || *s == delim 
                           || (delim && *s == '\\')))
		    {
		      n++;
		      if (p)
			*p++ = '\\';
		      switch (*s)
			{
                        case '\n': n++; if ( p ) *p++ = 'n'; break;
                        case '\r': n++; if ( p ) *p++ = 'r'; break;
                        case '\f': n++; if ( p ) *p++ = 'f'; break;
                        case '\v': n++; if ( p ) *p++ = 'v'; break;
                        case '\b': n++; if ( p ) *p++ = 'b'; break;
                        case    0: n++; if ( p ) *p++ = '0'; break;
			default:
			  n += 3;
			  if (p)
			    {
			      sprintf (p, "x%02x", *s);
			      p += 3;
			    }
			  break;
			}
		    }
		  else
		    {
		      if (p)
			*p++ = *s;
		      n++;
		    }
		}
	      else if ((*s & 0xe0) == 0xc0) /* 110x xxxx */
		{
		  val = *s & 0x1f;
		  nleft = 1;
		  encidx = 0;
		  encbuf[encidx++] = *s;
		}
	      else if ((*s & 0xf0) == 0xe0) /* 1110 xxxx */
		{	
		  val = *s & 0x0f;
		  nleft = 2;
		  encidx = 0;
		  encbuf[encidx++] = *s;
		}
	      else if ((*s & 0xf8) == 0xf0) /* 1111 0xxx */
		{	
		  val = *s & 0x07;
		  nleft = 3;
		  encidx = 0;
		  encbuf[encidx++] = *s;
		}
	      else if ((*s & 0xfc) == 0xf8) /* 1111 10xx */
		{	
		  val = *s & 0x03;
		  nleft = 4;
		  encidx = 0;
		  encbuf[encidx++] = *s;
		}
	      else if ((*s & 0xfe) == 0xfc) /* 1111 110x */
		{		
		  val = *s & 0x01;
		  nleft = 5;
		  encidx = 0;
		  encbuf[encidx++] = *s;
		}
	      else /* Invalid encoding: print as \xNN. */
		{		
		  if (p)
		    {
		      sprintf (p, "\\x%02x", *s);
		      p += 4;
		    }
		  n += 4;
		  resync = 1;
		}
	    }
	  else if (*s < 0x80 || *s >= 0xc0) /* Invalid utf-8 */
	    {
	      if (p)
		{
		  for (i = 0; i < encidx; i++)
		    {
		      sprintf (p, "\\x%02x", encbuf[i]);
		      p += 4;
		    }
		  sprintf (p, "\\x%02x", *s);
		  p += 4;
		}
	      n += 4 + 4 * encidx;
	      nleft = 0;
	      encidx = 0;
	      resync = 1;
	    }
	  else
	    {
	      encbuf[encidx++] = *s;
	      val <<= 6;
	      val |= *s & 0x3f;
	      if (!--nleft)  /* Ready. */
		{ 
		  if (no_translation)
		    {
		      if (p)
			{
			  for (i = 0; i < encidx; i++)
			    *p++ = encbuf[i];
			}
		      n += encidx;
		      encidx = 0;
		    }
                  else if (with_iconv)
                    {
                      /* Our strategy for using iconv is a bit strange
                         but it better keeps compatibility with
                         previous versions in regard to how invalid
                         encodings are displayed.  What we do is to
                         keep the utf-8 as is and have the real
                         translation step then at the end.  Yes, I
                         know that this is ugly.  However we are short
                         of the 1.4 release and for this branch we
                         should not mess too much around with iconv
                         things.  One reason for this is that we don't
                         know enough about non-GNU iconv
                         implementation and want to minimize the risk
                         of breaking the code on too many platforms.  */
                        if ( p )
                          {
                            for (i=0; i < encidx; i++ )
                              *p++ = encbuf[i];
                          }
                        n += encidx;
                        encidx = 0;
                    }
		  else 	/* Latin-1 case. */
                    {
		      if (val >= 0x80 && val < 256)
			{
                          /* We can simply print this character */
			  n++;	
			  if (p)
			    *p++ = val;
			}
		      else
			{	
                          /* We do not have a translation: print utf8. */
			  if (p)
			    {
			      for (i = 0; i < encidx; i++)
				{
				  sprintf (p, "\\x%02x", encbuf[i]);
				  p += 4;
				}
			    }
			  n += encidx * 4;
			  encidx = 0;
			}
		    }
		}

	    }
	}
      if (!buffer)
	{
          /* Allocate the buffer after the first pass. */
	  buffer = p = jnlib_xmalloc (n + 1);
	}
      else if (with_iconv)
        {
          /* Note: See above for comments.  */
          iconv_t cd;
          const char *inptr;
          char *outbuf, *outptr;
          size_t inbytes, outbytes;
          
          *p = 0;  /* Terminate the buffer. */

          cd = iconv_open (active_charset_name, "utf-8");
          if (cd == (iconv_t)-1)
            {
              handle_iconv_error (active_charset_name, "utf-8", 1);
              jnlib_free (buffer);
              return utf8_to_native (string, length, delim);
            }

          /* Allocate a new buffer large enough to hold all possible
             encodings. */
          n = p - buffer + 1;
          inbytes = n - 1;;
          inptr = buffer;
          outbytes = n * MB_LEN_MAX;
          if (outbytes / MB_LEN_MAX != n) 
            BUG (); /* Actually an overflow. */
          outbuf = outptr = jnlib_xmalloc (outbytes);
          if ( iconv (cd, (ICONV_CONST char **)&inptr, &inbytes,
                      &outptr, &outbytes) == (size_t)-1) 
            {
              static int shown;
              
              if (!shown)
                log_info (_("conversion from `%s' to `%s' failed: %s\n"),
                          "utf-8", active_charset_name, strerror (errno));
              shown = 1;
              /* Didn't worked out.  Try again but without iconv.  */
              jnlib_free (buffer);
              buffer = NULL;
              jnlib_free (outbuf);
              outbuf = do_utf8_to_native (string, length, delim, 0);
            }
            else /* Success.  */
              { 
                *outptr = 0; /* Make sure it is a string. */
                /* We could realloc the buffer now but I doubt that it
                   makes much sense given that it will get freed
                   anyway soon after.  */
                jnlib_free (buffer);
              }
          iconv_close (cd);
          return outbuf;
        }
      else /* Not using iconv. */
	{
	  *p = 0; /* Make sure it is a string. */
	  return buffer;
	}
    }
}

/* Convert string, which is in UTF-8 to native encoding.  Replace
   illegal encodings by some "\xnn" and quote all control
   characters. A character with value DELIM will always be quoted, it
   must be a vanilla ASCII character.  A DELIM value of -1 is special:
   it disables all quoting of control characters.  This function
   terminates the process on memory shortage.  */
char *
utf8_to_native (const char *string, size_t length, int delim)
{
  return do_utf8_to_native (string, length, delim, use_iconv);
}




/* Wrapper function for iconv_open, required for W32 as we dlopen that
   library on that system.  */
jnlib_iconv_t 
jnlib_iconv_open (const char *tocode, const char *fromcode)
{
#ifdef HAVE_W32_SYSTEM
  if (load_libiconv ())
    return (jnlib_iconv_t)(-1);
#endif /*HAVE_W32_SYSTEM*/      

  return (jnlib_iconv_t)iconv_open (tocode, fromcode);
}


/* Wrapper function for iconv, required for W32 as we dlopen that
   library on that system.  */
size_t
jnlib_iconv (jnlib_iconv_t cd,
             const char **inbuf, size_t *inbytesleft,
             char **outbuf, size_t *outbytesleft)
{

#ifdef HAVE_W32_SYSTEM
  if (load_libiconv ())
    return 0;
#endif /*HAVE_W32_SYSTEM*/      

  return iconv ((iconv_t)cd, (char**)inbuf, inbytesleft, outbuf, outbytesleft);
}

/* Wrapper function for iconv_close, required for W32 as we dlopen that
   library on that system.  */
int
jnlib_iconv_close (jnlib_iconv_t cd)
{
#ifdef HAVE_W32_SYSTEM
  if (load_libiconv ())
    return 0;
#endif /*HAVE_W32_SYSTEM*/      

  return iconv_close ((iconv_t)cd);
}
