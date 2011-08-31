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

// Decoder for "new-style" LHA algorithms, used with LHA v2 and onwards
// (-lh4-, -lh5-, -lh6-, -lh7-).
//
// This file is designed to be a template. It is #included by other
// files to generate an optimized decoder.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "lha_decoder.h"

#include "bit_stream_reader.c"

// Transform symbol names used in this file to use distinct names
// based on the algorithm.

#define TRANSFORM_NAME2(prefix, x) lha_ ## prefix ## _ ## x

#define LHANewDecoder      TRANSFORM_NAME(Decoder)
#define lha_lh_new_init    TRANSFORM_NAME(init)
#define lha_lh_new_read    TRANSFORM_NAME(read)
#define lha_lh_new_decoder TRANSFORM_NAME(decoder)

// Threshold for copying. The first copy code starts from here.

#define COPY_THRESHOLD       3 /* bytes */

// Required size of the output buffer.  At most, a single call to read()
// might result in a copy of the entire ring buffer.

#define OUTPUT_BUFFER_SIZE   RING_BUFFER_SIZE

// Number of different command codes. 0-255 range are literal byte
// values, while higher values indicate copy from history.

#define NUM_CODES            510

// Number of possible codes in the "temporary table" used to encode the
// codes table.

#define MAX_TEMP_CODES 19

typedef struct
{
	// Input bit stream.

	BitStreamReader bit_stream_reader;

	// Ring buffer of past data.  Used for position-based copies.

	uint8_t ringbuf[RING_BUFFER_SIZE];
	unsigned int ringbuf_pos;

	// Number of commands remaining before we start a new block.

	unsigned int block_remaining;

	// Lookup table to map from 12-bit input value to code.
	// Codes are variable length, up to 12 bits long. We therefore need a
	// table with 4096 entries.

	uint16_t code_lookup[4096];

	// Length of each code, in bits.

	uint8_t code_lengths[NUM_CODES];

} LHANewDecoder;

// Initialize the history ring buffer.

static void init_ring_buffer(LHANewDecoder *decoder)
{
	memset(decoder->ringbuf, ' ', RING_BUFFER_SIZE);
	decoder->ringbuf_pos = 0;
}

static int lha_lh_new_init(void *data, LHADecoderCallback callback,
                           void *callback_data)
{
	LHANewDecoder *decoder = data;

	// Initialize input stream reader.

	bit_stream_reader_init(&decoder->bit_stream_reader,
	                       callback, callback_data);

	// Initialize data structures.

	init_ring_buffer(decoder);

	// First read starts the first block.

	decoder->block_remaining = 0;

	return 1;
}

// Read a length value - this is normally a value in the 0-7 range, but
// sometimes can be longer.

static int read_length_value(LHANewDecoder *decoder)
{
	int i, len;

	len = read_bits(&decoder->bit_stream_reader, 3);

	if (len < 0) {
		return -1;
	}

	if (len == 7) {
		do {
			i = read_bit(&decoder->bit_stream_reader);

			if (i < 0) {
				return -1;
			}
		} while (i == 1);
	}

	return len;
}

// Read the values from the input stream that define the temporary table
// used for encoding the code table.

static int read_temp_table(LHANewDecoder *decoder)
{
	int i, j, n, len, code;
	uint8_t code_lengths[MAX_TEMP_CODES];

	// How many codes?

	n = read_bits(&decoder->bit_stream_reader, 5);

	if (n < 0) {
		return 0;
	}

	// n=0 is a special case, meaning only a single code that
	// is of zero length.

	if (n == 0) {
		code = read_bits(&decoder->bit_stream_reader, 5);

		// ...
		return 1;
	}

	// Enforce a hard limit on the number of codes.

	if (n >= MAX_TEMP_CODES) {
		n = MAX_TEMP_CODES;
	}

	// Read the length of each code.

	for (i = 0; i < n; ++i) {
		len = read_length_value(decoder);

		if (len < 0) {
			return -1;
		}

		code_lengths[i] = len;

		// After the first three lengths, there is a 2-bit
		// field to allow skipping over up to a further three
		// lengths. Not sure of the reason for this ...

		if (i == 2) {
			len = read_bits(&decoder->bit_stream_reader, 2);

			if (len < 0) {
				return -1;
			}

			for (j = 0; j < len; ++j) {
				++i;
				code_lengths[i] = 0;
			}
		}
	}

	// TODO: Build table from codes

	return 1;
}

// Start reading a new block from the input stream.

static int start_new_block(LHANewDecoder *decoder)
{
	int len;

	// Read length of new block (in commands).

	len = read_bits(&decoder->bit_stream_reader, 16);

	if (len < 0) {
		return 0;
	}

	decoder->block_remaining = (size_t) len;

	// Read the temporary decode table, used to encode the codes table.
	// The position table data structure is reused for this.

	if (!read_temp_table(decoder)) {
		return 0;
	}

	// TODO: Read encoded table data.

	return 1;
}

// Read the next code from the input stream. Returns the code, or -1 if
// an error occurred.

static int read_code(LHANewDecoder *decoder)
{
	int value;
	int code;

	// The code can be up to 12 bits long. Look at the next 12 bits
	// from the input stream and use the lookup table to find out
	// what code this is.

	value = peek_bits(&decoder->bit_stream_reader, 12);

	if (value < 0) {
		return -1;
	}

	code = decoder->code_lookup[value];

	// Skip past the code (depending on length) and return.

	read_bits(&decoder->bit_stream_reader, decoder->code_lengths[code]);

	return code;
}

// Add a byte value to the output stream.

static void output_byte(LHANewDecoder *decoder, uint8_t *buf,
                        size_t *buf_len, uint8_t b)
{
	buf[*buf_len] = b;
	++*buf_len;

	decoder->ringbuf[decoder->ringbuf_pos] = b;
	decoder->ringbuf_pos = (decoder->ringbuf_pos + 1) % RING_BUFFER_SIZE;
}

static size_t lha_lh_new_read(void *data, uint8_t *buf)
{
	LHANewDecoder *decoder = data;
	size_t result;
	int code;

	// Start of new block?

	if (decoder->block_remaining == 0) {
		if (!start_new_block(decoder)) {
			return 0;
		}
	}

	--decoder->block_remaining;

	// Read next command from input stream.

	result = 0;

	code = read_code(decoder);

	if (code < 0) {
		return 0;
	}

	// The code may be either a literal byte value or a copy command.

	if (code < 256) {
		output_byte(decoder, buf, &result, (uint8_t) code);
	} else {

	}

	return result;
}

LHADecoderType lha_lh_new_decoder = {
	lha_lh_new_init,
	NULL,
	lha_lh_new_read,
	sizeof(LHANewDecoder),
	OUTPUT_BUFFER_SIZE,
	RING_BUFFER_SIZE
};


