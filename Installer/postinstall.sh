#!/bin/bash
################################################################################
# postinstall for MacGPG2
#
# @author	Mento (mento@gpgtools.org)
# @see		http://gpgtools.org
# @thanks	Alex (alex@gpgtools.org) and Benjamin Donnachie. I adopted their script.
################################################################################

TMP_DESTINATION="/private/tmp/org.gpgtools/MacGPG2"
FINAL_DESTINATION="/usr/local/MacGPG2"

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

function moveToFinalDestination {
	if [[ ! -d "$TMP_DESTINATION" ]]; then
		myEcho "Failed to copy MacGPG2 to temporary destination"
		exit 1
	fi
	# If MacGPG2 already exists on this system, remove it.
	if [[ -e "$FINAL_DESTINATION" ]]; then
		myEcho "Remove old MacGPG2 installation."
		rm -rf "$FINAL_DESTINATION" || exit 1
	fi
	
	
	# Move MacGPG2 from temporary to final destination.
	myEcho "Move new MacGPG2 to final destination."
	mkdir -p /usr/local
	mv "$TMP_DESTINATION" "$FINAL_DESTINATION" || exit 1
}

function nudo { # Execute command as a normal user.
	if [[ -z "$uid" ]] ;then
		nudoUser=$USER
	else
		nudoUser="#$uid"
	fi
	sudo -u "$nudoUser" "${@}"
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


function globalFixes {
	myEcho "Checking symlinks..."
    mkdir -p /usr/local/bin
    chmod +rX /usr /usr/local /usr/local/bin /usr/local/MacGPG2

	[[ -h /usr/local/bin/gpg2 ]] && ln -sfh /usr/local/MacGPG2/bin/gpg2 /usr/local/bin/gpg2 # Make a symlink to our gpg2, if gpg2 is a symlink. 
	[[ -h /usr/local/bin/gpg-agent ]] && ln -sfh /usr/local/MacGPG2/bin/gpg-agent /usr/local/bin/gpg-agent # Same for gpg-agent. 
	[[ -f /usr/local/bin/gpg ]] && isBinaryWorking /usr/local/bin/gpg || ln -sfh /usr/local/MacGPG2/bin/gpg2 /usr/local/bin/gpg # If gpg doesn't exists or it doesn't work, make it to a symlink.


	myEcho "Cleaning old files..."

	# Clean up old garbage.
	rm -rf /Applications/start-gpg-agent.app

	# Remove the gpg-agent helper AppleScript from login items.
    osascript -e 'tell application "System Events" to delete login item "start-gpg-agent"' &>/dev/null

	# Remove old plist files.
	rm -f "$HOME/Library/LaunchAgents/org.gpgtools.macgpg2.gpg-agent.plist" \
		"/Library/LaunchAgents/com.gpgtools.macgpg2.gpg-agent.plist" \
		"/Library/LaunchAgents/com.sourceforge.macgpg2.gpg-agent.plist"
			
	# Remove old pinentry-mac.
	rm -f /Library/LaunchAgents/com.gpgtools.macgpg2.gpg-agent.plist

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

function loadLaunchAgents {
	myEcho "loadLaunchAgents start"

	killall -KILL gpg-agent
	nudo launchctl remove org.gpgtools.macgpg2.gpg-agent
	nudo launchctl unload /Library/LaunchAgents/org.gpgtools.macgpg2.fix.plist
	
	shtdwn_pid=$(ps -xo pid,command | sed -nE 's/^([^ ]+) .*\/shutdown\-gpg-agent.*$/\1/p')
	if [[ -n "$shtdwn_pid" ]] ;then
		kill -kill "$shtdwn_pid" # Kill shutdown-gpg-agent.
		wait "$shtdwn_pid"
	fi
	
	nudo launchctl unload /Library/LaunchAgents/org.gpgtools.macgpg2.shutdown-gpg-agent.plist
	
	# Run the fixer once as root, to fix potential permission problems.
	myEcho "fixGpgHome"
	sudo /usr/local/MacGPG2/libexec/fixGpgHome "$USER" "${GNUPGHOME:-$HOME/.gnupg}"
	myEcho "Load shutdown-gpg-agent"
	nudo launchctl load /Library/LaunchAgents/org.gpgtools.macgpg2.shutdown-gpg-agent.plist

	myEcho "loadLaunchAgents done"
	return 0
}

################################################################################


SCRIPT_NAME=macgpg2
[[ $EUID -eq 0 ]] || errExit "This script must be run as root"

moveToFinalDestination # See what I did there? ;-). Yes. I see!
cleanOldGpg
globalFixes
loadLaunchAgents

exit 0
