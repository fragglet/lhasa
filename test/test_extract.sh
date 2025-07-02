#!/usr/bin/env bash
#
# Copyright (c) 2011, 2012, Simon Howard
#
# Permission to use, copy, modify, and/or distribute this software
# for any purpose with or without fee is hereby granted, provided
# that the above copyright notice and this permission notice appear
# in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
# WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
# AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
# CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
# LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
# NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
# CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#
#
# Base functions for extract tests
#

. test_common.sh

run_sandbox="$wd/extract1"
w_sandbox="$wd/extract2"
gather_sandbox="$wd/extract3"

remove_sandboxes() {
	if [ -e "$run_sandbox" ]; then
		chmod -R +wx "$run_sandbox"
		rm -rf "$run_sandbox"
	fi
	if [ -e "$w_sandbox" ]; then
		chmod -R +wx "$w_sandbox"
		rm -rf "$w_sandbox"
	fi
	if [ -e "$gather_sandbox" ]; then
		chmod -R +wx "$gather_sandbox"
		rm -rf "$gather_sandbox"
	fi
}

make_sandboxes() {
	remove_sandboxes
	mkdir "$run_sandbox"
	mkdir "$w_sandbox"
	mkdir "$gather_sandbox"
}

# "Simplify" a filename - on Windows, filenames ending in a period
# have that period removed - ie. "gpl-2." becomes "gpl-2". So we
# must check for the latter rather than the former.

simplify_filename() {
	local filename=$1
	if [ "$build_arch" = "windows" ]; then
		echo "$filename" | sed 's/\.$//'
	else
		echo "$filename"
	fi
}

# Generate the specified files to overwrite in the run sandbox:

files_to_overwrite() {
	for filename in "$@"; do
		filename=$(simplify_filename "$filename")
		local tmpfile="$run_sandbox/$filename"
		mkdir -p "$(dirname "$tmpfile")"
		echo "__OW_FILE__" > "$tmpfile"
	done
}

# Check that the specified filenames exist in the run sandbox:

check_exists() {
	local archive_file=$1
	shift

	for filename in "$@"; do
		x_filename=$(simplify_filename "$filename")
		if [ ! -e "$run_sandbox/$x_filename" ] && \
		   [ ! -L "$run_sandbox/$x_filename" ]; then
			fail "File was not extracted as expected:"  \
			     "  archive: $archive_file"             \
			     "  filename: $filename"
		fi
	done
}

# Check that the specified filenames do not exist in the run sandbox:

check_not_exists() {
	local archive_file=$1
	shift

	for filename in "$@"; do
		x_filename=$(simplify_filename "$filename")
		if [ -e "$run_sandbox/$x_filename" ] || \
		   [ -L "$run_sandbox/$x_filename" ]; then
			fail "File was extracted, not as expected:"  \
			     "  archive: $archive_file"              \
			     "  filename: $filename"
		fi
	done
}

# Check that the specified filenames have been overwritten in the run sandbox:

check_overwritten() {
	local archive_file=$1
	shift

	for filename in "$@"; do
		x_filename=$(simplify_filename "$filename")
		if grep -q __OW_FILE__ "$run_sandbox/$x_filename" 2>/dev/null;
		then
			fail "File was not overwritten as expected:" \
			     "  archive: $archive_file"              \
			     "  filename: $filename"
		fi
	done
}

# Check that the specified filenames in the run sandbox were not overwritten:

check_not_overwritten() {
	local archive_file=$1
	shift

	for filename in "$@"; do
		x_filename=$(simplify_filename "$filename")
		if ! grep -q __OW_FILE__ "$run_sandbox/$x_filename" 2>/dev/null;
		then
			fail "File was overwritten, not as expected:"   \
			     "  archive: $archive_file"                 \
			     "  filename: $filename"
		fi
	done
}

