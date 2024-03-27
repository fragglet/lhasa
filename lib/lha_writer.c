/*

Copyright (c) 2011, 2012, Simon Howard

Permission to use, copy, modify, and/or distribute this software
for any purpose with or without fee is hereby granted, provided
that the above copyright notice and this permission notice appear
in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "public/lha_file_header.h"
#include "crc16.h"
#include "header_defs.h"
#include "lha_arch.h"
#include "lha_codec.h"
#include "lha_encoder.h"
#include "lha_endian.h"
#include "lha_input_stream.h"
#include "lha_output_stream.h"

#define L1_HEADER_MAX_LEN 0x101  /* 0xff + 2 to include mini-header */
#define MAX_FILE_LENGTH 0xffffffffUL

/*

Routines for writing an LHA file. There are many different versions of
the LHA header and extended headers that are supported, so for
simplicity we only generate level 1 headers with a small number of
extended headers attached. The justification for this is that level 1
headers are backwards compatible with level 0 headers and so
essentially supported by every tool in existence. The extension header
system added in level 1 has been around since LHA v2 (1991) and is
similarly well-supported.

Extended headers which are optionally included are:
 * Path header (for storing directory name)
 * Windows timestamp header
 * Unix timestamp header
 * Unix UID/GID header.
 * Unix permissions header.
 * Common header (CRC)

*/

/**
 * In order to generate a complete LHA header, we must write several
 * sub-headers (main header and extended headers). This structure
 * contains function pointers for generating data for an individual
 * subheader.
 */

typedef struct {

	/**
	 * Get the size of the header data to be written.
	 *
	 * @param header  The header structure to write.
	 * @return        Number of bytes to write.
	 */

	size_t (*get_size)(LHAFileHeader *header);

	/**
	 * Write the header data into the specified buffer.
	 *
	 * @param header            The header structure to write.
	 * @param buf               Pointer to buffer to write the data.
	 * @param buf_len           Length of the buffer, in bytes.
	 * @param next_header_len   Length of the next header block, in bytes,
	 *                          or zero if this is the last header.
	 */

	void (*write)(LHAFileHeader *header, uint8_t *buf, size_t buf_len,
	              size_t next_header_len);
} SubHeaderWriter;

// Calculate the simple checksum that covers the level 0 header.

static uint8_t l0_checksum(uint8_t *buf, size_t buf_len)
{
	unsigned int result;
	unsigned int i;

	result = 0;

	for (i = 0; i < buf_len; ++i) {
		result += buf[i];
	}

	return result & 0xff;
}

// Encode a Unix timestamp value to an MS-DOS 'FTIME' value.

static uint32_t encode_ftime(unsigned int unix_timestamp)
{
	struct tm *datetime;
	time_t tmp = unix_timestamp;

	// Unix timestamps are seconds since the epoch, and as such are
	// time zone-independent. By comparison, FTIME values store the
	// actual date and time components. To convert to FTIME, we must
	// therefore choose a time zone to use; the local time zone seems
	// like the obvious choice.

	// TODO: Use localtime_r when possible.
	datetime = localtime(&tmp);

	return ((datetime->tm_sec & 0x3e) >> 1)
	     | ((datetime->tm_min & 0x3f) << 5)
	     | ((datetime->tm_hour & 0x1f) << 11)
	     | ((datetime->tm_mday & 0x1f) << 16)
	     | (((datetime->tm_mon + 1) & 0xf) << 21)
	     | (((datetime->tm_year - 80) & 0x7f) << 25);
}

// Main LHA header. This is the original "level 0" part.

static size_t level1_header_get_size(LHAFileHeader *header)
{
	size_t filename_len;

	if (header->filename != NULL) {
		filename_len = strlen(header->filename);
	} else {
		filename_len = 0;
	}

	return LEVEL_1_MIN_HEADER_LEN + filename_len + 2;
}

static void level1_header_write(LHAFileHeader *header,
                                uint8_t *buf, size_t buf_len,
                                size_t next_header_len)
{
	size_t filename_len = buf_len - LEVEL_1_MIN_HEADER_LEN - 2;

	// Fill in main fields.

	memcpy(buf + 2, header->compress_method, 5);
	lha_encode_uint32(buf + 7,
	                  header->compressed_length
			  + header->raw_data_len - buf_len);
	lha_encode_uint32(buf + 11, header->length);
	lha_encode_uint32(buf + 15, encode_ftime(header->timestamp));

	// Normal MS-DOS attribute; level 1 header.

	buf[19] = 0x20;
	buf[20] = 1;

	buf[21] = filename_len;

	memcpy(&buf[22], header->filename, filename_len);
	lha_encode_uint16(buf + 22 + filename_len, header->crc);

	buf[24 + filename_len] = header->os_type;

	// Next header length field.

	lha_encode_uint16(buf + buf_len - 2, next_header_len);

	// Generate the "mini header" that is at the start of the header,
	// which contains the length of the LHA header and its checksum.

	buf[0] = (uint8_t) (buf_len - 2);
	buf[1] = l0_checksum(buf + 2, buf_len - 2);
}

