#!/bin/bash

export SOURCEDIR="`pwd`"
export PATCHDIR="$SOURCEDIR/Patches"
export BUILDDIR="$SOURCEDIR/build"
export INSTALLDIR="$BUILDDIR/MacGPG2"

export NO_BUILDROOT_EXISTS=$(test -d $INSTALLDIR -a -w $INSTALLDIR; echo $?)

if [ "$NO_BUILDROOT_EXISTS" == "1" ]; then
    # Download homebrew and install it in BUILDDIR
    mkdir -p "$INSTALLDIR"
    curl -L https://github.com/mxcl/homebrew/tarball/master | tar xz --strip 1 -C "$INSTALLDIR"
fi

# Update brew to the latest version.
pushd $INSTALLDIR
    ./bin/brew update
    # Patch brew to add the install name patch and the build options
    # patch.
    for file in $PATCHDIR/homebrew/*; do
        patch --force -p1 < $file
    done

    # Copy the MacGPG2 Formulas to homebrew
    cp -R $SOURCEDIR/Formula/* ./Library/Formula/
    
    # Build MacGPG2
    ./bin/brew install --universal MacGPG2
popd
