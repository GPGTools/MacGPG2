# doc - Automake template
# Copyright (C) 2002 g10 Code GmbH
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

DISTCLEANFILES = ksba.cps

info_TEXINFOS = ksba.texi
ksba_TEXINFOS = gpl.texi 

online: ksba.html ksba.pdf
	set -e; \
	echo "Uploading current manuals to www.gnupg.org ..."; \
        user=werner ; \
	(cd ksba.html && rsync -vr --exclude='.svn' .  \
	  $${user}@cvs.gnupg.org:webspace/manuals/ksba/ ); \
        rsync -v ksba.pdf $${user}@cvs.gnupg.org:webspace/manuals/


