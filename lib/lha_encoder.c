/*

Copyright (c) 2024, Simon Howard

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
#include <limits.h>

#include "crc16.h"
#include "lha_codec.h"
#include "lha_encoder.h"

// Null encoder, used for -lz4-, -lh0-, -pm0-:
extern LHACodec lha_null_codec;

static struct {
	char *name;
	LHACodec *codec;
} encoders[] = {
	{ "-lz4-", &lha_null_codec },
	{ "-lh0-", &lha_null_codec },
	{ "-pm0-", &lha_null_codec },
};

// When the codec is initialized for encoding, we do not pass through the
// callback supplied to lha_encoder_new. Instead, we pass this wrapper
// callback that tracks input CRC and stream length.
static size_t read_callback(void *buf, size_t buf_len, void *user_data)
{
	LHAEncoder *encoder = user_data;
	size_t result;

	result = encoder->callback(buf, buf_len, encoder->callback_data);

	// Update CRC.
	lha_crc16_buf(&encoder->crc, buf, result);

	// Track input length.
	encoder->instream_length += result;

	return result;
}

LHAEncoder *lha_encoder_new(LHACodec *codec,
                            LHACodecCallback callback,
                            void *callback_data)
{
	LHAEncoder *encoder;
	void *state;

	// Space is allocated together: the LHAEncoder structure,
	// then the private data area used by the algorithm,
	// followed by the output buffer,
	encoder = calloc(1, sizeof(LHAEncoder) + codec->state_size
	                        + codec->max_read);

	if (encoder == NULL) {
		return NULL;
	}

	encoder->codec = codec;
	encoder->outbuf_pos = 0;
	encoder->outbuf_len = 0;
	encoder->instream_length = 0;
	encoder->encoder_failed = 0;
	encoder->crc = 0;
	encoder->callback = callback;
	encoder->callback_data = callback_data;

	// Private data area follows the structure.
	state = encoder + 1;
	encoder->outbuf = ((uint8_t *) state) + codec->state_size;

	if (codec->init != NULL
	 && !codec->init(state, read_callback, encoder)) {
		free(encoder);
		return NULL;
	}

	return encoder;
}

LHACodec *lha_encoder_for_name(char *name)
{
	unsigned int i;

	for (i = 0; i < sizeof(encoders) / sizeof(*encoders); ++i) {
		if (!strcmp(name, encoders[i].name)) {
			return encoders[i].codec;
		}
	}

	// Unknown?

	return NULL;
}

void lha_encoder_free(LHAEncoder *encoder)
{
	if (encoder->codec->free != NULL) {
		encoder->codec->free(encoder + 1);
	}

	free(encoder);
}

size_t lha_encoder_read(LHAEncoder *encoder, uint8_t *buf, size_t buf_len)
{
	size_t filled, bytes;

	// Try to fill up the buffer that has been passed with as much
	// data as possible. Each call to read() will fill up outbuf
	// with some data; this is then copied into buf, with some
	// data left at the end for the next call.
	filled = 0;
	while (filled < buf_len) {

		// Try to empty out some of the output buffer first.
		bytes = encoder->outbuf_len - encoder->outbuf_pos;

		if (buf_len - filled < bytes) {
			bytes = buf_len - filled;
		}

		memcpy(buf + filled, encoder->outbuf + encoder->outbuf_pos,
		       bytes);
		encoder->outbuf_pos += bytes;
		filled += bytes;

		// If we previously encountered a failure reading from
		// the encoder, don't try to call the read function again.
		if (encoder->encoder_failed) {
			break;
		}

		// If outbuf is now empty, we can process another run to
		// re-fill it.
		if (encoder->outbuf_pos >= encoder->outbuf_len) {
			encoder->outbuf_len
			    = encoder->codec->read(encoder + 1,
			                           encoder->outbuf);
			encoder->outbuf_pos = 0;
		}

		// No more data to be read?

		if (encoder->outbuf_len == 0) {
			encoder->encoder_failed = 1;
			break;
		}
	}

	return filled;
}

uint16_t lha_encoder_get_crc(LHAEncoder *encoder)
{
	return encoder->crc;
}

size_t lha_encoder_get_length(LHAEncoder *encoder)
{
	return encoder->instream_length;
}
