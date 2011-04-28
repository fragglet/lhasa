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


#ifndef LHASA_LHA_FILE_HEADER_H
#define LHASA_LHA_FILE_HEADER_H

#include "lha_input_stream.h"

typedef struct _LHAFileHeader LHAFileHeader;

struct _LHAFileHeader {
	char compress_method[6];
	size_t compressed_length;
	size_t length;
	char *filename;
	unsigned int timestamp;
	uint16_t crc;
	void *raw_data;
	size_t raw_data_len;
};

/**
 * Read a file header from the input stream.
 *
 * @param stream         The input stream to read from.
 * @return               Pointer to a new LHAFileHeader structure, or NULL
 *                       if an error occurred or a valid header could not
 *                       be read.
 */

LHAFileHeader *lha_file_header_read(LHAInputStream *stream);

/**
 * Free a file header structure.
 *
 * @param header         The file header to free.
 */

void lha_file_header_free(LHAFileHeader *header);

#endif /* #ifndef LHASA_LHA_FILE_HEADER_H */

