#!/bin/bash

# This file is licensed under the GNU Lesser General Public License v3.0
# See https://www.gnu.org/licenses/lgpl-3.0.txt for a copy of the license
#
# Usage: ./create_gpg

BASE_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")"; pwd -P)
CACHE_DIR=${CACHE_DIR:-$BASE_DIR/../Caches}
WORKING_DIR=$BASE_DIR/build
BUILD_DIR=$WORKING_DIR/build
DIST_DIR=$WORKING_DIR/dist
UNIVERSAL_DIST_DIR=${DIST_DIR}/universal
FINAL_DIR=$WORKING_DIR/MacGPG2
TARGET_DIR=/usr/local/MacGPG2
TOOL="$1"

# Files in bin and libexec, which are copied to the MacGPG2 dir.
BIN_FILES=(gpg gpg-agent gpg-connect-agent gpg-error gpgconf gpgparsemail gpgsm gpgsplit gpgtar gpgv kbxutil watchgnupg dirmngr-client dirmngr mpicalc dumpsexp hmac256)
LIBEXEC_FILES=(dirmngr_ldap gpg-preset-passphrase scdaemon gpg-check-pattern gpg-protect-tool gpg-wks-client)

export MACOSX_DEPLOYMENT_TARGET=10.12
MACOS_MIN_VERSION="-mmacosx-version-min=10.12"

# Prepare logging.
mkdir -p "$WORKING_DIR" "$BUILD_DIR" "$DIST_DIR" "$CACHE_DIR"
if [[ -t 2 ]]; then
	HAVE_TERMINAL=1
	exec &> >(tee >(tr -u "\033" "#" | sed -E 's/#\[[0-9][^m]*m//g' >> "$WORKING_DIR/create_gpg.log"))
else
	HAVE_TERMINAL=0
	exec &> >(tee -a "$WORKING_DIR/create_gpg.log")
fi

set -o pipefail

# Turn on glob extensions. They are used in the verify function.
# But *must* be turned on, before the function is parsed.
shopt -s extglob

if [[ "$(type -t pkg-config)" != "file" ]]; then
	err_exit "ERROR: pkg-config is not installed. This script requires pkg-config to be available.\nPlease fix your setup and try again."
fi

# Helper functions.
function err_exit {
	msg="$* (${BASH_SOURCE[1]##*/}: line ${BASH_LINENO[0]})"
	if [[ "$HAVE_TERMINAL" == "1" ]] ;then
		echo -e "\033[1;31m$msg\033[0m" >&2
	else
		echo -e "$msg" >&2
	fi
	exit 1
}
function do_fail {
	msg="\n** ERROR at $* ** - build failed (${BASH_SOURCE[1]##*/}: line ${BASH_LINENO[0]})"
	if [[ "$HAVE_TERMINAL" == "1" ]] ;then
		echo -e "\033[1;31m$msg\033[0m" >&2
	else
		echo -e "$msg" >&2
	fi
	exit 1
}
function pycmd {
	python -c "c=$config
$1"
}

# Functions for the real work.
function download {
	if [[ -f "$archive_path" ]]; then
		echo "  - Skipping download $archive_name"
	else
		echo "  - Downloading $archive_name"
		curl -L -o "$archive_path" "$full_url"
	fi

	file_hash=$(shasum -a 256 "$archive_path" | cut -d ' ' -f 1)
	if [[ "${lib_sha256:?}" != "$file_hash" ]]; then
		echo "SHA-256 sums of file $archive_name don't match" >&2
		echo "expected: $lib_sha256" >&2
		echo "obtained: $file_hash" >&2
		do_fail "download: failed to download $archive_name from $full_url"
	fi
}
function unpack {
	local dirs=("$BUILD_DIR/$lib_name-"*)
	if [[ -d "${dirs[0]}" ]]; then
		echo "  - Removing dir ${dirs[@]##*/}"
		rm -rf "${dirs[@]}"
	fi

	echo "  - unpacking $archive_name"
	suffix=${archive_name##*.tar.}

	untar="tar -x --exclude gettext-tools"

	case "$suffix" in
	lz)
		lunzip=lunzip
		if ! command -v lunzip &>/dev/null; then
			lunzip="lzip -d"
		fi
		$lunzip -k -c "$archive_path" | $untar -C "$BUILD_DIR" -f - || do_fail "unpack: failed to unpack $archive_name"
		;;
	bz2)
		$untar -C "$BUILD_DIR" -jf "$archive_path" || do_fail "unpack: failed to unpack $archive_name"
		;;
	gz|xz)
		$untar -C "$BUILD_DIR" -zf "$archive_path" || do_fail "unpack: failed to unpack $archive_name"
		;;
	*)
		do_fail "unpack: failed to unpack $archive_name"
	esac
}
function apply_patches {
	patches=("$BASE_DIR/patches/$lib_name/"*.patch)

	if [[ -f "${patches[0]}" ]]; then
		echo "  - Applying patches"
		for patch in "${patches[@]}"; do
			echo "    - Applying patch $patch"
			patch -d "$dir_path" -p1 -t -N < "$patch" || do_fail "patch: failed to apply patch $patch"
		done
	fi
}

