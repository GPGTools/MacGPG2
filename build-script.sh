#!/bin/bash
# Build script for MacGPG2 - now part of GPGTools
# Copyright (c) Benjamin Donnachie 2010
# Released under version 3 of the GPL

function try {
	echo Trying "$@"
	echo
	"$@"
	if [ $? -ne 0 ]; then
	  echo
	  echo "*** ERROR - ABORTING ***"
	  echo
	  exit
	fi
}

# Set up build environment
WorkingDirectory="$PWD"
BuildDirectory="$PWD/build"
MacGPG2="/usr/local/MacGPG2"

MACOSX_DEPLOYMENT_TARGET=10.5
CFLAGS="-mmacosx-version-min=$MACOSX_DEPLOYMENT_TARGET -isysroot /Developer/SDKs/MacOSX$MACOSX_DEPLOYMENT_TARGET.sdk -arch i386 -arch x86_64"
CPPFLAGS=-I$MacGPG2/include
LDFLAGS=-L$MacGPG2/lib
CC=/usr/bin/gcc-4.0
CXX=/usr/bin/g++-4.0

# Create a build directory a level above
#  Really need build directory now self contained in $MacGPG2?
mkdir $BuildDirectory

echo Either run script as root with sudo or be available to enter 
echo  password for make install

# Circular dependency between iconv and gettext

cd $WorkingDirectory/source/libiconv
try ./configure --enable-static=no --disable-dependency-tracking --prefix=$MacGPG2 --enable-extra-encodings
try make
try sudo make install

cd $WorkingDirectory/source/gettext
try ./configure --enable-static=no --disable-dependency-tracking --prefix=$MacGPG2
try make
try sudo make install
try sudo make -e prefix=BuildDirectory install

cd $WorkingDirectory/source/libiconv
try make distclean
try ./configure --enable-static=no --disable-dependency-tracking --prefix=$MacGPG2 --enable-extra-encodings
try make
try sudo make install
try sudo make -e prefix=BuildDirectory install

# Next pth
cd $WorkingDirectory/source/pth
try ./configure --enable-static=no --disable-dependency-tracking --prefix=$MacGPG2 --with-mctx-mth=sjlj --with-mctx-dsp=ssjlj --with-mctx-stk=sas
# Patch make for multiple architecture builds
try patch -p0 < Makefile.patch
try make
try make test
try sudo make install
try sudo make -e prefix=BuildDirectory install

cd $WorkingDirectory/source/libusb
try ./configure --enable-static=no --disable-dependency-tracking --prefix=$MacGPG2
try make
try sudo make install
try sudo make -e prefix=BuildDirectory install

LIBUSB_1_0_CFLAGS=`$MacGPG2/bin/libusb-config --cflags`
LIBUSB_1_0_LIBS=`$MacGPG2/bin/libusb-config --libs`

cd $WorkingDirectory/source/libusb-compat
try ./configure --enable-static=no --disable-dependency-tracking --prefix=$MacGPG2
try make
try sudo make install
try sudo make -e prefix=BuildDirectory install

cd $WorkingDirectory/source/libgpg-error
try ./configure --enable-static=no --disable-dependency-tracking --prefix=$MacGPG2
try make
try sudo make install
try sudo make -e prefix=BuildDirectory install

cd $WorkingDirectory/source/libassuan
try ./configure --enable-static=no --disable-dependency-tracking --prefix=$MacGPG2 --with-gpg-error-prefix=$MacGPG2/
try make
try sudo make install
try sudo make -e prefix=BuildDirectory install

cd $WorkingDirectory/source/libassuan
try ./configure --enable-static=no --disable-dependency-tracking --prefix=$MacGPG2 --with-gpg-error-prefix=$MacGPG2/
try make
try sudo make install
try sudo make -e prefix=BuildDirectory install

cd $WorkingDirectory/source/libgcrypt
try ./configure --enable-static=no --disable-dependency-tracking --prefix=$MacGPG2 --with-gpg-error-prefix=$MacGPG2 --with-pth-prefix=$MacGPG2 --disable-asm
try make
try sudo make install
try sudo make -e prefix=BuildDirectory install

cd $WorkingDirectory/source/libksba
try ./configure --enable-static=no --disable-dependency-tracking --prefix=$MacGPG2 --with-gpg-error-prefix=$MacGPG2/
try make
try sudo make install
try sudo make -e prefix=BuildDirectory install

# make clean currently broke in GnuPG2
cd $WorkingDirectory/source/gnupg2
try ./configure --disable-dependency-tracking --with-pinentry-pgm=$MacGPG2/libexec/pinentry-mac.app/Contents/MacOS/pinentry-mac --prefix=$MacGPG2 --enable-standard-socket --with-gpg-error-prefix=$MacGPG2/ --with-libgcrypt-prefix=$MacGPG2/ --with-libassuan-prefix=$MacGPG2/ --with-ksba-prefix=$MacGPG2/ --with-pth-prefix=$MacGPG2/ --disable-gpgtar --with-libiconv-prefix=$MacGPG2 --with-libintl-prefix=$MacGPG2/
try make
try sudo make install
try sudo make -e prefix=BuildDirectory install

