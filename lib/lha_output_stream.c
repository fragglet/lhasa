/*

Copyright (c) 2012, Simon Howard

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


#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "lha_arch.h"
#include "lha_output_stream.h"

struct _LHAOutputStream {
	const LHAOutputStreamType *type;
	void *handle;
};

LHAOutputStream *lha_output_stream_new(const LHAOutputStreamType *type,
                                       void *handle)
{
	LHAOutputStream *stream;

	stream = malloc(sizeof(LHAOutputStream));

	if (stream == NULL) {
		return NULL;
	}

	stream->type = type;
	stream->handle = handle;

	return stream;
}

void lha_output_stream_free(LHAOutputStream *stream)
{
	if (stream->type->close != NULL) {
		stream->type->close(stream->handle);
	}

	free(stream);
}

int lha_output_stream_write(LHAOutputStream *stream,
                            void *buf, size_t buf_len)
{
	int bytes_written;

	bytes_written = stream->type->write(stream->handle, buf, buf_len);

	return bytes_written > 0 && (size_t) bytes_written == buf_len;
}

off_t lha_output_stream_tell(LHAOutputStream *stream)
{
	return stream->type->tell(stream->handle);
}

int lha_output_stream_seek(LHAOutputStream *stream, off_t position)
{
	return stream->type->seek(stream->handle, position);
}

// Write data to a FILE * output stream.

static int file_sink_write(void *handle, void *buf, size_t buf_len)
{
	return fwrite(buf, 1, buf_len, handle);
}

// Read the position within a FILE * output stream.

static off_t file_sink_tell(void *handle)
{
	return ftell(handle);
}

// Set the position within a FILE * output stream.

static int file_sink_seek(void *handle, off_t position)
{
	return fseek(handle, position, SEEK_SET) == 0;
}

// Close a FILE * output stream.

static void file_sink_close(void *handle)
{
	fclose(handle);
}

// A file sink that is "owned" by the library - the handle is closed when
// the stream is closed.

static const LHAOutputStreamType file_sink_owned = {
	file_sink_write,
	file_sink_tell,
	file_sink_seek,
	file_sink_close,
};

// A file sink that is not "owned" - the handle must be closed by the caller.

static const LHAOutputStreamType file_sink_unowned = {
	file_sink_write,
	file_sink_tell,
	file_sink_seek,
	NULL,
};

LHAOutputStream *lha_output_stream_to(char *filename)
{
	LHAOutputStream *result;
	FILE *fstream;

	fstream = fopen(filename, "wb");

	if (fstream == NULL) {
		return NULL;
	}

	result = lha_output_stream_new(&file_sink_owned, fstream);

	if (result == NULL) {
		fclose(fstream);
	}

	return result;
}

LHAOutputStream *lha_output_stream_to_FILE(FILE *stream)
{
	lha_arch_set_binary(stream);
	return lha_output_stream_new(&file_sink_unowned, stream);
}

