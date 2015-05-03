#!/bin/bash

make_boolean() {
	OPTION="${1}"

	case "${OPTION}" in
		("0"|"no"|""|"No"|"nO"|"NO"|"false"|"FALSE") OPTION="0";;
		(*) OPTION="1";;
	esac

	printf "${OPTION}"
}

phase() {
	echo
	echo "***"
	echo "*** ${1}..."
	echo "***"
	echo
}

usage() {
	exec >&2

	NO_VAL="0, no, NO, No, nO, false or FALSE"
	printf "$(basename ${0}): usage\n\n"
	printf "Accepted environment variables:\n"
	printf "\tSDK:\t\t\t\tsets the target SDK [string]\n\t\t\t\t\tdefault: /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.7.sdk\n"
	printf "\tMACOSX_DEPLOYMENT_TARGET:\tsets the deployment target (specific OS X version to optimize/build for) [string]\n\t\t\t\t\tdefault: 10.7\n"
	printf "\tDEBUG\t\t\t\tenables or disables debug builds [boolean]\n\t\t\t\t\tdefault: disabled\n"
	printf "\tBUNDLE\t\t\t\tenables or disables library bundling and the creation of a .dmg installer [boolean]\n\t\t\t\t\tdefault: enabled\n"
	printf "\tUNIVERSAL\t\t\tenables or disables x86 support. x86_64 support is always enabled [boolean]\n\t\t\t\t\tdefault: enabled\n"
	printf "\tMACPORTS_PREFIX\t\t\tsets the (MacPorts) prefix used to detect PulseAudio and nxproxy binaries\n\t\t\t\t\tdefault: /opt/local/\n"
	printf "\n"
	printf "Boolean values help:\n"
	printf "\ta value of ${NO_VAL} will be treated as false\n"
	printf "\tany other value will be treated as true\n"

	exit 2
}

MATCH_HELP='(^((-h)|(--help))([ 	]|$))|([ 	]+((-h)|(--help))([ 	]|$))'
[ -n "${*}" ] && [[ "${*}" =~ ${MATCH_HELP} ]] && usage

NAME="x2goclient"

TOP_DIR="$(dirname "$0")"
[[ "${TOP_DIR}" == /* ]] || TOP_DIR="${PWD}/${TOP_DIR#./}"
BUILD_DIR="${TOP_DIR}/client_build"
APPBUNDLE="${BUILD_DIR}/${NAME}.app"
DMGFILE="${BUILD_DIR}/${NAME}.dmg"
PROJECT="${TOP_DIR}/${NAME}.pro"
PKG_DMG="${TOP_DIR}/pkg-dmg"

NXPROXY="$(which nxproxy)"
# Try to find the MacPorts prefix.
typeset MACPORTS_PREFIX_SEARCH=""
if type -P port >/dev/null 2>&1; then
	MACPORTS_PREFIX_SEARCH="$(type -P port)"
	MACPORTS_PREFIX_SEARCH="${MACPORTS_PREFIX_SEARCH%%bin/port}"
else
	# Port not being part find in ${PATH} doesn't necessarily mean it isn't available.
	# Try to guess.
	MACPORTS_PREFIX_SEARCH="/opt/local/"
fi

: ${SDK:="/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.7.sdk"}
: ${MACOSX_DEPLOYMENT_TARGET:="10.7"}
: ${DEBUG:="0"}
: ${BUNDLE:="1"}
: ${UNIVERSAL:="1"}
: ${MACPORTS_PREFIX:="${MACPORTS_PREFIX_SEARCH}"}

unset MACPORTS_PREFIX_SEARCH

DEBUG="$(make_boolean "${DEBUG}")"
BUNDLE="$(make_boolean "${BUNDLE}")"
UNIVERSAL="$(make_boolean "${UNIVERSAL}")"

[ "${DEBUG}" -eq "0" ] && BUILD_MODE="release" || BUILD_MODE="debug"

BUILD_ARCH="x86_64"
[ "${UNIVERSAL}" -eq "1" ] && BUILD_ARCH="${BUILD_ARCH} x86"

SDK_MINOR_VERSION="$(/usr/bin/perl -pe 's#.*?10\.(\d+).*?\.sdk$#\1#' <<< "${SDK}")"

MATCH_NUMBERS='^[0-9]+$'
if [[ "${SDK_MINOR_VERSION}" =~ ${MATCH_NUMBERS} ]]; then
	[ "${SDK_MINOR_VERSION}" -gt "6" ] && STDLIB="libstdc++"
	[ "${SDK_MINOR_VERSION}" -gt "8" ] && STDLIB="libc++"
else
	echo "Unable to determine OS X version. Unknown value '${SDK_MINOR_VERSION}'." >&2
	exit 1
fi

set -e

phase "Cleaning"
make clean

# Create gitlog.
git --no-pager log --since "2 years ago" --format="%ai %aN (%h) %n%n%x09*%w(68,0,10) %s%d%n" > "ChangeLog.gitlog"
mv "ChangeLog.gitlog" "res/txt/git-info"

# Copy debian changelog as the general changelog.
cp -a "debian/changelog" "res/txt/"

[ -e "${BUILD_DIR}" ] && rm -rf "${BUILD_DIR}"

mkdir "${BUILD_DIR}"
pushd "${BUILD_DIR}"

phase "Running lrelease"
lrelease "${PROJECT}"

phase "Running qmake"
qmake -config "${BUILD_MODE}" -spec macx-g++ "${PROJECT}" \
	CONFIG+="${BUILD_ARCH}" \
	QMAKE_MAC_SDK="${SDK}" \
	QMAKE_MACOSX_DEPLOYMENT_TARGET="${MACOSX_DEPLOYMENT_TARGET}" \
	OSX_STDLIB="${STDLIB}"

phase "Running make"
make -j2

phase "Copying nxproxy"
mkdir -p "${APPBUNDLE}/Contents/exe"
cp "${NXPROXY}" "${APPBUNDLE}/Contents/exe"

if [ "${BUNDLE}" = "1" ]; then
	dylibbundler \
		--fix-file "${APPBUNDLE}/Contents/exe/nxproxy" \
		--bundle-deps \
		--dest-dir "${APPBUNDLE}/Contents/Frameworks" \
		--install-path "@executable_path/../Frameworks/" \
		--create-dir

	phase "Bundling up using macdeployqt"
	macdeployqt "${APPBUNDLE}" -verbose=2

	phase "Creating DMG"
	${PKG_DMG} \
		--source "${APPBUNDLE}" \
		--sourcefile \
		--target "${DMGFILE}" \
		--volname "x2goclient" \
		--verbosity 2 \
		--mkdir "/.background" \
		--copy "${TOP_DIR}/res/img/png/macinstaller_background.png:/.background" \
		--copy "${TOP_DIR}/res/osxbundle/macdmg.DS_Store:/.DS_Store" \
		--copy "${TOP_DIR}/LICENSE" \
		--copy "${TOP_DIR}/COPYING" \
		--symlink "/Applications: " \
		--icon "${TOP_DIR}/res/img/icons/x2go-mac.icns" \
		--format "UDBZ"
fi

popd
