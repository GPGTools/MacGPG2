/* assuan-defs.h - Internal definitions to Assuan
   Copyright (C) 2001, 2002, 2004, 2005, 2007, 2008,
                 2009, 2010 Free Software Foundation, Inc.

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

#ifndef ASSUAN_DEFS_H
#define ASSUAN_DEFS_H

#include <sys/types.h>
#ifndef HAVE_W32_SYSTEM
#include <sys/socket.h>
#include <sys/un.h>
#else
#include <windows.h>
#endif
#include <unistd.h>

#include "assuan.h"

#if __GNUC__ > 2 
# define ASSUAN_GCC_A_PURE  __attribute__ ((__pure__))
#else
# define ASSUAN_GCC_A_PURE
#endif

#ifndef HAVE_W32_SYSTEM
#define DIRSEP_C '/'
#else
#define DIRSEP_C '\\'
#endif

#define LINELENGTH ASSUAN_LINELENGTH

/* Generate an error code specific to a context.  */
#define _assuan_error(ctx, errcode) gpg_err_make ((ctx)->err_source, errcode)


struct cmdtbl_s
{
  const char *name;
  assuan_handler_t handler;
  const char *helpstr;
};



/* The context we use with most functions. */
struct assuan_context_s
{
  /* Members managed by the generic routines in assuan.c.  */

  /* The error source for errors generated from this context.  */
  gpg_err_source_t err_source;

#ifdef HAVE_W32_SYSTEM
  /* The per-context w32 error string.  */
  char w32_strerror[256];
#endif

  /* The allocation hooks.  */
  struct assuan_malloc_hooks malloc_hooks;

  /* Logging callback handler.  */
  assuan_log_cb_t log_cb;
  void *log_cb_data;

  void *user_pointer;

  /* Context specific flags (cf. assuan_flag_t). */
  struct
  {
    unsigned int no_waitpid : 1;
    unsigned int confidential : 1;
    unsigned int no_fixsignals : 1;
  } flags;

  /* If set, this is called right before logging an I/O line.  */
  assuan_io_monitor_t io_monitor;
  void *io_monitor_data;

  /* Callback handlers replacing system I/O functions.  */
  struct assuan_system_hooks system;

  int peercred_valid;   /* Whether this structure has valid information. */
  struct _assuan_peercred peercred;

  /* Now come the members specific to subsystems or engines.  FIXME:
     This is not developed yet.  See below for the legacy members.  */
  struct
  {
    void (*release) (assuan_context_t ctx);

    /* Routine to read from input_fd.  Sets errno on failure.  */
    ssize_t (*readfnc) (assuan_context_t, void *, size_t);
    /* Routine to write to output_fd.  Sets errno on failure.  */
    ssize_t (*writefnc) (assuan_context_t, const void *, size_t);
    /* Send a file descriptor.  */
    gpg_error_t (*sendfd) (assuan_context_t, assuan_fd_t);
    /* Receive a file descriptor.  */
    gpg_error_t (*receivefd) (assuan_context_t, assuan_fd_t *);
  } engine;


  /* Engine specific or other subsystem members.  */

  /* assuan-logging.c.  Does not require deallocation from us.  */
  FILE *log_fp;

  /* assuan-util.c  */
  gpg_error_t err_no;
  const char *err_str;

  int is_server;      /* Set if this is context belongs to a server */
  int in_inquire;
  int in_process_next;
  int process_complete;
  int in_command;

  /* The following members are used by assuan_inquire_ext.  */
  gpg_error_t (*inquire_cb) (void *cb_data, gpg_error_t rc,
			     unsigned char *buf, size_t len);
  void *inquire_cb_data;
  void *inquire_membuf;

  char *hello_line;
  char *okay_line;    /* See assuan_set_okay_line() */


  struct {
    assuan_fd_t fd;
    int eof;
    char line[LINELENGTH];
    int linelen;  /* w/o CR, LF - might not be the same as
                     strlen(line) due to embedded nuls. However a nul
                     is always written at this pos. */
    struct {
      char line[LINELENGTH];
      int linelen ;
      int pending; /* i.e. at least one line is available in the attic */
    } attic;
  } inbound;

  struct {
    assuan_fd_t fd;
    struct {
      FILE *fp;
      char line[LINELENGTH];
      int linelen;
      int error;
    } data;
  } outbound;

  int max_accepts;  /* If we can not handle more than one connection,
		       set this to 1, otherwise to -1.  */
  pid_t pid;	  /* The pid of the peer. */
  assuan_fd_t listen_fd;  /* The fd we are listening on (used by
                             socket servers) */
  assuan_sock_nonce_t listen_nonce; /* Used with LISTEN_FD.  */
  assuan_fd_t connected_fd; /* helper */