// Write the header and footer fields for an extended header.

static void begin_extended_header(uint8_t *buf, size_t buf_len,
                                  size_t next_header_len,
                                  uint8_t header_type, uint16_t header_len)
{
	buf[0] = header_type;

	// Every extended header includes 3 bytes of header and footer,
	// so add 3 to the header length when encoding it.

	lha_encode_uint16(buf + buf_len - 2, next_header_len);
}

// Path header. This is included when we have a directory name.

static size_t path_header_get_size(LHAFileHeader *header)
{
	if (header->path != NULL) {
		return 3 + strlen(header->path);
	} else {
		return 0;
	}
}

static void path_header_write(LHAFileHeader *header,
                              uint8_t *buf, size_t buf_len,
                              size_t next_header_len)
{
	size_t path_len;
	unsigned int i;

	path_len = buf_len - 3;

	begin_extended_header(buf, buf_len, next_header_len,
	                      LHA_EXT_HEADER_PATH, path_len);

	memcpy(buf + 1, header->path, path_len);

	// A value of 0xff is used as the path separator within the path
	// header. Translate from Unix path separator to this.

	for (i = 0; i < path_len; ++i) {
		if (buf[i + 1] == '/') {
			buf[i + 1] = 0xff;
		}
	}
}

// Windows timestamp header. This is only included when we have the
// Windows timestamp fields in the header structure set.

static size_t win_ts_header_get_size(LHAFileHeader *header)
{
	if ((header->extra_flags & LHA_FILE_WINDOWS_TIMESTAMPS) != 0) {
		return 3 + 24;
	} else {
		return 0;
	}
}

static void win_ts_header_write(LHAFileHeader *header,
                                uint8_t *buf, size_t buf_len,
                                size_t next_header_len)
{
	begin_extended_header(buf, buf_len, next_header_len,
	                      LHA_EXT_HEADER_WINDOWS_TIMESTAMPS, 24);

	lha_encode_uint64(buf + 1, header->win_creation_time);
	lha_encode_uint64(buf + 9, header->win_modification_time);
	lha_encode_uint64(buf + 17, header->win_access_time);
}

// Unix timestamp header.

static size_t unix_ts_header_get_size(LHAFileHeader *header)
{
	if (header->timestamp != 0) {
		return 3 + 4;
	} else {
		return 0;
	}
}

static void unix_ts_header_write(LHAFileHeader *header,
                                 uint8_t *buf, size_t buf_len,
	                         size_t next_header_len)
{
	begin_extended_header(buf, buf_len, next_header_len,
	                      LHA_EXT_HEADER_UNIX_TIMESTAMP, 4);

	lha_encode_uint32(buf + 1, header->timestamp);
}

// Unix UID/GID header. Only included when the Unix UID/GID fields in
// the header structure are set.

static size_t unix_uid_gid_header_get_size(LHAFileHeader *header)
{
	if ((header->extra_flags & LHA_FILE_UNIX_UID_GID) != 0) {
		return 3 + 4;
	} else {
		return 0;
	}
}

static void unix_uid_gid_header_write(LHAFileHeader *header,
                                      uint8_t *buf, size_t buf_len,
	                              size_t next_header_len)
{
	begin_extended_header(buf, buf_len, next_header_len,
	                      LHA_EXT_HEADER_UNIX_UID_GID, 4);

	lha_encode_uint16(buf + 1, header->unix_uid);
	lha_encode_uint16(buf + 3, header->unix_gid);
}

// Unix permissions header. Only included when the Unix permissions field in
// the header structure is set.

static size_t unix_perms_header_get_size(LHAFileHeader *header)
{
	if ((header->extra_flags & LHA_FILE_UNIX_PERMS) != 0) {
		return 3 + 2;
	} else {
		return 0;
	}
}

static void unix_perms_header_write(LHAFileHeader *header,
                                    uint8_t *buf, size_t buf_len,
	                            size_t next_header_len)
{
	begin_extended_header(buf, buf_len, next_header_len,
	                      LHA_EXT_HEADER_UNIX_PERMISSION, 2);

	lha_encode_uint16(buf + 1, header->unix_perms);
}

// Common header. This is a special header that is included as the final
// header, and contains a 16-bit CRC checksum of the entire header.

static size_t common_header_get_size(LHAFileHeader *header)
{
	return 3 + 2;
}

