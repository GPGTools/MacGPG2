/* system-w32.c - System support functions for Windows.
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
#include <time.h>
#include <fcntl.h>
#include <windows.h>

#include "assuan-defs.h"
#include "debug.h"



assuan_fd_t
assuan_fdopen (int fd)
{
  assuan_fd_t ifd = (assuan_fd_t) _get_osfhandle (fd);
  assuan_fd_t ofd;

  if (! DuplicateHandle(GetCurrentProcess(), ifd, 
			GetCurrentProcess(), &ofd, 0,
			TRUE, DUPLICATE_SAME_ACCESS))
    {
      gpg_err_set_errno (EIO);
      return ASSUAN_INVALID_FD;
    }
  return ofd;
}



/* Sleep for the given number of microseconds.  Default
   implementation.  */
void
__assuan_usleep (assuan_context_t ctx, unsigned int usec)
{
  if (!usec)
    return;

  Sleep (usec / 1000);
}



/* Three simple wrappers, only used because thes function are named in
   the def file.  */
HANDLE
_assuan_w32ce_prepare_pipe (int *r_rvid, int write_end)
{
  (void)r_rvid;
  (void)write_end;
  return INVALID_HANDLE_VALUE;
}

HANDLE
_assuan_w32ce_finish_pipe (int rvid, int write_end)
{
  (void)rvid;
  (void)write_end;
  return INVALID_HANDLE_VALUE;
}

DWORD
_assuan_w32ce_create_pipe (HANDLE *read_hd, HANDLE *write_hd,
                           LPSECURITY_ATTRIBUTES sec_attr, DWORD size)
{
  return CreatePipe (read_hd, write_hd, sec_attr, size);
}



/* Create a pipe with one inheritable end.  Default implementation.  */
int
__assuan_pipe (assuan_context_t ctx, assuan_fd_t fd[2], int inherit_idx)
{
  HANDLE rh;
  HANDLE wh;
  HANDLE th;
  SECURITY_ATTRIBUTES sec_attr;

  memset (&sec_attr, 0, sizeof (sec_attr));
  sec_attr.nLength = sizeof (sec_attr);
  sec_attr.bInheritHandle = FALSE;

  if (!CreatePipe (&rh, &wh, &sec_attr, 0))
    {
      TRACE1 (ctx, ASSUAN_LOG_SYSIO, "__assuan_pipe", ctx,
	      "CreatePipe failed: %s", _assuan_w32_strerror (ctx, -1));
      gpg_err_set_errno (EIO);
      return -1;
    }

  if (! DuplicateHandle (GetCurrentProcess(), (inherit_idx == 0) ? rh : wh,
			 GetCurrentProcess(), &th, 0,
			 TRUE, DUPLICATE_SAME_ACCESS ))
    {
      TRACE1 (ctx, ASSUAN_LOG_SYSIO, "__assuan_pipe", ctx,
	      "DuplicateHandle failed: %s", _assuan_w32_strerror (ctx, -1));
      CloseHandle (rh);
      CloseHandle (wh);
      gpg_err_set_errno (EIO);
      return -1;
    }
  if (inherit_idx == 0)
    {
      CloseHandle (rh);
      rh = th;
    }
  else
    {
      CloseHandle (wh);
      wh = th;
    }

  fd[0] = rh;
  fd[1] = wh;

  return 0;
}



/* Close the given file descriptor, created with _assuan_pipe or one
   of the socket functions.  Default implementation.  */
int
__assuan_close (assuan_context_t ctx, assuan_fd_t fd)
{
  int rc = closesocket (HANDLE2SOCKET(fd));
  if (rc)
    gpg_err_set_errno ( _assuan_sock_wsa2errno (WSAGetLastError ()) );
  if (rc && WSAGetLastError () == WSAENOTSOCK)
    {
      rc = CloseHandle (fd);
      if (rc)
        /* FIXME. */
        gpg_err_set_errno (EIO);
    }
  return rc;
}



/* Return true if HD refers to a socket.  */
static int
is_socket (HANDLE hd)
{
  /* We need to figure out whether we are working on a socket or on a
     handle.  A trivial way would be to check for the return code of
     recv and see if it is WSAENOTSOCK.  However the recv may block
     after the server process died and thus the destroy_reader will
     hang.  Another option is to use getsockopt to test whether it is
     a socket.  The bug here is that once a socket with a certain
     values has been opened, closed and later a CreatePipe returned
     the same value (i.e. handle), getsockopt still believes it is a
     socket.  What we do now is to use a combination of GetFileType
     and GetNamedPipeInfo.  The specs say that the latter may be used
     on anonymous pipes as well.  Note that there are claims that
     since winsocket version 2 ReadFile may be used on a socket but
     only if it is supported by the service provider.  Tests on a
     stock XP using a local TCP socket show that it does not work.  */
  DWORD dummyflags, dummyoutsize, dummyinsize, dummyinst;
  if (GetFileType (hd) == FILE_TYPE_PIPE
      && !GetNamedPipeInfo (hd, &dummyflags, &dummyoutsize,
                            &dummyinsize, &dummyinst))
    return 1; /* Function failed; thus we assume it is a socket.  */
  else
    return 0; /* Success; this is not a socket.  */
}


