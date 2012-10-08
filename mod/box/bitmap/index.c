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

int bitmap_index_new(struct bitmap_index **pindex, size_t initial_size)
{
	int rc = -1;

	*pindex = calloc(1, sizeof(struct bitmap_index));
	if (*pindex == NULL) {
		goto error_0;
	}

	struct bitmap_index *index = *pindex;
	index->bitmaps = calloc(initial_size+1, sizeof(*index->bitmaps));
	if (index->bitmaps == NULL) {
		goto error_1;
	}

	index->bitmaps_size = initial_size+1;
	if (bitmap_new(&(index->bitmaps[0])) < 0) {
		goto error_2;
	}

	return 0;

	/* error handling */
error_2:
	free(index->bitmaps);
error_1:
	free(*pindex);
error_0:
	*pindex = NULL;
	return rc;
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

int bitmap_index_insert(struct bitmap_index *index,
			void *key, size_t key_size,
			size_t value) {
	size_t bitmaps_size_new = 1 + key_size * CHAR_BIT;
	if (bitmaps_size_new > index->bitmaps_size) {
		/* Resize index */
		struct bitmap **bitmaps = realloc(index->bitmaps,
				bitmaps_size_new * sizeof(*index->bitmaps));
		if (bitmaps == NULL) {
			return -1;
		}

		for (size_t b = index->bitmaps_size; b < bitmaps_size_new; b++) {
			bitmaps[b] = NULL;
		}
		index->bitmaps = bitmaps;
		index->bitmaps_size = bitmaps_size_new;
	}

	/* Set bits in bitmaps */
	size_t pos = 0;
	while(next_bit(key, key_size, &pos)) {
		size_t b = pos + 1;
		if (index->bitmaps[b] == NULL) {
			if (bitmap_new(&index->bitmaps[b]) < 0) {
				return -1;
			}
		}

		/* TODO(roman): rollback previous operations on error */
		if (bitmap_set(index->bitmaps[b], value, 1) < 0) {
			return -1;
		}
		pos++;
	}

	if (bitmap_set(index->bitmaps[0], value, 1) < 0) {
		return -1;
	}

	return 0;
}

int bitmap_index_remove(struct bitmap_index *index,
			void *key, size_t key_size,
			size_t value) {
	size_t bitmaps_size_needed = 1 + key_size * CHAR_BIT;
	if (bitmaps_size_needed > index->bitmaps_size) {
		return 0;
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

		/* TODO(roman): rollback previous operations on error */
		if (bitmap_set(index->bitmaps[b], value, 0) < 0) {
			return -1;
		}
		pos++;
	}

	if (bitmap_set(index->bitmaps[0], value, 0) < 0) {
		return -1;
	}

	return 0;
}

int bitmap_index_iterate(struct bitmap_index *index,
			struct bitmap_iterator **pit,
			void *key, size_t key_size) {
	int rc = -1;
	*pit = NULL;

	struct bitmap **used_bitmaps = calloc(index->bitmaps_size,
					sizeof(*used_bitmaps));
	if (used_bitmaps == NULL) {
		goto free_0;
	}

	int *used_bitmap_flags = calloc(index->bitmaps_size,
					sizeof(*used_bitmap_flags));
	if (used_bitmap_flags == NULL) {
		goto free_1;
	}

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

	if (bitmap_iterator_newn(pit, used_bitmaps, used_bitmaps_size,
			used_bitmap_flags, result_flags) < 0) {
		goto free_1;
	}

	rc = 0;


free_1:
	free(used_bitmap_flags);
free_0:
	free(used_bitmaps);

	return rc;
}

bool bitmap_index_contains_value(struct bitmap_index *index, size_t value) {
	return bitmap_get(index->bitmaps[0], value);
}

size_t bitmap_index_size(struct bitmap_index *index)
{
	return bitmap_cardinality(index->bitmaps[0]);
}

