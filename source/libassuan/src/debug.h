/* debug.h - interface to debugging functions
   Copyright (C) 2002, 2004, 2005, 2007 g10 Code GmbH
 
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
   License along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#ifndef DEBUG_H
#define DEBUG_H

#include <string.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif

#include "assuan-defs.h"

/* Indirect stringification, requires __STDC__ to work.  */
#define STRINGIFY(v) #v
#define XSTRINGIFY(v) STRINGIFY(v)



/* Remove path components from filenames (i.e. __FILE__) for cleaner
   logs. */
static inline const char *_assuan_debug_srcname (const char *file)
  ASSUAN_GCC_A_PURE;

static inline const char *
_assuan_debug_srcname (const char *file)
{
  const char *s = strrchr (file, '/');
  return s ? s + 1 : file;
}


/* Called early to initialize the logging.  */
void _assuan_debug_subsystem_init (void);

/* Log the formatted string FORMAT at debug level LEVEL or higher.  */
void _assuan_debug (assuan_context_t ctx, unsigned int cat,
		    const char *format, ...);

/* Start a new debug line in *LINE, logged at level LEVEL or higher,
   and starting with the formatted string FORMAT.  */
void _assuan_debug_begin (assuan_context_t ctx,
			  void **helper, unsigned int cat,
			  const char *format, ...);

/* Add the formatted string FORMAT to the debug line *LINE.  */
void _assuan_debug_add (assuan_context_t ctx,
			void **helper, const char *format, ...);

/* Finish construction of *LINE and send it to the debug output
   stream.  */
void _assuan_debug_end (assuan_context_t ctx,
			void **helper, unsigned int cat);

void _assuan_debug_buffer (assuan_context_t ctx, unsigned int cat,
			   const char *const fmt,
			   const char *const func, const char *const tagname,
			   void *tag, const char *const buffer, size_t len);


/* Trace support.  */

#define _TRACE(ctx, lvl, name, tag)					\
  assuan_context_t _assuan_trace_context = ctx;				\
  int _assuan_trace_level = lvl;					\
  const char *const _assuan_trace_func = name;				\
  const char *const _assuan_trace_tagname = STRINGIFY (tag);		\
  void *_assuan_trace_tag = (void *) (uintptr_t) tag

#define TRACE_BEG(ctx,lvl, name, tag)			     \
  _TRACE (ctx, lvl, name, tag);				     \
  _assuan_debug (_assuan_trace_context, _assuan_trace_level, \
		 "%s (%s=%p): enter\n",			     \
		_assuan_trace_func, _assuan_trace_tagname,   \
		_assuan_trace_tag), 0
#define TRACE_BEG0(ctx, lvl, name, tag, fmt)				\
  _TRACE (ctx, lvl, name, tag);						\
  _assuan_debug (_assuan_trace_context, _assuan_trace_level,		\
		 "%s (%s=%p): enter: " fmt "\n",			\
		_assuan_trace_func, _assuan_trace_tagname,		\
		_assuan_trace_tag), 0
#define TRACE_BEG1(ctx, lvl, name, tag, fmt, arg1)			\
  _TRACE (ctx, lvl, name, tag);						\
  _assuan_debug (_assuan_trace_context, _assuan_trace_level,		\
		 "%s (%s=%p): enter: " fmt "\n",			\
		_assuan_trace_func, _assuan_trace_tagname,		\
		_assuan_trace_tag, arg1), 0
#define TRACE_BEG2(ctx, lvl, name, tag, fmt, arg1, arg2)		\
  _TRACE (ctx, lvl, name, tag);						\
  _assuan_debug (_assuan_trace_context, _assuan_trace_level,		\
		 "%s (%s=%p): enter: " fmt "\n",			\
		 _assuan_trace_func, _assuan_trace_tagname,		\
		 _assuan_trace_tag, arg1, arg2), 0
#define TRACE_BEG3(ctx, lvl, name, tag, fmt, arg1, arg2, arg3)	      \
  _TRACE (ctx, lvl, name, tag);					      \
  _assuan_debug (_assuan_trace_context, _assuan_trace_level,	      \
		 "%s (%s=%p): enter: " fmt "\n",		      \
		 _assuan_trace_func, _assuan_trace_tagname,	      \
		 _assuan_trace_tag, arg1, arg2, arg3), 0
#define TRACE_BEG4(ctx, lvl, name, tag, fmt, arg1, arg2, arg3, arg4)	\
  _TRACE (ctx, lvl, name, tag);						\
  _assuan_debug (_assuan_trace_context, _assuan_trace_level,		\
		 "%s (%s=%p): enter: " fmt "\n",			\
		 _assuan_trace_func, _assuan_trace_tagname,		\
		 _assuan_trace_tag, arg1, arg2, arg3, arg4), 0
