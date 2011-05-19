#!/bin/bash
##
# Build file for GnuPG 2 on OS X.
# This version downloads the sources. Based on the GnuPG 1 & 2 build script.
#
# @author   Alexander Willner <alex@gpgtools.org>
# @version  2011-05-18
# @see      https://github.com/GPGTools/MacGPG1/blob/master/build-script.sh
# @todo     Create fat binaries for ppc, i386 and x86_64
# @todo     Enhance speed by downloading / compiling in parallel (starting with the smallest file)
# @todo     Add config to an assoc. array and iterate over it
# @todo     Enhance this script for all possible platforms
##

# configuration ################################################################
export keys="A9C09E30 1CE0C630";
export prefix_build="`pwd`/build/MacGPG2";
export prefix_install="/usr/local/MacGPG2";
export LOGFILE="`pwd`/build.log";
export PATH=$PATH:$prefix_build/bin
export configureFlags="--enable-static=no --disable-maintainer-mode --disable-dependency-tracking --prefix=$prefix_install";
export rootPath="`pwd`";
export MACOSX_DEPLOYMENT_TARGET="10.5";
export CFLAGS="-mmacosx-version-min=$MACOSX_DEPLOYMENT_TARGET -DUNIX -isysroot /Developer/SDKs/MacOSX$MACOSX_DEPLOYMENT_TARGET.sdk -arch i386";
export CXXFLAGS="$CFLAGS";
export CPPFLAGS="-I$prefix_build/include";
export LDFLAGS="-L$prefix_build/lib";

#export CFLAGS="-mmacosx-version-min=10.5 -DUNIX -isysroot /Developer/SDKs/MacOSX10.5.sdk -arch i386 -arch ppc"

iconv_url="ftp://ftp.gnu.org/pub/gnu/libiconv/";
iconv_version="libiconv-1.13.1";
iconv_fileExt=".tar.gz";
iconv_sigExt=".tar.gz.sig"
iconv_build="`pwd`/build/libiconv";
iconv_flags="$configureFlags --enable-extra-encodings";
iconv_patch="";

gettext_url="ftp://ftp.gnu.org/pub/gnu/gettext/";
gettext_version="gettext-0.18.1.1";
gettext_fileExt=".tar.gz";
gettext_sigExt=".tar.gz.sig"
gettext_build="`pwd`/build/gettext";
gettext_flags="$configureFlags --disable-csharp --disable-native-java --without-emacs --with-included-gettext --with-included-glib --with-included-libcroco --with-included-libxml --disable-java";
gettext_patch="";

pth_url="ftp://ftp.gnu.org/gnu/pth/";
pth_version="pth-2.0.7";
pth_fileExt=".tar.gz";
pth_sigExt=".tar.gz.sig"
pth_build="`pwd`/build/pth";
pth_flags="$configureFlags --with-mctx-mth=sjlj --with-mctx-dsp=ssjlj --with-mctx-stk=sas";
pth_patch="pth/Makefile.patch";

libusb_url="http://sourceforge.net/projects/libusb/files/libusb-1.0/libusb-1.0.8/";
libusb_version="libusb-1.0.8";
libusb_fileExt=".tar.bz2";
libusb_sigExt=""
libusb_build="`pwd`/build/libusb";
libusb_flags="$configureFlags";
libusb_patch="";

libcompat_url="http://sourceforge.net/projects/libusb/files/libusb-compat-0.1/libusb-compat-0.1.3/";
libcompat_version="libusb-compat-0.1.3";
libcompat_fileExt=".tar.bz2";
libcompat_sigExt=""
libcompat_build="`pwd`/build/lib-compat";
libcompat_flags="$configureFlags";
libcompat_patch="";
export LIBUSB_1_0_CFLAGS="-I$prefix_build/include/libusb-1.0/"
export LIBUSB_1_0_LIBS="-L$prefix_build/lib"

libgpgerror_url="ftp://ftp.gnupg.org/gcrypt/libgpg-error/";
libgpgerror_version="libgpg-error-1.10";
libgpgerror_fileExt=".tar.bz2";
libgpgerror_sigExt=".tar.bz2.sig"
libgpgerror_build="`pwd`/build/libgpg-error";
libgpgerror_flags="$configureFlags";
libgpgerror_patch="";

libassuan_url="ftp://ftp.gnupg.org/gcrypt/libassuan/";
libassuan_version="libassuan-2.0.1";
libassuan_fileExt=".tar.bz2";
libassuan_sigExt=".tar.bz2.sig"
libassuan_build="`pwd`/build/libassuan";
libassuan_flags="$configureFlags --with-gpg-error-prefix=$prefix_build";
libassuan_patch="";

