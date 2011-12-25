#!/bin/bash
##
# Click'n'go build file for GnuPG 2.
# Based on the MacGPG 1 & 2 build script.
#
# @usage    ./build-script.sh clean
#
# @author   Alex <alex@gpgtools.org> and Mento <mento@gpgtools.org>
# @version  2011-12-23
# @see      https://github.com/GPGTools/MacGPG1/blob/master/build-script.sh
# @see      https://github.com/GPGTools/MacGPG2/blob/master/build-script.sh
#
# @todo     Major: pinentry-mac.app is missing
# @todo	    Major: Can only compile libgcrypt wth "-arch i386 -arch x86_64" and not "-arch i386"
# @todo     Minor: clang can not compile gnupg
# @todo		Minor: Compile libgcrypt without --disable-aesni-support
#
# @todo     Enhancement: configure/compile more in the background (e.g. gettext)
# @todo     Enhancement: re-enable gpg validation of the sources
#
# Tipps to compile for ppc on 10.7:
#	Follow this guid: http://hints.macworld.com/article.php?story=20110318050811544
#	cd /Developer/SDKs/MacOSX10.5.sdk/usr/lib/gcc
#	sudo ln -s i686-apple-darwin11 i686-apple-darwin10
#	sudo ln -s powerpc-apple-darwin11 powerpc-apple-darwin10
##

# configuration ################################################################
## where to start from
export rootPath="$(pwd)"
export buildDir="$rootPath/build"
## where to copy the binaries to
export prefix_build="$buildDir/MacGPG2"
## where the binaries will get installed to (e.g. by using an installer)
export prefix_install="/usr/local/MacGPG2"
## to speed up the configuration process
export ccache="$buildDir/config.cache"
## logging
export LOGPATH="$buildDir"
export LOGFILE="$LOGPATH/build.log"
## validation of the sources
export GNUPGHOME="$buildDir"
## ...
export PATH="$PATH:$prefix_install/bin"
export DYLD_LIBRARY_PATH="$prefix_install/lib"


export MACOSX_DEPLOYMENT_TARGET="10.5"

export CFLAGS="-arch i386 -arch ppc"
export configureFlags="--enable-osx-universal-binaries"

export CFLAGS="-mmacosx-version-min=$MACOSX_DEPLOYMENT_TARGET -DUNIX -isysroot /Developer/SDKs/MacOSX$MACOSX_DEPLOYMENT_TARGET.sdk $CFLAGS"

#export CC="/usr/bin/clang -ansi" # faster modern compiler
export CC=gcc-4.2


## general flags
export configureFlagsNoCache="$configureFlags --enable-static=no --disable-maintainer-mode --disable-dependency-tracking --prefix=$prefix_install"
export configureFlags="$configureFlagsNoCache --cache-file=$ccache"
export CXXFLAGS="$CFLAGS"
export CPPFLAGS="-I$prefix_install/include"
export LDFLAGS="-L$prefix_install/lib"
export LD_LIBRARY_PATH="$prefix_install/lib"
export LIBUSB_1_0_CFLAGS="-I$prefix_install/include/libusb-1.0/"
export LIBUSB_1_0_LIBS="-L$prefix_install/lib"
################################################################################


# the sources ##################################################################
iconv_url="ftp://ftp.gnu.org/pub/gnu/libiconv/"
iconv_version="libiconv-1.14"
iconv_fileExt=".tar.gz"
iconv_sigExt=".tar.gz.sig"
iconv_build="$buildDir/libiconv"
iconv_flags1="$configureFlagsNoCache --enable-extra-encodings"
iconv_flags2="$configureFlags --enable-extra-encodings"
iconv_patch=""

