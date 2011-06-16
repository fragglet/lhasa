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

//
// Data structure used to read bits from an input source as a stream.
//
// This file is designed to be #included by other source files to
// make a complete decoder.
//

typedef struct {

	// Callback function to invoke to read more data from the
	// input stream.

	LHADecoderCallback callback;
	void *callback_data;

	// Bits from the input stream that are waiting to be read.

	uint32_t bit_buffer;
	unsigned int bits;

} BitStreamReader;

// Initialize bit stream reader structure.

static void bit_stream_reader_init(BitStreamReader *reader,
                                   LHADecoderCallback callback,
                                   void *callback_data)
{
	reader->callback = callback;
	reader->callback_data = callback_data;

	reader->bits = 0;
	reader->bit_buffer = 0;
}

// Return the next n bits waiting to be read from the input stream,
// without removing any.

static int peek_bits(BitStreamReader *reader,
		     unsigned int n,
                     unsigned int *result)
{
	uint8_t buf[3];
	size_t bytes;

	// Always try to keep at least 8 bits in the input buffer.
	// When the level drops low, read some more bytes to top it up.

	if (reader->bits < 8) {
		bytes = reader->callback(buf, 3, reader->callback_data);

		reader->bit_buffer |= (uint32_t) buf[0] << (24 - reader->bits);
		reader->bit_buffer |= (uint32_t) buf[1] << (16 - reader->bits);
		reader->bit_buffer |= (uint32_t) buf[2] << (8 - reader->bits);

		reader->bits += bytes * 8;
	}

	if (reader->bits < n) {
		return 0;
	}

	*result = reader->bit_buffer >> (32 - n);

	return 1;
}

// Read a bit from the input stream.
// Returns true on success and sets *result.

static int read_bits(BitStreamReader *reader,
                     unsigned int n,
                     unsigned int *result)
{
	if (!peek_bits(reader, n, result)) {
		return 0;
	}

	reader->bit_buffer <<= n;
	reader->bits -= n;

	return 1;
}


// Read a bit from the input stream.
// Returns true on success and sets *result.

static int read_bit(BitStreamReader *reader,
                    unsigned int *result)
{
	return read_bits(reader, 1, result);
}

