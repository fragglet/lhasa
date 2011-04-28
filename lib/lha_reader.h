#ifndef LHASA_LHA_READER_H
#define LHASA_LHA_READER_H

#include "lha_input_stream.h"
#include "lha_file_header.h"

typedef struct _LHAReader LHAReader;

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
 * Read the header of the next archived file from the input stream.
 *
 * @param reader     The LHAReader structure.
 * @return           Pointer to an LHAFileHeader structure, or NULL if
 *                   an error occurred.  This pointer is only valid until
 *                   the next time that lha_reader_next_file is called.
 */

LHAFileHeader *lha_reader_next_file(LHAReader *reader);

/**
 * Read some of the compressed data for the current archived file.
 *
 * @param reader     The LHAReader structure.
 * @param buf        Pointer to the buffer in which to store the data.
 * @param buf_len    Size of the buffer, in bytes.
 */

size_t lha_reader_read_compressed(LHAReader *reader, void *buf, size_t buf_len);

/**
 * Read some of the data for the current archived file, decompressing
 * as appropriate.
 *
 * @param reader     The LHAReader structure.
 * @param buf        Pointer to the buffer in which to store the data.
 * @param buf_len    Size of the buffer, in bytes.
 */

size_t lha_reader_read(LHAReader *reader, void *buf, size_t buf_len);

#endif /* #ifndef LHASA_LHA_READER_H */