gettext_url="ftp://ftp.gnu.org/pub/gnu/gettext/"
gettext_version="gettext-0.18.1.1"
gettext_fileExt=".tar.gz"
gettext_sigExt=".tar.gz.sig"
gettext_build="$buildDir/gettext"
gettext_flags="$configureFlagsNoCache --disable-csharp --disable-native-java --without-emacs --with-included-gettext --with-included-glib --with-included-libcroco --with-included-libxml --disable-java"
gettext_patch=""

pth_url="ftp://ftp.gnu.org/gnu/pth/"
pth_version="pth-2.0.7"
pth_fileExt=".tar.gz"
pth_sigExt=".tar.gz.sig"
pth_build="$buildDir/pth"
pth_flags="$configureFlags --with-mctx-mth=sjlj --with-mctx-dsp=ssjlj --with-mctx-stk=sas"
pth_patch="pth/Makefile.in.patch"

libusb_url="http://sourceforge.net/projects/libusb/files/libusb-1.0/libusb-1.0.8/"
libusb_version="libusb-1.0.8"
libusb_fileExt=".tar.bz2"
libusb_sigExt=""
libusb_build="$buildDir/libusb"
libusb_flags="$configureFlags"
libusb_patch=""

libusbcompat_url="http://sourceforge.net/projects/libusb/files/libusb-compat-0.1/libusb-compat-0.1.3/"
libusbcompat_version="libusb-compat-0.1.3"
libusbcompat_fileExt=".tar.bz2"
libusbcompat_sigExt=""
libusbcompat_build="$buildDir/lib-compat"
libusbcompat_flags="$configureFlags"
libusbcompat_patch=""

libgpgerror_url="ftp://ftp.gnupg.org/gcrypt/libgpg-error/"
libgpgerror_version="libgpg-error-1.10"
libgpgerror_fileExt=".tar.bz2"
libgpgerror_sigExt=".tar.bz2.sig"
libgpgerror_build="$buildDir/libgpg-error"
libgpgerror_flags="$configureFlagsNoCache"
libgpgerror_patch="libgpg-error/inline.patch"

libassuan_url="ftp://ftp.gnupg.org/gcrypt/libassuan/"
libassuan_version="libassuan-2.0.3"
libassuan_fileExt=".tar.bz2"
libassuan_sigExt=".tar.bz2.sig"
libassuan_build="$buildDir/libassuan"
libassuan_flags="$configureFlags --with-gpg-error-prefix=$prefix_install"
libassuan_patch=""

libgcrypt_url="ftp://ftp.gnupg.org/gcrypt/libgcrypt/"
libgcrypt_version="libgcrypt-1.5.0"
libgcrypt_fileExt=".tar.gz"
libgcrypt_sigExt=".tar.gz.sig"
libgcrypt_build="$buildDir/libgcrypt"
libgcrypt_flags="$configureFlags --disable-aesni-support --with-gpg-error-prefix=$prefix_install --with-pth-prefix=$prefix_install --disable-asm" #--disable-endian-check
libgcrypt_patch="libgcrypt/IDEA.patch"

libksba_url="ftp://ftp.gnupg.org/gcrypt/libksba/"
libksba_version="libksba-1.2.0"
libksba_fileExt=".tar.bz2"
libksba_sigExt=".tar.bz2.sig"
libksba_build="$buildDir/libksba"
libksba_flags="$configureFlags --with-gpg-error-prefix=$prefix_install"
libksba_patch=""

zlib_url="http://zlib.net/"
zlib_version="zlib-1.2.5"
zlib_fileExt=".tar.gz"
zlib_sigExt=""
zlib_build="$buildDir/zlib"
zlib_flags=""
zlib_patch=""

