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

// Code for handling MacBinary headers.
//
// Classic Mac OS attaches more metadata to files than other operating
// systems. For example, each file has a file type that is used to
// determine the application to open it with. Files can also have both
// a data fork and a resource fork. Because of this design, when
// transferring a file between computers (eg. over a network), all of
// the data associated with the file must be bundled up together to
// preserve the file.
//
// MacLHA uses the MacBinary container format to do this. Within the
// compressed data, the file contents are preceded by a 128 byte
// header that contains the metadata. The data from the data fork can
// also be followed by the data from the resource fork.
//
// Because this is incompatible with .lzh archives from other operating
// systems, MacLHA has two menu items to create new archives - one
// creates a "Mac" archive, while the other creates a "non-Mac"
// (standard) archive that contains just the file contents. This quote
// from the documentation (MacLHAE.doc) describes what is stored when
// the latter option is used:
//
// > If a file has only either Data Fork or Resource Fork, it's stored
// > into archives. In case a file has both Data Fork and Resource Fork,
// > only the Data Fork is stored.
//
// --
//
// Mac OS X has essentially abandoned this practise of using filesystem
// metadata and other systems do not use it, either. It is therefore
// sensible and desirable to strip off the MacBinary header (if present)
// and extract just the normal file contents. It makes sense to use the
// same strategy quoted above.
//
// The possible presence of a MacBinary header can be inferred using the
// OS type field from the LHA header - a value of 'm' indicates that it
// was generated by MacLHA. However, there are some issues with this:
//
// 1. This type is set regardless of whether a MacBinary header is
//    attached or not. There is no other field to indicate the
//    difference, and MacBinary headers do not have a magic number, so
//    the presence of one must be determined heuristically.
//    Realistically, though, this can be done without too much
//    difficulty, by strictly checking all the fields in the MacBinary
//    header. If an invalid header is seen, it can be rejected and
//    assumed to be a normal file.
//
// 2. MacBinary is a standard container format for transferring files
//    between Macs and not used just by MacLHA. Therefore, it is
//    plausible that a .lzh archive might "deliberately" contain a
//    MacBinary file, in which case it would be a mistake to strip
//    off the header.
//
//    This is an unlikely but still a plausible scenario. It can be
//    mitigated by comparing the MacBinary header values against the
//    values from the .lzh header. A header added by MacLHA will have
//    a filename that matches the .lzh header's filename (MacBinary
//    files usually have a .bin extension appended, so the filenames
//    would not match. Also, the modification timestamp should match
//    the timestamp from the .lzh header.
//
// 3. Comparing the MacBinary header modification timestamp with the
//    .lzh header modification timestamp is complicated by the fact
//    that the former is stored as a Macintosh 1904-based timestamp
//    in the local timezone, while the latter is stored as a Unix
//    timestamp in UTC time. Although converting timestamp formats
//    is trivial, the two do not compare exactly due to the timezone
//    offset.
//
// --
//
// Summary of MacBinary header fields and policy for each
// (Z = check zero, C = check value, I = ignore):
//
// 0x00      - Z - "Old version number", must be zero for compatibility
// 0x01      - C - Filename length, must match .lzh header filename.
// 0x02-0x40 - C - Filename, must match .lzh header filename.
//             Z - Remainder following filename contents must be zero
// 0x41-0x44 - I - File type
// 0x45-0x48 - I - File creator
// 0x49      - I - Finder flags
// 0x4a      - Z - "Must be zero for compatibility"
// 0x4b-0x4c - I - Icon vertical position
// 0x4d-0x4e - I - Icon horizontal position
// 0x4f-0x50 - I - Window ID
// 0x51      - I - "Protected" flag
// 0x52      - Z - "Must be zero for compatibility"
// 0x53-0x56 - C - Data fork length     }- added together, equal uncompressed
// 0x57-0x5a - C - Resource fork length }- data length rounded up to 256
// 0x5b-0x5e - I - File creation date
// 0x5f-0x62 - C - File modification date - should match .lzh header
// 0x63-0x64 - Z - Finder "Get Info" comment length - unused by MacLHA
// 0x65-0x7f - Z - MacBinary II data - unused by MacLHA

#include <stdlib.h>
#include <string.h>

#include "lha_decoder.h"
#include "lha_endian.h"
#include "lha_file_header.h"

#define OUTPUT_BUFFER_SIZE 4096 /* bytes */

// Classic Mac OS represents time in seconds since 1904, instead of
// Unix time's 1970 epoch. This is the difference between the two.

#define MAC_TIME_OFFSET 2082844800 /* seconds */

// Size of the MacBinary header.

#define MBHDR_SIZE 128 /* bytes */

// Offsets of fields in MacBinary header (and their sizes):

