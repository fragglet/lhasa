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

#ifndef LHASA_LHA_READER_H
#define LHASA_LHA_READER_H

#include "lha_input_stream.h"
#include "lha_file_header.h"
#include "lha_decoder.h"

typedef struct _LHAReader LHAReader;

/**
 * Create a new LHA reader to read data from an input stream.
 *
 * @param stream     The input stream to read from.
 * @return           Pointer to an LHAReader structure, or NULL for error.
 */

LHAReader *lha_reader_new(LHAInputStream *stream);

/**
 * Free an LHA reader.
 *
 * @param reader     The LHAReader structure.
 */

void lha_reader_free(LHAReader *reader);

/**
 * Read the header of the next archived file from the input stream.
 *
 * @param reader     The LHAReader structure.
 * @return           Pointer to an LHAFileHeader structure, or NULL if
 *                   an error occurred.  This pointer is only valid until
 *                   the next time that lha_reader_next_file is called.
 */

LHAFileHeader *lha_reader_next_file(LHAReader *reader);

/**
 * Read some of the compressed data for the current archived file.
 *
 * @param reader     The LHAReader structure.
 * @param buf        Pointer to the buffer in which to store the data.
 * @param buf_len    Size of the buffer, in bytes.
 */

size_t lha_reader_read_compressed(LHAReader *reader, void *buf, size_t buf_len);

/**
 * Read some of the data for the current archived file, decompressing
 * as appropriate.
 *
 * @param reader     The LHAReader structure.
 * @param buf        Pointer to the buffer in which to store the data.
 * @param buf_len    Size of the buffer, in bytes.
 */

size_t lha_reader_read(LHAReader *reader, void *buf, size_t buf_len);

/**
 * Decompress the contents of the current archived file, and check
 * that the checksum matches correctly.
 *
 * @param reader         The LHAReader structure.
 * @param callback       Callback function to invoke to monitor progress (or
 *                       NULL if progress does not need to be monitored).
 * @param callback_data  Extra data to pass to the callback function.
 * @return               Non-zero if the checksum matches.
 */

int lha_reader_check(LHAReader *reader,
                     LHADecoderProgressCallback callback,
                     void *callback_data);

/**
 * Extract the contents of the current archived file.
 *
 * @param reader         The LHAReader structure.
 * @param filename       Filename to extract the archived file to, or NULL
 *                       to use the path and filename from the header.
 * @param callback       Callback function to invoke to monitor progress (or
 *                       NULL if progress does not need to be monitored).
 * @param callback_data  Extra data to pass to the callback function.
 * @return               Non-zero for success, or zero for failure (including
 *                       CRC error).
 */

int lha_reader_extract(LHAReader *reader,
                       char *filename,
                       LHADecoderProgressCallback callback,
                       void *callback_data);

#endif /* #ifndef LHASA_LHA_READER_H */

