#!/bin/bash

export SOURCEDIR="`pwd`"
export PATCHDIR="$SOURCEDIR/Patches"
export BUILDDIR="$SOURCEDIR/build"
export LOGPATH="$BUILDDIR"
export INSTALLDIR="$BUILDDIR/homebrew"
export DEPLOYDIR="$BUILDDIR/MacGPG2"
export BUILDENV_DMG="/Users/lukele/Desktop/MacGPG2PPCBuildEnv.dmg"
export BUILDENV_MOUNT_DIR="$SOURCEDIR/build-env"
export BUILD_PPC=0
export NO_BUILDROOT_EXISTS=$(test -d $INSTALLDIR -a -w $INSTALLDIR; echo $?)

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
    
    pushd "$INSTALLDIR" > /dev/null
        ./bin/brew update 1>/dev/null 2>/dev/null
        # Patch brew to add the install name patch and the build options
        # patch.
        status "Applying GPGTools homebrew patches"
        for file in "$PATCHDIR"/homebrew/*.patch; do
            patch --force -p1 < $file > /dev/null
        done
    popd > /dev/null
    
fi

# Update brew to the latest version.
pushd "$INSTALLDIR" > /dev/null
    
    # Copy the MacGPG2 Formulas to homebrew
    cp -R "$SOURCEDIR"/Formula/* ./Library/Formula/
    
    # Link the jail dir which contains all compilers.
    if [ ! -h ./build-env ]; then
        ln -s "${BUILDENV_MOUNT_DIR}" ./build-env
    fi
    
    BUILD_PPC_ARG=""
    if [ "$BUILD_PPC" == "1" ]; then
        # Set the Build Environment for Homebrew to use.
        export HOMEBREW_GPGTOOLS_BUILD_ENV="${INSTALLDIR}/build-env"
        BUILD_PPC_ARG="--with-ppc"
    fi
    
    # Build MacGPG2
    ./bin/brew install --universal $BUILD_PPC_ARG --use-llvm --quieter MacGPG2
    EXIT="$?"

popd > /dev/null

if [ "$BUILD_PPC" == "1" ]; then
    tryToUnmountBuildEnvironment
fi

if [ "$EXIT" == "0" ]; then
    if [ -d "$DEPLOYDIR" ] && [ "$DEPLOYDIR" != "/" ]; then
        rm -rf "$DEPLOYDIR"
    fi
    # After building successfully
    mkdir "$DEPLOYDIR"
    DIRS="usr bin sbin include lib libexec man share"
    for dir in $DIRS; do
        if [ -d "$INSTALLDIR/$dir" ]; then
            cp -RL "$INSTALLDIR/$dir" "$DEPLOYDIR/"
        fi
    done
    
    success "Build succeeded!"
    exit 0
else
    error "Build failed!"
    exit 1
fi
