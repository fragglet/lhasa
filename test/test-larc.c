#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "lha_reader.h"

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

int main(int argc, char *argv[])
{
	test_lz4();
	test_lz5();

	return 0;
}

