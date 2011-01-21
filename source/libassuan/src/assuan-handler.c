/* assuan-handler.c - dispatch commands 
   Copyright (C) 2001, 2002, 2003, 2007, 2009 Free Software Foundation, Inc.

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
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "assuan-defs.h"
#include "debug.h"


#define spacep(p)  (*(p) == ' ' || *(p) == '\t')
#define digitp(a) ((a) >= '0' && (a) <= '9')

static int my_strcasecmp (const char *a, const char *b);


#define PROCESS_DONE(ctx, rc) \
  ((ctx)->in_process_next ? assuan_process_done ((ctx), (rc)) : (rc))

static gpg_error_t
dummy_handler (assuan_context_t ctx, char *line)
{
  return
    PROCESS_DONE (ctx, set_error (ctx, GPG_ERR_ASSUAN_SERVER_FAULT,
				  "no handler registered"));
}


static gpg_error_t
std_handler_nop (assuan_context_t ctx, char *line)
{
  return PROCESS_DONE (ctx, 0); /* okay */
}
  
static gpg_error_t
std_handler_cancel (assuan_context_t ctx, char *line)
{
  if (ctx->cancel_notify_fnc)
    /* Return value ignored.  */
    ctx->cancel_notify_fnc (ctx, line);
  return PROCESS_DONE (ctx, set_error (ctx, GPG_ERR_NOT_IMPLEMENTED, NULL));
}

static gpg_error_t
std_handler_option (assuan_context_t ctx, char *line)
{
  char *key, *value, *p;

  for (key=line; spacep (key); key++)
    ;
  if (!*key)
    return
      PROCESS_DONE (ctx, set_error (ctx, GPG_ERR_ASS_SYNTAX, "argument required"));
  if (*key == '=')
    return
      PROCESS_DONE (ctx, set_error (ctx, GPG_ERR_ASS_SYNTAX,
				    "no option name given"));
  for (value=key; *value && !spacep (value) && *value != '='; value++)
    ;
  if (*value)
    {
      if (spacep (value))
        *value++ = 0; /* terminate key */
      for (; spacep (value); value++)
        ;
      if (*value == '=')
        {
          *value++ = 0; /* terminate key */
          for (; spacep (value); value++)
            ;
          if (!*value)
            return
	      PROCESS_DONE (ctx, set_error (ctx, GPG_ERR_ASS_SYNTAX,
					    "option argument expected"));
        }
      if (*value)
        {
          for (p = value + strlen(value) - 1; p > value && spacep (p); p--)
            ;
          if (p > value)
            *++p = 0; /* strip trailing spaces */
        }
    }

  if (*key == '-' && key[1] == '-' && key[2])
    key += 2; /* the double dashes are optional */
  if (*key == '-')
    return PROCESS_DONE (ctx,
			 set_error (ctx, GPG_ERR_ASS_SYNTAX,
				    "option should not begin with one dash"));

  if (ctx->option_handler_fnc)
    return PROCESS_DONE (ctx, ctx->option_handler_fnc (ctx, key, value));
  return PROCESS_DONE (ctx, 0);
}
  
static gpg_error_t
std_handler_bye (assuan_context_t ctx, char *line)
{
  if (ctx->bye_notify_fnc)
    /* Return value ignored.  */
    ctx->bye_notify_fnc (ctx, line);
  assuan_close_input_fd (ctx);
  assuan_close_output_fd (ctx);
  /* pretty simple :-) */
  ctx->process_complete = 1;
  return PROCESS_DONE (ctx, 0);
}
  
static gpg_error_t
std_handler_auth (assuan_context_t ctx, char *line)
{
  return PROCESS_DONE (ctx, set_error (ctx, GPG_ERR_NOT_IMPLEMENTED, NULL));
}
  