gpg_url="ftp://ftp.gnupg.org/gcrypt/gnupg/"
gpg_version="gnupg-2.0.18"
gpg_fileExt=".tar.bz2"
gpg_sigExt=".tar.bz2.sig"
gpg_build="$buildDir/gnupg"
gpg_flags="$configureFlags --disable-gpgtar --enable-standard-socket --with-pinentry-pgm=$prefix_install/libexec/pinentry-mac.app/Contents/MacOS/pinentry-mac --with-gpg-error-prefix=$prefix_install --with-libgcrypt-prefix=$prefix_install --with-libassuan-prefix=$prefix_install --with-ksba-prefix=$prefix_install --with-pth-prefix=$prefix_install --with-iconv-dir=$prefix_install --with-zlib=$prefix_install --with-libiconv-prefix=$prefix_install --with-libintl-prefix=$prefix_install"
gpg_patch="gnupg/AllInOne.patch"
################################################################################


## testing environment
echo " * Testing environment..."
echo -n "   * GCC: "
echo "main() {return 0;}" | $CC $CFLAGS -xc -o /dev/null - 2>$LOGFILE
if [ "$?" == 0 ]; then echo "OK"; else echo "FAIL (see $LOGFILE)"; fi
exit 0
################################################################################


# init #########################################################################
if [ "$1" == "clean" ]; then
    echo -n " * Cleaning..."
    rm -f "$ccache"
    rm -rf "$prefix_build"
    rm -f "$LOGPATH/"*.log
    rm -rf "$iconv_build/$iconv_version"
    rm -rf "$gettext_build/$gettext_version"
    rm -rf "$pth_build/$pth_version"
    rm -rf "$libusb_build/$libusb_version"
    rm -rf "$libusbcompat_build/$libusbcompat_version"
    rm -rf "$libgpgerror_build/$libgpgerror_version"
    rm -rf "$libassuan_build/$libassuan_version"
    rm -rf "$libgcrypt_build/$libgcrypt_version"
    rm -rf "$libksba_build/$libksba_version"
    rm -rf "$zlib_build/$zlib_version"
    rm -rf "$gpg_build/$gpg_version"
	echo " OK"
	exit 0
fi


echo " * Logfiles: $LOGPATH/build-xyz.log"
echo " * Target: $prefix_build"

if [ -L "$prefix_install" ]; then
	sudo rm "$prefix_install"
elif [ -e "$prefix_install" ]; then
	sudo mv "$prefix_install" "$prefix_install.bak"
fi

mkdir -p "$prefix_build/lib"
sudo ln -Fs "$prefix_build" "$prefix_install"
cp "$rootPath/Keys.gpg" "$buildDir/pubring.gpg"
################################################################################






# functions ####################################################################

function setLogPipe {
    LOGFILE="$LOGPATH/build${1:+-$1}.log"
	exec 3>&1 4>&2 >>"$LOGPATH/build${1:+-$1}.log" 2>&1
}
function resetLogPipe {
	exec 1>&3 2>&4
}
function errExit {
	resetLogPipe
	echo "$1"
	echo "See $LOGFILE for details."
	exit 1
}
function waitfor {
	if ps "$2" >/dev/null; then
		echo "   * [`date '+%H:%M:%S'`] Waiting for download of '$1'..."
		wait "$2"
	fi
}

function download {
    mkdir -p "$1"
	cd "$1"
    if [ -e "$2$3" ]; then
		return 0
	fi
    setLogPipe

    echo " ############### Download: $5$2$3"

    curl -s -C - -L -O "$5$2$3" ||
		errExit "Could not get the sources for '$5$2$3'!"


    if [ "$4" != "" ]; then
        curl -s -O "$5$2$4"

		if GPG=$(which gpg2) || GPG=$(which gpg); then
			"$GPG" -q --no-permission-warning --no-auto-check-trustdb --trust-model always --verify "$2$4" >/dev/null 2>&1 ||
				errExit "Signature of '$2$3' invalid!"
		fi
    fi

    resetLogPipe
}

