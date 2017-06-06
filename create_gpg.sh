#!/bin/bash

# This file is licensed under the GNU Lesser General Public License v3.0
# See https://www.gnu.org/licenses/lgpl-3.0.txt for a copy of the license
#
# Usage: ./create_gpg



BASE_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")"; pwd -P)
CACHE_DIR=${CACHE_DIR:-$BASE_DIR/../Caches}
WORKING_DIR=$BASE_DIR/build
BUILD_DIR=$WORKING_DIR/build
DIST_DIR=$WORKING_DIR/dist
FINAL_DIR=$WORKING_DIR/MacGPG2
TARGET_DIR=/usr/local/MacGPG2




# Files in bin and libexec, which are copied to the MacGPG2 dir.
BIN_FILES=(gpg gpgv gpg-agent gpg-connect-agent gpg-error gpgconf gpgparsemail gpgsm gpgtar kbxutil watchgnupg dirmngr-client dirmngr mpicalc dumpsexp hmac256)
LIBEXEC_FILES=(dirmngr_ldap gpg-preset-passphrase scdaemon gpg-check-pattern gpg-protect-tool gpg-wks-client)




# Some build parameters.
ARCH="x86_64"
ABI="64"
export MACOSX_DEPLOYMENT_TARGET=10.9
# Additional flags to add to the CFLAGS parameter
ADDITIONAL_CFLAGS="-Ofast -mmacosx-version-min=10.9"



# Prepare logging.
mkdir -p "$WORKING_DIR" "$BUILD_DIR" "$DIST_DIR" "$CACHE_DIR"
if [[ -t 2 ]]; then
	HAVE_TERMINAL=1
	exec &> >(tee >(tr -u "\033" "#" | sed -E 's/#\[[0-9][^m]*m//g' >> "$WORKING_DIR/create_gpg.log"))
else
	HAVE_TERMINAL=0
	exec &> >(tee -a "$WORKING_DIR/create_gpg.log")
fi

echo -n "Build started at "; date


set -o pipefail
NCPU=$(sysctl hw.ncpu | awk '{print $2}')




# Helper functions.
function errExit {
	msg="$* (${BASH_SOURCE[1]##*/}: line ${BASH_LINENO[0]})"
	if [[ "$HAVE_TERMINAL" == "1" ]] ;then
		echo -e "\033[1;31m$msg\033[0m" >&2
	else
		echo -e "$msg" >&2
	fi
	exit 1
}
function doFail {
	msg="\n** ERROR at $* ** - build failed (${BASH_SOURCE[1]##*/}: line ${BASH_LINENO[0]})"
	if [[ "$HAVE_TERMINAL" == "1" ]] ;then
		echo -e "\033[1;31m$msg\033[0m" >&2
	else
		echo -e "$msg" >&2
	fi
	exit 1
}
function pycmd {
	python -c "c=$config
$1"
}


