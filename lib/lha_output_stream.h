/*

Copyright (c) 2012, Simon Howard

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

#ifndef LHASA_LHA_OUTPUT_STREAM_H
#define LHASA_LHA_OUTPUT_STREAM_H

#include <stdlib.h>
#include <inttypes.h>
#include "public/lha_output_stream.h"

/**
 * Write a block of data to the output stream, of the specified number
 * of bytes.
 *
 * @param stream       The output stream.
 * @param buf          Pointer to buffer containing data to write.
 * @param buf_len      Number of bytes to write.
 * @return             Non-zero if the data was written, or zero if
 *                     an error occurred.
 */

int lha_output_stream_write(LHAOutputStream *stream,
                            void *buf, size_t buf_len);

/**
 * Get the current position within the output stream.
 *
 * @param stream       The output stream.
 * @return             Current position in output stream, as byte offset
 *                     from the start of the file.
 */

off_t lha_output_stream_tell(LHAOutputStream *stream);

/**
 * Set the current position within the output stream.
 *
 * @param stream       The output stream.
 * @param position     The position to seek to, as a byte offset from the
 *                     start of the file.
 * @return             Non-zero for success, zero for failure.
 */

int lha_output_stream_seek(LHAOutputStream *stream, off_t position);

#endif /* #ifndef LHASA_LHA_OUTPUT_STREAM_H */