static gpg_error_t
std_handler_reset (assuan_context_t ctx, char *line)
{
  gpg_error_t err = 0;

  if (ctx->reset_notify_fnc)
    err = ctx->reset_notify_fnc (ctx, line);
  if (! err)
    {
      assuan_close_input_fd (ctx);
      assuan_close_output_fd (ctx);
      _assuan_uds_close_fds (ctx);
    }
  return PROCESS_DONE (ctx, err);
}
  
static gpg_error_t
std_handler_help (assuan_context_t ctx, char *line)
{
  unsigned int i;
  char buf[ASSUAN_LINELENGTH];
  const char *helpstr;
  size_t n;

  n = strcspn (line, " \t\n");
  if (!n)
    {
      /* Print all commands.  If a help string is available and that
         starts with the command name, print the first line of the
         help string.  */
      for (i = 0; i < ctx->cmdtbl_used; i++)
        {
          n = strlen (ctx->cmdtbl[i].name);
          helpstr = ctx->cmdtbl[i].helpstr; 
          if (helpstr
              && !strncmp (ctx->cmdtbl[i].name, helpstr, n)
              && (!helpstr[n] || helpstr[n] == '\n' || helpstr[n] == ' ')
              && (n = strcspn (helpstr, "\n"))          )
            snprintf (buf, sizeof (buf), "# %.*s", (int)n, helpstr);
          else
            snprintf (buf, sizeof (buf), "# %s", ctx->cmdtbl[i].name);
          buf[ASSUAN_LINELENGTH - 1] = '\0';
          assuan_write_line (ctx, buf);
        }
    }
  else
    {
      /* Print the help for the given command.  */
      int c = line[n];
      line[n] = 0;
      for (i=0; ctx->cmdtbl[i].name; i++)
        if (!my_strcasecmp (line, ctx->cmdtbl[i].name))
          break;
      line[n] = c;
      if (!ctx->cmdtbl[i].name)
        return PROCESS_DONE (ctx, set_error (ctx,GPG_ERR_UNKNOWN_COMMAND,NULL));
      helpstr = ctx->cmdtbl[i].helpstr; 
      if (!helpstr)
        return PROCESS_DONE (ctx, set_error (ctx, GPG_ERR_NOT_FOUND, NULL));
      do
        {
          n = strcspn (helpstr, "\n");
          snprintf (buf, sizeof (buf), "# %.*s", (int)n, helpstr);
          helpstr += n;
          if (*helpstr == '\n')
            helpstr++;
          buf[ASSUAN_LINELENGTH - 1] = '\0';
          assuan_write_line (ctx, buf);
        }
      while (*helpstr);
    }

  return PROCESS_DONE (ctx, 0);
}


static gpg_error_t
std_handler_end (assuan_context_t ctx, char *line)
{
  return PROCESS_DONE (ctx, set_error (ctx, GPG_ERR_NOT_IMPLEMENTED, NULL));
}


gpg_error_t
assuan_command_parse_fd (assuan_context_t ctx, char *line, assuan_fd_t *rfd)
{
  char *endp;

  if ((strncmp (line, "FD", 2) && strncmp (line, "fd", 2))
      || (line[2] != '=' && line[2] != '\0' && !spacep(&line[2])))
    return set_error (ctx, GPG_ERR_ASS_SYNTAX, "FD[=<n>] expected");
  line += 2;
  if (*line == '=')
    {
      line ++;
      if (!digitp (*line))
	return set_error (ctx, GPG_ERR_ASS_SYNTAX, "number required");
#ifdef HAVE_W32_SYSTEM
      /* Fixme: For a W32/64bit system we will need to change the cast
         and the conversion function.  */
      *rfd = (void*)strtoul (line, &endp, 10);
#else
      *rfd = strtoul (line, &endp, 10);
#endif
      /* Remove that argument so that a notify handler won't see it. */
      memset (line, ' ', endp? (endp-line):strlen(line));

      if (*rfd == ctx->inbound.fd)
	return set_error (ctx, GPG_ERR_ASS_PARAMETER, "fd same as inbound fd");
      if (*rfd == ctx->outbound.fd)
	return set_error (ctx, GPG_ERR_ASS_PARAMETER, "fd same as outbound fd");
      return 0;
    }
  else
    /* Our peer has sent the file descriptor.  */
    return assuan_receivefd (ctx, rfd);
}


