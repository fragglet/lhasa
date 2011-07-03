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

#include "lha_reader.h"

// Maximum number of dots in progress output:

#define MAX_PROGRESS_LEN 58

static void print_filename(LHAFileHeader *header, char *status)
{
	printf("\r%s%s\t- %s  ",
	       header->path != NULL ? header->path : "",
	       header->filename, status);
}

// Callback function invoked during CRC check.

static void crc_check_callback(unsigned int block,
                               unsigned int num_blocks,
                               void *data)
{
	LHAFileHeader *header = data;
	unsigned int factor;
	unsigned int i;

	// Scale factor for blocks, so that the line is never too long.  When
	// MAX_PROGRESS_LEN is exceeded, the length is halved (factor=2), then
	// progressively larger scale factors are applied.

	factor = 1 + (num_blocks / MAX_PROGRESS_LEN);
	num_blocks = (num_blocks + factor - 1) / factor;

	// First call to specify number of blocks?

	if (block == 0) {
		print_filename(header, "Testing  :");

		for (i = 0; i < num_blocks; ++i) {
			printf(".");
		}

		print_filename(header, "Testing  :");
	} else if (((block + factor - 1) % factor) == 0) {
		// Otherwise, signal progress:

		printf("o");
	}

	fflush(stdout);
}

// Perform CRC check of an archived file.

static void test_archived_file_crc(LHAReader *reader,
                                   LHAFileHeader *header)
{
	int success;

	success = lha_reader_check(reader, crc_check_callback, header);

	if (success) {
		print_filename(header, "Tested");
		printf("\n");
	} else {
		print_filename(header, "CRC error");
		printf("\n");

		// TODO: Exit with error
	}
}

// lha -t command.

void test_file_crc(LHAReader *reader)
{
	for (;;) {
		LHAFileHeader *header;

		header = lha_reader_next_file(reader);

		if (header == NULL) {
			break;
		}

		test_archived_file_crc(reader, header);
	}
}