#define MBHDR_OFF_VERSION           0x00
#define MBHDR_OFF_FILENAME_LEN      0x01
#define MBHDR_OFF_FILENAME          0x02
#define MBHDR_LEN_FILENAME          63
#define MBHDR_OFF_ZERO_COMPAT1      0x4a
#define MBHDR_OFF_ZERO_COMPAT2      0x52
#define MBHDR_OFF_DATA_FORK_LEN     0x53
#define MBHDR_OFF_RES_FORK_LEN      0x57
#define MBHDR_OFF_FILE_MOD_DATE     0x5f
#define MBHDR_OFF_COMMENT_LEN       0x63
#define MBHDR_OFF_MACBINARY2_DATA   0x65
#define MBHDR_LEN_MACBINARY2_DATA   (MBHDR_SIZE - MBHDR_OFF_MACBINARY2_DATA)

// Check that the given block of data contains only zero bytes.

static int block_is_zero(uint8_t *data, size_t data_len)
{
	unsigned int i;

	for (i = 0; i < data_len; ++i) {
		if (data[i] != 0) {
			return 0;
		}
	}

	return 1;
}

// Check that the specified modification time matches the modification
// time from the file header.

static int check_modification_time(unsigned int mod_time,
                                   LHAFileHeader *header)
{
	unsigned int time_diff;

	// In an ideal world, mod_time should match header->timestamp
	// exactly. However, there's an additional complication
	// because mod_time is local time, not UTC time, so there is
	// a timezone difference.

	if (header->timestamp > mod_time) {
		time_diff = header->timestamp - mod_time;
	} else {
		time_diff = mod_time - header->timestamp;
	}

	// The maximum UTC timezone difference is UTC+14, used in
	// New Zealand and some	other islands in the Pacific.

	if (time_diff > 14 * 60 * 60) {
		return 0;
	}

	// If the world was simpler, all time zones would be exact
	// hour offsets, but in fact, some regions use half or
	// quarter hour offsets. So the difference should be a
	// multiple of 15 minutes. Actually, the control panel in
	// Mac OS allows any minute offset to be configured, but if
	// people are crazy enough to do that, they deserve the
	// brokenness they get as a result. It's preferable to use
	// a 15 minute check rather than a 1 minute check, because
	// this allows MacLHA-added MacBinary headers to be
	// distinguished from archived MacBinary files more reliably.

	//return (time_diff % (15 * 60)) == 0;

	// It turns out the assumption above doesn't hold, and MacLHA
	// does generate archives where the timestamps don't always
	// exactly match. Oh well.

	return 1;
}

// Given the specified data buffer, check whether it has a MacBinary
// header with contents that match the specified .lzh header.

static int is_macbinary_header(uint8_t *data, LHAFileHeader *header)
{
	unsigned int filename_len;
	unsigned int data_fork_len, res_fork_len, expected_len;
	unsigned int mod_time;

	// Check fields in the header that should be zero.

	if (data[MBHDR_OFF_VERSION] != 0
	 || data[MBHDR_OFF_ZERO_COMPAT1] != 0
	 || data[MBHDR_OFF_ZERO_COMPAT2] != 0
	 || !block_is_zero(&data[MBHDR_OFF_COMMENT_LEN], 2)
	 || !block_is_zero(&data[MBHDR_OFF_MACBINARY2_DATA],
	                   MBHDR_LEN_MACBINARY2_DATA)) {
		return 0;
	}

	// Check that the filename matches the filename from the
	// lzh header.

	filename_len = data[MBHDR_OFF_FILENAME_LEN];

	if (filename_len > MBHDR_LEN_FILENAME
	 || filename_len != strlen(header->filename)
	 || memcmp(&data[MBHDR_OFF_FILENAME],
	           header->filename, filename_len) != 0) {
		return 0;
	}

	// Data following the filename must be zero as well.

	if (!block_is_zero(data + MBHDR_OFF_FILENAME + filename_len,
	                   MBHDR_LEN_FILENAME - filename_len)) {
		return 0;
	}

	// Decode data fork / resource fork lengths. Their combined
	// lengths, plus the MacBinary header, should match the
	// compressed data length (rounded up to the nearest 128).

	data_fork_len = lha_decode_be_uint32(&data[MBHDR_OFF_DATA_FORK_LEN]);
	res_fork_len = lha_decode_be_uint32(&data[MBHDR_OFF_RES_FORK_LEN]);

	expected_len = (data_fork_len + res_fork_len + MBHDR_SIZE);

	if (header->length != ((expected_len + 0x7f) & ~0x7f)) {
		return 0;
	}

	// Check modification time.

	mod_time = lha_decode_be_uint32(&data[MBHDR_OFF_FILE_MOD_DATE]);

	if (mod_time < MAC_TIME_OFFSET
	 || !check_modification_time(mod_time - MAC_TIME_OFFSET, header)) {
		return 0;
	}

	return 1;
}

