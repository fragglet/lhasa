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


#ifndef LHASA_PUBLIC_LHA_OUTPUT_STREAM_H
#define LHASA_PUBLIC_LHA_OUTPUT_STREAM_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file lha_output_stream.h
 *
 * @brief LHA output stream structure.
 *
 * This file defines the functions relating to the @ref LHAOutputStream
 * structure, used to write an LZH file.
 */

/**
 * Opaque structure, representing an output stream used to write an
 * LZH file.
 */

typedef struct _LHAOutputStream LHAOutputStream;

/**
 * Structure containing pointers to callback functions to write data to
 * the output stream. All functions must be implemented.
 */

typedef struct {

	/**
	 * Write a block of data from the specified buffer.
	 *
	 * @param handle       Handle pointer.
	 * @param buf          Pointer to buffer containing the data.
	 * @param buf_len      Number of bytes to write.
	 * @return             Number of bytes written, or -1 for error.
	 */

	int (*write)(void *handle, void *buf, size_t buf_len);


	/**
	 * Get the current position within the file.
	 *
	 * @param handle       Handle pointer.
	 * @return             Position within the file (byte offset from
	 *                     the start of file).
	 */

	off_t (*tell)(void *handle);

	/**
	 * Set the current position within the file.
	 *
	 * @param handle       Handle pointer.
	 * @param position     New position in the file.
	 * @return             Non-zero for success, zero for failure.
	 */

	int (*seek)(void *handle, off_t position);

	/**
	 * Close the output stream.
	 *
	 * @param handle       Handle pointer.
	 */

	void (*close)(void *handle);

} LHAOutputStreamType;

/**
 * Create new @ref LHAOutputStream structure, using a set of generic functions
 * to access the file.
 *
 * @param type         Pointer to a @ref LHAOutputStreamType structure
 *                     containing callback functions to write data.
 * @param handle       Handle pointer to be passed to callback functions.
 * @return             Pointer to a new @ref LHAOutputStream or NULL for error.
 */

LHAOutputStream *lha_output_stream_new(const LHAOutputStreamType *type,
                                       void *handle);

/**
 * Create new @ref LHAOutputStream, writing to the specified filename.
 * If the file already exists, it will be overwritten. The file is
 * automatically closed when the output stream is freed.
 *
 * @param filename     Name of the file to write to.
 * @return             Pointer to a new @ref LHAOutputStream or NULL for error.
 */

LHAOutputStream *lha_output_stream_to(char *filename);

/**
 * Create new @ref LHAOutputStream, to write from an already-open FILE
 * pointer. The FILE is not closed when the output stream is freed; the
 * calling code must close it.
 *
 * @param stream       The open FILE structure to write data to.
 * @return             Pointer to a new @ref LHAOutputStream or NULL for error.
 */

LHAOutputStream *lha_output_stream_to_FILE(FILE *stream);

/**
 * Free an @ref LHAOutputStream structure.
 *
 * @param stream       The input stream.
 */

void lha_output_stream_free(LHAOutputStream *stream);

#ifdef __cplusplus
}
#endif

#endif /* #ifndef LHASA_PUBLIC_LHA_OUTPUT_STREAM_H */

