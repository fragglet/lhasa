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
// Architecture-specific files for compilation on Unix.
//

#define _GNU_SOURCE
#include "lha_arch.h"

#if LHA_ARCH == LHA_ARCH_UNIX

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <utime.h>
#include <sys/stat.h>
#include <sys/types.h>

// TODO: This file depends on vasprintf(), which is a non-standard
// function (_GNU_SOURCE above). Most modern Unix systems have an
// implementation of it, but develop a compatible workaround for
// operating systems that don't have it.

int lha_arch_vasprintf(char **result, char *fmt, va_list args)
{
	return vasprintf(result, fmt, args);
}

void lha_arch_set_binary(FILE *handle)
{
	// No-op on Unix systems: there is no difference between
	// "text" and "binary" files.
}

int lha_arch_mkdir(char *path, unsigned int unix_perms)
{
	return mkdir(path, unix_perms) == 0;
}

int lha_arch_chown(char *filename, int unix_uid, int unix_gid)
{
	return chown(filename, unix_uid, unix_gid) == 0;
}

int lha_arch_chmod(char *filename, int unix_perms)
{
	return chmod(filename, unix_perms) == 0;
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
	FILE *fstream;
	int fileno;

	// The O_EXCL flag will cause the open() below to fail if the
	// file already exists. Remove it first.

	unlink(filename);

	// If we have file permissions, they must be set after the
	// file is created and UID/GID have been set.  When open()ing
	// the file, create it with minimal permissions granted only
	// to the current user.
	// Use O_EXCL so that symlinks are not followed; this prevents
	// a malicious symlink from overwriting arbitrary filesystem
	// locations.

	fileno = open(filename, O_CREAT|O_WRONLY|O_EXCL, 0600);

	if (fileno < 0) {
		return NULL;
	}

	// Set owner and group.

	if (unix_uid >= 0) {
		if (fchown(fileno, unix_uid, unix_gid) != 0) {
			// On most Unix systems, only root can change
			// ownership. But if we can't change ownership,
			// it isn't a fatal error. So ignore the failure
			// and continue.

			// TODO: Implement some kind of alternate handling
			// here?

			/* close(fileno);
			remove(filename);
			return NULL; */
		}
	}

	// Set file permissions.
	// File permissions must be set *after* owner and group have
	// been set; otherwise, we might briefly be granting permissions
	// to the wrong group.

	if (unix_perms >= 0) {
		if (fchmod(fileno, unix_perms) != 0) {
			close(fileno);
			remove(filename);
			return NULL;
		}
	}

	// Create stdc FILE handle.

	fstream = fdopen(fileno, "wb");

	if (fstream == NULL) {
		close(fileno);
		remove(filename);
		return NULL;
	}

	return fstream;
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
	unlink(path);
	return symlink(target, path) == 0;
}

// Wrapper around readlink() that returns an allocated buffer.
static char *do_readlink(const char *path)
{
	char *buf = NULL, *newbuf;
	size_t next_bufsize = 128;

	for (;;) {
		ssize_t nbytes;

		newbuf = realloc(buf, next_bufsize);
		if (newbuf == NULL) {
			free(buf);
			return NULL;
		}

		buf = newbuf;
		nbytes = readlink(path, buf, next_bufsize);
		if (nbytes < 0) {
			// TODO: Error reporting?
			free(buf);
			return NULL;
		} else if ((size_t) nbytes + 1 < next_bufsize) {
			buf[nbytes] = '\0';
			return buf;
		}

		// String truncated, try again with a bigger string.
		next_bufsize *= 2;
	}
}

int lha_arch_stat(const char *path, LHAFileHeader *header)
{
	struct stat buf;
	size_t len;
	const char *p;

	if (lstat(path, &buf) != 0) {
		return 0;
	}

	// TODO: Populate the filename and path fields, as appropriate.

	header->timestamp = buf.st_mtime;
	header->extra_flags = LHA_FILE_UNIX_PERMS | LHA_FILE_UNIX_UID_GID;
	header->unix_perms = buf.st_mode;
	header->unix_uid = buf.st_uid;
	header->unix_gid = buf.st_gid;

	if (S_ISLNK(buf.st_mode)) {
		header->symlink_target = do_readlink(path);
	} else {
		header->symlink_target = NULL;
	}

	// Directory?
	if (S_ISDIR(buf.st_mode)) {
		len = strlen(path);
		header->filename = NULL;
		header->path = malloc(len + 2);
		if (header->path == NULL) {
			goto fail;
		}
		memcpy(header->path, path, len + 1);
		// Path must end in a /
		if (len > 0 && header->path[len - 1] != '/') {
			header->path[len] = '/';
			header->path[len + 1] = '\0';
		}
		return 1;
	}

	// Normal file, or symlink.
	p = strrchr(path, '/');
	if (p == NULL) {
		header->filename = strdup(path);
		header->path = NULL;

		if (header->filename == NULL) {
			goto fail;
		}
	} else {
		// File is in a subdirectory.
		len = p - path;
		header->filename = strdup(p + 1);
		header->path = malloc(len + 2);

		if (header->filename == NULL || header->path == NULL) {
			goto fail;
		}

		memcpy(header->path, path, len + 1);
		header->path[len + 1] = '\0';
	}

	return 1;

fail:
	free(header->filename);
	free(header->path);
	free(header->symlink_target);
	header->filename = NULL;
	header->path = NULL;
	header->symlink_target = NULL;
	return 0;
}

#endif /* LHA_ARCH_UNIX */
