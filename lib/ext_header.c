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

#include "ext_header.h"

// ext_common.c:

extern LHAExtHeaderType lha_ext_header_common;
extern LHAExtHeaderType lha_ext_header_filename;
extern LHAExtHeaderType lha_ext_header_path;

// ext_unix.c:

extern LHAExtHeaderType lha_ext_header_unix_perms;
extern LHAExtHeaderType lha_ext_header_unix_uid_gid;
extern LHAExtHeaderType lha_ext_header_unix_username;
extern LHAExtHeaderType lha_ext_header_unix_group;
extern LHAExtHeaderType lha_ext_header_unix_timestamp;

// ext_win.c:

extern LHAExtHeaderType lha_ext_header_windows_timestamps;

static const LHAExtHeaderType *ext_header_types[] = {
	&lha_ext_header_common,
	&lha_ext_header_filename,
	&lha_ext_header_path,
	&lha_ext_header_unix_perms,
	&lha_ext_header_unix_uid_gid,
	&lha_ext_header_unix_username,
	&lha_ext_header_unix_group,
	&lha_ext_header_unix_timestamp,
	&lha_ext_header_windows_timestamps,
};

/**
 * Look up the extended header parser for the specified header code.
 *
 * @param num       Extended header type.
 * @return          Matching @ref LHAExtHeaderType structure, or NULL if
 *                  not found for this header type.
 */

static const LHAExtHeaderType *ext_header_for_num(uint8_t num)
{
	unsigned int i;

	for (i = 0; i < sizeof(ext_header_types) / sizeof(*ext_header_types); ++i) {
		if (ext_header_types[i]->num == num) {
			return ext_header_types[i];
		}
	}

	return NULL;
}

int lha_ext_header_decode(LHAFileHeader *header,
                          uint8_t num,
                          uint8_t *data,
                          size_t data_len)
{
	const LHAExtHeaderType *htype;

	htype = ext_header_for_num(num);

	if (htype == NULL) {
		return 0;
	}

	if (data_len < htype->min_len) {
		return 0;
	}

	return htype->decoder(header, data, data_len);
}

