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

typedef struct {
	char *method;
	char *filename;
	size_t length;
	size_t compressed_length;
	uint16_t crc16;
	uint32_t crc32;
} ExpectedFileDetails;

static void test_read_directory(char *filename,
                                ExpectedFileDetails *expected)
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
	assert(header->crc == expected->crc16);

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

static void test_crc_check(char *filename)
{
	FILE *fstream;
	LHAInputStream *stream;
	LHAReader *reader;
	LHAFileHeader *header;

	fstream = fopen(filename, "rb");
	stream = lha_input_stream_new(fstream);
	reader = lha_reader_new(stream);

	// Read all files in the directory, and check CRCs.

	for (;;) {
		header = lha_reader_next_file(reader);

		if (header == NULL) {
			break;
		}

		assert(lha_reader_check(reader, NULL, NULL) != 0);
	}

	fclose(fstream);
	lha_input_stream_free(stream);
	lha_reader_free(reader);
}

// Run all tests for the specified file.

static void test_file(char *filename, ExpectedFileDetails *expected)
{
	test_read_directory(filename, expected);
	test_decompress(filename, expected->filename, expected->crc32);
	test_crc_check(filename);

	//printf("Passed: %s\n", filename);
}

//
// LArc (.lzs) archive tests:
//

static ExpectedFileDetails lz4_expected = {
	"-lz4-",
	"gpl-2.gz",
	6829,
	6829,
	0xb6d5,
	0xe4690583
};

static ExpectedFileDetails lz5_expected = {
	"-lz5-",
	"gpl-2",
	18092,
	8480,
	0xa33a,
	0x4e46f4a1
};

static ExpectedFileDetails lz5_long_expected = {
	"-lz5-",
	"long.txt",
	1241658,
	226557,
	0x6a7c,
	0x06788e85
};

void test_larc(void)
{
	// LZ4 (LArc uncompressed):

	test_file("archives/larc333/lz4.lzs", &lz4_expected);

	// LZ5 (LArc compressed):

	test_file("archives/larc333/lz5.lzs", &lz5_expected);
	test_file("archives/larc333/long.lzs", &lz5_long_expected);
}

//
// LHArc ("old" .lzh) archive tests:
//

static ExpectedFileDetails lh0_expected = {
	"-lh0-",
	"gpl-2.gz",
	6829,
	6829,
	0xb6d5,
	0xe4690583
};

static ExpectedFileDetails lh1_expected = {
	"-lh1-",
	"gpl-2",
	18092,
	7518,
	0xa33a,
	0x4e46f4a1
};

static ExpectedFileDetails lh1_long_expected = {
	"-lh1-",
	"long.txt",
	1241658,
	114249,
	0x6a7c,
	0x06788e85
};

void test_lharc(void)
{
	// lh0 (LHA uncompressed):

	test_file("archives/lharc113/lh0.lzh", &lh0_expected);

	// lh1 (LHA "old" algorithm):

	test_file("archives/lharc113/lh1.lzh", &lh1_expected);
	test_file("archives/lharc113/long.lzh", &lh1_long_expected);
}

//
// PMArc (.pma) archive tests:
//

static ExpectedFileDetails pm0_expected = {
	"-pm0-",
	"gpl-2.gz",
	6912,
	6912,
	0x39cc,
	0x549d935a
};

static ExpectedFileDetails pm2_expected = {
	"-pm2-",
	"gpl-2.",
	 18176,
	 7098,
	 0x83cd,
	 0x8e2093a7
};

static ExpectedFileDetails pm2_long_expected = {
	"-pm2-",
	"long.txt",
	1241659,
	85397,
	0x2aea,
	0xb2b419d6
};

static void test_pmarc()
{
	// pm0 (PMA uncompressed):

	test_file("archives/pmarc2/pm0.pma", &pm0_expected);

	// pm2 (PMA compressed):

	test_file("archives/pmarc2/pm2.pma", &pm2_expected);
	test_file("archives/pmarc2/long.pma", &pm2_long_expected);
}

int main(int argc, char *argv[])
{
	test_larc();
	test_lharc();
	test_pmarc();

	return 0;
}

