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

typedef struct
{
	unsigned int num_files;
	unsigned int compressed_length;
	unsigned int length;
} FileStatistics;

typedef struct
{
	char *name;
	void (*handler)(LHAFileHeader *header);
	void (*footer)(FileStatistics *stats);
} ListColumn;

// File permissions

static void permission_column_print(LHAFileHeader *header)
{
	printf("[generic] ");
}

static void permission_column_footer(FileStatistics *stats)
{
	printf(" Total    ");
}

static ListColumn permission_column = {
	" PERMSSN  ",
	permission_column_print,
	permission_column_footer
};

// Unix UID/GID

static void unix_uid_gid_column_print(LHAFileHeader *header)
{
	printf("           ");
}

static void unix_uid_gid_column_footer(FileStatistics *stats)
{
	// The UID/GID column has the total number of files
	// listed below it.

	if (stats->num_files == 1) {
		printf("%6i file", stats->num_files);
	} else {
		printf("%5i files", stats->num_files);
	}
}

static ListColumn unix_uid_gid_column = {
	" UID  GID  ",
	unix_uid_gid_column_print,
	unix_uid_gid_column_footer
};

// Compressed file size

static void packed_column_print(LHAFileHeader *header)
{
	printf("%7i", header->compressed_length);
}

static void packed_column_footer(FileStatistics *stats)
{
	printf("%7i", stats->compressed_length);
}

static ListColumn packed_column = {
	" PACKED",
	packed_column_print,
	packed_column_footer
};

// Uncompressed file size

static void size_column_print(LHAFileHeader *header)
{
	printf("%7i", header->length);
}

static void size_column_footer(FileStatistics *stats)
{
	printf("%7i", stats->length);
}

static ListColumn size_column = {
	"   SIZE",
	size_column_print,
	size_column_footer
};

// Compression ratio

static void ratio_column_print(LHAFileHeader *header)
{
	printf("%5.1f%%", compression_percent(header->compressed_length,
	                                      header->length));
}

static void ratio_column_footer(FileStatistics *stats)
{
	printf("%5.1f%%", compression_percent(stats->compressed_length,
	                                      stats->length));
}

static ListColumn ratio_column = {
	" RATIO",
	ratio_column_print,
	ratio_column_footer
};

// Compression method and CRC checksum

static void method_crc_column_print(LHAFileHeader *header)
{
	printf("%-5s %4x", header->compress_method, header->crc);
}

static ListColumn method_crc_column = {
	"METHOD CRC",
	method_crc_column_print
};

// File timestamp

static void timestamp_column_print(LHAFileHeader *header)
{
	printf(" -- TODO -- ");
};

static void timestamp_column_footer(FileStatistics *stats)
{
	printf(" -- TODO -- ");
};

static ListColumn timestamp_column = {
	"    STAMP   ",
	timestamp_column_print,
	timestamp_column_footer
};

// Filename

static void name_column_print(LHAFileHeader *header)
{
	printf("%s", header->filename);
}

static ListColumn name_column = {
	"       NAME         ",
	name_column_print
};

// Print the names of the column headings at the top of the file list.

static void print_list_headings(ListColumn **columns)
{
	unsigned int i;

	for (i = 0; columns[i] != NULL; ++i) {
		printf("%s ", columns[i]->name);
	}

	printf("\n");
}

// Print separator lines shown at top and bottom of file list.

static void print_list_separators(ListColumn **columns)
{
	unsigned int i, j, len;

	for (i = 0; columns[i] != NULL; ++i) {
		len = strlen(columns[i]->name);

		for (j = 0; j < len; ++j) {
			printf("-");
		}
		printf(" ");
	}

	printf("\n");
}

// Print a row in the list corresponding to a file.

static void print_columns(ListColumn **columns, LHAFileHeader *header)
{
	unsigned int i;

	for (i = 0; columns[i] != NULL; ++i) {
		columns[i]->handler(header);
		printf(" ");
	}

	printf("\n");
}

// Print footer information shown at end of list (overall file stats)

static void print_footers(ListColumn **columns, FileStatistics *stats)
{
	unsigned int i, j, len;

	for (i = 0; columns[i] != NULL; ++i) {
		if (columns[i]->footer != NULL) {
			columns[i]->footer(stats);
		} else {
			len = strlen(columns[i]->name);

			for (j = 0; j < len; ++j) {
				printf(" ");
			}
		}

		printf(" ");
	}

	printf("\n");
}

static void list_file_contents(char *filename, ListColumn **columns)
{
	FILE *fstream;
	LHAInputStream *stream;
	LHAReader *reader;
	FileStatistics stats;

	fstream = fopen(filename, "rb");

	if (fstream == NULL) {
		fprintf(stderr, "LHa: Error: %s %s\n",
		                filename, strerror(errno));
		exit(-1);
	}

	print_list_headings(columns);
	print_list_separators(columns);

	stream = lha_input_stream_new(fstream);
	reader = lha_reader_new(stream);
	stats.num_files = 0;
	stats.length = 0;
	stats.compressed_length = 0;

	for (;;) {
		LHAFileHeader *header;

		header = lha_reader_next_file(reader);

		if (header == NULL) {
			break;
		}

		print_columns(columns, header);

		++stats.num_files;
		stats.length += header->length;
		stats.compressed_length += header->compressed_length;
	}

	lha_reader_free(reader);
	lha_input_stream_free(stream);

	print_list_separators(columns);
	print_footers(columns, &stats);

	fclose(fstream);
}

static ListColumn *normal_column_headers[] = {
	&permission_column,
	&unix_uid_gid_column,
	&size_column,
	&ratio_column,
	&timestamp_column,
	&name_column,
	NULL
};

static ListColumn *verbose_column_headers[] = {
	&permission_column,
	&unix_uid_gid_column,
	&packed_column,
	&size_column,
	&ratio_column,
	&method_crc_column,
	&timestamp_column,
	&name_column,
	NULL
};

int main(int argc, char *argv[])
{
	if (argc < 3) {
		help_page(argv[0]);
	}

	if (!strcmp(argv[1], "l")) {
		list_file_contents(argv[2], normal_column_headers);
	} else if (!strcmp(argv[1], "v")) {
		list_file_contents(argv[2], verbose_column_headers);
	} else {
		help_page(argv[0]);
	}
}

