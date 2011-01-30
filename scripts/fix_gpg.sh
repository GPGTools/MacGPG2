#!/bin/bash
################################################################################
#
# GPG auto fix.
#
# @author   Alexander Willner <alex@willner.ws>
# @version  2011-01-25 (v0.1)
#
# @changelog    2011-02-25 (v0.1)   initial release
################################################################################


################################################################################
# setup
################################################################################
_bundleId="gpg";
################################################################################

################################################################################
# Precoditions
################################################################################
  if [ ! -e "/usr/local/MacGPG2/bin/gpg2" ]; then
    echo "[$_bundleId] Please install MacGPG2 first (http://gpgtools.org)";
    exit 1;
  fi

################################################################################
# Clean up (also clean up bad GPGTools behaviour)
################################################################################
  echo "[$_bundleId] Removing invalid symbolic links...";
  [ -h "$HOME/.gnupg/S.gpg-agent" ] && rm -f "$HOME/.gnupg/S.gpg-agent"
  [ -h "$HOME/.gnupg/S.gpg-agent.ssh" ] && rm -f "$HOME/.gnupg/S.gpg-agent.ssh"


################################################################################
# Add some links (force the symlink to be sure)
################################################################################
  echo "[$_bundleId] Linking gpg2...";
  rm -f /usr/local/bin/gpg2; ln -s /usr/local/MacGPG2/bin/gpg2 /usr/local/bin/gpg2
  [ ! -e /usr/local/bin/gpg ] && ln -s /usr/local/MacGPG2/bin/gpg2 /usr/local/bin/gpg


################################################################################
# Create a new gpg.conf if none is existing from the skeleton file
################################################################################
  echo "[$_bundleId] Checking gpg.conf...";
    if ( ! test -e $HOME/.gnupg/gpg.conf ) then
		echo "[$_bundleId] Not found!";
    	mkdir -p $HOME/.gnupg
    	cp /usr/local/MacGPG2/share/gnupg/gpg-conf.skel $HOME/.gnupg/gpg.conf
    fi
    if ( ! /usr/local/MacGPG2/bin/gpg2 --gpgconf-test ) then
		echo "[$_bundleId] Was invalid!";
        mv $HOME/.gnupg/gpg.conf $HOME/.gnupg/gpg.conf.moved-by-gpgtools-installer
        cp /usr/local/MacGPG2/share/gnupg/gpg-conf.skel $HOME/.gnupg/gpg.conf
    fi
    if [ "" == "`grep 'comment GPGTools' $HOME/.gnupg/gpg.conf`" ]; then
        echo "comment GPGTools - http://gpgtools.org" >> $HOME/.gnupg/gpg.conf;
    fi
    # Remove any gpg-agent pinentry program options
    if [ -e "$HOME/.gnupg/gpg-agent.conf" ]; then
        sed -i '' 's/^[ 	]*pinentry-program/#pinentry-program/g' "$HOME/.gnupg/gpg-agent.conf"
    fi


################################################################################
# Fix permissions (just to be sure)
################################################################################
  echo "[$_bundleId] Fixing permissions...";
  osascript -e 'do shell script "sh ./fix_gpg_permissions.sh" with administrator privileges'

exit 0
