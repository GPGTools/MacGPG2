#!/bin/bash
################################################################################
# postinstall for MacGPG2
#
# @author	Mento (mento@gpgtools.org)
# @see		http://gpgtools.org
# @thanks	Alex (alex@gpgtools.org) and Benjamin Donnachie. I adopted their script.
################################################################################



# Helper functions #############################################################
function errExit {
	msg="$* (${BASH_SOURCE[1]##*/}: line ${BASH_LINENO[0]})"
	if [[ -t 1 ]] ;then
		echo -e "\033[1;31m$msg\033[0m" >&2
	else
		echo "$msg" >&2
	fi
	exit 1
}
function myEcho {
	echo "${SCRIPT_NAME:+[$SCRIPT_NAME] }$*"
}


function nudo { # Execute command as a normal user.
	sudo -u "#$uid" "${@}"
}
function isBinaryWorking {
	# Check if the binary, passed to this function, is working.
	nudo "$1" --version >&/dev/null
	return $?
}
function findWorkingPinentry {
	# Pinentry binary
	PINENTRY_BINARY_PATH="pinentry-mac.app/Contents/MacOS/pinentry-mac"
	# Pinentry in Libmacgpg
	PINENTRY_PATHS[1]="/Library/Frameworks/Libmacgpg.framework/Versions/A/Resources"
	# Pinentry in MacGPG2
	PINENTRY_PATHS[0]="/usr/local/MacGPG2/libexec"

	for pinentry_path in "${PINENTRY_PATHS[@]}"; do
		full_pinentry_path="$pinentry_path/$PINENTRY_BINARY_PATH"

		if isBinaryWorking "$full_pinentry_path" ;then
			echo "$full_pinentry_path"
			return 0
		fi
	done
	
	return 1
}

################################################################################

function userFixes {
	# Diese Methode um die Benutzerkonten zu durchlaufen, ist etwa 50 mal schneller als die alte mit einzelnen dscl Aufrufen.
	a=$'\6'
	b=$'\7'
	p1=".*?${b}name: ([^$a]*)"
	p2=".*?${a}uid: ([^$a]*)"
	p3=".*?${a}gid: ([^$a]*)"
	p4=".*?${a}dir: ([^$a]*)"
	pend="[^$b]*"

	temptext=$b$(dscacheutil -q user | perl -0 -pe "s/\n\n/$b/g;s/\n/$a/g") # Get all users and replace all newlines, so the next RE can work correctly.
	perl -pe "s/$p1$p2$p3$p4$pend/\1 \2 \3 \4\n/g" <<<"$temptext" | # This RE create one line per user.
	while read username uid gid homedir # Iterate through each user.
	do
		[[ -n "$uid" && "$uid" -ge 500 && -d "$homedir" ]] || continue # Only proceed with regular accounts, which also have a homedir.
		[[ "$gid" -lt 500 ]] || continue # I think a gid >= 500 indicates a special user. (e.g. like macports)

		[[ "$homedir" == "${homedir#/Network}" ]] || continue # Ignore home-dirs starting with "/Network".

		GNUPGHOME=$homedir/.gnupg
		fixGpgHome
		fixGPGAgent
	done

}
function fixGpgHome {
	myEcho "Fixing '$GNUPGHOME'..."
	# Only call this functions from userFixes.

	# Permissions
	[[ -e "$GNUPGHOME" ]] || mkdir -m 0700 "$GNUPGHOME"
	chown -R "$uid:$gid" "$GNUPGHOME"
	chmod -R -N "$GNUPGHOME" 2>/dev/null
	chmod -R u+rwX,go= "$GNUPGHOME"

	# Create gpg.conf if needed.
	if [[ ! -e "$GNUPGHOME/gpg.conf" ]] ;then
		nudo cp "/usr/local/MacGPG2/share/gnupg/gpg-conf.skel" "$GNUPGHOME/gpg.conf"
		myEcho "Created gpg.conf"
	elif ! nudo /usr/local/MacGPG2/bin/gpg2 --gpgconf-test ;then
		myEcho "Fixing gpg.conf"
		nudo mv "$GNUPGHOME/gpg.conf" "$GNUPGHOME/gpg.conf.moved-by-gpgtools-installer"
		nudo cp /usr/local/MacGPG2/share/gnupg/gpg-conf.skel "$GNUPGHOME/gpg.conf"
		myEcho "Replaced gpg.conf"
	fi

	# Add a keyserver if none exits
	if ! grep -q '^[ 	]*keyserver ' "$GNUPGHOME/gpg.conf" ;then
        echo "keyserver hkp://pool.sks-keyservers.net" >> "$GNUPGHOME/gpg.conf"
    fi


	rm -f "$GNUPGHOME/S.gpg-agent" "$GNUPGHOME/S.gpg-agent.ssh"
}