/* Format is INPUT FD=<n> */
static gpg_error_t
std_handler_input (assuan_context_t ctx, char *line)
{
  gpg_error_t rc;
  assuan_fd_t fd, oldfd;

  rc = assuan_command_parse_fd (ctx, line, &fd);
  if (rc)
    return PROCESS_DONE (ctx, rc);

#ifdef HAVE_W32CE_SYSTEM
  oldfd = fd;
  fd = _assuan_w32ce_finish_pipe ((int)fd, 0);
  if (fd == INVALID_HANDLE_VALUE)
    return PROCESS_DONE (ctx, set_error (ctx, GPG_ERR_ASS_PARAMETER,
					 "rvid conversion failed"));
  TRACE2 (ctx, ASSUAN_LOG_SYSIO, "std_handler_input", ctx,
	  "turned RVID 0x%x into handle 0x%x", oldfd, fd);
#endif

  if (ctx->input_notify_fnc)
    {
      oldfd = ctx->input_fd;
      ctx->input_fd = fd;
      rc = ctx->input_notify_fnc (ctx, line);
      if (rc)
        ctx->input_fd = oldfd;
    }
  else if (!rc)
    ctx->input_fd = fd;
  return PROCESS_DONE (ctx, rc);
}


/* Format is OUTPUT FD=<n> */
static gpg_error_t
std_handler_output (assuan_context_t ctx, char *line)
{
  gpg_error_t rc;
  assuan_fd_t fd, oldfd;
  
  rc = assuan_command_parse_fd (ctx, line, &fd);
  if (rc)
    return PROCESS_DONE (ctx, rc);

#ifdef HAVE_W32CE_SYSTEM
  oldfd = fd;
  fd = _assuan_w32ce_finish_pipe ((int)fd, 1);
  if (fd == INVALID_HANDLE_VALUE)
    return PROCESS_DONE (ctx, set_error (ctx, gpg_err_code_from_syserror (),
					 "rvid conversion failed"));
  TRACE2 (ctx, ASSUAN_LOG_SYSIO, "std_handler_output", ctx,
	  "turned RVID 0x%x into handle 0x%x", oldfd, fd);
#endif

  if (ctx->output_notify_fnc)
    {
      oldfd = ctx->output_fd;
      ctx->output_fd = fd;
      rc = ctx->output_notify_fnc (ctx, line);
      if (rc)
        ctx->output_fd = oldfd;
    }
  else if (!rc)
    ctx->output_fd = fd;
  return PROCESS_DONE (ctx, rc);
}


/* This is a table with the standard commands and handler for them.
   The table is used to initialize a new context and associate strings
   with default handlers */
static struct {
  const char *name;
  gpg_error_t (*handler)(assuan_context_t, char *line);
  int always; /* always initialize this command */
} std_cmd_table[] = {
  { "NOP",    std_handler_nop, 1 },
  { "CANCEL", std_handler_cancel, 1 },
  { "OPTION", std_handler_option, 1 },
  { "BYE",    std_handler_bye, 1 },
  { "AUTH",   std_handler_auth, 1 },
  { "RESET",  std_handler_reset, 1 },
  { "END",    std_handler_end, 1 },
  { "HELP",   std_handler_help, 1 },
              
  { "INPUT",  std_handler_input, 0 },
  { "OUTPUT", std_handler_output, 0 },
  { NULL, NULL, 0 }
};


