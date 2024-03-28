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

//
// Data structure used to read bits from an input source as a stream.
//
// This file is designed to be #included by other source files to
// make a complete decoder.
//

typedef struct {
	uint32_t bit_buffer;
	int bits;
} BitStreamWriter;

static void bit_stream_writer_init(BitStreamWriter *writer)
{
	writer->bits = 0;
	writer->bit_buffer = 0;
}

static int write_bits(BitStreamWriter *writer, unsigned int bits,
                      unsigned int n)
{
	if (writer->bits + n > 8 * sizeof(writer->bit_buffer)) {
		return 0;
	}

	writer->bit_buffer = (writer->bit_buffer << n) | bits;
	writer->bits += n;

	return 1;
}

static unsigned int flush_bytes(BitStreamWriter *writer, uint8_t *buf, size_t n)
{
	unsigned int result = 0;
	unsigned int b;

	while (result < n && writer->bits >= 8) {
		b = writer->bit_buffer >> (writer->bits - 8);
		buf[result] = b & 0xff;
		writer->bits -= 8;
		++result;
	}

	return result;
}
