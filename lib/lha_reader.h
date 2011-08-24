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

#ifndef LHASA_LHA_READER_H
#define LHASA_LHA_READER_H

#include "lha_input_stream.h"
#include "lha_file_header.h"
#include "lha_decoder.h"

typedef struct _LHAReader LHAReader;

/**
 * Policy for extracting directories.
 *
 * When extracting a directory, some of the metadata associated with
 * it need to be set after the contents of the directory have been
 * extracted. This includes the modification time (which would
 * otherwise be reset to the current time) and the permissions (which
 * can affect the ability to extract files into the directory).
 * To work around this problem there are several ways of handling
 * directory extraction.
 */

typedef enum {

	/**
	 * "Plain" policy. In this mode, the metadata is set at the
	 * same time that the directory is created. This is the
	 * simplest to comprehend, and the files returned from
	 * @ref lha_reader_next_file will match the files in the
	 * archive, but it is not recommended.
	 */

	LHA_READER_DIR_PLAIN,

	/**
	 * "End of directory" policy. In this mode, if a directory
	 * is extracted, the directory name will be saved. Once the
	 * contents of the directory appear to have been extracted
	 * (ie. a file is found that is not within the directory),
	 * the directory will be returned again by
	 * @ref lha_reader_next_file. This time, when the directory
	 * is "extracted" (via @ref lha_reader_extract), the metadata
	 * will be set.
	 *
	 * This method uses less memory than
	 * @ref LHA_READER_DIR_END_OF_FILE, but there is the risk
	 * that a file will appear within the archive after the
	 * metadata has been set for the directory. However, this is
	 * not normally the case, as files and directories typically
	 * appear within an archive in order. GNU tar uses the same
	 * method to address this problem with tar files.
	 *
	 * This is the default policy.
	 */

	LHA_READER_DIR_END_OF_DIR,

	/**
	 * "End of file" policy. In this mode, each directory that
	 * is extracted is recorded in a list. When the end of the
	 * archive is reached, these directories are returned again by
	 * @ref lha_reader_next_file. When the directories are
	 * "extracted" again (via @ref lha_reader_extract), the
	 * metadata is set.
	 *
	 * This avoids the problems that can potentially occur with
	 * @ref LHA_READER_DIR_END_OF_DIR, but uses more memory.
	 */

	LHA_READER_DIR_END_OF_FILE

} LHAReaderDirPolicy;

/**
 * Create a new LHA reader to read data from an input stream.
 *
 * @param stream     The input stream to read from.
 * @return           Pointer to an LHAReader structure, or NULL for error.
 */

LHAReader *lha_reader_new(LHAInputStream *stream);

/**
 * Free an LHA reader.
 *
 * @param reader     The LHAReader structure.
 */

void lha_reader_free(LHAReader *reader);

/**
 * Set the policy used to extract directories.
 *
 * @param reader     The LHAReader structure.
 * @param policy     The policy to use for directories.
 */

void lha_reader_set_dir_policy(LHAReader *reader,
                               LHAReaderDirPolicy policy);

/**
 * Read the header of the next archived file from the input stream.
 *
 * @param reader     The LHAReader structure.
 * @return           Pointer to an LHAFileHeader structure, or NULL if
 *                   an error occurred.  This pointer is only valid until
 *                   the next time that lha_reader_next_file is called.
 */

LHAFileHeader *lha_reader_next_file(LHAReader *reader);

/**
 * Read some of the data for the current archived file, decompressing
 * as appropriate.
 *
 * @param reader     The LHAReader structure.
 * @param buf        Pointer to the buffer in which to store the data.
 * @param buf_len    Size of the buffer, in bytes.
 */

size_t lha_reader_read(LHAReader *reader, void *buf, size_t buf_len);

/**
 * Decompress the contents of the current archived file, and check
 * that the checksum matches correctly.
 *
 * @param reader         The LHAReader structure.
 * @param callback       Callback function to invoke to monitor progress (or
 *                       NULL if progress does not need to be monitored).
 * @param callback_data  Extra data to pass to the callback function.
 * @return               Non-zero if the checksum matches.
 */

int lha_reader_check(LHAReader *reader,
                     LHADecoderProgressCallback callback,
                     void *callback_data);

/**
 * Extract the contents of the current archived file.
 *
 * @param reader         The LHAReader structure.
 * @param filename       Filename to extract the archived file to, or NULL
 *                       to use the path and filename from the header.
 * @param callback       Callback function to invoke to monitor progress (or
 *                       NULL if progress does not need to be monitored).
 * @param callback_data  Extra data to pass to the callback function.
 * @return               Non-zero for success, or zero for failure (including
 *                       CRC error).
 */

int lha_reader_extract(LHAReader *reader,
                       char *filename,
                       LHADecoderProgressCallback callback,
                       void *callback_data);

#endif /* #ifndef LHASA_LHA_READER_H */

