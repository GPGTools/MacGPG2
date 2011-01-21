# mkoidtbl.awk  - Create OID table from Peter Gutmann's dumpasn1.cfg
# Copyright (C) 2004 g10 Code GmbH
# 
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This file is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#

# This file takes a list of OID description in a format like
#
#  # Comment line, the next line identifies a new record
#  OID = 06 05 02 82 06 01 0A
#  Comment = Deutsche Telekom
#  Description = Telesec (0 2 262 1 10)
#
#  And creates a new table in IETF notation with lines like
#  0.2.262.1.10 Telesec Deutsche Telekom
#  comment lines may also occur in the output.
#      


BEGIN {
  print "static struct { char *oid, *desc, *comment; } oidtranstbl[] = {"
}

/^[ \t]*#/ { next }
/^OID/     { flush() }
/^Comment/ { comment = substr($0, index($0, "=") + 2 )
             gsub(/\r/, "", comment) 
             gsub (/\\/, "\\\\", comment)
             gsub (/"/, "\\\"", comment)
}
/^Description/ {
  desc = substr($0, index($0, "=") + 2)
  gsub(/\r/, "", desc)
  if (match (desc, /\([0-9 \t]+\)/) > 2) {
    oid = substr(desc, RSTART+1, RLENGTH-2 )
    desc = substr(desc, 1, RSTART-1);
    gsub (/[ \t]+/, ".", oid)
    gsub (/\\/, "\\\\", desc)
    gsub (/"/, "\\\"", desc)
    sub (/[ \t]*$/, "", desc)
  }
}

END { flush();  print "  { NULL, NULL, NULL }\n};"  }

function flush() {
  if(oid && desc)
     printf "  { \"%s\", \"%s\", \"%s\" },\n", oid, desc, comment
  oid = desc = comment = ""
}
