/* system-posix.c - System support functions.
   Copyright (C) 2009, 2010 Free Software Foundation, Inc.

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
#include <errno.h>
/* Solaris 8 needs sys/types.h before time.h.  */
#include <sys/types.h>
#include <time.h>
#include <fcntl.h>
#include <sys/wait.h>

#include "assuan-defs.h"
#include "debug.h"

#ifdef _POSIX_OPEN_MAX
#define MAX_OPEN_FDS _POSIX_OPEN_MAX
#else
#define MAX_OPEN_FDS 20
#endif




assuan_fd_t
assuan_fdopen (int fd)
{
  return dup (fd);
}



/* Sleep for the given number of microseconds.  Default
   implementation.  */
void
__assuan_usleep (assuan_context_t ctx, unsigned int usec)
{
  if (! usec)
    return;

#ifdef HAVE_NANOSLEEP
  {
    struct timespec req;
    struct timespec rem;
      
    req.tv_sec = 0;
    req.tv_nsec = usec * 1000;
  
    while (nanosleep (&req, &rem) < 0 && errno == EINTR)
      req = rem;
  }
#else
  {
    struct timeval tv;
  
    tv.tv_sec  = usec / 1000000;
    tv.tv_usec = usec % 1000000;
    select (0, NULL, NULL, NULL, &tv);
  }
#endif
}



/* Create a pipe with one inheritable end.  Easy for Posix.  */
int
__assuan_pipe (assuan_context_t ctx, assuan_fd_t fd[2], int inherit_idx)
{
  return pipe (fd);
}



/* Close the given file descriptor, created with _assuan_pipe or one
   of the socket functions.  Easy for Posix.  */
int
__assuan_close (assuan_context_t ctx, assuan_fd_t fd)
{
  return close (fd);
}



static ssize_t
__assuan_read (assuan_context_t ctx, assuan_fd_t fd, void *buffer, size_t size)
{
  return read (fd, buffer, size);
}



static ssize_t
__assuan_write (assuan_context_t ctx, assuan_fd_t fd, const void *buffer,
		size_t size)
{
  return write (fd, buffer, size);
}



static int
__assuan_recvmsg (assuan_context_t ctx, assuan_fd_t fd, assuan_msghdr_t msg,
		  int flags)
{
  int ret;

  do
    ret = recvmsg (fd, msg, flags);
  while (ret == -1 && errno == EINTR);

  return ret;
}



static int
__assuan_sendmsg (assuan_context_t ctx, assuan_fd_t fd, assuan_msghdr_t msg,
		  int flags)
{
  int ret;

  do
    ret = sendmsg (fd, msg, flags);
  while (ret == -1 && errno == EINTR);

  return ret;
}



static int
writen (int fd, const char *buffer, size_t length)
{
  while (length)
    {
      int nwritten = write (fd, buffer, length);
      
      if (nwritten < 0)
        {
          if (errno == EINTR)
            continue;
          return -1; /* write error */
        }
      length -= nwritten;
      buffer += nwritten;
    }
  return 0;  /* okay */
}


