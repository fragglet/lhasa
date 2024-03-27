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

// TODO: Much of the text below should be in a public header file.

/**
 * Opaque type representing an instance of an encoder.
 *
 * An encoder is used to compress a stream of data. Instantiated using the @ref
 * lha_encoder_new function and freed using the @ref lha_encoder_free function.
 */
typedef struct _LHAEncoder LHAEncoder;

/**
 * Allocate a new encoder for the specified compression type.
 *
 * @param codec          Codec for compressing data, as returned by
 *                       @ref lha_encoder_for_name.
 * @param callback       Callback function for the encoder to call to read more
 *                       data to compress.
 * @param callback_data  Extra data to pass to the callback function.
 * @return               Pointer to the new encoder, or NULL for failure.
 */
LHAEncoder *lha_encoder_new(LHACodec *codec,
                            LHACodecCallback callback,
                            void *callback_data);

/**
 * Get the encoder type for the specified name.
 * @param name        String identifying the compression scheme, for example
 *                    "-lh1-".
 * @return            Pointer to the encoder type, or NULL if there is no
 *                    encoder type with that name.
 */
LHACodec *lha_encoder_for_name(char *name);

/**
 * Free an encoder.
 *
 * @param encoder      The encoder.
 */
void lha_encoder_free(LHAEncoder *encoder);

/**
 * Encode (compress) more data.
 *
 * @param encoder       The encoder.
 * @param buf           Pointer to buffer to store compressed data.
 * @param buf_len       Size of the buffer in bytes.
 * @return              Number of compressed bytes read.
 */
size_t lha_encoder_read(LHAEncoder *encoder, uint8_t *buf, size_t buf_len);

/**
 * Get the current 16-bit CRC of the uncompressed data.
 *
 * This should be called at the end of compression and stored in the file
 * header.
 *
 * @param encoder       The encoder.
 * @return              16-bit CRC of the data read for compression so far.
 */
uint16_t lha_encoder_get_crc(LHAEncoder *encoder);

/**
 * Get the count of the number of uncompressed bytes read.
 *
 * @param encoder       The encoder.
 * @return              The number of bytes read for compression so far.
 */
size_t lha_encoder_get_length(LHAEncoder *encoder);

#endif /* #ifndef LHASA_LHA_ENCODER_H */
