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

typeset -a otool_fail_str
otool_fail_str=( "is not an object file"
     "can't open file"
     "Archive : " )

parse_otool_output() {
#set -x
  typeset raw_output="${@}"

  typeset fail_str=""
  for fail_str in "${otool_fail_str[@]}"; do
    if echo "${raw_output}" | grep -q "${fail_str}"; then
      return 1
    fi
  done

  typeset tmp_regex='^[[:space:]]+(.*)[[:space:]]\(compatibility version .*, current version .*\)'

  # In this special case, we do not want read to perform any word splitting.
  typeset oldifs="${IFS}"
  IFS=''

  # Used for skipping the first matching entry - which should typically be the ID line...
  # That's a very naïve way to do this. Maybe there should be a bit more magic
  # to catch this more reliably.
  typeset -i first="1"

  typeset line=""
  while read -r line; do
    if [[ "${line}" =~ ${tmp_regex} ]]; then
      if [ "${first}" -ne "1" ]; then
        echo "${BASH_REMATCH[1]}"
      else
        first="0"
      fi
    fi
  done <<< "${raw_output}"

  IFS="${oldifs}"
#set +x
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
  # Port not being found in ${PATH} doesn't necessarily mean it isn't available.
  # Try to guess.
  MACPORTS_PREFIX_SEARCH="/opt/local/"
fi

NXPROXY="nxproxy"
PULSEAUDIO_BINARIES=( "pulseaudio" "esdcompat" "pacat" "pacmd"      "pactl"
                      "pamon"      "paplay"    "parec" "parecord"   "pasuspender" )
PULSEAUDIO_LIBRARIES=( "libpulse-simple.0.dylib"
                       "pulse-6.0"
                       "pulseaudio" )

typeset -a special_files_regex
special_files_regex+=( "pulseaudio/libpulsecommon-[0-9]\.[0-9]\.dylib" )

typeset -r dependency_base_format='@executable_path/../Frameworks/'

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

  if [ ! -d "${cur_lib_or_libdir}" ] && [ -x "${cur_lib_or_libdir}" ]; then
    echo "Adding ${cur_lib_or_libdir} to \${PULSEAUDIO_LIBRARIES_FULL}"
    PULSEAUDIO_LIBRARIES_FULL+=( "${cur_lib_or_libdir}" )
  elif [ -d "${cur_lib_or_libdir}" ]; then
    # That's a directory... more work needed here.
    echo "Scrubbing directory ${cur_lib_or_libdir}"
    typeset entry=""

    # -r parameter to read: Backslashes may NOT escape any characters!
    # -d '': specifies the delimiter to be used - as '' resolves to an empty string followed
    #        by a NUL character, the delimiter is set to this very NUL (\000) character.
    while read -r -d '' entry; do
      typeset cur_file="$(basename "${entry}")"
      typeset TMP_REGEX='^.*\.(\.[0-9]+){0,2}(so|dylib|bundle)$'

      # This is only here should the PA build system ever break and create
      # "linux-style" library file names. Let's hope it never actually comes to that.
      typeset TMP_REGEX_LINUX_COMPAT='^.*\.(so|dylib|bundle)(\.[0-9]+){0,2}$'

      if [[ "${cur_file}" =~ ${TMP_REGEX} ]] || [[ "${cur_file}" =~ ${TMP_REGEX_LINUX_COMPAT} ]]; then
        # Filename matched the expected template.
        echo "Adding $(lazy_canonical_path "${entry}") to \${PULSEAUDIO_LIBRARIES_FULL}"
        PULSEAUDIO_LIBRARIES_FULL+=( "$(lazy_canonical_path "${entry}")" )
      fi
    done < <(find "${cur_lib_or_libdir}" -type 'f' -print0)
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

phase "Copying PulseAudio"
for cur_binary in ${PULSEAUDIO_BINARIES_FULL[@]}; do
  cp -av "${cur_binary}" "${EXE_DIR}/"
done

typeset intermediate_lib_dir=""
for cur_binary in ${PULSEAUDIO_LIBRARIES_FULL[@]}; do
set -x
  intermediate_lib_dir="$(lazy_canonical_path "$(dirname "${cur_binary}")/")"
  intermediate_lib_dir="${intermediate_lib_dir##"$(lazy_canonical_path "${MACPORTS_PREFIX}/lib/")"}"

  mkdir -p "${FRAMEWORKS_DIR}/${intermediate_lib_dir}/"

  cp -av "${cur_binary}" "${FRAMEWORKS_DIR}/${intermediate_lib_dir}/"
set +x
done

if [ "${BUNDLE}" = "1" ]; then
  phase "Bundling nxproxy"
  dylibbundler \
    --fix-file "${EXE_DIR}/nxproxy" \
    --bundle-deps \
    --dest-dir "${FRAMEWORKS_DIR}/" \
    --install-path "@executable_path/../Frameworks/" \
    --create-dir

  phase "Bundling PulseAudio"
  typeset cur_binary_name=""
  for cur_binary in ${PULSEAUDIO_BINARIES_FULL[@]}; do
    if [ ! -L "${cur_binary}" ]; then
      cur_binary_name="$(basename "${cur_binary}")"
      dylibbundler \
        --fix-file "${EXE_DIR}/${cur_binary_name}" \
        --bundle-deps \
        --dest-dir "${FRAMEWORKS_DIR}/" \
        --install-path "@executable_path/../Frameworks/" \
        --create-dir \
        --overwrite-files
    fi
  done

  typeset intermediate_lib_dir=""
  for cur_binary in ${PULSEAUDIO_LIBRARIES_FULL[@]}; do
    intermediate_lib_dir="$(lazy_canonical_path "$(dirname "${cur_binary}")/")"
    intermediate_lib_dir="${intermediate_lib_dir##"$(lazy_canonical_path "${MACPORTS_PREFIX}/lib/")"}"

    if [ ! -L "${cur_binary}" ]; then
      cur_binary_name="$(basename "${cur_binary}")"
      echo "Handling ${cur_binary} => ${intermediate_lib_dir}/${cur_binary_name}."

      typeset nesting_level="$(get_nesting_level "${intermediate_lib_dir}")"

      dylibbundler \
        --fix-file "${FRAMEWORKS_DIR}/${intermediate_lib_dir}/${cur_binary_name}" \
        --bundle-deps \
        --dest-dir "${FRAMEWORKS_DIR}/${intermediate_lib_dir}" \
        --install-path "@executable_path/$(repeat_str "../" "${nesting_level}")../Frameworks/${intermediate_lib_dir}" \
        --create-dir \
        --overwrite-files
    fi
  done

  phase "Deduplicating PulseAudio libraries and dependencies"
  typeset -r base_dir="$(lazy_canonical_path "${FRAMEWORKS_DIR}")"

  typeset -a all_files
  typeset entry=""
  while read -r -d '' entry; do
    echo "Adding ${entry} to all files"
    all_files+=( "${entry}" )
  done < <(find "${base_dir}" -type 'f' -print0)

  typeset -a top_files
  for entry in "${all_files[@]}"; do
    typeset relative_path="${entry##"${base_dir}/"}"
    typeset tmp_regex='^[^/]+$'
    echo "Checking ${relative_path} against regex '${tmp_regex}'"
    if [[ "${relative_path}" =~ ${tmp_regex} ]]; then
      echo "${relative_path} is top file, adding to array."
      top_files+=( "${relative_path}" )
    fi
  done

  typeset -a duplicates
  for entry in "${all_files[@]}"; do
    typeset relative_path="${entry##"${base_dir}/"}"
    typeset file_name="$(basename "${entry}")"
    typeset top_entry=""
    for top_entry in "${top_files[@]}"; do
      if [ "${top_entry}" != "${relative_path}" ]; then
        if [ "${file_name}" = "${top_entry}" ]; then
          echo "Adding duplicate: ${relative_path}"
          duplicates+=( "${relative_path}" )
        fi
      fi
    done
  done

  echo "duplicates array before:"
  for entry in "${duplicates[@]}"; do
    echo "${entry}"
  done

  typeset -i i="0"
  for i in "${!duplicates[@]}"; do
    entry="${duplicates[${i}]}"
    typeset special_file_regex=""
    for special_file_regex in "${special_files_regex[@]}"; do
      typeset tmp_regex='^'"${special_file_regex}"'$'
      if [[ "${entry}" =~ ${tmp_regex} ]]; then
        mv -v "${base_dir}/$(basename "${entry}")" "${base_dir}/$(dirname "${special_file_regex}")/"
        duplicates[${i}]="$(basename "${entry}")"
        echo "Renamed ${entry} in duplicates array to ${duplicates[${i}]}"
      fi
    done
  done

  echo "duplicates array after:"
  for entry in "${duplicates[@]}"; do
    echo "${entry}"
  done

  for entry in "${duplicates[@]}"; do
    rm -v "${base_dir}/${entry}"
    typeset -i i="0"
    for i in "${!all_files[@]}"; do
      typeset all_entry="${all_files[${i}]}"
      typeset relative_path="${all_entry##"${base_dir}/"}"
      if [ "${relative_path}" = "${entry}" ]; then
        unset all_files[${i}]
      fi
    done
  done

  echo "New value for all_files:"
  for entry in "${all_files[@]}"; do
    echo "${entry}"
  done

  echo "Duplicates-to-real map:"
  # Build complementary array to duplicates.
  typeset -a to_files
  for entry in "${duplicates[@]}"; do
    typeset filename="$(basename "${entry}")"

    typeset all_entry=""
    for all_entry in "${all_files[@]}"; do
      typeset all_entry_filename="$(basename "${all_entry}")"

      if [ -n "${filename}" ] && [ -n "${all_entry_filename}" ]; then
        if [ "${filename}" = "${all_entry_filename}" ]; then
          typeset dependency_format="$(lazy_canonical_path "${dependency_base_format}/${all_entry##${base_dir}}")"
          to_files+=( "${dependency_format}" )

          echo "${entry} => ${dependency_format}"

          # There should be only one entry matching, so we can save a bit of time and break out of the loop.
          # Even more importantly, we only want one entry for each duplicates entry anyway...
          break
        fi
      else
        echo "ERROR: empty file name while matching duplicates with non-duplicates." >&2
        echo "ERROR: duplicate entry: \"${entry}\"" >&2
        echo "ERROR: real entry: \"${all_entry}\"" >&2
        exit 1
      fi
    done
  done

  # Try to fixup files broken by duplicates removal.
  for all_entry in "${all_files[@]}"; do
    typeset otool_out="$(otool -L "${all_entry}")"

    # Don't merge the declaration and initialization with the real value assignment.
    # We need the return value of parse_otool_output(), but running
    # typeset foo="$(bar)" will give us the return value of typeset, not bar().
    typeset dependencies=""
    dependencies="$(parse_otool_output "${otool_out}")"

    if [ "${?}" -eq "0" ]; then
      typeset line=""
      while read -r line; do
        #echo "dependency of ${all_entry}: ${line}"

        typeset duplicate_entry=""
        typeset -i i="0"
        for i in "${!duplicates[@]}"; do
          typeset duplicate_entry="${duplicates[${i}]}"
          #echo "checking for duplicate ${duplicate_entry}"
          typeset duplicate_format="$(lazy_canonical_path "${dependency_base_format}/${duplicate_entry}")"

          if [ -n "${line}" ] && [ -n "${duplicate_format}" ]; then
            if [ "${line}" = "${duplicate_format}" ]; then
              install_name_tool -change "${line}" "${to_files[${i}]}" "${all_entry}"
            fi
          else
            echo "ERROR: empty file name while replacing duplicate dependencies." >&2
            echo "ERROR: for file ${all_entry}" >&2
            echo "ERROR: at dependency ${line}" >&2
            echo "ERROR: duplicate entry: \"${duplicate_entry}\"" >&2
            echo "ERROR: dependency: \"${line}\"" >&2
            exit 1
          fi
        done
      done <<< "${dependencies}"
    else
      echo "WARNING: otool returned error for file: ${all_entry}" >&2
      echo "WARNING: skipping." >&2
    fi
  done

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