libgcrypt_url="ftp://ftp.gnupg.org/gcrypt/libgcrypt/";
libgcrypt_version="libgcrypt-1.4.6";
libgcrypt_fileExt=".tar.gz";
libgcrypt_sigExt=".tar.gz.sig"
libgcrypt_build="`pwd`/build/libgcrypt";
libgcrypt_flags="$configureFlags --with-gpg-error-prefix=$prefix_build --with-pth-prefix=$prefix_build --disable-asm --disable-endian-check";
libgcrypt_patch="";

libksba_url="ftp://ftp.gnupg.org/gcrypt/libksba/";
libksba_version="libksba-1.2.0";
libksba_fileExt=".tar.bz2";
libksba_sigExt=".tar.bz2.sig"
libksba_build="`pwd`/build/libksba";
libksba_flags="$configureFlags --with-gpg-error-prefix=$prefix_build";
libksba_patch="";

gpg_url="ftp://ftp.gnupg.org/gcrypt/gnupg/";
gpg_version="gnupg-2.0.17";
gpg_fileExt=".tar.bz2";
gpg_sigExt=".tar.bz2.sig"
gpg_build="`pwd`/build/gnupg";
#gpg_flags="$configureFlags --with-pinentry-pgm=$prefix_install/libexec/pinentry-mac.app/Contents/MacOS/pinentry-mac --prefix=$prefix_install --enable-standard-socket --with-gpg-error-prefix=$prefix_install --with-libgcrypt-prefix=$prefix_install --with-libassuan-prefix=$prefix_install --with-ksba-prefix=$prefix_install --with-pth-prefix=$prefix_install --disable-gpgtar --with-libiconv-prefix=$prefix_install --with-libintl-prefix=$prefix_install";
gpg_flags="$configureFlags --with-pinentry-pgm=$prefix_build/libexec/pinentry-mac.app/Contents/MacOS/pinentry-mac --prefix=$prefix_build --enable-standard-socket --with-gpg-error-prefix=$prefix_build --with-libgcrypt-prefix=$prefix_build --with-libassuan-prefix=$prefix_build --with-ksba-prefix=$prefix_build --with-pth-prefix=$prefix_build --disable-gpgtar --with-libiconv-prefix=$prefix_build --with-libintl-prefix=$prefix_build";
gpg_patch="";
################################################################################

# setup ########################################################################
echo "Have a look at '$LOGFILE' for all the details...";
: > $LOGFILE
mkdir -p "$prefix_build";
which -s gpg2
if [ "$?" != "0" ]; then
    echo " * No GnuPG2 found.";
else
    echo -n " * Getting OpenPGP keys...";
    gpg2 --list-keys "$keys" >>$LOGFILE 2>&1
    if [ "$?" == "0" ]; then
        echo "skipped";
    else
        echo "";
        gpg2 --recv-keys "$keys"  >>$LOGFILE 2>&1
    fi
    if [ "$?" != "0" ]; then
      echo "Could not get keys!";
      exit 1;
    fi
fi
################################################################################


# functions ####################################################################
function download {
    echo -n "   * Downloading...";
    mkdir -p "$1"; cd "$1"
    if [ -e "$2$3" ]; then echo "skipped"; return 0; else echo ""; fi
    exec 3>&1 4>&2 >>$LOGFILE 2>&1
    echo " ############### Download: $5$2$3"
    curl -C - -L -O "$5$2$3"
    if [ "$4" != "" ]; then
        curl -O "$5$2$4"
        gpg --verify "$2$4"
    fi
    if [ "$?" != "0" ]; then
        exec 1>&3 2>&4
        echo "Could not get the sources for '$5$2$3'!";
        exit 1;
    fi
    exec 1>&3 2>&4
}

function compile {
    echo -n "   * Compiling...";
    cd "$1"
    if [ -e "$3/.installed" ]; then echo "skipped"; return 0; else echo ""; fi
    exec 3>&1 4>&2 >>$LOGFILE 2>&1
    echo " ############### Configue: ./configure $5"
    tar -x$2f "$3$4" && cd "$3" && ./configure "$5"
    if [ "$?" != "0" ]; then
        exec 1>&3 2>&4
        echo "Could not configure the sources for '$1'!";
        exit 1;
    fi
    if [ "$6" != "" ]; then
        patch -p0 < "$rootPath/patches/$6"
    fi
    echo " ############### Make..."
    make # && make test
    if [ "$?" != "0" ]; then
        exec 1>&3 2>&4
        echo "Could not compile the sources for '$1'!";
        exit 1;
    fi
    exec 1>&3 2>&4
}

