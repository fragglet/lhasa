
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "lha_decoder.h"

#define TREE_REORDER_LIMIT   32 * 1024  /* 32 kB */
#define NUM_CODES            314
#define NUM_TREE_NODES       (NUM_CODES * 2 - 1)
#define NUM_OFFSETS          64
#define MIN_OFFSET_LENGTH    3 /* bits */

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
} LZHUFDecoder;

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

static uint16_t alloc_group(LZHUFDecoder *decoder)
{
	uint16_t result;

	result = decoder->groups[decoder->num_groups];
	++decoder->num_groups;

	return result;
}

// Free a group that is no longer in use.

static void free_group(LZHUFDecoder *decoder, uint16_t group)
{
	--decoder->num_groups;
	decoder->groups[decoder->num_groups] = group;
}

// Initialize groups array.

static void init_groups(LZHUFDecoder *decoder)
{
	unsigned int i;

	for (i = 0; i < NUM_TREE_NODES; ++i) {
		decoder->groups[i] = i;
	}

	decoder->num_groups = 0;
}

// Initialize the tree with its basic initial configuration.

static void init_tree(LZHUFDecoder *decoder)
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
		node->child_index = i;
		node->freq = 1;
		node->group = leaf_group;

		decoder->group_leader[leaf_group] = node_index;
		decoder->leaf_nodes[i] = node_index;

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
		decoder->nodes[child].parent = node_index;
		decoder->nodes[child - 1].parent = node_index;

		// The node's frequency is equal to the sum of the frequencies
		// of its children.

		node->freq = decoder->nodes[child].freq
		           + decoder->nodes[child - 1].freq;

		// Is the frequency the same as the last node we processed?
		// if so, we are in the same group. If not, we must
		// allocate a new group.  Either way, this node is now the
		// leader of its group.

		if (node->freq == decoder->nodes[node_index + 1].freq) {
			node->group = decoder->nodes[node_index + 1].group;
		} else {
			node->group = alloc_group(decoder);
		}

		decoder->group_leader[node->group] = node_index;

		// Process next node.

		--node_index;
		child -= 2;
	}
}

// Fill in a range of values in the offset_lookup table, which have
// the bits from 'code' as the high bits, and the low bits can be
// any values in the range from 'mask'.  Set these values to point
// to 'offset'.

static void fill_offset_range(LZHUFDecoder *decoder, uint8_t code,
                              uint8_t mask, unsigned int offset)
{
	unsigned int i;

	// Set offset lookup table to map from all possible input values
	// that fit within the mask to the target offset.

	for (i = 0; (i & ~mask) == 0; ++i) {
		decoder->offset_lookup[code | i] = offset;
	}
}

// Calculate the values for the offset_lookup and offset_lengths
// tables.

static void init_offset_table(LZHUFDecoder *decoder)
{
	unsigned int i, j, len;
	unsigned int offset, iterbit;
	uint8_t code;

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
		iterbit = 1 << (8 - len);

		for (j = 0; j < offset_fdist[i]; ++j) {

			// Store lookup values for this offset in the
			// lookup table, and save the code length.
			// (iterbit - 1) turns into a mask for the lower
			// bits that are not part of the code.

			fill_offset_range(decoder, code, iterbit - 1, offset);
			decoder->offset_lengths[offset] = len;

			// Iterate to next code.

			code += iterbit;
			++offset;
		}
	}

/*
	for (i = 0; i < 256; ++i) {
		if ((i % 16) == 0)
			printf("%02x: ", i);
		printf("%2i ", decoder->offset_lookup[i]);

		if ((i % 16) == 15)
			printf("\n");
	}
*/
}

static int lha_lzh_init(void *data)
{
	LZHUFDecoder *decoder = data;

	init_groups(decoder);
	init_tree(decoder);
	init_offset_table(decoder);

	decoder->bits = 0;

	return 1;
}

// Return the next n bits waiting to be read from the input stream,
// without removing any.

