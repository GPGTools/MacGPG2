#!/bin/bash

echo
echo "****************************"
echo IDEA support not recommended
echo "****************************"
echo
echo Please ensure that you have an appropriate licence to use algorithm
echo
echo

export MacGPG2="/usr/local/MacGPG2"

export MACOSX_DEPLOYMENT_TARGET=10.5
export CFLAGS="-mmacosx-version-min=$MACOSX_DEPLOYMENT_TARGET -isysroot /Developer/SDKs/MacOSX$MACOSX_DEPLOYMENT_TARGET.sdk -arch i386 -arch x86_64"
export CPPFLAGS=-I$MacGPG2/include
export LDFLAGS=-L$MacGPG2/lib
export CC=/usr/bin/gcc-4.0
export CXX=/usr/bin/g++-4.0


if [ ! -e cipher/idea.c ]; then
  curl -O http://www.kfwebs.com/libgcrypt-1.2.4-idea.diff.bz2

  bunzip2 libgcrypt-1.2.4-idea.diff.bz2

  # Just use the parts we need:

  tail -n +14 libgcrypt-1.2.4-idea.diff | head -n 276 | patch -p1
fi

./configure --enable-static=no --disable-dependency-tracking --prefix=$MacGPG2 --with-gpg-error-prefix=$MacGPG2 --with-pth-prefix=$MacGPG2 --disable-asm

# Just in case
make clean

make

echo
echo "****************************"
echo IDEA support not recommended
echo "****************************"
echo
echo Please ensure that you have an appropriate licence to use algorithm
echo
echo

sudo make install

echo
echo "****************************"
echo IDEA support not recommended
echo "****************************"
echo
echo Please ensure that you have an appropriate licence to use algorithm
echo
echo

