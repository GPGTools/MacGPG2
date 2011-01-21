/* assuan-pipe-connect.c - Establish a pipe connection (client) 
   Copyright (C) 2001-2003, 2005-2007, 2009 Free Software Foundation, Inc.

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
/* On Windows systems signal.h is not needed and even not supported on
   WindowsCE. */
#ifndef HAVE_DOSISH_SYSTEM 
# include <signal.h>
#endif
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#ifndef HAVE_W32_SYSTEM
#include <sys/wait.h>
#else
#include <windows.h>
#endif

#include "assuan-defs.h"
#include "debug.h"

/* Hacks for Slowaris.  */
#ifndef PF_LOCAL
# ifdef PF_UNIX
#  define PF_LOCAL PF_UNIX
# else
#  define PF_LOCAL AF_UNIX
# endif
#endif
#ifndef AF_LOCAL
# define AF_LOCAL AF_UNIX
#endif


#ifdef _POSIX_OPEN_MAX
#define MAX_OPEN_FDS _POSIX_OPEN_MAX
#else
#define MAX_OPEN_FDS 20
#endif


/* This should be called to make sure that SIGPIPE gets ignored.  */
static void
fix_signals (void)
{
#ifndef HAVE_DOSISH_SYSTEM  /* No SIGPIPE for these systems.  */
  static int fixed_signals;

  if (!fixed_signals)
    { 
      struct sigaction act;
        
      sigaction (SIGPIPE, NULL, &act);
      if (act.sa_handler == SIG_DFL)
	{
	  act.sa_handler = SIG_IGN;
	  sigemptyset (&act.sa_mask);
	  act.sa_flags = 0;
	  sigaction (SIGPIPE, &act, NULL);
        }
      fixed_signals = 1;
      /* FIXME: This is not MT safe */
    }
#endif /*HAVE_DOSISH_SYSTEM*/
}


/* Helper for pipe_connect. */
static gpg_error_t
initial_handshake (assuan_context_t ctx)
{
  assuan_response_t response;
  int off;
  gpg_error_t err;
  
  err = _assuan_read_from_server (ctx, &response, &off);
  if (err)
    TRACE1 (ctx, ASSUAN_LOG_SYSIO, "initial_handshake", ctx,
	    "can't connect server: %s", gpg_strerror (err));
  else if (response != ASSUAN_RESPONSE_OK)
    {
      TRACE1 (ctx, ASSUAN_LOG_SYSIO, "initial_handshake", ctx,
	      "can't connect server: `%s'", ctx->inbound.line);
      err = _assuan_error (ctx, GPG_ERR_ASS_CONNECT_FAILED);
    }

  return err;
}


struct at_pipe_fork
{
  void (*user_atfork) (void *opaque, int reserved);
  void *user_atforkvalue;
};


static void
at_pipe_fork_cb (void *opaque, int reserved)
{
  struct at_pipe_fork *atp = opaque;
          
  if (atp->user_atfork)
    atp->user_atfork (atp->user_atforkvalue, reserved);

#ifndef HAVE_W32_SYSTEM
  {
    char mypidstr[50];
    
    /* We store our parents pid in the environment so that the execed
       assuan server is able to read the actual pid of the client.
       The server can't use getppid because it might have been double
       forked before the assuan server has been initialized. */
    sprintf (mypidstr, "%lu", (unsigned long) getpid ());
    setenv ("_assuan_pipe_connect_pid", mypidstr, 1);

    /* Make sure that we never pass a connection fd variable when
       using a simple pipe.  */
    unsetenv ("_assuan_connection_fd");
  }
#endif
}


