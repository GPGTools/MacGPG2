#!/bin/bash
##
# Build file for GnuPG 2 on OS X.
# This version downloads all sources. Based on the GnuPG 1 & 2 build script.
#
# @author   Alexander Willner <alex@gpgtools.org>
# @version  2011-05-20
# @see      https://github.com/GPGTools/MacGPG1/blob/master/build-script.sh
# @see      https://github.com/GPGTools/MacGPG2/blob/master/build-script.sh
#
# @todo     Compatibility: adopt this script for all supported platforms
# @todo     Fix: libassuan: linker expects $prefix_install/lib/libintl*
# @todo     Fix: libgpg-error: linker expects $prefix_install/lib/libiconv*
# @todo     Fix: libgpg-error: not compatible with "clang -ansi"
# @todo     Fix: pth: not compatible with make -jX
# @todo     Fix: pth: conftest crashes
# @todo     Fix: i386/ppc/x86_64 mode: install-info crashes
# @todo     Mac: pinentry-mac.app is missing
#
# @status   OS X 10.6.7 (i386)            : passed tests on x86_64,
# @status   OS X 10.6.7 (i386/ppc/x86_64) : passwd tests on x86_64
##

# configuration ################################################################
## where to start from
export rootPath="`pwd`";
## where to copy the binaries to
export prefix_build="$rootPath/build/MacGPG2";
## where the binaries will get installed to (e.g. by using an installer)
export prefix_install="/usr/local/MacGPG2";
## to speed up the configuration process
export ccache="$rootPath/build/config.cache";
## logging
export LOGFILE="$rootPath/build.log";
## validation of the sources
export keys="A9C09E30 1CE0C630";
## ...
export PATH="$PATH:$prefix_build/bin";

## to speed up the compile time
#export CC="/usr/bin/clang -ansi"

## for Mac OS X
export DYLD_LIBRARY_PATH="$prefix_build/lib";
export MACOSX_DEPLOYMENT_TARGET="10.5";
export CFLAGS="-mmacosx-version-min=$MACOSX_DEPLOYMENT_TARGET -DUNIX -isysroot /Developer/SDKs/MacOSX$MACOSX_DEPLOYMENT_TARGET.sdk";
export configureFlags="--enable-osx-universal-binaries"; # fat
export CFLAGS="$CFLAGS -arch i386 -arch ppc -arch x86_64" # fat
export CFLAGS="$CFLAGS -arch i386" # easy

## general flags
export configureFlags="$configureFlags --enable-static=no --disable-maintainer-mode --disable-dependency-tracking --prefix=$prefix_install";
export CXXFLAGS="$CFLAGS";
export CPPFLAGS="-I$prefix_build/include";
export LDFLAGS="-L$prefix_build/lib";
export LD_LIBRARY_PATH="$prefix_build/lib";
export LIBUSB_1_0_CFLAGS="-I$prefix_build/include/libusb-1.0/"
export LIBUSB_1_0_LIBS="-L$prefix_build/lib"
################################################################################


# the sources ##################################################################
iconv_url="ftp://ftp.gnu.org/pub/gnu/libiconv/";
iconv_version="libiconv-1.13.1";
iconv_fileExt=".tar.gz";
iconv_sigExt=".tar.gz.sig"
iconv_build="`pwd`/build/libiconv";
iconv_flags="--enable-extra-encodings";
iconv_patch="";

gettext_url="ftp://ftp.gnu.org/pub/gnu/gettext/";
gettext_version="gettext-0.18.1.1";
gettext_fileExt=".tar.gz";
gettext_sigExt=".tar.gz.sig"
gettext_build="`pwd`/build/gettext";
gettext_flags="--disable-csharp --disable-native-java --without-emacs --with-included-gettext --with-included-glib --with-included-libcroco --with-included-libxml --disable-java";
gettext_patch="";

pth_url="ftp://ftp.gnu.org/gnu/pth/";
pth_version="pth-2.0.7";
pth_fileExt=".tar.gz";
pth_sigExt=".tar.gz.sig"
pth_build="`pwd`/build/pth";
pth_flags="--with-mctx-mth=sjlj --with-mctx-dsp=ssjlj --with-mctx-stk=sas";
pth_patch="pth/Makefile.patch";

libusb_url="http://sourceforge.net/projects/libusb/files/libusb-1.0/libusb-1.0.8/";
libusb_version="libusb-1.0.8";
libusb_fileExt=".tar.bz2";
libusb_sigExt=""
libusb_build="`pwd`/build/libusb";
libusb_flags="";
libusb_patch="";

libusbcompat_url="http://sourceforge.net/projects/libusb/files/libusb-compat-0.1/libusb-compat-0.1.3/";
libusbcompat_version="libusb-compat-0.1.3";
libusbcompat_fileExt=".tar.bz2";
libusbcompat_sigExt=""
libusbcompat_build="`pwd`/build/lib-compat";
libusbcompat_flags="";
libusbcompat_patch="";

