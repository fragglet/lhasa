
#include <stdio.h>
#include <string.h>

#include "lhasa.h"

int main(int argc, char *argv[])
{
	LHAOutputStream *out;
	LHAFileHeader header;
	FILE *fs;

	if (argc < 3) {
		fprintf(stderr, "Usage: %s file.lzh filename\n", argv[0]);
		exit(1);
	}

	out = lha_output_stream_to(argv[1]);
	if (out == NULL) {
		fprintf(stderr, "failed to open %s for writing\n", argv[1]);
		exit(1);
	}

	memset(&header, 0, sizeof(header));
	header.filename = argv[2];
	header.os_type = LHA_OS_TYPE_UNIX;

	fs = fopen(argv[2], "rb");
	lha_write_file(out, &header, fs);
	fclose(fs);

	lha_output_stream_free(out);

	return 0;
}

