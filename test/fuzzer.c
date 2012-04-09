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

// Fuzz testing system for stress-testing the decompressors.
// This works by repeatedly generating new random streams of
// data and feeding them to the decompressor. This on its own
// works for the majority of decompressors (-lh1-, -lh5-, etc.)
//
// Some decompressors (eg -pm2-) are more particular about
// input checking, and will fail if given random data. To
// cope with these, a genetic algorithm is used to generate
// progressively longer valid input streams.

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include "lha_decoder.h"

// Maximum signature length before stopping.

#define MAX_FUZZ_LEN 2000000

typedef struct {
	uint8_t *data;
	unsigned int data_len;
} FuzzSignature;

typedef struct {
	FuzzSignature *signature;
	unsigned int read;
	unsigned int max_len;
} ReadCallbackData;

// Contents of "canary buffer" that is put around allocated blocks to
// check their contents.

static const uint8_t canary_block[] = {
	0xdf, 0xba, 0x18, 0xa0, 0x51, 0x91, 0x3c, 0xd6, 
	0x03, 0xfb, 0x2c, 0xa6, 0xd6, 0x88, 0xa5, 0x75, 
};

// Allocate some memory with canary blocks surrounding it.

static void *canary_malloc(size_t nbytes)
{
	uint8_t *result;

	result = malloc(nbytes + 2 * sizeof(canary_block) + sizeof(size_t));
	assert(result != NULL);

	memcpy(result, &nbytes, sizeof(size_t));
	memcpy(result + sizeof(size_t), canary_block, sizeof(canary_block));
	memset(result + sizeof(size_t) + sizeof(canary_block), 0, nbytes);
	memcpy(result + sizeof(size_t) + sizeof(canary_block) + nbytes,
	       canary_block, sizeof(canary_block));

	return result + sizeof(size_t) + sizeof(canary_block);
}

// Free memory allocated with canary_malloc().

static void canary_free(void *data)
{
	if (data != NULL) {
		free((uint8_t *) data - sizeof(size_t) - sizeof(canary_block));
	}
}

// Check the canary blocks surrounding memory allocated with canary_malloc().

static void canary_check(void *_data)
{
	uint8_t *data = _data;
	size_t nbytes;

	memcpy(&nbytes, data - sizeof(size_t) - sizeof(canary_block),
	       sizeof(size_t));

	assert(!memcmp(data - sizeof(canary_block), canary_block,
	               sizeof(canary_block)));
	assert(!memcmp(data + nbytes, canary_block, sizeof(canary_block)));
}

// Fill in the specified block with random data.

static void fuzz_block(uint8_t *data, unsigned int data_len)
{
	unsigned int i;

	for (i = 0; i < data_len; ++i) {
		data[i] = rand() & 0xff;
	}
}

// Create an empty signature.

static FuzzSignature *empty_signature(void)
{
	FuzzSignature *result;

	result = malloc(sizeof(FuzzSignature));
	assert(result != NULL);
	result->data = NULL;
	result->data_len = 0;

	return result;
}

// Copy a signature to create a new one.

static FuzzSignature *dup_signature(FuzzSignature *signature)
{
	FuzzSignature *result;

	result = malloc(sizeof(FuzzSignature));
	assert(result != NULL);
	result->data = malloc(signature->data_len);
	assert(result->data != NULL);
	memcpy(result->data, signature->data, signature->data_len);
	result->data_len = signature->data_len;

	return result;
}

// Create a new "child" signature, derived from an existing
// signature with the last few bytes changed.

static FuzzSignature *child_signature(FuzzSignature *signature,
                                      unsigned int nbytes)
{
	FuzzSignature *child;

	child = dup_signature(signature);

	if (nbytes > child->data_len) {
		nbytes = child->data_len;
	}

	fuzz_block(child->data + child->data_len - nbytes, nbytes);

	return child;
}

// Free a signature.

static void free_signature(FuzzSignature *signature)
{
	if (signature != NULL) {
		free(signature->data);
		free(signature);
	}
}

// "Extend" a signature, adding some more random data to the end.

static void extend_signature(FuzzSignature *signature)
{
	unsigned int new_len;

	new_len = signature->data_len + 16;
	signature->data = realloc(signature->data, new_len);
	assert(signature->data != NULL);

	fuzz_block(signature->data + signature->data_len, 16);
	signature->data_len = new_len;
}

