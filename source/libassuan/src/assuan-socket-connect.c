/* assuan-socket-connect.c - Assuan socket based client
   Copyright (C) 2002, 2003, 2004, 2009 Free Software Foundation, Inc.

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

#include <config.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif
#include <unistd.h>
#include <sys/types.h>
#ifdef HAVE_W32_SYSTEM
# include <windows.h>
#else
# include <sys/socket.h>
# include <sys/un.h>
# include <netinet/in.h>
# include <arpa/inet.h>
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

#ifndef SUN_LEN
# define SUN_LEN(ptr) ((size_t) (((struct sockaddr_un *) 0)->sun_path) \
	               + strlen ((ptr)->sun_path))
#endif


#undef WITH_IPV6
#if defined (AF_INET6) && defined(PF_INET) \
    && defined (INET6_ADDRSTRLEN) && defined(HAVE_INET_PTON)
# define WITH_IPV6 1
#endif



/* Returns true if STR represents a valid port number in decimal
   notation and no garbage is following.  */
static int 
parse_portno (const char *str, uint16_t *r_port)
{
  unsigned int value;

  for (value=0; *str && (*str >= '0' && *str <= '9'); str++)
    {
      value = value * 10 + (*str - '0');
      if (value > 65535)
        return 0;
    }
  if (*str || !value)
    return 0;

  *r_port = value;
  return 1;
}



/* Make a connection to the Unix domain socket NAME and return a new
   Assuan context in CTX.  SERVER_PID is currently not used but may
   become handy in the future.  Defined flag bits are:

     ASSUAN_SOCKET_CONNECT_FDPASSING
        sendmsg and recvmsg are used.

   NAME must either start with a slash and optional with a drive
   prefix ("c:") or use one of these URL schemata:

      file://<fname>

        This is the same as the defualt just with an explicit schemata.

      assuan://<ipaddr>:<port>
      assuan://[<ip6addr>]:<port>
         
        Connect using TCP to PORT of the server with the numerical
        IPADDR.  Not that the '[' and ']' are literal characters.

  */