static gpg_error_t
pipe_connect (assuan_context_t ctx,
	      const char *name, const char **argv,
	      assuan_fd_t *fd_child_list,
	      void (*atfork) (void *opaque, int reserved),
	      void *atforkvalue, unsigned int flags)
{
  gpg_error_t rc;
  assuan_fd_t rp[2];
  assuan_fd_t wp[2];
  pid_t pid;
  int res;
  struct at_pipe_fork atp;
  unsigned int spawn_flags;

  atp.user_atfork = atfork;
  atp.user_atforkvalue = atforkvalue;

  if (!ctx || !name || !argv || !argv[0])
    return _assuan_error (ctx, GPG_ERR_ASS_INV_VALUE);

  if (! ctx->flags.no_fixsignals)
    fix_signals ();

  if (_assuan_pipe (ctx, rp, 1) < 0)
    return _assuan_error (ctx, gpg_err_code_from_syserror ());
  
  if (_assuan_pipe (ctx, wp, 0) < 0)
    {
      _assuan_close (ctx, rp[0]);
      _assuan_close_inheritable (ctx, rp[1]);
      return _assuan_error (ctx, gpg_err_code_from_syserror ());
    }
  
  spawn_flags = 0;
  if (flags & ASSUAN_PIPE_CONNECT_DETACHED)
    spawn_flags |= ASSUAN_SPAWN_DETACHED;

  /* FIXME: Use atfork handler that closes child fds on Unix.  */
  res = _assuan_spawn (ctx, &pid, name, argv, wp[0], rp[1],
		       fd_child_list, at_pipe_fork_cb, &atp, spawn_flags);
  if (res < 0)
    {
      rc = gpg_err_code_from_syserror ();
      _assuan_close (ctx, rp[0]);
      _assuan_close_inheritable (ctx, rp[1]);
      _assuan_close_inheritable (ctx, wp[0]);
      _assuan_close (ctx, wp[1]);
      return _assuan_error (ctx, rc);
    }

  /* Close the stdin/stdout child fds in the parent.  */
  _assuan_close_inheritable (ctx, rp[1]);
  _assuan_close_inheritable (ctx, wp[0]);

  ctx->engine.release = _assuan_client_release;
  ctx->engine.readfnc = _assuan_simple_read;
  ctx->engine.writefnc = _assuan_simple_write;
  ctx->engine.sendfd = NULL;
  ctx->engine.receivefd = NULL;
  ctx->finish_handler = _assuan_client_finish;
  ctx->max_accepts = 1;
  ctx->accept_handler = NULL;
  ctx->inbound.fd  = rp[0];  /* Our inbound is read end of read pipe. */
  ctx->outbound.fd = wp[1];  /* Our outbound is write end of write pipe. */
  ctx->pid = pid;

  rc = initial_handshake (ctx);
  if (rc)
    _assuan_reset (ctx);
  return rc;
}


/* FIXME: For socketpair_connect, use spawn function and add atfork
   handler to do the right thing.  Instead of stdin and stdout, we
   extend the fd_child_list by fds[1].  */

#ifndef HAVE_W32_SYSTEM
struct at_socketpair_fork
{
  assuan_fd_t peer_fd;
  void (*user_atfork) (void *opaque, int reserved);
  void *user_atforkvalue;
};


static void
at_socketpair_fork_cb (void *opaque, int reserved)
{
  struct at_socketpair_fork *atp = opaque;
          
  if (atp->user_atfork)
    atp->user_atfork (atp->user_atforkvalue, reserved);

#ifndef HAVE_W32_SYSTEM
  {
    char mypidstr[50];
    
    /* We store our parents pid in the environment so that the execed
       assuan server is able to read the actual pid of the client.
       The server can't use getppid because it might have been double
       forked before the assuan server has been initialized. */
    sprintf (mypidstr, "%lu", (unsigned long) getpid ());
    setenv ("_assuan_pipe_connect_pid", mypidstr, 1);

    /* Now set the environment variable used to convey the
       connection's file descriptor.  */
    sprintf (mypidstr, "%d", atp->peer_fd);
    if (setenv ("_assuan_connection_fd", mypidstr, 1))
      _exit (4);
  }
#endif
}


/* This function is similar to pipe_connect but uses a socketpair and
   sets the I/O up to use sendmsg/recvmsg. */
