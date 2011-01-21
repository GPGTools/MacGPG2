/* ce-server.c - An Assuan testbed for W32CE; server code
   Copyright (C) 2010 Free Software Foundation, Inc.

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/stat.h>
#ifdef HAVE_W32_SYSTEM
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
#else
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
#endif
#include <errno.h>

#ifdef HAVE_W32CE_SYSTEM
# ifndef FILE_ATTRIBUTE_ROMSTATICREF
# define FILE_ATTRIBUTE_ROMSTATICREF FILE_ATTRIBUTE_OFFLINE
# endif
extern BOOL GetStdioPathW (int, wchar_t *, DWORD *);
extern BOOL SetStdioPathW (int, const wchar_t *);
#endif /*!HAVE_W32CE_SYSTEM*/

#include "../src/assuan.h"

#include "common.h"

/* The port we are using by default. */
static short server_port = 15898;

/* Flag set to indicate a shutdown.  */
static int shutdown_pending;

/* An object to keep track of file descriptors.  */
struct fdinfo_s
{
  struct fdinfo_s *next;
  assuan_fd_t fd;  /* The descriptor.  */
};
typedef struct fdinfo_s *fdinfo_t;


/* The local state of a connection.  */
struct state_s
{
  /* The current working directory - access using get_cwd().  */
  char *cwd;  
  
  /* If valid, a socket in listening state created by the dataport
     command.  */
  assuan_fd_t dataport_listen_fd;

  /* If valid the socket accepted for the dataport.  */
  assuan_fd_t dataport_accepted_fd;

  /* The error code from a dataport accept operation.  */
  gpg_error_t dataport_accept_err;

  /* A list of all unused descriptors created by dataport commands.  */
  fdinfo_t dataport_fds;

  /* The descriptor set by the DATAPORT command.  */
  assuan_fd_t dataport_fd;
};
typedef struct state_s *state_t;



/* Local prototypes.  */
static gpg_error_t cmd_newdataport_cont (void *opaque, gpg_error_t err,
                                         unsigned char *data, size_t datalen);



/* A wrapper around read to make it work under Windows with HANDLES
   and socket descriptors.   Takes care of EINTR on POSIX. */
