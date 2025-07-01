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


#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "lib/search_buffer.h"

#define TEST_BUFFER_LEN 4096

#define TEST_STRING \
	"Space is big. You just won't believe how vastly, hugely, " \
	"mind-bogglingly big it is. I mean, you may think it's a long " \
	"way down the road to the chemist's, but that's just peanuts " \
	"to space - listen,"

// Custom random number generator, to ensure tests are deterministic.
static unsigned int my_random(void)
{
	static unsigned int state = 1;
	unsigned int result = state;
	state = state * 134775813 + 1;
	return result >> 16;
}

void test_insert_search(void)
{
	SearchBuffer b;
	unsigned int i;
	struct {
		const char *s;
		unsigned int offset, length;
	} tests[] = {
		// Short strings, and not found.
		{"", 0, 0},
		{"a", 0, 0},
		{"aa", 0, 0},
		{"text not found anywhere", 0, 0},

		// Full matches and substring matches.
		{"Space", 196, 5},
		{"big it is", 123, 9},
		{"big it isn't", 123, 9},
		{"pacer", 14, 4},

		// End of buffer. This includes the BANANANANANA.. optimization.
		{"listen,", 7, 7},
		{"listen, maybe", 7, 7},
		{"listen,listen,listen,listen,listen,listen,listen,listen,", 7, 56},
	};

	lha_search_buffer_init(&b, TEST_BUFFER_LEN);

	// At first, none of the tests match.
	for (i = 0; i < sizeof(tests) / sizeof(*tests); ++i) {
		SearchResult r = lha_search_buffer_search(
			&b, (uint8_t *) tests[i].s, strlen(tests[i].s));
		assert(r.offset == 0 && r.length == 0);
	}

	for (i = 0; i < strlen(TEST_STRING); ++i) {
		lha_search_buffer_insert(&b, TEST_STRING[i]);
	}

	// After writing the test string, all tests pass.
	for (i = 0; i < sizeof(tests) / sizeof(*tests); ++i) {
		SearchResult r = lha_search_buffer_search(
			&b, (uint8_t *) tests[i].s, strlen(tests[i].s));

		//printf("%s -> <%d, %d>\n", tests[i].s, r.offset, r.length);
		assert(r.offset == tests[i].offset);
		assert(r.length == tests[i].length);
	}

	// After the buffer wraps around, none of the tests will match again.
	for (i = 0; i < TEST_BUFFER_LEN; ++i) {
		lha_search_buffer_insert(&b, 'z');
	}

	for (i = 0; i < sizeof(tests) / sizeof(*tests); ++i) {
		SearchResult r = lha_search_buffer_search(
			&b, (uint8_t *) tests[i].s, strlen(tests[i].s));
		assert(r.offset == 0 && r.length == 0);
	}

}

void test_long_sequence(void)
{
	SearchBuffer b;
	SearchResult r;
	unsigned int i;

	lha_search_buffer_init(&b, TEST_BUFFER_LEN);

	// We run 10MiB of data through the buffer.
	for (i = 0; i < 10 * 1024 * 1024; ++i) {
		lha_search_buffer_insert(&b, my_random() & 0xff);
	}

	// Now check (almost) every offset in the history buffer and ensure
	// that we can find everything.
	for (i = 0; i < TEST_BUFFER_LEN - 20; ++ i) {
		r = lha_search_buffer_search(&b, b.history + i, 20);
		assert(r.offset == TEST_BUFFER_LEN - i);
		assert(r.length == 20);
	}

	lha_search_buffer_free(&b);
}

int main(int argc, char *argv[])
{
	test_insert_search();
	test_long_sequence();
}
