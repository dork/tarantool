/*
 * Copyright (C) 2012 Mail.RU
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "index.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <limits.h>

#include "say.h"

static
bool next_bit(char *data, size_t data_len, size_t *pos) {
	size_t cur_pos = *pos / CHAR_BIT;
	size_t cur_offset = *pos % CHAR_BIT;

	while (cur_pos < data_len) {
		char c = data[cur_pos] >> cur_offset;
		if (c & 0x1) {
			*pos = cur_pos * CHAR_BIT + cur_offset;
			return true;
		}

		cur_offset++;
		if (cur_offset >= CHAR_BIT) {
			cur_offset = 0;
			cur_pos++;
			continue;
		}
	}

	*pos = SIZE_MAX;
	return false;
}

struct bitmap_index {
	struct bitmap **bitmaps;
	size_t bitmaps_size;
};

void bitmap_index_new(struct bitmap_index **pindex, size_t initial_size)
{
	*pindex = calloc(1, sizeof(struct bitmap_index));
	struct bitmap_index *index = *pindex;
	index->bitmaps = calloc(initial_size+1, sizeof(*index->bitmaps));
	index->bitmaps_size = initial_size+1;
	bitmap_new(&(index->bitmaps[0]));
}

void bitmap_index_free(struct bitmap_index **pindex)
{
	struct bitmap_index *index = *pindex;
	if (index != NULL) {
		for(size_t b = 0; b < index->bitmaps_size; b++) {
			if (index->bitmaps[b] == NULL) {
				continue;
			}

			bitmap_free(&(index->bitmaps[b]));
		}
		free(index);
	}

	*pindex = NULL;
}

void bitmap_index_insert(struct bitmap_index *index,
			void *key, size_t key_size,
			size_t value) {
	size_t bitmaps_size_new = 1 + key_size * CHAR_BIT;
	if (bitmaps_size_new > index->bitmaps_size) {
		/* Resize index */
		struct bitmap **bitmaps = realloc(index->bitmaps,
				bitmaps_size_new * sizeof(*index->bitmaps));
		for (size_t b = index->bitmaps_size; b < bitmaps_size_new; b++) {
			bitmaps[b] = NULL;
		}
		index->bitmaps = bitmaps;
		index->bitmaps_size = bitmaps_size_new;

#ifdef DEBUG
		say_debug("Resize index to: %zu\n", index->bitmaps_size);
#endif
	}

	/* Set bits in bitmaps */
	size_t pos = 0;
	while(next_bit(key, key_size, &pos)) {
		size_t b = pos + 1;
		if (index->bitmaps[b] == NULL) {
			bitmap_new(&index->bitmaps[b]);
		}

		bitmap_set(index->bitmaps[b], value, 1);
		pos++;
	}

	bitmap_set(index->bitmaps[0], value, 1);
}

void bitmap_index_remove(struct bitmap_index *index,
			void *key, size_t key_size,
			size_t value) {
	size_t bitmaps_size_needed = 1 + key_size * CHAR_BIT;
	if (bitmaps_size_needed > index->bitmaps_size) {
		return;
	}

	size_t pos = 0;
	while(next_bit(key, key_size, &pos)) {
		size_t b = pos + 1;
		if (b >= index->bitmaps_size) {
			break;
		}

		if (index->bitmaps[b] == NULL) {
			continue;
		}

		bitmap_set(index->bitmaps[b], value, 0);
		pos++;
	}

	bitmap_set(index->bitmaps[0], value, 0);
}

void bitmap_index_iter(struct bitmap_index *index,
			struct bitmap_iterator **pit,
			void *key, size_t key_size) {

	struct bitmap **used_bitmaps = calloc(index->bitmaps_size,
					sizeof(*used_bitmaps));
	int *used_bitmap_flags = calloc(index->bitmaps_size,
					sizeof(*used_bitmap_flags));
	size_t used_bitmaps_size = 0;
	int result_flags = 0;

	used_bitmaps[0] = index->bitmaps[0];
	used_bitmaps_size++;

	size_t bitmaps_size_needed = 1 + key_size * CHAR_BIT;
	if (bitmaps_size_needed <= index->bitmaps_size) {
		size_t pos = 0;
		while(next_bit(key, key_size, &pos)) {
			size_t b = pos + 1;
			if (index->bitmaps[b] == NULL) {
				/* do not have bitmap for this bit */
				used_bitmaps_size = 0;
				break;
			}

			used_bitmaps[used_bitmaps_size++] = index->bitmaps[b];

			pos++;
		}
	} else {
		used_bitmaps_size = 0;
	}

	bitmap_iterator_newn(pit, used_bitmaps, used_bitmaps_size,
			used_bitmap_flags, result_flags);

	free(used_bitmap_flags);
	free(used_bitmaps);
}

bool bitmap_index_contains_value(struct bitmap_index *index, size_t value) {
	return bitmap_get(index->bitmaps[0], value);
}