# Functions for the real work.
function downloadLib {
	if [[ -f "$archivePath" ]]; then
		echo "Skipping $archiveName"
	else
		echo "Downloading $archiveName"
		curl -L -o "$archivePath" "$fullUrl"
	fi
	
	if [[ -n "$lib_sha256" ]]; then
		hash=$lib_sha256
		algo=256
	else
		hash=$lib_sha1
		algo=1
	fi
	
	fileHash=$(shasum -a $algo "$archivePath" | cut -d ' ' -f 1)
	if [[ "$hash" != "$fileHash" ]]; then
		echo "SHA-$algo sums of file $archiveName don't match" >&2
		echo "expected: $hash" >&2
		echo "obtained: $fileHash" >&2
		doFail "downloadLib"
	fi
}
function unpackLib {
	libDirs=("$BUILD_DIR/$lib_name-"*)
	if [[ -d "$libDirs" ]]; then
	    echo "removing dir ${libDirs[@]##*/}"
		rm -rf "${libDirs[@]}"
	fi
	
    echo "unpacking $archiveName"
	suffix=${archiveName##*.tar.}
		
	case "$suffix" in
	lz)
		lunzip=lunzip
		if ! command -v lunzip &>/dev/null; then
			lunzip="lzip -d"
		fi
		$lunzip -k -c "$archivePath" | tar -C "$BUILD_DIR" -xf - || doFail "$archiveName"
		;;
	bz2)
		tar -C "$BUILD_DIR" -jxf "$archivePath" || doFail "$archiveName"
		;;
	gz|xz)
		tar -C "$BUILD_DIR" -zxf "$archivePath" || doFail "$archiveName"
		;;
	*)
		doFail "$archiveName"
	esac	
}
function patchLib {
	patches=("$BASE_DIR/patches/$lib_name/"*.patch)
  
	if [[ -f "$patches" ]]; then
		echo "patching $libDirName"
		for patch in "${patches[@]}"; do
			echo "Applying patch $patch"
			patch -d "$libDirPath" -p1 -t -N < "$patch" || doFail "patchLib"
		done
	fi
	
}
function buildLib {
	oldPATH=$PATH
	export PATH=$DIST_DIR/bin:$PATH
    pushd $libDirPath >/dev/null
	
    echo "building $lib_name"
	
    make distclean &>/dev/null
	
	n_cpu=${lib_n_cpu:-$NCPU}

    if [[ ! -f "configure" ]]; then
		./autogen.sh
    fi
	

	moreCFlags="$ADDITIONAL_CFLAGS"
	if [ ${lib_name:0:6} = "sqlite" ]; then
		moreCFlags=
	fi
  
    CFLAGS="-arch $ARCH -m$ABI -ULOCALEDIR -DLOCALEDIR=\\\"${TARGET_DIR}/share/locale\\\" $moreCFlags" \
    CXXFLAGS="-arch $ARCH" \
    ABI=$ABI \
    LDFLAGS="-L$DIST_DIR/lib -arch $ARCH" \
    CPPFLAGS="-I$DIST_DIR/include -arch $ARCH" \
    PKG_CONFIG_PATH=$DIST_DIR/lib/pkgconfig \
	ac_cv_search_clock_gettime=no \
	ac_cv_func_clock_gettime=no \
    ./configure --prefix=$DIST_DIR \
    	$lib_configure_args || doFail "buildLib configure"

    make -j $n_cpu || doFail "buildLib make"
	make install || doFail "buildLib install"

	popd >/dev/null
	PATH=$oldPATH
}
function buildGnuPG {
	# Remove some unnecessary files.
	rm -fr "$DIST_DIR/share/man" "$DIST_DIR/share/locale" || doFail "buildGnuPG rm"
	
	oldPATH=$PATH
    export PATH=$BASE_DIR/bin:$PATH
    pushd $libDirPath >/dev/null
	
    GPGCONFIGPARAM=""
    if [[ ! -f ./configure ]]; then
      GPGCONFIGPARAM="--enable-maintainer-mode"
      GETTEXT_PREFIX=$DIST_DIR/bin/ ./autogen.sh --force || doFail "buildGnuPG autogen"
    fi


    CFLAGS="-arch $ARCH $ADDITIONAL_CFLAGS -ULOCALEDIR -DLOCALEDIR=\\\"${TARGET_DIR}/share/locale\\\"" \
    CXXFLAGS="-arch $ARCH" \
    ABI=$ABI \
    LDFLAGS="-L$DIST_DIR/lib -arch $ARCH" \
    CPPFLAGS="-I$DIST_DIR/include -arch $ARCH" \
    PKG_CONFIG_PATH=$DIST_DIR/lib/pkgconfig \
	ac_cv_search_clock_gettime=no \
	ac_cv_func_clock_gettime=no \
	./configure \
      	--prefix=$DIST_DIR \
        --localstatedir=/var \
        --sysconfdir=${TARGET_DIR}/etc \
        --disable-rpath \
        --with-pinentry-pgm=${TARGET_DIR}/libexec/pinentry-mac.app/Contents/MacOS/pinentry-mac \
        --with-agent-pgm=${TARGET_DIR}/bin/gpg-agent \
        --with-scdaemon-pgm=${TARGET_DIR}/libexec/scdaemon \
        --with-dirmngr-pgm=${TARGET_DIR}/bin/dirmngr \
        --with-dirmngr-ldap-pgm=${TARGET_DIR}/libexec/dirmngr_ldap \
        --with-protect-tool-pgm=${TARGET_DIR}/libexec/gpg-protect-tool \
        --enable-gpg2-is-gpg \
        --with-libgpg-error-prefix=$DIST_DIR \
        --with-libgcrypt-prefix=$DIST_DIR \
        --with-libassuan-prefix=$DIST_DIR \
        --with-ksba-prefix=$DIST_DIR \
        --with-npth-prefix=$DIST_DIR \
        --with-readline=$DIST_DIR \
        --with-libintl-prefix=$DIST_DIR \
        $GPGCONFIGPARAM \
        --with-libiconv-prefix=$DIST_DIR || doFail "buildGnuPG configure"
	
	n_cpu=${lib_n_cpu:-$NCPU}
    make -j $NCPU || doFail "buildGnuPG make"
    make install || doFail "buildGnuPG install"	

	
	PATH=$oldPATH
}