libgpgerror_url="ftp://ftp.gnupg.org/gcrypt/libgpg-error/";
libgpgerror_version="libgpg-error-1.10";
libgpgerror_fileExt=".tar.bz2";
libgpgerror_sigExt=".tar.bz2.sig"
libgpgerror_build="`pwd`/build/libgpg-error";
libgpgerror_flags="";
libgpgerror_patch="";

libassuan_url="ftp://ftp.gnupg.org/gcrypt/libassuan/";
libassuan_version="libassuan-2.0.1";
libassuan_fileExt=".tar.bz2";
libassuan_sigExt=".tar.bz2.sig"
libassuan_build="`pwd`/build/libassuan";
libassuan_flags="--with-gpg-error-prefix=$prefix_build";
libassuan_patch="";

libgcrypt_url="ftp://ftp.gnupg.org/gcrypt/libgcrypt/";
libgcrypt_version="libgcrypt-1.4.6";
libgcrypt_fileExt=".tar.gz";
libgcrypt_sigExt=".tar.gz.sig"
libgcrypt_build="`pwd`/build/libgcrypt";
libgcrypt_flags="--with-gpg-error-prefix=$prefix_build --with-pth-prefix=$prefix_build --disable-asm --disable-endian-check";
libgcrypt_patch="";

libksba_url="ftp://ftp.gnupg.org/gcrypt/libksba/";
libksba_version="libksba-1.2.0";
libksba_fileExt=".tar.bz2";
libksba_sigExt=".tar.bz2.sig"
libksba_build="`pwd`/build/libksba";
libksba_flags="--with-gpg-error-prefix=$prefix_build";
libksba_patch="";

gpg_url="ftp://ftp.gnupg.org/gcrypt/gnupg/";
gpg_version="gnupg-2.0.17";
gpg_fileExt=".tar.bz2";
gpg_sigExt=".tar.bz2.sig"
gpg_build="`pwd`/build/gnupg";
gpg_flags="--disable-gpgtar --enable-standard-socket --with-pinentry-pgm=$prefix_build/libexec/pinentry-mac.app/Contents/MacOS/pinentry-mac --with-gpg-error-prefix=$prefix_build --with-libgcrypt-prefix=$prefix_build --with-libassuan-prefix=$prefix_build --with-ksba-prefix=$prefix_build --with-pth-prefix=$prefix_build --with-libiconv-prefix=$prefix_build --with-libintl-prefix=$prefix_build";
gpg_patch="";
################################################################################


# init #########################################################################
echo " * Logfile: $LOGFILE"; : > $LOGFILE
echo " * Target: $prefix_build"; mkdir -p "$prefix_build";
#if [ "`which gpg2`" == "" ]; then
#    echo " * No GnuPG2 found.";
#else
#    echo -n " * Getting OpenPGP keys...";
#    gpg2 --list-keys "$keys" >>$LOGFILE 2>&1
#    if [ "$?" == "0" ]; then
#        echo "skipped";
#    else
#        echo "";
#        gpg2 --recv-keys "$keys"  >>$LOGFILE 2>&1
#    fi
#    if [ "$?" != "0" ]; then
#      echo "Could not get keys!";
#      exit 1;
#    fi
#fi
################################################################################


# functions ####################################################################
function download {
    mkdir -p "$1"; cd "$1"
    if [ -e "$2$3" ]; then return 0; fi
    #echo -n "   * Downloading...";
    #if [ -e "$2$3" ]; then echo "skipped"; return 0; else echo ""; fi
    exec 3>&1 4>&2 >>$LOGFILE 2>&1
    echo " ############### Download: $5$2$3"
    curl -s -C - -L -O "$5$2$3"
    if [ "$4" != "" ] && [ "" != "`which gpg2`" ] ; then
        curl -s -O "$5$2$4"
        gpg2 --verify "$2$4"
    fi
    if [ "$?" != "0" ]; then
        exec 1>&3 2>&4
        echo "Could not get the sources for '$5$2$3'!";
        exit 1;
    fi
    exec 1>&3 2>&4
}

function compile {
    echo -n "   * [`date '+%H:%M:%S'`] Configure...";
    cd "$1"
    if [ -e "$3/.installed" ]; then echo "skipped"; return 0; else echo ""; fi
    exec 3>&1 4>&2 >>$LOGFILE 2>&1
    echo " ############### Configue: ./configure --cache-file=$ccache $configureFlags $5"
    tar -x$2f "$3$4" && cd "$3" && ./configure --cache-file=$ccache $configureFlags $5
    if [ "$?" != "0" ]; then
        exec 1>&3 2>&4
        echo "Could not configure the sources for '$1'!";
        exit 1;
    fi
    exec 1>&3 2>&4
    echo "   * [`date '+%H:%M:%S'`] Make...";
    exec 3>&1 4>&2 >>$LOGFILE 2>&1
    echo " ############### Make..."
    if [ "$6" != "" ]; then
        patch -p0 < "$rootPath/patches/$6"
    fi
    make # && make test
    if [ "$?" != "0" ]; then
        exec 1>&3 2>&4
        echo "Could not compile the sources for '$1'!";
        exit 1;
    fi
    exec 1>&3 2>&4
}

