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

#ifndef LHASA_LHA_DECODER_H
#define LHASA_LHA_DECODER_H

#include <stdlib.h>
#include <inttypes.h>

typedef struct _LHADecoder LHADecoder;
typedef struct _LHADecoderType LHADecoderType;

/**
 * Callback function invoked when a decoder wants to read more compressed
 * data.
 *
 * @param buf        Pointer to the buffer in which to store the data.
 * @param buf_len    Size of the buffer, in bytes.
 * @param user_data  Extra pointer to pass to the decoder.
 * @return           Number of bytes read.
 */

typedef size_t (*LHADecoderCallback)(void *buf, size_t buf_len, void *user_data);

struct _LHADecoderType {

	/**
	 * Callback function to initialize the decoder.
	 *
	 * @param extra_data     Pointer to the extra data area allocated for
	 *                       the decoder.
	 * @return               Non-zero for success.
	 */
	
	int (*init)(void *extra_data);

	/**
	 * Callback function to free the decoder.
	 *
	 * @param extra_data     Pointer to the extra data area allocated for
	 *                       the decoder.
	 */
	
	void (*free)(void *extra_data);

	/**
	 * Callback function to read (ie. decompress) data from the
	 * decoder.
	 *
	 * @param extra_data     Pointer to the decoder's custom data.
	 * @param buf            Pointer to the buffer in which to store
	 *                       the decompressed data.
	 * @param buf_len        Size of the buffer, in bytes.
	 * @param callback       Callback function to invoke to read more
	 *                       compressed data.
	 * @param callback_data  Extra pointer to pass to the callback.
	 * @return               Number of bytes decompressed.
	 */

	size_t (*read)(void *extra_data, uint8_t *buf, size_t buf_len,
	               LHADecoderCallback callback, void *callback_data);

	/** Number of bytes of extra data to allocate for the decoder. */

	size_t extra_size;
};

struct _LHADecoder {
	LHADecoderType *dtype;
	LHADecoderCallback callback;
	void *callback_data;
};

/**
 * Allocate a new decoder for the specified type.
 *
 * @param dtype          The decoder type.
 * @param callback       Callback function for the decoder to call to read
 *                       more compressed data.
 * @param callback_data  Extra data to pass to the callback function.
 * @return               Pointer to the new decoder, or NULL for failure.
 */

LHADecoder *lha_decoder_new(LHADecoderType *dtype,
                            LHADecoderCallback callback,
			    void *callback_data);

/**
 * Get the decoder type for the specified name.
 *
 * @param name           The decoder type.
 * @return               Pointer to the decoder type, or NULL for unknown
 *                       decoder type.
 */

LHADecoderType *lha_decoder_for_name(char *name);

/**
 * Free a decoder.
 *
 * @param decoder        The decoder to free.
 */

void lha_decoder_free(LHADecoder *decoder);

/**
 * Decode (decompress) more data.
 *
 * @param decoder        The decoder.
 * @param buf            Pointer to buffer to store decompressed data.
 * @param buf_len        Size of the buffer, in bytes.
 * @return               Number of bytes decompressed.
 */

size_t lha_decoder_read(LHADecoder *decoder, uint8_t *buf, size_t buf_len);

#endif /* #ifndef LHASA_LHA_DECODER_H */

