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
// Decoder for PMarc -pm2- compression format.  PMarc is a variant
// of LHA commonly used on the MSX computer architecture.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "lha_decoder.h"

#include "bit_stream_reader.c"

// Size of the ring buffer (in bytes) used to store past history
// for copies.

#define RING_BUFFER_SIZE    8192

// Upper bit is set in a node value to indicate a leaf.

#define TREE_NODE_LEAF      0x80

// Maximum number of bytes that might be placed in the output buffer
// from a single call to lha_pma_decoder_read (largest copy size).

#define OUTPUT_BUFFER_SIZE  256

typedef enum
{
	PMA_REBUILD_UNBUILT,          // At start of stream
	PMA_REBUILD_BUILD1,           // After 1KiB
	PMA_REBUILD_BUILD2,           // After 2KiB
	PMA_REBUILD_BUILD3,           // After 4KiB
	PMA_REBUILD_CONTINUING,       // 8KiB onwards...
} PMARebuildState;

typedef struct
{
	uint8_t prev;
	uint8_t next;
} HistoryNode;

typedef struct
{
	BitStreamReader bit_stream_reader;

	// State of decode tree.

	PMARebuildState tree_state;

	// Number of bytes until we initiate a tree rebuild.

	size_t tree_rebuild_remaining;

	// History ring buffer, for copies:

	uint8_t ringbuf[RING_BUFFER_SIZE];
	unsigned int ringbuf_pos;

	// History linked list. In the decode stream, codes representing
	// characters are not the character itself, but the number of
	// nodes to count back in time in the linked list. Every time
	// a character is output, it is moved to the front of the linked
	// list. The entry point index into the list is the last output
	// character, given by history_head;

	HistoryNode history[256];
	uint8_t history_head;

	// Array representing the huffman tree used for representing
	// code values. A given node of the tree has children
	// code_tree[n] and code_tree[n + 1].  code_tree[0] is the
	// root node.

	uint8_t code_tree[65];

	// If zero, we don't need an offset tree:

	int need_offset_tree;

	// Array representing huffman tree used to look up offsets.
	// Same format as code_tree[].

	uint8_t offset_tree[17];

} LHAPMADecoder;

// Structure used to hold data needed to build the tree.

typedef struct
{
	// The tree data and its size (must not be exceeded)

	uint8_t *tree;
	unsigned int tree_len;

	// Counter used to allocate entries from the tree.
	// Every time a new node is allocated, this increase by 2.

	unsigned int tree_allocated;

	// Circular buffer of available tree entries.  These are
	// indices into tree[], and either reference a tree node's
	// child pointer (left or right) or the root node pointer.
	// As we add leaves to the tree, they are read from here.

	uint8_t entries[32];

	// The next tree entry.

	unsigned int next_entry;

	// The number of entries in the queue.

	unsigned int entries_len;
} TreeBuildData;

typedef struct
{
	unsigned int offset;
	unsigned int bits;
} VariableLengthTable;

// Decode table for history value. Characters that appeared recently in
// the history are more likely than ones that appeared a long time ago,
// so the history value is huffman coded so that small values require
// fewer bits. The history value is then used to search within the
// history linked list to get the actual character.

static const VariableLengthTable history_decode[] = {
	{   0, 3 },   //   0 + (1 << 3) =   8
	{   8, 3 },   //   8 + (1 << 3) =  16
	{  16, 4 },   //  16 + (1 << 4) =  32
	{  32, 5 },   //  32 + (1 << 5) =  64
	{  64, 5 },   //  64 + (1 << 5) =  96
	{  96, 5 },   //  96 + (1 << 5) = 128
	{ 128, 6 },   // 128 + (1 << 6) = 192
	{ 192, 6 },   // 192 + (1 << 6) = 256
};

// Decode table for copies. As with history_decode[], small copies
// are more common, and require fewer bits.

static const VariableLengthTable copy_decode[] = {
	{  17, 3 },   //  17 + (1 << 3) =  25
	{  25, 3 },   //  25 + (1 << 3) =  33
	{  33, 5 },   //  33 + (1 << 5) =  65
	{  65, 6 },   //  65 + (1 << 6) = 129
	{ 129, 7 },   // 129 + (1 << 7) = 256
	{ 256, 0 },   // 256 (unique value)
};

// Initialize the history buffer.