//
// MacBinary "decoder". This reuses the LHADecoder framework to provide
// a "pass-through" decoder that detects and strips the MacBinary header.
//

typedef struct {

	// When the decoder is initialized, the first 128 bytes of
	// data are read into this buffer and analysed. If it is
	// not a MacBinary header, the data must be kept so that it
	// can be returned in the first call to .read().
	// mb_header_bytes contains the number of bytes still to read.

	uint8_t mb_header[MBHDR_SIZE];
	size_t mb_header_bytes;

	// The "inner" decoder used to read the compressed data.

	LHADecoder *decoder;

	// Number of bytes still to read before decode should be
	// terminated.

	size_t stream_remaining;
} MacBinaryDecoder;

// Structure used when initializing a MacBinaryDecoder.

typedef struct {
	LHADecoder *decoder;
	LHAFileHeader *header;
} MacBinaryDecoderClosure;

static int read_macbinary_header(MacBinaryDecoder *decoder,
                                 LHAFileHeader *header)
{
	unsigned int data_fork_len, res_fork_len;
	size_t n, bytes;

	bytes = 0;

	while (bytes < MBHDR_SIZE) {
		n = lha_decoder_read(decoder->decoder,
		                     decoder->mb_header + bytes,
		                     MBHDR_SIZE - bytes);

		// Unexpected EOF?

		if (n == 0) {
			return 0;
		}

		bytes += n;
	}

	// Check if the data that was read corresponds to a MacBinary
	// header that matches the .lzh header. If not, just decode it
	// as a normal stream.

	if (!is_macbinary_header(decoder->mb_header, header)) {
		decoder->mb_header_bytes = bytes;
		return 1;
	}

	// We have a MacBinary header, so skip over it. Decide how
	// long the data stream is (see policy in comment at start
	// of file).

	decoder->mb_header_bytes = 0;

	data_fork_len = lha_decode_be_uint32(
	                   &decoder->mb_header[MBHDR_OFF_DATA_FORK_LEN]);
	res_fork_len = lha_decode_be_uint32(
	                   &decoder->mb_header[MBHDR_OFF_RES_FORK_LEN]);

	if (data_fork_len > 0) {
		decoder->stream_remaining = data_fork_len;
	} else {
		decoder->stream_remaining = res_fork_len;
	}

	return 1;
}

static int macbinary_decoder_init(void *_decoder,
                                  LHADecoderCallback callback,
                                  void *_closure)
{
	MacBinaryDecoder *decoder = _decoder;
	MacBinaryDecoderClosure *closure = _closure;

	decoder->decoder = closure->decoder;
	decoder->mb_header_bytes = 0;
	decoder->stream_remaining = closure->header->length;

	if (closure->header->length >= MBHDR_SIZE
	 && !read_macbinary_header(decoder, closure->header)) {
		return 0;
	}

	return 1;
}

static void decode_to_end(LHADecoder *decoder)
{
	uint8_t buf[128];
	size_t n;

	do {
		n = lha_decoder_read(decoder, buf, sizeof(buf));
	} while (n > 0);
}

static size_t macbinary_decoder_read(void *_decoder, uint8_t *buf)
{
	MacBinaryDecoder *decoder = _decoder;
	size_t result;
	size_t to_read;
	size_t n;

	result = 0;

	// If there is data from the mb_header buffer waiting to be
	// read, add it first.

	if (decoder->mb_header_bytes > 0) {
		memcpy(buf, decoder->mb_header, decoder->mb_header_bytes);
		result = decoder->mb_header_bytes;
		decoder->mb_header_bytes = 0;
	}

	// Read further data, if there is some in the stream still to read.

	to_read = OUTPUT_BUFFER_SIZE - result;

	if (to_read > decoder->stream_remaining) {
		to_read = decoder->stream_remaining;
	}

	n = lha_decoder_read(decoder->decoder, buf + result, to_read);

	decoder->stream_remaining -= n;
	result += n;

	// Once the end of the stream is reached, there may still be
	// data from the inner decoder to decompress. When this happens,
	// run the decoder until the end.

	if (decoder->stream_remaining == 0) {
		decode_to_end(decoder->decoder);
	}

	return result;
}

static const LHADecoderType macbinary_decoder_type = {
	macbinary_decoder_init,
	NULL,
	macbinary_decoder_read,
	sizeof(MacBinaryDecoder),
	OUTPUT_BUFFER_SIZE,
	0,
};

LHADecoder *lha_macbinary_passthrough(LHADecoder *decoder,
                                      LHAFileHeader *header)
{
	MacBinaryDecoderClosure closure;
	LHADecoder *result;

	closure.decoder = decoder;
	closure.header = header;

	result = lha_decoder_new(&macbinary_decoder_type, NULL,
	                         &closure, header->length);

	return result;
}
