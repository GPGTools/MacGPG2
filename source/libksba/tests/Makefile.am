# Makefile.am - for the KSBA regression tests
#       Copyright (C) 2001, 2003 g10 Code GmbH
# 	
# This file is part of KSBA.
# 
# KSBA is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
# 
# KSBA is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <http://www.gnu.org/licenses/>.

## Process this file with automake to produce Makefile.in

TESTS_ENVIRONMENT = 

test_certs = cert_dfn_pca01.der cert_dfn_pca15.der \
	     cert_g10code_test1.der crl_testpki_testpca.der \
	     samples/authority.crt samples/betsy.crt samples/bull.crt \
             samples/ov-ocsp-server.crt samples/ov-userrev.crt \
             samples/ov-root-ca-cert.crt samples/ov-serverrev.crt \
	     samples/ov-user.crt samples/ov-server.crt  \
             samples/ov2-root-ca-cert.crt samples/ov2-ocsp-server.crt \
             samples/ov2-user.crt samples/ov2-userrev.crt 

test_crls = samples/ov-test-crl.crl 

test_keys = samples/ov-server.p12  samples/ov-userrev.p12 \
             samples/ov-serverrev.p12  samples/ov-user.p12

EXTRA_DIST = $(test_certs) samples/README mkoidtbl.awk

BUILT_SOURCES = oidtranstbl.h
CLEANFILES = oidtranstbl.h

TESTS = cert-basic t-crl-parser t-dnparser

AM_CFLAGS = $(GPG_ERROR_CFLAGS)

noinst_HEADERS = t-common.h
noinst_PROGRAMS = $(TESTS) t-cms-parser t-crl-parser t-dnparser t-ocsp
LDADD = ../src/libksba.la $(GPG_ERROR_LIBS)

t_ocsp_SOURCES = t-ocsp.c sha1.c

# Build the OID table: Note that the binary includes data from an
# another program and we may not be allowed to distribute this.  This
# ain't no problem as the programs using this generated data are not
# installed and thus not distributed.
oidtranstbl.h: Makefile mkoidtbl.awk 
	set -e; f="/dev/null"; \
        for i in /etc/dumpasn1 /usr/local/bin /usr/local/share /usr/bin \
                 /usr/share ; do \
          if test -f $$i/dumpasn1.cfg; then f=$$i/dumpasn1.cfg; break; fi; \
        done; $(AWK) -f $(srcdir)/mkoidtbl.awk $$f >$@

