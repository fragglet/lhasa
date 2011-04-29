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

#include "crc16.h"

#include "lha_decoder.h"
#include "lha_reader.h"

struct _LHAReader {
	LHAInputStream *stream;
	LHAFileHeader *curr_file;
	LHADecoder *decoder;
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
	reader->decoder = NULL;
	reader->eof = 0;

	return reader;
}

void lha_reader_free(LHAReader *reader)
{
	if (reader->curr_file != NULL) {
		lha_file_header_free(reader->curr_file);
	}

	if (reader->decoder != NULL) {
		lha_decoder_free(reader->decoder);
	}

	free(reader);
}

LHAFileHeader *lha_reader_next_file(LHAReader *reader)
{
	if (reader->eof) {
		return NULL;
	}

	// Free a decoder if we have one.

	if (reader->decoder != NULL) {
		lha_decoder_free(reader->decoder);
		reader->decoder = NULL;
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

static size_t decoder_callback(void *buf, size_t buf_len, void *user_data)
{
	return lha_reader_read_compressed(user_data, buf, buf_len);
}

static void open_decoder(LHAReader *reader)
{
	LHADecoderType *dtype;

	// Look up the decoder to use for this compression method.

	dtype = lha_decoder_for_name(reader->curr_file->compress_method);

	if (dtype == NULL) {
		return;
	}

	// Create decoder.

	reader->decoder = lha_decoder_new(dtype, decoder_callback, reader);
}

size_t lha_reader_read(LHAReader *reader, void *buf, size_t buf_len)
{
	// The first time this is called, we have to create a decoder.

	if (reader->curr_file != NULL && reader->decoder == NULL) {
		open_decoder(reader);

		if (reader->decoder == NULL) {
			return 0;
		}
	}

	// Read from decoder and return result.

	return lha_decoder_read(reader->decoder, buf, buf_len);
}

int lha_reader_check(LHAReader *reader)
{
	uint8_t buf[64];
	uint16_t crc;
	unsigned int bytes, total_bytes;

	if (reader->curr_file == NULL) {
		return 0;
	}

	// Decompress the current file, performing a running
	// CRC of the contents as we go.

	total_bytes = 0;
	crc = 0;

	do {
		bytes = lha_reader_read(reader, buf, sizeof(buf));

		lha_crc16_buf(&crc, buf, bytes);

		total_bytes += bytes;
	} while (bytes > 0);

	// Decompressed length should match, as well as CRC.

	return total_bytes == reader->curr_file->length
	    && crc == reader->curr_file->crc;
}

