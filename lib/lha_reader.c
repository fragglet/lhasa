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
#include "lha_reader.h"

struct _LHAReader {
	LHAInputStream *stream;
	LHAFileHeader *curr_file;
	LHADecoder *decoder;
	size_t curr_file_remaining;
	int eof;
};

LHAReader *lha_reader_new(LHAInputStream *stream)
{
	LHAReader *reader;

	reader = malloc(sizeof(LHAReader));

	if (reader == NULL) {
		return NULL;
	}

	reader->stream = stream;
	reader->curr_file = NULL;
	reader->curr_file_remaining = 0;
	reader->decoder = NULL;
	reader->eof = 0;

	return reader;
}

void lha_reader_free(LHAReader *reader)
{
	if (reader->curr_file != NULL) {
		lha_file_header_free(reader->curr_file);
	}

	if (reader->decoder != NULL) {
		lha_decoder_free(reader->decoder);
	}

	free(reader);
}

LHAFileHeader *lha_reader_next_file(LHAReader *reader)
{
	if (reader->eof) {
		return NULL;
	}

	// Free a decoder if we have one.

	if (reader->decoder != NULL) {
		lha_decoder_free(reader->decoder);
		reader->decoder = NULL;
	}

	// Free the current file header and skip over any remaining
	// compressed data that hasn't been read yet.

	if (reader->curr_file != NULL) {
		lha_file_header_free(reader->curr_file);

		lha_input_stream_skip(reader->stream,
		                      reader->curr_file_remaining);
	}

	// Read the header for the next file.

	reader->curr_file = lha_file_header_read(reader->stream);

	if (reader->curr_file == NULL) {
		reader->eof = 1;
		return NULL;
	}

	reader->curr_file_remaining = reader->curr_file->compressed_length;

	return reader->curr_file;
}

size_t lha_reader_read_compressed(LHAReader *reader, void *buf, size_t buf_len)
{
	size_t bytes;

	if (reader->eof || reader->curr_file_remaining == 0) {
		return 0;
	}

	// Read up to the number of bytes of compressed data remaining.

	if (buf_len > reader->curr_file_remaining) {
		bytes = reader->curr_file_remaining;
	} else {
		bytes = buf_len;
	}

	if (!lha_input_stream_read(reader->stream, buf, bytes)) {
		reader->eof = 1;
		return 0;
	}

	// Update counter and return success.

	reader->curr_file_remaining -= bytes;

	return bytes;
}

static size_t decoder_callback(void *buf, size_t buf_len, void *user_data)
{
	return lha_reader_read_compressed(user_data, buf, buf_len);
}

static void open_decoder(LHAReader *reader)
{
	LHADecoderType *dtype;

	// Look up the decoder to use for this compression method.

	dtype = lha_decoder_for_name(reader->curr_file->compress_method);

	if (dtype == NULL) {
		return;
	}

	// Create decoder.

	reader->decoder = lha_decoder_new(dtype, decoder_callback, reader,
	                                  reader->curr_file->length);
}

size_t lha_reader_read(LHAReader *reader, void *buf, size_t buf_len)
{
	// The first time this is called, we have to create a decoder.

	if (reader->curr_file != NULL && reader->decoder == NULL) {
		open_decoder(reader);

		if (reader->decoder == NULL) {
			return 0;
		}
	}

	// Read from decoder and return result.

	return lha_decoder_read(reader->decoder, buf, buf_len);
}

// Decompress the current file, invoking the specified callback function
// to monitor progress (if not NULL).  Decompressed data is written to
// 'output' if it is not NULL.
// Returns true if the file decompressed successfully.

static int do_decompress(LHAReader *reader,
                         FILE *output,
                         LHADecoderProgressCallback callback,
                         void *callback_data)
{
	uint8_t buf[64];
	uint16_t crc;
	unsigned int bytes, total_bytes;

	// Initialize decoder, and set progress callback.

	open_decoder(reader);

	if (reader->decoder == NULL) {
		return 0;
	}

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
	if (reader->curr_file == NULL) {
		return 0;
	}

	// CRC checking of directories is not necessary.

	if (!strcmp(reader->curr_file->compress_method, LHA_COMPRESS_TYPE_DIR)) {
		return 1;
	}

	return do_decompress(reader, NULL, callback, callback_data);
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

// Open an output stream to decompress the current file.
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

// Extract directory, setting Unix permissions corresponding to
// file headers.
// TODO: Make this compile-dependent so that we can compile
// using ANSI C and on non-Unix platforms.

static int extract_directory_unix(LHAFileHeader *header, char *path,
                                  int set_perms)
{
	mode_t mode;

	// Create directory. If there are permissions to be set, create
	// the directory with minimal permissions limited to the running
	// user. Otherwise use the default umask.

	if (set_perms && (header->extra_flags & LHA_FILE_UNIX_PERMS)) {
		mode = 0700;
	} else {
		mode = 0777;
	}

	if (mkdir(path, mode)) {
		return 0;
	}

	// Don't set permissions?

	if (!set_perms) {
		return 1;
	}

	// Set owner and group:

	if (header->extra_flags & LHA_FILE_UNIX_UID_GID) {
		if (chown(path, header->unix_uid, header->unix_gid)) {
			rmdir(path);
			return 0;
		}
	}

	// Set permissions on directory:

	if (header->extra_flags & LHA_FILE_UNIX_PERMS) {
		if (chmod(path, header->unix_perms)) {
			rmdir(path);
			return 0;
		}
	}

	return 1;
}

static int extract_directory(LHAReader *reader, char *path)
{
	// If path is not specified, use the path from the file header.

	if (path == NULL) {
		path = reader->curr_file->path;
	}

	// Try to create directory setting Unix file permissions; if that
	// fails, just perform a simple mkdir() ignoring file permissions:

	return extract_directory_unix(reader->curr_file, path, 1)
	    || extract_directory_unix(reader->curr_file, path, 0);
}

int lha_reader_extract(LHAReader *reader,
                       char *filename,
                       LHADecoderProgressCallback callback,
                       void *callback_data)
{
	FILE *fstream;
	int result;

	if (reader->curr_file == NULL) {
		return 0;
	}

	// Directories are a special case:

	if (!strcmp(reader->curr_file->compress_method, LHA_COMPRESS_TYPE_DIR)) {
		return extract_directory(reader, filename);
	}

	// Open output file and perform decompress:

	fstream = open_output_file(reader, filename);

	if (fstream == NULL) {
		return 0;
	}

	result = do_decompress(reader, fstream, callback, callback_data);

	fclose(fstream);

	return result;
}