static int peek_bits(LZHUFDecoder *decoder,
                     LHADecoderCallback callback,
                     void *callback_data,
		     unsigned int n,
                     unsigned int *result)
{
	uint8_t buf[3];

	// Always try to keep at least 8 bits in the input buffer.
	// When the level drops low, read some more bytes to top it up.

	if (decoder->bits < 8) {
		if (!callback(buf, 3, callback_data)) {
			return 0;
		}

		decoder->bit_buffer |= buf[0] << (24 - decoder->bits);
		decoder->bit_buffer |= buf[1] << (16 - decoder->bits);
		decoder->bit_buffer |= buf[2] << (8 - decoder->bits);

		decoder->bits += 24;
	}


	*result = decoder->bit_buffer >> (32 - n);

	return 1;
}

// Read a bit from the input stream.
// Returns true on success and sets *result.

static int read_bits(LZHUFDecoder *decoder,
                     LHADecoderCallback callback,
                     void *callback_data,
                     unsigned int n,
                     unsigned int *result)
{
	if (!peek_bits(decoder, callback, callback_data, n, result)) {
		return 0;
	}

	decoder->bit_buffer <<= n;
	decoder->bits -= n;

	return 1;
}


// Read a bit from the input stream.
// Returns true on success and sets *result.

static int read_bit(LZHUFDecoder *decoder,
                    LHADecoderCallback callback,
                    void *callback_data,
                    unsigned int *result)
{
	return read_bits(decoder, callback, callback_data, 1, result);
}

// Make the given node the leader of its group: swap it with the current
// leader so that it is in the left-most position.  Returns the new index
// of the node.

static unsigned int make_group_leader(LZHUFDecoder *decoder,
                                      unsigned int node_index)
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

static void increment_node_freq(LZHUFDecoder *decoder, unsigned int node_index)
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

static void increment_for_code(LZHUFDecoder *decoder, uint16_t code)
{
	unsigned int node_index;

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

static int read_code(LZHUFDecoder *decoder,
                     LHADecoderCallback callback,
                     void *callback_data,
                     uint16_t *result)
{
	unsigned int node_index;
	unsigned int bit;

	// Start from the root node, and traverse down until a leaf is
	// reached.

	node_index = 0;

	//printf("<root ");
	while (!decoder->nodes[node_index].leaf) {
		if (!read_bit(decoder, callback, callback_data, &bit)) {
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

static int read_offset(LZHUFDecoder *decoder,
                       LHADecoderCallback callback,
                       void *callback_data,
                       uint16_t *result)
{
	unsigned int future, offset, offset2;

	// The offset can be up to 8 bits long, but is likely not
	// that long. Use the lookup table to find the offset
	// and its length.

	if (!peek_bits(decoder, callback, callback_data, 8, &future)) {
		return 0;
	}

	offset = decoder->offset_lookup[future];
	//printf("<future=%i, offset=%i, len=%i>", future, offset,  decoder->offset_lengths[offset]);

	// Skip past the offset bits and also read the following
	// lower-order bits.

	read_bits(decoder, callback, callback_data, 6, &offset2);
	read_bits(decoder, callback, callback_data,
	          decoder->offset_lengths[offset], &offset2);

	*result = (offset << 6) | offset2;

	//printf("<position=%i>", *result);

	return 1;
}

static size_t lha_lzh_read(void *data, uint8_t *buf, size_t buf_len,
                           LHADecoderCallback callback, void *callback_data)
{
	LZHUFDecoder *decoder = data;
	uint16_t code;

	if (!read_code(decoder, callback, callback_data, &code)) {
printf("failed to read code\n");
		return 0;
	}

	if (code < 0x100) {
		buf[0] = code;
		putchar(code);
		// TODO: Update ring buffer ...
		return 1;
	} else {
		unsigned int count = code - 253;
		uint16_t offset;
		int i;

	//	printf("<%i>", count);

		if (!read_offset(decoder, callback, callback_data, &offset)) {
			return 0;
		}

		for (i = 0; i < count; ++i) {
			putchar('-');
		}

		return 1;
	}
}

FILE *instream;
LZHUFDecoder the_decoder;

static size_t instream_reader(void *buf, size_t buf_len, void *unused)
{
	return fread(buf, buf_len, 1, instream);
}

int main(int argc, char *argv[])
{
	instream = fopen(argv[1], "rb");

	lha_lzh_init(&the_decoder);

	for (;;) {
		uint8_t buf;
		size_t b;

		b = lha_lzh_read(&the_decoder, &buf, 1,
		                 instream_reader, NULL);

	//	if (b > 0) {
	//		putchar(buf);
	//	}
	}
}