if [[ "$(type -t pkg-config)" != "file" ]]; then
	errExit "ERROR: pkg-config is not installed. This script requires pkg-config to be available.\nPlease fix your setup and try again."
fi



# Read libs.json
config=$(python -c "import sys,json; print(json.loads(sys.stdin.read()))" < "$BASE_DIR/libs.json" 2>/dev/null) || doFail "read libs.json"
count=$(pycmd "print(len(c))") || doFail "process libs.json" 

# Loop over all libs
for (( i=0; i<count; i++ )); do
	# Set the variables lib_name, lib_url, etc.
	unset $(compgen -A variable | grep "^lib_") 
	eval $(pycmd "for k, v in c[$i].iteritems(): print('lib_%s=\'%s\'' % (k, v))")
	
	fullUrl=${lib_url/\$\{VERSION\}/$lib_version}
	archiveName=${fullUrl##*/}
	archivePath=$CACHE_DIR/$archiveName
	libDirName=${archiveName%%.tar*}
	libDirPath=$BUILD_DIR/$libDirName
	
	
	if [[ -n "$lib_installed_file" && -d "$libDirPath" && -f "$DIST_DIR/$lib_installed_file" ]]; then
		echo "Already built $libDirName"
		continue
	fi
	downloadLib
	
	unpackLib
	
	patchLib
	
	if [[ "$lib_name" == "gnupg" ]]; then
		buildGnuPG
	else 
		buildLib
	fi

done



# some files get wrong permissions, let's just fix them ...
find "$DIST_DIR" -type f -exec chmod +w {} +



# Adjust ids using otool for libraries in lib/
pushd "$DIST_DIR/lib" >/dev/null
for dylib in *.dylib; do
	if [[ ! -L "$dylib" ]]; then
		echo "adapting ID for $dylib"
		install_name_tool -id "${TARGET_DIR}/lib/$dylib" "$dylib"
	fi
done
popd >/dev/null



# Adjust rpath using otool for libraries and binaries.
pushd "$DIST_DIR" >/dev/null
NL="
"
have_rpath_regex="cmd LC_RPATH${NL}[^${NL}]+${NL} *path @loader_path/../lib"
dylib_files=(lib/*.dylib)

for file in "${BIN_FILES[@]/#/bin/}" "${LIBEXEC_FILES[@]/#/libexec/}" "${dylib_files[@]}"; do
	if [[ -L "$file" ]]; then
		continue
	fi
	
	need_rpath=0
	for loadPath in $(otool -L "$file" | cut -d' ' -f1); do
		if [[ "$loadPath" =~ "$DIST_DIR/lib/" ]]; then
			if [[ $need_rpath -eq 0 ]]; then
				echo "adapting ld-paths for $file"
				need_rpath=1
			fi
			rpath=@rpath/${loadPath##*/}
			install_name_tool -change "$loadPath" "$rpath" "$file"
		fi
	done
	
	# Add rpath if necessary.
	if [[ $need_rpath -eq 1 && ! "$(otool -l "$file")" =~ $have_rpath_regex ]]; then
		echo "adding rpath to $file"
		install_name_tool -add_rpath @loader_path/../lib "$file"
	fi
done
popd >/dev/null



echo "copying files to ${FINAL_DIR##*/}"
rm -rf "$FINAL_DIR" &>/dev/null
mkdir -p "$FINAL_DIR"
pushd "$DIST_DIR" >/dev/null

tar -cf - \
	"${BIN_FILES[@]/#/bin/}" \
	"${LIBEXEC_FILES[@]/#/libexec/}" \
	lib/*dylib \
	share/gnupg \
	share/man \
	share/locale | tar -C "$FINAL_DIR" -xf -

ln -s gpg "$FINAL_DIR/bin/gpg2"
cp "$BASE_DIR/scripts/convert-keyring" "$DIST_DIR/bin/"

rm -f "$FINAL_DIR/lib/libgettext"*
popd >/dev/null



# ensure that all executables and libraries have correct lib-path settings
pushd "$FINAL_DIR" >/dev/null
otool -L $(ls -d bin/* | egrep -v '(convert-keyring)') | grep "$WORKING_DIR" && doFail "otool bin"
otool -L libexec/* | grep "$WORKING_DIR" && doFail "otool libexec"
otool -L lib/* | grep "$WORKING_DIR" && doFail "otool lib"
popd >/dev/null


