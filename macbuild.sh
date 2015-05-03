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

dependency_error() {
	exec >&2

	typeset element="${1}"; shift
	typeset component="${1}"; shift
	typeset type="${1}"; shift

	echo "${element} ${type} not found."
	echo "Install ${component} -- e.g., via "port -vt install ${component}" if using MacPorts."
	echo "If ${component} is already installed, try passing MACPORTS_PREFIX if the autodetected or default value (${MACPORTS_PREFIX}) does not match your setup."

	exit 3
}

lazy_canonical_path() {
	typeset path="${1}"

	typeset old_path=""
	while [ "${old_path}" != "${path}" ]; do
		old_path="${path}"
		path="${path//\/\///}"
	done

	printf "${old_path}"
}

get_nesting_level() {
set -x
	typeset -i level=0
	typeset path="${1}"

	while [ -n "${path}" ] && [ "${path}" != "." ] && [ "${path}" != "/" ]; do
		i="$((${i} + 1))"
		path="$(dirname "${path}")"
	done

	printf "${level}"
set +x
}

repeat_str() { # INPUT COUNT
	typeset INPUT="${1:?"Error: no input string passed to ${FUNCNAME}()."}"
	typeset COUNT="${2:?"Error: no count passed to ${FUNCNAME}()."}"

	typeset ret=""
	typeset -i i=0
	while [ "${i}" -lt "${COUNT}" ]; do
		ret="${ret}$(printf "${INPUT}")"
		i=$((${i} + 1))
	done

	printf "${ret}"

	return 0
}

MATCH_HELP='(^((-h)|(--help))([ 	]|$))|([ 	]+((-h)|(--help))([ 	]|$))'
[ -n "${*}" ] && [[ "${*}" =~ ${MATCH_HELP} ]] && usage

NAME="x2goclient"

TOP_DIR="$(dirname "$0")"
[[ "${TOP_DIR}" == /* ]] || TOP_DIR="${PWD}/${TOP_DIR#./}"
BUILD_DIR="${TOP_DIR}/client_build"
APPBUNDLE="${BUILD_DIR}/${NAME}.app"
EXE_DIR="${APPBUNDLE}/Contents/exe/"
FRAMEWORKS_DIR="${APPBUNDLE}/Contents/Frameworks/"
DMGFILE="${BUILD_DIR}/${NAME}.dmg"
PROJECT="${TOP_DIR}/${NAME}.pro"
PKG_DMG="${TOP_DIR}/pkg-dmg"

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

NXPROXY="nxproxy"
PULSEAUDIO_BINARIES=( "pulseaudio" "esdcompat" "pacat" "pacmd"      "pactl"
                      "pamon"      "paplay"    "parec" "parecord"   "pasuspender" )
PULSEAUDIO_LIBRARIES=( "libpulse-simple.0.dylib"
                       "libpulse.0.dylib"
                       "libpulsecore-6.0.dylib"
                       "pulse-6.0"
                       "pulseaudio" )

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

# Gather files.
NXPROXY="$(lazy_canonical_path "${MACPORTS_PREFIX}/bin/${NXPROXY}")"

[ -x "${NXPROXY}" ] || dependency_error "nxproxy" "nxproxy" "binary"

typeset -i i
typeset -i fail
typeset -a PULSEAUDIO_BINARIES_FULL
typeset cur_binary
fail="0"
for cur_binary in ${PULSEAUDIO_BINARIES[@]}; do
	cur_binary="$(lazy_canonical_path "${MACPORTS_PREFIX}/bin/${cur_binary}")"

	if [ -x "${cur_binary}" ]; then
		PULSEAUDIO_BINARIES_FULL+=( "${cur_binary}" )
	else
		fail="1"
		break
	fi
done

[ "${fail}" -eq "1" ] && dependency_error "${cur_binary##"$(lazy_canonical_path "${MACPORTS_PREFIX}/bin/")"}" "pulseaudio" "binary"

typeset cur_lib_or_libdir
typeset -a PULSEAUDIO_LIBRARIES_FULL
fail="0"
for cur_lib_or_libdir in ${PULSEAUDIO_LIBRARIES[@]}; do
	cur_lib_or_libdir="$(lazy_canonical_path "${MACPORTS_PREFIX}/lib/${cur_lib_or_libdir}")"

	if [ -x "${cur_lib_or_libdir}" ]; then
		PULSEAUDIO_LIBRARIES_FULL+=( "${cur_lib_or_libdir}" )
	elif [ -d "${cur_lib_or_libdir}" ]; then
		# That's a directory... more work needed here.
		typeset entry=""
		for entry in "${cur_lib_or_libdir}"/*; do
			typeset TMP_REGEX='^.*\.(so|dylib|bundle)(\.[0-9]+){0,2}$'
			if [[ "${entry}" =~ ${TMP_REGEX} ]]; then
				# Filename matched the expected template.
				PULSEAUDIO_LIBRARIES_FULL+=( "$(lazy_canonical_path "${cur_lib_or_libdir}/${entry}")" )
			fi
		done
	else
		fail="1"
		break
	fi
done

[ "${fail}" -eq "1" ] && dependency_error "${cur_lib_or_libdir}" "pulseaudio" "library or library directory"

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

mkdir -p "${EXE_DIR}/"
mkdir -p "${FRAMEWORKS_DIR}/"

phase "Copying nxproxy"
cp -av "${NXPROXY}" "${EXE_DIR}/"

if [ "${BUNDLE}" = "1" ]; then
	dylibbundler \
		--fix-file "${EXE_DIR}/nxproxy" \
		--bundle-deps \
		--dest-dir "${FRAMEWORKS_DIR}/" \
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