static void common_header_write(LHAFileHeader *header,
                                uint8_t *buf, size_t buf_len,
                                size_t next_header_len)
{
	uint16_t crc;

	begin_extended_header(buf, buf_len, next_header_len,
	                      LHA_EXT_HEADER_COMMON, 2);

	// There's a catch-22 when calculating the CRC, in that the CRC
	// field is part of the header itself. So we calculate with the
	// CRC field set to zero and then rewrite it.
	// Note that this only works because it's the last header that
	// we generate, so the raw_data array has been fully populated.

	lha_encode_uint16(buf + 1, 0);

	crc = 0;
	lha_crc16_buf(&crc, header->raw_data, header->raw_data_len);
	header->common_crc = crc;

	lha_encode_uint16(buf + 1, crc);
}

static const SubHeaderWriter subheaders[] = {
	{ level1_header_get_size,        level1_header_write },
	{ path_header_get_size,          path_header_write },
	{ win_ts_header_get_size,        win_ts_header_write },
	{ unix_ts_header_get_size,       unix_ts_header_write },
	{ unix_uid_gid_header_get_size,  unix_uid_gid_header_write },
	{ unix_perms_header_get_size,    unix_perms_header_write },
	{ common_header_get_size,        common_header_write },
};

#define NUM_SUBHEADERS (sizeof(subheaders) / sizeof(*subheaders))

/**
 * Calculate the lengths of each of the subheaders in the
 * 'subheaders' array.
 *
 * @param header              The LHA header to be written.
 * @param subheader_lengths   Pointer to array in which to store
 *                            subheader lengths.
 * @return                    Total length of header data.
 */

static size_t calculate_subheader_lengths(LHAFileHeader *header,
                                          size_t *subheader_lengths)
{
	unsigned int i;
	size_t total;

	total = 0;

	for (i = 0; i < NUM_SUBHEADERS; ++i) {
		subheader_lengths[i] = subheaders[i].get_size(header);
		total += subheader_lengths[i];
	}

	return total;
}

/**
 * Calculate the length of the next subheader to follow the
 * specified subheader.
 *
 * @param subheader_lengths  Pointer to array containing the length of each
 *                           subheader in the 'subheaders' array.
 * @param index              Index of the subheader for which we are
 *                           calculating the 'next subheader' value.
 * @return                   Length of the next subheader, or zero if
 *                           this is the last subheader.
 */

static size_t next_subheader_length(size_t *subheader_lengths,
                                    unsigned int index)
{
	unsigned int i;

	// Find the first subheader following the specified subheader that has
	// a non-zero length.

	for (i = index + 1; i < NUM_SUBHEADERS; ++i) {
		if (subheader_lengths[i] != 0) {
			return subheader_lengths[i];
		}
	}

	// Last subheader.

	return 0;
}

/**
 * Generate the 'raw_data' buffer containing LHA header data.
 *
 * @param header          The LHA header to be written.
 * @param header_lengths  Pointer to array containing the length of each
 *                        subheader in the 'subheaders' array.
 * @return                Non-zero for success.
 */

static int generate_header_data(LHAFileHeader *header,
                                size_t *subheader_lengths)
{
	unsigned int i;
	unsigned int offset;

	header->raw_data = malloc(header->raw_data_len);

	if (header->raw_data == NULL) {
		return 0;
	}

	offset = 0;

	for (i = 0; i < NUM_SUBHEADERS; ++i) {

		// If the length is zero, it indicates that this
		// subheader should be skipped.

		if (subheader_lengths[i] == 0) {
			continue;
		}

		subheaders[i].write(
			header, header->raw_data + offset,
			subheader_lengths[i],
			next_subheader_length(subheader_lengths, i));

		offset += subheader_lengths[i];
	}

	return 1;
}

static size_t file_read_callback(void *buf, size_t buf_len, void *user_data)
{
	return fread(buf, 1, buf_len, user_data);
}

static int lha_write_file_data(LHAOutputStream *out, LHAFileHeader *header,
                               FILE *instream)
{
	LHACodec *codec;
	LHAEncoder *encoder;
	uint8_t buf[64];
	uint64_t uncompressed_len;
	size_t compressed_len = 0, cnt;

	// TODO: For now, we don't do any kind of encoding, we only write
	// the -lh0- uncompressed encoding type.
	codec = lha_encoder_for_name("-lh0-");
	if (codec == NULL) {
		return 0;
	}

	encoder = lha_encoder_new(codec, file_read_callback, instream);

	for (;;) {
		cnt = lha_encoder_read(encoder, buf, sizeof(buf));
		if (cnt == 0) {
			break;
		}
		if (ferror(instream)
		 || !lha_output_stream_write(out, buf, cnt)) {
			return 0;
		}

		// The header format uses 32-bit integers to represent the file
		// uncompressed and compressed sizes, so there is an inherent
		// limit on file size. We must therefore ensure that the
		// counter does not overflow.
		if (compressed_len > MAX_FILE_LENGTH - cnt) {
			lha_encoder_free(encoder);
			return 0;
		}
		compressed_len += cnt;
	}

	// Overflow check for uncompressed size.
	uncompressed_len = lha_encoder_get_length(encoder);
	if (uncompressed_len > UINT32_MAX) {
		lha_encoder_free(encoder);
		return 0;
	}

	memcpy(header->compress_method, "-lh0-", 6);
	header->length = (uint32_t) uncompressed_len;
	header->compressed_length = compressed_len;
	header->crc = lha_encoder_get_crc(encoder);
	lha_encoder_free(encoder);

	return header->crc;
}

