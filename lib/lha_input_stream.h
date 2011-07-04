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


#ifndef LHASA_LHA_INPUT_STREAM_H
#define LHASA_LHA_INPUT_STREAM_H

#include <stdio.h>
#include <inttypes.h>

typedef struct _LHAInputStream LHAInputStream;

/** Structure containing pointers to callback functions to read data from
    the input stream. */

typedef struct {

	/**
	 * Read a block of data into the specified buffer.
	 *
	 * @param handle       Handle pointer.
	 * @param buf          Pointer to buffer in which to store read data.
	 * @param buf_len      Size of buffer, in bytes.
	 * @return             Number of bytes read, or -1 for error.
	 */

	int (*read)(void *handle, void *buf, size_t buf_len);


	/**
	 * Skip the specified number of bytes from the input stream.
	 * This is an optional function.
	 *
	 * @param handle       Handle pointer.
	 * @param bytes        Number of bytes to skip.
	 * @return             Non-zero for success, or zero for failure.
	 */

	int (*skip)(void *handle, size_t bytes);

	/**
	 * Close the input stream.
	 *
	 * @param handle       Handle pointer.
	 */

	void (*close)(void *handle);

} LHAInputStreamType;

/**
 * Create new LHA input stream structure, using a set of generic functions
 * to provide LHA data.
 *
 * @param type         Pointer to @param LHAInputStreamType structure
 *                     containing callback functions to read data.
 * @param handle       Handle pointer to pass to callback functions.
 * @return             Pointer to LHAInputStream or NULL for error.
 */

LHAInputStream *lha_input_stream_new(const LHAInputStreamType *type,
                                     void *handle);

/**
 * Create new LHA input stream reading from the specified filenamme.
 * The file is automatically closed when the input stream is freed.
 *
 * @param filename     Name of the file to read from.
 * @return             Pointer to LHAInputStream or NULL for error.
 */

LHAInputStream *lha_input_stream_from(char *filename);

/**
 * Create new LHA input stream to read from the specified open FILE pointer.
 * The file is not closed when the input stream structure is freed; the
 * calling code must close the file.
 *
 * @param stream       The open file from which to read data.
 * @return             Pointer to LHAInputStream or NULL for error.
 */

LHAInputStream *lha_input_stream_from_FILE(FILE *stream);

/**
 * Free an LHA input stream object.
 *
 * @param stream       The input stream.
 */

void lha_input_stream_free(LHAInputStream *stream);

/**
 * Read a block of data from the LHA stream, of the specified number
 * of bytes.
 *
 * @param stream       The input stream.
 * @param buf          Pointer to buffer in which to store read data.
 * @param buf_len      Size of buffer, in bytes.
 * @return             Non-zero if buffer was filled, or zero if an
 *                     error occurred, or end of file was reached.
 */

int lha_input_stream_read(LHAInputStream *stream, void *buf, size_t buf_len);

/**
 * Read a byte of data from the LHA stream.
 *
 * @param stream       The input stream.
 * @param result       Pointer to variable in which to store the result.
 * @return             Non-zero for success.
 */

int lha_input_stream_read_byte(LHAInputStream *stream, uint8_t *result);

/**
 * Read a 16-bit integer (little endian) from the LHA stream.
 *
 * @param stream       The input stream.
 * @param result       Pointer to variable in which to store the result.
 * @return             Non-zero for success.
 */

int lha_input_stream_read_short(LHAInputStream *stream, uint16_t *result);

/**
 * Read a 32-bit integer (little endian) from the LHA stream.
 *
 * @param stream       The input stream.
 * @param result       Pointer to variable in which to store the result.
 * @return             Non-zero for success.
 */

int lha_input_stream_read_long(LHAInputStream *stream, uint32_t *result);

/**
 * Skip over the specified number of bytes.
 *
 * @param stream       The input stream.
 * @param bytes        Number of bytes to skip.
 * @return             Non-zero for success, zero for failure.
 */

int lha_input_stream_skip(LHAInputStream *stream, size_t bytes);

#endif /* #ifndef LHASA_LHA_INPUT_STREAM_H */