function define_build_vars {
	dest_arch="${1:-x86_64}"
	build_cflags="${MACOS_MIN_VERSION} -ULOCALEDIR -DLOCALEDIR='\"${TARGET_DIR}/share/locale\"'"
	# For some reason, many configure based libraries test for aarch64 instead of arm64,
	# so replace arm64 with aarch64.
	build_alias="${dest_arch/arm64/aarch64}-apple-darwin"
	host_alias="${build_alias}"

	build_arch_flags=""
	if [[ "$dest_arch" == "arm64" ]]; then
		build_arch_flags="${build_arch_flags} -m64 -target ${build_alias} -arch ${dest_arch}"
	else
		build_arch_flags="${build_arch_flags} -march=core2 -arch ${dest_arch} -target ${build_alias}"
	fi

	build_cflags="${build_cflags} -I${arch_dist_dir}/include ${build_arch_flags}"
	build_ldflags="-L${arch_dist_dir}/lib ${build_arch_flags}"
	build_cxxflags="${build_cflags}"
	build_cppflags="${build_cflags}"
}

function customize_build_for_libksba {
	configure_args="$configure_args --with-libgpg-error-prefix=${arch_dist_dir}"
}

function customize_build_for_sqlite {
	cache_file="$WORKING_DIR/config.${dest_arch}.sqlite.cache"
}

function customize_build_for_libgcrypt {
	if [[ "$1" = "arm64" ]]; then
		configure_args="$configure_args --disable-asm"
	fi
}

function post_configure_for_libtasn1 {
	if [[ "$1" != "arm64" ]]; then
		return
	fi

	echo "Disabling 'tests' and 'examples' make steps for $1"
	# `make tests` and `make examples` will fail on arm64 due to linking x86_64 and arm64
	# so replace the Makefile with a dummy one.
	echo -e "all:\n\techo test\n\ninstall:\n\techo install" > ./tests/Makefile
	echo -e "all:\n\techo test\n\ninstall:\n\techo install" > ./examples/Makefile
}

function customize_build_for_gnupg {
	build_cflags="${build_cflags} -I${arch_dist_dir}/include -I${arch_dist_dir}/include/libusb-1.0/"
	build_cflags="${build_cflags} -UGNUPG_BINDIR -DGNUPG_BINDIR=\"\\\"${TARGET_DIR}/bin\\\"\" \
				      -UGNUPG_LIBEXECDIR -DGNUPG_LIBEXECDIR=\"\\\"${TARGET_DIR}/libexec\\\"\" \
				      -UGNUPG_LIBDIR -DGNUPG_LIBDIR=\"\\\"${TARGET_DIR}/lib/gnupg\\\"\" \
				      -UGNUPG_DATADIR -DGNUPG_DATADIR=\"\\\"${TARGET_DIR}/share/gnupg\\\"\" \
				      -UGNUPG_SYSCONFDIR -DGNUPG_SYSCONFDIR=\"\\\"${TARGET_DIR}/etc/gnupg\\\"\""
	build_cxxflags="${build_cflags}"
	build_cppflags="${build_cflags}"
	build_ldflags="${build_ldflags} -framework Foundation -framework Security"
	cache_file="${WORKING_DIR}/config.${dest_arch}.gnupg.cache"

	configure_args="$configure_args \
		--localstatedir=/var \
		--sysconfdir=${TARGET_DIR}/etc \
		--disable-rpath \
		--with-pinentry-pgm=${TARGET_DIR}/libexec/pinentry-mac.app/Contents/MacOS/pinentry-mac \
		--with-agent-pgm=${TARGET_DIR}/bin/gpg-agent \
		--with-scdaemon-pgm=${TARGET_DIR}/libexec/scdaemon \
		--with-dirmngr-pgm=${TARGET_DIR}/bin/dirmngr \
		--with-dirmngr-ldap-pgm=${TARGET_DIR}/libexec/dirmngr_ldap \
		--with-protect-tool-pgm=${TARGET_DIR}/libexec/gpg-protect-tool \
		--with-libassuan-prefix=${arch_dist_dir} \
		--with-ksba-prefix=${arch_dist_dir} \
		--with-npth-prefix=${arch_dist_dir} \
		--with-readline=${arch_dist_dir} \
		--with-libintl-prefix=${arch_dist_dir} \
		--with-libiconv-prefix=${arch_dist_dir}"
}

