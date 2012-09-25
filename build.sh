#!/bin/bash

export SOURCEDIR="`pwd`"
export PATCHDIR="$SOURCEDIR/Patches"
export BUILDDIR="$SOURCEDIR/build"
export LOGPATH="$BUILDDIR"
export INSTALLDIR="$BUILDDIR/homebrew"
export DEPLOYDIR="$BUILDDIR/MacGPG2"
export BUILDENV_DMG=""
export BUILDENV_MOUNT_DIR="$SOURCEDIR/build-env"
export BUILD_PPC=0
export NO_BUILDROOT_EXISTS=$(test -d $INSTALLDIR -a -w $INSTALLDIR; echo $?)

# Include the build-env file.
test -f ".build-env" && source ".build-env"
# Include the local build-env file which is not in git.
test -f ".build-env.local" && source ".build-env.local"

# Include status messages functions.
. status.sh

ACTION=$1

function tryToMountBuildEnvironment {
    status "Try to mount the MacGPG2 build environment for ppc support"
    if [ -f "$BUILDENV_DMG" ]; then
        STATUS=$(mountBuildEnvironment "$BUILDENV_DMG")
        if [ "$STATUS" == "0" ]; then
            BUILD_PPC=1
            stat=$(printf "%s%s%s" $(white) "enabled" $(reset))
            status "PPC support $stat."
        else
            BUILD_PPC=0
            stat=$(printf "%s%s%s" $(white) "disabled" $(reset))
            status "PPC support $stat."
        fi
    else
        BUILD_PPC=0
        stat=$(printf "%s%s%s" $(white) "disabled" $(reset))
        status "PPC support $stat."
    fi
}

function mountBuildEnvironment {
    hdiutil attach -mountpoint "$BUILDENV_MOUNT_DIR" -noverify -quiet "$1"
    echo $?
}

function tryToUnmountBuildEnvironment {
    status "Unomunt the MacGPG2 build environment"
    hdiutil detach -force -quiet "$BUILDENV_MOUNT_DIR"
}

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
tryToMountBuildEnvironment

if [ "$NO_BUILDROOT_EXISTS" == "1" ]; then
    status "Bootstrapping Homebrew"
    
    # Download homebrew and install it in BUILDDIR
    mkdir -p "$INSTALLDIR"
    curl -s -L https://github.com/mxcl/homebrew/tarball/master 2> /dev/null | tar xz --strip 1 -C "$INSTALLDIR"
    
    bail_if_necessary "$?" "Failed to bootstrap homebrew"
    
    pushd "$INSTALLDIR" > /dev/null
        ./bin/brew update 1>/dev/null 2>/dev/null
        # Patch brew to add the install name patch and the build options
        # patch.
        status "Applying GPGTools homebrew patches"
        for file in "$PATCHDIR"/homebrew/*.patch; do
            patch --force -p1 < $file > /dev/null
            bail_if_necessary "$?" "Failed to apply homebrew patch $file"
        done
    popd > /dev/null
    
fi

# Update brew to the latest version.
pushd "$INSTALLDIR" > /dev/null
    
    # Copy the MacGPG2 Formulas to homebrew
    cp -R "$SOURCEDIR"/Formula/* ./Library/Formula/
    bail_if_necessary "$?" "Failed to copy MacGPG2 formulae"
    
    # Link the jail dir which contains all compilers.
    if [ ! -h ./build-env ]; then
        ln -s "${BUILDENV_MOUNT_DIR}" ./build-env
        bail_if_necessary "$?" "Failed to symlink build-env"
    fi
    
    BUILD_PPC_ARG=""
    if [ "$BUILD_PPC" == "1" ]; then
        # Set the Build Environment for Homebrew to use.
        export HOMEBREW_GPGTOOLS_BUILD_ENV="${INSTALLDIR}/build-env"
        BUILD_PPC_ARG="--with-ppc"
    fi
    
    # Build MacGPG2
    echo "./bin/brew install --env=std --universal $BUILD_PPC_ARG --use-llvm --quieter MacGPG2"
    ./bin/brew install --env=std --universal $BUILD_PPC_ARG --use-llvm --quieter MacGPG2
    EXIT="$?"

popd > /dev/null

if [ "$BUILD_PPC" == "1" ]; then
    tryToUnmountBuildEnvironment
fi

if [ "$EXIT" != "0" ]; then
    error "Build failed!"
    exit 1
fi

/usr/bin/python packer --prune "$INSTALLDIR" "$DEPLOYDIR"

if [ "$?" != "0" ]; then
    error "Preparing files for the installer failed."
    exit 1
fi

success "Build succeeded!"
exit 0
