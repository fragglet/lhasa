/*

Copyright (c) 2023, Simon Howard

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


#ifndef LHASA_PUBLIC_LHA_WRITER_H
#define LHASA_PUBLIC_LHA_WRITER_H

#include <stdio.h>
#include "lha_output_stream.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file lha_writer.h
 *
 * @brief TODO
 */

/* TODO: Document this function. */
int lha_write_file(LHAOutputStream *out, LHAFileHeader *header, FILE *instream);

#ifdef __cplusplus
}
#endif

#endif /* #ifndef LHASA_PUBLIC_LHA_WRITER_H */

