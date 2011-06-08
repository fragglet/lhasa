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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "lha_decoder.h"

// Size of the ring buffer used to hold history:

#define RING_BUFFER_SIZE     4096 /* bytes */

// When this limit is reached, the code tree is reordered.

#define TREE_REORDER_LIMIT   32 * 1024  /* 32 kB */

// Number of codes ('byte' codes + 'copy' codes):

#define NUM_CODES            314

// Number of nodes in the code tree.

#define NUM_TREE_NODES       (NUM_CODES * 2 - 1)

// Number of possible offsets:

#define NUM_OFFSETS          64

// Minimum length of the offset top bits:

#define MIN_OFFSET_LENGTH    3 /* bits */

// Threshold for copying. The first copy code starts from here.

#define COPY_THRESHOLD       3 /* bytes */

// Required size of the output buffer.  At most, a single call to read()
// might result in a copy of the entire ring buffer.

#define OUTPUT_BUFFER_SIZE   RING_BUFFER_SIZE

typedef struct
{
	// If true, this node is a leaf node.

	unsigned int leaf        :1;

	// If this is a leaf node, child_index is the code represented by
	// this node. Otherwise, nodes[child_index] and nodes[child_index-1]
	// are the children of this node.

	unsigned int child_index :15;

	// Index of the parent node of this node.

	uint16_t parent;

	// Frequency count for this node - number of times that it has
	// received a hit.

	uint16_t freq;

	// Group that this node belongs to.

	uint16_t group;
} Node;

typedef struct
{
	// Callback function to invoke to read more data from the
	// input stream.

	LHADecoderCallback callback;
	void *callback_data;

	// Ring buffer of past data.  Used for position-based copies.

	uint8_t ringbuf[RING_BUFFER_SIZE];
	unsigned int ringbuf_pos;

	// Array of tree nodes. nodes[0] is the root node.  The array
	// is maintained in order by frequency.

	Node nodes[NUM_TREE_NODES];

	// Indices of leaf nodes of the tree (map from code to leaf
	// node index)

	uint16_t leaf_nodes[NUM_CODES];

	// Groups list.  Every node belongs to a group.  All nodes within
	// a group have the same frequency. There can be at most
	// NUM_TREE_NODES groups (one for each node). num_groups is used
	// to allocate and deallocate groups as needed.

	uint16_t groups[NUM_TREE_NODES];
	unsigned int num_groups;

	// Index of the "leader" of a group within the nodes[] array.
	// The leader is the left-most node within a span of nodes with
	// the same frequency.

	uint16_t group_leader[NUM_TREE_NODES];

	// Bits from the input stream that are waiting to be read.

	uint32_t bit_buffer;
	unsigned int bits;

	// Offset lookup table.  Maps from a byte value (sequence of next
	// 8 bits from input stream) to an offset value.

	uint8_t offset_lookup[256];

	// Length of offsets, in bits.

	uint8_t offset_lengths[NUM_OFFSETS];
} LHALZHUFDecoder;

// Frequency distribution used to calculate the offset codes.

static const unsigned int offset_fdist[] = {
	1,    // 3 bits
	3,    // 4 bits
	8,    // 5 bits
	12,   // 6 bits
	24,   // 7 bits
	16,   // 8 bits
};

// Allocate a group from the free groups array.

static uint16_t alloc_group(LHALZHUFDecoder *decoder)
{
	uint16_t result;

	result = decoder->groups[decoder->num_groups];
	++decoder->num_groups;

	return result;
}

// Free a group that is no longer in use.

static void free_group(LHALZHUFDecoder *decoder, uint16_t group)
{
	--decoder->num_groups;
	decoder->groups[decoder->num_groups] = group;
}

// Initialize groups array.

static void init_groups(LHALZHUFDecoder *decoder)
{
	unsigned int i;

	for (i = 0; i < NUM_TREE_NODES; ++i) {
		decoder->groups[i] = (uint16_t) i;
	}

	decoder->num_groups = 0;
}

// Initialize the tree with its basic initial configuration.

