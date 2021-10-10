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

//
// Architecture-specific files for compilation on Amiga.
//

#define _GNU_SOURCE
#include "lha_arch.h"

#if LHA_ARCH == LHA_ARCH_AMIGA

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <utime.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>

int lha_arch_vasprintf(char **result, char *fmt, va_list args)
{
	int len;
	va_list args2;
	va_copy(args2, args);
	char szTmp[2];
	len = vsnprintf(szTmp, 2, fmt, args2);
	va_end(args2);
	if (len >= 0) {
		*result = malloc(len + 1);
		if (*result != NULL) {
			va_copy(args2, args);
			return vsprintf(*result, fmt, args);
			va_end(args2);
		}
	}
	*result = NULL;
	return -1;
}

void lha_arch_set_binary(FILE *handle)
{
	// No-op on Amiga systems: there is no difference between
	// "text" and "binary" files.
}

int lha_arch_mkdir(char *path, unsigned int unix_perms)
{
	return mkdir(path, unix_perms) == 0;
}

int lha_arch_chown(char *filename, int unix_uid, int unix_gid)
{
	return 1;
}

int lha_arch_chmod(char *filename, int unix_perms)
{
	return 1;
}

int lha_arch_utime(char *filename, unsigned int timestamp)
{
	struct utimbuf times;

	times.actime = (time_t) timestamp;
	times.modtime = (time_t) timestamp;

	return utime(filename, &times) == 0;
}

FILE *lha_arch_fopen(char *filename, int unix_uid, int unix_gid, int unix_perms)
{
	return fopen(filename, "wb");
}

LHAFileType lha_arch_exists(char *filename)
{
	struct stat statbuf;

	if (stat(filename, &statbuf) != 0) {
		if (errno == ENOENT) {
			return LHA_FILE_NONE;
		} else {
			return LHA_FILE_ERROR;
		}
	}

	if (S_ISDIR(statbuf.st_mode)) {
		return LHA_FILE_DIRECTORY;
	} else {
		return LHA_FILE_FILE;
	}
}

int lha_arch_symlink(char *path, char *target)
{
	return 1;
}

#endif /* LHA_ARCH_AMIGA */


