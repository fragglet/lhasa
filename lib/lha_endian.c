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

#include "lha_endian.h"

uint16_t lha_decode_uint16(uint8_t *buf)
{
	return (uint16_t) (buf[0] | (buf[1] << 8));
}

uint32_t lha_decode_uint32(uint8_t *buf)
{
	return (uint32_t) (buf[0] | (buf[1] << 8)
	                 | (buf[2] << 16) | (buf[3] << 24));
}

uint64_t lha_decode_uint64(uint8_t *buf)
{
	return ((uint64_t) buf[0])
	     | ((uint64_t) buf[1] << 8)
	     | ((uint64_t) buf[2] << 16)
	     | ((uint64_t) buf[3] << 24)
	     | ((uint64_t) buf[4] << 32)
	     | ((uint64_t) buf[5] << 40)
	     | ((uint64_t) buf[6] << 48)
	     | ((uint64_t) buf[7] << 56);
}

uint16_t lha_decode_be_uint16(uint8_t *buf)
{
	return (uint16_t) ((buf[0] << 8) | buf[1]);
}

uint32_t lha_decode_be_uint32(uint8_t *buf)
{
	return (uint32_t) ((buf[0] << 24) | (buf[1] << 16)
	                 | (buf[2] << 8) | buf[3]);
}


void lha_encode_uint16(uint8_t *buf, uint16_t value)
{
	buf[0] = value & 0xff;
	buf[1] = (value >> 8) & 0xff;
}

void lha_encode_uint32(uint8_t *buf, uint32_t value)
{
	buf[0] = value & 0xff;
	buf[1] = (value >> 8) & 0xff;
	buf[2] = (value >> 16) & 0xff;
	buf[3] = (value >> 24) & 0xff;
}

void lha_encode_uint64(uint8_t *buf, uint64_t value)
{
	buf[0] = value & 0xff;
	buf[1] = (value >> 8) & 0xff;
	buf[2] = (value >> 16) & 0xff;
	buf[3] = (value >> 24) & 0xff;
	buf[4] = (value >> 32) & 0xff;
	buf[5] = (value >> 40) & 0xff;
	buf[6] = (value >> 48) & 0xff;
	buf[7] = (value >> 56) & 0xff;
}