static void init_tree(LHALZHUFDecoder *decoder)
{
	unsigned int i, child;
	int node_index;
	uint16_t leaf_group;
	Node *node;

	// Leaf nodes are placed at the end of the table.  Start by
	// initializing these, and working backwards.

	node_index = NUM_TREE_NODES - 1;
	leaf_group = alloc_group(decoder);

	for (i = 0; i < NUM_CODES; ++i) {
		node = &decoder->nodes[node_index];
		node->leaf = 1;
		node->child_index = (unsigned short) i;
		node->freq = 1;
		node->group = leaf_group;

		decoder->group_leader[leaf_group] = (uint16_t) node_index;
		decoder->leaf_nodes[i] = (uint16_t) node_index;

		--node_index;
	}

	// Now build up the intermediate nodes, up to the root.  Each
	// node gets two nodes as children.

	child = NUM_TREE_NODES - 1;

	while (node_index >= 0) {
		node = &decoder->nodes[node_index];
		node->leaf = 0;

		// Set child pointer and update the parent pointers of the
		// children.

		node->child_index = child;
		decoder->nodes[child].parent = (uint16_t) node_index;
		decoder->nodes[child - 1].parent = (uint16_t) node_index;

		// The node's frequency is equal to the sum of the frequencies
		// of its children.

		node->freq = (uint16_t) (decoder->nodes[child].freq
		                       + decoder->nodes[child - 1].freq);

		// Is the frequency the same as the last node we processed?
		// if so, we are in the same group. If not, we must
		// allocate a new group.  Either way, this node is now the
		// leader of its group.

		if (node->freq == decoder->nodes[node_index + 1].freq) {
			node->group = decoder->nodes[node_index + 1].group;
		} else {
			node->group = alloc_group(decoder);
		}

		decoder->group_leader[node->group] = (uint16_t) node_index;

		// Process next node.

		--node_index;
		child -= 2;
	}
}

// Fill in a range of values in the offset_lookup table, which have
// the bits from 'code' as the high bits, and the low bits can be
// any values in the range from 'mask'.  Set these values to point
// to 'offset'.

static void fill_offset_range(LHALZHUFDecoder *decoder, uint8_t code,
                              uint8_t mask, unsigned int offset)
{
	unsigned int i;

	// Set offset lookup table to map from all possible input values
	// that fit within the mask to the target offset.

	for (i = 0; (i & ~mask) == 0; ++i) {
		decoder->offset_lookup[code | i] = (uint8_t) offset;
	}
}

// Calculate the values for the offset_lookup and offset_lengths
// tables.

static void init_offset_table(LHALZHUFDecoder *decoder)
{
	unsigned int i, j, len;
	uint8_t code, iterbit, offset;

	code = 0;
	offset = 0;

	// Iterate through each entry in the frequency distribution table,
	// filling in codes in the lookup table as we go.

	for (i = 0; i < sizeof(offset_fdist) / sizeof(*offset_fdist); ++i) {

		// offset_fdist[0] is the number of codes of length
		// MIN_OFFSET_LENGTH bits, increasing as we go. As the
		// code increases in length, we must iterate progressively
		// lower bits in the code (moving right - extending the
		// code to be 1 bit longer).

		len = i + MIN_OFFSET_LENGTH;
		iterbit = (uint8_t) (1 << (8 - len));

		for (j = 0; j < offset_fdist[i]; ++j) {

			// Store lookup values for this offset in the
			// lookup table, and save the code length.
			// (iterbit - 1) turns into a mask for the lower
			// bits that are not part of the code.

			fill_offset_range(decoder, code,
			                  (uint8_t) (iterbit - 1), offset);
			decoder->offset_lengths[offset] = (uint8_t) len;

			// Iterate to next code.

			code = (uint8_t) (code + iterbit);
			++offset;
		}
	}
}

// Initialize the history ring buffer.

static void init_ring_buffer(LHALZHUFDecoder *decoder)
{
	memset(decoder->ringbuf, ' ', RING_BUFFER_SIZE);
	decoder->ringbuf_pos = 0;
}

