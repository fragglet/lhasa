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

// "Headroom" space to allocate in the output buffer. We can always use
// `lha_encoder_fill` to fill the buffer up to this size.
#define OUTBUF_HEADROOM (4 * 1024)

// Null encoder, used for -lz4-, -lh0-, -pm0-:
extern LHACodec lha_null_codec;

extern LHACodec lha_lh1_encoder;

static struct {
	char *name;
	LHACodec *codec;
} encoders[] = {
	{ "-lz4-", &lha_null_codec },
	{ "-lh0-", &lha_null_codec },
	{ "-lh1-", &lha_lh1_encoder },
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
	size_t buf_len = OUTBUF_HEADROOM + codec->max_read;
	void *state;

	// Space is allocated together: the LHAEncoder structure,
	// then the private data area used by the algorithm,
	// followed by the output buffer,
	encoder = calloc(1, sizeof(LHAEncoder) + codec->state_size
	                        + buf_len);

	if (encoder == NULL) {
		return NULL;
	}

	encoder->codec = codec;
	encoder->outbuf_pos = 0;
	encoder->outbuf_len = 0;
	encoder->outbuf_alloced = buf_len;
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

void lha_encoder_fill(LHAEncoder *encoder, size_t min_size)
{
	size_t max_fill = encoder->outbuf_alloced - encoder->codec->max_read;
	size_t nbytes;

	while (!encoder->encoder_failed
	    && encoder->outbuf_len < min_size
	    && encoder->outbuf_len <= max_fill) {
		nbytes = encoder->codec->read(
			encoder + 1, encoder->outbuf + encoder->outbuf_len);

		// No more data to be read?
		if (nbytes == 0) {
			encoder->encoder_failed = 1;
		}

		encoder->outbuf_len += nbytes;
	}
}

size_t lha_encoder_read(LHAEncoder *encoder, uint8_t *buf, size_t buf_len)
{
	size_t result = 0, nbytes;

	do {
		if (encoder->outbuf_pos >= encoder->outbuf_len) {
			encoder->outbuf_pos = 0;
			encoder->outbuf_len = 0;
			lha_encoder_fill(encoder, buf_len);
		}

		nbytes = encoder->outbuf_len - encoder->outbuf_pos;
		nbytes = nbytes < buf_len ? nbytes : buf_len;
		memcpy(buf, encoder->outbuf + encoder->outbuf_pos, nbytes);
		encoder->outbuf_pos += nbytes;

		result += nbytes;
		buf += nbytes;
		buf_len -= nbytes;
	} while (nbytes > 0);

	return result;
}

uint16_t lha_encoder_get_crc(LHAEncoder *encoder)
{
	return encoder->crc;
}

uint64_t lha_encoder_get_length(LHAEncoder *encoder)
{
	return encoder->instream_length;
}