/**
 * assuan_register_command:
 * @ctx: the server context
 * @cmd_name: A string with the command name
 * @handler: The handler function to be called or NULL to use a default
 *           handler.
 * HELPSTRING
 * 
 * Register a handler to be used for a given command.  Note that
 * several default handlers are already regsitered with a new context.
 * This function however allows to override them.
 * 
 * Return value: 0 on success or an error code
 **/
gpg_error_t
assuan_register_command (assuan_context_t ctx, const char *cmd_name,
                         assuan_handler_t handler, const char *help_string)
{
  int i;
  const char *s;

  if (cmd_name && !*cmd_name)
    cmd_name = NULL;

  if (!cmd_name)
    return _assuan_error (ctx, GPG_ERR_ASS_INV_VALUE);

  if (!handler)
    { /* find a default handler. */
      for (i=0; (s=std_cmd_table[i].name) && strcmp (cmd_name, s); i++)
        ;
      if (!s)
        { /* Try again but case insensitive. */
          for (i=0; (s=std_cmd_table[i].name)
                    && my_strcasecmp (cmd_name, s); i++)
            ;
        }
      if (s)
        handler = std_cmd_table[i].handler;
      if (!handler)
        handler = dummy_handler; /* Last resort is the dummy handler. */
    }
  
  if (!ctx->cmdtbl)
    {
      ctx->cmdtbl_size = 50;
      ctx->cmdtbl = _assuan_calloc (ctx, ctx->cmdtbl_size, sizeof *ctx->cmdtbl);
      if (!ctx->cmdtbl)
	return _assuan_error (ctx, gpg_err_code_from_syserror ());
      ctx->cmdtbl_used = 0;
    }
  else if (ctx->cmdtbl_used >= ctx->cmdtbl_size)
    {
      struct cmdtbl_s *x;

      x = _assuan_realloc (ctx, ctx->cmdtbl, (ctx->cmdtbl_size+10) * sizeof *x);
      if (!x)
	return _assuan_error (ctx, gpg_err_code_from_syserror ());
      ctx->cmdtbl = x;
      ctx->cmdtbl_size += 50;
    }

  ctx->cmdtbl[ctx->cmdtbl_used].name = cmd_name;
  ctx->cmdtbl[ctx->cmdtbl_used].handler = handler;
  ctx->cmdtbl[ctx->cmdtbl_used].helpstr = help_string;
  ctx->cmdtbl_used++;
  return 0;
}

/* Return the name of the command currently processed by a handler.
   The string returned is valid until the next call to an assuan
   function on the same context.  Returns NULL if no handler is
   executed or the command is not known.  */
const char *
assuan_get_command_name (assuan_context_t ctx)
{
  return ctx? ctx->current_cmd_name : NULL;
}

gpg_error_t
assuan_register_post_cmd_notify (assuan_context_t ctx,
                                 void (*fnc)(assuan_context_t, gpg_error_t))
{
  if (!ctx)
    return _assuan_error (ctx, GPG_ERR_ASS_INV_VALUE);
  ctx->post_cmd_notify_fnc = fnc;
  return 0;
}

gpg_error_t
assuan_register_bye_notify (assuan_context_t ctx, assuan_handler_t fnc)
{
  if (!ctx)
    return _assuan_error (ctx, GPG_ERR_ASS_INV_VALUE);
  ctx->bye_notify_fnc = fnc;
  return 0;
}

gpg_error_t
assuan_register_reset_notify (assuan_context_t ctx, assuan_handler_t fnc)
{
  if (!ctx)
    return _assuan_error (ctx, GPG_ERR_ASS_INV_VALUE);
  ctx->reset_notify_fnc = fnc;
  return 0;
}

gpg_error_t
assuan_register_cancel_notify (assuan_context_t ctx, assuan_handler_t fnc)
{
  if (!ctx)
    return _assuan_error (ctx, GPG_ERR_ASS_INV_VALUE);
  ctx->cancel_notify_fnc = fnc;
  return 0;
}

