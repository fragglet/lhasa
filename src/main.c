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
#include <time.h>

#include "lha_reader.h"

#include "filter.h"
#include "extract.h"
#include "list.h"

typedef enum
{
	MODE_UNKNOWN,
	MODE_LIST,
	MODE_LIST_VERBOSE,
	MODE_CRC_CHECK,
	MODE_EXTRACT
} ProgramMode;

static void help_page(char *progname)
{
	printf("usage: %s [-]{lv} archive_file\n", progname);

	printf("commands:\n"
	       " l,v List / Verbose List\n"
	       " t   Test file CRC in archive\n"
	       " x,e Extract from archive\n");
	exit(-1);
}

static void do_command(ProgramMode mode, char *filename,
                       char **filters, unsigned int num_filters)
{
	FILE *fstream;
	LHAInputStream *stream;
	LHAReader *reader;
	LHAFilter filter;

	fstream = fopen(filename, "rb");

	if (fstream == NULL) {
		fprintf(stderr, "LHa: Error: %s %s\n",
		                filename, strerror(errno));
		exit(-1);
	}

	stream = lha_input_stream_from_FILE(fstream);
	reader = lha_reader_new(stream);
	lha_filter_init(&filter, reader, filters, num_filters);

	switch (mode) {
		case MODE_LIST:
			list_file_basic(&filter, fstream);
			break;

		case MODE_LIST_VERBOSE:
			list_file_verbose(&filter, fstream);
			break;

		case MODE_CRC_CHECK:
			test_file_crc(&filter);
			break;

		case MODE_EXTRACT:
			extract_archive(&filter);
			break;

		case MODE_UNKNOWN:
			break;
	}

	lha_reader_free(reader);
	lha_input_stream_free(stream);

	fclose(fstream);
}

int main(int argc, char *argv[])
{
	ProgramMode mode;

	if (argc < 3) {
		help_page(argv[0]);
	}

	mode = MODE_UNKNOWN;

	if (!strcmp(argv[1], "l")) {
		mode = MODE_LIST;
	} else if (!strcmp(argv[1], "v")) {
		mode = MODE_LIST_VERBOSE;
	} else if (!strcmp(argv[1], "t")) {
		mode = MODE_CRC_CHECK;
	} else if (!strcmp(argv[1], "e") || !strcmp(argv[1], "x")) {
		mode = MODE_EXTRACT;
	} else {
		help_page(argv[0]);
	}

	do_command(mode, argv[2], argv + 3, argc - 3);

	return 0;
}

