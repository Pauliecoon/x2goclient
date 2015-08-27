#!/bin/bash

typeset base_dir=""
base_dir="${1:?"No base dir given."}"

typeset -a special_files_regex
special_files_regex+=( "pulseaudio/libpulsecommon-[0-9]\.[0-9]\.dylib" )

typeset -a otool_fail_str
otool_fail_str=( "is not an object file"
		 "can't open file"
		 "Archive : " )

parse_otool_output() {
set -x
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
set +x
	return 0
}

typeset -a all_files
typeset entry=""
while read -r -d '' entry; do
	all_files+=( "${entry}" )
done < <(find "${base_dir}" -type 'f' -print0)

typeset -a top_files
for entry in "${all_files[@]}"; do
	typeset relative_path="${entry##"${base_dir}/"}"
	typeset tmp_regex='^[^/]+$'
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
			echo "mv \"${base_dir}/$(basename "${entry}")\" \"${base_dir}/$(dirname "${special_file_regex}")/\""
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
	echo "rm -v ${base_dir}/${entry}"
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
				typeset dependency_format="@executable_path/../Frameworks/${all_entry##${base_dir}}"
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

	typeset dependencies="$(parse_otool_output "${otool_out}")"
	if [ "${?}" -eq 0 ]; then
		typeset line=""
		while read -r line; do
			typeset dependencies_filename="$(basename "${line}")"

			typeset duplicate_entry=""
			typeset -i i="0"
			for i in "${!duplicates[@]}"; do
				typeset duplicate_entry="${duplicates[${i}]}"
				typeset duplicate_filename="$(basename "${duplicate_entry}")"

				if [ -n "${dependencies_filename}" ] && [ -n "${duplicate_filename}" ]; then
					if [ "${dependencies_filename}" = "${duplicate_filename}" ]; then
						echo "install_name_tool -change \"${line}\" \"${to_files[${i}]}\" \"${all_entry}\""
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
		echo "ERROR: otool returned error for file: ${all_entry}" >&2
		exit 1
	fi
done
