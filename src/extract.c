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

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

#include "extract.h"

// Maximum number of dots in progress output:

#define MAX_PROGRESS_LEN 58

typedef struct {
	int invoked;
	LHAFileHeader *header;
	LHAOptions *options;
	char *operation;
} ProgressCallbackData;

static void print_filename(LHAFileHeader *header, int use_path, char *status)
{
	printf("\r%s%s\t- %s  ",
	       use_path && header->path != NULL ? header->path : "",
	       header->filename, status);
}

static void print_filename_brief(LHAFileHeader *header, int use_path)
{
	printf("\r%s%s :",
	       use_path && header->path != NULL ? header->path : "",
	       header->filename);
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

	// If the quiet mode options are specified, print a limited amount
	// of information without a progress bar (level 1) or no message
	// at all (level 2).

	if (progress->options->quiet >= 2) {
		return;
	} else if (progress->options->quiet == 1) {
		if (block == 0) {
			print_filename_brief(progress->header,
			                     progress->options->use_path);
			fflush(stdout);
		}

		return;
	}

	// Scale factor for blocks, so that the line is never too long.  When
	// MAX_PROGRESS_LEN is exceeded, the length is halved (factor=2), then
	// progressively larger scale factors are applied.

	factor = 1 + (num_blocks / MAX_PROGRESS_LEN);
	num_blocks = (num_blocks + factor - 1) / factor;

	// First call to specify number of blocks?

	if (block == 0) {
		print_filename(progress->header,
		               progress->options->use_path,
		               progress->operation);

		for (i = 0; i < num_blocks; ++i) {
			printf(".");
		}

		print_filename(progress->header,
		               progress->options->use_path,
		               progress->operation);
	} else if (((block + factor - 1) % factor) == 0) {
		// Otherwise, signal progress:

		printf("o");
	}

	fflush(stdout);
}

// Perform CRC check of an archived file.

static void test_archived_file_crc(LHAReader *reader,
                                   LHAFileHeader *header,
                                   LHAOptions *options)
{
	ProgressCallbackData progress;
	int success;

	progress.invoked = 0;
	progress.operation = "Testing  :";
	progress.options = options;
	progress.header = header;

	success = lha_reader_check(reader, progress_callback, &progress);

	if (progress.invoked && options->quiet < 2) {
		if (success) {
			print_filename(header, options->use_path, "Tested");
			printf("\n");
		} else {
			print_filename(header, options->use_path, "CRC error");
			printf("\n");
		}
	}

	if (!success) {
		// TODO: Exit with error
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

// Prompt the user with a message, and return the first character of
// the typed response.

static char prompt_user(char *message)
{
	char result;
	int c;

	printf("%s", message);
	fflush(stdout);

	// Read characters until a newline is found, saving the first
	// character entered.

	result = 0;

	do {
		c = getchar();

		if (c < 0) {
			exit(-1);
		}

		if (result == 0) {
			result = c;
		}
	} while (c != '\n');

	return result;
}

static int confirm_file_overwrite(LHAFileHeader *header,
                                  char *fullpath,
                                  LHAOptions *options)
{
	char response;

	switch (options->overwrite_policy) {
		case LHA_OVERWRITE_PROMPT:
			break;
		case LHA_OVERWRITE_SKIP:
			return 0;
		case LHA_OVERWRITE_ALL:
			return 1;
	}

	for (;;) {
		printf("%s ", fullpath);

		response = prompt_user("OverWrite ?(Yes/[No]/All/Skip) ");

		switch (tolower(response)) {
			case 'y':
				return 1;
			case 'n':
			case '\n':
				return 0;
			case 'a':
				options->overwrite_policy = LHA_OVERWRITE_ALL;
				return 1;
			case 's':
				options->overwrite_policy = LHA_OVERWRITE_SKIP;
				return 0;
			default:
				break;
		}
	}

	return 0;
}

// Check the specified file header. If the file already exists, prompt
// for whether it should be overwritten. Returns true if the file
// should continue to be extracted.

static int check_file_overwrite(LHAFileHeader *header, LHAOptions *options)
{
	struct stat statbuf;
	char *fullpath;
	int result;

	if (options->use_path && header->path != NULL) {
		fullpath = malloc(strlen(header->path)
		                  + strlen(header->filename) + 1);
		strcpy(fullpath, header->path);
		strcat(fullpath, header->filename);
	} else {
		fullpath = header->filename;
	}

	if (stat(fullpath, &statbuf) == 0) {
		// File already exists. Confirm overwrite.

		result = confirm_file_overwrite(header, fullpath, options);
	} else {
		// TODO: Check errno, continue with extract when
		// file not found; otherwise print an error.
		result = 1;
	}

	if (options->use_path && header->path != NULL) {
		free(fullpath);
	}

	return result;
}

// Extract an archived file.

static void extract_archived_file(LHAReader *reader,
                                  LHAFileHeader *header,
                                  LHAOptions *options)
{
	ProgressCallbackData progress;
	char *filename;
	int success;

	if (strcmp(header->compress_method, LHA_COMPRESS_TYPE_DIR) != 0
	 && !check_file_overwrite(header, options)) {
		return;
	}

	// Create parent directories for file:

	if (options->use_path && header->path != NULL) {
		if (!make_parent_directories(header->path)) {
			return;
		}
	}

	progress.invoked = 0;
	progress.operation = "Melting  :";
	progress.options = options;
	progress.header = header;

	if (options->use_path) {
		filename = NULL;
	} else {
		filename = header->filename;
	}

	success = lha_reader_extract(reader, filename,
	                             progress_callback, &progress);

	if (progress.invoked && options->quiet < 2) {
		if (success) {
			print_filename(header, options->use_path, "Melted");
			printf("\n");
		} else {
			print_filename(header, options->use_path, "Failure");
			printf("\n");
		}
	}

	if (!success) {
		// TODO: Exit with error
	}
}

// lha -t command.

void test_file_crc(LHAFilter *filter, LHAOptions *options)
{
	for (;;) {
		LHAFileHeader *header;

		header = lha_filter_next_file(filter);

		if (header == NULL) {
			break;
		}

		test_archived_file_crc(filter->reader, header, options);
	}
}

// lha -e / -x

void extract_archive(LHAFilter *filter, LHAOptions *options)
{
	for (;;) {
		LHAFileHeader *header;

		header = lha_filter_next_file(filter);

		if (header == NULL) {
			break;
		}

		extract_archived_file(filter->reader, header, options);
	}
}