function compile {
    cd "$1"
    echo -n "   * [`date '+%H:%M:%S'`] Configuring '$2'..."
    if [ -e "$2/.installed" ]; then
		echo " skipped"
		return 0
	fi
	echo
    setLogPipe "$2"


    echo " ############### Extract"
    tar -xf "$2$3" ||
		errExit "Could not extract the sources for '$1'!"
	cd "$2"


    if [ "$5" != "" ]; then
		echo " ############### Patch"
		patch -p0 < "$rootPath/Patches/$5" ||
			errExit "Could not patch the sources for '$1'!"
    fi


    echo " ############### Configure: ./configure $4"
	./configure $4 ||
		errExit "Could not configure the sources for '$1'!"


    echo "   * [`date '+%H:%M:%S'`] Compiling '$2'..." >&3
    echo " ############### Make: make -j4 $6"
    make -j4 $6 ||
		errExit "Could not compile the sources for '$1'!"


    resetLogPipe
}

function install {
    cd "$1/$2"
    echo -n "   * [`date '+%H:%M:%S'`] Installing '$2'..."
    if [ -e '.installed' ]; then
		echo "skipped"
		return 0
	fi
	echo
    setLogPipe "$2"

    echo " ############### Make: make -e prefix=$prefix_install install"

    make -e prefix="$prefix_install" install ||
        errExit "Could not install the binaries for '$1'!"

    touch '.installed'

    resetLogPipe
}
################################################################################


# download files ###############################################################
echo " * Downloading the sources in the background..."
#first one is the buffer
download "$iconv_build" "$iconv_version" "$iconv_fileExt" "$iconv_sigExt" "$iconv_url"
download "$gettext_build" "$gettext_version" "$gettext_fileExt" "$gettext_sigExt" "$gettext_url" &
gettext_pid=${!}
download "$pth_build" "$pth_version" "$pth_fileExt" "$pth_sigExt" "$pth_url" &
pth_pid=${!}
download "$libusb_build" "$libusb_version" "$libusb_fileExt" "$libusb_sigExt" "$libusb_url" &
libusb_pid=${!}
download "$libusbcompat_build" "$libusbcompat_version" "$libusbcompat_fileExt" "$libusbcompat_sigExt" "$libusbcompat_url" &
libusbcompat_pid=${!}
download "$libgpgerror_build" "$libgpgerror_version" "$libgpgerror_fileExt" "$libgpgerror_sigExt" "$libgpgerror_url" &
libgpgerror_pid=${!}
download "$libassuan_build" "$libassuan_version" "$libassuan_fileExt" "$libassuan_sigExt" "$libassuan_url" &
libassuan_pid=${!}
download "$libgcrypt_build" "$libgcrypt_version" "$libgcrypt_fileExt" "$libgcrypt_sigExt" "$libgcrypt_url" &
libgcrypt_pid=${!}
download "$libksba_build" "$libksba_version" "$libksba_fileExt" "$libksba_sigExt" "$libksba_url" &
libksba_pid=${!}
download "$zlib_build" "$zlib_version" "$zlib_fileExt" "$zlib_sigExt" "$zlib_url" &
zlib_pid=${!}
download "$gpg_build" "$gpg_version" "$gpg_fileExt" "$gpg_sigExt" "$gpg_url" &
gpg_pid=${!}
################################################################################


# libiconv #####################################################################
echo " * Working on 'libiconv' (first run)..."
compile "$iconv_build" "$iconv_version" "$iconv_fileExt" "$iconv_flags1" "$iconv_patch"
[ -e "$iconv_build/$iconv_version/.installed" ]
alreadyInstalled=$?
install "$iconv_build" "$iconv_version"
[ $alreadyInstalled -eq 1 ] && rm "$iconv_build/$iconv_version/.installed"
#[ -e $ccache ] && rm $ccache
################################################################################


# gettext ####################################################################
echo " * Working on 'gettext'..."
waitfor "$gettext_version" "$gettext_pid"
compile "$gettext_build" "$gettext_version" "$gettext_fileExt" "$gettext_flags" "$gettext_patch"
install "$gettext_build" "$gettext_version"
#[ -e $ccache ] && rm $ccache
################################################################################