static int lha_lzhuf_init(void *data, LHADecoderCallback callback,
                          void *callback_data)
{
	LHALZHUFDecoder *decoder = data;

	decoder->callback = callback;
	decoder->callback_data = callback_data;

	// Initialize data structures.

	init_groups(decoder);
	init_tree(decoder);
	init_offset_table(decoder);
	init_ring_buffer(decoder);

	decoder->bits = 0;

	return 1;
}

// Return the next n bits waiting to be read from the input stream,
// without removing any.

static int peek_bits(LHALZHUFDecoder *decoder,
		     unsigned int n,
                     unsigned int *result)
{
	uint8_t buf[3];
	size_t bytes;

	// Always try to keep at least 8 bits in the input buffer.
	// When the level drops low, read some more bytes to top it up.

	if (decoder->bits < 8) {
		bytes = decoder->callback(buf, 3, decoder->callback_data);

		decoder->bit_buffer |= buf[0] << (24 - decoder->bits);
		decoder->bit_buffer |= buf[1] << (16 - decoder->bits);
		decoder->bit_buffer |= buf[2] << (8 - decoder->bits);

		decoder->bits += bytes * 8;
	}

	if (decoder->bits < n) {
		return 0;
	}

	*result = decoder->bit_buffer >> (32 - n);

	return 1;
}

// Read a bit from the input stream.
// Returns true on success and sets *result.

static int read_bits(LHALZHUFDecoder *decoder,
                     unsigned int n,
                     unsigned int *result)
{
	if (!peek_bits(decoder, n, result)) {
		return 0;
	}

	decoder->bit_buffer <<= n;
	decoder->bits -= n;

	return 1;
}


// Read a bit from the input stream.
// Returns true on success and sets *result.

static int read_bit(LHALZHUFDecoder *decoder,
                    unsigned int *result)
{
	return read_bits(decoder, 1, result);
}

// Make the given node the leader of its group: swap it with the current
// leader so that it is in the left-most position.  Returns the new index
// of the node.

static uint16_t make_group_leader(LHALZHUFDecoder *decoder,
                                  uint16_t node_index)
{
	Node *node, *leader;
	uint16_t group;
	uint16_t leader_index;
	unsigned int tmp;

	group = decoder->nodes[node_index].group;
	leader_index = decoder->group_leader[group];

	// Already the leader?  If so, there is nothing to do.

	if (leader_index == node_index) {
		return node_index;
	}

	node = &decoder->nodes[node_index];
	leader = &decoder->nodes[leader_index];

	// Swap leaf and child indices in the two nodes:

	tmp = leader->leaf;
	leader->leaf = node->leaf;
	node->leaf = tmp;

	tmp = leader->child_index;
	leader->child_index = node->child_index;
	node->child_index = tmp;

	if (node->leaf) {
		decoder->leaf_nodes[node->child_index] = node_index;
	} else {
		decoder->nodes[node->child_index].parent = node_index;
		decoder->nodes[node->child_index - 1].parent = node_index;
	}

	if (leader->leaf) {
		decoder->leaf_nodes[leader->child_index] = leader_index;
	} else {
		decoder->nodes[leader->child_index].parent = leader_index;
		decoder->nodes[leader->child_index - 1].parent = leader_index;
	}

	return leader_index;
}

// Increase the frequency count for a node, rearranging groups as
// appropriate.

static void increment_node_freq(LHALZHUFDecoder *decoder,
                                uint16_t node_index)
{
	Node *node, *other;

	node = &decoder->nodes[node_index];
	other = &decoder->nodes[node_index - 1];

	++node->freq;

	// If the node is part of a group containing other nodes, it
	// must leave the group.

	if (node_index < NUM_TREE_NODES - 1
	 && node->group == decoder->nodes[node_index + 1].group) {

		// Next node in the group now becomes the leader.

		++decoder->group_leader[node->group];

		// The node must now either join the group to its
		// left, or start a new group.

		if (node->freq == other->freq) {
			node->group = other->group;
		} else {
			node->group = alloc_group(decoder);
			decoder->group_leader[node->group] = node_index;
		}

	} else {
		// The node is in a group of its own (single-node
		// group).  It might need to join the group of the
		// node on its left if it has the same frequency.

		if (node->freq == other->freq) {
			free_group(decoder, node->group);
			node->group = other->group;
		}
	}
}