function pre_build {
	if [[ "$lib_name" = "gettext" ]]; then
		pushd gettext-runtime || exit
	fi

	make distclean &>/dev/null

	if [[ ! -f "configure" ]]; then
		./autogen.sh
	fi
}

function post_build {
	if [[ "$lib_name" = "gettext" ]]; then
		popd || exit
	fi
}

function post_configure {
	if [[ "$lib_name" = "gnupg" ]]; then
		# Change the name from "GnuPG" to "GnuPG/MacGPG2".
		echo -e "#undef GNUPG_NAME\n#define GNUPG_NAME \"GnuPG/MacGPG2\"" >> config.h
	fi
}

function customize_build {
	if [[ "${lib_name}" != "sqlite" ]]; then
		build_cflags="${build_cflags} -Ofast"
	else
		echo SQLite no Ofast
	fi

	if [[ "${lib_name}" != "gnupg" ]]; then
		configure_args="$configure_args --enable-static=no"
	fi
}

function build {
	pushd "$dir_path" >/dev/null || exit

	local dest_arch="${1:-x86_64}"
	arch_dist_dir="${DIST_DIR}/${dest_arch}"

	echo "  - building $lib_name for ${dest_arch}"

	define_build_vars "${dest_arch}"

	pre_build

	configure_args="$lib_configure_args"

	cache_file="${WORKING_DIR}/config.${dest_arch}.cache"

	# Call the general customize function.
	customize_build

	# Check if any tool based functions are configured.
	customize_func="customize_build_for_${lib_name//-/_}"
	if [[ "$(type -t "${customize_func}")" == "function" ]]; then
		${customize_func} "${dest_arch}"
	fi

	# GMP by default produces assembly which is only compatible
	# with the CPU the lib is built on or newer.
	# --disable-assembly might have to be added as well.
	# By defining host_alias, gmp will build for a generic 64bit CPU.
	configure_args="$configure_args --host=${host_alias} --target=${build_alias}"
	# 1. SYSROOT so libraries such as libgpg-error are automatically detected.
	# 2. Define the build flags to pass to configure.
	# 3. ABI is always 64-bit
	# 4. Define ac_cv_* to work around a bug on macOS where this check failed (caused a runtime segfault.)
	SYSROOT="${arch_dist_dir}" \
	CFLAGS="$build_cflags" CXXFLAGS="$build_cxxflags" \
	LDFLAGS="$build_ldflags" CPPFLAGS="$build_cppflags" \
	PKG_CONFIG_PATH="${arch_dist_dir}/lib/pkgconfig" \
	ABI=64 \
	ac_cv_search_clock_gettime=no ac_cv_func_clock_gettime=no \
	./configure \
		--prefix="${arch_dist_dir:?}" \
		--cache-file="${cache_file:?}" \
		$configure_args || \
	do_fail "build: ${lib_name} (${dest_arch}) configure"

	post_configure

	post_configure_func="post_configure_for_${lib_name//-/_}"
	if [[ "$(type -t "${post_configure_func}")" == "function" ]]; then
		${post_configure_func} "${dest_arch}"
	fi

	make localedir="${TARGET_DIR}/share/locale" datarootdir="${TARGET_DIR}/share" || do_fail "build: ${lib_name} (${dest_arch}) make"
	make install || do_fail "build: ${lib_name} (${dest_arch}) install"

	post_build

	popd >/dev/null || exit
}