static gpg_error_t
socketpair_connect (assuan_context_t ctx,
                    const char *name, const char **argv,
                    assuan_fd_t *fd_child_list,
                    void (*atfork) (void *opaque, int reserved),
                    void *atforkvalue)
{
  gpg_error_t err;
  int idx;
  int fds[2];
  char mypidstr[50];
  pid_t pid;
  int *child_fds = NULL;
  int child_fds_cnt = 0;
  struct at_socketpair_fork atp;
  int rc;

  TRACE_BEG3 (ctx, ASSUAN_LOG_CTX, "socketpair_connect", ctx,
	      "name=%s,atfork=%p,atforkvalue=%p", name ? name : "(null)",
	      atfork, atforkvalue);

  atp.user_atfork = atfork;
  atp.user_atforkvalue = atforkvalue;

  if (!ctx
      || (name && (!argv || !argv[0]))
      || (!name && !argv))
    return _assuan_error (ctx, GPG_ERR_ASS_INV_VALUE);

  if (! ctx->flags.no_fixsignals)
    fix_signals ();

  sprintf (mypidstr, "%lu", (unsigned long)getpid ());

  if (fd_child_list)
    while (fd_child_list[child_fds_cnt] != ASSUAN_INVALID_FD)
      child_fds_cnt++;
  child_fds = _assuan_malloc (ctx, (child_fds_cnt + 2) * sizeof (int));
  if (! child_fds)
    return TRACE_ERR (gpg_err_code_from_syserror ());
  child_fds[1] = ASSUAN_INVALID_FD;
  if (fd_child_list)
    memcpy (&child_fds[1], fd_child_list, (child_fds_cnt + 1) * sizeof (int));
  
  if (_assuan_socketpair (ctx, AF_LOCAL, SOCK_STREAM, 0, fds))
    {
      TRACE_LOG1 ("socketpair failed: %s", strerror (errno));
      _assuan_free (ctx, child_fds);
      return TRACE_ERR (GPG_ERR_ASS_GENERAL);
    }
  atp.peer_fd = fds[1];
  child_fds[0] = fds[1];

  rc = _assuan_spawn (ctx, &pid, name, argv, ASSUAN_INVALID_FD,
		      ASSUAN_INVALID_FD, child_fds, at_socketpair_fork_cb,
		      &atp, 0);
  if (rc < 0)
    {
      err = gpg_err_code_from_syserror ();
      _assuan_close (ctx, fds[0]);
      _assuan_close (ctx, fds[1]);
      _assuan_free (ctx, child_fds);
      return TRACE_ERR (err);
    }

  /* For W32, the user needs to know the server-local names of the
     inherited handles.  Return them here.  Note that the translation
     of the peer socketpair fd (fd_child_list[0]) must be done by the
     wrapper program based on the environment variable
     _assuan_connection_fd.  */
  if (fd_child_list)
    {
      for (idx = 0; fd_child_list[idx] != -1; idx++)
	/* We add 1 to skip over the socketpair end.  */
	fd_child_list[idx] = child_fds[idx + 1];
    }

  /* If this is the server child process, exit early.  */
  if (! name && (*argv)[0] == 's')
    {
      _assuan_free (ctx, child_fds);
      _assuan_close (ctx, fds[0]);
      return 0;
    }

  _assuan_close (ctx, fds[1]);

  ctx->engine.release = _assuan_client_release;
  ctx->finish_handler = _assuan_client_finish;
  ctx->max_accepts = 1;
  ctx->inbound.fd  = fds[0]; 
  ctx->outbound.fd = fds[0]; 
  _assuan_init_uds_io (ctx);
  
  err = initial_handshake (ctx);
  if (err)
    _assuan_reset (ctx);
  return err;
}
#endif /*!HAVE_W32_SYSTEM*/


/* Connect to a server over a full-duplex socket (i.e. created by
   socketpair), creating the assuan context and returning it in CTX.
   The server filename is NAME, the argument vector in ARGV.
   FD_CHILD_LIST is a -1 terminated list of file descriptors not to
   close in the child.  ATFORK is called in the child right after the
   fork; ATFORKVALUE is passed as the first argument and 0 is passed
   as the second argument. The ATFORK function should only act if the
   second value is 0.

   FLAGS is a bit vector and controls how the function acts:
   Bit 0: If cleared a simple pipe based server is expected and the
          function behaves similar to `assuan_pipe_connect'.

          If set a server based on full-duplex pipes is expected. Such
          pipes are usually created using the `socketpair' function.
          It also enables features only available with such servers.

   Bit 7: If set and there is a need to start the server it will be
          started as a background process.  This flag is useful under
          W32 systems, so that no new console is created and pops up a
          console window when starting the server


   If NAME is NULL, no exec is done but the same process is continued.
   However all file descriptors are closed and some special
   environment variables are set. To let the caller detect whether the
   child or the parent continues, the child returns "client" or
   "server" in *ARGV (but it is sufficient to check only the first
   character).  This feature is only available on POSIX platforms.  */
gpg_error_t
assuan_pipe_connect (assuan_context_t ctx,
		     const char *name, const char *argv[],
		     assuan_fd_t *fd_child_list,
		     void (*atfork) (void *opaque, int reserved),
		     void *atforkvalue, unsigned int flags)
{
  TRACE2 (ctx, ASSUAN_LOG_CTX, "assuan_pipe_connect", ctx,
	  "name=%s, flags=0x%x", name ? name : "(null)", flags);

  if (flags & ASSUAN_PIPE_CONNECT_FDPASSING)
    {
#ifdef HAVE_W32_SYSTEM
      return _assuan_error (ctx, GPG_ERR_NOT_IMPLEMENTED);
#else
      return socketpair_connect (ctx, name, argv, fd_child_list,
                                 atfork, atforkvalue);
#endif
    }
  else
    return pipe_connect (ctx, name, argv, fd_child_list, atfork, atforkvalue,
                         flags);
}

