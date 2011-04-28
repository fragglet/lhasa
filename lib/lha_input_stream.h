
#ifndef LHASA_LHA_INPUT_STREAM_H
#define LHASA_LHA_INPUT_STREAM_H

#include <stdio.h>
#include <inttypes.h>

typedef struct _LHAInputStream LHAInputStream;

/**
 * Create new LHA reader to read from the specified FILE.
 *
 * @param stream       The open file from which to read data.
 * @return             Pointer to LHAInputStream or NULL for error.
 */

LHAInputStream *lha_input_stream_new(FILE *stream);

/**
 * Free an LHA input stream object.
 *
 * @param reader       The input stream.
 */

void lha_input_stream_free(LHAInputStream *reader);

/**
 * Read a block of data from the LHA stream, of the specified number
 * of bytes.
 *
 * @param reader       The input stream.
 * @param buf          Pointer to buffer in which to store read data.
 * @param buf_len      Size of buffer, in bytes.
 * @return             Non-zero if buffer was filled, or zero if an
 *                     error occurred, or end of file was reached.
 */

int lha_input_stream_read(LHAInputStream *reader, void *buf, size_t buf_len);

/**
 * Read a byte of data from the LHA stream.
 *
 * @param reader       The input stream.
 * @param result       Pointer to variable in which to store the result.
 * @return             Non-zero for success.
 */

int lha_input_stream_read_byte(LHAInputStream *reader, uint8_t *result);

/**
 * Read a 16-bit integer (little endian) from the LHA stream.
 *
 * @param reader       The input stream.
 * @param result       Pointer to variable in which to store the result.
 * @return             Non-zero for success.
 */

int lha_input_stream_read_short(LHAInputStream *reader, uint16_t *result);

/**
 * Read a 32-bit integer (little endian) from the LHA stream.
 *
 * @param reader       The input stream.
 * @param result       Pointer to variable in which to store the result.
 * @return             Non-zero for success.
 */

int lha_input_stream_read_long(LHAInputStream *reader, uint32_t *result);

/**
 * Skip over the specified number of bytes.
 *
 * @param reader       The input stream.
 * @param bytes        Number of bytes to skip.
 * @return             Non-zero for success, zero for failure.
 */

int lha_input_stream_skip(LHAInputStream *reader, size_t bytes);

#endif /* #ifndef LHASA_LHA_INPUT_STREAM_H */