static void *init_decoder(LHADecoderType *dtype,
                          LHADecoderCallback read_callback,
                          void *callback_data)
{
	void *result;

	result = canary_malloc(dtype->extra_size);

	assert(dtype->init(result, read_callback, callback_data));

	return result;
}

// Callback function used to read more data from the signature being
// processed. If the end of the signature is reached, new data is
// randomly generated and the length of the signature extended.

static size_t read_more_data(void *buf, size_t buf_len, void *user_data)
{
	ReadCallbackData *cb_data = user_data;

	// Limit how much input data is read, and return end of file
	// when we hit the limit.

	if (cb_data->read >= cb_data->max_len) {
		return 0;
	}

	// If we reach the end of the signature, extend it.

	if (cb_data->read >= cb_data->signature->data_len) {
		extend_signature(cb_data->signature);
	}

	// Only copy a single byte at a time. This allows us to
	// accurately track how much of the signature is valid.

	memcpy(buf, cb_data->signature->data + cb_data->read, 1);
	++cb_data->read;

	return 1;
}

// Decode data from the specified signature block, using a decoder
// of the specified type.

static unsigned int run_fuzz_test(LHADecoderType *dtype,
                                  FuzzSignature *signature,
                                  unsigned int max_len)
{
	ReadCallbackData cb_data;
	uint8_t *read_buf;
	size_t result;
	void *handle;

	// Init decoder.

	cb_data.signature = signature;
	cb_data.read = 0;
	cb_data.max_len = max_len;

	handle = init_decoder(dtype, read_more_data, &cb_data);

	// Create a buffer into which to decompress data.

	read_buf = canary_malloc(dtype->max_read);
	assert(read_buf != NULL);

	for (;;) {
		memset(read_buf, 0, dtype->max_read);
		result = dtype->read(handle, read_buf);
		canary_check(read_buf);

		//printf("read: %i\n", result);
		if (result == 0) {
			break;
		}
	}

	// Destroy the decoder and free buffers.

	if (dtype->free != NULL) {
		dtype->free(handle);
	}

	canary_check(handle);
	canary_free(handle);
	canary_free(read_buf);

	//printf("Fuzz test complete, %i bytes read\n", cb_data.read);

	return cb_data.read;
}

static void fuzz_test(LHADecoderType *dtype, unsigned int iterations,
                      unsigned int max_len)
{
	FuzzSignature *signature;
	FuzzSignature *new_signature;
	FuzzSignature *best;
	unsigned int len;
	unsigned int i;

	signature = empty_signature();

	for (; iterations > 0; --iterations) {
		// Generate several new signatures, based on the current
		// signature, but with the last few bytes changed.
		// Then run each and see how far they get, cropping
		// them at the number of bytes that are read. The best
		// signature wins and becomes the new signature.

		best = NULL;

		for (i = 0; i < 4; ++i) {
			new_signature = child_signature(signature, 4);
			len = run_fuzz_test(dtype, new_signature, max_len);
			new_signature->data_len = len;

			if (len > signature->data_len
			 && (best == NULL || len > best->data_len)) {
				free_signature(best);
				best = new_signature;
			} else {
				free_signature(new_signature);
			}

			if (len >= max_len) {
				printf("\tReached limit.\n");
				free_signature(best);
				goto finished;
			}
		}

		// If one of the current signatures did better than
		// the current signature, replace it.

		if (best != NULL) {
			printf("\tNew signature: %i bytes\n", best->data_len);

			free_signature(signature);
			signature = best;
		}

		// If none of the new signatures succeeded, there may
		// be something in the existing signature preventing
		// us getting any further. Back up a bit and try again.

		else {
			printf("\tFailed to generate new signature.\n");
			if (signature->data_len >= 16) {
				signature->data_len -= 16;
			}
		}
	}

finished:
	free_signature(signature);
}

int main(int argc, char *argv[])
{
	LHADecoderType *dtype;
	unsigned int i;

	if (argc < 2) {
		printf("Usage: %s <decoder-type>\n", argv[0]);
		exit(-1);
	}

	dtype = lha_decoder_for_name(argv[1]);

	if (dtype == NULL) {
		fprintf(stderr, "Unknown decoder type '%s'\n", argv[1]);
		exit(-1);
	}

	srand(time(NULL));

	for (i = 0; ; ++i) {
		printf("Iteration %i:\n", i);
		fuzz_test(dtype, 100, MAX_FUZZ_LEN);
	}

	return 0;
}

