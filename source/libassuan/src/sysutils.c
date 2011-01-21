/* sysutils.c - System utilities
   Copyright (C) 2010 Free Software Foundation, Inc.

   This file is part of Assuan.

   Assuan is free software; you can redistribute it and/or modify it
   under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of
   the License, or (at your option) any later version.

   Assuan is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#ifdef HAVE_W32_SYSTEM
#include <windows.h>
# ifdef HAVE_W32CE_SYSTEM
# include <winioctl.h>
# include <devload.h>
# endif /*HAVE_W32CE_SYSTEM*/
#endif /*HAVE_W32_SYSTEM*/

#include "assuan-defs.h"


/* This is actually a dummy function to make sure that is module is
   not empty.  Some compilers barf on empty modules.  */
const char *
_assuan_sysutils_blurb (void)
{
  static const char blurb[] = 
    "\n\n"
    "This is Libassuan - The GnuPG IPC Library\n"
    "Copyright 2000, 2002, 2003, 2004, 2007, 2008, 2009,\n"
    "          2010 Free Software Foundation, Inc.\n"
    "\n\n";
  return blurb;
}


/* Return a string from the Win32 Registry or NULL in case of error.
   The returned string is allocated using a plain malloc and thus the
   caller needs to call the standard free().  The string is looked up
   under HKEY_LOCAL_MACHINE.  */
#ifdef HAVE_W32CE_SYSTEM
static char *
w32_read_registry (const wchar_t *dir, const wchar_t *name)
{
  HKEY handle;
  DWORD n, nbytes;
  wchar_t *buffer = NULL;
  char *result = NULL;
  
  if (RegOpenKeyEx (HKEY_LOCAL_MACHINE, dir, 0, KEY_READ, &handle))
    return NULL; /* No need for a RegClose, so return immediately. */

  nbytes = 1;
  if (RegQueryValueEx (handle, name, 0, NULL, NULL, &nbytes))
    goto leave;
  buffer = malloc ((n=nbytes+2));
  if (!buffer)
    goto leave;
  if (RegQueryValueEx (handle, name, 0, NULL, (PBYTE)buffer, &n))
    {
      free (buffer);
      buffer = NULL;
      goto leave;
    }
  
  n = WideCharToMultiByte (CP_UTF8, 0, buffer, nbytes, NULL, 0, NULL, NULL);
  if (n < 0 || (n+1) <= 0)
    goto leave;
  result = malloc (n+1);
  if (!result)
    goto leave;
  n = WideCharToMultiByte (CP_UTF8, 0, buffer, nbytes, result, n, NULL, NULL);
  if (n < 0)
    {
      free (result);
      result = NULL;
      goto leave;
    }
  result[n] = 0;

 leave:
  free (buffer);
  RegCloseKey (handle);
  return result;
}
#endif /*HAVE_W32CE_SYSTEM*/



#ifdef HAVE_W32CE_SYSTEM
/* Replacement for getenv which takes care of the our use of getenv.
   The code is not thread safe but we expect it to work in all cases
   because it is called for the first time early enough.  */
char *
_assuan_getenv (const char *name)
{
  static int initialized;
  static char *val_debug;
  static char *val_full_logging;

  if (!initialized)
    {
      val_debug = w32_read_registry (L"\\Software\\GNU\\libassuan",
                                     L"debug");
      val_full_logging = w32_read_registry (L"\\Software\\GNU\\libassuan",
                                            L"full_logging");
      initialized = 1;
    }


  if (!strcmp (name, "ASSUAN_DEBUG"))
    return val_debug;
  else if (!strcmp (name, "ASSUAN_FULL_LOGGING"))
    return val_full_logging;
  else
    return NULL;
}
#endif /*HAVE_W32CE_SYSTEM*/