static void init_history(LHAPMADecoder *decoder)
{
	unsigned int i;

	// History buffer is initialized to a linear chain to
	// start off with:

	for (i = 0; i < 256; ++i) {
		decoder->history[i].prev = (uint8_t) (i + 1);
		decoder->history[i].next = (uint8_t) (i - 1);
	}

	// The chain is cut into groups and initially arranged so
	// that the ASCII characters are closest to the start of
	// the chain. This is followed by ASCII control characters,
	// then various other groups.

	decoder->history_head = 0x20;

	decoder->history[0x7f].prev = 0x00;  // 0x20 ... 0x7f -> 0x00
	decoder->history[0x00].next = 0x7f;

	decoder->history[0x1f].prev = 0xa0;  // 0x00 ... 0x1f -> 0xa0
	decoder->history[0xa0].next = 0x1f;

	decoder->history[0xdf].prev = 0x80;  // 0xa0 ... 0xdf -> 0x80
	decoder->history[0x80].next = 0xdf;

	decoder->history[0x9f].prev = 0xe0;  // 0x80 ... 0x9f -> 0xe0
	decoder->history[0xe0].next = 0x9f;

	decoder->history[0xff].prev = 0x20;  // 0xe0 ... 0xff -> 0x20
	decoder->history[0x20].next = 0xff;
}

// Look up an entry in the history chain, returning the code found.

static uint8_t find_in_history(LHAPMADecoder *decoder, uint8_t count)
{
	unsigned int i;
	uint8_t code;

	// Start from the last outputted byte.

	code = decoder->history_head;

	// Walk along the history chain until we reach the desired
	// node.  If we will have to walk more than half the chain,
	// go the other way around.

	if (count < 128) {
		for (i = 0; i < count; ++i) {
			code = decoder->history[code].prev;
		}
	} else {
		for (i = 0; i < 256U - count; ++i) {
			code = decoder->history[code].next;
		}
	}

	return code;
}

// Update history buffer, by moving the specified byte to the head
// of the queue.

static void update_history(LHAPMADecoder *decoder, uint8_t b)
{
	HistoryNode *node, *old_head;

	// No update necessary?

	if (decoder->history_head == b) {
		return;
	}

	// Unhook the entry from its current position:

	node = &decoder->history[b];
	decoder->history[node->next].prev = node->prev;
	decoder->history[node->prev].next = node->next;

	// Hook in between the old head and old_head->next:

	old_head = &decoder->history[decoder->history_head];
	node->prev = decoder->history_head;
	node->next = old_head->next;

	decoder->history[old_head->next].prev = b;
	old_head->next = b;

	// 'b' is now the head of the queue:

	decoder->history_head = b;
}

// Initialize PMA decoder.

static int lha_pma_decoder_init(void *data, LHADecoderCallback callback,
                                void *callback_data)
{
	LHAPMADecoder *decoder = data;

	bit_stream_reader_init(&decoder->bit_stream_reader,
	                       callback, callback_data);

	// Tree has not been built yet.  It needs to be built on
	// the first call to read().

	decoder->tree_state = PMA_REBUILD_UNBUILT;
	decoder->tree_rebuild_remaining = 0;

	// Initialize ring buffer contents.

	memset(&decoder->ringbuf, ' ', RING_BUFFER_SIZE);
	decoder->ringbuf_pos = 0;

	// Init history lookup list.

	init_history(decoder);

	// Initialize the lookup trees to a known state.

	memset(&decoder->code_tree, TREE_NODE_LEAF,
	       sizeof(decoder->code_tree));
	memset(&decoder->offset_tree, TREE_NODE_LEAF,
	       sizeof(decoder->offset_tree));

	return 1;
}

// Add an entry to the tree entry queue.

static void add_queue_entry(TreeBuildData *build, uint8_t index)
{
	if (build->entries_len >= 32) {
		return;
	}

	build->entries[(build->next_entry + build->entries_len) % 32] = index;
	++build->entries_len;
}

// Read an entry from the tree entry queue.

static uint8_t read_queue_entry(TreeBuildData *build)
{
	uint8_t result;

	if (build->entries_len == 0) {
		return 0;
	}

	result = build->entries[build->next_entry];
	build->next_entry = (build->next_entry + 1) % 32;
	--build->entries_len;

	return result;
}

// "Expand" the list of queue entries. This generates a new child
// node at each of the entries currently in the queue, adding the
// children of those nodes into the queue to replace them.
// The effect of this is to add an extra level to the tree, and
// to increase the tree depth of the indices in the queue.

