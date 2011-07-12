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


#ifndef LHASA_LHA_FILE_HEADER_H
#define LHASA_LHA_FILE_HEADER_H

#include "lha_input_stream.h"

#define LHA_FILE_UNIX_PERMS            0x01
#define LHA_FILE_UNIX_UID_GID          0x02

// Common OS types:

#define LHA_OS_TYPE_UNKNOWN            0x00
#define LHA_OS_TYPE_MSDOS              'M'  /* Microsoft MS/DOS */
#define LHA_OS_TYPE_WIN95              'w'  /* Microsoft Windows 95 */
#define LHA_OS_TYPE_WINNT              'W'  /* Microsoft Windows NT */
#define LHA_OS_TYPE_UNIX               'U'  /* Generic Unix */
#define LHA_OS_TYPE_OS2                '2'  /* IBM OS/2 */
#define LHA_OS_TYPE_MACOS              'm'  /* Apple classic Mac OS */

// Obscure:

#define LHA_OS_TYPE_JAVA               'J'  /* Java */
#define LHA_OS_TYPE_CPM                'C'  /* Digital Research CP/M */
#define LHA_OS_TYPE_FLEX               'F'  /* Digital Research FlexOS */
#define LHA_OS_TYPE_RUNSER             'R'
#define LHA_OS_TYPE_TOWNSOS            'T'  /* Fujitsu FM Towns */
#define LHA_OS_TYPE_OS9                '9'  /* Microware OS-9 */
#define LHA_OS_TYPE_OS9_68K            'K'  /* Microware OS-9 - 68k */
#define LHA_OS_TYPE_OS386              '3'
#define LHA_OS_TYPE_HUMAN68K           'H'  /* Sharp X68000 Human68K OS */

typedef struct _LHAFileHeader LHAFileHeader;

struct _LHAFileHeader {
	char compress_method[6];
	size_t compressed_length;
	size_t length;
	char *path;
	char *filename;
	uint8_t header_level;
	uint8_t os_type;
	uint16_t crc;
	unsigned int timestamp;
	void *raw_data;
	size_t raw_data_len;
	unsigned int extra_flags;

	// Optional data (from extended headers):

	unsigned int unix_perms;
	unsigned int unix_uid;
	unsigned int unix_gid;
	char *unix_group;
	char *unix_username;
};

/**
 * Read a file header from the input stream.
 *
 * @param stream         The input stream to read from.
 * @return               Pointer to a new LHAFileHeader structure, or NULL
 *                       if an error occurred or a valid header could not
 *                       be read.
 */

LHAFileHeader *lha_file_header_read(LHAInputStream *stream);

/**
 * Free a file header structure.
 *
 * @param header         The file header to free.
 */

void lha_file_header_free(LHAFileHeader *header);

#endif /* #ifndef LHASA_LHA_FILE_HEADER_H */

