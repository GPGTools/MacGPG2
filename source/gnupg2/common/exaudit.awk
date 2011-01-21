# exaudit.awk - Extract audit event codes from audit.h
#	Copyright (C) 2007 Free Software Foundation, Inc.
#
# This file is part of GnuPG.
#
# GnuPG is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# GnuPG is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <http://www.gnu.org/licenses/>.

BEGIN {
  print "# Output of exaudit.awk - DO NOT EDIT."
  topheader = 0;
  okay = 0;
  code = 0;
}

topheader == 0 && /^\/\*/  { topheader = 1 }
topheader == 1             { print $0 }
topheader == 1 && /\*\//   { topheader = 2; print "" }

/AUDIT_NULL_EVENT/   { okay = 1 }
!okay                { next }
/AUDIT_LAST_EVENT/   { exit }
/AUDIT_[A-Za-z_]+/  { 
  sub (/[,\/\*]+/, "", $1);
  desc = tolower (substr($1,7));
  gsub (/_/," ",desc);
  printf "%d\t%s\t%s\n", code, $1, desc;
  code++;
}

END {
  print "# end of audit codes."
}
