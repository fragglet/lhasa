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

#ifndef LHASA_HEADER_DEFS_H
#define LHASA_HEADER_DEFS_H

// Minimum length of an LHA header, regardless of header type - any
// header must be at least this long.
#define COMMON_HEADER_LEN 22 /* bytes */

// Minimum length of a level 0 header (with zero-length filename).
#define LEVEL_0_MIN_HEADER_LEN 22 /* bytes */

// Minimum length of a level 1 base header (with zero-length filename).
#define LEVEL_1_MIN_HEADER_LEN 25 /* bytes */

// Length of a level 2 base header.
#define LEVEL_2_HEADER_LEN 26 /* bytes */

// Length of a level 3 base header.
#define LEVEL_3_HEADER_LEN 32 /* bytes */

// Maximum length of a level 3 header (including extended headers).
#define LEVEL_3_MAX_HEADER_LEN (1024 * 1024) /* 1 MB */

// Length of a level 0 Unix extended area.
#define LEVEL_0_UNIX_EXTENDED_LEN 12 /* bytes */

// Length of a level 0 OS-9 extended area.
#define LEVEL_0_OS9_EXTENDED_LEN 22 /* bytes */

// Extended header types:

#define LHA_EXT_HEADER_COMMON              0x00
#define LHA_EXT_HEADER_FILENAME            0x01
#define LHA_EXT_HEADER_PATH                0x02
#define LHA_EXT_HEADER_MULTI_DISC          0x39
#define LHA_EXT_HEADER_COMMENT             0x3f

#define LHA_EXT_HEADER_WINDOWS_TIMESTAMPS  0x41

#define LHA_EXT_HEADER_UNIX_PERMISSION     0x50
#define LHA_EXT_HEADER_UNIX_UID_GID        0x51
#define LHA_EXT_HEADER_UNIX_GROUP          0x52
#define LHA_EXT_HEADER_UNIX_USER           0x53
#define LHA_EXT_HEADER_UNIX_TIMESTAMP      0x54

#define LHA_EXT_HEADER_OS9                 0xcc

#endif /* #ifndef LHASA_HEADER_DEFS_H */

