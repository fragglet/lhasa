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

#include "lha_file_header.h"

#define MIN_HEADER_LEN 22 /* bytes */

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

// Decode 16-bit integer.

static uint16_t decode_uint16(uint8_t *buf)
{
	return buf[0] | (buf[1] << 8);
}

// Decode 32-bit integer.

static uint32_t decode_uint32(uint8_t *buf)
{
	return buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
}

// Decode MS-DOS timestamp.

static unsigned decode_ftime(uint8_t *buf)
{
	// Ugh. TODO

	return decode_uint32(buf);
}

// Decode the contents of the header.

static int decode_header(LHAFileHeader *header)
{
	uint8_t *data;
	size_t len, path_len;

	data = header->raw_data;
	len = header->raw_data_len;

	// Sanity check header length.  This is the minimum header length
	// for a header that has a zero-length path.

	if (len < MIN_HEADER_LEN) {
		return 0;
	}

	// Compression method:

	memcpy(header->compress_method, data, 5);
	header->compress_method[5] = '\0';

	// File lengths:

	header->compressed_length = decode_uint32(data + 5);
	header->length = decode_uint32(data + 9);

	// Timestamp:

	header->timestamp = decode_ftime(data + 13);

	// Read path.  Check path length field - is the header long enough
	// to hold this full path?

	path_len = data[19];

	if (MIN_HEADER_LEN + path_len > len) {
		return 0;
	}

	header->filename = malloc(path_len + 1);

	if (header->filename == NULL) {
		return 0;
	}

	memcpy(header->filename, data + 20, path_len);
	header->filename[path_len] = '\0';

	// CRC field.

	header->crc = decode_uint16(data + 20 + path_len);

	return 1;
}

LHAFileHeader *lha_file_header_read(LHAInputStream *stream)
{
	LHAFileHeader *header;
	uint8_t header_len;
	uint8_t header_csum;

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

	// Read the raw header data and perform checksum.

	header->raw_data = header + 1;
	header->raw_data_len = header_len;

	if (!lha_input_stream_read(stream, header->raw_data, header_len)
	 || !checksum_header(header->raw_data, header_len, header_csum)) {
		free(header);
		return NULL;
	}

	// Checksum passes. Decode the header contents.

	if (!decode_header(header)) {
		free(header);
		return NULL;
	}

	return header;
}

void lha_file_header_free(LHAFileHeader *header)
{
	free(header->filename);
	free(header);
}