gpg_error_t
assuan_register_option_handler (assuan_context_t ctx,
				gpg_error_t (*fnc)(assuan_context_t,
						   const char*, const char*))
{
  if (!ctx)
    return _assuan_error (ctx, GPG_ERR_ASS_INV_VALUE);
  ctx->option_handler_fnc = fnc;
  return 0;
}

gpg_error_t
assuan_register_input_notify (assuan_context_t ctx, assuan_handler_t fnc)
{
  if (!ctx)
    return _assuan_error (ctx, GPG_ERR_ASS_INV_VALUE);
  ctx->input_notify_fnc = fnc;
  return 0;
}

gpg_error_t
assuan_register_output_notify (assuan_context_t ctx, assuan_handler_t fnc)
{
  if (!ctx)
    return _assuan_error (ctx, GPG_ERR_ASS_INV_VALUE);
  ctx->output_notify_fnc = fnc;
  return 0;
}


/* Helper to register the standards commands */
gpg_error_t
_assuan_register_std_commands (assuan_context_t ctx)
{
  gpg_error_t rc;
  int i;

  for (i = 0; std_cmd_table[i].name; i++)
    {
      if (std_cmd_table[i].always)
        {
          rc = assuan_register_command (ctx, std_cmd_table[i].name, NULL, NULL);
          if (rc)
            return rc;
        }
    } 
  return 0;
}



/* Process the special data lines.  The "D " has already been removed
   from the line.  As all handlers this function may modify the line.  */
static gpg_error_t
handle_data_line (assuan_context_t ctx, char *line, int linelen)
{
  return set_error (ctx, GPG_ERR_NOT_IMPLEMENTED, NULL);
}

/* like ascii_strcasecmp but assume that B is already uppercase */
static int
my_strcasecmp (const char *a, const char *b)
{
    if (a == b)
        return 0;

    for (; *a && *b; a++, b++)
      {
	if (((*a >= 'a' && *a <= 'z')? (*a&~0x20):*a) != *b)
	    break;
      }
    return *a == *b? 0 : (((*a >= 'a' && *a <= 'z')? (*a&~0x20):*a) - *b);
}


/* Parse the line, break out the command, find it in the command
   table, remove leading and white spaces from the arguments, call the
   handler with the argument line and return the error.  */
static gpg_error_t
dispatch_command (assuan_context_t ctx, char *line, int linelen)
{
  gpg_error_t err;
  char *p;
  const char *s;
  int shift, i;

  /* Note that as this function is invoked by assuan_process_next as
     well, we need to hide non-critical errors with PROCESS_DONE.  */

  if (*line == 'D' && line[1] == ' ') /* divert to special handler */
    /* FIXME: Depending on the final implementation of
       handle_data_line, this may be wrong here.  For example, if a
       user callback is invoked, and that callback is responsible for
       calling assuan_process_done, then this is wrong.  */
    return PROCESS_DONE (ctx, handle_data_line (ctx, line+2, linelen-2));

  for (p=line; *p && *p != ' ' && *p != '\t'; p++)
    ;
  if (p==line)
    return PROCESS_DONE
      (ctx, set_error (ctx, GPG_ERR_ASS_SYNTAX, "leading white-space")); 
  if (*p) 
    { /* Skip over leading WS after the keyword */
      *p++ = 0;
      while ( *p == ' ' || *p == '\t')
        p++;
    }
  shift = p - line;

  for (i=0; (s=ctx->cmdtbl[i].name); i++)
    {
      if (!strcmp (line, s))
        break;
    }
  if (!s)
    { /* and try case insensitive */
      for (i=0; (s=ctx->cmdtbl[i].name); i++)
        {
          if (!my_strcasecmp (line, s))
            break;
        }
    }
  if (!s)
    return PROCESS_DONE (ctx, set_error (ctx, GPG_ERR_ASS_UNKNOWN_CMD, NULL));
  line += shift;
  linelen -= shift;

/*    fprintf (stderr, "DBG-assuan: processing %s `%s'\n", s, line); */
  ctx->current_cmd_name = ctx->cmdtbl[i].name;
  err = ctx->cmdtbl[i].handler (ctx, line);
  ctx->current_cmd_name = NULL;
  return err;
}