function install {
    echo -n "   * Installing...";
    cd "$1/$2"
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


# libiconv #####################################################################
echo " * Working on 'libiconv'...";
download "$iconv_build" "$iconv_version" "$iconv_fileExt" "$iconv_sigExt" "$iconv_url"
compile "$iconv_build" "z" "$iconv_version" "$iconv_fileExt" "$iconv_flags" "$iconv_patch"
install "$iconv_build" "$iconv_version" "$prefix_build"
################################################################################

# gettext ####################################################################
echo " * Working on 'gettext'...";
download "$gettext_build" "$gettext_version" "$gettext_fileExt" "$gettext_sigExt" "$gettext_url"
compile "$gettext_build" "j" "$gettext_version" "$gettext_fileExt" "$gettext_flags" "$gettext_patch"
install "$gettext_build" "$gettext_version" "$prefix_build"
################################################################################

# pth ##########################################################################
echo " * Working on 'pth'...";
download "$pth_build" "$pth_version" "$pth_fileExt" "$pth_sigExt" "$pth_url"
compile "$pth_build" "z" "$pth_version" "$pth_fileExt" "$pth_flags" "$pth_patch"
install "$pth_build" "$pth_version" "$prefix_build"
################################################################################

# libusb ##########################################################################
echo " * Working on 'libusb'...";
download "$libusb_build" "$libusb_version" "$libusb_fileExt" "$libusb_sigExt" "$libusb_url"
compile "$libusb_build" "z" "$libusb_version" "$libusb_fileExt" "$libusb_flags" "$libusb_patch"
install "$libusb_build" "$libusb_version" "$prefix_build"
################################################################################

# libcompat ##########################################################################
echo " * Working on 'libcompat'...";
download "$libcompat_build" "$libcompat_version" "$libcompat_fileExt" "$libcompat_sigExt" "$libcompat_url"
compile "$libcompat_build" "j" "$libcompat_version" "$libcompat_fileExt" "$libcompat_flags" "$libcompat_patch"
install "$libcompat_build" "$libcompat_version" "$prefix_build"
################################################################################

# libgpgerror ####################################################################
echo " * Working on 'libgpgerror'...";
download "$libgpgerror_build" "$libgpgerror_version" "$libgpgerror_fileExt" "$libgpgerror_sigExt" "$libgpgerror_url"
compile "$libgpgerror_build" "j" "$libgpgerror_version" "$libgpgerror_fileExt" "$libgpgerror_flags" "$libgpgerror_patch"
install "$libgpgerror_build" "$libgpgerror_version" "$prefix_build"
################################################################################

# libassuan ####################################################################
echo " * Working on 'libassuan'...";
download "$libassuan_build" "$libassuan_version" "$libassuan_fileExt" "$libassuan_sigExt" "$libassuan_url"
compile "$libassuan_build" "z" "$libassuan_version" "$libassuan_fileExt" "$libassuan_flags" "$libassuan_patch"
install "$libassuan_build" "$libassuan_version" "$prefix_build"
################################################################################

# libgcrypt ####################################################################
echo " * Working on 'libgcrypt'...";
download "$libgcrypt_build" "$libgcrypt_version" "$libgcrypt_fileExt" "$libgcrypt_sigExt" "$libgcrypt_url"
compile "$libgcrypt_build" "z" "$libgcrypt_version" "$libgcrypt_fileExt" "$libgcrypt_flags" "$libgcrypt_patch"
install "$libgcrypt_build" "$libgcrypt_version" "$prefix_build"
################################################################################

# libksba ####################################################################
echo " * Working on 'libksba'...";
download "$libksba_build" "$libksba_version" "$libksba_fileExt" "$libksba_sigExt" "$libksba_url"
compile "$libksba_build" "z" "$libksba_version" "$libksba_fileExt" "$libksba_flags" "$libksba_patch"
install "$libksba_build" "$libksba_version" "$prefix_build"
################################################################################

# gpg ##########################################################################
echo " * Working on 'gpg2'...";
download "$gpg_build" "$gpg_version" "$gpg_fileExt" "$gpg_sigExt" "$gpg_url"
compile "$gpg_build" "j" "$gpg_version" "$gpg_fileExt" "$gpg_flags" "$gpg_patch"
install "$gpg_build" "$gpg_version" "$prefix_build"
################################################################################
