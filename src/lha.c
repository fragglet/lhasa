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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "lha_reader.h"

static void help_page(char *progname)
{
	printf("usage: %s [-]{lv} archive_file\n", progname);

	printf("commands:\n"
	       " l,v List / Verbose List\n");
	exit(-1);
}

static float compression_percent(unsigned int compressed,
                                 unsigned int uncompressed)
{
	float factor;

	if (uncompressed > 0) {
		factor = (float) compressed / uncompressed;
	} else {
		factor = 1.0;
	}

	return factor * 100.0;
}

static void list_file_contents(char *filename)
{
	FILE *fstream;
	LHAInputStream *stream;
	LHAReader *reader;
	unsigned int total_files;
	unsigned int length, compressed_length;

	fstream = fopen(filename, "rb");

	if (fstream == NULL) {
		fprintf(stderr, "LHa: Error: %s %s\n",
		                filename, strerror(errno));
		exit(-1);
	}

	printf(" PERMSSN    UID  GID      SIZE  RATIO     STAMP           NAME\n"
	       "---------- ----------- ------- ------ ------------ --------------------\n");

	stream = lha_input_stream_new(fstream);
	reader = lha_reader_new(stream);
	total_files = 0;
	length = 0;
	compressed_length = 0;

	for (;;) {
		LHAFileHeader *header;

		header = lha_reader_next_file(reader);

		if (header == NULL) {
			break;
		}

		printf("[generic]             ");
		printf("%8i", header->length);
		printf("%6.1f%%", compression_percent(header->compressed_length,
		                                      header->length));
		printf("  -- TODO --  ");
		printf("%s\n", header->filename);

		++total_files;
		length += header->length;
		compressed_length += header->compressed_length;
	}

	lha_reader_free(reader);
	lha_input_stream_free(stream);

	printf("---------- ----------- ------- ------ ------------ --------------------\n");
	printf(" Total ");
	printf("%9i files", total_files);
	printf("%8i", length);
	printf("%6.1f%%", compression_percent(compressed_length, length));
	printf("  -- TODO -- \n");

	fclose(fstream);
}

int main(int argc, char *argv[])
{
	if (argc < 3) {
		help_page(argv[0]);
	}

	if (!strcmp(argv[1], "l")) {
		list_file_contents(argv[2]);
	} else {
		help_page(argv[0]);
	}
}

