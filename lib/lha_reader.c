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

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "crc16.h"

#include "lha_decoder.h"
#include "lha_basic_reader.h"
#include "lha_reader.h"

typedef enum {

	// Initial state at start of stream:

	CURR_FILE_START,

	// Current file is a "normal" file (or directory) read from
	// the input stream.

	CURR_FILE_NORMAL,

	// Current file is a directory that has been popped from the
	// directory stack.

	CURR_FILE_FAKE_DIR,

	// End of input stream has been reached.

	CURR_FILE_EOF,
} CurrFileType;

struct _LHAReader {
	LHABasicReader *reader;

	// The current file that we are processing (last file returned
	// by lha_reader_next_file).

	LHAFileHeader *curr_file;
	CurrFileType curr_file_type;

	// Pointer to decoder being used to decompress the current file,
	// or NULL if we have not yet started decompression.

	LHADecoder *decoder;

	// Policy used to extract directories.

	LHAReaderDirPolicy dir_policy;

	// Directories that have been created by lha_reader_extract but
	// have not yet had their metadata set. This is a linked list
	// using the _next field in LHAFileHeader.
	// In the case of LHA_READER_DIR_END_OF_DIR this is a stack;
	// in the case of LHA_READER_DIR_END_OF_FILE it is a list.

	LHAFileHeader *dir_stack;
};

LHAReader *lha_reader_new(LHAInputStream *stream)
{
	LHABasicReader *basic_reader;
	LHAReader *reader;

	basic_reader = lha_basic_reader_new(stream);

	if (basic_reader == NULL)
	{
		return NULL;
	}

	reader = malloc(sizeof(LHAReader));

	if (reader == NULL) {
		lha_basic_reader_free(basic_reader);
		return NULL;
	}

	reader->reader = basic_reader;
	reader->curr_file = NULL;
	reader->curr_file_type = CURR_FILE_START;
	reader->decoder = NULL;
	reader->dir_stack = NULL;
	reader->dir_policy = LHA_READER_DIR_END_OF_DIR;

	return reader;
}

void lha_reader_free(LHAReader *reader)
{
	LHAFileHeader *header;

	if (reader->decoder != NULL) {
		lha_decoder_free(reader->decoder);
	}

	// Free any file headers in the stack.

	while (reader->dir_stack != NULL) {
		header = reader->dir_stack;
		reader->dir_stack = header->_next;
		lha_file_header_free(header);
	}

	lha_basic_reader_free(reader->reader);
	free(reader);
}

void lha_reader_set_dir_policy(LHAReader *reader,
                               LHAReaderDirPolicy policy)
{
	reader->dir_policy = policy;
}

// Returns true if the top directory in the stack should be popped off.

static int end_of_top_dir(LHAReader *reader)
{
	LHAFileHeader *input;

	// No directories to pop?

	if (reader->dir_stack == NULL) {
		return 0;
	}

	// Once the end of the input stream is reached, all that is
	// left to do is pop off the remaining directories.

	input = lha_basic_reader_curr_file(reader->reader);

	if (input == NULL) {
		return 1;
	}

	switch (reader->dir_policy) {

		// Shouldn't happen?

		case LHA_READER_DIR_PLAIN:
		default:
			return 1;

		// Don't process directories until we reach the end of
		// the input stream.

		case LHA_READER_DIR_END_OF_FILE:
			return 0;

		// Once we reach a file from the input that is not within
		// the directory at the top of the stack, we have reached
		// the end of that directory, so we can pop it off.

		case LHA_READER_DIR_END_OF_DIR:
			return input->path == NULL
			     || strncmp(input->path,
			               reader->dir_stack->path,
			               strlen(reader->dir_stack->path)) != 0;
	}
}

// Read the next file from the input stream.

LHAFileHeader *lha_reader_next_file(LHAReader *reader)
{
	// Free the current decoder if there is one.

	if (reader->decoder != NULL) {
		lha_decoder_free(reader->decoder);
		reader->decoder = NULL;
	}

	// No point continuing once the end of the input stream has
	// been reached.

	if (reader->curr_file_type == CURR_FILE_EOF) {
		return NULL;
	}

	// Advance to the next file from the input stream?
	// Don't advance until we've done the fake directories first.

	if (reader->curr_file_type == CURR_FILE_START
	 || reader->curr_file_type == CURR_FILE_NORMAL) {
		lha_basic_reader_next_file(reader->reader);
	}

	// Pop off all appropriate directories from the stack first.

	if (end_of_top_dir(reader)) {
		reader->curr_file = reader->dir_stack;
		reader->dir_stack = reader->dir_stack->_next;
		reader->curr_file_type = CURR_FILE_FAKE_DIR;
	} else {
		reader->curr_file = lha_basic_reader_curr_file(reader->reader);

		if (reader->curr_file != NULL) {
			reader->curr_file_type = CURR_FILE_NORMAL;
		} else {
			reader->curr_file_type = CURR_FILE_EOF;
		}
	}

	return reader->curr_file;
}

