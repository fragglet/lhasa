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
#include <assert.h>

#include "lha_reader.h"
#include "crc32.h"

struct expected_header {
	char *method;
	char *filename;
	size_t length;
	size_t compressed_length;
	uint16_t crc;
};

static void test_read_directory(char *filename,
                                struct expected_header *expected)
{
	FILE *fstream;
	LHAInputStream *stream;
	LHAReader *reader;
	LHAFileHeader *header;

	fstream = fopen(filename, "rb");
	stream = lha_input_stream_new(fstream);
	reader = lha_reader_new(stream);

	header = lha_reader_next_file(reader);

	assert(header != NULL);

	// Check file header fields:

	assert(!strcmp(header->compress_method, expected->method));
	assert(!strcmp(header->filename, expected->filename));
	assert(header->length == expected->length);
	assert(header->compressed_length == expected->compressed_length);
	assert(header->crc == expected->crc);

	// Only entry in the file:

	assert(lha_reader_next_file(reader) == NULL);

	fclose(fstream);
	lha_input_stream_free(stream);
	lha_reader_free(reader);
}

static void test_decompress(char *arcname, char *filename,
                            uint32_t expected_crc)
{
	FILE *fstream;
	LHAInputStream *stream;
	LHAReader *reader;
	LHAFileHeader *header;
	size_t bytes, total_bytes;
	uint32_t crc;

	fstream = fopen(arcname, "rb");
	stream = lha_input_stream_new(fstream);
	reader = lha_reader_new(stream);

	// Loop through directory until we find the file.

	do {

		header = lha_reader_next_file(reader);

		assert(header != NULL);
	} while (strcmp(filename, header->filename) != 0);

	// This is the file.  Read and calculate CRC.

	crc = 0;
	total_bytes = 0;

	do {
		uint8_t buf[64];

		bytes = lha_reader_read(reader, buf, sizeof(buf));
		total_bytes += bytes;

		crc32_buf(&crc, buf, bytes);
		//fwrite(buf, bytes, 1, stdout);
	} while (bytes > 0);

	assert(crc == expected_crc);

	fclose(fstream);
	lha_input_stream_free(stream);
	lha_reader_free(reader);
}

void test_lz4(void)
{
	struct expected_header expected = {
		"-lz4-",
		"GPL-2.GZ",
		6829,
		6829,
		0xb6d5
	};

	test_read_directory("larc_lz4.lzs", &expected);
}

void test_lz4_decompress(void)
{
	test_decompress("larc_lz4.lzs", "GPL-2.GZ", 0xe4690583);
}

void test_lz5(void)
{
	struct expected_header expected = {
		"-lz5-",
		"GPL-2",
		18092,
		8480,
		0xa33a
	};

	test_read_directory("larc_lz5.lzs", &expected);
}

void test_lz5_decompress(void)
{
	test_decompress("larc_lz5.lzs", "GPL-2", 0x4e46f4a1);
}

int main(int argc, char *argv[])
{
	test_lz4();
	test_lz4_decompress();

	test_lz5();
	test_lz5_decompress();

	return 0;
}