check_extracted_file() {
	local archive_file=$1
	local filename=$2
	local timestamp=$(get_file_data "$archive_file" \
	                  "$filename" timestamp)
	local unix_perms=$(get_file_data "$archive_file" \
	                   "$filename" unix_perms)
	local symlink_target=$(get_file_data "$archive_file" \
	                       "$filename" symlink_target)

	#echo "check_extracted_file: $@"

	check_exists "$archive_file" "$filename"

	# Symbolic links are treated specially. Just check where the
	# link points to. If this isn't Unix, don't expect anything.

	if [ "$symlink_target" != "" ]; then
		local link_value=$(readlink "$run_sandbox/$filename")

		if [ "$link_value" != "$symlink_target" ]; then
			fail "Symlink mismatch for $archive_file" \
			     "'$link_value' != '$symlink_target'"
		fi

		return
	fi

	if [ "$timestamp" != "" ]; then
		local file_ts=$(file_mod_time "$run_sandbox/$filename")

		if [ "$file_ts" != "$timestamp" ]; then
			fail "Timestamp mismatch for $archive_file" \
			     "$filename: $file_ts != $timestamp"
		fi
	fi

	# Check file permissions. The permissions in the -hdr files
	# look like "0100644" - strip these down to just the last
	# three numbers.

	if [ "$build_arch" = "unix" ] && [ "$unix_perms" != "" ]; then
		local file_perms=$(file_perms "$run_sandbox/$filename" \
		                   | sed 's/.*\(...\)/\1/')
		unix_perms=$(echo $unix_perms | sed 's/.*\(...\)/\1/')

		if [ "$file_perms" != "$unix_perms" ]; then
			fail "Permission mismatch for $archive_file" \
			     "$filename: $file_perms != $unix_perms"
		fi
	fi
}

check_extracted_files() {
	local archive_file=$1

	get_file_data "$archive_file" | while read; do
		check_extracted_file "$archive_file" "$REPLY"
	done
}

# Run the program under test, comparing the output to the contents
# of the specified file.

lha_check_output() {
	#echo "lha_check_output: $@"
	local expected_output="$1"
	shift
	local output="$wd/output.txt"

	cd "$run_sandbox"
	# Invoke test command and save output.
	# test outputs have their test root as '/tmp'; adjust accordingly
	test_lha "$@" 2>&1 | $test_base/string-replace "$wd" /tmp > "$output"

	cd "$test_base"

	if $GATHER && [ ! -e "$expected_output" ]; then
		cd "$gather_sandbox"
		$LHA_TOOL "$@" > $expected_output
		cd "$test_base"
	fi

	if ! diff -u "$expected_output" "$output"; then
		fail "Output not as expected for command:" \
		     "    lha $*" >&2
	fi

	rm -f "$output"
}

# Basic 'lha e' extract.

test_basic_extract() {
	local archive_file=$1
	local expected_file="$test_base/output/$archive_file-e.txt"

	make_sandboxes

	lha_check_output "$expected_file" e $(test_arc_file "$archive_file")

	check_extracted_files "$archive_file"

	remove_sandboxes
}

# Basic extract, reading from stdin.

test_stdin_extract() {
	local archive_file=$1
	local expected_file="$test_base/output/$archive_file-e.txt"

	make_sandboxes

	# This is *not* a useless use of cat. If a < pipe was used,
	# the input would come from a file handle, not a pipe. Using
	# cat forces the data to come from a pipe.

	cat $(test_arc_file "$archive_file") | \
	    lha_check_output "$expected_file" e -

	check_extracted_files "$archive_file"

	remove_sandboxes
}

# Extract with 'w' option to specify destination directory.

