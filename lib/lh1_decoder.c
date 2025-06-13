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
#include <inttypes.h>

#include "lh1_common.h"
#include "lha_codec.h"

#include "bit_stream_reader.c"

typedef struct {
	LHALH1State state;

	// Input bit stream.
	BitStreamReader bit_stream_reader;

	// Ring buffer of past data.  Used for position-based copies.
	uint8_t ringbuf[RING_BUFFER_SIZE];
	unsigned int ringbuf_pos;
} LHALH1Decoder;

// Initialize the history ring buffer.

static int lha_lh1_init(void *data, LHACodecCallback callback,
                        void *callback_data)
{
	LHALH1Decoder *decoder = data;

	// Initialize input stream reader.

	bit_stream_reader_init(&decoder->bit_stream_reader,
	                       callback, callback_data);

	// Initialize data structures.
	lha_lh1_init_state(&decoder->state);

	memset(decoder->ringbuf, ' ', RING_BUFFER_SIZE);
	decoder->ringbuf_pos = 0;

	return 1;
}

// Read a code from the input stream.

static int read_code(LHALH1Decoder *decoder, uint16_t *result)
{
	LHALH1State *state = &decoder->state;
	unsigned int node_index;
	int bit;

	// Start from the root node, and traverse down until a leaf is
	// reached.

	node_index = 0;

	//printf("<root ");
	while (!state->nodes[node_index].leaf) {
		bit = read_bit(&decoder->bit_stream_reader);

		if (bit < 0) {
			return 0;
		}

		//printf("<%i>", bit);

		// Choose one of the two children depending on the
		// bit that was read.

		node_index = state->nodes[node_index].child_index
		           - (unsigned int) bit;
	}

	*result = state->nodes[node_index].child_index;
	//printf(" -> %i!>\n", *result);

	lha_lh1_increment_for_code(state, *result);

	return 1;
}

// Read an offset code from the input stream.

static int read_offset(LHALH1Decoder *decoder, unsigned int *result)
{
	unsigned int offset;
	int future, offset2;

	// The offset can be up to 8 bits long, but is likely not
	// that long. Use the lookup table to find the offset
	// and its length.

	future = peek_bits(&decoder->bit_stream_reader, 8);

	if (future < 0) {
		return 0;
	}

	offset = decoder->state.offset_lookup[future];

	// Skip past the offset bits and also read the following
	// lower-order bits.

	read_bits(&decoder->bit_stream_reader,
	          decoder->state.offset_lengths[offset]);

	offset2 = read_bits(&decoder->bit_stream_reader, 6);

	if (offset2 < 0) {
		return 0;
	}

	*result = (offset << 6) | (unsigned int) offset2;

	return 1;
}

static void output_byte(LHALH1Decoder *decoder, uint8_t *buf,
                        size_t *buf_len, uint8_t b)
{
	buf[*buf_len] = b;
	++*buf_len;

	decoder->ringbuf[decoder->ringbuf_pos] = b;
	decoder->ringbuf_pos = (decoder->ringbuf_pos + 1) % RING_BUFFER_SIZE;
}

static size_t lha_lh1_read(void *data, uint8_t *buf)
{
	LHALH1Decoder *decoder = data;
	size_t result;
	uint16_t code;

	result = 0;

	// Read the next code from the input stream.

	if (!read_code(decoder, &code)) {
		return 0;
	}

	// The code either indicates a single byte to be output, or
	// it indicates that a block should be copied from the ring
	// buffer as it is a repeat of a sequence earlier in the
	// stream.

	if (code < 0x100) {
		output_byte(decoder, buf, &result, (uint8_t) code);
	} else {
		unsigned int count, start, i, pos, offset;

		// Read the offset into the history at which to start
		// copying.

		if (!read_offset(decoder, &offset)) {
			return 0;
		}

		count = code - 0x100U + COPY_THRESHOLD;
		start = decoder->ringbuf_pos - offset + RING_BUFFER_SIZE - 1;

		// Copy from history into output buffer:

		for (i = 0; i < count; ++i) {
			pos = (start + i) % RING_BUFFER_SIZE;

			output_byte(decoder, buf, &result,
			            decoder->ringbuf[pos]);
		}
	}

	return result;
}

const LHACodec lha_lh1_decoder = {
	lha_lh1_init,
	NULL,
	lha_lh1_read,
	sizeof(LHALH1Decoder),
	OUTPUT_BUFFER_SIZE,
	RING_BUFFER_SIZE
};