  /* Used for Unix domain sockets.  */
  struct sockaddr_un myaddr;
  struct sockaddr_un serveraddr;

  /* Structure used for unix domain sockets.  */
  struct {
    assuan_fd_t pendingfds[5]; /* Array to save received descriptors.  */
    int pendingfdscount;  /* Number of received descriptors. */
  } uds;

  gpg_error_t (*accept_handler)(assuan_context_t);
  void (*finish_handler)(assuan_context_t);

  struct cmdtbl_s *cmdtbl;
  size_t cmdtbl_used; /* used entries */
  size_t cmdtbl_size; /* allocated size of table */
  
  /* The name of the command currently processed by a command handler.
     This is a pointer into CMDTBL.  NULL if not in a command
     handler.  */
  const char *current_cmd_name;

  assuan_handler_t bye_notify_fnc;
  assuan_handler_t reset_notify_fnc;
  assuan_handler_t cancel_notify_fnc;
  gpg_error_t  (*option_handler_fnc)(assuan_context_t,const char*, const char*);
  assuan_handler_t input_notify_fnc;
  assuan_handler_t output_notify_fnc;

  /* This function is called right after a command has been processed.
     It may be used to command related cleanup.  */
  void (*post_cmd_notify_fnc)(assuan_context_t, gpg_error_t);


  assuan_fd_t input_fd;   /* Set by the INPUT command.  */
  assuan_fd_t output_fd;  /* Set by the OUTPUT command.  */
};


/* Release all resources associated with an engine operation.  */
void _assuan_reset (assuan_context_t ctx);

/* Default log handler.  */
int _assuan_log_handler (assuan_context_t ctx, void *hook,
			  unsigned int cat, const char *msg);


/* Manage memory specific to a context.  */
void *_assuan_malloc (assuan_context_t ctx, size_t cnt);
void *_assuan_realloc (assuan_context_t ctx, void *ptr, size_t cnt);
void *_assuan_calloc (assuan_context_t ctx, size_t cnt, size_t elsize);
void _assuan_free (assuan_context_t ctx, void *ptr);

/* System hooks.  */
void _assuan_usleep (assuan_context_t ctx, unsigned int usec);
int _assuan_pipe (assuan_context_t ctx, assuan_fd_t fd[2], int inherit_idx);
int _assuan_close (assuan_context_t ctx, assuan_fd_t fd);
int _assuan_close_inheritable (assuan_context_t ctx, assuan_fd_t fd);
ssize_t _assuan_read (assuan_context_t ctx, assuan_fd_t fd, void *buffer,
		      size_t size);
ssize_t _assuan_write (assuan_context_t ctx, assuan_fd_t fd, const void *buffer,
		       size_t size);
int _assuan_recvmsg (assuan_context_t ctx, assuan_fd_t fd,
		     assuan_msghdr_t msg, int flags);
int _assuan_sendmsg (assuan_context_t ctx, assuan_fd_t fd,
		     assuan_msghdr_t msg, int flags);
int _assuan_spawn (assuan_context_t ctx, pid_t *r_pid, const char *name,
		   const char *argv[],
		   assuan_fd_t fd_in, assuan_fd_t fd_out,
		   assuan_fd_t *fd_child_list,
		   void (*atfork) (void *opaque, int reserved),
		   void *atforkvalue, unsigned int flags);
pid_t  _assuan_waitpid (assuan_context_t ctx, pid_t pid, int nowait,
			int *status, int options);
int _assuan_socketpair (assuan_context_t ctx, int namespace, int style,
			int protocol, assuan_fd_t filedes[2]);

extern struct assuan_system_hooks _assuan_system_hooks;

/* Copy the system hooks struct, paying attention to version
   differences.  SRC is usually from the user, DST MUST be from the
   library.  */
void
_assuan_system_hooks_copy (assuan_system_hooks_t dst,
			   assuan_system_hooks_t src);


/*-- assuan-pipe-server.c --*/
void _assuan_release_context (assuan_context_t ctx);

/*-- assuan-uds.c --*/
void _assuan_uds_close_fds (assuan_context_t ctx);
void _assuan_uds_deinit (assuan_context_t ctx);
void _assuan_init_uds_io (assuan_context_t ctx);


/*-- assuan-handler.c --*/
gpg_error_t _assuan_register_std_commands (assuan_context_t ctx);

/*-- assuan-buffer.c --*/
gpg_error_t _assuan_read_line (assuan_context_t ctx);
int _assuan_cookie_write_data (void *cookie, const char *buffer, size_t size);
int _assuan_cookie_write_flush (void *cookie);
gpg_error_t _assuan_write_line (assuan_context_t ctx, const char *prefix,
                                   const char *line, size_t len);