function prepare_for_packaging {
	# some files get wrong permissions, let's just fix them ...
	echo "- Fix permissions"
	find "$DIST_DIR" -type f -exec chmod +w {} +

	echo "- Copy share folder to universal build"
	mkdir -p "${UNIVERSAL_DIST_DIR:?}/share"
	cp -R "${DIST_DIR}/arm64/share/"* "${UNIVERSAL_DIST_DIR:?}/share/"

	mkdir -p "${UNIVERSAL_DIST_DIR:?}/include"
	cp -R "${DIST_DIR}/arm64/include/"* "${UNIVERSAL_DIST_DIR:?}/include/"

	# Walk through the files and copy non-binary files as is and
	# perform necessary fixups (adjusting library ids, rpaths)
	# and combining variants into one universal binary.
	echo "- Fix up executables and create universal binaries."
	pushd "$DIST_DIR" || exit
		for dir in "lib" "bin" "sbin" "libexec"; do
			# Create the directory if it does not exist yet.
			if [[ ! -d "${UNIVERSAL_DIST_DIR}/${dir}" ]]; then
				mkdir -p "${UNIVERSAL_DIST_DIR}/${dir}"
			fi

			for item in "x86_64/${dir}/"*; do
				filename=${item##*/}
				executable=$(file "$item" | grep "64-bit")
				item_arm=${item/x86_64/arm64}
				item_without_arch=${item/x86_64\//}

				# Not a executable, simply copy the file.
				if [[ -z "$executable" || -L "$item" ]]; then
					echo "  - Copy non-executable or symlink $item"
					cp -R "$item" "${UNIVERSAL_DIST_DIR}/${dir}/"
					continue;
				fi

				echo "  - Fix dylib paths $item (x86_64)"
				fixup_lib_paths "$item"
				echo "  - Fix dylib paths $item (arm64)"
				fixup_lib_paths "$item_arm"

				echo "  - Combine variants into universal binary $item_without_arch"
				create_universal_binary "$item_without_arch"
				codesign_binary "${UNIVERSAL_DIST_DIR}/${item_without_arch}"
			done
		done
	popd || exit
}

function fixup_lib_paths {
	local path="$1"
	local filename="$(basename "$path")"

	# If this is an dylib adjust the ID.
	if [[ "${filename##*.}" = "dylib" ]]; then
		echo "    - Adjusting ID for ${path}"
		install_name_tool -id "${TARGET_DIR}/lib/${filename}" "${path}"
	fi

	# Check if rpaths need to be adjusted.
	libs=$(otool -L "$path" | cut -d' ' -f1 | tr -d "\t" | tail -n +2)
	need_rpath=0
	for lib in $libs; do
		if [[ "$lib" =~ $DIST_DIR ]]; then
			need_rpath=1
			rpath="@rpath/${lib##*/}"
			echo "    - Change $lib to relative path $rpath"
			install_name_tool -change "$lib" "$rpath" "$path"
		fi
	done

	# Add the rpath if necessary and a loader rpath doesn't already exist.
	loader_path_rpath="$(otool -l "$path" | grep -A2 "cmd LC_RPATH" | grep -E "path\s@loader_path")"
	if [[ $need_rpath -eq 1 && -z "$loader_path_rpath" ]]; then
		echo "    - Adding @loader_path as rpath to $path"
		install_name_tool -add_rpath "@loader_path/../lib" "$path"
	fi
}

function create_universal_binary {
	local path="$1"

	echo "    - Combine x86_64/${path} with arm64/${path} -> ${path}"
	lipo -create -output "${UNIVERSAL_DIST_DIR}/${path}" "x86_64/${path}" "arm64/${path}"
}

function codesign_binary {
	local path="$1"

	if [[ -z "$CERT_NAME_APPLICATION" ]]; then
		echo "  - *DO NOT* code sign universal binary $item_without_arch"

		return
	else
		echo "  - Code sign universal binary $item_without_arch"
	fi

	KEYCHAIN=$(security find-certificate -c "$CERT_NAME_APPLICATION" | sed -En 's/^keychain: "(.*)"/\1/p')
	[[ -n "$KEYCHAIN" ]] || err_exit "I require certificate '$CERT_NAME_APPLICATION' but it can't be found.  Aborting."

	if [[ -n "$UNLOCK_PWD" ]]; then
		security unlock-keychain -p "$UNLOCK_PWD" "$KEYCHAIN"
	fi

	codesign --force --timestamp -o runtime --sign "$CERT_NAME_APPLICATION" "$path" || do_fail "codesign_binary - failed to sign binary $path"
}

function copy_to_final_destination {
	echo "* Copying files to final destination ${FINAL_DIR##*/}"
	rm -rf "$FINAL_DIR" &>/dev/null
	mkdir -p "$FINAL_DIR"

	pushd "${UNIVERSAL_DIST_DIR}" >/dev/null || exit
		tar -cf - \
			"${BIN_FILES[@]/#/bin/}" \
			"${LIBEXEC_FILES[@]/#/libexec/}" \
			lib/*dylib \
			include \
			share/gnupg \
			share/man \
			share/locale | tar -C "$FINAL_DIR" -xf -

		ln -s gpg "${FINAL_DIR}/bin/gpg2"
		cp -r "${BASE_DIR}/payload/" "${FINAL_DIR}/" || do_fail "copy_to_final_destination: failed to copy payload files."
		cp sbin/gpg-zip "$FINAL_DIR/bin/" || do_fail "copy_to_final_destination: failed to copy gpg-zip"

		# Remove any libgettext files.
		rm -f "$FINAL_DIR/lib/libgettext"*
	popd >/dev/null || exit
}

function verify {
	echo "* Verify binary correctnes"
	# ensure that all executables and libraries have correct lib-path settings and are signed correctly
	pushd "$FINAL_DIR" >/dev/null || exit
		otool -L bin/!(convert-keyring|gpg-zip) | grep "$WORKING_DIR" && do_fail "verify: some binaries in bin/ still contain wrong paths"
		otool -L libexec/!(fixGpgHome|shutdown-gpg-agent) | grep "$WORKING_DIR" && do_fail "verify: some binaries in libexec/ still contain wrong paths"
		otool -L lib/* | grep "$WORKING_DIR" && do_fail "verify: some binaries in lib/ still contain wrong paths"

		if [[ -n "$CERT_NAME_APPLICATION" ]]; then
			# file prints 3 lines for each binary. the first with the filename and next one line for each architecture.
			# By using grep -v '(' only the first line is kept.
			find . -print0 | xargs -0 file | grep -F Mach-O | grep -v '(' | cut -d: -f1 | while read -r file; do
				codesign --verify "$file" || do_fail "verify: invalid signature for $file"
			done
			echo "  - All binaries are signed correctly."
		else
			echo "  - No code signing certificate specified."
		fi
	popd >/dev/null || exit
}

echo -n "Build started at "; date

# Read libs.json
config=$(python -c "import sys,json; print(json.loads(sys.stdin.read()))" < "$BASE_DIR/libs.json" 2>/dev/null) || do_fail "read libs.json"
count=$(pycmd "print(len(c))") || do_fail "process libs.json"

if [[ -n "${TOOL}" ]]; then
	echo "== Building ${TOOL} =="
fi

# Loop over all libs
for (( i=0; i<count; i++ )); do
	# Set the variables lib_name, lib_url, etc.
	unset $(compgen -A variable | grep "^lib_")
	eval $(pycmd "for k, v in c[$i].items(): print('lib_%s=\'%s\'' % (k, v))")

	[[ -n "$lib_url" ]] || err_exit "Variable lib_url not set!"

	full_url=${lib_url/\$\{VERSION\}/$lib_version}
	archive_name=${full_url##*/}
	archive_path=$CACHE_DIR/$archive_name
	dir_name=${archive_name%%.tar*}
	dir_path=$BUILD_DIR/$dir_name

	echo "* Build $lib_name"

	if [[ -n "$TOOL" && "$TOOL" != "$lib_name" ]]; then
		echo "Skip $lib_name"
		continue
	fi

	if [[ -n "$lib_installed_file" && -d "$dir_path" && (-f "$DIST_DIR/$lib_installed_file" || -f "$DIST_DIR/arm64/$lib_installed_file") ]]; then
		echo "Already built $dir_name"
		continue
	fi

	download

	unpack

	apply_patches

	build "x86_64"
	build "arm64"
done

echo "* Prepare for packaging"

# Merge the architectures together.
prepare_for_packaging

# Copy all files to the final destination
copy_to_final_destination

# Verify all files
verify

echo -n "Build ended at "; date
