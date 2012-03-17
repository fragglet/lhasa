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

#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#include "filter.h"

// Maximum number of dots in progress output:

#define MAX_PROGRESS_LEN 58

typedef struct {
	int invoked;
	LHAFileHeader *header;
	char *operation;
} ProgressCallbackData;

static void print_filename(LHAFileHeader *header, char *status)
{
	printf("\r%s%s\t- %s  ",
	       header->path != NULL ? header->path : "",
	       header->filename, status);
}

// Callback function invoked during decompression progress.

static void progress_callback(unsigned int block,
                              unsigned int num_blocks,
                              void *data)
{
	ProgressCallbackData *progress = data;
	unsigned int factor;
	unsigned int i;

	progress->invoked = 1;

	// Scale factor for blocks, so that the line is never too long.  When
	// MAX_PROGRESS_LEN is exceeded, the length is halved (factor=2), then
	// progressively larger scale factors are applied.

	factor = 1 + (num_blocks / MAX_PROGRESS_LEN);
	num_blocks = (num_blocks + factor - 1) / factor;

	// First call to specify number of blocks?

	if (block == 0) {
		print_filename(progress->header, progress->operation);

		for (i = 0; i < num_blocks; ++i) {
			printf(".");
		}

		print_filename(progress->header, progress->operation);
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
	ProgressCallbackData progress;
	int success;

	progress.invoked = 0;
	progress.operation = "Testing  :";
	progress.header = header;

	success = lha_reader_check(reader, progress_callback, &progress);

	if (progress.invoked) {
		if (success) {
			print_filename(header, "Tested");
			printf("\n");
		} else {
			print_filename(header, "CRC error");
			printf("\n");

			// TODO: Exit with error
		}
	}
}

// Check that the specified directory exists, and create it if it
// does not.

static int check_parent_directory(char *path)
{
	struct stat fs;

	if (stat(path, &fs) == 0) {
		// Check it's a directory:

		if ((fs.st_mode & S_IFDIR) == 0) {
			fprintf(stderr, "Parent path %s is not a directory!\n",
			        path);
			return 0;
		}
	} else if (errno == ENOENT) {
		// Create the missing directory:

		if (mkdir(path, 0755) != 0) {
			fprintf(stderr,
			        "Failed to create parent directory %s: %s\n",
			        path, strerror(errno));
			return 0;
		}
	} else {
		fprintf(stderr, "Failed to stat %s: %s\n",
		        path, strerror(errno));
		return 0;
	}

	return 1;
}

// Given a directory, ensure that it and all its parent directories
// exist.

static int make_parent_directories(char *path)
{
	int result;
	char *p;

	result = 1;
	path = strdup(path);

	// Iterate through the string, finding each path separator. At
	// each place, temporarily chop off the end of the path to get
	// each parent directory in turn.

	p = path;

	do {
		p = strchr(p, '/');

		// Terminate string here.

		if (p != NULL) {
			*p = '\0';
		}

		if (!check_parent_directory(path)) {
			result = 0;
			break;
		}

		// Restore path separator and advance to next parent dir.

		if (p != NULL) {
			*p = '/';
			++p;
		}
	} while (p != NULL);

	free(path);

	return result;
}

// Extract an archived file.

static void extract_archived_file(LHAReader *reader,
                                  LHAFileHeader *header)
{
	ProgressCallbackData progress;
	int success;

	// Create parent directories for file:

	if (header->path != NULL && !make_parent_directories(header->path)) {
		return;
	}

	progress.invoked = 0;
	progress.operation = "Melting  :";
	progress.header = header;

	success = lha_reader_extract(reader, NULL, progress_callback, &progress);

	if (progress.invoked) {
		if (success) {
			print_filename(header, "Melted");
			printf("\n");
		} else {
			print_filename(header, "Failure");
			printf("\n");

			// TODO: Exit with error
		}
	} else {
		if (!success) {
			// TODO - failure to extract directory
		}
	}
}

// lha -t command.

void test_file_crc(LHAFilter *filter)
{
	for (;;) {
		LHAFileHeader *header;

		header = lha_filter_next_file(filter);

		if (header == NULL) {
			break;
		}

		test_archived_file_crc(filter->reader, header);
	}
}

// lha -e / -x

void extract_archive(LHAFilter *filter)
{
	for (;;) {
		LHAFileHeader *header;

		header = lha_filter_next_file(filter);

		if (header == NULL) {
			break;
		}

		extract_archived_file(filter->reader, header);
	}
}