/* Call this to acknowledge the current command.  */
gpg_error_t
assuan_process_done (assuan_context_t ctx, gpg_error_t rc)
{
  if (!ctx->in_command)
    return _assuan_error (ctx, GPG_ERR_ASS_GENERAL);

  ctx->in_command = 0;

  /* Check for data write errors.  */
  if (ctx->outbound.data.fp)
    {
      /* Flush the data lines.  */
      fclose (ctx->outbound.data.fp);
      ctx->outbound.data.fp = NULL;
      if (!rc && ctx->outbound.data.error)
	rc = ctx->outbound.data.error;
    }
  else
    {
      /* Flush any data send without using the data FP.  */
      assuan_send_data (ctx, NULL, 0);
      if (!rc && ctx->outbound.data.error)
	rc = ctx->outbound.data.error;
    }
  
  /* Error handling.  */
  if (!rc)
    {
      if (ctx->process_complete)
	{
	  /* No error checking because the peer may have already
	     disconnect. */ 
	  assuan_write_line (ctx, "OK closing connection");
	  ctx->finish_handler (ctx);
	}
      else
	rc = assuan_write_line (ctx, ctx->okay_line ? ctx->okay_line : "OK");
    }
  else 
    {
      char errline[300];
      const char *text = ctx->err_no == rc ? ctx->err_str : NULL;
      char ebuf[50];
	  
      gpg_strerror_r (rc, ebuf, sizeof (ebuf));
      sprintf (errline, "ERR %d %.50s <%.30s>%s%.100s",
	       rc, ebuf, gpg_strsource (rc),
	       text? " - ":"", text?text:"");

      rc = assuan_write_line (ctx, errline);
    }
  
  if (ctx->post_cmd_notify_fnc)
    ctx->post_cmd_notify_fnc (ctx, rc);
  
  ctx->flags.confidential = 0;
  if (ctx->okay_line)
    {
      _assuan_free (ctx, ctx->okay_line);
      ctx->okay_line = NULL;
    }

  return rc;
}


static gpg_error_t
process_next (assuan_context_t ctx)
{
  gpg_error_t rc;

  /* What the next thing to do is depends on the current state.
     However, we will always first read the next line.  The client is
     required to write full lines without blocking long after starting
     a partial line.  */
  rc = _assuan_read_line (ctx);
  if (_assuan_error_is_eagain (ctx, rc))
    return 0;
  if (gpg_err_code (rc) == GPG_ERR_EOF)
    {
      ctx->process_complete = 1;
      return 0;
    }
  if (rc)
    return rc;
  if (*ctx->inbound.line == '#' || !ctx->inbound.linelen)
     /* Comment lines are ignored.  */
    return 0;

  /* Now we have a line that really means something.  It could be one
     of the following things: First, if we are not in a command
     already, it is the next command to dispatch.  Second, if we are
     in a command, it can only be the response to an INQUIRE
     reply.  */

  if (!ctx->in_command)
    {
      ctx->in_command = 1;

      ctx->outbound.data.error = 0;
      ctx->outbound.data.linelen = 0;
      /* Dispatch command and return reply.  */
      ctx->in_process_next = 1;
      rc = dispatch_command (ctx, ctx->inbound.line, ctx->inbound.linelen);
      ctx->in_process_next = 0;
    }
  else if (ctx->in_inquire)
    {
      /* FIXME: Pick up the continuation.  */
      rc = _assuan_inquire_ext_cb (ctx);
    }
  else
    {
      /* Should not happen.  The client is sending data while we are
	 in a command and not waiting for an inquire.  We log an error
	 and discard it.  */
      TRACE0 (ctx, ASSUAN_LOG_DATA, "process_next", ctx,
	      "unexpected client data");
      rc = 0;
    }

  return rc;
}