/*-- client.c --*/
gpg_error_t _assuan_read_from_server (assuan_context_t ctx,
				      assuan_response_t *okay, int *off);

/*-- assuan-error.c --*/

/*-- assuan-inquire.c --*/
gpg_error_t _assuan_inquire_ext_cb (assuan_context_t ctx);
void _assuan_inquire_release (assuan_context_t ctx);

/* Check if ERR means EAGAIN.  */
int _assuan_error_is_eagain (assuan_context_t ctx, gpg_error_t err);



#define set_error(c,e,t)						\
  assuan_set_error ((c), _assuan_error (c,e), (t))

#ifdef HAVE_W32_SYSTEM
char *_assuan_w32_strerror (assuan_context_t ctx, int ec);
#endif /*HAVE_W32_SYSTEM*/


/*-- assuan-logging.c --*/
void _assuan_init_log_envvars (void);
void _assuan_log_control_channel (assuan_context_t ctx, int outbound,
                                  const char *string,
                                  const void *buffer1, size_t length1,
                                  const void *buffer2, size_t length2);


/*-- assuan-io.c --*/
ssize_t _assuan_simple_read (assuan_context_t ctx, void *buffer, size_t size);
ssize_t _assuan_simple_write (assuan_context_t ctx, const void *buffer,
			      size_t size);

/*-- assuan-socket.c --*/

assuan_fd_t _assuan_sock_new (assuan_context_t ctx, int domain, int type,
			      int proto);
int _assuan_sock_connect (assuan_context_t ctx, assuan_fd_t sockfd,
                          struct sockaddr *addr, int addrlen);
int _assuan_sock_bind (assuan_context_t ctx, assuan_fd_t sockfd,
		       struct sockaddr *addr, int addrlen);
int _assuan_sock_get_nonce (assuan_context_t ctx, struct sockaddr *addr,
			    int addrlen, assuan_sock_nonce_t *nonce);
int _assuan_sock_check_nonce (assuan_context_t ctx, assuan_fd_t fd,
			      assuan_sock_nonce_t *nonce);
#ifdef HAVE_W32_SYSTEM
int _assuan_sock_wsa2errno (int err);
#endif

#ifdef HAVE_FOPENCOOKIE
/* We have to implement funopen in terms of glibc's fopencookie. */
FILE *_assuan_funopen(void *cookie,
                      cookie_read_function_t *readfn,
                      cookie_write_function_t *writefn,
                      cookie_seek_function_t *seekfn,
                      cookie_close_function_t *closefn);
#define funopen(a,r,w,s,c) _assuan_funopen ((a), (r), (w), (s), (c))
#endif /*HAVE_FOPENCOOKIE*/

/*-- sysutils.c --*/
const char *_assuan_sysutils_blurb (void);

#ifdef HAVE_W32CE_SYSTEM

#define getpid() GetCurrentProcessId ()
char *_assuan_getenv (const char *name);
#define getenv(a) _assuan_getenv ((a))

#endif /*HAVE_W32CE_SYSTEM*/


/* Prototypes for replacement functions.  */
#ifndef HAVE_MEMRCHR
void *memrchr (const void *block, int c, size_t size);
#endif
#ifndef HAVE_STPCPY
char *stpcpy (char *dest, const char *src);
#endif
#ifndef HAVE_SETENV
#define setenv _assuan_setenv
#define unsetenv _assuan_unsetenv
#define clearenv _assuan_clearenv
int setenv (const char *name, const char *value, int replace);
#endif
#ifndef HAVE_PUTC_UNLOCKED
int putc_unlocked (int c, FILE *stream);
#endif
#ifndef HAVE_VASPRINTF
int _assuan_vasprintf (char **result, const char *format, va_list args);
int _assuan_asprintf (char **buf, const char *fmt, ...);
#define vasprintf _assuan_vasprintf
#define asprintf  _assuan_asprintf
#endif


#define DIM(v)		     (sizeof(v)/sizeof((v)[0]))

#if HAVE_W32_SYSTEM
#define SOCKET2HANDLE(s) ((void *)(s))
#define HANDLE2SOCKET(h) ((unsigned int)(h))
#else
#define SOCKET2HANDLE(s) (s)
#define HANDLE2SOCKET(h) (h)
#endif


void _assuan_client_finish (assuan_context_t ctx);
void _assuan_client_release (assuan_context_t ctx);

void _assuan_server_finish (assuan_context_t ctx);
void _assuan_server_release (assuan_context_t ctx);


/* Encode the C formatted string SRC and return the malloc'ed result.  */
char *_assuan_encode_c_string (assuan_context_t ctx, const char *src);


#endif /*ASSUAN_DEFS_H*/