gpg_error_t
assuan_socket_connect (assuan_context_t ctx, const char *name,
		       pid_t server_pid, unsigned int flags)
{
  gpg_error_t err = 0;
  assuan_fd_t fd;
#ifdef WITH_IPV6
  struct sockaddr_in6 srvr_addr_in6;
#endif
  struct sockaddr_un srvr_addr_un;
  struct sockaddr_in srvr_addr_in;
  struct sockaddr *srvr_addr = NULL;
  uint16_t port = 0;
  size_t len = 0;
  const char *s;
  int af = AF_LOCAL;
  int pf = PF_LOCAL;

  TRACE2 (ctx, ASSUAN_LOG_CTX, "assuan_socket_connect", ctx,
	  "name=%s, flags=0x%x", name ? name : "(null)", flags);

  if (!ctx || !name)
    return _assuan_error (ctx, GPG_ERR_ASS_INV_VALUE);

  if (!strncmp (name, "file://", 7) && name[7])
    name += 7;
  else if (!strncmp (name, "assuan://", 9) && name[9])
    {
      name += 9;
      af = AF_INET;
      pf = PF_INET;
    }
  else /* Default.  */
    {
      /* We require that the name starts with a slash if no URL
         schemata is used.  To make things easier we allow an optional
         driver prefix.  */
      s = name;
      if (*s && s[1] == ':')
        s += 2;
      if (*s != DIRSEP_C && *s != '/')
        return _assuan_error (ctx, GPG_ERR_ASS_INV_VALUE);
    }

  if (af == AF_LOCAL)
    {
      if (strlen (name)+1 >= sizeof srvr_addr_un.sun_path)
        return _assuan_error (ctx, GPG_ERR_ASS_INV_VALUE);

      memset (&srvr_addr_un, 0, sizeof srvr_addr_un);
      srvr_addr_un.sun_family = AF_LOCAL;
      strncpy (srvr_addr_un.sun_path, name, sizeof (srvr_addr_un.sun_path) - 1);
      srvr_addr_un.sun_path[sizeof (srvr_addr_un.sun_path) - 1] = 0;
      len = SUN_LEN (&srvr_addr_un);

      srvr_addr = (struct sockaddr *)&srvr_addr_un;
    }
  else
    {
      char *addrstr, *p;
      void *addrbuf = NULL;

      addrstr = _assuan_malloc (ctx, strlen (name) + 1);
      if (!addrstr)
        return _assuan_error (ctx, gpg_err_code_from_syserror ());

      if (*addrstr == '[')
        {
          strcpy (addrstr, name+1);
          p = strchr (addrstr, ']');
          if (!p || p[1] != ':' || !parse_portno (p+2, &port))
            err = _assuan_error (ctx, GPG_ERR_BAD_URI);
          else 
            {
              *p = 0;
#ifdef WITH_IPV6
              af = AF_INET6;
              pf = PF_INET6;
              memset (&srvr_addr_in6, 0, sizeof srvr_addr_in6);
              srvr_addr_in6.sin6_family = af;
              srvr_addr_in6.sin6_port = htons (port);
              addrbuf = &srvr_addr_in6.sin6_addr;
              srvr_addr = (struct sockaddr *)&srvr_addr_in6;
              len = sizeof srvr_addr_in6;
#else
              err =  _assuan_error (ctx, GPG_ERR_EAFNOSUPPORT);
#endif
            }
        }
      else
        {
          strcpy (addrstr, name);
          p = strchr (addrstr, ':');
          if (!p || !parse_portno (p+1, &port))
            err = _assuan_error (ctx, GPG_ERR_BAD_URI);
          else
            {
              *p = 0;
              memset (&srvr_addr_in, 0, sizeof srvr_addr_in);
              srvr_addr_in.sin_family = af;
              srvr_addr_in.sin_port = htons (port);
              addrbuf = &srvr_addr_in.sin_addr;
              srvr_addr = (struct sockaddr *)&srvr_addr_in;
              len = sizeof srvr_addr_in;
            }
        }

      if (!err)
        {
#ifdef HAVE_INET_PTON
          switch (inet_pton (af, addrstr, addrbuf))
            {
            case 1:  break;
            case 0:  err = _assuan_error (ctx, GPG_ERR_BAD_URI); break;
            default: err = _assuan_error (ctx, gpg_err_code_from_syserror ());
            }
#else /*!HAVE_INET_PTON*/
          /* We need to use the old function.  If we are here v6
             support isn't enabled anyway and thus we can do fine
             without.  Note that Windows as a compatible inet_pton
             function named inetPton, but only since Vista.  */
          srvr_addr_in.sin_addr.s_addr = inet_addr (addrstr);
          if (srvr_addr_in.sin_addr.s_addr == INADDR_NONE)
            err = _assuan_error (ctx, GPG_ERR_BAD_URI);
#endif /*!HAVE_INET_PTON*/
        }
      
      _assuan_free (ctx, addrstr);
      if (err)
        return err;
    }
  
  fd = _assuan_sock_new (ctx, pf, SOCK_STREAM, 0);
  if (fd == ASSUAN_INVALID_FD)
    {
      err = _assuan_error (ctx, gpg_err_code_from_syserror ());
      TRACE1 (ctx, ASSUAN_LOG_SYSIO, "assuan_socket_connect", ctx,
              "can't create socket: %s", strerror (errno));
      return err;
    }

  if (_assuan_sock_connect (ctx, fd, srvr_addr, len) == -1)
    {
      TRACE2 (ctx, ASSUAN_LOG_SYSIO, "assuan_socket_connect", ctx,
	      "can't connect to `%s': %s\n", name, strerror (errno));
      _assuan_close (ctx, fd);
      return _assuan_error (ctx, GPG_ERR_ASS_CONNECT_FAILED);
    }
 
  ctx->engine.release = _assuan_client_release;
  ctx->engine.readfnc = _assuan_simple_read;
  ctx->engine.writefnc = _assuan_simple_write;
  ctx->engine.sendfd = NULL;
  ctx->engine.receivefd = NULL;
  ctx->finish_handler = _assuan_client_finish;
  ctx->inbound.fd = fd;
  ctx->outbound.fd = fd;
  ctx->max_accepts = -1;

  if (flags & ASSUAN_SOCKET_CONNECT_FDPASSING)
    _assuan_init_uds_io (ctx);

  /* initial handshake */
  {
    assuan_response_t response;
    int off;

    err = _assuan_read_from_server (ctx, &response, &off);
    if (err)
      TRACE1 (ctx, ASSUAN_LOG_SYSIO, "assuan_socket_connect", ctx,
	      "can't connect to server: %s\n", gpg_strerror (err));
    else if (response != ASSUAN_RESPONSE_OK)
      {
	char *sname = _assuan_encode_c_string (ctx, ctx->inbound.line);
	if (sname)
	  {
	    TRACE1 (ctx, ASSUAN_LOG_SYSIO, "assuan_socket_connect", ctx,
		    "can't connect to server: %s", sname);
	    _assuan_free (ctx, sname);
	  }
	err = _assuan_error (ctx, GPG_ERR_ASS_CONNECT_FAILED);
      }
  }
  
  if (err)
    _assuan_reset (ctx);

  return err;
}
