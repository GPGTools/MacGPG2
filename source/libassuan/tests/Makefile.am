# Makefile for Assuan regression tests
# Copyright (C) 2006 Free Software Foundation, Inc.
#
# This file is part of Assuan.
#
# Assuan is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation; either version 3 of
# the License, or (at your option) any later version.
#
# Assuan is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this program; if not, see <http://www.gnu.org/licenses/>.

## Process this file with automake to produce Makefile.in

TESTS_ENVIRONMENT = 

EXTRA_DIST = motd ce-createpipe.c

BUILT_SOURCES = 
CLEANFILES = 

TESTS = pipeconnect

if HAVE_W32CE_SYSTEM
w32cetools = ce-createpipe
endif

if USE_DESCRIPTOR_PASSING
TESTS += fdpassing
endif


AM_CFLAGS = $(GPG_ERROR_CFLAGS)

noinst_HEADERS = common.h
noinst_PROGRAMS = $(TESTS) ce-server $(w32cetools)
LDADD = ../src/libassuan.la  $(NETLIBS) $(GPG_ERROR_LIBS)