static void increment_for_code(LHALZHUFDecoder *decoder, uint16_t code)
{
	uint16_t node_index;

	// When the limit is reached, we must reorder the code tree
	// to better match the code frequencies:

	if (decoder->nodes[0].freq >= TREE_REORDER_LIMIT) {
		// TODO:
		//reorder_tree(decoder);
	}

	++decoder->nodes[0].freq;

	// Dynamically adjust the tree.  Start from the leaf node of
	// the tree and walk back up, rearranging nodes to the root.

	node_index = decoder->leaf_nodes[code];

	while (node_index != 0) {

		// Shift the node to the left side of its group,
		// and bump the frequency count.

		node_index = make_group_leader(decoder, node_index);

		increment_node_freq(decoder, node_index);

		// Iterate up to the parent node.

		node_index = decoder->nodes[node_index].parent;
	}
}

// Read a code from the input stream.

static int read_code(LHALZHUFDecoder *decoder,
                     uint16_t *result)
{
	unsigned int node_index;
	unsigned int bit;

	// Start from the root node, and traverse down until a leaf is
	// reached.

	node_index = 0;

	//printf("<root ");
	while (!decoder->nodes[node_index].leaf) {
		if (!read_bit(decoder, &bit)) {
			return 0;
		}

		//printf("<%i>", bit);

		// Choose one of the two children depending on the
		// bit that was read.

		node_index = decoder->nodes[node_index].child_index - bit;
	}

	*result = decoder->nodes[node_index].child_index;
	//printf(" -> %i!>\n", *result);

	increment_for_code(decoder, *result);

	return 1;
}

// Read an offset code from the input stream.

static int read_offset(LHALZHUFDecoder *decoder,
                       unsigned int *result)
{
	unsigned int future, offset, offset2;

	// The offset can be up to 8 bits long, but is likely not
	// that long. Use the lookup table to find the offset
	// and its length.

	if (!peek_bits(decoder, 8, &future)) {
		return 0;
	}

	offset = decoder->offset_lookup[future];

	// Skip past the offset bits and also read the following
	// lower-order bits.

	if (!read_bits(decoder, decoder->offset_lengths[offset], &offset2)
	 || !read_bits(decoder, 6, &offset2)) {
		return 0;
	}

	*result = (offset << 6) | offset2;

	return 1;
}

static void output_byte(LHALZHUFDecoder *decoder, uint8_t *buf,
                        size_t *buf_len, uint8_t b)
{
	buf[*buf_len] = b;
	++*buf_len;

	decoder->ringbuf[decoder->ringbuf_pos] = b;
	decoder->ringbuf_pos = (decoder->ringbuf_pos + 1) % RING_BUFFER_SIZE;
}

static size_t lha_lzhuf_read(void *data, uint8_t *buf)
{
	LHALZHUFDecoder *decoder = data;
	size_t result;
	uint16_t code;

	result = 0;

	// Read the next code from the input stream.

	if (!read_code(decoder, &code)) {
		return 0;
	}

	// The code either indicates a single byte to be output, or
	// it indicates that a block should be copied from the ring
	// buffer as it is a repeat of a sequence earlier in the
	// stream.

	if (code < 0x100) {
		output_byte(decoder, buf, &result, (uint8_t) code);
	} else {
		unsigned int count, start, i, pos, offset;

		// Read the offset into the history at which to start
		// copying.

		if (!read_offset(decoder, &offset)) {
			return 0;
		}

		count = code - 0x100U + COPY_THRESHOLD;
		start = decoder->ringbuf_pos - offset + RING_BUFFER_SIZE - 1;

		// Copy from history into output buffer:

		for (i = 0; i < count; ++i) {
			pos = (start + i) % RING_BUFFER_SIZE;

			output_byte(decoder, buf, &result,
			            decoder->ringbuf[pos]);
		}
	}

	return result;
}

LHADecoderType lha_lzhuf_decoder = {
	lha_lzhuf_init,
	NULL,
	lha_lzhuf_read,
	sizeof(LHALZHUFDecoder),
	OUTPUT_BUFFER_SIZE
};


