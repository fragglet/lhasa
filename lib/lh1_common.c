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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "lh1_common.h"

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
static uint16_t alloc_group(LHALH1State *state)
{
	uint16_t result;

	result = state->groups[state->num_groups];
	++state->num_groups;

	return result;
}

// Free a group that is no longer in use.
static void free_group(LHALH1State *state, uint16_t group)
{
	--state->num_groups;
	state->groups[state->num_groups] = group;
}

// Initialize groups array.
static void init_groups(LHALH1State *state)
{
	unsigned int i;

	for (i = 0; i < NUM_TREE_NODES; ++i) {
		state->groups[i] = (uint16_t) i;
	}

	state->num_groups = 0;
}

// Initialize the tree with its basic initial configuration.
static void init_tree(LHALH1State *state)
{
	unsigned int i, child;
	int node_index;
	uint16_t leaf_group;
	Node *node;

	// Leaf nodes are placed at the end of the table.  Start by
	// initializing these, and working backwards.
	node_index = NUM_TREE_NODES - 1;
	leaf_group = alloc_group(state);

	for (i = 0; i < NUM_CODES; ++i) {
		node = &state->nodes[node_index];
		node->leaf = 1;
		node->child_index = (unsigned short) i;
		node->freq = 1;
		node->group = leaf_group;

		state->group_leader[leaf_group] = (uint16_t) node_index;
		state->leaf_nodes[i] = (uint16_t) node_index;

		--node_index;
	}

	// Now build up the intermediate nodes, up to the root.  Each
	// node gets two nodes as children.
	child = NUM_TREE_NODES - 1;

	while (node_index >= 0) {
		node = &state->nodes[node_index];
		node->leaf = 0;

		// Set child pointer and update the parent pointers of the
		// children.
		node->child_index = child;
		state->nodes[child].parent = (uint16_t) node_index;
		state->nodes[child - 1].parent = (uint16_t) node_index;

		// The node's frequency is equal to the sum of the frequencies
		// of its children.
		node->freq = (uint16_t) (state->nodes[child].freq
		                       + state->nodes[child - 1].freq);

		// Is the frequency the same as the last node we processed?
		// if so, we are in the same group. If not, we must
		// allocate a new group.  Either way, this node is now the
		// leader of its group.
		if (node->freq == state->nodes[node_index + 1].freq) {
			node->group = state->nodes[node_index + 1].group;
		} else {
			node->group = alloc_group(state);
		}

		state->group_leader[node->group] = (uint16_t) node_index;

		// Process next node.
		--node_index;
		child -= 2;
	}
}

// Fill in a range of values in the offset_lookup table, which have
// the bits from 'code' as the high bits, and the low bits can be
// any values in the range from 'mask'.  Set these values to point
// to 'offset'.
static void fill_offset_range(LHALH1State *state, uint8_t code,
                              unsigned int mask, unsigned int offset)
{
	unsigned int i;

	// Set offset lookup table to map from all possible input values
	// that fit within the mask to the target offset.
	for (i = 0; (i & ~mask) == 0; ++i) {
		state->offset_lookup[code | i] = (uint8_t) offset;
	}
}

// Calculate the values for the offset_lookup and offset_lengths
// tables.
static void init_offset_table(LHALH1State *state)
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
			state->offset_codes[offset] = code >> (8 - len);

			// Store lookup values for this offset in the
			// lookup table, and save the code length.
			// (iterbit - 1) turns into a mask for the lower
			// bits that are not part of the code.
			fill_offset_range(state, code,
			                  (uint8_t) (iterbit - 1), offset);
			state->offset_lengths[offset] = (uint8_t) len;

			// Iterate to next code.
			code = (uint8_t) (code + iterbit);
			++offset;
		}
	}
}

int lha_lh1_init_state(LHALH1State *state)
{
	// Initialize data structures.
	init_groups(state);
	init_tree(state);
	init_offset_table(state);

	return 1;
}

// Make the given node the leader of its group: swap it with the current
// leader so that it is in the left-most position.  Returns the new index
// of the node.
static uint16_t make_group_leader(LHALH1State *state, uint16_t node_index)
{
	Node *node, *leader;
	uint16_t group;
	uint16_t leader_index;
	unsigned int tmp;

	group = state->nodes[node_index].group;
	leader_index = state->group_leader[group];

	// Already the leader?  If so, there is nothing to do.
	if (leader_index == node_index) {
		return node_index;
	}

	node = &state->nodes[node_index];
	leader = &state->nodes[leader_index];

	// Swap leaf and child indices in the two nodes:
	tmp = leader->leaf;
	leader->leaf = node->leaf;
	node->leaf = tmp;

	tmp = leader->child_index;
	leader->child_index = node->child_index;
	node->child_index = tmp;

	if (node->leaf) {
		state->leaf_nodes[node->child_index] = node_index;
	} else {
		state->nodes[node->child_index].parent = node_index;
		state->nodes[node->child_index - 1].parent = node_index;
	}

	if (leader->leaf) {
		state->leaf_nodes[leader->child_index] = leader_index;
	} else {
		state->nodes[leader->child_index].parent = leader_index;
		state->nodes[leader->child_index - 1].parent = leader_index;
	}

	return leader_index;
}

