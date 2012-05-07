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

#include "lha_endian.h"
#include "ext_header.h"

static int ext_header_windows_timestamps(LHAFileHeader *header,
                                         uint8_t *data,
                                         size_t data_len)
{
	header->extra_flags |= LHA_FILE_WINDOWS_TIMESTAMPS;
	header->win_creation_time = lha_decode_uint64(data);
	header->win_modification_time = lha_decode_uint64(data + 8);
	header->win_access_time = lha_decode_uint64(data + 16);

	return 1;
}

LHAExtHeaderType lha_ext_header_windows_timestamps = {
	LHA_EXT_HEADER_WINDOWS_TIMESTAMPS,
	ext_header_windows_timestamps,
	24
};

