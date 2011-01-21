/* context.c - Context specific interface.
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

#include "assuan-defs.h"
#include "debug.h"


/* Set user-data in a context.  */
void
assuan_set_pointer (assuan_context_t ctx, void *user_pointer)
{
  TRACE1 (ctx, ASSUAN_LOG_CTX, "assuan_set_pointer", ctx,
	  "user_pointer=%p", user_pointer);

  if (ctx)
    ctx->user_pointer = user_pointer;
}


/* Get user-data in a context.  */
void *
assuan_get_pointer (assuan_context_t ctx)
{
#if 0
  /* This is called often.  */
  TRACE1 (ctx, ASSUAN_LOG_CTX, "assuan_get_pointer", ctx,
	  "ctx->user_pointer=%p", ctx ? ctx->user_pointer : NULL);
#endif

  if (! ctx)
    return NULL;

  return ctx->user_pointer;
}


/* For context CTX, set the flag FLAG to VALUE.  Values for flags
   are usually 1 or 0 but certain flags might allow for other values;
   see the description of the type assuan_flag_t for details.  */
void
assuan_set_flag (assuan_context_t ctx, assuan_flag_t flag, int value)
{
  TRACE2 (ctx, ASSUAN_LOG_CTX, "assuan_set_flag", ctx,
	  "flag=%i,value=%i", flag, value);

  if (!ctx)
    return;

  switch (flag)
    {
    case ASSUAN_NO_WAITPID:
      ctx->flags.no_waitpid = value;
      break;

    case ASSUAN_CONFIDENTIAL:
      ctx->flags.confidential = value;
      break;

    case ASSUAN_NO_FIXSIGNALS:
      ctx->flags.no_fixsignals = value;
      break;
    }
}


/* Return the VALUE of FLAG in context CTX.  */
int
assuan_get_flag (assuan_context_t ctx, assuan_flag_t flag)
{
  int res = 0;
  TRACE_BEG1 (ctx, ASSUAN_LOG_CTX, "assuan_get_flag", ctx,
	      "flag=%i", flag);

  if (! ctx)
    return 0;

  switch (flag)
    {
    case ASSUAN_NO_WAITPID:
      res = ctx->flags.no_waitpid;
      break;

    case ASSUAN_CONFIDENTIAL:
      res = ctx->flags.confidential;
      break;

    case ASSUAN_NO_FIXSIGNALS:
      res = ctx->flags.no_fixsignals;
      break;
    }

  return TRACE_SUC1 ("flag_value=%i", res);
}


/* Same as assuan_set_flag (ctx, ASSUAN_NO_WAITPID, 1).  */
void
assuan_begin_confidential (assuan_context_t ctx)
{
  assuan_set_flag (ctx, ASSUAN_CONFIDENTIAL, 1);
}


/* Same as assuan_set_flag (ctx, ASSUAN_NO_WAITPID, 0).  */
void
assuan_end_confidential (assuan_context_t ctx)
{
  assuan_set_flag (ctx, ASSUAN_CONFIDENTIAL, 0);
}


/* Set the system callbacks.  */
void
assuan_ctx_set_system_hooks (assuan_context_t ctx,
			     assuan_system_hooks_t system_hooks)
{
  TRACE2 (ctx, ASSUAN_LOG_CTX, "assuan_set_system_hooks", ctx,
	  "system_hooks=%p (version %i)", system_hooks,
	  system_hooks->version);

  _assuan_system_hooks_copy (&ctx->system, system_hooks);
}


/* Set the IO monitor function.  */
void assuan_set_io_monitor (assuan_context_t ctx,
			    assuan_io_monitor_t io_monitor, void *hook_data)
{
  TRACE2 (ctx, ASSUAN_LOG_CTX, "assuan_set_io_monitor", ctx,
	  "io_monitor=%p,hook_data=%p", io_monitor, hook_data);

  if (! ctx)
    return;

  ctx->io_monitor = io_monitor;
  ctx->io_monitor_data = hook_data;
}


/* Store the error in the context so that the error sending function
  can take out a descriptive text.  Inside the assuan code, use the
  macro set_error instead of this function. */
gpg_error_t
assuan_set_error (assuan_context_t ctx, gpg_error_t err, const char *text)
{
  TRACE4 (ctx, ASSUAN_LOG_CTX, "assuan_set_error", ctx,
	  "err=%i (%s,%s),text=%s", err, gpg_strsource (err),
	  gpg_strerror (err), text);
  
  ctx->err_no = err;
  ctx->err_str = text;
  return err;
}


/* Return the PID of the peer or ASSUAN_INVALID_PID if not known.
   This function works in some situations where assuan_get_ucred
   fails. */
pid_t
assuan_get_pid (assuan_context_t ctx)
{
  TRACE1 (ctx, ASSUAN_LOG_CTX, "assuan_get_pid", ctx,
	  "pid=%i", ctx ? ctx->pid : -1);

  return (ctx && ctx->pid) ? ctx->pid : ASSUAN_INVALID_PID;
}


/* Return user credentials.  For getting the pid of the peer the
   assuan_get_pid is usually better suited. */
gpg_error_t
assuan_get_peercred (assuan_context_t ctx, assuan_peercred_t *peercred)
{
  TRACE (ctx, ASSUAN_LOG_CTX, "assuan_get_peercred", ctx);

  if (!ctx)
    return _assuan_error (ctx, GPG_ERR_ASS_INV_VALUE);
  if (!ctx->peercred_valid)
    return _assuan_error (ctx, GPG_ERR_ASS_GENERAL);

  *peercred = &ctx->peercred;

  return 0;
}
