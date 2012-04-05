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
#include "safe.h"

// Maximum number of dots in progress output:

#define MAX_PROGRESS_LEN 58

typedef struct {
	int invoked;
	LHAFileHeader *header;
	LHAOptions *options;
	char *filename;
	char *operation;
} ProgressCallbackData;

// Given a file header structure, get the path to extract to.
// Returns a newly allocated string that must be free()d.

static char *file_full_path(LHAFileHeader *header, LHAOptions *options)
{
	char *result;
	char *p;

	// Full path, or filename only?

	if (options->use_path && header->path != NULL) {
		result = malloc(strlen(header->path)
		                + strlen(header->filename) + 1);
		strcpy(result, header->path);
		strcat(result, header->filename);
	} else {
		result = strdup(header->filename);
	}

	// If the header contains leading '/' characters, it is an
	// absolute path (eg. /etc/passwd). Strip these leading
	// characters to always generate a relative path, otherwise
	// it can be a security risk - extracting an archive might
	// overwrite arbitrary files on the filesystem.

	p = result;

	while (*p == '/') {
		++p;
	}

	if (p > result) {
		memmove(result, p, strlen(p) + 1);
	}

	return result;
}

static void print_filename(char *filename, char *status)
{
	printf("\r");
	safe_printf("%s", filename);
	printf("\t- %s  ", status);
}

static void print_filename_brief(char *filename)
{
	printf("\r");
	safe_printf("%s :", filename);
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
			print_filename_brief(progress->filename);
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
		print_filename(progress->filename, progress->operation);

		for (i = 0; i < num_blocks; ++i) {
			printf(".");
		}

		print_filename(progress->filename, progress->operation);
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
	char *filename;
	int success;

	filename = file_full_path(header, options);

	if (options->dry_run) {
		if (strcmp(header->compress_method,
		           LHA_COMPRESS_TYPE_DIR) != 0) {
			safe_printf("VERIFY %s\n", filename);
		}

		free(filename);
		return;
	}

	progress.invoked = 0;
	progress.operation = "Testing  :";
	progress.options = options;
	progress.filename = filename;
	progress.header = header;

	success = lha_reader_check(reader, progress_callback, &progress);

	if (progress.invoked && options->quiet < 2) {
		if (success) {
			print_filename(filename, "Tested");
			printf("\n");
		} else {
			print_filename(filename, "CRC error");
			printf("\n");
		}
	}

	if (!success) {
		// TODO: Exit with error
	}

	free(filename);
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

// A file to be extracted already exists. Apply the overwrite policy
// to decide whether to overwrite the existing file, prompting the
// user if necessary.

static int confirm_file_overwrite(char *filename, LHAOptions *options)
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
		safe_printf("%s ", filename);

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

// Check if the file pointed to by the specified header exists.

static int file_exists(char *filename)
{
	struct stat statbuf;
	int exists;

	if (stat(filename, &statbuf) == 0) {
		exists = 1;
	} else {
		// TODO: Check errno, continue with extract when
		// file not found; otherwise print an error.
		exists = 0;
	}

	return exists;
}

// Extract an archived file.

static void extract_archived_file(LHAReader *reader,
                                  LHAFileHeader *header,
                                  LHAOptions *options)
{
	ProgressCallbackData progress;
	char *filename;
	int success;
	int is_dir;

	filename = file_full_path(header, options);
	is_dir = !strcmp(header->compress_method, LHA_COMPRESS_TYPE_DIR);

	// Print appropriate message and stop if we are performing
	// a dry run. The message if we have an existing file is
	// weird, but this is just accurately duplicating what the
	// Unix lha tool says.

	if (options->dry_run) {
		if (is_dir) {
			safe_printf("EXTRACT %s (directory)\n", filename);
		} else if (file_exists(filename)) {
			safe_printf("EXTRACT %s but file is exist\n", filename);
		} else {
			safe_printf("EXTRACT %s\n", filename);
		}

		free(filename);
		return;
	}

	// If a file already exists with this name, confirm overwrite.

	if (!is_dir && file_exists(filename)
	 && !confirm_file_overwrite(filename, options)) {
		free(filename);
		return;
	}

	// Create parent directories for file:

	if (options->use_path && header->path != NULL) {
		if (!make_parent_directories(header->path)) {
			free(filename);
			return;
		}
	}

	progress.invoked = 0;
	progress.operation = "Melting  :";
	progress.options = options;
	progress.header = header;
	progress.filename = filename;

	success = lha_reader_extract(reader, filename,
	                             progress_callback, &progress);

	if (progress.invoked && options->quiet < 2) {
		if (success) {
			print_filename(filename, "Melted");
			printf("\n");
		} else {
			print_filename(filename, "Failure");
			printf("\n");
		}
	}

	if (!success) {
		// TODO: Exit with error
	}

	free(filename);
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
	// Change directory before extract? (-w option).

	if (options->extract_path != NULL) {
		if (chdir(options->extract_path) != 0) {
			fprintf(stderr, "Failed to change directory to %s.\n",
			        options->extract_path);
			exit(-1);
		}
	}

	for (;;) {
		LHAFileHeader *header;

		header = lha_filter_next_file(filter);

		if (header == NULL) {
			break;
		}

		extract_archived_file(filter->reader, header, options);
	}
}

