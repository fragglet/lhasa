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

#ifndef LHASA_LHA_ENCODER_H
#define LHASA_LHA_ENCODER_H

#include "public/lha_decoder.h"

struct _LHAEncoder {

	/** Type of encoder (algorithm) */
	LHACodec *codec;

	/** Output buffer, containing encoded data not yet returned. */
	unsigned int outbuf_pos, outbuf_len;
	uint8_t *outbuf;

	/** Length of input stream */
	size_t instream_length;

	/** If true, the read() function returned zero. */
	unsigned int encoder_failed;

	/** Current CRC of the input stream. */
	uint16_t crc;

	/** Callback to read more input data to compress. */
	LHACodecCallback callback;
	void *callback_data;
};

typedef struct _LHAEncoder LHAEncoder;

#endif /* #ifndef LHASA_LHA_ENCODER_H */