static ssize_t
__assuan_read (assuan_context_t ctx, assuan_fd_t fd, void *buffer, size_t size)
{
  int res;
  int ec = 0;
  
  if (is_socket (fd))
    {
      res = recv (HANDLE2SOCKET (fd), buffer, size, 0);
      if (res == -1)
        ec = WSAGetLastError ();
    }
  else
    {
       DWORD nread = 0;
       if (!ReadFile (fd, buffer, size, &nread, NULL))
         {
           res = -1;
           ec = GetLastError ();
         }
      else 
        res = nread;
    }
  if (res == -1)
    {
      switch (ec)
        {
        case WSAENOTSOCK:
	  gpg_err_set_errno (EBADF);
          break;
          
        case WSAEWOULDBLOCK:
	  gpg_err_set_errno (EAGAIN);
	  break;

        case ERROR_BROKEN_PIPE:
	  gpg_err_set_errno (EPIPE);
	  break;

        default:
	  gpg_err_set_errno (EIO);
	  break;
        }
    }
  return res;
}



static ssize_t
__assuan_write (assuan_context_t ctx, assuan_fd_t fd, const void *buffer,
		size_t size)
{
  int res;
  int ec = 0;
  
  if (is_socket (fd))
    {
      res = send (HANDLE2SOCKET (fd), buffer, size, 0);
      if (res == -1)
        ec = WSAGetLastError ();
    }
  else
    {
      DWORD nwrite;

      if (!WriteFile (fd, buffer, size, &nwrite, NULL))
        {
          res = -1;
          ec = GetLastError ();
        }
      else 
        res = (int)nwrite;
    }
  if (res == -1)
    {
      switch (ec)
        {
        case WSAENOTSOCK:
	  gpg_err_set_errno (EBADF);
          break;
          
        case WSAEWOULDBLOCK:
	  gpg_err_set_errno (EAGAIN);
	  break;

        case ERROR_BROKEN_PIPE:
        case ERROR_NO_DATA:
	  gpg_err_set_errno (EPIPE);
	  break;

        default:
	  gpg_err_set_errno (EIO);
	  break;
        }

    }
  return res;
}



static int
__assuan_recvmsg (assuan_context_t ctx, assuan_fd_t fd, assuan_msghdr_t msg,
		  int flags)
{
  gpg_err_set_errno (ENOSYS);
  return -1;
}




static int
__assuan_sendmsg (assuan_context_t ctx, assuan_fd_t fd, assuan_msghdr_t msg,
		  int flags)
{
  gpg_err_set_errno (ENOSYS);
  return -1;
}




/* Build a command line for use with W32's CreateProcess.  On success
   CMDLINE gets the address of a newly allocated string.  */
static int
build_w32_commandline (assuan_context_t ctx, const char * const *argv,
		       char **cmdline)
{
  int i, n;
  const char *s;
  char *buf, *p;

  *cmdline = NULL;
  n = 0;
  for (i=0; (s = argv[i]); i++)
    {
      n += strlen (s) + 1 + 2;  /* (1 space, 2 quoting */
      for (; *s; s++)
        if (*s == '\"')
          n++;  /* Need to double inner quotes.  */
    }
  n++;

  buf = p = _assuan_malloc (ctx, n);
  if (! buf)
    return -1;

  for (i = 0; argv[i]; i++) 
    {
      if (i)
        p = stpcpy (p, " ");
      if (! *argv[i]) /* Empty string. */
        p = stpcpy (p, "\"\"");
      else if (strpbrk (argv[i], " \t\n\v\f\""))
        {
          p = stpcpy (p, "\"");
          for (s = argv[i]; *s; s++)
            {
              *p++ = *s;
              if (*s == '\"')
                *p++ = *s;
            }
          *p++ = '\"';
          *p = 0;
        }
      else
        p = stpcpy (p, argv[i]);
    }

  *cmdline= buf;
  return 0;
}