#define TRACE_BEG6(ctx, lvl, name, tag, fmt, arg1, arg2, arg3, arg4,arg5,arg6) \
  _TRACE (ctx, lvl, name, tag);						\
  _assuan_debug (_assuan_trace_context, _assuan_trace_level,		\
		 "%s (%s=%p): enter: " fmt "\n",			\
		 _assuan_trace_func, _assuan_trace_tagname,		\
		 _assuan_trace_tag, arg1, arg2, arg3, arg4, arg5, arg6), 0

#define TRACE_BEG8(ctx, lvl, name, tag, fmt, arg1, arg2, arg3, arg4,	\
		   arg5, arg6, arg7, arg8)				\
  _TRACE (ctx, lvl, name, tag);						\
  _assuan_debug (_assuan_trace_context, _assuan_trace_level, "%s (%s=%p): enter: " fmt "\n", \
		 _assuan_trace_func, _assuan_trace_tagname,		\
		 _assuan_trace_tag, arg1, arg2, arg3, arg4,		\
		 arg5, arg6, arg7, arg8), 0

#define TRACE(ctx, lvl, name, tag)					\
  _assuan_debug (ctx, lvl, "%s (%s=%p): call\n",			\
		 name, STRINGIFY (tag), (void *) (uintptr_t) tag), 0
#define TRACE0(ctx, lvl, name, tag, fmt)				\
  _assuan_debug (ctx, lvl, "%s (%s=%p): call: " fmt "\n",		\
		 name, STRINGIFY (tag), (void *) (uintptr_t) tag), 0
#define TRACE1(ctx, lvl, name, tag, fmt, arg1)				\
  _assuan_debug (ctx, lvl, "%s (%s=%p): call: " fmt "\n",		\
		 name, STRINGIFY (tag), (void *) (uintptr_t) tag, arg1), 0
#define TRACE2(ctx, lvl, name, tag, fmt, arg1, arg2)			\
  _assuan_debug (ctx, lvl, "%s (%s=%p): call: " fmt "\n",		\
		 name, STRINGIFY (tag), (void *) (uintptr_t) tag, arg1, \
		 arg2), 0
#define TRACE3(ctx, lvl, name, tag, fmt, arg1, arg2, arg3)		\
  _assuan_debug (ctx, lvl, "%s (%s=%p): call: " fmt "\n",		\
		 name, STRINGIFY (tag), (void *) (uintptr_t) tag, arg1, \
		 arg2, arg3), 0
#define TRACE4(ctx, lvl, name, tag, fmt, arg1, arg2, arg3, arg4)	\
  _assuan_debug (ctx, lvl, "%s (%s=%p): call: " fmt "\n",		\
		 name, STRINGIFY (tag), (void *) (uintptr_t) tag, arg1, \
		 arg2, arg3, arg4), 0
#define TRACE6(ctx, lvl, name, tag, fmt, arg1, arg2, arg3, arg4, arg5, arg6) \
  _assuan_debug (ctx, lvl, "%s (%s=%p): call: " fmt "\n",		\
		 name, STRINGIFY (tag), (void *) (uintptr_t) tag, arg1,	\
		 arg2, arg3, arg4, arg5, arg6), 0

#define TRACE_ERR(err)							\
  err == 0 ? (TRACE_SUC ()) :						\
    (_assuan_debug (_assuan_trace_context, _assuan_trace_level,		\
		    "%s (%s=%p): error: %s <%s>\n",			\
		    _assuan_trace_func, _assuan_trace_tagname,		\
		    _assuan_trace_tag, gpg_strerror (err),		\
		    gpg_strsource (ctx->err_source)),			\
     _assuan_error (ctx, err))

/* The cast to void suppresses GCC warnings.  */
#define TRACE_SYSRES(res)						\
  res >= 0 ? ((void) (TRACE_SUC1 ("result=%i", res)), (res)) :		\
    (_assuan_debug (_assuan_trace_context, _assuan_trace_level, "%s (%s=%p): error: %s\n", \
		    _assuan_trace_func, _assuan_trace_tagname,		\
		   _assuan_trace_tag, strerror (errno)), (res))
#define TRACE_SYSERR(res)						\
  res == 0 ? ((void) (TRACE_SUC1 ("result=%i", res)), (res)) :		\
    (_assuan_debug (_assuan_trace_context, _assuan_trace_level, "%s (%s=%p): error: %s\n", \
		    _assuan_trace_func, _assuan_trace_tagname,		\
		    _assuan_trace_tag, strerror (res)), (res))

#define TRACE_SUC()						 \
  _assuan_debug (_assuan_trace_context, _assuan_trace_level,	 \
		 "%s (%s=%p): leave\n",				 \
		_assuan_trace_func, _assuan_trace_tagname,	 \
		_assuan_trace_tag), 0
#define TRACE_SUC0(fmt)							\
  _assuan_debug (_assuan_trace_context, _assuan_trace_level,		\
		 "%s (%s=%p): leave: " fmt "\n",			\
		 _assuan_trace_func, _assuan_trace_tagname,		\
		 _assuan_trace_tag), 0
