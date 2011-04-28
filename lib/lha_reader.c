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

#include "lha_reader.h"

struct _LHAReader {
	LHAInputStream *stream;
	LHAFileHeader *curr_file;
	size_t curr_file_remaining;
	int eof;
};

LHAReader *lha_reader_new(LHAInputStream *stream)
{
	LHAReader *reader;

	reader = malloc(sizeof(LHAReader));

	if (reader == NULL) {
		return NULL;
	}

	reader->stream = stream;
	reader->curr_file = NULL;
	reader->curr_file_remaining = 0;
	reader->eof = 0;

	return reader;
}

void lha_reader_free(LHAReader *reader)
{
	if (reader->curr_file != NULL) {
		lha_file_header_free(reader->curr_file);
	}

	free(reader);
}

LHAFileHeader *lha_reader_next_file(LHAReader *reader)
{
	if (reader->eof) {
		return NULL;
	}

	// Free the current file header and skip over any remaining
	// compressed data that hasn't been read yet.

	if (reader->curr_file != NULL) {
		lha_file_header_free(reader->curr_file);

		lha_input_stream_skip(reader->stream,
		                      reader->curr_file_remaining);
	}

	// Read the header for the next file.

	reader->curr_file = lha_file_header_read(reader->stream);

	if (reader->curr_file == NULL) {
		reader->eof = 1;
		return NULL;
	}

	reader->curr_file_remaining = reader->curr_file->compressed_length;

	return reader->curr_file;
}

size_t lha_reader_read_compressed(LHAReader *reader, void *buf, size_t buf_len)
{
	size_t bytes;

	if (reader->eof || reader->curr_file_remaining == 0) {
		return 0;
	}

	// Read up to the number of bytes of compressed data remaining.

	if (buf_len > reader->curr_file_remaining) {
		bytes = reader->curr_file_remaining;
	} else {
		bytes = buf_len;
	}

	if (!lha_input_stream_read(reader->stream, buf, bytes)) {
		reader->eof = 1;
		return 0;
	}

	// Update counter and return success.

	reader->curr_file_remaining -= bytes;

	return bytes;
}

size_t lha_reader_read(LHAReader *reader, void *buf, size_t buf_len)
{
	// TODO
	return 0;
}