// Create the decoder structure to decompress the data from the
// current file. Returns 1 for success.

static int open_decoder(LHAReader *reader)
{
	// Can only read from a normal file.

	if (reader->curr_file_type != CURR_FILE_NORMAL) {
		return 0;
	}

	reader->decoder = lha_basic_reader_decode(reader->reader);

	if (reader->decoder == NULL) {
		return 0;
	}

	return 1;
}

size_t lha_reader_read(LHAReader *reader, void *buf, size_t buf_len)
{
	// The first time that we try to read the current file, we
	// must create the decoder to decompress it.

	if (reader->decoder == NULL) {
		if (!open_decoder(reader)) {
			return 0;
		}
	}

	// Read from decoder and return the result.

	return lha_decoder_read(reader->decoder, buf, buf_len);
}

// Decompress the current file, invoking the specified callback function
// to monitor progress (if not NULL). Assumes that open_decoder() has
// already been called to initialize decode the file. Decompressed data
// is written to 'output' if it is not NULL.
// Returns true if the file decompressed successfully.

static int do_decode(LHAReader *reader,
                     FILE *output,
                     LHADecoderProgressCallback callback,
                     void *callback_data)
{
	uint8_t buf[64];
	uint16_t crc;
	unsigned int bytes, total_bytes;

	// Set progress callback for decoder.

	if (callback != NULL) {
		lha_decoder_monitor(reader->decoder, callback,
		                    callback_data);
	}

	// Decompress the current file, performing a running
	// CRC of the contents as we go.

	total_bytes = 0;
	crc = 0;

	do {
		bytes = lha_reader_read(reader, buf, sizeof(buf));

		if (output != NULL) {
			if (fwrite(buf, 1, bytes, output) < bytes) {
				return 0;
			}
		}

		lha_crc16_buf(&crc, buf, bytes);
		total_bytes += bytes;

	} while (bytes > 0);

	// Decompressed length should match, as well as CRC.

	return total_bytes == reader->curr_file->length
	    && crc == reader->curr_file->crc;
}

int lha_reader_check(LHAReader *reader,
                     LHADecoderProgressCallback callback,
                     void *callback_data)
{
	if (reader->curr_file_type != CURR_FILE_NORMAL) {
		return 0;
	}

	// CRC checking of directories is not necessary.

	if (!strcmp(reader->curr_file->compress_method, LHA_COMPRESS_TYPE_DIR)) {
		return 1;
	}

	// Decode file.

	return open_decoder(reader)
	    && do_decode(reader, NULL, callback, callback_data);
}

// Open file, setting Unix permissions
// TODO: Make this compile-dependent so that we can compile only
// using ANSI C.

static FILE *open_output_file_unix(LHAReader *reader, char *filename)
{
	FILE *fstream;
	int fileno;

	// If we have file permissions, they must be set after the
	// file is created and UID/GID have been set.  When open()ing
	// the file, create it with minimal permissions granted only
	// to the current user.

	fileno = open(filename, O_CREAT|O_WRONLY|O_TRUNC, 0600);

	if (fileno < 0) {
		return NULL;
	}

	// Set owner and group.

	if (reader->curr_file->extra_flags & LHA_FILE_UNIX_UID_GID) {
		if (fchown(fileno, reader->curr_file->unix_uid,
		           reader->curr_file->unix_gid)) {
			close(fileno);
			remove(filename);
			return NULL;
		}
	}

	// Set file permissions.
	// File permissiosn must be set *after* owner and group have
	// been set; otherwise, we might briefly be granting permissions
	// to the wrong group.

	if (reader->curr_file->extra_flags & LHA_FILE_UNIX_PERMS) {
		if (fchmod(fileno, reader->curr_file->unix_perms)) {
			close(fileno);
			remove(filename);
			return NULL;
		}
	}

	// Create stdc FILE handle.

	fstream = fdopen(fileno, "wb");

	if (fstream == NULL) {
		close(fileno);
		remove(filename);
		return NULL;
	}

	return fstream;
}