static int
my_read (assuan_fd_t fd, void *buffer, size_t size)
{
  int res;

#ifdef HAVE_W32_SYSTEM
  res = recv (HANDLE2SOCKET (fd), buffer, size, 0);
  if (res == -1)
    {
      switch (WSAGetLastError ())
        {
        case WSAENOTSOCK:
          {
            DWORD nread = 0;
            
            res = ReadFile (fd, buffer, size, &nread, NULL);
            if (!res)
              {
                switch (GetLastError ())
                  {
                  case ERROR_BROKEN_PIPE:
		    gpg_err_set_errno (EPIPE);
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
  return res;
#else	/*!HAVE_W32_SYSTEM*/
  do
    res = read (fd, buffer, size);
  while (res == -1 && errno == EINTR);
  return res;
#endif	/*!HAVE_W32_SYSTEM*/
}


/* Extended version of write(2) to guarantee that all bytes are
   written.  Returns 0 on success or -1 and ERRNO on failure.  NOTE:
   This function does not return the number of bytes written, so any
   error must be treated as fatal for this connection as the state of
   the receiver is unknown.  This works best if blocking is allowed
   (so EAGAIN cannot occur).  Under Windows this function handles
   socket descriptors and system handles.  */
static int
my_writen (assuan_fd_t fd, const char *buffer, size_t length)
{
  while (length)
    {
      int nwritten;
#ifdef HAVE_W32_SYSTEM
      nwritten = send (HANDLE2SOCKET (fd), buffer, length, 0);
      if (nwritten == -1 && WSAGetLastError () == WSAENOTSOCK)
        {
          DWORD nwrite;
          
          nwritten = WriteFile (fd, buffer, length, &nwrite, NULL);
          if (!nwritten)
            {
              switch (GetLastError ())
                {
                case ERROR_BROKEN_PIPE: 
                case ERROR_NO_DATA:
                  gpg_err_set_errno (EPIPE);
                  break;
                  
                default:
                  gpg_err_set_errno (EIO);
                  break;
                }
              nwritten= -1;
            }
          else
            nwritten = (int)nwrite;
        }
#else	/*!HAVE_W32_SYSTEM*/
      nwritten = write (fd, buffer, length);
#endif	/*!HAVE_W32_SYSTEM*/
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



static state_t 
new_state (void)
{
  state_t state = xcalloc (1, sizeof *state);
  state->dataport_listen_fd = ASSUAN_INVALID_FD;
  state->dataport_accepted_fd = ASSUAN_INVALID_FD;
  state->dataport_fd = ASSUAN_INVALID_FD;
  return state;
}
  
static void 
release_state (state_t state)
{
  fdinfo_t fi, fi2;

  if (!state)
    return;

  xfree (state->cwd);

  if (state->dataport_fd != ASSUAN_INVALID_FD)
    assuan_sock_close (state->dataport_fd);
  if (state->dataport_listen_fd != ASSUAN_INVALID_FD)
    assuan_sock_close (state->dataport_listen_fd);
  if (state->dataport_accepted_fd != ASSUAN_INVALID_FD)
    assuan_sock_close (state->dataport_accepted_fd);

  for (fi=state->dataport_fds; fi; fi = fi2)
    {
      fi2 = fi->next;
      if (fi->fd != ASSUAN_INVALID_FD)
        assuan_sock_close (fi->fd);
    }

  xfree (state);
}


/* Helper to print a message while leaving a command and to
   acknowledge the command.  */
static gpg_error_t
leave_cmd (assuan_context_t ctx, gpg_error_t err)
{
  if (err)
    {
      const char *name = assuan_get_command_name (ctx);
      if (!name)
        name = "?";
      if (gpg_err_source (err) == GPG_ERR_SOURCE_DEFAULT)
        log_error ("command '%s' failed: %s\n", name, gpg_strerror (err));
      else
        log_error ("command '%s' failed: %s <%s>\n", name,
                   gpg_strerror (err), gpg_strsource (err));
    }
  return assuan_process_done (ctx, err);
}


#ifdef HAVE_W32CE_SYSTEM
static char *
wchar_to_utf8 (const wchar_t *string)
{
  int n;
  size_t length = wcslen (string);
  char *result;

  n = WideCharToMultiByte (CP_UTF8, 0, string, length, NULL, 0, NULL, NULL);
  if (n < 0 || (n+1) <= 0)
    log_fatal ("WideCharToMultiByte failed\n");

  result = xmalloc (n+1);
  n = WideCharToMultiByte (CP_ACP, 0, string, length, result, n, NULL, NULL);
  if (n < 0)
    log_fatal ("WideCharToMultiByte failed\n");
  
  result[n] = 0;
  return result;
}

static wchar_t *
utf8_to_wchar (const char *string)
{
  int n;
  size_t length = strlen (string);
  wchar_t *result;
  size_t nbytes;

  n = MultiByteToWideChar (CP_UTF8, 0, string, length, NULL, 0);
  if (n < 0 || (n+1) <= 0)
    log_fatal ("MultiByteToWideChar failed\n");

  nbytes = (size_t)(n+1) * sizeof(*result);
  if (nbytes / sizeof(*result) != (n+1)) 
    log_fatal ("utf8_to_wchar: integer overflow\n");
  result = xmalloc (nbytes);
  n = MultiByteToWideChar (CP_UTF8, 0, string, length, result, n);
  if (n < 0)
    log_fatal ("MultiByteToWideChar failed\n");
  result[n] = 0;
  
  return result;
}
#endif /*HAVE_W32CE_SYSTEM*/

#ifndef HAVE_W32CE_SYSTEM
static char *
gnu_getcwd (void)
{
  size_t size = 100;
  
  while (1)
    {
      char *buffer = xmalloc (size);
      if (getcwd (buffer, size) == buffer)
        return buffer;
      xfree (buffer);
      if (errno != ERANGE)
        return 0;
      size *= 2;
    }
}
#endif /*!HAVE_W32CE_SYSTEM*/


/* Return the current working directory.  The returned string is valid
   as long as STATE->cwd is not changed.  */
static const char *
get_cwd (state_t state)
{
  if (!state->cwd)
    {
      /* No working directory yet.  On WindowsCE make it the module
         directory of this process.  */
#ifdef HAVE_W32_SYSTEM
      char *p;
#endif
#ifdef HAVE_W32CE_SYSTEM
      wchar_t buf[MAX_PATH+1];
      size_t n;

      n = GetModuleFileName (NULL, buf, MAX_PATH);
      if (!n)
        state->cwd = xstrdup ("/");
      else
        {
          buf[n] = 0;
          state->cwd = wchar_to_utf8 (buf);
          p = strrchr (state->cwd, '\\');
          if (p)
            *p = 0;
        }
#else
      state->cwd = gnu_getcwd ();
#endif
#ifdef HAVE_W32_SYSTEM
      for (p=state->cwd; *p; p++)
        if (*p == '\\')
          *p = '/';
#endif /*HAVE_W32_SYSTEM*/
    }

  return state->cwd;
}



static gpg_error_t
reset_notify (assuan_context_t ctx, char *line)
{
  state_t state = assuan_get_pointer (ctx);
  fdinfo_t fi, fi2;

  /* Close all lingering dataport connections.  */
  for (fi=state->dataport_fds; fi; fi = fi2)
    {
      fi2 = fi->next;
      if (fi->fd != ASSUAN_INVALID_FD)
        assuan_sock_close (fi->fd);
    }
  state->dataport_fds = NULL;

  return 0;
}


static gpg_error_t
input_notify (assuan_context_t ctx, char *line)
{
  state_t state = assuan_get_pointer (ctx);
  assuan_fd_t fd = assuan_get_input_fd (ctx);
  fdinfo_t fi;

  if (fd != ASSUAN_INVALID_FD)
    {
      /* The fd is now in use use - remove it from the unused list.  */
      for (fi=state->dataport_fds; fi; fi = fi->next)
        if (fi->fd == fd)
          fi->fd = ASSUAN_INVALID_FD;
    }

  return 0;
}


static gpg_error_t
output_notify (assuan_context_t ctx, char *line)
{
  state_t state = assuan_get_pointer (ctx);
  assuan_fd_t fd = assuan_get_output_fd (ctx);
  fdinfo_t fi;

  if (fd != ASSUAN_INVALID_FD)
    {
      /* The fd is now in use - remove it from the unused list.  */
      for (fi=state->dataport_fds; fi; fi = fi->next)
        if (fi->fd == fd)
          fi->fd = ASSUAN_INVALID_FD;
    }

  return 0;
}



static const char hlp_echo[] = 
  "ECHO <line>\n"
  "\n"
  "Print LINE as data lines.\n";
static gpg_error_t
cmd_echo (assuan_context_t ctx, char *line)
{
  gpg_error_t err;

  err = assuan_send_data (ctx, line, strlen (line));

  return leave_cmd (ctx, err);
}



static const char hlp_cat[] = 
  "CAT [<filename>]\n"
  "\n"
  "Copy the content of FILENAME to the descriptor set by the OUTPUT\n"
  "command.  If no OUTPUT command has been given, send the content\n"
  "using data lines.  Without FILENAME take the content from the\n"
  "descriptor set by the INPUT command; if a DATAPORT has been set\n"
  "this descriptor is used for I/O and the INOPUT/OUTPUT descriptors\n"
  "are not touched.";
static gpg_error_t
cmd_cat (assuan_context_t ctx, char *line)
{
  state_t state = assuan_get_pointer (ctx);
  gpg_error_t err = 0;
  assuan_fd_t fd_in = ASSUAN_INVALID_FD;
  assuan_fd_t fd_out = ASSUAN_INVALID_FD;
  FILE *fp_in = NULL;
  char buf[256];
  size_t nread;
  int use_dataport = 0;

  if (*line)
    {
      fp_in = fopen (line, "rb");
      if (!fp_in)
        err = gpg_error_from_syserror ();
      else
        fd_out = assuan_get_output_fd (ctx);
    }
  else if (state->dataport_fd != ASSUAN_INVALID_FD)
    {
      use_dataport = 1;
      fd_in = state->dataport_fd;
      fd_out = state->dataport_fd;
    }
  else if ((fd_in = assuan_get_input_fd (ctx)) != ASSUAN_INVALID_FD)
    {
      /* This FD is actually a socket descriptor.  We can't fdopen it
         because under Windows we ust use recv(2) instead of read(2).
         Note that on POSIX systems there is no difference between
         libc file descriptors and socket descriptors.  */

      fd_out = assuan_get_output_fd (ctx);
    }
  else
    err = gpg_error (GPG_ERR_ASS_NO_INPUT);
  if (err)
    goto leave;

  do
    {
      if (fp_in)
        {
          nread = fread (buf, 1, sizeof buf, fp_in);
          if (nread < sizeof buf)
            {
              if (ferror (fp_in))
                err = gpg_error_from_syserror ();
              else if (feof (fp_in))
                err = gpg_error (GPG_ERR_EOF);
            }
        }
      else
        {
          int n;

          nread = 0;
          n = my_read (fd_in, buf, sizeof buf);
          if (n < 0)
            err = gpg_error_from_syserror ();
          else if (!n)
            err = gpg_error (GPG_ERR_EOF);
          else
            nread = n;
        }
      

      if (fd_out != ASSUAN_INVALID_FD)
        {
          if (nread && my_writen (fd_out, buf, nread))
            err = gpg_error_from_syserror ();
        }
      else if (nread)
        err = assuan_send_data (ctx, buf, nread);
    }
  while (!err);
  if (gpg_err_code (err) == GPG_ERR_EOF)
    err = 0;

leave:
  if (fp_in)
    fclose (fp_in);
  if (use_dataport)
    {
      if (state->dataport_fd != ASSUAN_INVALID_FD)
        {
          assuan_sock_close (state->dataport_fd);
          state->dataport_fd = ASSUAN_INVALID_FD;
        }
    }
  else
    {
      assuan_close_input_fd (ctx);
      assuan_close_output_fd (ctx);
    }
  return leave_cmd (ctx, err);
}


static const char hlp_pwd[] = 
  "PWD\n"
  "\n"
  "Print the curent working directory of this session.\n";
static gpg_error_t
cmd_pwd (assuan_context_t ctx, char *line)
{
  state_t state = assuan_get_pointer (ctx);
  gpg_error_t err;
  const char *string;
  
  string = get_cwd (state);
  err = assuan_send_data (ctx, string, strlen (string));

  return leave_cmd (ctx, err);
}


static const char hlp_cd[] = 
  "CD [dir]\n"
  "\n"
  "Change the curretn directory of the session.\n";
static gpg_error_t
cmd_cd (assuan_context_t ctx, char *line)
{
  state_t state = assuan_get_pointer (ctx);
  gpg_error_t err = 0;
  char *newdir, *p;

  for (p=line; *p; p++)
    if (*p == '\\')
      *p = '/';

  if (!*line)
    {
      xfree (state->cwd);
      state->cwd = NULL;
      get_cwd (state);
    }
  else
    {
      if (*line == '/')
        newdir = xstrdup (line);
      else
        newdir = xstrconcat (get_cwd (state), "/", line, NULL);
      
      while (strlen(newdir) > 1 && line[strlen(newdir)-1] == '/')
        line[strlen(newdir)-1] = 0;
      xfree (state->cwd);
      state->cwd = newdir;
    }

  return leave_cmd (ctx, err);
}





#ifdef HAVE_W32CE_SYSTEM
static const char hlp_ls[] = 
  "LS [<pattern>]\n"
  "\n"
  "List the files described by PATTERN.\n";
static gpg_error_t
cmd_ls (assuan_context_t ctx, char *line)
{
  state_t state = assuan_get_pointer (ctx);
  gpg_error_t err;
  WIN32_FIND_DATA fi;
  char buf[500];
  HANDLE hd;
  char *p, *fname;
  wchar_t *wfname;

  if (!*line)
    fname = xstrconcat (get_cwd (state), "/*", NULL);
  else if (*line == '/' || *line == '\\')
    fname = xstrdup (line);
  else
    fname = xstrconcat (get_cwd (state), "/", line, NULL);
  for (p=fname; *p; p++)
    if (*p == '/')
      *p = '\\';
  assuan_write_status (ctx, "PATTERN", fname);
  wfname = utf8_to_wchar (fname);
  xfree (fname);
  hd = FindFirstFile (wfname, &fi);
  free (wfname);
  if (hd == INVALID_HANDLE_VALUE)
    {
      log_info ("FindFirstFile returned %d\n", GetLastError ());
      err = gpg_error_from_syserror ();  /* Works for W32CE.  */
      goto leave;
    }

  do
    {
      DWORD attr = fi.dwFileAttributes;

      fname = wchar_to_utf8 (fi.cFileName);
      snprintf (buf, sizeof buf, 
                "%c%c%c%c%c%c%c%c%c%c%c%c%c %7lu%c %s\n",
                (attr & FILE_ATTRIBUTE_DIRECTORY)
                ? ((attr & FILE_ATTRIBUTE_DEVICE)? 'c':'d'):'-',
                (attr & FILE_ATTRIBUTE_READONLY)? 'r':'-',
                (attr & FILE_ATTRIBUTE_HIDDEN)? 'h':'-',
                (attr & FILE_ATTRIBUTE_SYSTEM)? 's':'-',
                (attr & FILE_ATTRIBUTE_ARCHIVE)? 'a':'-',
                (attr & FILE_ATTRIBUTE_COMPRESSED)? 'c':'-',
                (attr & FILE_ATTRIBUTE_ENCRYPTED)? 'e':'-',
                (attr & FILE_ATTRIBUTE_INROM)? 'R':'-',
                (attr & FILE_ATTRIBUTE_REPARSE_POINT)? 'P':'-',
                (attr & FILE_ATTRIBUTE_ROMMODULE)? 'M':'-',
                (attr & FILE_ATTRIBUTE_ROMSTATICREF)? 'R':'-',
                (attr & FILE_ATTRIBUTE_SPARSE_FILE)? 'S':'-',
                (attr & FILE_ATTRIBUTE_TEMPORARY)? 't':'-',
                (unsigned long)fi.nFileSizeLow,
                fi.nFileSizeHigh? 'X':' ',
                fname);
      free (fname);
      err = assuan_send_data (ctx, buf, strlen (buf));
      if (!err)
        err = assuan_send_data (ctx, NULL, 0);
    }
  while (!err && FindNextFile (hd, &fi));
  if (err)
    ;
  else if (GetLastError () == ERROR_NO_MORE_FILES)
    err = 0;
  else
    {
      log_info ("FindNextFile returned %d\n", GetLastError ());
      err = gpg_error_from_syserror (); 
    }
  FindClose (hd);

 leave:
  return leave_cmd (ctx, err);
}
#endif /*HAVE_W32CE_SYSTEM*/


#ifdef HAVE_W32CE_SYSTEM
static const char hlp_run[] = 
  "RUN <filename> [<args>]\n"
  "\n"
  "Run the program in FILENAME with the arguments ARGS.\n"
  "This creates a new process and waits for it to finish.\n"
  "FIXME: The process' stdin is connected to the file set by the\n"
  "INPUT command; stdout and stderr to the one set by OUTPUT.\n";
static gpg_error_t
cmd_run (assuan_context_t ctx, char *line)
{
  /*  state_t state = assuan_get_pointer (ctx); */
  gpg_error_t err;
  BOOL w32ret;
  PROCESS_INFORMATION pi = { NULL, 0, 0, 0 };
  char *p;
  wchar_t *pgmname = NULL;
  wchar_t *cmdline = NULL;
  int code;
  DWORD exc;
  int idx;
  struct {
    HANDLE hd[2];
    int oldname_valid;
    wchar_t oldname[MAX_PATH];
  } pipes[3];

  for (idx=0; idx < 3; idx++)
    {
      pipes[idx].hd[0] = pipes[idx].hd[1] = INVALID_HANDLE_VALUE;
      pipes[idx].oldname_valid = 0;
    }

  p = strchr (line, ' ');
  if (p)
    {
      *p = 0;
      pgmname = utf8_to_wchar (line);
      for (p++; *p && *p == ' '; p++)
        ;
      cmdline = utf8_to_wchar (p);
    }
  else
    pgmname = utf8_to_wchar (line);
  {
    char *tmp1 = wchar_to_utf8 (pgmname);
    char *tmp2 = wchar_to_utf8 (cmdline);
    log_info ("CreateProcess, path=`%s' cmdline=`%s'\n", tmp1, tmp2);
    xfree (tmp2);
    xfree (tmp1);
  }

  /* Redirect the standard handles.  */
  /* Create pipes.  */
  for (idx=0; idx < 3; idx++)
    {
      if (!_assuan_w32ce_create_pipe (&pipes[idx].hd[0], &pipes[idx].hd[1],
                                      NULL, 0))
        {
          err = gpg_error_from_syserror ();
          log_error ("CreatePipe failed: %d\n", GetLastError ());
          pipes[idx].hd[0] = pipes[idx].hd[1] = INVALID_HANDLE_VALUE;
          goto leave;
        }
    }

  /* Save currently assigned devices.  */
  for (idx=0; idx < 3; idx++)
    {
      DWORD dwlen = MAX_PATH;
      if (!GetStdioPathW (idx, pipes[idx].oldname, &dwlen))
        {
          err = gpg_error_from_syserror ();
          log_error ("GetStdioPath failed: %d\n", GetLastError ());
          goto leave;
        }
      pipes[idx].oldname_valid = 1;
    }

  /* Connect the pipes.  */
    {
      if (!SetStdioPathW (1, L"\\mystdout.log"))
        {
          err = gpg_error_from_syserror ();
          log_error ("SetStdioPathW(%d) failed: %d\n", idx, GetLastError ());
          goto leave;
        }
      if (!SetStdioPathW (2, L"\\mystderr.log"))
        {
          err = gpg_error_from_syserror ();
          log_error ("SetStdioPathW(%d) failed: %d\n", idx, GetLastError ());
          goto leave;
        }
    }

  /* Create the process, restore the devices and check the error.  */
  w32ret = CreateProcess (pgmname,     /* Program to start.  */
                          cmdline,     /* Command line arguments.  */
                          NULL,        /* Process security.  Not used. */
                          NULL,        /* Thread security.  Not used. */
                          FALSE,       /* Inherit handles.  Not used.  */
                          CREATE_SUSPENDED, /* Creation flags.  */
                          NULL,        /* Environment.  Not used.  */
                          NULL,        /* Use current dir.  Not used. */
                          NULL,        /* Startup information.  Not used. */
                          &pi          /* Returns process information.  */
                          );
  for (idx=0; idx < 3; idx++)
    {
      if (pipes[idx].oldname_valid)
        {
          if (!SetStdioPathW (idx, pipes[idx].oldname))
            log_error ("SetStdioPath(%d) failed during restore: %d\n",
                       idx, GetLastError ());
          else
            pipes[idx].oldname_valid = 0;
        }
    }
  if (!w32ret)
    {
      /* Error checking after restore so that the messages are visible.  */
      log_error ("CreateProcess failed: %d\n", GetLastError ());
      err = gpg_error_from_syserror ();
      goto leave;
    }

  log_info ("CreateProcess ready: hProcess=%p hThread=%p" 
            " dwProcessID=%d dwThreadId=%d\n", 
            pi.hProcess, pi.hThread,
            (int) pi.dwProcessId, (int) pi.dwThreadId);

  ResumeThread (pi.hThread);
  CloseHandle (pi.hThread); 

  code = WaitForSingleObject (pi.hProcess, INFINITE);
  switch (code) 
    {
      case WAIT_FAILED:
        err = gpg_error_from_syserror ();;
        log_error ("waiting for process %d to terminate failed: %d\n",
                   (int)pi.dwProcessId, GetLastError ());
        break;

      case WAIT_OBJECT_0:
        if (!GetExitCodeProcess (pi.hProcess, &exc))
          {
            err = gpg_error_from_syserror ();;
            log_error ("error getting exit code of process %d: %s\n",
                       (int)pi.dwProcessId, GetLastError () );
          }
        else if (exc)
          {
            log_info ("error running process: exit status %d\n", (int)exc);
            err = gpg_error (GPG_ERR_GENERAL);
          }
        else
          {
            err = 0;
          }
        break;
        
      default:
        err = gpg_error_from_syserror ();;
        log_error ("WaitForSingleObject returned unexpected "
                   "code %d for pid %d\n", code, (int)pi.dwProcessId);
        break;
    }
  CloseHandle (pi.hProcess);
  
 leave:
  for (idx=0; idx < 3; idx++)
    {
      if (pipes[idx].oldname_valid)
        {
          if (!SetStdioPathW (idx, pipes[idx].oldname))
            log_error ("SetStdioPath(%d) failed during restore: %d\n",
                       idx, GetLastError ());
          else
            pipes[idx].oldname_valid = 0;
        }
    }
  for (idx=0; idx < 3; idx++)
    {
      if (pipes[idx].hd[0] != INVALID_HANDLE_VALUE)
        CloseHandle (pipes[idx].hd[0]);
      if (pipes[idx].hd[1] != INVALID_HANDLE_VALUE)
        CloseHandle (pipes[idx].hd[1]);
    }
  xfree (cmdline);
  xfree (pgmname);
  return leave_cmd (ctx, err);
}
#endif /*HAVE_W32CE_SYSTEM*/





static const char hlp_newdataport[] = 
  "NEWDATAPORT\n"
  "\n"
  "Create a new dataport.  The server creates a listening socket and\n"
  "issues the inquiry:\n"
  "  INQUIRE CONNECT-TO <port>\n"
  "The client is expected to connect to PORT of the server and confirm\n"
  "this by sending just an \"END\".  In turn the server sends:\n"
  "  S FDINFO <n>\n"
  "With N being the local descriptor for the accepted connection.  This\n"
  "descriptor may now be used with INPUT or OUTPUT commands.";
struct cmd_dataport_locals
{
  assuan_context_t ctx;
  int passthru;
  int port;
};
static gpg_error_t
cmd_newdataport (assuan_context_t ctx, char *line)
{
  state_t state = assuan_get_pointer (ctx);
  gpg_error_t err = 0;
  struct sockaddr_in addr;
  socklen_t addrlen;
  struct cmd_dataport_locals *cont;
  char inqline[100];

  cont = xmalloc (sizeof *cont);
  cont->ctx = ctx;
  cont->passthru = 0;
  cont->port = 0;

  if (state->dataport_listen_fd != ASSUAN_INVALID_FD)
    {
      log_error ("Oops, still listening on a dataport socket\n");
      state->dataport_listen_fd = ASSUAN_INVALID_FD;
    }
  if (state->dataport_accepted_fd != ASSUAN_INVALID_FD)
    {
      log_error ("Oops, still holding an accepted dataport socket\n");
      state->dataport_accepted_fd = ASSUAN_INVALID_FD;
    }
  state->dataport_accept_err = 0;

  state->dataport_listen_fd = assuan_sock_new (PF_INET, SOCK_STREAM, 0);
  if (state->dataport_listen_fd == ASSUAN_INVALID_FD)
    {
      err = gpg_error_from_syserror ();
      log_error ("socket() failed: %s\n", strerror (errno));
      goto leave;
    }

  addr.sin_family = AF_INET;
  addr.sin_port = 0;
  addr.sin_addr.s_addr = htonl (INADDR_ANY);
  if (assuan_sock_bind (state->dataport_listen_fd,
                        (struct sockaddr *)&addr, sizeof addr))
    {
      err = gpg_error_from_syserror ();
      log_error ("listen() failed: %s\n", strerror (errno));
      goto leave;
    }
  
  if (listen (HANDLE2SOCKET (state->dataport_listen_fd), 1))
    {
      err = gpg_error_from_syserror ();
      log_error ("listen() failed: %s\n", strerror (errno));
      goto leave;
    }

  addrlen = sizeof addr;
  if (getsockname (HANDLE2SOCKET (state->dataport_listen_fd),
                   (struct sockaddr *)&addr, &addrlen))
    {
      err = gpg_error_from_syserror ();
      log_error ("getsockname() failed: %s\n", strerror (errno));
      goto leave;
    }
  cont->port = ntohs (addr.sin_port);

  if (verbose)
    log_info ("server now also listening on port %d\n", cont->port);
  snprintf (inqline, sizeof inqline, "CONNECT-TO %d", cont->port);
  err = assuan_inquire_ext (ctx, inqline, 0, cmd_newdataport_cont, cont);
  if (!err)
    return 0; /* Transfer to continuation.  */

 leave:
  cont->passthru = 1;
  return cmd_newdataport_cont (cont, err, NULL, 0);
}

/* Continuation used by cmd_newdataport.  */
static gpg_error_t
cmd_newdataport_cont (void *opaque, gpg_error_t err,
                      unsigned char *data, size_t datalen)
{
  struct cmd_dataport_locals *cont = opaque;
  assuan_context_t ctx  = cont->ctx;
  state_t state = assuan_get_pointer (ctx);
  char numbuf[35];
  fdinfo_t fi;

  if (cont->passthru || err)
    goto leave;

  err = state->dataport_accept_err;
  if (err)
    goto leave;
  if (state->dataport_listen_fd != ASSUAN_INVALID_FD
      || state->dataport_accepted_fd == ASSUAN_INVALID_FD)
    {
      err = gpg_error (GPG_ERR_MISSING_ACTION);
      goto leave;
    }

  for (fi = state->dataport_fds; fi; fi = fi->next)
    if (fi->fd == ASSUAN_INVALID_FD)
      break;
  if (!fi)
    {
      fi = xcalloc (1, sizeof *fi);
      fi->next = state->dataport_fds;
      state->dataport_fds = fi;
    }
  fi->fd = state->dataport_accepted_fd;
  state->dataport_accepted_fd = ASSUAN_INVALID_FD;

  /* Note that under Windows the FD is the socket descriptor.  Socket
     descriptors are neither handles nor libc file descriptors.  */
  snprintf (numbuf, sizeof numbuf, "%d", HANDLE2SOCKET (fi->fd));
  err = assuan_write_status (ctx, "FDINFO", numbuf);

 leave:
  if (state->dataport_listen_fd != ASSUAN_INVALID_FD)
    {
      assuan_sock_close (state->dataport_listen_fd);
      state->dataport_listen_fd = ASSUAN_INVALID_FD;
    }
  if (state->dataport_accepted_fd != ASSUAN_INVALID_FD)
    {
      assuan_sock_close (state->dataport_accepted_fd);
      state->dataport_accepted_fd = ASSUAN_INVALID_FD;
    }
  xfree (cont);
  return leave_cmd (ctx, err);
}



static const char hlp_dataport[] =
  "DATAPORT FD[=<n>]\n"
  "\n"
  "Set the file descriptor to read and write data via port.\n"
  "This is similar to the \"INPUT\" and \"OUTPUT\" commands\n"
  "but useful for socketpairs.";
static gpg_error_t
cmd_dataport (assuan_context_t ctx, char *line)
{
  state_t state = assuan_get_pointer (ctx);
  gpg_error_t err;
  assuan_fd_t fd;

  if (state->dataport_fd != ASSUAN_INVALID_FD)
    {
      assuan_sock_close (state->dataport_fd);
      state->dataport_fd = ASSUAN_INVALID_FD;
    }

  err = assuan_command_parse_fd (ctx, line, &fd);
  if (!err && fd != ASSUAN_INVALID_FD)
    {
      fdinfo_t fi;

      state->dataport_fd = fd;

      /* The fd is now in use use - remove it from the unused list.  */
      for (fi=state->dataport_fds; fi; fi = fi->next)
        if (fi->fd == fd)
          fi->fd = ASSUAN_INVALID_FD;
    }

  return leave_cmd (ctx, err);
}



static const char hlp_getinfo[] = 
  "GETINFO <what>\n"
  "\n"
  "Multipurpose function to return a variety of information.\n"
  "Supported values for WHAT are:\n"
  "\n"
  "  version     - Return the version of the program.\n"
  "  pid         - Return the process id of the server.\n"
  "  dataports   - Return a list of usused dataports.";
static gpg_error_t
cmd_getinfo (assuan_context_t ctx, char *line)
{
  state_t state = assuan_get_pointer (ctx);
  gpg_error_t err = 0;
  char numbuf[50];

  if (!strcmp (line, "version"))
    {
      const char *s = VERSION;
      err = assuan_send_data (ctx, s, strlen (s));
    }
  else if (!strcmp (line, "pid"))
    {
      snprintf (numbuf, sizeof numbuf, "%lu", (unsigned long)getpid ());
      err = assuan_send_data (ctx, numbuf, strlen (numbuf));
    }
  else if (!strcmp (line, "dataports"))
    {
      fdinfo_t fi;
      int any = 0;

      for (fi=state->dataport_fds; !err && fi; fi = fi->next)
        {
          if (fi->fd != ASSUAN_INVALID_FD)
            {
              snprintf (numbuf, sizeof numbuf, "%s%d",
                        any? " ":"", HANDLE2SOCKET (fi->fd));
              any = 1;
              err = assuan_send_data (ctx, numbuf, strlen (numbuf));
            }
        }
    }
  else
    err = gpg_error (GPG_ERR_ASS_PARAMETER);

  return leave_cmd (ctx, err);
}



static const char hlp_shutdown[] = 
  "SHUTDOWN\n"
  "\n"
  "Shutdown the server process\n";
static gpg_error_t
cmd_shutdown (assuan_context_t ctx, char *line)
{
  (void)line;
  shutdown_pending = 1;
  return leave_cmd (ctx, 0);;
}


static gpg_error_t
register_commands (assuan_context_t ctx)
{
  static struct
  {
    const char *name;
    gpg_error_t (*handler) (assuan_context_t, char *line);
    const char * const help;
  } table[] =
      {
#ifdef HAVE_W32CE_SYSTEM
        { "LS",   cmd_ls, hlp_ls },
        { "RUN",  cmd_run, hlp_run },
#endif
        { "PWD",  cmd_pwd, hlp_pwd },
        { "CD",   cmd_cd,  hlp_cd },
	{ "ECHO", cmd_echo, hlp_echo },
        { "CAT",  cmd_cat, hlp_cat },
        { "NEWDATAPORT", cmd_newdataport, hlp_newdataport },
        { "DATAPORT",    cmd_dataport,    hlp_dataport },
	{ "INPUT", NULL },
	{ "OUTPUT", NULL },
        { "GETINFO",  cmd_getinfo, hlp_getinfo },
	{ "SHUTDOWN", cmd_shutdown, hlp_shutdown },
	{ NULL, NULL }
      };
  int i;
  gpg_error_t rc;

  for (i=0; table[i].name; i++)
    {
      rc = assuan_register_command (ctx, table[i].name, 
                                    table[i].handler, table[i].help);
      if (rc)
        return rc;
    }
  return 0;
}



static assuan_fd_t
get_connection_fd (assuan_context_t ctx)
{
  assuan_fd_t fds[5];

  if (assuan_get_active_fds (ctx, 0, fds, DIM (fds)) < 1)
    log_fatal ("assuan_get_active_fds failed\n");
  if (fds[0] == ASSUAN_INVALID_FD)
    log_fatal ("assuan_get_active_fds returned invalid conenction fd\n");
  return fds[0];
}


/* Startup the server.  */
static void
server (void)
{
  gpg_error_t err;
  assuan_fd_t server_fd;
  assuan_sock_nonce_t server_nonce;
  int one = 1;
  struct sockaddr_in name;
  assuan_context_t ctx;
  state_t state = NULL;

  err = assuan_new (&ctx);
  if (err)
    log_fatal ("assuan_new failed: %s\n", gpg_strerror (err));

  server_fd = assuan_sock_new (PF_INET, SOCK_STREAM, 0);
  if (server_fd == ASSUAN_INVALID_FD)
    log_fatal ("socket() failed: %s\n", strerror (errno));

  if (setsockopt (HANDLE2SOCKET (server_fd), 
                  SOL_SOCKET, SO_REUSEADDR, (void*)&one, sizeof one))
    log_error ("setsockopt(SO_REUSEADDR) failed: %s\n", strerror (errno));

  name.sin_family = AF_INET;
  name.sin_port = htons (server_port);
  name.sin_addr.s_addr = htonl (INADDR_ANY);
  if (assuan_sock_bind (server_fd, (struct sockaddr *) &name, sizeof name))
    log_fatal ("bind() failed: %s\n", strerror (errno));
  if (assuan_sock_get_nonce ((struct sockaddr*)&name, sizeof name, 
                             &server_nonce))
    log_fatal ("assuan_sock_get_nonce failed: %s\n", strerror (errno));

  /* Register the nonce with the context so that assuan_accept knows
     about it.  We can't do that directly in assuan_sock_bind because
     we want these socket wrappers to be context neutral and drop in
     replacement for the standard socket functions.  */
  assuan_set_sock_nonce (ctx, &server_nonce);

  if (listen (HANDLE2SOCKET (server_fd), 5))
    log_fatal ("listen() failed: %s\n", strerror (errno));

  log_info ("server listening on port %hd\n", server_port);

  err = assuan_init_socket_server (ctx, server_fd, 0);
  if (err)
    log_fatal ("assuan_init_socket_server failed: %s\n", gpg_strerror (err));

  err = register_commands (ctx);
  if (err)
    log_fatal ("register_commands failed: %s\n", gpg_strerror(err));

  if (debug)
    assuan_set_log_stream (ctx, stderr);

  assuan_register_reset_notify (ctx, reset_notify);
  assuan_register_input_notify (ctx, input_notify);
  assuan_register_output_notify (ctx, output_notify);

  
  state = new_state ();
  
  assuan_set_pointer (ctx, state);

  while (!shutdown_pending)
    {
      int done;

      err = assuan_accept (ctx);
      if (err)
        {
          if (gpg_err_code (err) == GPG_ERR_EOF || err == -1)
            log_error ("assuan_accept failed: %s\n", gpg_strerror (err));
          break;
        }
      
      log_info ("client connected.  Client's pid is %ld\n",
                (long)assuan_get_pid (ctx));
      do
        {
          /* We need to use select here so that we can accept
             supplemental connections from the client as requested by
             the DATAPORT command.  */
          fd_set rfds;
          int connfd, datafd, max_fd;

          connfd = HANDLE2SOCKET (get_connection_fd (ctx));
          FD_ZERO (&rfds);
          FD_SET (connfd, &rfds);
          max_fd = connfd;

          if (state->dataport_listen_fd != ASSUAN_INVALID_FD)
            {
              datafd = HANDLE2SOCKET (state->dataport_listen_fd);
              FD_SET (datafd, &rfds);
              if (datafd > max_fd)
                max_fd = datafd;
            }
          else
            datafd = -1;

          if (select (max_fd + 1, &rfds, NULL, NULL, NULL) > 0)
            {
              if (datafd != -1 && FD_ISSET (datafd, &rfds))
                {
                  struct sockaddr_in clnt_addr;
                  socklen_t len = sizeof clnt_addr;
                  int fd;

                  fd = accept (datafd, (struct sockaddr*)&clnt_addr, &len);
                  if (fd == -1)
                    {
                      err = gpg_err_code_from_syserror ();
                      assuan_sock_close (state->dataport_listen_fd);
                      state->dataport_listen_fd = ASSUAN_INVALID_FD;
                      log_error ("accepting on dataport failed: %s\n",
                                 gpg_strerror (err));
                      state->dataport_accept_err = err;
                      err = 0;
                    }
                  else
                    {
                      /* No more need for the listening socket.  */
                      assuan_sock_close (state->dataport_listen_fd);
                      state->dataport_listen_fd = ASSUAN_INVALID_FD;
                      /* Record the accepted fd.  */
                      state->dataport_accept_err = 0;
                      state->dataport_accepted_fd = SOCKET2HANDLE (fd);
                    }
                }

              if (FD_ISSET (connfd, &rfds))
                {
                  err = assuan_process_next (ctx, &done);
                }
            }
        }
      while (!err && !done && !shutdown_pending);
      if (err)
        log_error ("assuan_process failed: %s\n", gpg_strerror (err));
    }
  
  assuan_sock_close (server_fd);
  assuan_release (ctx);
  release_state (state);
}





/* 
 
     M A I N

*/
int 
main (int argc, char **argv)
{
  gpg_error_t err;
  int last_argc = -1;

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
          printf (
                  "usage: %s [options]\n"
                  "\n"
                  "Options:\n"
                  "  --verbose      Show what is going on\n",
                  log_get_prefix ());
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
    }

  assuan_set_assuan_log_prefix (log_prefix);
  if (debug)
    assuan_set_assuan_log_stream (stderr);

  err = assuan_sock_init ();
  if (err)
    log_fatal ("assuan_sock_init failed: %s\n", gpg_strerror (err));

  log_info ("server starting...\n");
  server ();
  log_info ("server finished\n");

  assuan_sock_deinit ();

  return errorcount ? 1 : 0;
}

