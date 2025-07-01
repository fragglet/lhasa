/*

Copyright (c) 2025 Simon Howard

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

/* This is an implementation of a searchable ring buffer, that stores the
   last {N} bytes of processed data, and allows efficient searches for
   substrings found in the buffer. This is used during compression to
   generate the copy commands.  */

#include <stdlib.h>
#include <stdint.h>

#include "search_buffer.h"

#define HASH_CHAIN_END  0xffff

int lha_search_buffer_init(SearchBuffer *b, size_t history_len)
{
	unsigned int i;

	b->history = calloc(history_len, 1);
	b->history_pos = 0;
	b->history_len = history_len;
	b->hash_chain_next = calloc(history_len, sizeof(uint16_t));

	if (b->history == NULL || b->hash_chain_next == NULL) {
		free(b->history);
		free(b->hash_chain_next);
		return 0;
	}

	for (i = 0; i < history_len; ++i) {
		b->hash_chain_next[i] = HASH_CHAIN_END;
	}
	for (i = 0; i < SEARCH_BUFFER_HASH_SIZE; ++i) {
		b->hash_chain_head[i] = HASH_CHAIN_END;
	}

	return 1;
}

void lha_search_buffer_free(SearchBuffer *b)
{
	free(b->history);
	free(b->hash_chain_next);
}

// For the hash function we look at three character prefixes. This is the
// minimum copy length for the lha algorithms (see COPY_THRESHOLD in
// lh1_common.h), and using all three characters gives maximum entropy.
static uint16_t hash_func(uint8_t x, uint8_t y, uint8_t z)
{
	uint16_t result = 5381;

	result = (result << 5) + result + x;
	result = (result << 5) + result + y;
	result = (result << 5) + result + z;

	return result % SEARCH_BUFFER_HASH_SIZE;
}

static uint16_t hash_at_position(SearchBuffer *b, unsigned int idx)
{
	uint8_t x, y, z;

	x = b->history[idx];
	idx = (idx + 1) % b->history_len;
	y = b->history[idx];
	idx = (idx + 1) % b->history_len;
	z = b->history[idx];

	//printf("hash at %d: %02x, %02x, %02x\n", idx, x, y, z);

	return hash_func(x, y, z);
}

static void unhook(SearchBuffer *b, unsigned int idx)
{
	uint16_t hash = hash_at_position(b, idx);
	uint16_t *rover;

	//printf("unhook at %d (hash %d)\n", idx, hash);
	rover = &b->hash_chain_head[hash];
	//printf("\thead[%d] = %d\n", hash, *rover);
	while (*rover != HASH_CHAIN_END) {
		if (*rover == idx) {
			*rover = b->hash_chain_next[idx];
			b->hash_chain_next[idx] = HASH_CHAIN_END;
			return;
		}
		//printf("\tnext[%d] = %d\n", *rover, b->hash_chain_next[*rover]);
		rover = &b->hash_chain_next[*rover];
	}
}

static void hook(SearchBuffer *b, unsigned int idx)
{
	uint16_t hash = hash_at_position(b, idx);

	//printf("hook at %d (hash %d)\n", idx, hash);
	//printf("\tnext[%d] = %d\n", idx, b->hash_chain_head[hash]);
	//printf("\thead[%d] = %d\n", hash, idx);

	b->hash_chain_next[idx] = b->hash_chain_head[hash];
	b->hash_chain_head[hash] = idx;
}

void lha_search_buffer_insert(SearchBuffer *b, uint8_t c)
{
	// We must first invalidate whatever was previously in the buffer.
	unhook(b, b->history_pos);

	// Once the history buffer has been updated, there's now a new
	// substring to hook in, but we start two bytes back (it's the
	// last byte of the three-byte triple that has changed).
	b->history[b->history_pos] = c;
	hook(b, (b->history_pos + b->history_len - 2) % b->history_len);

	b->history_pos = (b->history_pos + 1) % b->history_len;
}

static size_t substring_match_len(const uint8_t *s, size_t s_len, size_t start)
{
	unsigned int i = 0;

	while (start + i < s_len && s[i] == s[start + i]) {
		++i;
	}

	return i;
}

// Returns the number of bytes that match between `s` and the string
// that starts at b->history[idx] in the search buffer.
static size_t match_len(SearchBuffer *b, unsigned int idx, const uint8_t *s,
                        size_t s_len)
{
	uint8_t c;
	uint16_t check_idx = idx;
	unsigned int i;

	for (i = 0; i < s_len; ++i) {
		c = b->history[check_idx];
		if (c != s[i]) {
			return i;
		}
		// Stop once we reach the latest byte in the history buffer.
		// Note that we don't stop here, but check for an overlapping
		// substring match if the search string repeats (the
		// BANANANANANA... case).
		check_idx = (check_idx + 1) % b->history_len;
		if (check_idx == b->history_pos) {
			return i + 1
			     + substring_match_len(s, s_len, i + 1);
		}
	}

	return s_len;
}

SearchResult lha_search_buffer_search(SearchBuffer *b, const uint8_t *s,
                                      size_t s_len)
{
	SearchResult result = {0, 0};
	int hash;
	uint16_t idx;

	if (s_len < 3) {
		return result;
	}

	// Walk along the hash chain, checking for matches at each index
	// in the chain. We return the longest matching substring.
	hash = hash_func(s[0], s[1], s[2]);
	idx = b->hash_chain_head[hash];
	while (idx != HASH_CHAIN_END && result.length < s_len) {
		size_t this_match_len = match_len(b, idx, s, s_len);

		//printf("\tchecked at position %d, got match length %d\n",
		//       idx, this_match_len);

		if (this_match_len > result.length) {
			result.offset = idx < b->history_pos ? b->history_pos - idx :
			                b->history_pos + b->history_len - idx;
			result.length = this_match_len;
		}
		idx = b->hash_chain_next[idx];
	}

	return result;
}
