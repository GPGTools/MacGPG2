/* system-w32ce.c - System support functions for WindowsCE.
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

#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <windows.h>
#include <winioctl.h>
#include <devload.h>

#include "assuan-defs.h"
#include "debug.h"


#define GPGCEDEV_IOCTL_GET_RVID                                         \
  CTL_CODE (FILE_DEVICE_STREAMS, 2048, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define GPGCEDEV_IOCTL_MAKE_PIPE                                        \
  CTL_CODE (FILE_DEVICE_STREAMS, 2049, METHOD_BUFFERED, FILE_ANY_ACCESS)




static wchar_t *
utf8_to_wchar (const char *string)
{
  int n;
  size_t nbytes;
  wchar_t *result;

  if (!string)
    return NULL;

  n = MultiByteToWideChar (CP_UTF8, 0, string, -1, NULL, 0);
  if (n < 0)
    {
      gpg_err_set_errno (EINVAL);
      return NULL;
    }

  nbytes = (size_t)(n+1) * sizeof(*result);
  if (nbytes / sizeof(*result) != (n+1)) 
    {
      gpg_err_set_errno (ENOMEM);
      return NULL;
    }
  result = malloc (nbytes);
  if (!result)
    return NULL;

  n = MultiByteToWideChar (CP_UTF8, 0, string, -1, result, n);
  if (n < 0)
    {
      free (result);
      gpg_err_set_errno (EINVAL);
      result = NULL;
    }
  return result;
}

/* Convenience function.  */
static void
free_wchar (wchar_t *string)
{
  if (string)
    free (string);
}



assuan_fd_t
assuan_fdopen (int fd)
{
  return (assuan_fd_t)fd;
}



/* Sleep for the given number of microseconds.  Default
   implementation.  */
void
__assuan_usleep (assuan_context_t ctx, unsigned int usec)
{
  if (usec)
    Sleep (usec / 1000);
}



/* Prepare a pipe.  Returns a handle which is, depending on WRITE_END,
   will either act the read or as the write end of the pipe.  The
   other value returned is a rendezvous id used to complete the pipe
   creation with _assuan_w32ce_finish_pipe.  The rendezvous id may be
   passed to another process and that process may finish the pipe
   creation.  This creates the interprocess pipe.  The rendezvous id
   is not a handle but a plain number; there is no release function
   and care should be taken not to pass it to a function expecting a
   handle.  */
