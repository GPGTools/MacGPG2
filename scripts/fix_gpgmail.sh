#!/bin/bash
################################################################################
#
# GPGTools auto fix (for new Mail.app releases).
#
# @author   Alexander Willner <alex@willner.ws>
# @version  2011-01-27 (v0.4)
# @source   http://www.mail-archive.com/gpgmail-users@lists.sourceforge.net/msg00178.html
#
# To install it:
#   * cp org.gpgmail.loginscript.plist ~/Library/LaunchAgents/
#   * cp org.gpgmail.loginscript.sh /Applications/.org.gpgmail.loginscript.sh
# To test it:
#   * launchctl load ~/Library/LaunchAgents/org.gpgmail.loginscript.plist
#   * bash /Applications/.org.gpgmail.loginscript.sh
#
# @changelog    2010-08-02 (v0.1)   initial release
#               2010-08-03 (v0.2)   added "(Disabled" to the search path
#                                   using "defaults write" and "plutil" now
#               2010-08-05 (v0.3)	using "defaults read"
#               2011-01-27 (v0.4)	Updates for 10.6.7
################################################################################


################################################################################
# setup
################################################################################
_bundleId="gpgmail";
_bundleName="GPGMail.mailbundle";
_bundleRootPath="$HOME/Library/Mail/Bundles";
_bundlePath="$_bundleRootPath/$_bundleName";
_plistBundle="$_bundlePath/Contents/Info";
_plistMail="/Applications/Mail.app/Contents/Info";
_plistFramework="/System/Library/Frameworks/Message.framework/Resources/Info";
################################################################################


################################################################################
# is already disabled?
################################################################################
isInstalled=`if [ -d "$_bundlePath" ]; then echo "1"; else echo "0"; fi`

if [ "1" == "$isInstalled" ]; then
  echo "[$_bundleId] is installed";
else
  foundDisabled=`find "$_bundleRootPath ("* -type d -name "$_bundleName"|head -n1`
  if [ "" != "$foundDisabled" ]; then
    mkdir -p "$_bundleRootPath";
    mv "$foundDisabled" "$_bundleRootPath";
  else
    echo "[$_bundleId] not found";
    exit 1;
  fi
    echo "[$_bundleId] was reinstalled";
fi
################################################################################


################################################################################
# is already patched?
################################################################################
uuid1=`defaults read "$_plistMail" "PluginCompatibilityUUID"`
uuid2=`defaults read "$_plistFramework" "PluginCompatibilityUUID"`
isPatched1=`grep $uuid1 "$_bundlePath/Contents/Info.plist" 2>/dev/null`
isPatched2=`grep $uuid2 "$_bundlePath/Contents/Info.plist" 2>/dev/null`

if [ "" != "$isPatched1" ] && [ "" != "$isPatched2" ] ; then
  echo "[$_bundleId] already patched";
else
  defaults write "$_plistBundle" "SupportedPluginCompatibilityUUIDs" -array-add "$uuid1"
  defaults write "$_plistBundle" "SupportedPluginCompatibilityUUIDs" -array-add "$uuid2"
  plutil -convert xml1 "$_plistBundle.plist"
  echo "[$_bundleId] successfully patched";
fi
################################################################################