#define TRACE_SUC1(fmt, arg1)						\
  _assuan_debug (_assuan_trace_context, _assuan_trace_level,		\
		 "%s (%s=%p): leave: " fmt "\n",			\
		 _assuan_trace_func, _assuan_trace_tagname,		\
		 _assuan_trace_tag, arg1), 0
#define TRACE_SUC2(fmt, arg1, arg2)					\
  _assuan_debug (_assuan_trace_context, _assuan_trace_level,		\
		 "%s (%s=%p): leave: " fmt "\n",			\
		 _assuan_trace_func, _assuan_trace_tagname,		\
		 _assuan_trace_tag, arg1, arg2), 0
#define TRACE_SUC5(fmt, arg1, arg2, arg3, arg4, arg5)			\
  _assuan_debug (_assuan_trace_context, _assuan_trace_level,		\
		 "%s (%s=%p): leave: " fmt "\n",			\
		 _assuan_trace_func, _assuan_trace_tagname,		\
		 _assuan_trace_tag, arg1, arg2, arg3, arg4, arg5), 0

#define TRACE_LOG(fmt)							\
  _assuan_debug (_assuan_trace_context, _assuan_trace_level,		\
		 "%s (%s=%p): check: " fmt "\n",			\
		 _assuan_trace_func, _assuan_trace_tagname,		\
		 _assuan_trace_tag), 0
#define TRACE_LOG1(fmt, arg1)						\
  _assuan_debug (_assuan_trace_context, _assuan_trace_level,		\
		 "%s (%s=%p): check: " fmt "\n",			\
		_assuan_trace_func, _assuan_trace_tagname,		\
		_assuan_trace_tag, arg1), 0
#define TRACE_LOG2(fmt, arg1, arg2)				    \
  _assuan_debug (_assuan_trace_context, _assuan_trace_level,	    \
		 "%s (%s=%p): check: " fmt "\n",		    \
		 _assuan_trace_func, _assuan_trace_tagname,	    \
		 _assuan_trace_tag, arg1, arg2), 0
#define TRACE_LOG3(fmt, arg1, arg2, arg3)			    \
  _assuan_debug (_assuan_trace_context, _assuan_trace_level,	    \
		 "%s (%s=%p): check: " fmt "\n",		    \
		 _assuan_trace_func, _assuan_trace_tagname,	    \
		 _assuan_trace_tag, arg1, arg2, arg3), 0
#define TRACE_LOG4(fmt, arg1, arg2, arg3, arg4)			    \
  _assuan_debug (_assuan_trace_context, _assuan_trace_level,	    \
		 "%s (%s=%p): check: " fmt "\n",		    \
		_assuan_trace_func, _assuan_trace_tagname,	    \
		_assuan_trace_tag, arg1, arg2, arg3, arg4), 0
#define TRACE_LOG5(fmt, arg1, arg2, arg3, arg4, arg5)		    \
  _assuan_debug (_assuan_trace_context, _assuan_trace_level,	    \
		 "%s (%s=%p): check: " fmt "\n",		    \
		_assuan_trace_func, _assuan_trace_tagname,	    \
		 _assuan_trace_tag, arg1, arg2, arg3, arg4, arg5), 0
#define TRACE_LOG6(fmt, arg1, arg2, arg3, arg4, arg5, arg6)	    \
  _assuan_debug (_assuan_trace_context, _assuan_trace_level,	    \
		 "%s (%s=%p): check: " fmt "\n",			\
		 _assuan_trace_func, _assuan_trace_tagname,		\
		 _assuan_trace_tag, arg1, arg2, arg3, arg4, arg5,	\
		 arg6), 0

#define TRACE_LOGBUF(buf, len)						\
  _assuan_debug_buffer (_assuan_trace_context, _assuan_trace_level,	\
			"%s (%s=%p): check: %s",			\
			_assuan_trace_func, _assuan_trace_tagname,	\
			_assuan_trace_tag, buf, len)

#define TRACE_SEQ(hlp,fmt)						\
  _assuan_debug_begin (_assuan_trace_context, &(hlp),			\
		       "%s (%s=%p): check: " fmt,			\
		       _assuan_trace_func, _assuan_trace_tagname,	\
		       _assuan_trace_tag)
#define TRACE_ADD0(hlp,fmt) \
  _assuan_debug_add (_assuan_trace_context, &(hlp), fmt)
#define TRACE_ADD1(hlp,fmt,a) \
  _assuan_debug_add (_assuan_trace_context, &(hlp), fmt, (a))
#define TRACE_ADD2(hlp,fmt,a,b) \
  _assuan_debug_add (_assuan_trace_context, &(hlp), fmt, (a), (b))
#define TRACE_ADD3(hlp,fmt,a,b,c) \
  _assuan_debug_add (_assuan_trace_context, &(hlp), fmt, (a), (b), (c))
#define TRACE_END(hlp,fmt) \
  _assuan_debug_add (_assuan_trace_context, &(hlp), fmt); \
  _assuan_debug_end (_assuan_trace_context, &(hlp), _assuan_trace_level)
#define TRACE_ENABLED(hlp) (!!(hlp))

#endif	/* DEBUG_H */