int
__assuan_spawn (assuan_context_t ctx, pid_t *r_pid, const char *name,
		const char **argv,
		assuan_fd_t fd_in, assuan_fd_t fd_out,
		assuan_fd_t *fd_child_list,
		void (*atfork) (void *opaque, int reserved),
		void *atforkvalue, unsigned int flags)
{
  SECURITY_ATTRIBUTES sec_attr;
  PROCESS_INFORMATION pi = 
    {
      NULL,      /* Returns process handle.  */
      0,         /* Returns primary thread handle.  */
      0,         /* Returns pid.  */
      0          /* Returns tid.  */
    };
  STARTUPINFO si;
  assuan_fd_t fd;
  assuan_fd_t *fdp;
  char *cmdline;
  HANDLE nullfd = INVALID_HANDLE_VALUE;

  /* fixme: Actually we should set the "_assuan_pipe_connect_pid" env
     variable.  However this requires us to write a full environment
     handler, because the strings are expected in sorted order.  The
     suggestion given in the MS Reference Library, to save the old
     value, changeit, create proces and restore it, is not thread
     safe.  */

  /* Build the command line.  */
  if (build_w32_commandline (ctx, argv, &cmdline))
    return -1;

  /* Start the process.  */
  memset (&sec_attr, 0, sizeof sec_attr);
  sec_attr.nLength = sizeof sec_attr;
  sec_attr.bInheritHandle = FALSE;
  
  memset (&si, 0, sizeof si);
  si.cb = sizeof (si);
  si.dwFlags = STARTF_USESTDHANDLES;
  /* FIXME: Dup to nul if ASSUAN_INVALID_FD.  */
  si.hStdInput  = fd_in;
  si.hStdOutput = fd_out;

  /* Dup stderr to /dev/null unless it is in the list of FDs to be
     passed to the child. */
  fd = assuan_fd_from_posix_fd (fileno (stderr));
  fdp = fd_child_list;
  if (fdp)
    {
      for (; *fdp != ASSUAN_INVALID_FD && *fdp != fd; fdp++)
        ;
    }
  if (!fdp || *fdp == ASSUAN_INVALID_FD)
    {
      nullfd = CreateFileW (L"nul", GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE,
                           NULL, OPEN_EXISTING, 0, NULL);
      if (nullfd == INVALID_HANDLE_VALUE)
        {
	  TRACE1 (ctx, ASSUAN_LOG_SYSIO, "__assuan_spawn", ctx,
		  "can't open `nul': %s", _assuan_w32_strerror (ctx, -1));
          _assuan_free (ctx, cmdline);
          gpg_err_set_errno (EIO);
          return -1;
        }
      si.hStdError = nullfd;
    }
  else
    si.hStdError = fd;

  /* Note: We inherit all handles flagged as inheritable.  This seems
     to be a security flaw but there seems to be no way of selecting
     handles to inherit.  A fix for this would be to use a helper
     process like we have in gpgme.  */
  /*   _assuan_log_printf ("CreateProcess, path=`%s' cmdline=`%s'\n", */
  /*                       name, cmdline); */
  if (!CreateProcess (name,                 /* Program to start.  */
                      cmdline,              /* Command line arguments.  */
                      &sec_attr,            /* Process security attributes.  */
                      &sec_attr,            /* Thread security attributes.  */
                      TRUE,                 /* Inherit handles.  */
                      (CREATE_DEFAULT_ERROR_MODE
                       | ((flags & 128)? DETACHED_PROCESS : 0)
                       | GetPriorityClass (GetCurrentProcess ())
                       | CREATE_SUSPENDED), /* Creation flags.  */
                      NULL,                 /* Environment.  */
                      NULL,                 /* Use current drive/directory.  */
                      &si,                  /* Startup information. */
                      &pi                   /* Returns process information.  */
                      ))
    {
      TRACE1 (ctx, ASSUAN_LOG_SYSIO, "pipe_connect_w32", ctx,
	      "CreateProcess failed: %s", _assuan_w32_strerror (ctx, -1));
      _assuan_free (ctx, cmdline);
      if (nullfd != INVALID_HANDLE_VALUE)
        CloseHandle (nullfd);

      gpg_err_set_errno (EIO);
      return -1;
    }

  _assuan_free (ctx, cmdline);
  if (nullfd != INVALID_HANDLE_VALUE)
    CloseHandle (nullfd);

  ResumeThread (pi.hThread);
  CloseHandle (pi.hThread); 

  /*   _assuan_log_printf ("CreateProcess ready: hProcess=%p hThread=%p" */
  /*                       " dwProcessID=%d dwThreadId=%d\n", */
  /*                       pi.hProcess, pi.hThread, */
  /*                       (int) pi.dwProcessId, (int) pi.dwThreadId); */

  *r_pid = (pid_t) pi.hProcess;

  /* No need to modify peer process, as we don't change the handle
     names.  However this also means we are not safe, as we inherit
     too many handles.  Should use approach similar to gpgme and glib
     using a helper process.  */

  return 0;
}




/* FIXME: Add some sort of waitpid function that covers GPGME and
   gpg-agent's use of assuan.  */
static pid_t 
__assuan_waitpid (assuan_context_t ctx, pid_t pid, int nowait,
		  int *status, int options)
{
  CloseHandle ((HANDLE) pid);
  return 0;
}



int
__assuan_socketpair (assuan_context_t ctx, int namespace, int style,
		     int protocol, assuan_fd_t filedes[2])
{
  gpg_err_set_errno (ENOSYS);
  return -1;
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
