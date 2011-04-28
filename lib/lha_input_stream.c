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

struct _LHAInputStream {
	FILE *stream;
};

LHAInputStream *lha_input_stream_new(FILE *stream)
{
	LHAInputStream *result;

	result = malloc(sizeof(LHAInputStream));

	if (result == NULL) {
		return NULL;
	}

	result->stream = stream;

	return result;
}

void lha_input_stream_free(LHAInputStream *reader)
{
	free(reader);
}

int lha_input_stream_read(LHAInputStream *reader, void *buf, size_t buf_len)
{
	size_t read;

	read = fread(buf, buf_len, 1, reader->stream);

	// Only successful if the complete item is read.
	return read == 1;
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

	*result = buf[0] | (buf[1] << 8);

	return 1;
}

int lha_input_stream_read_long(LHAInputStream *reader, uint32_t *result)
{
	uint8_t buf[4];
	
	if (!lha_input_stream_read(reader, buf, 4)) {
		return 0;
	}

	*result = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);

	return 1;
}

int lha_input_stream_skip(LHAInputStream *reader, size_t bytes)
{
	int result;

	result = fseek(reader->stream, bytes, SEEK_CUR);

	return result == 0;
}

