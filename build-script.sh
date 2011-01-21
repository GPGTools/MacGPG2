#!/bin/bash
# Build script for MacGPG2 - now part of GPGTools
# Copyright (c) Benjamin Donnachie 2010
# Released under version 3 of the GPL

# Set up build environment
MACOSX_DEPLOYMENT_TARGET=10.5
CFLAGS="-mmacosx-version-min=$MACOSX_DEPLOYMENT_TARGET -isysroot /Developer/SDKs/MacOSX$MACOSX_DEPLOYMENT_TARGET.sdk -arch i386 -arch x86_64"
CPPFLAGS=-I/usr/local/MacGPG2/include
LDFLAGS=-L/usr/local/MacGPG2/lib
CC=/usr/bin/gcc-4.0
CXX=/usr/bin/g++-4.0

WorkingDirectory="$PWD"
BuildDirectory="$PWD/build"

# Create a build directory a level above

mkdir $BuildDirectory

# Circular dependency between iconv and gettext

cd $WorkingDirectory/source/libiconv
./configure --enable-static=no --disable-dependency-tracking --prefix=/usr/local/MacGPG2 --enable-extra-encodings
make
make install

cd $WorkingDirectory/source/gettext
./configure --enable-static=no --disable-dependency-tracking --prefix=/usr/local/MacGPG2
make
make install
make -e prefix=BuildDirectory install

cd $WorkingDirectory/source/libiconv
./configure --enable-static=no --disable-dependency-tracking --prefix=/usr/local/MacGPG2 --enable-extra-encodings
make
make install
make -e prefix=BuildDirectory install

# Next pth
cd $WorkingDirectory/source/pth
./configure --enable-static=no --disable-dependency-tracking --prefix=/usr/local/MacGPG2 --with-mctx-mth=sjlj --with-mctx-dsp=ssjlj --with-mctx-stk=sas
# Patch make for multiple architecture builds
patch -p0 < Makefile.patch
make
make install
make -e prefix=BuildDirectory install

cd $WorkingDirectory/source/libusb
./configure --enable-static=no --disable-dependency-tracking --prefix=/usr/local/MacGPG2
make
make install
make -e prefix=BuildDirectory install

LIBUSB_1_0_CFLAGS=`/usr/local/MacGPG2/bin/libusb-config --cflags`
LIBUSB_1_0_LIBS=`/usr/local/MacGPG2/bin/libusb-config --libs`

cd $WorkingDirectory/source/libusb-compat
./configure --enable-static=no --disable-dependency-tracking --prefix=/usr/local/MacGPG2
make
make install
make -e prefix=BuildDirectory install

cd $WorkingDirectory/source/libgpg-error
./configure --enable-static=no --disable-dependency-tracking --prefix=/usr/local/MacGPG2
make
make install
make -e prefix=BuildDirectory install

cd $WorkingDirectory/source/libassuan
./configure --enable-static=no --disable-dependency-tracking --prefix=/usr/local/MacGPG2 --with-gpg-error-prefix=/usr/local/MacGPG2/
make
make install
make -e prefix=BuildDirectory install

cd $WorkingDirectory/source/libassuan
./configure --enable-static=no --disable-dependency-tracking --prefix=/usr/local/MacGPG2 --with-gpg-error-prefix=/usr/local/MacGPG2/
make
make install
make -e prefix=BuildDirectory install

cd $WorkingDirectory/source/libgcrypt
./configure --enable-static=no --disable-dependency-tracking --prefix=/usr/local/MacGPG2 --with-gpg-error-prefix=/usr/local/MacGPG2 --with-pth-prefix=/usr/local/MacGPG2 --disable-asm
make
make install
make -e prefix=BuildDirectory install

cd $WorkingDirectory/source/libksba
./configure --enable-static=no --disable-dependency-tracking --prefix=/usr/local/MacGPG2 --with-gpg-error-prefix=/usr/local/MacGPG2/
make
make install
make -e prefix=BuildDirectory install

cd $WorkingDirectory/source/gnupg2
./configure --disable-dependency-tracking --with-pinentry-pgm=/usr/local/MacGPG2/libexec/pinentry-mac.app/Contents/MacOS/pinentry-mac --prefix=/usr/local/MacGPG2 --enable-standard-socket --with-gpg-error-prefix=/usr/local/MacGPG2/ --with-libgcrypt-prefix=/usr/local/MacGPG2/ --with-libassuan-prefix=/usr/local/MacGPG2/ --with-ksba-prefix=/usr/local/MacGPG2/ --with-pth-prefix=/usr/local/MacGPG2/ --disable-gpgtar --with-libiconv-prefix=/usr/local/MacGPG2 --with-libintl-prefix=/usr/local/MacGPG2/
make
make install
make -e prefix=BuildDirectory install







