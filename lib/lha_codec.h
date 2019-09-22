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

#ifndef LHASA_LHA_CODEC_H
#define LHASA_LHA_CODEC_H

#include <inttypes.h>
#include "public/lha_codec.h"

struct _LHACodec {

	/**
	 * Callback function to initialize the codec.
	 *
	 * @param state          Pointer to the state area allocated for
	 *                       the codec.
	 * @param callback       Callback function to invoke to read more
	 *                       compressed data.
	 * @param callback_data  Extra pointer to pass to the callback.
	 * @return               Non-zero for success.
	 */

	int (*init)(void *state, LHACodecCallback callback,
	            void *callback_data);

	/**
	 * Callback function to free the codec state.
	 *
	 * @param state     Pointer to the state area allocated for
	 *                  the codec.
	 */

	void (*free)(void *state);

	/**
	 * Callback function to read data from the codec.
	 *
	 * @param state          Pointer to the codec's state.
	 * @param buf            Pointer to the buffer in which to store
	 *                       the output data.  The buffer is at least
	 *                       'max_read' bytes in size.
	 * @return               Number of bytes decompressed.
	 */

	size_t (*read)(void *state, uint8_t *buf);

	/** Number of bytes of state to allocate for the codec. */

	size_t state_size;

	/** Maximum number of bytes that might be put into the buffer by
	    a single call to read() */

	size_t max_read;

	/** Block size. Used for calculating number of blocks for
	    progress bar. */

	size_t block_size;
};

#endif /* #ifndef LHASA_LHA_CODEC_H */