function install {
    cd "$1/$2"
    echo -n "   * [`date '+%H:%M:%S'`] Installing...";
    if [ -e '.installed' ]; then echo "skipped"; return 0; else echo ""; fi
    exec 3>&1 4>&2 >>$LOGFILE 2>&1
    echo " ############### Make: make prefix=$3"

    make prefix="$3" install
    if [ "$?" != "0" ]; then
        exec 1>&3 2>&4
        echo "Could not install the binaries for '$1'!";
        exit 1;
    fi
    touch '.installed'
    exec 1>&3 2>&4
}
################################################################################


# download files ###############################################################
echo " * Downloading the sources in the background...";
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
download "$gpg_build" "$gpg_version" "$gpg_fileExt" "$gpg_sigExt" "$gpg_url" &
gpg_pid=${!}
################################################################################

# libiconv #####################################################################
echo " * Working on 'libiconv'...";
compile "$iconv_build" "z" "$iconv_version" "$iconv_fileExt" "$iconv_flags" "$iconv_patch"
install "$iconv_build" "$iconv_version" "$prefix_build"
################################################################################

# gettext ####################################################################
echo " * Working on 'gettext'...";
echo "   * Waiting for download..."; wait "$gettext_pid";
compile "$gettext_build" "j" "$gettext_version" "$gettext_fileExt" "$gettext_flags" "$gettext_patch"
install "$gettext_build" "$gettext_version" "$prefix_build"
################################################################################

# pth ##########################################################################
echo " * Working on 'pth'...";
echo "   * Waiting for download..."; wait "$pth_pid";
compile "$pth_build" "z" "$pth_version" "$pth_fileExt" "$pth_flags" "$pth_patch"
install "$pth_build" "$pth_version" "$prefix_build"
################################################################################

# libusb ##########################################################################
echo " * Working on 'libusb'...";
echo "   * Waiting for download..."; wait "$libusb_pid";
compile "$libusb_build" "z" "$libusb_version" "$libusb_fileExt" "$libusb_flags" "$libusb_patch"
install "$libusb_build" "$libusb_version" "$prefix_build"
################################################################################

# libusb-compat ################################################################
echo " * Working on 'libusb-compat'...";
echo "   * Waiting for download..."; wait "$libusbcompat_pid";
compile "$libusbcompat_build" "j" "$libusbcompat_version" "$libusbcompat_fileExt" "$libusbcompat_flags" "$libusbcompat_patch"
install "$libusbcompat_build" "$libusbcompat_version" "$prefix_build"
################################################################################

# libgpgerror ####################################################################
echo " * Working on 'libgpgerror'...";
echo "   * Starting ugly workaround...";
mkdir -p "$prefix_install/lib/"; cp $prefix_build/lib/libiconv* $prefix_install/lib/;
echo "   * Waiting for download..."; wait "$libgpgerror_pid";
compile "$libgpgerror_build" "j" "$libgpgerror_version" "$libgpgerror_fileExt" "$libgpgerror_flags" "$libgpgerror_patch"
install "$libgpgerror_build" "$libgpgerror_version" "$prefix_build"
################################################################################

# libassuan ####################################################################
echo " * Working on 'libassuan'...";
echo "   * Starting ugly workaround...";
mkdir -p "$prefix_install/lib/"; cp $prefix_build/lib/libint* $prefix_install/lib/;
echo "   * Waiting for download..."; wait "$libassuan_pid";
compile "$libassuan_build" "z" "$libassuan_version" "$libassuan_fileExt" "$libassuan_flags" "$libassuan_patch"
install "$libassuan_build" "$libassuan_version" "$prefix_build"
################################################################################

# libgcrypt ####################################################################
echo " * Working on 'libgcrypt'...";
echo "   * Waiting for download..."; wait "$libgcrypt_pid";
compile "$libgcrypt_build" "z" "$libgcrypt_version" "$libgcrypt_fileExt" "$libgcrypt_flags" "$libgcrypt_patch"
install "$libgcrypt_build" "$libgcrypt_version" "$prefix_build"
################################################################################

# libksba ####################################################################
echo " * Working on 'libksba'...";
echo "   * Waiting for download..."; wait "$libksba_pid";
compile "$libksba_build" "z" "$libksba_version" "$libksba_fileExt" "$libksba_flags" "$libksba_patch"
install "$libksba_build" "$libksba_version" "$prefix_build"
################################################################################

# gpg ##########################################################################
echo " * Working on 'gpg2'...";
echo "   * Deleting cache..."; rm $ccache;
echo "   * Waiting for download..."; wait "$gpg_pid";
compile "$gpg_build" "j" "$gpg_version" "$gpg_fileExt" "$gpg_flags" "$gpg_patch"
install "$gpg_build" "$gpg_version" "$prefix_build"
################################################################################

echo "";
echo "To test: ";
echo " * cd $gpg_build/$gpg_version";
echo " * make check";
echo "To install: ";
echo " * rm -rf $prefix_install";
echo " * mv $prefix_build $prefix_install";
