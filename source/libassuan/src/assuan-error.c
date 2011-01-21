/* assuan-error.c
   Copyright (C) 2009 Free Software Foundation, Inc.

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
#include <assert.h>
#include <errno.h>

#undef _ASSUAN_IN_LIBASSUAN /* undef to get all error codes. */
#include "assuan.h"
#include "assuan-defs.h"


/* A small helper function to treat EAGAIN transparently to the
   caller.  */
int
_assuan_error_is_eagain (assuan_context_t ctx, gpg_error_t err)
{
  if (gpg_err_code (err) == GPG_ERR_EAGAIN)
    {
      /* Avoid spinning by sleeping for one tenth of a second.  */
      _assuan_usleep (ctx, 100000);
      return 1;
    }
  else
    return 0;
}



#ifdef HAVE_W32_SYSTEM
char *
_assuan_w32_strerror (assuan_context_t ctx, int ec)
{
  if (ec == -1)
    ec = (int)GetLastError ();
#ifdef HAVE_W32CE_SYSTEM
  snprintf (ctx->w32_strerror, sizeof (ctx->w32_strerror) - 1,
            "ec=%d", (int)GetLastError ());
#else
  FormatMessage (FORMAT_MESSAGE_FROM_SYSTEM, NULL, ec,
                 MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT),
                 ctx->w32_strerror, sizeof (ctx->w32_strerror) - 1, NULL);
#endif
  return ctx->w32_strerror;
}
#endif

