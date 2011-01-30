#!/bin/bash
#
# This script creates a DMG  for GPGTools
#
# (c) by Felix Co & Alexander Willner & Roman Zechmeister
#

pushd "$1" > /dev/null

#config ------------------------------------------------------------------
name="Example"
version="0.1a"
appName="$name.pkg"
releaseDir="build/Release/";
appPkg="$name.pkgproj"
appPath="$releaseDir/$appName"
appPos="160, 220"
rmName="Uninstall.app"
rmPath="./$rmName"
rmPos="370, 220"
iconSize="80";
dmgName="$name-$version.dmg"
dmgPath="build/$dmgName"
dmgTempPath="build/temp.dmg"
volumeName="$name"
imgBackground="./Dependencies/GPGTools_Core/images/background_dmg.example.png";
imgDmg="./Dependencies/GPGTools_Core/images/icon_dmg.icns";
imgTrash="./Dependencies/GPGTools_Core/images/icon_uninstaller.icns";
imgInstaller="./Dependencies/GPGTools_Core/images/icon_installer.icns";
pathSetIcon="./Dependencies/GPGTools_Core/bin/";
source "Makefile.config"
#-------------------------------------------------------------------------



#-------------------------------------------------------------------------
read -p "Create DMG [y/n]? " input

if [ "x$input" == "xy" -o "x$input" == "xY" ] ;then

	if ( ! test -e Makefile.config ) then
		echo "Wrong directory..."
		exit 1;
	fi
	if [ "" != "$appPkg" ]; then
    	if ( test -e /usr/local/bin/packagesbuild ) then
	    	echo "Building the installer..."
		    /usr/local/bin/packagesbuild "$appPkg"
    	else
	    	echo "ERROR: You need the Application \"Packages\"!"
		    echo "get it at http://s.sudre.free.fr/Software/Packages.html"
    		exit 1
	    fi
	fi

	echo "Removing old files..."
	rm -f "$dmgTempPath"
	rm -f "$dmgPath"
	rm -rf "build/dmgTemp"

	echo "Creating temp folder..."
	mkdir -p build/dmgTemp

	echo "Copying files..."
	if [ "" != "$rmPath" ]; then
        "$pathSetIcon"/setfileicon "$imgTrash" "$rmPath"
        cp -PR "$rmPath" build/dmgTemp/
    fi
    "$pathSetIcon"/setfileicon "$imgInstaller" "$appPath"
    cp -PR "$appPath" build/dmgTemp/
	mkdir build/dmgTemp/.background
	cp "$imgBackground" build/dmgTemp/.background/Background.png
	cp "$imgDmg" build/dmgTemp/.VolumeIcon.icns


	echo "Creating DMG..."
	hdiutil create -scrub -quiet -fs HFS+ -fsargs "-c c=64,a=16,e=16" -format UDRW -srcfolder build/dmgTemp -volname "$volumeName" "$dmgTempPath"
	mountInfo=$(hdiutil attach -readwrite -noverify "$dmgTempPath")
	device=$(echo "$mountInfo" | head -1 | cut -d " " -f 1)
	mountPoint=$(echo "$mountInfo" | tail -1 | sed -En 's/([^	]+[	]+){2}//p')

	echo "Setting attributes..."
		SetFile -a C "$mountPoint"

		osascript >/dev/null << EOT1
		tell application "Finder"
			tell disk "$volumeName"
				open
				set viewOptions to icon view options of container window
				set current view of container window to icon view
				set toolbar visible of container window to false
				set statusbar visible of container window to false
				set bounds of container window to {400, 200, 580 + 400, 320 + 200}
				set arrangement of viewOptions to not arranged
				set icon size of viewOptions to 64
				set text size of viewOptions to 13
				set background picture of viewOptions to file ".background:Background.png"
    			set position of item "$appName" of container window to {$appPos}
			end tell
		end tell
EOT1

if [ "" != "$rmName" ]; then
    osascript >/dev/null << EOT1
	tell application "Finder"
		tell disk "$volumeName"
			set position of item "$rmName" of container window to {$rmPos}
		end tell
	end tell
EOT1
fi

osascript >/dev/null << EOT1
	tell application "Finder"
		tell disk "$volumeName"
			update without registering applications
			close
		end tell
	end tell
EOT1



	chmod -Rf +r,go-w "$mountPoint"
	rm -r "$mountPoint/.Trashes" "$mountPoint/.fseventsd"
	hdiutil detach -quiet "$mountPoint"
	hdiutil convert "$dmgTempPath" -quiet -format UDZO -imagekey zlib-level=9 -o "$dmgPath"

	echo -e "DMG created\n\n"
	open "$dmgPath"

	echo "Cleanup..."
	rm -rf build/dmgTemp
	rm -f "$dmgTempPath"
fi
#-------------------------------------------------------------------------


#-------------------------------------------------------------------------
read -p "Create a detached signature [y/n]? " input

if [ "x$input" == "xy" -o "x$input" == "xY" ] ;then
	echo "Removing old signature..."
	rm -f "$dmgPath.sig"

	echo "Signing..."
	gpg2 -bau 76D78F0500D026C4 -o "${dmgPath}.sig"  "$dmgPath"
fi
#-------------------------------------------------------------------------


#-------------------------------------------------------------------------
## todo: update Makefile.conf
####################################################
read -p "Create Sparkle appcast entry [y/n]? " input

if [ "x$input" == "xy" -o "x$input" == "xY" ] ;then
	PRIVATE_KEY_NAME="$sparkle_keyname"

	signature=$(openssl dgst -sha1 -binary < "$dmgPath" |
	  openssl dgst -dss1 -sign <(security find-generic-password -g -s "$PRIVATE_KEY_NAME" 2>&1 >/dev/null | perl -pe '($_) = /<key>NOTE<\/key>.*<string>(.*)<\/string>/; s/\\012/\n/g') |
	  openssl enc -base64)

	date=$(LC_TIME=en_US date +"%a, %d %b %G %T %z")
	size=$(stat -f "%z" "$dmgPath")
	echo -e "\n"

	cat <<EOT2
<item>
	<title>Version ${version}</title>
	<description>Visit http://www.gpgtools.org/$html.html for further information.</description>
	<sparkle:releaseNotesLink>http://www.gpgtools.org/$html_sparkle.html</sparkle:releaseNotesLink>
	<pubDate>${date}</pubDate>
	<enclosure url="$sparkle_url"
			   sparkle:version="${version}"
			   sparkle:dsaSignature="${signature}"
			   length="${size}"
			   type="application/octet-stream" />
</item>
EOT2

	echo
fi
#-------------------------------------------------------------------------


#-------------------------------------------------------------------------
## todo: implement this
####################################################
read -p "Create github tag [y/n]? " input
if [ "x$input" == "xy" -o "x$input" == "xY" ] ;then
    echo "to be implemented. start this e.g. for each release";
fi
#-------------------------------------------------------------------------


popd > /dev/null
