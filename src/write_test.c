
#include <stdio.h>
#include <string.h>

#include "lhasa.h"
#include "../lha_arch.h"

int main(int argc, char *argv[])
{
	LHAOutputStream *out;
	LHAFileHeader header;
	FILE *fs;
	int i;

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
	header.path = "my_directory/";
	header.os_type = LHA_OS_TYPE_UNIX;
	lha_write_file(out, &header, NULL);

	for (i = 2; i < argc; i++) {
		memset(&header, 0, sizeof(header));
		if (!lha_arch_stat(argv[i], &header)) {
			// TODO
		}
		header.filename = argv[i];
		header.os_type = LHA_OS_TYPE_UNIX;

		fs = fopen(argv[i], "rb");
		if (fs == NULL) {
			// TODO
		}
		lha_write_file(out, &header, fs);
		if (fs != NULL) {
			fclose(fs);
		}

		free(header.symlink_target);
		free(header.raw_data);
	}

	lha_output_stream_free(out);

	return 0;
}

