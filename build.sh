#!/bin/bash

export SOURCEDIR="`pwd`"
export PATCHDIR="$SOURCEDIR/Patches"
export BUILDDIR="$SOURCEDIR/build"
export HOMEBREWDIR="$SOURCEDIR/Dependencies/homebrew"
export LOGPATH="$BUILDDIR"
export INSTALLDIR="$BUILDDIR/homebrew"
export DEPLOYDIR="$BUILDDIR/MacGPG2"
export BUILDENV_DMG=""
export BUILDENV_MOUNT_DIR="$SOURCEDIR/build-env"
export BUILD_PPC=0
export NO_BUILDROOT_EXISTS=$(test -d $INSTALLDIR -a -w $INSTALLDIR; echo $?)
export HOMEBREW_CACHE=${HOMEBREW_CACHE:-"$INSTALLDIR/Caches"}
export UPDATER_PLIST="org.gpgtools.macgpg2.updater.plist"

# Include the build-env file.
test -f ".build-env" && source ".build-env"
# Include the local build-env file which is not in git.
test -f ".build-env.local" && source ".build-env.local"

# Include status messages functions.
. status.sh

ACTION=$1
FORCE=0
if [ "$ACTION" == "force" ]; then
    FORCE=1
fi

function tryToMountBuildEnvironment {
    status "Try to mount the MacGPG2 build environment for ppc support"
    echo "Build env: ${BUILDENV_DMG}"
    if [ -f "$BUILDENV_DMG" ]; then
        STATUS=$(mountBuildEnvironment "$BUILDENV_DMG")
        echo "STATUS: ${STATUS}"
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
    echo hdiutil attach -mountpoint "$BUILDENV_MOUNT_DIR" -noverify "$1"
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
    mkdir -p "$BUILDDIR"
    # Clone the homebrew from Dependencies into the build dir.
    git clone --recursive $HOMEBREWDIR $INSTALLDIR > /dev/null
    
    bail_if_necessary "$?" "Failed to bootstrap homebrew"
    
    pushd "$INSTALLDIR" > /dev/null
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
    
    # Link 10.6 curl into hombrew dir.
    if [ ! -h ./curl-10.6 ]; then
        ln -s "${SOURCEDIR}/curl-10.6" ./curl-10.6
        bail_if_necessary "$?" "Failed to symlink 10.6 curl"
    fi
    
    BUILD_PPC_ARG=""
    if [ "$BUILD_PPC" == "1" ]; then
        # Set the Build Environment for Homebrew to use.
        export HOMEBREW_GPGTOOLS_BUILD_ENV="${INSTALLDIR}/build-env"
        BUILD_PPC_ARG="--with-ppc"
    fi
    
    # Check if MacGPG2 is already built. If so, don't do it again,
    # unless force is set.
    EXIT="$?"
    MACGPG2_ALREADY_BUILT=$(./bin/brew list | grep macgpg2 >/dev/null; echo $?)
    
    if [ "$MACGPG2_ALREADY_BUILT" != "0" ] || [ "$FORCE" == "1" ]; then
        # Build MacGPG2 with make -j4
	export HOMEBREW_MAKE_JOBS=4
        ./bin/brew install --env=std --universal $BUILD_PPC_ARG --use-llvm --quieter MacGPG2
        EXIT="$?"
    else
        success "MacGPG2 is already built. No need to do it again."
    fi

popd > /dev/null

if [ "$BUILD_PPC" == "1" ]; then
    tryToUnmountBuildEnvironment
fi

if [ "$EXIT" != "0" ]; then
    error "Build failed!"
    exit 1
fi

/usr/bin/python packer --prune "$INSTALLDIR" "$DEPLOYDIR" || (error "Preparing files for the installer failed." && exit 1)
# Move the MacGPG2_Updater plist file from $DEPLOYDIR/share/ into build,
# so the packager can find it.
cp -f "$SOURCEDIR/MacGPG2_Updater/$UPDATER_PLIST" "$BUILDDIR/" || (error "Failed to copy updater plist." && exit 1)


success "Build succeeded!"
exit 0
