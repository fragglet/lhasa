/*

Copyright (c) 2012, Simon Howard

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

#ifndef LHASA_LHA_ARCH_H
#define LHASA_LHA_ARCH_H

#include <stdio.h>

#define LHA_ARCH_UNIX     1
#define LHA_ARCH_WINDOWS  2

#ifdef _WIN32
#define LHA_ARCH LHA_ARCH_WINDOWS
#else
#define LHA_ARCH LHA_ARCH_UNIX
#endif

/**
 * Create a directory.
 *
 * @param path        Path to the directory to create.
 * @param unix_perms  Unix permissions for the directory to create.
 * @return            Non-zero if the directory was created successfully.
 */

int lha_arch_mkdir(char *path, unsigned int unix_perms);

/**
 * Change the Unix ownership of the specified file or directory.
 * If this is not a Unix system, do nothing.
 *
 * @param filename   Path to the file or directory.
 * @param unix_uid   The UID to set.
 * @param unix_gid   The GID to set.
 * @return           Non-zero if set successfully.
 */

int lha_arch_chown(char *filename, int unix_uid, int unix_gid);

/**
 * Change the Unix permissions on the specified file or directory.
 *
 * @param filename    Path to the file or directory.
 * @param unix_perms  The permissions to set.
 * @return            Non-zero if set successfully.
 */

int lha_arch_chmod(char *filename, int unix_perms);

/**
 * Set the file creation / modification time on the specified file or
 * directory.
 *
 * @param filename    Path to the file or directory.
 * @param timestamp   The Unix timestamp to set.
 * @return            Non-zero if set successfully.
 */

int lha_arch_utime(char *filename, unsigned int timestamp);

/**
 * Open a new file for writing.
 *
 * @param filename    Path to the file or directory.
 * @param unix_uid    Unix UID to set for the new file, or -1 to not set.
 * @param unix_gid    Unix GID to set for the new file, or -1 to not set.
 * @param unix_perms  Unix permissions to set for the new file, or -1 to not
 *                    set.
 */

FILE *lha_arch_fopen(char *filename, int unix_uid,
                     int unix_gid, int unix_perms);

#endif /* ifndef LHASA_LHA_ARCH_H */

