/*

Copyright (c) 2011-2019 Simon Howard

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

#ifndef LHA_LH1_COMMON_FILE_HEADER_H
#define LHA_LH1_COMMON_FILE_HEADER_H

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

typedef struct {

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

typedef struct {
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

	// Offset lookup table.  Maps from a byte value (sequence of next
	// 8 bits from input stream) to an offset value.
	uint8_t offset_lookup[256];

	// Length of offsets, in bits.
	uint8_t offset_lengths[NUM_OFFSETS];
} LHALH1State;

int lha_lh1_init_state(LHALH1State *state);
void lha_lh1_increment_for_code(LHALH1State *state, uint16_t code);

#endif /* #ifndef LHA_LH1_COMMON_FILE_HEADER_H */