// Open an output stream into which to decompress the current file.
// The filename is constructed from the file header of the current file,
// or 'filename' is used if it is not NULL.

static FILE *open_output_file(LHAReader *reader, char *filename)
{
	FILE *fstream;
	char *tmp_filename = NULL;
	unsigned int filename_len;

	// Construct filename?

	if (filename == NULL) {
		if (reader->curr_file->path != NULL) {
			filename_len = strlen(reader->curr_file->filename)
			             + strlen(reader->curr_file->path)
			             + 1;

			tmp_filename = malloc(filename_len);

			if (tmp_filename == NULL) {
				return NULL;
			}

			sprintf(tmp_filename, "%s%s", reader->curr_file->path,
			                      reader->curr_file->filename);

			filename = tmp_filename;
		} else {
			filename = reader->curr_file->filename;
		}
	}

	// If this file has Unix file permission headers, make sure
	// the file is created using the correct permissions.
	// If this fails, we fall back to creating a normal file.

	if (reader->curr_file->extra_flags
	      & (LHA_FILE_UNIX_PERMS | LHA_FILE_UNIX_UID_GID)) {
		fstream = open_output_file_unix(reader, filename);
	} else {
		fstream = NULL;
	}

	// Create file normally via fopen():

	if (fstream == NULL) {
		fstream = fopen(filename, "wb");
	}

	free(tmp_filename);

	return fstream;
}


// Create a directory.
// TODO: Make this compile-dependent so that we can compile
// using ANSI C and on non-Unix platforms.

static int create_directory_unix(LHAFileHeader *header, char *path)
{
	mode_t mode;

	// Create directory. If there are permissions to be set, create
	// the directory with minimal permissions limited to the running
	// user. Otherwise use the default umask.

	if (header->extra_flags & LHA_FILE_UNIX_PERMS) {
		mode = 0700;
	} else {
		mode = 0777;
	}

	return mkdir(path, mode) == 0;
}

// Second stage of directory extraction: set metadata.

static int set_directory_metadata(LHAFileHeader *header, char *path)
{
	// TODO: modification date/time

	// Set owner and group:

	if (header->extra_flags & LHA_FILE_UNIX_UID_GID) {
		if (chown(path, header->unix_uid, header->unix_gid)) {
			return 0;
		}
	}

	// Set permissions on directory:

	if (header->extra_flags & LHA_FILE_UNIX_PERMS) {
		if (chmod(path, header->unix_perms)) {
			return 0;
		}
	}

	return 1;
}

static int extract_directory(LHAReader *reader, char *path)
{
	LHAFileHeader *header;

	header = reader->curr_file;

	// If path is not specified, use the path from the file header.

	if (path == NULL) {
		path = header->path;
	}

	// Create the directory.

	if (!create_directory_unix(header, path)) {
		return 0;
	}

	// The directory has been created, but the metadata has not yet
	// been applied. It depends on the directory policy how this
	// is handled. If we are using LHA_READER_DIR_PLAIN, set
	// metadata now. Otherwise, save the directory for later.

	if (reader->dir_policy == LHA_READER_DIR_PLAIN) {
		set_directory_metadata(header, path);
	} else {
		lha_file_header_add_ref(header);
		header->_next = reader->dir_stack;
		reader->dir_stack = header;
	}

	return 1;
}

static int extract_normal(LHAReader *reader,
                          char *filename,
                          LHADecoderProgressCallback callback,
                          void *callback_data)
{
	FILE *fstream;
	int result;

	// Directories are a special case:

	if (!strcmp(reader->curr_file->compress_method, LHA_COMPRESS_TYPE_DIR)) {
		return extract_directory(reader, filename);
	}

	// Create decoder. If the file cannot be created, there is no
	// need to even create an output file.

	if (!open_decoder(reader)) {
		return 0;
	}

	// Open output file and perform decode:

	fstream = open_output_file(reader, filename);

	if (fstream == NULL) {
		return 0;
	}

	result = do_decode(reader, fstream, callback, callback_data);

	fclose(fstream);

	return result;
}

int lha_reader_extract(LHAReader *reader,
                       char *filename,
                       LHADecoderProgressCallback callback,
                       void *callback_data)
{
	switch (reader->curr_file_type) {

		case CURR_FILE_NORMAL:
			return extract_normal(reader, filename, callback,
			                      callback_data);

		case CURR_FILE_FAKE_DIR:
			set_directory_metadata(reader->curr_file, filename);
			return 1;

		case CURR_FILE_START:
		case CURR_FILE_EOF:
			break;
	}

	return 0;
}

