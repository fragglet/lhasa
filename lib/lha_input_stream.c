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

#include "lha_input_stream.h"

// Maximum length of the self-extractor header.
// If we don't find an LHA file header after this many bytes, give up.

#define MAX_SFX_HEADER_LEN 4096

// Size of the lead-in buffer used to skip the self-extractor.

#define LEADIN_BUFFER_LEN 16

typedef enum {
	LHA_INPUT_STREAM_INIT,
	LHA_INPUT_STREAM_READING,
	LHA_INPUT_STREAM_FAIL
} LHAInputStreamState;

struct _LHAInputStream {
	LHAInputStreamState state;
	FILE *stream;
	uint8_t leadin[LEADIN_BUFFER_LEN];
	size_t leadin_len;
};

LHAInputStream *lha_input_stream_new(FILE *stream)
{
	LHAInputStream *result;

	result = malloc(sizeof(LHAInputStream));

	if (result == NULL) {
		return NULL;
	}

	result->stream = stream;
	result->leadin_len = 0;
	result->state = LHA_INPUT_STREAM_INIT;

	return result;
}

void lha_input_stream_free(LHAInputStream *reader)
{
	free(reader);
}

// Check if the specified buffer is the start of a file header.

static int file_header_match(uint8_t *buf)
{
	return buf[2] == '-'
	    && (!strncmp((char *) buf + 3, "lh", 2)
	     || !strncmp((char *) buf + 3, "pm", 2)
	     || !strncmp((char *) buf + 3, "lz", 2))
	    && buf[6] == '-';
}

// Empty some of the bytes from the start of the lead-in buffer.

static void empty_leadin(LHAInputStream *reader, size_t bytes)
{
	memmove(reader->leadin, reader->leadin + bytes,
	        reader->leadin_len - bytes);
	reader->leadin_len -= bytes;
}

// Skip the self-extractor header at the start of the file.
// Returns non-zero if a header was found.

static int skip_sfx(LHAInputStream *reader)
{
	size_t read, filepos;
	unsigned int i;

	filepos = 0;

	while (filepos < MAX_SFX_HEADER_LEN) {

		// Add some more bytes to the lead-in buffer:

		read = fread(reader->leadin + reader->leadin_len,
		             1, LEADIN_BUFFER_LEN - reader->leadin_len,
		             reader->stream);

		if (read <= 0) {
			break;
		}

		reader->leadin_len += read;

		// Check the lead-in buffer for a file header.

		for (i = 0; i + 7 < reader->leadin_len; ++i) {
			if (file_header_match(reader->leadin + i)) {
				empty_leadin(reader, i);
				return 1;
			}
		}

		empty_leadin(reader, i);
		filepos += i;
	}

	return 0;
}

int lha_input_stream_read(LHAInputStream *reader, void *buf, size_t buf_len)
{
	size_t n, total_bytes;

	// Start of the stream?  Skip self-extract header, if there is one.

	if (reader->state == LHA_INPUT_STREAM_INIT) {
		if (skip_sfx(reader)) {
			reader->state = LHA_INPUT_STREAM_READING;
		} else {
			reader->state = LHA_INPUT_STREAM_FAIL;
		}
	}

	if (reader->state == LHA_INPUT_STREAM_FAIL) {
		return 0;
	}

	// Now fill the result buffer. Start by emptying the lead-in buffer.

	total_bytes = 0;

	if (reader->leadin_len > 0) {
		if (buf_len < reader->leadin_len) {
			n = buf_len;
		} else {
			n = reader->leadin_len;
		}

		memcpy(buf, reader->leadin, n);
		empty_leadin(reader, n);
		total_bytes += n;
	}

	// Read from the input stream.

	if (total_bytes < buf_len) {
		n = fread((uint8_t *) buf + total_bytes,
		          1, buf_len - total_bytes,
		          reader->stream);
		
		if (n > 0) {
			total_bytes += n;
		}
	}

	// Only successful if the complete buffer is filled.

	return total_bytes == buf_len;
}

int lha_input_stream_read_byte(LHAInputStream *reader, uint8_t *result)
{
	return lha_input_stream_read(reader, result, 1);
}

int lha_input_stream_read_short(LHAInputStream *reader, uint16_t *result)
{
	uint8_t buf[2];
	
	if (!lha_input_stream_read(reader, buf, 2)) {
		return 0;
	}

	*result = (uint16_t) (buf[0] | (buf[1] << 8));

	return 1;
}

int lha_input_stream_read_long(LHAInputStream *reader, uint32_t *result)
{
	uint8_t buf[4];
	
	if (!lha_input_stream_read(reader, buf, 4)) {
		return 0;
	}

	*result = (uint32_t) (buf[0] | (buf[1] << 8)
	                    | (buf[2] << 16) | (buf[3] << 24));

	return 1;
}

int lha_input_stream_skip(LHAInputStream *reader, size_t bytes)
{
	int result;

	result = fseek(reader->stream, (long) bytes, SEEK_CUR);

	return result == 0;
}