test_w_option() {
	local archive_file=$1
	shift

	local expected_file="$test_base/output/$archive_file-ew.txt"

	# Extract into a subdirectory of the 'w' sandbox that does not
	# exist: the path should be created as part of the extract.

	local extract_dir="$w_sandbox/dir"

	if $is_cygwin; then
		extract_dir=$(cygpath -w "$w_sandbox/dir")
	fi

	make_sandboxes

	lha_check_output "$expected_file" \
	                 "ew=$extract_dir" $(test_arc_file "$archive_file")

	# Check that the specified filenames exist in w_sandbox.

	for filename in "$@"; do
		x_filename=$(simplify_filename "$filename")

		if [ ! -e "$w_sandbox/dir/$x_filename" ] && \
		   [ ! -L "$w_sandbox/dir/$x_filename" ]; then
			fail "Failed to extract $filename from $archive_file"
		fi
	done

	remove_sandboxes
}

# Extract with level 1 quiet option to partially silence output.

test_q1_option() {
	local archive_file=$1
	shift

	make_sandboxes
	expected="$wd/expected.txt"

	for filename in "$@"; do
		local symlink=$(get_file_data "$archive_file" \
		                              "$filename" symlink_target)
		if [ "$symlink" != "" ]; then
			printf "Symbolic Link $filename -> $symlink\n"
		else
			printf "\r$filename :\r$filename\t- Melted  \n"
		fi
	done >"$expected"

	lha_check_output "$expected" \
	                 eq1 $(test_arc_file "$archive_file")

	check_exists "$archive_file" "$@"

	rm -f "$expected"

	remove_sandboxes
}

# Extract with level 2 quiet option to fully silence output.

test_q_option() {
	local cmd=$1
	local archive_file=$2
	shift; shift

	make_sandboxes

	files_to_overwrite "$@"
	lha_check_output /dev/null $cmd $(test_arc_file "$archive_file")

	check_exists "$archive_file" "$filename"

	# The -q option also causes an existing file to be overwritten
	# without confirmation (like -f). Make sure files are overwritten.

	check_overwritten "$archive_file" "$@"

	remove_sandboxes
}

# Extract with 'i' option to ignore directory of archived files.

test_i_option() {
	local archive_file=$1
	shift

	make_sandboxes

	# Hackishly transform the file containing the expected output to
	# remove the parent directory. This gives the expected output when
	# using the -i option.
	# The transformation turns out to be a bit complicated because for
	# "Symbolic Link" lines we only want to transform the left side.
	sed '/^Symbolic/  s/[A-Za-z0-9\/]*\/\([a-z]* ->\)/\1/;
	     /^Symbolic/! s/[A-Za-z0-9\/ ]*\///g'                \
	      < "$test_base/output/$archive_file-e.txt"         \
	      > "$expected"

	lha_check_output "$expected" \
	                 ei $(test_arc_file "$archive_file")

	for filename in "$@"; do
		local base_filename=$(basename "$filename")
		check_exists "$archive_file" "$base_filename"
	done

	rm -f "$expected"
	remove_sandboxes
}

# Extract with the 'f' option to force overwrite of an existing file.

test_f_option() {
	local archive_file=$1
	shift

	local expected_file="$test_base/output/$archive_file-e.txt"

	make_sandboxes
	files_to_overwrite "$@"

	lha_check_output "$expected_file" \
	                 ef $(test_arc_file "$archive_file")

	check_exists "$archive_file" "$@"
	check_overwritten "$archive_file" "$@"

	remove_sandboxes
}

test_archive() {
	local archive_file=$1
	shift

	#echo "test_archive $archive_file"

	# Don't check symlink archives on non-Unix systems that
	# don't support them.

	if [ "$build_arch" != "unix" ] && echo "$1" | grep -q symlink; then
		return
	fi

	test_basic_extract "$archive_file" "$@"
	test_stdin_extract "$archive_file" "$@"
	test_w_option "$archive_file" "$@"
	test_q_option eq "$archive_file" "$@"
	test_q_option eq2 "$archive_file" "$@"
	test_q1_option "$archive_file" "$@"
	test_i_option "$archive_file" "$@"
	test_f_option "$archive_file" "$@"
	# TODO: check v option
}
