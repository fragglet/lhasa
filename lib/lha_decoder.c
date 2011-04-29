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

#include "lha_decoder.h"

#include "lha_null_decoder.h"
#include "lha_lzss_decoder.h"

static struct {
	char *name;
	LHADecoderType *dtype;
} decoders[] = {
	{ "-lz4-", &lha_null_decoder },
	{ "-lz5-", &lha_lzss_decoder }
};

LHADecoder *lha_decoder_new(LHADecoderType *dtype,
                            LHADecoderCallback callback,
			    void *callback_data)
{
	LHADecoder *decoder;

	decoder = malloc(sizeof(LHADecoder) + dtype->extra_size);

	if (decoder == NULL) {
		return NULL;
	}

	decoder->dtype = dtype;
	decoder->callback = callback;
	decoder->callback_data = callback_data;

	if (dtype->init != NULL && !dtype->init(decoder + 1)) {
		free(decoder);
		return NULL;
	}

	return decoder;
}

LHADecoderType *lha_decoder_for_name(char *name)
{
	unsigned int i;

	for (i = 0; i < sizeof(decoders) / sizeof(*decoders); ++i) {
		if (!strcmp(name, decoders[i].name)) {
			return decoders[i].dtype;
		}
	}

	// Unknown?

	return NULL;
}

void lha_decoder_free(LHADecoder *decoder)
{
	if (decoder->dtype->free != NULL) {
		decoder->dtype->free(decoder + 1);
	}

	free(decoder);
}

size_t lha_decoder_read(LHADecoder *decoder, uint8_t *buf, size_t buf_len)
{
	return decoder->dtype->read(decoder + 1, buf, buf_len,
	                            decoder->callback, decoder->callback_data);
}