static void expand_queue(TreeBuildData *build)
{
	unsigned int num_nodes, i;
	uint8_t node, entry_index;

	num_nodes = build->entries_len;

	for (i = 0; i < num_nodes; ++i) {

		if (build->tree_allocated >= build->tree_len) {
			return;
		}

		// Allocate a new node.

		node = (uint8_t) build->tree_allocated;
		build->tree_allocated += 2;

		// Add into tree at the next available location.

		entry_index = read_queue_entry(build);
		build->tree[entry_index] = node;

		// Add child pointers of this node.

		add_queue_entry(build, node);
		add_queue_entry(build, (uint8_t) (node + 1));
	}
}

// Add all codes to the tree that have the specified length.
// Returns non-zero if there are any entries in code_lengths[] still
// waiting to be added to the tree.

static int add_codes_with_length(TreeBuildData *build,
                                 uint8_t *code_lengths,
                                 unsigned int num_code_lengths,
                                 unsigned int code_len)
{
	unsigned int i;
	unsigned int node;
	int codes_remaining;

	codes_remaining = 0;

	for (i = 0; i < num_code_lengths; ++i) {

		// Does this code belong at this depth in the tree?

		if (code_lengths[i] == code_len) {
			node = read_queue_entry(build);

			build->tree[node] = (uint8_t) i | TREE_NODE_LEAF;
		}

		// More work to be done after this pass?

		else if (code_lengths[i] > code_len) {
			codes_remaining = 1;
		}
	}

	return codes_remaining;
}

// Build a tree, given the specified array of codes indicating the
// required depth within the tree at which each code should be
// located.

static void build_tree(uint8_t *tree, size_t tree_len,
                       uint8_t *code_lengths, unsigned int num_code_lengths)
{
	TreeBuildData build;
	unsigned int code_len;

	build.tree = tree;
	build.tree_len = tree_len;

	// Start with a single entry in the queue - the root node
	// pointer.

	build.entries[0] = 0;
	build.next_entry = 0;
	build.entries_len = 1;

	// We always have the root ...

	build.tree_allocated = 1;

	// Iterate over each possible code length.
	// Note: code_len == 0 is deliberately skipped over, as 0
	// indicates "not used".

	code_len = 0;

	do {
		// Advance to the next code length by allocating extra
		// nodes to the tree - the slots waiting in the queue
		// will now be one level deeper in the tree (and the
		// codes 1 bit longer).

		expand_queue(&build);
		++code_len;

		// Add all codes that have this length.

	} while (add_codes_with_length(&build, code_lengths,
		                       num_code_lengths, code_len));
}

/*
static void display_tree(uint8_t *tree, unsigned int node, int offset)
{
	unsigned int i;

	if (node & TREE_NODE_LEAF) {
		for (i = 0; i < offset; ++i) putchar(' ');
		printf("leaf %i\n", node & ~TREE_NODE_LEAF);
	} else {
		for (i = 0; i < offset; ++i) putchar(' ');
		printf("0 ->\n");
		display_tree(tree, tree[node], offset + 4);
		for (i = 0; i < offset; ++i) putchar(' ');
		printf("1 ->\n");
		display_tree(tree, tree[node + 1], offset + 4);
	}
}
*/

// Read the list of code lengths to use for the code tree and construct
// the code_tree structure.

static int read_code_tree(LHAPMADecoder *decoder)
{
	uint8_t code_lengths[31];
	int num_codes, min_code_length, length_bits, val;
	unsigned int i;

	// Read the number of codes in the tree.

	num_codes = read_bits(&decoder->bit_stream_reader, 5);

	// Read min_code_length, which is used as an offset.

	min_code_length = read_bits(&decoder->bit_stream_reader, 3);

	if (min_code_length < 0 || num_codes < 0) {
		return 0;
	}

	// Store flag variable indicating whether we want to read
	// the offset tree as well.

	decoder->need_offset_tree
	    = num_codes >= 10
	   && !(num_codes == 29 && min_code_length == 0);

	// Minimum length of zero means a tree containing a single code.

	if (min_code_length == 0) {
		decoder->code_tree[0]
		  = (uint8_t) (TREE_NODE_LEAF | (num_codes - 1));
		return 1;
	}

	// How many bits are used to represent each table entry?

	length_bits = read_bits(&decoder->bit_stream_reader, 3);

	if (length_bits < 0) {
		return 0;
	}

	// Read table of code lengths:

	for (i = 0; i < (unsigned int) num_codes; ++i) {

		// Read a table entry.  A value of zero represents an
		// unused code.  Otherwise the value represents
		// an offset from the minimum length (previously read).

		val = read_bits(&decoder->bit_stream_reader,
		                (unsigned int) length_bits);

		if (val < 0) {
			return 0;
		} else if (val == 0) {
			code_lengths[i] = 0;
		} else {
			code_lengths[i] = (uint8_t) (min_code_length + val - 1);
		}
	}

	// Build the tree.

	build_tree(decoder->code_tree, sizeof(decoder->code_tree),
	           code_lengths, (unsigned int) num_codes);

	return 1;
}

