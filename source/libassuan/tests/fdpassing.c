/* fdpassing - Check the fiel descriptor passing.
   Copyright (C) 2006, 2009 Free Software Foundation, Inc.

   This file is part of Assuan.

   Assuan is free software; you can redistribute it and/or modify it
   under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 3 of
   the License, or (at your option) any later version.

   Assuan is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>  /* Used by main driver. */

#include "../src/assuan.h"
#include "common.h"


/*

       S E R V E R

*/

static gpg_error_t
cmd_echo (assuan_context_t ctx, char *line)
{
  int fd;
  int c;
  FILE *fp;
  int nbytes;

  log_info ("got ECHO command (%s)\n", line);

  fd = assuan_get_input_fd (ctx);
  if (fd == -1)
    return gpg_error (GPG_ERR_ASS_NO_INPUT);
  fp = fdopen (fd, "r");
  if (!fp)
    {
      log_error ("fdopen failed on input fd: %s\n", strerror (errno));
      return gpg_error (GPG_ERR_ASS_GENERAL);
    }
  nbytes = 0;
  while ( (c=getc (fp)) != -1)
    {
      putc (c, stdout); 
      nbytes++;
    }
  fflush (stdout); 
  log_info ("done printing %d bytes to stdout\n", nbytes);

  fclose (fp);
  return 0;
}

static gpg_error_t
register_commands (assuan_context_t ctx)
{
  static struct
  {
    const char *name;
    gpg_error_t (*handler) (assuan_context_t, char *line);
  } table[] =
      {
	{ "ECHO", cmd_echo },
	{ "INPUT", NULL },
	{ "OUTPUT", NULL },
	{ NULL, NULL }
      };
  int i;
  gpg_error_t rc;

  for (i=0; table[i].name; i++)
    {
      rc = assuan_register_command (ctx, table[i].name, table[i].handler, NULL);
      if (rc)
        return rc;
    }
  return 0;
}


static void
server (void)
{
  int rc;
  assuan_context_t ctx;

  log_info ("server started\n");

  rc = assuan_new (&ctx);
  if (rc)
    log_fatal ("assuan_new failed: %s\n", gpg_strerror (rc));

  rc = assuan_init_pipe_server (ctx, NULL);
  if (rc)
    log_fatal ("assuan_init_pipe_server failed: %s\n", gpg_strerror (rc));

  rc = register_commands (ctx);
  if (rc)
    log_fatal ("register_commands failed: %s\n", gpg_strerror(rc));

  assuan_set_log_stream (ctx, stderr);

  for (;;) 
    {
      rc = assuan_accept (ctx);
      if (rc)
        {
          if (rc != -1)
            log_error ("assuan_accept failed: %s\n", gpg_strerror (rc));
          break;
        }
      
      log_info ("client connected.  Client's pid is %ld\n",
                (long)assuan_get_pid (ctx));

      rc = assuan_process (ctx);
      if (rc)
        log_error ("assuan_process failed: %s\n", gpg_strerror (rc));
    }
  
  assuan_release (ctx);
}




/*

       C L I E N T

*/


/* Client main.  If true is returned, a disconnect has not been done. */
static int
client (assuan_context_t ctx, const char *fname)
{
  int rc;
  FILE *fp;
  int i;

  log_info ("client started. Servers's pid is %ld\n",
            (long)assuan_get_pid (ctx));

  for (i=0; i < 6; i++)
    {
      fp = fopen (fname, "r");
      if (!fp)
        {
          log_error ("failed to open `%s': %s\n", fname,
                     strerror (errno));
          return -1;
        }
      
      rc = assuan_sendfd (ctx, fileno (fp));
      if (rc)
        {
          log_error ("assuan_sendfd failed: %s\n", gpg_strerror (rc));
          return -1;
        }
      fclose (fp);

      rc = assuan_transact (ctx, "INPUT FD", NULL, NULL, NULL, NULL,
                            NULL, NULL);
      if (rc)
        {
          log_error ("sending INPUT FD failed: %s\n", gpg_strerror (rc));
          return -1;
        }

      rc = assuan_transact (ctx, "ECHO", NULL, NULL, NULL, NULL, NULL, NULL);
      if (rc)
        {
          log_error ("sending ECHO failed: %s\n", gpg_strerror (rc));
          return -1;
        }
    }

  /* Give us some time to check with lsof that all descriptors are closed. */
/*   sleep (10); */

  assuan_release (ctx);
  return 0;
}




/* 
 
     M A I N

*/
int 
main (int argc, char **argv)
{
  int last_argc = -1;
  assuan_context_t ctx;
  gpg_error_t err;
  int no_close_fds[2];
  const char *arglist[10];
  int is_server = 0;
  int with_exec = 0;
  char *fname = prepend_srcdir ("motd");

  if (argc)
    {
      log_set_prefix (*argv);
      argc--; argv++;
    }
  while (argc && last_argc != argc )
    {
      last_argc = argc;
      if (!strcmp (*argv, "--help"))
        {
          puts (
"usage: ./fdpassing [options]\n"
"\n"
"Options:\n"
"  --verbose      Show what is going on\n"
"  --with-exec    Exec the child.  Default is just a fork\n"
);
          exit (0);
        }
      if (!strcmp (*argv, "--verbose"))
        {
          verbose = 1;
          argc--; argv++;
        }
      else if (!strcmp (*argv, "--debug"))
        {
          verbose = debug = 1;
          argc--; argv++;
        }
      else if (!strcmp (*argv, "--server"))
        {
          is_server = 1;
          argc--; argv++;
        }
      else if (!strcmp (*argv, "--with-exec"))
        {
          with_exec = 1;
          argc--; argv++;
        }
    }


  assuan_set_assuan_log_prefix (log_prefix);

  if (is_server)
    {
      server ();
      log_info ("server finished\n");
    }
  else
    {
      const char *loc;

      no_close_fds[0] = 2;
      no_close_fds[1] = -1;
      if (with_exec)
        {
          arglist[0] = "fdpassing";
          arglist[1] = "--server";
          arglist[2] = verbose? "--verbose":NULL;
          arglist[3] = NULL;
        }

      err = assuan_new (&ctx);
      if (err)
	log_fatal ("assuan_new failed: %s\n", gpg_strerror (err));

      err = assuan_pipe_connect (ctx, with_exec? "./fdpassing":NULL,
				 with_exec ? arglist : &loc,
				 no_close_fds, NULL, NULL, 1);
      if (err)
        {
          log_error ("assuan_pipe_connect failed: %s\n", gpg_strerror (err));
          return 1;
        }
      
      if (!with_exec && loc[0] == 's')
        {
          server ();
          log_info ("server finished\n");
        }
      else
        {
          if (client (ctx, fname)) 
            {
              log_info ("waiting for server to terminate...\n");
              assuan_release (ctx);
            }
          log_info ("client finished\n");
        }
    }

  return errorcount ? 1 : 0;
}