# libiconv #####################################################################
echo " * Working on 'libiconv' (second run)..."
compile "$iconv_build" "$iconv_version" "$iconv_fileExt" "$iconv_flags2" "$iconv_patch"
install "$iconv_build" "$iconv_version"
################################################################################


# pth ##########################################################################
echo " * Working on 'pth'..."
waitfor "$pth_version" "$pth_pid"
compile "$pth_build" "$pth_version" "$pth_fileExt" "$pth_flags" "$pth_patch" "-j1"
install "$pth_build" "$pth_version"
################################################################################


# libusb ##########################################################################
echo " * Working on 'libusb'..."
waitfor "$libusb_version" "$libusb_pid"
compile "$libusb_build" "$libusb_version" "$libusb_fileExt" "$libusb_flags" "$libusb_patch"
install "$libusb_build" "$libusb_version"
################################################################################


# libusb-compat ################################################################
echo " * Working on 'libusb-compat'..."
waitfor "$libusbcompat_version" "$libusbcompat_pid"
compile "$libusbcompat_build" "$libusbcompat_version" "$libusbcompat_fileExt" "$libusbcompat_flags" "$libusbcompat_patch"
install "$libusbcompat_build" "$libusbcompat_version"
################################################################################


# libgpgerror ####################################################################
echo " * Working on 'libgpgerror'..."
waitfor "$libgpgerror_version" "$libgpgerror_pid"
oldCFLAGS=$CFLAGS
export CFLAGS="$CFLAGS -arch x86_64"
compile "$libgpgerror_build" "$libgpgerror_version" "$libgpgerror_fileExt" "$libgpgerror_flags" "$libgpgerror_patch"
export CFLAGS=$oldCFLAGS
install "$libgpgerror_build" "$libgpgerror_version"
################################################################################


# libassuan ####################################################################
echo " * Working on 'libassuan'..."
waitfor "$libassuan_version" "$libassuan_pid"
compile "$libassuan_build" "$libassuan_version" "$libassuan_fileExt" "$libassuan_flags" "$libassuan_patch"
install "$libassuan_build" "$libassuan_version"
################################################################################


# libgcrypt ####################################################################
echo " * Working on 'libgcrypt'..."
waitfor "$libgcrypt_version" "$libgcrypt_pid"
compile "$libgcrypt_build" "$libgcrypt_version" "$libgcrypt_fileExt" "$libgcrypt_flags" "$libgcrypt_patch"
install "$libgcrypt_build" "$libgcrypt_version"
################################################################################


# libksba ####################################################################
echo " * Working on 'libksba'..."
waitfor "$libksba_version" "$libksba_pid"
compile "$libksba_build" "$libksba_version" "$libksba_fileExt" "$libksba_flags" "$libksba_patch"
install "$libksba_build" "$libksba_version"
################################################################################


# zlib ##########################################################################
echo " * Working on 'zlib'..."
waitfor "$zlib_version" "$zlib_pid"
compile "$zlib_build" "$zlib_version" "$zlib_fileExt" "$zlib_flags" "$zlib_patch"
install "$zlib_build" "$zlib_version"
################################################################################


# gpg ##########################################################################
echo " * Working on 'gpg2'..."
waitfor "$gpg_version" "$gpg_pid"
compile "$gpg_build" "$gpg_version" "$gpg_fileExt" "$gpg_flags" "$gpg_patch"
install "$gpg_build" "$gpg_version"
################################################################################


echo " * Checking..."
setLogPipe "check"
cd $gpg_build/$gpg_version
make check ||
	errExit "Check of GnuPG failed!"
resetLogPipe


if sudo rm -f "$prefix_install"; then
	if [ -e "$prefix_install.bak" ]; then
		sudo mv "$prefix_install.bak" "$prefix_install"
	fi
fi




