/*

Copyright (c) 2011, Simon Howard

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

#include <stdlib.h>
#include <string.h>

#include "endian.h"
#include "lha_file_header.h"
#include "ext_header.h"

#define LEVEL_0_MIN_HEADER_LEN 22 /* bytes */
#define LEVEL_1_MIN_HEADER_LEN 25 /* bytes */

// Perform checksum of header contents.

static int checksum_header(uint8_t *header, size_t header_len, size_t csum)
{
	unsigned int result;
	unsigned int i;

	result = 0;

	for (i = 0; i < header_len; ++i) {
		result += header[i];
	}

	return (result & 0xff) == csum;
}

// Decode MS-DOS timestamp.

static unsigned decode_ftime(uint8_t *buf)
{
	// Ugh. TODO

	return lha_decode_uint32(buf);
}

// Decode the contents of the header.

static int decode_header(LHAFileHeader *header)
{
	uint8_t *data;
	size_t len, path_len;
	size_t min_len;

	data = header->raw_data;
	len = header->raw_data_len;

	// Sanity check header length.  This is the minimum header length
	// for a header that has a zero-length path.

	if (len < LEVEL_0_MIN_HEADER_LEN) {
		return 0;
	}

	// Compression method:

	memcpy(header->compress_method, data, 5);
	header->compress_method[5] = '\0';

	// File lengths:

	header->compressed_length = lha_decode_uint32(data + 5);
	header->length = lha_decode_uint32(data + 9);

	// Timestamp:

	header->timestamp = decode_ftime(data + 13);

	// Header level:

	header->header_level = data[18];

	switch (header->header_level) {
		case 0:
			min_len = LEVEL_0_MIN_HEADER_LEN;
			break;
		case 1:
			min_len = LEVEL_1_MIN_HEADER_LEN;
			break;

		default:
			// TODO
			return 0;
	}

	// Read path.  Check path length field - is the header long enough
	// to hold this full path?

	path_len = data[19];

	if (min_len + path_len > len) {
		return 0;
	}

	header->filename = malloc(path_len + 1);

	if (header->filename == NULL) {
		return 0;
	}

	memcpy(header->filename, data + 20, path_len);
	header->filename[path_len] = '\0';

	// CRC field.

	header->crc = lha_decode_uint16(data + 20 + path_len);

	return 1;
}

static int read_next_ext_header(LHAFileHeader **header,
                                LHAInputStream *stream,
                                uint8_t **ext_header,
                                size_t *ext_header_len)
{
	LHAFileHeader *new_header;
	size_t new_raw_len;

	// Last two bytes of the header raw data contain the size
	// of the next header.

	*ext_header_len = lha_decode_uint16((*header)->raw_data
	                              + (*header)->raw_data_len - 2);

	// No more headers?

	if (*ext_header_len == 0) {
		*ext_header = NULL;
		return 1;
	}

	// Reallocate header structure larger to accomodate the new
	// extended header.

	new_raw_len = (*header)->raw_data_len + *ext_header_len;
	new_header = realloc(*header, sizeof(LHAFileHeader) + new_raw_len);

	if (new_header == NULL) {
		return 0;
	}

	*header = new_header;
	new_header->raw_data = new_header + 1;
	*ext_header = new_header->raw_data + new_header->raw_data_len;

	// Read extended data from stream into new area.

	if (!lha_input_stream_read(stream, *ext_header, *ext_header_len)) {
		return 0;
	}

	new_header->raw_data_len = new_raw_len;

	return 1;
}

static int decode_extended_headers(LHAFileHeader **header,
                                   LHAInputStream *stream)
{
	uint8_t *ext_header;
	size_t ext_header_len;

	for (;;) {
		// Try to read the next header.

		if (!read_next_ext_header(header, stream,
		                          &ext_header, &ext_header_len)) {
			return 0;
		}

		// Last header?

		if (ext_header_len == 0) {
			break;
		}

		// In level 1 headers, the compressed length field is
		// actually "compressed length + length of all extended
		// headers":

		if ((*header)->header_level <= 1) {
			(*header)->compressed_length -= ext_header_len;
		}

		// Must be at least 3 bytes - 1 byte header type
		// + 2 bytes for next header length

		if (ext_header_len < 3) {
			return 0;
		}

		// Process header:

		lha_ext_header_decode(*header, ext_header[0],
		                      ext_header + 1, ext_header_len - 3);
	}

	return 1;
}

LHAFileHeader *lha_file_header_read(LHAInputStream *stream)
{
	LHAFileHeader *header;
	uint8_t header_len;
	uint8_t header_csum;

	// TODO: Needs refactoring to support level 2, 3 headers.

	// Read the "mini-header":

	if (!lha_input_stream_read_byte(stream, &header_len)
	 || !lha_input_stream_read_byte(stream, &header_csum)) {
		return NULL;
	}

	// Allocate result structure.

	header = malloc(sizeof(LHAFileHeader) + header_len);

	if (header == NULL) {
		return NULL;
	}

	memset(header, 0, sizeof(LHAFileHeader));

	// Read the raw header data and perform checksum.

	header->raw_data = header + 1;
	header->raw_data_len = header_len;

	if (!lha_input_stream_read(stream, header->raw_data, header_len)
	 || !checksum_header(header->raw_data, header_len, header_csum)) {
		goto fail;
	}

	// Checksum passes. Decode the header contents.

	if (!decode_header(header)) {
		goto fail;
	}

	// Read extended headers.
	// TODO: Fallback to ignoring extended headers if failure occurs
	// here?

	if (header->header_level >= 1
	 && !decode_extended_headers(&header, stream)) {
		goto fail;
	}

	return header;
fail:
	lha_file_header_free(header);
	return NULL;
}

void lha_file_header_free(LHAFileHeader *header)
{
	free(header->filename);
	free(header->path);
	free(header->unix_username);
	free(header->unix_group);
	free(header);
}

