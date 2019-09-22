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

#ifndef LHASA_PUBLIC_LHA_CODEC_H
#define LHASA_PUBLIC_LHA_CODEC_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Opaque type representing a type of decoder or encoder.
 *
 * This is an implementation of the compression or decompression code for
 * one of the algorithms used in LZH archive files. Pointers to these
 * structures are retrieved by using the @ref lha_decoder_for_name
 * or @ref lha_encoder_for_name functions.
 */

typedef struct _LHACodec LHACodec;

/**
 * Callback function invoked when a codec wants to read more input data.
 *
 * @param buf        Pointer to the buffer in which to store the data.
 * @param buf_len    Size of the buffer, in bytes.
 * @param user_data  Extra pointer for the codec to pass.
 * @return           Number of bytes read.
 */

typedef size_t (*LHACodecCallback)(void *buf, size_t buf_len,
                                   void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* #ifndef LHASA_PUBLIC_LHA_CODEC_H */

