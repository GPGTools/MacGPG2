#!/bin/sh

# this skript returns 0 if either gpg or gpg2 is installed on the system

PATH=/usr/bin:/bin:/usr/sbin:/sbin:/usr/local/bin:/opt/local/bin
[ -r "$HOME/.profile" ] && . "$HOME/.profile" 
if( /usr/bin/which -s gpg ) then
	exit 0;
fi
if( /usr/bin/which -s gpg2 ) then
	exit 0;
fi
exit 1;