// Read the code lengths for the offset tree and construct the offset
// tree lookup table.

static int read_offset_tree(LHAPMADecoder *decoder,
                            unsigned int num_offsets)
{
	uint8_t offset_lengths[8];
	unsigned int off;
	unsigned int single_offset, num_codes;
	int len;

	if (!decoder->need_offset_tree) {
		return 1;
	}

	// Read 'num_offsets' 3-bit length values.  For each offset
	// value 'off', offset_lengths[off] is the length of the
	// code that will represent 'off', or 0 if it will not
	// appear within the tree.

	num_codes = 0;
	single_offset = 0;

	for (off = 0; off < num_offsets; ++off) {
		len = read_bits(&decoder->bit_stream_reader, 3);

		if (len < 0) {
			return 0;
		}

		offset_lengths[off] = (uint8_t) len;

		// Track how many actual codes were in the tree.

		if (len != 0) {
			single_offset = off;
			++num_codes;
		}
	}

	// If there was a single code, this is a single node tree.

	if (num_codes == 1) {
		decoder->offset_tree[0]
		  = (uint8_t) (single_offset | TREE_NODE_LEAF);
		return 1;
	}

	// Build the tree.

	build_tree(decoder->offset_tree, sizeof(decoder->offset_tree),
	           offset_lengths, num_offsets);

	return 1;
}

// Rebuild the decode trees used to compress data.  This is called when
// decoder->tree_rebuild_remaining reaches zero.

static void rebuild_tree(LHAPMADecoder *decoder)
{
	switch (decoder->tree_state) {

		// Initial tree build, from start of stream:

		case PMA_REBUILD_UNBUILT:
			read_code_tree(decoder);
			read_offset_tree(decoder, 5);
			decoder->tree_state = PMA_REBUILD_BUILD1;
			decoder->tree_rebuild_remaining = 1024;
			break;

		// Tree rebuild after 1KiB of data has been read:

		case PMA_REBUILD_BUILD1:
			read_offset_tree(decoder, 6);
			decoder->tree_state = PMA_REBUILD_BUILD2;
			decoder->tree_rebuild_remaining = 1024;
			break;

		// Tree rebuild after 2KiB of data has been read:

		case PMA_REBUILD_BUILD2:
			read_offset_tree(decoder, 7);
			decoder->tree_state = PMA_REBUILD_BUILD3;
			decoder->tree_rebuild_remaining = 2048;
			break;

		// Tree rebuild after 4KiB of data has been read:

		case PMA_REBUILD_BUILD3:
			if (read_bit(&decoder->bit_stream_reader) == 1) {
				read_code_tree(decoder);
			}
			read_offset_tree(decoder, 8);
			decoder->tree_state = PMA_REBUILD_CONTINUING;
			decoder->tree_rebuild_remaining = 4096;
			break;

		// Tree rebuild after 8KiB of data has been read,
		// and every 4KiB after that:

		case PMA_REBUILD_CONTINUING:
			if (read_bit(&decoder->bit_stream_reader) == 1) {
				read_code_tree(decoder);
				read_offset_tree(decoder, 8);
			}
			decoder->tree_rebuild_remaining = 4096;
			break;
	}
}

// Read bits from the input stream, traversing the specified tree
// from the root node until we reach a leaf.  The leaf value is
// returned.

static int read_from_tree(LHAPMADecoder *decoder, uint8_t *tree)
{
	uint8_t code;
	int bit;

	// Start from root.

	code = tree[0];

	while ((code & TREE_NODE_LEAF) == 0) {

		bit = read_bit(&decoder->bit_stream_reader);

		if (bit < 0) {
			return -1;
		}

		code = tree[code + (unsigned int) bit];
	}

	// Mask off leaf bit to get the plain code.

	return (int) (code & ~TREE_NODE_LEAF);
}

static void output_byte(LHAPMADecoder *decoder, uint8_t *buf,
                        size_t *buf_len, uint8_t b)
{
	// Add to history ring buffer.

	decoder->ringbuf[decoder->ringbuf_pos] = b;
	decoder->ringbuf_pos = (decoder->ringbuf_pos + 1) % RING_BUFFER_SIZE;

	// Add to output buffer.

	buf[*buf_len] = b;
	++*buf_len;

	// Update history chain.

	update_history(decoder, b);

	// Count down until it is time to perform a rebuild of the
	// lookup trees.

	--decoder->tree_rebuild_remaining;

	if (decoder->tree_rebuild_remaining == 0) {
		rebuild_tree(decoder);
	}
}

