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
#include <ctype.h>

#include "lh1_common.h"
#include "lha_codec.h"
#include "search_buffer.h"

#include "bit_stream_writer.c"

#define READ_BUFFER_SIZE  64

typedef struct {
	LHALH1State state;

	// Input bit stream.
	BitStreamWriter bit_stream_writer;

	LHACodecCallback callback;
	void *callback_data;

	uint8_t read_buffer[READ_BUFFER_SIZE];
	unsigned int read_buffer_pos, read_buffer_len;
	int eof;

	SearchBuffer search_buffer;
} LHALH1Encoder;

// Initialize the history ring buffer.

static int lha_lh1_encoder_init(void *data, LHACodecCallback callback,
                                void *callback_data)
{
	LHALH1Encoder *encoder = data;

	bit_stream_writer_init(&encoder->bit_stream_writer);
	lha_lh1_init_state(&encoder->state);

	encoder->callback = callback;
	encoder->callback_data = callback_data;
	encoder->read_buffer_pos = 0;
	encoder->read_buffer_len = 0;
	encoder->eof = 0;

	return lha_search_buffer_init(&encoder->search_buffer,
	                              RING_BUFFER_SIZE);
}

static int refill_input_buffer(LHALH1Encoder *encoder)
{
	// Empty out already-read data, but only when the buffer is half-empty.
	if (encoder->read_buffer_pos >= encoder->read_buffer_len
	 || encoder->read_buffer_pos > READ_BUFFER_SIZE / 2) {
		memmove(encoder->read_buffer,
		        encoder->read_buffer + encoder->read_buffer_pos,
		        encoder->read_buffer_len - encoder->read_buffer_pos);
		encoder->read_buffer_len -= encoder->read_buffer_pos;
		encoder->read_buffer_pos = 0;
	}

	// Refill the buffer. We do this repeatedly to catch EOF condition.
	while (!encoder->eof && encoder->read_buffer_len < READ_BUFFER_SIZE) {
		size_t cnt = encoder->callback(
			encoder->read_buffer + encoder->read_buffer_len,
			READ_BUFFER_SIZE - encoder->read_buffer_len,
			encoder->callback_data);
		encoder->read_buffer_len += cnt;
		if (cnt == 0) {
			encoder->eof = 1;
		}
	}

	return encoder->read_buffer_pos < encoder->read_buffer_len;
}

static uint8_t read_next_byte(LHALH1Encoder *encoder)
{
	uint8_t result = encoder->read_buffer[encoder->read_buffer_pos];
	++encoder->read_buffer_pos;
	return result;
}

static void write_code(LHALH1Encoder *encoder, unsigned int code)
{
	LHALH1State *state = &encoder->state;
	unsigned int node_index, parent_index, bit;
	unsigned int out, bits;

	node_index = state->leaf_nodes[code];

	// We start from the leaf node and walk up to the root.
	bits = 0;
	out = 0;
	while (node_index != 0) {
		parent_index = state->nodes[node_index].parent;

		bit = node_index != state->nodes[parent_index].child_index;
		out = out | (bit << bits);
		bits++;
		node_index = parent_index;
	}

	write_bits(&encoder->bit_stream_writer, out, bits);
	lha_lh1_increment_for_code(state, code);
}

static void write_offset(LHALH1Encoder *encoder, int offset)
{
	unsigned int top, bottom;

	top = (offset >> 6) & 0x3f;
	bottom = offset & 0x3f;

	write_bits(&encoder->bit_stream_writer,
	           encoder->state.offset_codes[top],
	           encoder->state.offset_lengths[top]);
	write_bits(&encoder->bit_stream_writer, bottom, 6);
}

static size_t search_bytes(LHALH1Encoder *encoder)
{
	size_t result = encoder->read_buffer_len - encoder->read_buffer_pos;
	size_t max_copy = NUM_CODES - 0x100 - 1 + COPY_THRESHOLD;
	if (result < max_copy) {
		return result;
	} else {
		return max_copy;
	}
}

static size_t lha_lh1_encoder_read(void *data, uint8_t *buf)
{
	LHALH1Encoder *encoder = data;
	SearchResult r;
	size_t result = 0;
	unsigned int i;

	while (result < OUTPUT_BUFFER_SIZE) {
		result += flush_bytes(&encoder->bit_stream_writer, buf + result,
		                      OUTPUT_BUFFER_SIZE - result);

		if (!refill_input_buffer(encoder)) {
			break;
		}

		r = lha_search_buffer_search(
			&encoder->search_buffer,
			encoder->read_buffer + encoder->read_buffer_pos,
			search_bytes(encoder));

		if (r.length < COPY_THRESHOLD) {
			uint8_t c = read_next_byte(encoder);
			write_code(encoder, c);
			lha_search_buffer_insert(&encoder->search_buffer, c);
#ifdef TRACE
			printf("byte %d", c);
			if (isprint(c)) {
				printf(" (%c)", c);
			}
			printf("\n");
#endif
		} else {
			write_code(encoder, 0x100 + r.length - COPY_THRESHOLD);
			write_offset(encoder, r.offset - 1);

			for (i = 0; i < r.length; ++i) {
				lha_search_buffer_insert(
					&encoder->search_buffer,
					read_next_byte(encoder));
			}
#ifdef TRACE
			printf("copy %d, %d\n", r.length, r.offset - 1);
#endif
		}

		// At EOF, there may still be bits left over waiting to be
		// written, so flush them out by writing some zero bits.
		if (encoder->eof
		 && encoder->read_buffer_pos >= encoder->read_buffer_len) {
			write_bits(&encoder->bit_stream_writer, 0, 7);
		}
	}

	return result;
}

LHACodec lha_lh1_encoder = {
	lha_lh1_encoder_init,
	NULL,
	lha_lh1_encoder_read,
	sizeof(LHALH1Encoder),
	OUTPUT_BUFFER_SIZE,
	RING_BUFFER_SIZE
};