/* This function should be invoked when the assuan connected FD is
   ready for reading.  If the equivalent to EWOULDBLOCK is returned
   (this should be done by the command handler), assuan_process_next
   should be invoked the next time the connected FD is readable.
   Eventually, the caller will finish by invoking assuan_process_done.
   DONE is set to 1 if the connection has ended.  */
gpg_error_t
assuan_process_next (assuan_context_t ctx, int *done)
{
  gpg_error_t rc;

  if (done)
    *done = 0;
  ctx->process_complete = 0;
  do
    {
      rc = process_next (ctx);
    }
  while (!rc && !ctx->process_complete && assuan_pending_line (ctx));

  if (done)
    *done = !!ctx->process_complete;

  return rc;
}



static gpg_error_t
process_request (assuan_context_t ctx)
{
  gpg_error_t rc;

  if (ctx->in_inquire)
    return _assuan_error (ctx, GPG_ERR_ASS_NESTED_COMMANDS);

  do
    {
      rc = _assuan_read_line (ctx);
    }
  while (_assuan_error_is_eagain (ctx, rc));
  if (gpg_err_code (rc) == GPG_ERR_EOF)
    {
      ctx->process_complete = 1;
      return 0;
    }
  if (rc)
    return rc;
  if (*ctx->inbound.line == '#' || !ctx->inbound.linelen)
    return 0; /* comment line - ignore */

  ctx->in_command = 1;
  ctx->outbound.data.error = 0;
  ctx->outbound.data.linelen = 0;
  /* dispatch command and return reply */
  rc = dispatch_command (ctx, ctx->inbound.line, ctx->inbound.linelen);

  return assuan_process_done (ctx, rc);
}

/**
 * assuan_process:
 * @ctx: assuan context
 * 
 * This function is used to handle the assuan protocol after a
 * connection has been established using assuan_accept().  This is the
 * main protocol handler.
 * 
 * Return value: 0 on success or an error code if the assuan operation
 * failed.  Note, that no error is returned for operational errors.
 **/
gpg_error_t
assuan_process (assuan_context_t ctx)
{
  gpg_error_t rc;

  ctx->process_complete = 0;
  do {
    rc = process_request (ctx);
  } while (!rc && !ctx->process_complete);

  return rc;
}


/**
 * assuan_get_active_fds:
 * @ctx: Assuan context
 * @what: 0 for read fds, 1 for write fds
 * @fdarray: Caller supplied array to store the FDs
 * @fdarraysize: size of that array
 * 
 * Return all active filedescriptors for the given context.  This
 * function can be used to select on the fds and call
 * assuan_process_next() if there is an active one.  The first fd in
 * the array is the one used for the command connection.
 *
 * Note, that write FDs are not yet supported.
 * 
 * Return value: number of FDs active and put into @fdarray or -1 on
 * error which is most likely a too small fdarray.
 **/
int 
assuan_get_active_fds (assuan_context_t ctx, int what,
                       assuan_fd_t *fdarray, int fdarraysize)
{
  int n = 0;

  if (!ctx || fdarraysize < 2 || what < 0 || what > 1)
    return -1;

  if (!what)
    {
      if (ctx->inbound.fd != ASSUAN_INVALID_FD)
        fdarray[n++] = ctx->inbound.fd;
    }
  else
    {
      if (ctx->outbound.fd != ASSUAN_INVALID_FD)
        fdarray[n++] = ctx->outbound.fd;
      if (ctx->outbound.data.fp)
#if defined(HAVE_W32CE_SYSTEM)
        fdarray[n++] = (void*)fileno (ctx->outbound.data.fp);
#elif defined(HAVE_W32_SYSTEM)
        fdarray[n++] = (void*)_get_osfhandle (fileno (ctx->outbound.data.fp));
#else
        fdarray[n++] = fileno (ctx->outbound.data.fp);
#endif
    }

  return n;
}