// Read a variable length code, given the header bits already read.
// Returns the decoded value, or -1 for error.

static int decode_variable_length(LHAPMADecoder *decoder,
                                  const VariableLengthTable *table,
                                  unsigned int header)
{
	int value;

	value = read_bits(&decoder->bit_stream_reader, table[header].bits);

	if (value < 0) {
		return -1;
	}

	return (int) table[header].offset + value;
}

// Read a single byte from the input stream and add it to the output
// buffer.

static void read_single_byte(LHAPMADecoder *decoder, unsigned int code,
                             uint8_t *buf, size_t *buf_len)
{
	int offset;
	uint8_t b;

	offset = decode_variable_length(decoder, history_decode, code);

	if (offset < 0) {
		return;
	}

	b = find_in_history(decoder, (uint8_t) offset);
	output_byte(decoder, buf, buf_len, b);
}

// Calculate how many bytes from history to copy:

static int history_get_count(LHAPMADecoder *decoder, unsigned int code)
{
	// How many bytes to copy?  A small value represents the
	// literal number of bytes to copy; larger values are a header
	// for a variable length value to be decoded.

	if (code < 15) {
		return (int) code + 2;
	} else {
		return decode_variable_length(decoder, copy_decode, code - 15);
	}
}

// Calculate the offset within history at which to start copying:

static int history_get_offset(LHAPMADecoder *decoder, unsigned int code)
{
	unsigned int bits;
	int result, val;

	result = 0;

	// Calculate number of bits to read.

	// Code of zero indicates a simple 6-bit value giving the offset.

	if (code == 0) {
		bits = 6;
	}

	// Mid-range encoded offset value.
	// Read a code using the offset tree, indicating the length
	// of the offset value to follow.  The code indicates the
	// number of bits (values 0-7 = 6-13 bits).

	else if (code < 20) {

		val = read_from_tree(decoder, decoder->offset_tree);

		if (val < 0) {
			return -1;
		} else if (val == 0) {
			bits = 6;
		} else {
			bits = (unsigned int) val + 5;
			result = 1 << bits;
		}
	}

	// Large copy values start from offset zero.

	else {
		return 0;
	}

	// Read a number of bits representing the offset value.  The
	// number of length of this value is variable, and is calculated
	// above.

	val = read_bits(&decoder->bit_stream_reader, bits);

	if (val < 0) {
		return -1;
	}

	result += val;

	return result;
}

static void copy_from_history(LHAPMADecoder *decoder, unsigned int code,
                              uint8_t *buf, size_t *buf_len)
{
	int to_copy, offset;
	unsigned int i, pos, start;

	// Read number of bytes to copy and offset within history to copy
	// from.

	to_copy = history_get_count(decoder, code);
	offset = history_get_offset(decoder, code);

	if (to_copy < 0 || offset < 0) {
		return;
	}

	// Sanity check to prevent the potential for buffer overflow.

	if (to_copy > OUTPUT_BUFFER_SIZE) {
		return;
	}

	// Perform copy.

	start = decoder->ringbuf_pos + RING_BUFFER_SIZE - 1
	      - (unsigned int) offset;

	for (i = 0; i < (unsigned int) to_copy; ++i) {
		pos = (start + i) % RING_BUFFER_SIZE;

		output_byte(decoder, buf, buf_len, decoder->ringbuf[pos]);
	}
}

// Decode data and store it into buf[], returning the number of
// bytes decoded.

static size_t lha_pma_decoder_read(void *data, uint8_t *buf)
{
	LHAPMADecoder *decoder = data;
	size_t result;
	int code;

	// On first pass through, build initial lookup trees.

	if (decoder->tree_state == PMA_REBUILD_UNBUILT) {

		// First bit in stream is discarded?

		read_bit(&decoder->bit_stream_reader);
		rebuild_tree(decoder);
	}

	result = 0;

	code = read_from_tree(decoder, decoder->code_tree);

	if (code < 0) {
		return 0;
	}

	if (code < 8) {
		read_single_byte(decoder, (unsigned int) code, buf, &result);
	} else {
		copy_from_history(decoder, (unsigned int) code - 8,
		                  buf, &result);
	}

	return result;
}

LHADecoderType lha_pma_decoder = {
	lha_pma_decoder_init,
	NULL,
	lha_pma_decoder_read,
	sizeof(LHAPMADecoder),
	OUTPUT_BUFFER_SIZE,
	RING_BUFFER_SIZE
};