function fixGPGAgent {
	gpgAgentConf="$GNUPGHOME/gpg-agent.conf"
	myEcho "Fixing '$gpgAgentConf'..."
	nudo touch "$gpgAgentConf"

	# Fix pinentry.
	currentPinetry=$(sed -En '/^[ 	]*pinentry-program "?([^"]*)"?/{s//\1/p;q;}' "$gpgAgentConf")
	if ! isBinaryWorking "$currentPinetry" ;then
		# Let's find a working pinentry.
		myEcho "Found working pinentry at: $working_pinentry"
		if ! working_pinentry=$(findWorkingPinentry) ;then
			myEcho "No working pinentry found. Abort!"
			return 1
		fi

		if [[ -n "$currentPinetry" ]] ;then
			myEcho "Replacing existing pinentry"
			sed -Ei '' '/^([ 	]*pinentry-program ).*/s@@\1'"$working_pinentry@" "$gpgAgentConf"
		else
			myEcho "Add new pinentry"
			echo "pinentry-program $working_pinentry" >> "$gpgAgentConf"
		fi
	fi


	# "$GNUPGHOME" on NFS volumes
    # http://gpgtools.lighthouseapp.com/projects/66001-macgpg2/tickets/55
    if ! grep -Eq '^[       ]*no-use-standard-socket' "$gpgAgentConf" ;then
		tempFile="$GNUPGHOME/test.tmp"
		rm -f "$tempFile"
		if ! mkfifo "$tempFile" >&/dev/null ;then
			echo "no-use-standard-socket" >> "$gpgAgentConf"
		fi
		rm -f "$tempFile"
    fi

	killall -u "$username" gpg-agent 2>/dev/null

    rm -f "$GNUPGHOME/S.gpg-agent" "$GNUPGHOME/S.gpg-agent.ssh"
}


function globalFixes {
	myEcho "Checking symlinks..."
    mkdir -p /usr/local/bin
    chmod +rX /usr /usr/local /usr/local/bin /usr/local/MacGPG2 >&/dev/null

	[[ -h /usr/local/bin/gpg2 ]] && ln -sfh /usr/local/MacGPG2/bin/gpg2 /usr/local/bin/gpg2 # Make a symlink to our gpg2, if gpg2 is a symlink. 
	[[ -h /usr/local/bin/gpg-agent ]] && ln -sfh /usr/local/MacGPG2/bin/gpg-agent /usr/local/bin/gpg-agent # Same for gpg-agent. 
	[[ -f /usr/local/bin/gpg ]] && isBinaryWorking /usr/local/bin/gpg || ln -sfh /usr/local/MacGPG2/bin/gpg2 /usr/local/bin/gpg # If gpg doesn't exists or it doesn't work, make it to a symlink.


	myEcho "Cleaning old files..."

	# Clean up old garbage.
	rm -rf /Applications/start-gpg-agent.app

	# Remove the gpg-agent helper AppleScript from login items.
    osascript -e 'tell application "System Events" to delete login item "start-gpg-agent"' 2> /dev/null

	# Remove old plist files.
	rm -f "$HOME/Library/LaunchAgents/org.gpgtools.macgpg2.gpg-agent.plist" \
		"/Library/LaunchAgents/org.gpgtools.macgpg2.gpg-agent.plist" \
		"/Library/LaunchAgents/com.sourceforge.macgpg2.gpg-agent.plist"

	# Remove old pinentry-mac.
	rm -fr /usr/local/libexec/pinentry-mac.app

    # Remove obsolete login and logout scripts. 
    defaults read com.apple.loginwindow LoginHook 2>/dev/null  | grep -qF "/sbin/gpg-login.sh"  && defaults delete com.apple.loginwindow LoginHook
    defaults read com.apple.loginwindow LogoutHook 2>/dev/null | grep -qF "/sbin/gpg-logout.sh" && defaults delete com.apple.loginwindow LogoutHook

}

