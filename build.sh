#!/bin/bash

export SOURCEDIR="`pwd`"
export PATCHDIR="$SOURCEDIR/Patches"
export BUILDDIR="$SOURCEDIR/build"
export HOMEBREWDIR="$SOURCEDIR/Dependencies/homebrew"
export LOGPATH="$BUILDDIR"
export INSTALLDIR="$BUILDDIR/homebrew"
export DEPLOYDIR="$BUILDDIR/MacGPG2"
export NO_BUILDROOT_EXISTS=$(test -d "$INSTALLDIR" -a -w "$INSTALLDIR"; echo $?)
export HOMEBREW_CACHE=${HOMEBREW_CACHE:-"$INSTALLDIR/Caches"}


# Include status messages functions.
source status.sh

ACTION=$1
FORCE=0
if [ "$ACTION" == "force" ]; then
    FORCE=1
fi


function clean {
    if [ -d "$BUILDDIR" ]; then
        rm -rf "$BUILDDIR";
    fi
}

function bail_if_necessary {
    if [ "$1" == "0" ]; then
        return
    fi
    
    error "$2"
    exit 1
}

if [ "$ACTION" == "clean" ]; then
    title "Cleaning MacGPG2 build"
    clean
    title "Finished"
    exit 0
fi

title "Building MacGPG2"

if [ "$NO_BUILDROOT_EXISTS" == "1" ]; then
    status "Bootstrapping Homebrew"
    
    # Download homebrew and install it in BUILDDIR
    mkdir -p "$BUILDDIR"
    # Clone the homebrew from Dependencies into the build dir.
    git clone --recursive "$HOMEBREWDIR" "$INSTALLDIR" > /dev/null
    
    bail_if_necessary "$?" "Failed to bootstrap homebrew"
	    
    pushd "$INSTALLDIR" > /dev/null
        # Patch brew to add the install name patch and the build options
        # patch.
        status "Applying GPGTools homebrew patches"
        for file in "$PATCHDIR"/homebrew/*.patch; do
            patch --force -p1 < "$file"
            bail_if_necessary "$?" "Failed to apply homebrew patch $file"
        done
    popd > /dev/null
    
fi

# Update brew to the latest version.
pushd "$INSTALLDIR" > /dev/null
    
    # Copy the MacGPG2 Formulas to homebrew
    cp -R "$SOURCEDIR"/Formula/* ./Library/Formula/
    bail_if_necessary "$?" "Failed to copy MacGPG2 formulae"
    
    # Check if MacGPG2 is already built. If so, don't do it again,
    # unless force is set.
    EXIT="$?"
    MACGPG2_ALREADY_BUILT=$(./bin/brew list | grep macgpg2 >/dev/null; echo $?)
    
    if [ "$MACGPG2_ALREADY_BUILT" != "0" ] || [ "$FORCE" == "1" ]; then
        # Build MacGPG2 with make -j4
		export HOMEBREW_MAKE_JOBS=4
        ./bin/brew install --env=std --universal MacGPG2
        EXIT="$?"
    else
        success "MacGPG2 is already built. No need to do it again."
    fi

popd > /dev/null


if [ "$EXIT" != "0" ]; then
    error "Build failed!"
    exit 1
fi

/usr/bin/python packer.py --prune "$INSTALLDIR" "$DEPLOYDIR" || (error "Preparing files for the installer failed." && exit 1)


success "Build succeeded!"
exit 0
