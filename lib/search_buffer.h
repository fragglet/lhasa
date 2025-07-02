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

#ifndef LHASA_LHA_SEARCH_BUFFER_H
#define LHASA_LHA_SEARCH_BUFFER_H

#define SEARCH_BUFFER_HASH_SIZE  4096

typedef struct {
	uint8_t *history;
	size_t history_pos, history_len;

	uint16_t *hash_chain_prev;
	uint16_t *hash_chain_next;
	uint16_t hash_chain_head[SEARCH_BUFFER_HASH_SIZE];
} SearchBuffer;

typedef struct {
	unsigned int offset;
	unsigned int length;
} SearchResult;

int lha_search_buffer_init(SearchBuffer *b, size_t history_len);
void lha_search_buffer_free(SearchBuffer *b);
void lha_search_buffer_insert(SearchBuffer *b, uint8_t c);
SearchResult lha_search_buffer_search(
	SearchBuffer *b, const uint8_t *s, size_t s_len);

#endif /* #ifndef LHASA_LHA_SEARCH_BUFFER_H */
