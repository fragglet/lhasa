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

#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "crc16.h"
#include "lha_codec.h"
#include "lha_decoder.h"

// Null decoder, used for -lz4-, -lh0-, -pm0-:
extern LHACodec lha_null_codec;

// LArc compression algorithms:
extern LHACodec lha_lz5_decoder;
extern LHACodec lha_lzs_decoder;

// LHarc compression algorithms:
extern LHACodec lha_lh1_decoder;
extern LHACodec lha_lh4_decoder;
extern LHACodec lha_lh5_decoder;
extern LHACodec lha_lh6_decoder;
extern LHACodec lha_lh7_decoder;
extern LHACodec lha_lhx_decoder;
extern LHACodec lha_lk7_decoder;

// PMarc compression algorithms:
extern LHACodec lha_pm1_decoder;
extern LHACodec lha_pm2_decoder;

static struct {
	char *name;
	LHACodec *codec;
} decoders[] = {
	{ "-lz4-", &lha_null_codec },
	{ "-lz5-", &lha_lz5_decoder },
	{ "-lzs-", &lha_lzs_decoder },
	{ "-lh0-", &lha_null_codec },
	{ "-lh1-", &lha_lh1_decoder },
	{ "-lh4-", &lha_lh4_decoder },
	{ "-lh5-", &lha_lh5_decoder },
	{ "-lh6-", &lha_lh6_decoder },
	{ "-lh7-", &lha_lh7_decoder },
	{ "-lhx-", &lha_lhx_decoder },
	{ "-lk7-", &lha_lk7_decoder },
	{ "-pm0-", &lha_null_codec },
	{ "-pm1-", &lha_pm1_decoder },
	{ "-pm2-", &lha_pm2_decoder },
};

LHADecoder *lha_decoder_new(LHACodec *codec,
                            LHACodecCallback callback,
                            void *callback_data,
                            size_t stream_length)
{
	LHADecoder *decoder;
	void *state;

	// Space is allocated together: the LHADecoder structure,
	// then the private data area used by the algorithm,
	// followed by the output buffer,

	decoder = calloc(1, sizeof(LHADecoder) + codec->state_size
	                        + codec->max_read);

	if (decoder == NULL) {
		return NULL;
	}

	decoder->codec = codec;
	decoder->progress_callback = NULL;
	decoder->last_block = UINT_MAX;
	decoder->outbuf_pos = 0;
	decoder->outbuf_len = 0;
	decoder->stream_pos = 0;
	decoder->stream_length = stream_length;
	decoder->decoder_failed = 0;
	decoder->crc = 0;

	// Private data area follows the structure.

	state = decoder + 1;
	decoder->outbuf = ((uint8_t *) state) + codec->state_size;

	if (codec->init != NULL
	 && !codec->init(state, callback, callback_data)) {
		free(decoder);
		return NULL;
	}

	return decoder;
}

LHACodec *lha_decoder_for_name(char *name)
{
	unsigned int i;

	for (i = 0; i < sizeof(decoders) / sizeof(*decoders); ++i) {
		if (!strcmp(name, decoders[i].name)) {
			return decoders[i].codec;
		}
	}

	// Unknown?

	return NULL;
}

void lha_decoder_free(LHADecoder *decoder)
{
	if (decoder->codec->free != NULL) {
		decoder->codec->free(decoder + 1);
	}

	free(decoder);
}

// Check if the stream has progressed far enough that the progress callback
// should be invoked again.

static void check_progress_callback(LHADecoder *decoder)
{
	unsigned int block;

	block = (decoder->stream_pos + decoder->codec->block_size - 1)
	      / decoder->codec->block_size;

	// If the stream has advanced by another block, invoke the callback
	// function. Invoke it multiple times if it has advanced by
	// more than one block.

	while (decoder->last_block != block) {
		++decoder->last_block;
		decoder->progress_callback(decoder->last_block,
		                           decoder->total_blocks,
		                           decoder->progress_callback_data);
	}
}

void lha_decoder_monitor(LHADecoder *decoder,
                         LHADecoderProgressCallback callback,
                         void *callback_data)
{
	decoder->progress_callback = callback;
	decoder->progress_callback_data = callback_data;

	decoder->total_blocks
	  = (decoder->stream_length + decoder->codec->block_size - 1)
	  / decoder->codec->block_size;

	check_progress_callback(decoder);
}

size_t lha_decoder_read(LHADecoder *decoder, uint8_t *buf, size_t buf_len)
{
	size_t filled, bytes;

	// When we reach the end of the stream, we must truncate the
	// decompressed data at exactly the right point (stream_length),
	// or we may read a few extra false byte(s) by mistake.
	// Reduce buf_len when we get to the end to limit it to the
	// real number of remaining characters.

	if (decoder->stream_pos + buf_len > decoder->stream_length) {
		buf_len = decoder->stream_length - decoder->stream_pos;
	}

	// Try to fill up the buffer that has been passed with as much
	// data as possible. Each call to read() will fill up outbuf
	// with some data; this is then copied into buf, with some
	// data left at the end for the next call.

	filled = 0;

	while (filled < buf_len) {

		// Try to empty out some of the output buffer first.

		bytes = decoder->outbuf_len - decoder->outbuf_pos;

		if (buf_len - filled < bytes) {
			bytes = buf_len - filled;
		}

		memcpy(buf + filled, decoder->outbuf + decoder->outbuf_pos,
		       bytes);
		decoder->outbuf_pos += bytes;
		filled += bytes;

		// If we previously encountered a failure reading from
		// the decoder, don't try to call the read function again.

		if (decoder->decoder_failed) {
			break;
		}

		// If outbuf is now empty, we can process another run to
		// re-fill it.

		if (decoder->outbuf_pos >= decoder->outbuf_len) {
			decoder->outbuf_len
			    = decoder->codec->read(decoder + 1,
			                           decoder->outbuf);
			decoder->outbuf_pos = 0;
		}

		// No more data to be read?

		if (decoder->outbuf_len == 0) {
			decoder->decoder_failed = 1;
			break;
		}
	}

	// Update CRC.

	lha_crc16_buf(&decoder->crc, buf, filled);

	// Track stream position.

	decoder->stream_pos += filled;

	// Check progress callback, if one is set:

	if (decoder->progress_callback != NULL) {
		check_progress_callback(decoder);
	}

	return filled;
}

uint16_t lha_decoder_get_crc(LHADecoder *decoder)
{
	return decoder->crc;
}

size_t lha_decoder_get_length(LHADecoder *decoder)
{
	return decoder->stream_pos;
}