// Increase the frequency count for a node, rearranging groups as
// appropriate.
static void increment_node_freq(LHALH1State *state, uint16_t node_index)
{
	Node *node, *other;

	node = &state->nodes[node_index];
	other = &state->nodes[node_index - 1];

	++node->freq;

	// If the node is part of a group containing other nodes, it
	// must leave the group.
	if (node_index < NUM_TREE_NODES - 1
	 && node->group == state->nodes[node_index + 1].group) {

		// Next node in the group now becomes the leader.
		++state->group_leader[node->group];

		// The node must now either join the group to its
		// left, or start a new group.
		if (node->freq == other->freq) {
			node->group = other->group;
		} else {
			node->group = alloc_group(state);
			state->group_leader[node->group] = node_index;
		}

	} else {
		// The node is in a group of its own (single-node
		// group).  It might need to join the group of the
		// node on its left if it has the same frequency.
		if (node->freq == other->freq) {
			free_group(state, node->group);
			node->group = other->group;
		}
	}
}

// Reconstruct the code huffman tree to be more evenly distributed.
// Invoked periodically as data is processed.
static void reconstruct_tree(LHALH1State *state)
{
	Node *leaf;
	unsigned int child;
	unsigned int freq;
	unsigned int group;
	int i;

	// Gather all leaf nodes at the start of the table.
	leaf = state->nodes;

	for (i = 0; i < NUM_TREE_NODES; ++i) {
		if (state->nodes[i].leaf) {
			leaf->leaf = 1;
			leaf->child_index = state->nodes[i].child_index;

			// Frequency of the nodes in the new tree is halved,
			// this acts as a running average each time the
			// tree is reconstructed.
			leaf->freq = (uint16_t) (state->nodes[i].freq + 1) / 2;

			++leaf;
		}
	}

	// The leaf nodes are now all at the start of the table.  Now
	// reconstruct the tree, starting from the end of the table and
	// working backwards, inserting branch nodes between the leaf
	// nodes.  Each branch node inherits the sum of the frequencies
	// of its children, and must be placed to maintain the ordering
	// within the table by decreasing frequency.
	leaf = &state->nodes[NUM_CODES - 1];
	child = NUM_TREE_NODES - 1;
	i = NUM_TREE_NODES - 1;

	while (i >= 0) {

		// Before we can add a new branch node, we need at least
		// two nodes to use as children.  If we don't have this
		// then we need to copy some from the leaves.
		while ((int) child - i < 2) {
			state->nodes[i] = *leaf;
			state->leaf_nodes[leaf->child_index] = (uint16_t) i;

			--i;
			--leaf;
		}

		// Now that we have at least two nodes to take as children
		// of the new branch node, we can calculate the branch
		// node's frequency.
		freq = (unsigned int) (state->nodes[child].freq
		                     + state->nodes[child - 1].freq);

		// Now copy more leaf nodes until the correct place to
		// insert the new branch node presents itself.
		while (leaf >= state->nodes && freq >= leaf->freq) {
			state->nodes[i] = *leaf;
			state->leaf_nodes[leaf->child_index] = (uint16_t) i;

			--i;
			--leaf;
		}

		// The new branch node can now be inserted.
		state->nodes[i].leaf = 0;
		state->nodes[i].freq = (uint16_t) freq;
		state->nodes[i].child_index = (uint16_t) child;

		state->nodes[child].parent = (uint16_t) i;
		state->nodes[child - 1].parent = (uint16_t) i;

		--i;

		// Process the next pair of children.
		child -= 2;
	}

	// Reconstruct the group data.  Start by resetting group data.
	init_groups(state);

	// Assign a group to the first node.
	group = alloc_group(state);
	state->nodes[0].group = (uint16_t) group;
	state->group_leader[group] = 0;

	// Assign a group number to each node, nodes having the same
	// group if the have the same frequency, and allocating new
	// groups when a new frequency is found.
	for (i = 1; i < NUM_TREE_NODES; ++i) {
		if (state->nodes[i].freq == state->nodes[i - 1].freq) {
			state->nodes[i].group = state->nodes[i - 1].group;
		} else {
			group = alloc_group(state);
			state->nodes[i].group = (uint16_t) group;

			// First node with a particular frequency is leader.
			state->group_leader[group] = (uint16_t) i;
		}
	}
}

// Increment the counter for the specific code, reordering the tree as
// necessary.
void lha_lh1_increment_for_code(LHALH1State *state, uint16_t code)
{
	uint16_t node_index;

	// When the limit is reached, we must reorder the code tree
	// to better match the code frequencies:
	if (state->nodes[0].freq >= TREE_REORDER_LIMIT) {
		reconstruct_tree(state);
	}

	++state->nodes[0].freq;

	// Dynamically adjust the tree.  Start from the leaf node of
	// the tree and walk back up, rearranging nodes to the root.
	node_index = state->leaf_nodes[code];

	while (node_index != 0) {

		// Shift the node to the left side of its group,
		// and bump the frequency count.
		node_index = make_group_leader(state, node_index);

		increment_node_freq(state, node_index);

		// Iterate up to the parent node.
		node_index = state->nodes[node_index].parent;
	}
}
