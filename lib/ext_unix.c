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

static int ext_header_unix_perms_decoder(LHAFileHeader *header,
                                         uint8_t *data,
                                         size_t data_len)
{
	header->extra_flags |= LHA_FILE_UNIX_PERMS;
	header->unix_perms = lha_decode_uint16(data);

	return 1;
}

LHAExtHeaderType lha_ext_header_unix_perms = {
	LHA_EXT_HEADER_UNIX_PERMISSION,
	ext_header_unix_perms_decoder,
	2
};

static int ext_header_unix_uid_gid_decoder(LHAFileHeader *header,
                                           uint8_t *data,
                                           size_t data_len)
{
	header->extra_flags |= LHA_FILE_UNIX_UID_GID;
	header->unix_gid = lha_decode_uint16(data);
	header->unix_uid = lha_decode_uint16(data + 2);

	return 1;
}

LHAExtHeaderType lha_ext_header_unix_uid_gid = {
	LHA_EXT_HEADER_UNIX_UID_GID,
	ext_header_unix_uid_gid_decoder,
	4
};

static int ext_header_unix_username_decoder(LHAFileHeader *header,
                                            uint8_t *data,
                                            size_t data_len)
{
	header->unix_username = malloc(data_len + 1);

	if (header->unix_username == NULL) {
		return 0;
	}

	memcpy(header->unix_username, data, data_len);
	header->unix_username[data_len] = '\0';

	return 1;
}

LHAExtHeaderType lha_ext_header_unix_username = {
	LHA_EXT_HEADER_UNIX_USER,
	ext_header_unix_username_decoder,
	1
};

static int ext_header_unix_group_decoder(LHAFileHeader *header,
                                         uint8_t *data,
                                         size_t data_len)
{
	header->unix_group = malloc(data_len + 1);

	if (header->unix_group == NULL) {
		return 0;
	}

	memcpy(header->unix_group, data, data_len);
	header->unix_group[data_len] = '\0';

	return 1;
}

LHAExtHeaderType lha_ext_header_unix_group = {
	LHA_EXT_HEADER_UNIX_GROUP,
	ext_header_unix_group_decoder,
	1
};

static int ext_header_unix_timestamp_decoder(LHAFileHeader *header,
                                             uint8_t *data,
                                             size_t data_len)
{
	header->timestamp = lha_decode_uint32(data);

	return 1;
}

LHAExtHeaderType lha_ext_header_unix_timestamp = {
	LHA_EXT_HEADER_UNIX_TIMESTAMP,
	ext_header_unix_timestamp_decoder,
	4
};

