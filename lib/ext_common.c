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

#include <string.h>
#include <ctype.h>

#include "ext_header.h"

static int ext_header_filename_decoder(LHAFileHeader *header,
                                       uint8_t *data,
                                       size_t data_len)
{
	char *new_filename;

	new_filename = malloc(data_len + 1);

	if (new_filename == NULL) {
		return 0;
	}

	memcpy(new_filename, data, data_len);
	new_filename[data_len] = '\0';

	free(header->filename);
	header->filename = new_filename;

	return 1;
}

LHAExtHeaderType lha_ext_header_filename = {
	LHA_EXT_HEADER_FILENAME,
	ext_header_filename_decoder,
	1
};

static int ext_header_path_decoder(LHAFileHeader *header,
                                   uint8_t *data,
                                   size_t data_len)
{
	unsigned int i;
	char *new_path;

	new_path = malloc(data_len + 1);

	if (new_path == NULL) {
		return 0;
	}

	memcpy(new_path, data, data_len);
	new_path[data_len] = '\0';

	free(header->path);
	header->path = new_path;

	for (i = 0; i < data_len; ++i) {
		if (data[i] == 0xff) {
			new_path[i] = '/';
		} else if (header->os_type == LHA_OS_TYPE_MSDOS) {
			new_path[i] = (char) tolower(new_path[i]);
		}
	}

	return 1;
}

LHAExtHeaderType lha_ext_header_path = {
	LHA_EXT_HEADER_PATH,
	ext_header_path_decoder,
	1
};

