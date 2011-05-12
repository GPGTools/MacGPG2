#!/bin/sh
##
# Build file for GnuPG 2 on OS X.
# This version downloads the sources. Based on the GnuPG 1 build script.
#
# @author   Alexander Willner <alex@gpgtools.org>
# @version  2011-05-12
# @see      https://github.com/GPGTools/MacGPG1/blob/master/build-script.sh
# @todo     Download and compile other sources first, e.g. Pth
# @todo     Create fat binaries for ppc, i386 and x86_64
##

# configuration ################################################################
url="ftp://ftp.gnupg.org/gcrypt/gnupg/";
version="gnupg-2.0.17";
fileExt=".tar.bz2";
sigExt=".tar.bz2.sig"
build="`pwd`/build/gnupg";
prefix_build="`pwd`/build/MacGPG2";
prefix_install="/usr/local/MacGPG2"
gpgFile="Makefile.gpg";

export MACOSX_DEPLOYMENT_TARGET="10.5"
export CFLAGS="-mmacosx-version-min=10.5 -DUNIX -isysroot /Developer/SDKs/MacOSX10.5.sdk -arch i386 -arch ppc"

gpg --import "$gpgFile";
mkdir -p "$build";
mkdir -p "$target";
################################################################################

pushd "$1" > /dev/null

# download sources #############################################################
cd "$build";
if [ ! -e "$version$fileExt" ]; then
    curl -O "$url$version$fileExt"
    curl -O "$url$version$sigExt"
fi
gpg --verify "$version$sigExt"
if [ "$?" != "0" ]; then
    echo "Could not get the sources!";
    exit 1;
fi
################################################################################

# compile sources ##############################################################
tar -xjf "$version$fileExt";
cd "$version";
./configure \
  --enable-static=yes \
  --disable-endian-check \
  --disable-dependency-tracking \
  --disable-asm \
  --enable-osx-universal-binaries \
  --prefix="$prefix_install" && \
make -j2
#&& make check

if [ "$?" != "0" ]; then
    echo "Could not compile the sources!";
    exit 1;
fi
################################################################################


# install binaries #############################################################
make prefix="$prefix_build" install
################################################################################

popd > /dev/null