/* Two simple wrappers to make the expected function types match. */
#ifdef HAVE_FUNOPEN
static int
fun1_cookie_write (void *cookie, const char *buffer, int orig_size)
{
  return _assuan_cookie_write_data (cookie, buffer, orig_size);
}
#endif /*HAVE_FUNOPEN*/
#ifdef HAVE_FOPENCOOKIE
static ssize_t
fun2_cookie_write (void *cookie, const char *buffer, size_t orig_size)
{
  return _assuan_cookie_write_data (cookie, buffer, orig_size);
}
#endif /*HAVE_FOPENCOOKIE*/

/* Return a FP to be used for data output.  The FILE pointer is valid
   until the end of a handler.  So a close is not needed.  Assuan does
   all the buffering needed to insert the status line as well as the
   required line wappping and quoting for data lines.

   We use GNU's custom streams here.  There should be an alternative
   implementaion for systems w/o a glibc, a simple implementation
   could use a child process */
FILE *
assuan_get_data_fp (assuan_context_t ctx)
{
#if defined (HAVE_FOPENCOOKIE) || defined (HAVE_FUNOPEN)
  if (ctx->outbound.data.fp)
    return ctx->outbound.data.fp;
  
#ifdef HAVE_FUNOPEN
  ctx->outbound.data.fp = funopen (ctx, 0, fun1_cookie_write,
				   0, _assuan_cookie_write_flush);
#else
  ctx->outbound.data.fp = funopen (ctx, 0, fun2_cookie_write,
				   0, _assuan_cookie_write_flush);
#endif                                   

  ctx->outbound.data.error = 0;
  return ctx->outbound.data.fp;
#else
  gpg_err_set_errno (ENOSYS);
  return NULL;
#endif
}


/* Set the text used for the next OK reponse.  This string is
   automatically reset to NULL after the next command. */
gpg_error_t
assuan_set_okay_line (assuan_context_t ctx, const char *line)
{
  if (!ctx)
    return _assuan_error (ctx, GPG_ERR_ASS_INV_VALUE);
  if (!line)
    {
      _assuan_free (ctx, ctx->okay_line);
      ctx->okay_line = NULL;
    }
  else
    {
      /* FIXME: we need to use gcry_is_secure() to test whether
         we should allocate the entire line in secure memory */
      char *buf = _assuan_malloc (ctx, 3 + strlen(line) + 1);
      if (!buf)
        return _assuan_error (ctx, gpg_err_code_from_syserror ());
      strcpy (buf, "OK ");
      strcpy (buf+3, line);
      _assuan_free (ctx, ctx->okay_line);
      ctx->okay_line = buf;
    }
  return 0;
}



gpg_error_t
assuan_write_status (assuan_context_t ctx,
                     const char *keyword, const char *text)
{
  char buffer[256];
  char *helpbuf;
  size_t n;
  gpg_error_t ae;

  if ( !ctx || !keyword)
    return _assuan_error (ctx, GPG_ERR_ASS_INV_VALUE);
  if (!text)
    text = "";

  n = 2 + strlen (keyword) + 1 + strlen (text) + 1;
  if (n < sizeof (buffer))
    {
      strcpy (buffer, "S ");
      strcat (buffer, keyword);
      if (*text)
        {
          strcat (buffer, " ");
          strcat (buffer, text);
        }
      ae = assuan_write_line (ctx, buffer);
    }
  else if ( (helpbuf = _assuan_malloc (ctx, n)) )
    {
      strcpy (helpbuf, "S ");
      strcat (helpbuf, keyword);
      if (*text)
        {
          strcat (helpbuf, " ");
          strcat (helpbuf, text);
        }
      ae = assuan_write_line (ctx, helpbuf);
      _assuan_free (ctx, helpbuf);
    }
  else
    ae = 0;
  return ae;
}