HANDLE
_assuan_w32ce_prepare_pipe (int *r_rvid, int write_end)
{
  HANDLE hd;
  LONG rvid;

  ActivateDevice (L"Drivers\\GnuPG_Device", 0);

  /* Note: Using "\\$device\\GPG1" should be identical to "GPG1:".
     However this returns an invalid parameter error without having
     called GPG_Init in the driver.  The docs mention something about
     RegisterAFXEx but that API is not documented.  */
  hd = CreateFile (L"GPG1:", write_end? GENERIC_WRITE : GENERIC_READ,
                   FILE_SHARE_READ | FILE_SHARE_WRITE,
                   NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (hd != INVALID_HANDLE_VALUE)
    {
      if (!DeviceIoControl (hd, GPGCEDEV_IOCTL_GET_RVID,
                            NULL, 0, &rvid, sizeof rvid, NULL, NULL))
        {
          DWORD lastrc = GetLastError ();
          CloseHandle (hd);
          hd = INVALID_HANDLE_VALUE;
          SetLastError (lastrc);
        }
      else
        *r_rvid = rvid;
    }
  
  return hd;
}


/* Create a pipe.  WRITE_END shall have the opposite value of the one
   pssed to _assuan_w32ce_prepare_pipe; see there for more
   details.  */
HANDLE
_assuan_w32ce_finish_pipe (int rvid, int write_end)
{
  HANDLE hd;

  if (!rvid)
    {
      SetLastError (ERROR_INVALID_HANDLE);
      return INVALID_HANDLE_VALUE;
    }

  hd = CreateFile (L"GPG1:", write_end? GENERIC_WRITE : GENERIC_READ,
                   FILE_SHARE_READ | FILE_SHARE_WRITE,
                   NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL,NULL);
  if (hd != INVALID_HANDLE_VALUE)
    {
      if (!DeviceIoControl (hd, GPGCEDEV_IOCTL_MAKE_PIPE,
                            &rvid, sizeof rvid, NULL, 0, NULL, NULL))
        {
          DWORD lastrc = GetLastError ();
          CloseHandle (hd);
          hd = INVALID_HANDLE_VALUE;
          SetLastError (lastrc);
        }
    }

  return hd;
}


/* WindowsCE does not provide a pipe feature.  However we need
   something like a pipe to convey data between processes and in some
   cases within a process.  This replacement is not only used by
   libassuan but exported and thus usable by gnupg and gpgme as well.  */
DWORD
_assuan_w32ce_create_pipe (HANDLE *read_hd, HANDLE *write_hd,
                           LPSECURITY_ATTRIBUTES sec_attr, DWORD size)
{
  HANDLE hd[2] = {INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE};
  int rvid;
  int rc = 0;

  hd[0] = _assuan_w32ce_prepare_pipe (&rvid, 0);
  if (hd[0] != INVALID_HANDLE_VALUE)
    {
      hd[1] = _assuan_w32ce_finish_pipe (rvid, 1);
      if (hd[1] != INVALID_HANDLE_VALUE)
        rc = 1;
      else
        {
          DWORD lastrc = GetLastError ();
          CloseHandle (hd[0]);
          hd[0] = INVALID_HANDLE_VALUE;
          SetLastError (lastrc);
        }
    }
  
  *read_hd = hd[0];
  *write_hd = hd[1];
  return rc;
}


/* Create a pipe with one inheritable end.  Default implementation.
   If INHERIT_IDX is 0, the read end of the pipe is made inheritable;
   with INHERIT_IDX is 1 the write end will be inheritable.  The
   question now is how we create an inheritable pipe end under windows
   CE were handles are process local objects?  The trick we employ is
   to defer the actual creation to the other end: We create an
   incomplete pipe and pass a rendezvous id to the other end
   (process).  The other end now uses the rendezvous id to lookup the
   pipe in our device driver, creates a new handle and uses that one
   to finally establish the pipe.  */
int
__assuan_pipe (assuan_context_t ctx, assuan_fd_t fd[2], int inherit_idx)
{
  HANDLE hd;
  int rvid;

  hd = _assuan_w32ce_prepare_pipe (&rvid, !inherit_idx);
  if (hd == INVALID_HANDLE_VALUE)
    {
      TRACE1 (ctx, ASSUAN_LOG_SYSIO, "__assuan_pipe", ctx,
	      "CreatePipe failed: %s", _assuan_w32_strerror (ctx, -1));
      gpg_err_set_errno (EIO);
      return -1;
    }

  if (inherit_idx)
    {
      fd[0] = hd;
      fd[1] = (void*)rvid;
    }
  else
    {
      fd[0] = (void*)rvid;
      fd[1] = hd;
    }
  return 0;
}



/* Close the given file descriptor, created with _assuan_pipe or one
   of the socket functions.  Default implementation.  */
int
__assuan_close (assuan_context_t ctx, assuan_fd_t fd)
{
  int rc = closesocket (HANDLE2SOCKET(fd));
  int err = WSAGetLastError ();

  /* Note that gpg_err_set_errno on Windows CE overwrites
     WSAGetLastError() (via SetLastError()).  */
  if (rc)
    gpg_err_set_errno (_assuan_sock_wsa2errno (err));
  if (rc && err == WSAENOTSOCK)
    {
      rc = CloseHandle (fd);
      if (rc)
        /* FIXME. */
        gpg_err_set_errno (EIO);
    }
  return rc;
}



static ssize_t
__assuan_read (assuan_context_t ctx, assuan_fd_t fd, void *buffer, size_t size)
{
  /* Due to the peculiarities of the W32 API we can't use read for a
     network socket and thus we try to use recv first and fallback to
     read if recv detects that it is not a network socket.  */
  int res;

  TRACE_BEG3 (ctx, ASSUAN_LOG_SYSIO, "__assuan_read", ctx,
	      "fd=0x%x, buffer=%p, size=%i", fd, buffer, size);

#ifdef HAVE_W32CE_SYSTEM
  /* This is a bit of a hack to support stdin over ssh.  Note that
     fread buffers fully while getchar is line buffered.  Weird, but
     that's the way it is.  ASSUAN_STDIN and ASSUAN_STDOUT are
     special handle values that shouldn't occur in the wild.  */
  if (fd == ASSUAN_STDIN)
    {
      int i = 0;
      int chr;
      while (i < size)
	{
	  chr = getchar();
	  if (chr == EOF)
	    break;
	  ((char*)buffer)[i++] = (char) chr;
	  if (chr == '\n')
	    break;
	}
      return TRACE_SYSRES (i);
    }
#endif

  res = recv (HANDLE2SOCKET (fd), buffer, size, 0);
  if (res == -1)
    {
      TRACE_LOG1 ("recv failed: rc=%d", (int)WSAGetLastError ());
      switch (WSAGetLastError ())
        {
        case WSAENOTSOCK:
          {
            DWORD nread = 0;
            
            res = ReadFile (fd, buffer, size, &nread, NULL);
            if (! res)
              {
                TRACE_LOG1 ("ReadFile failed: rc=%d", (int)GetLastError ());
                switch (GetLastError ())
                  {
                  case ERROR_BROKEN_PIPE:
		    gpg_err_set_errno (EPIPE);
		    break;

                  case ERROR_PIPE_NOT_CONNECTED:
                  case ERROR_BUSY:
		    gpg_err_set_errno (EAGAIN);
		    break;

                  default:
		    gpg_err_set_errno (EIO); 
                  }
                res = -1;
              }
            else
              res = (int) nread;
          }
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
  return TRACE_SYSRES (res);
}



static ssize_t
__assuan_write (assuan_context_t ctx, assuan_fd_t fd, const void *buffer,
		size_t size)
{
  /* Due to the peculiarities of the W32 API we can't use write for a
     network socket and thus we try to use send first and fallback to
     write if send detects that it is not a network socket.  */
  int res;

  TRACE_BEG3 (ctx, ASSUAN_LOG_SYSIO, "__assuan_write", ctx,
	      "fd=0x%x, buffer=%p, size=%i", fd, buffer, size);

#ifdef HAVE_W32CE_SYSTEM
  /* This is a bit of a hack to support stdout over ssh.  Note that
     fread buffers fully while getchar is line buffered.  Weird, but
     that's the way it is.  ASSUAN_STDIN and ASSUAN_STDOUT are
     special handle values that shouldn't occur in the wild.  */
  if (fd == ASSUAN_STDOUT)
    {
      res = fwrite (buffer, 1, size, stdout);
      return TRACE_SYSRES (res);
    }
#endif

  res = send ((int)fd, buffer, size, 0);
  if (res == -1 && WSAGetLastError () == WSAENOTSOCK)
    {
      DWORD nwrite;

      TRACE_LOG ("send call failed - trying WriteFile");
      res = WriteFile (fd, buffer, size, &nwrite, NULL);
      if (! res)
        {
          TRACE_LOG1 ("WriteFile failed: rc=%d", (int)GetLastError ());
          switch (GetLastError ())
            {
            case ERROR_BROKEN_PIPE: 
            case ERROR_NO_DATA:
	      gpg_err_set_errno (EPIPE);
	      break;

            case ERROR_PIPE_NOT_CONNECTED:
            case ERROR_BUSY:
              gpg_err_set_errno (EAGAIN);
              break;
	      
            default:
	      gpg_err_set_errno (EIO);
	      break;
            }
          res = -1;
        }
      else
        res = (int) nwrite;
    }
  else if (res == -1)
    TRACE_LOG1 ("send call failed: rc=%d", (int)GetLastError ());
  return TRACE_SYSRES (res);
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
		       assuan_fd_t fd0, assuan_fd_t fd1, assuan_fd_t fd2,
                       int fd2_isnull,
                       char **cmdline)
{
  int i, n;
  const char *s;
  char *buf, *p;
  char fdbuf[3*30];

  p = fdbuf;
  *p = 0;
  if (fd0 != ASSUAN_INVALID_FD)
    {
      snprintf (p, 25, "-&S0=%d ", (int)fd0);
      p += strlen (p);
    }
  if (fd1 != ASSUAN_INVALID_FD)
    {
      snprintf (p, 25, "-&S1=%d ", (int)fd1);
      p += strlen (p);
    }
  if (fd2 != ASSUAN_INVALID_FD)
    {
      if (fd2_isnull)
        strcpy (p, "-&S2=null ");
      else
        snprintf (p, 25, "-&S2=%d ", (int)fd2);
      p += strlen (p);
    }
  
  *cmdline = NULL;
  n = strlen (fdbuf);
  for (i=0; (s = argv[i]); i++)
    {
      if (!i)
        continue; /* Ignore argv[0].  */
      n += strlen (s) + 1 + 2;  /* (1 space, 2 quoting) */
      for (; *s; s++)
        if (*s == '\"')
          n++;  /* Need to double inner quotes.  */
    }
  n++;

  buf = p = _assuan_malloc (ctx, n);
  if (! buf)
    return -1;

  p = stpcpy (p, fdbuf);
  for (i = 0; argv[i]; i++) 
    {
      if (!i)
        continue; /* Ignore argv[0].  */
      if (i > 1)
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

  *cmdline = buf;
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
  PROCESS_INFORMATION pi = 
    {
      NULL,      /* Returns process handle.  */
      0,         /* Returns primary thread handle.  */
      0,         /* Returns pid.  */
      0          /* Returns tid.  */
    };
  assuan_fd_t fd;
  assuan_fd_t *fdp;
  assuan_fd_t fd_err;
  int fd_err_isnull = 0;
  char *cmdline;

  /* Dup stderr to /dev/null unless it is in the list of FDs to be
     passed to the child.  Well we don't actually open nul because
     that is not available on Windows, but use our hack for it.
     Because an RVID of 0 is an invalid value and HANDLES will never
     have this value either, we test for this as well.  */

  /* FIXME: As long as we can't decide whether a handle is a real
     handler or an rendezvous id we can't do anything with the
     FD_CHILD_LIST.  We can't do much with stderr either, thus we
     better don't pass stderr to the child at all.  If we would do so
     and it is not a rendezvous id the client would run into
     problems.  */
  fd = assuan_fd_from_posix_fd (fileno (stderr));
  fdp = fd_child_list;
  if (fdp)
    {
      for (; *fdp != ASSUAN_INVALID_FD && *fdp != 0 && *fdp != fd; fdp++)
        ;
    }
  if (!fdp || *fdp == ASSUAN_INVALID_FD)
    fd_err_isnull = 1;
  fd_err = ASSUAN_INVALID_FD;

  if (build_w32_commandline (ctx, argv, fd_in, fd_out, fd_err, fd_err_isnull,
                             &cmdline))
    {
      return -1;
    }

  TRACE2 (ctx, ASSUAN_LOG_SYSIO, "__assuan_spawn", ctx,
          "path=`%s' cmdline=`%s'", name, cmdline);

  {
    wchar_t *wcmdline, *wname;

    wcmdline = utf8_to_wchar (cmdline);
    _assuan_free (ctx, cmdline);
    if (!wcmdline)
      return -1;

    wname = utf8_to_wchar (name);
    if (!wname)
      {
        free_wchar (wcmdline);
        return -1;
      }
    
    if (!CreateProcess (wname,                /* Program to start.  */
                        wcmdline,             /* Command line arguments.  */
                        NULL,                 /* (not supported)  */
                        NULL,                 /* (not supported)  */
                        FALSE,                /* (not supported)  */
                        (CREATE_SUSPENDED),   /* Creation flags.  */
                        NULL,                 /* (not supported)  */
                        NULL,                 /* (not supported)  */
                        NULL,                 /* (not supported) */
                        &pi                   /* Returns process information.*/
                        ))
      {
        TRACE1 (ctx, ASSUAN_LOG_SYSIO, "__assuan_spawn", ctx,
                "CreateProcess failed: %s", _assuan_w32_strerror (ctx, -1));
        free_wchar (wname);
        free_wchar (wcmdline);
        gpg_err_set_errno (EIO);
        return -1;
      }
    free_wchar (wname);
    free_wchar (wcmdline);
  }

  ResumeThread (pi.hThread);

  TRACE4 (ctx, ASSUAN_LOG_SYSIO, "__assuan_spawn", ctx,
          "CreateProcess ready: hProcess=%p hThread=%p"
          " dwProcessID=%d dwThreadId=%d\n",
          pi.hProcess, pi.hThread, (int) pi.dwProcessId, (int) pi.dwThreadId);

  CloseHandle (pi.hThread); 
  
  *r_pid = (pid_t) pi.hProcess;
  return 0;
}




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
