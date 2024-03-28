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

	return 1;
}

static int read_next_byte(LHALH1Encoder *encoder, uint8_t *result)
{
	size_t cnt;

	if (!encoder->eof
	 && (encoder->read_buffer_pos >= encoder->read_buffer_len
	  || encoder->read_buffer_pos > READ_BUFFER_SIZE / 2)) {
		memmove(encoder->read_buffer,
		        encoder->read_buffer + encoder->read_buffer_pos,
		        encoder->read_buffer_len - encoder->read_buffer_pos);
		encoder->read_buffer_len -= encoder->read_buffer_pos;
		encoder->read_buffer_pos = 0;

		cnt = encoder->callback(
			encoder->read_buffer + encoder->read_buffer_len,
			READ_BUFFER_SIZE - encoder->read_buffer_len,
			encoder->callback_data);
		encoder->read_buffer_len += cnt;
		if (cnt == 0) {
			encoder->eof = 1;
		}
	}

	if (encoder->read_buffer_pos >= encoder->read_buffer_len) {
		return 0;
	}

	*result = encoder->read_buffer[encoder->read_buffer_pos];
	++encoder->read_buffer_pos;
	return 1;
}

static void write_code(LHALH1Encoder *encoder, uint8_t b)
{
	LHALH1State *state = &encoder->state;
	unsigned int node_index, parent_index, bit;
	unsigned int out, bits;

	node_index = state->leaf_nodes[b];

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
	lha_lh1_increment_for_code(state, b);
}

static size_t lha_lh1_encoder_read(void *data, uint8_t *buf)
{
	LHALH1Encoder *encoder = data;
	size_t result = 0, cnt;
	uint8_t b;

	while (result < RING_BUFFER_SIZE) {
		cnt = flush_bytes(&encoder->bit_stream_writer, buf + result,
		                  OUTPUT_BUFFER_SIZE - result);
		result += cnt;

		if (!read_next_byte(encoder, &b)) {
			break;
		}

		// TODO: We currently do not do copies from history at all.
		// This greatly reduces the effectiveness of the encoder,
		// since we are not making full use of the format.
		write_code(encoder, b);
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