int
__assuan_spawn (assuan_context_t ctx, pid_t *r_pid, const char *name,
		const char **argv,
		assuan_fd_t fd_in, assuan_fd_t fd_out,
		assuan_fd_t *fd_child_list,
		void (*atfork) (void *opaque, int reserved),
		void *atforkvalue, unsigned int flags)
{
  int pid;

  pid = fork ();
  if (pid < 0)
    return -1;

  if (pid == 0)
    {
      /* Child process (server side).  */
      int i;
      int n;
      char errbuf[512];
      int *fdp;
      int fdnul;

      if (atfork)
	atfork (atforkvalue, 0);

      fdnul = open ("/dev/null", O_WRONLY);
      if (fdnul == -1)
	{
	  TRACE1 (ctx, ASSUAN_LOG_SYSIO, "__assuan_spawn", ctx,
		  "can't open `/dev/null': %s", strerror (errno));
	  _exit (4);
	}
      
      /* Dup handles to stdin/stdout. */
      if (fd_out != STDOUT_FILENO)
	{
	  if (dup2 (fd_out == ASSUAN_INVALID_FD ? fdnul : fd_out,
		    STDOUT_FILENO) == -1)
	    {
	      TRACE1 (ctx, ASSUAN_LOG_SYSIO, "__assuan_spawn", ctx,
		      "dup2 failed in child: %s", strerror (errno));
	      _exit (4);
	    }
	}
      
      if (fd_in != STDIN_FILENO)
	{
	  if (dup2 (fd_in == ASSUAN_INVALID_FD ? fdnul : fd_in,
		    STDIN_FILENO) == -1)
	    {
	      TRACE1 (ctx, ASSUAN_LOG_SYSIO, "__assuan_spawn", ctx,
		      "dup2 failed in child: %s", strerror (errno));
	      _exit (4);
	    }
	}
      
      /* Dup stderr to /dev/null unless it is in the list of FDs to be
	 passed to the child. */
      fdp = fd_child_list;
      if (fdp)
	{
	  for (; *fdp != -1 && *fdp != STDERR_FILENO; fdp++)
	    ;
	}
      if (!fdp || *fdp == -1)
	{
	  if (dup2 (fdnul, STDERR_FILENO) == -1)
	    {
	      TRACE1 (ctx, ASSUAN_LOG_SYSIO, "pipe_connect_unix", ctx,
		      "dup2(dev/null, 2) failed: %s", strerror (errno));
	      _exit (4);
	    }
	}
      close (fdnul);
      
      /* Close all files which will not be duped and are not in the
	 fd_child_list. */
      n = sysconf (_SC_OPEN_MAX);
      if (n < 0)
	n = MAX_OPEN_FDS;
      for (i = 0; i < n; i++)
	{
	  if (i == STDIN_FILENO || i == STDOUT_FILENO || i == STDERR_FILENO)
	    continue;
	  fdp = fd_child_list;
	  if (fdp)
	    {
	      while (*fdp != -1 && *fdp != i)
		fdp++;
	    }
	  
	  if (!(fdp && *fdp != -1))
	    close (i);
	}
      gpg_err_set_errno (0);
      
      if (! name)
	{
	  /* No name and no args given, thus we don't do an exec
	     but continue the forked process.  */
	  *argv = "server";
	  
	  /* FIXME: Cleanup.  */
	  return 0;
	}
      
      execv (name, (char *const *) argv); 
      
      /* oops - use the pipe to tell the parent about it */
      snprintf (errbuf, sizeof(errbuf)-1,
		"ERR %d can't exec `%s': %.50s\n",
		_assuan_error (ctx, GPG_ERR_ASS_SERVER_START),
		name, strerror (errno));
      errbuf[sizeof(errbuf)-1] = 0;
      writen (1, errbuf, strlen (errbuf));
      _exit (4);
    }

  if (! name)
    *argv = "client";
  
  *r_pid = pid;

  return 0;
}



/* FIXME: Add some sort of waitpid function that covers GPGME and
   gpg-agent's use of assuan.  */
static pid_t 
__assuan_waitpid (assuan_context_t ctx, pid_t pid, int nowait,
		  int *status, int options)
{
  /* We can't just release the PID, a waitpid is mandatory.  But
     NOWAIT in POSIX systems just means the caller already did the
     waitpid for this child.  */
  if (! nowait)
    return waitpid (pid, NULL, 0); 
  return 0;
}



int
__assuan_socketpair (assuan_context_t ctx, int namespace, int style,
		     int protocol, assuan_fd_t filedes[2])
{
  return socketpair (namespace, style, protocol, filedes);
}



/* The default system hooks for assuan contexts.  */
struct assuan_system_hooks _assuan_system_hooks =
  {
    ASSUAN_SYSTEM_HOOKS_VERSION,
    __assuan_usleep,
    __assuan_pipe,
    __assuan_close,
    __assuan_read,
    __assuan_write,
    __assuan_recvmsg,
    __assuan_sendmsg,
    __assuan_spawn,
    __assuan_waitpid,
    __assuan_socketpair    
  };