// Unix LHa generates symlinks in a weird way, but we don't want to invent
// yet another variant on headers, so we preserve that format. Here's what
// it looks like:
//
//   1.  b -> d      -  no path      filename=b|d
//   2.  b -> c/d    -  path=b|c/    filename=d
//   3.  a/b -> d    -  path=a/      filename=b|d
//   4.  a/b -> c/d  -  path=a/b|c/  filename=d
//
// To avoid adding lots of complication to the header writing code above, we
// do a "patch-up" transform of the path and filename fields to get them
// into the format we want. But before lha_write_file() returns, we change
// the fields back again back to their previous values.

static int symlink_filename_transform(LHAFileHeader *header, char **orig_path,
                                      char **orig_filename)
{
	char *buf, *p;
	size_t len;

	*orig_path = header->path;
	*orig_filename = header->filename;

	if (header->symlink_target == NULL) {
		return 1;
	}

	len = (header->path != NULL ? strlen(header->path) : 0)
	    + strlen(header->filename) + 1 + strlen(header->symlink_target);

	buf = malloc(len + 1);
	if (buf == NULL) {
		return 0;
	}

	snprintf(buf, len + 1, "%s%s|%s",
	         header->path != NULL ? header->path : "",
	         header->filename, header->symlink_target);

	p = strrchr(buf, '/');
	if (p == NULL) {
		// case 1 above.
		header->filename = buf;
		header->path = NULL;
		return 1;
	}

	header->filename = strdup(p + 1);
	len = p - buf + 1;
	header->path = malloc(len + 1);

	if (header->filename == NULL || header->path == NULL) {
		free(header->filename);
		free(header->path);
		free(buf);
		header->filename = *orig_filename;
		header->path = *orig_path;

		return 0;
	}

	memcpy(header->path, buf, len);
	header->path[len] = '\0';
	free(buf);
	return 1;
}

static void symlink_filename_restore(LHAFileHeader *header, char **orig_path,
                                     char **orig_filename)
{
	if (*orig_path != header->path || *orig_filename != header->filename) {
		free(header->path);
		free(header->filename);
		header->path = *orig_path;
		header->filename = *orig_filename;
	}
}

int lha_write_file(LHAOutputStream *out, LHAFileHeader *header, FILE *instream)
{
	size_t subheader_lengths[NUM_SUBHEADERS];
	off_t header_loc, eof_loc, data_loc;
	char *orig_path, *orig_filename;

	if (level1_header_get_size(header) > L1_HEADER_MAX_LEN
	 || (header->filename == NULL && header->path == NULL)) {
		return 0;
	}

	if (!symlink_filename_transform(header, &orig_path, &orig_filename)) {
		return 0;
	}

	// We need to save the location of the header in the output file so
	// that we can come back later to write it.
	header_loc = lha_output_stream_tell(out);

	// Some of the header contents may change or not be known until after
	// we have written the compressed file contents, but the header length
	// will not change.
	header->raw_data_len =
		calculate_subheader_lengths(header, subheader_lengths);

	data_loc = header_loc + header->raw_data_len;
	if (!lha_output_stream_seek(out, data_loc)) {
		goto restore_and_fail;
	}

	if (header->filename != NULL && header->symlink_target == NULL) {
		// Write compressed data. The compress_method, length and
		// compressed_length fields will be populated.
		if (!lha_write_file_data(out, header, instream)) {
			goto restore_and_fail;
		}
	} else {
		memcpy(header->compress_method, LHA_COMPRESS_TYPE_DIR, 6);
		header->length = 0;
		header->compressed_length = 0;
		header->crc = 0;
	}

	// Generate the header data.
	if (!generate_header_data(header, subheader_lengths)) {
		goto restore_and_fail;
	}

	symlink_filename_restore(header, &orig_path, &orig_filename);

	// Go back and write the header.
	eof_loc = lha_output_stream_tell(out);
	if (!lha_output_stream_seek(out, header_loc)
	 || !lha_output_stream_write(out, header->raw_data,
	                             header->raw_data_len)) {
		return 0;
	}

	// Done.
	return lha_output_stream_seek(out, eof_loc);

restore_and_fail:
	symlink_filename_restore(header, &orig_path, &orig_filename);
	return 0;
}

