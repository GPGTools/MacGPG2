/* assuan.c - Global interface (not specific to context).
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

#include <stdlib.h>

#include "assuan-defs.h"
#include "debug.h"
 

/* Global default state.  */

/* The default error source gor generated error codes.  */
static gpg_err_source_t _assuan_default_err_source = GPG_ERR_SOURCE_USER_1;

/* The default memory management functions.  */
static struct assuan_malloc_hooks _assuan_default_malloc_hooks =
  { malloc, realloc, free };

/* The default logging handler.  */
static assuan_log_cb_t _assuan_default_log_cb = _assuan_log_handler;
static void *_assuan_default_log_cb_data = NULL;


/* Set the default gpg error source.  */
void
assuan_set_gpg_err_source (gpg_err_source_t errsource)
{
  _assuan_default_err_source = errsource;
}


/* Get the default gpg error source.  */
gpg_err_source_t
assuan_get_gpg_err_source (void)
{
  return _assuan_default_err_source;
}


/* Set the default malloc hooks.  */
void
assuan_set_malloc_hooks (assuan_malloc_hooks_t malloc_hooks)
{
  _assuan_default_malloc_hooks = *malloc_hooks;
}


/* Get the default malloc hooks.  */
assuan_malloc_hooks_t
assuan_get_malloc_hooks (void)
{
  return &_assuan_default_malloc_hooks;
}


/* Set the default log callback handler.  */
void
assuan_set_log_cb (assuan_log_cb_t log_cb, void *log_cb_data)
{
  _assuan_default_log_cb = log_cb;
  _assuan_default_log_cb_data = log_cb_data;
  _assuan_init_log_envvars ();
}


/* Get the default log callback handler.  */
void
assuan_get_log_cb (assuan_log_cb_t *log_cb, void **log_cb_data)
{
  *log_cb = _assuan_default_log_cb;
  *log_cb_data = _assuan_default_log_cb_data;
}


void
assuan_set_system_hooks (assuan_system_hooks_t system_hooks)
{
  _assuan_system_hooks_copy (&_assuan_system_hooks, system_hooks);
}


/* Create a new Assuan context.  The initial parameters are all needed
   in the creation of the context.  */
gpg_error_t
assuan_new_ext (assuan_context_t *r_ctx, gpg_err_source_t err_source,
		assuan_malloc_hooks_t malloc_hooks, assuan_log_cb_t log_cb,
		void *log_cb_data)
{
  struct assuan_context_s wctx;
  assuan_context_t ctx;

  /* Set up a working context so we can use standard functions.  */
  memset (&wctx, 0, sizeof (wctx));
  wctx.err_source = err_source;
  wctx.malloc_hooks = *malloc_hooks;
  wctx.log_cb = log_cb;
  wctx.log_cb_data = log_cb_data;

  /* Need a new block for the trace macros to work.  */
  {
    TRACE_BEG8 (&wctx, ASSUAN_LOG_CTX, "assuan_new_ext", r_ctx,
		"err_source = %i (%s), malloc_hooks = %p (%p, %p, %p), "
		"log_cb = %p, log_cb_data = %p", err_source,
		gpg_strsource (err_source), malloc_hooks, malloc_hooks->malloc,
		malloc_hooks->realloc, malloc_hooks->free, log_cb, log_cb_data);

    *r_ctx = NULL;
    ctx = _assuan_malloc (&wctx, sizeof (*ctx));
    if (!ctx)
      return TRACE_ERR (gpg_err_code_from_syserror ());

    memcpy (ctx, &wctx, sizeof (*ctx));
    ctx->system = _assuan_system_hooks;

    /* FIXME: Delegate to subsystems/engines, as the FDs are not our
       responsibility (we don't deallocate them, for example).  */
    ctx->input_fd = ASSUAN_INVALID_FD;
    ctx->output_fd = ASSUAN_INVALID_FD;
    ctx->inbound.fd = ASSUAN_INVALID_FD;
    ctx->outbound.fd = ASSUAN_INVALID_FD;
    ctx->listen_fd = ASSUAN_INVALID_FD;

    *r_ctx = ctx;

    return TRACE_SUC1 ("ctx=%p", ctx);
  }
}


/* Create a new context with default arguments.  */
gpg_error_t
assuan_new (assuan_context_t *r_ctx)
{
  return assuan_new_ext (r_ctx, _assuan_default_err_source,
			 &_assuan_default_malloc_hooks,
			 _assuan_default_log_cb, 
			 _assuan_default_log_cb_data);
}


/* Release all resources associated with an engine operation.  */
void
_assuan_reset (assuan_context_t ctx)
{
  if (ctx->engine.release)
    {
      (*ctx->engine.release) (ctx);
      ctx->engine.release = NULL;
    }

  /* FIXME: Clean standard commands */
}


/* Release all resources associated with the given context.  */
void
assuan_release (assuan_context_t ctx)
{
  if (! ctx)
    return;

  TRACE (ctx, ASSUAN_LOG_CTX, "assuan_release", ctx);

  _assuan_reset (ctx);
  /* None of the members that are our responsibility requires
     deallocation.  */
  _assuan_free (ctx, ctx);
}
