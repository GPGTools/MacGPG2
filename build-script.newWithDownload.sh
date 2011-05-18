#!/bin/bash
##
# Build file for GnuPG 2 on OS X.
# This version downloads the sources. Based on the GnuPG 1 & 2 build script.
#
# @author   Alexander Willner <alex@gpgtools.org>
# @version  2011-05-18
# @see      https://github.com/GPGTools/MacGPG1/blob/master/build-script.sh
# @todo     Download and compile more sources first, e.g. gettext, libusb, ...
# @todo     Create fat binaries for ppc, i386 and x86_64
##

# configuration ################################################################
export prefix_build="`pwd`/build/MacGPG2";
export prefix_install="/usr/local/MacGPG2"
export MACOSX_DEPLOYMENT_TARGET="10.5";
export LOGFILE="build.log";
export configureFlags="--enable-static=no --disable-maintainer-mode --disable-dependency-tracking --prefix=$prefix_install"
export rootPath="`pwd`";
##export CFLAGS="-mmacosx-version-min=10.5 -DUNIX -isysroot /Developer/SDKs/MacOSX10.5.sdk -arch i386 -arch ppc"

iconv_url="ftp://ftp.gnu.org/pub/gnu/libiconv/";
iconv_version="libiconv-1.13.1";
iconv_fileExt=".tar.gz";
iconv_sigExt=".tar.gz.sig"
iconv_build="`pwd`/build/libiconv";
iconv_flags="$configureFlags --enable-extra-encodings";
iconv_patch="";

pth_url="ftp://ftp.gnu.org/gnu/pth/";
pth_version="pth-2.0.7";
pth_fileExt=".tar.gz";
pth_sigExt=".tar.gz.sig"
pth_build="`pwd`/build/pth";
pth_flags="--with-mctx-mth=sjlj --with-mctx-dsp=ssjlj --with-mctx-stk=sas";
pth_patch="pth/Makefile.patch";

gpg_url="ftp://ftp.gnupg.org/gcrypt/gnupg/";
gpg_version="gnupg-2.0.17";
gpg_fileExt=".tar.bz2";
gpg_sigExt=".tar.bz2.sig"
gpg_build="`pwd`/build/gnupg";
gpg_flags="$configureFlags --disable-gpgtar --enable-standard-socket --prefix=$prefix_install --with-pth-prefix=$pth_build/$pth_version/ --with-libiconv-prefix=$iconv_build/$iconv_version/";
gpg_patch="";
################################################################################

# setup ########################################################################
: > $LOGFILE
mkdir -p "$prefix_build";
gpg2 --recv-keys A9C09E30 1CE0C630 >>$LOGFILE 2>&1
if [ "$?" != "0" ]; then
  echo "Could not get keys!";
  exit 1;
fi
################################################################################


# functions ####################################################################
function download {
    echo "   * Downloading...";
    exec 3>&1 4>&2 >>$LOGFILE 2>&1
    mkdir -p "$1" && cd "$1"
    if [ ! -e "$2$3" ]; then
        curl -O "$5$2$3"
        curl -O "$5$2$4"
    fi
    gpg --verify "$2$4"
    if [ "$?" != "0" ]; then
        exec 1>&3 2>&4
        echo "Could not get the sources for '$5$2$3'!";
        exit 1;
    fi
    cd -
    exec 1>&3 2>&4
}

function compile {
    echo "   * Compiling...";
    exec 3>&1 4>&2 >>$LOGFILE 2>&1
    cd "$1" && tar -x$2f "$3$4" && cd "$3" && \
    ./configure "$5"
    if [ "$?" != "0" ]; then
        exec 1>&3 2>&4
        echo "Could not configure the sources for '$1'!";
        exit 1;
    fi
    if [ "$6" != "" ]; then
        patch -p0 < "$rootPath/patches/$6"
    fi
    make -j2 #&& make check
    if [ "$?" != "0" ]; then
        exec 1>&3 2>&4
        echo "Could not compile the sources for '$1'!";
        exit 1;
    fi
    cd -
    exec 1>&3 2>&4
}

function install {
    echo "   * Installing...";
    exec 3>&1 4>&2 >>$LOGFILE 2>&1
    cd "$1/$2" && make prefix="$3" install
    if [ "$?" != "0" ]; then
        exec 1>&3 2>&4
        echo "Could not install the binaries for '$1'!";
        exit 1;
    fi
    cd -
    exec 1>&3 2>&4
}
################################################################################


# libiconv #####################################################################
echo " * Building libiconv...";
download "$iconv_build" "$iconv_version" "$iconv_fileExt" "$iconv_sigExt" "$iconv_url"
compile "$iconv_build" "z" "$iconv_version" "$iconv_fileExt" "$iconv_flags" "$iconv_patch"
cd "$iconv_build" && make prefix="$prefix_build" install >> build.log 2>&1
cd "$rootPath"
################################################################################

# pth ##########################################################################
echo " * Building pth...";
download $pth_build $pth_version $pth_fileExt $pth_sigExt $pth_url
compile $pth_build "z" $pth_version $pth_fileExt $pth_flags $pth_patch
cd $pth_build && make prefix="$prefix_build" install
cd $rootPath
################################################################################

# gpg ##########################################################################
echo " * gpg";
download "$gpg_build" "$gpg_version" "$gpg_fileExt" "$gpg_sigExt" "$gpg_url"
compile "$gpg_build" "j" "$gpg_version" "$gpg_fileExt" "$gpg_flags" "$gpg_patch"
install "$gpg_build" "$gpg_version" "$prefix_build"
################################################################################