function cleanOldGpg {
	[[ -f /usr/local/bin/gpg2 && ! -h /usr/local/bin/gpg2 ]] || return # Only clean, if there is an old gpg2. 
	nudo /usr/local/bin/gpg2 --version | grep -qF MacGPG2 || return # Only clean, if the old gpg2 is MacGPG2

	myEcho "Removing old MacGPG2..."

	# Remove old files.
	a=/usr/local; b=$a/bin; c=$a/libexec; d=$a/sbin; e=$a/share; f=$e/info; g=$e/locale; h=$e/man/man1; i=$e/man/man8; j=/LC_MESSAGES/gnupg2.mo
	rm -f $b/gpg-agent $b/gpg-connect-agent $b/gpg-error $b/gpg2 $b/gpgconf $b/gpgkey2ssh $b/gpgparsemail $b/gpgsm $b/gpgsm-gencert.sh $b/gpgv2 \
		$b/scdaemon $b/watchgnupg $c/gnupg-pcsc-wrapper $c/gpg-check-pattern $c/gpg-preset-passphrase $c/gpg-protect-tool $c/gpg2keys_curl \
		$c/gpg2keys_finger $c/gpg2keys_hkp $c/gpg2keys_ldap $d/addgnupghome $d/applygnupgdefaults $f/gnupg.info $f/gnupg.info-1 $f/gnupg.info-2 \
		$g/be$j $g/ca$j $g/cs$j $g/da$j $g/de$j $g/el$j $g/en@boldquot$j $g/en@quot$j $g/eo$j $g/es$j $g/et$j $g/fi$j $g/fr$j $g/gl$j \
		$g/hu$j $g/id$j $g/it$j $g/ja$j $g/nb$j $g/pl$j $g/pt$j $g/pt_BR$j $g/ro$j $g/ru$j $g/sk$j $g/sv$j $g/tr$j $g/zh_CN$j $g/zh_TW$j \
		$h/gpg-agent.1 $h/gpg-connect-agent.1 $h/gpg-preset-passphrase.1 $h/gpg-zip.1 $h/gpg2.1 $h/gpgconf.1 $h/gpgparsemail.1 \
		$h/gpgsm-gencert.sh.1 $h/gpgsm.1 $h/gpgv2.1 $h/scdaemon.1 $h/symcryptrun.1 $h/watchgnupg.1 $i/addgnupghome.8 $i/applygnupgdefaults.8
	rm -rf $e/doc/gnupg $e/gnupg
}

function registerShutdownAgentHelper {
    # Create LaunchAgents dir if it doesn't already exist.
    PLIST_NAME="org.gpgtools.macgpg2.shutdown-gpg-agent.plist"
    LAUNCH_AGENTS_PATH="/Library/LaunchAgents"
    PLIST_DESTINATION_PATH="$LAUNCH_AGENTS_PATH/$PLIST_NAME"
    PLIST_SOURCE_PATH="/usr/local/MacGPG2/share/$PLIST_NAME"
    if [ ! -d "$LAUNCH_AGENTS_PATH" ]; then
        mkdir -p "$LAUNCH_AGENTS_PATH"
    fi
    if [ -L "$PLIST_DESTINATION_PATH" ]; then
        rm "$PLIST_DESTINATION_PATH"
    fi
    
    ln -s "$PLIST_SOURCE_PATH" "$PLIST_DESTINATION_PATH"
    
    launchctl unload "$LAUNCH_AGENTS_PATH/$PLIST_NAME" &> /dev/null
    uid="$(id -u $USER)"
    nudo launchctl load "$PLIST_DESTINATION_PATH" || errExit "Couldn't start gpg-agent kill on logout script."
}

################################################################################

SCRIPT_NAME=macgpg2
[[ $EUID -eq 0 ]] || errExit "This script must be run as root"

cleanOldGpg
globalFixes
userFixes
registerShutdownAgentHelper


