/*
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the
 *    following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * <COPYRIGHT HOLDER> OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "index.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <limits.h>
#include <assert.h>

#include "bitmap_p.h"

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
	while(find_next_set_bit(key, key_size, &pos) == 0) {
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
	while(find_next_set_bit(key, key_size, &pos) == 0) {
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

static
int bitmap_index_iterate_equals(struct bitmap_index *index,
			       struct bitmap_iterator **pit,
			       void *key, size_t key_size)
{
	int rc = 0;

	size_t key_bits = (key_size * CHAR_BIT);
	size_t elements_size = 1;

	for (size_t pos = 0; pos < key_bits; pos++) {
		size_t b = pos + 1;
		if (test_bit(key, pos)) {
			if (index->bitmaps[b] == NULL ||
				index->bitmaps_size <= pos) {
				elements_size = 0;
				break;
			}

			elements_size++;
		} else {
			if (index->bitmaps[b] == NULL
				|| index->bitmaps_size <= pos) {
				continue;
			}
			elements_size++;
		}
	}

	struct bitmap_iterator_group *set;
	size_t set_size = sizeof(*set) + sizeof(*(set->elements)) *
			elements_size;
	set = calloc(1, set_size);
	if (!set) {
		return -1;
	}

	set->reduce_op = BITMAP_OP_AND;
	set->post_op = BITMAP_OP_NULL;
	set->elements_size = 0;

	if (elements_size == 0) {
		goto exit;
	}

	set->elements[set->elements_size].bitmap = index->bitmaps[0];
	set->elements[set->elements_size].pre_op = BITMAP_OP_NULL;
	set->elements_size++;

	for (size_t pos = 0; pos < key_size * CHAR_BIT; pos++) {
		size_t b = pos + 1;
		if (test_bit(key, pos)) {
			assert (index->bitmaps[b] != NULL);

			set->elements[set->elements_size].bitmap =
					index->bitmaps[b];
			set->elements[set->elements_size].pre_op =
					BITMAP_OP_NULL;
			set->elements_size++;
		} else {
			if (index->bitmaps[b] == NULL) {
				continue;
			}

			set->elements[set->elements_size].bitmap =
					index->bitmaps[b];
			set->elements[set->elements_size].pre_op =
					BITMAP_OP_NOT;
			set->elements_size++;
		}
	}

exit:
	assert(set->elements_size == elements_size);

	rc = bitmap_iterator_newgroup(pit, &set, 1);
	free(set);

	return rc;
}

static
int bitmap_index_iterate_contains(struct bitmap_index *index,
				     struct bitmap_iterator **pit,
				     void *key, size_t key_size)
{
	size_t used_bitmaps_count = 1;

	size_t pos = 0;
	while(find_next_set_bit(key, key_size, &pos) == 0) {
		size_t b = pos + 1;
		if (index->bitmaps[b] == NULL || b >= index->bitmaps_size) {
			/* do not have bitmap for this bit */
			used_bitmaps_count = 0;
			break;
		}

		used_bitmaps_count++;
		pos++;
	}

	struct bitmap_iterator_group *set;
	size_t set_size = sizeof(*set) + sizeof(*(set->elements)) *
			used_bitmaps_count;
	set = calloc(1, set_size);
	if (!set) {
		return -1;
	}

	set->reduce_op = BITMAP_OP_AND;
	set->post_op = BITMAP_OP_NULL;
	set->elements_size = used_bitmaps_count;

	size_t b = 0;
	if (used_bitmaps_count > 0) {
		set->elements[b].bitmap = index->bitmaps[0];
		set->elements[b].pre_op = BITMAP_OP_NULL;
		b++;

		pos = 0;
		while(find_next_set_bit(key, key_size, &pos) == 0) {
			set->elements[b].bitmap = index->bitmaps[pos + 1];
			set->elements[b].pre_op = BITMAP_OP_NULL;
			b++;

			pos++;
		}
	}

	int rc = bitmap_iterator_newgroup(pit, &set, 1);

	free(set);

	return rc;
}

static
int bitmap_index_iterate_intersects(struct bitmap_index *index,
				    struct bitmap_iterator **pit,
				    void *key, size_t key_size)
{
	int rc = 0;

	struct bitmap_iterator_group *set1;
	size_t set_size1 = sizeof(*set1) + sizeof(*(set1->elements)) *
			(index->bitmaps_size - 1);
	set1 = calloc(1, set_size1);
	if (!set1) {
		rc = -1;
		goto error_1;
	}

	set1->reduce_op = BITMAP_OP_OR;
	set1->post_op = BITMAP_OP_NULL;
	set1->elements_size = 0;

	for (size_t pos = 0;
	     find_next_set_bit(key, key_size, &pos) == 0;
	     pos++) {
		size_t b = pos + 1;
		if (index->bitmaps[b] == NULL) {
			/* do not have bitmap for this bit */
			continue;
		}
		set1->elements[set1->elements_size].bitmap = index->bitmaps[b];
		set1->elements[set1->elements_size].pre_op = BITMAP_OP_NULL;
		set1->elements_size++;
	}

	struct bitmap_iterator_group *set2;
	size_t set_size2 = sizeof(*set2) + sizeof(*(set2->elements));
	set2 = calloc(1, set_size2);
	if (!set2) {
		rc = -1;
		goto error_2;
	}

	set2->reduce_op = BITMAP_OP_AND;
	set2->post_op = BITMAP_OP_NULL;
	set2->elements_size = 1;
	set2->elements[0].bitmap = index->bitmaps[0];
	set2->elements[0].pre_op = BITMAP_OP_NULL;

	struct bitmap_iterator_group *groups[] = { set1, set2 };

	rc = bitmap_iterator_newgroup(pit, groups, 2);

	free(set2);
error_2:
	free(set1);
error_1:
	return rc;
}

int bitmap_index_iterate(struct bitmap_index *index,
			struct bitmap_iterator **pit,
			void *key, size_t key_size,
			int match_type) {


	switch((enum bitmap_index_match_type) match_type) {
	case BITMAP_INDEX_MATCH_EQUALS:
		return bitmap_index_iterate_equals(index, pit, key, key_size);
	case BITMAP_INDEX_MATCH_CONTAINS:
		return bitmap_index_iterate_contains(index, pit, key, key_size);
	case BITMAP_INDEX_MATCH_INTERSECTS:
		return bitmap_index_iterate_intersects(index, pit, key, key_size);
	default:
		/* not implemented */
		errno = ENOTSUP;
		return -2;
	}
}

bool bitmap_index_contains_value(struct bitmap_index *index, size_t value) {
	return bitmap_get(index->bitmaps[0], value);
}

size_t bitmap_index_size(struct bitmap_index *index)
{
	return bitmap_cardinality(index->bitmaps[0]);
}

#if defined(DEBUG)
void bitmap_index_dump(struct bitmap_index *index,
		       int verbose, FILE *stream)
{
	struct bitmap_stat stat;
	bitmap_stat(index->bitmaps[0], &stat);

	fprintf(stream, "BitmapIndex %p\n", index);
	fprintf(stream, "{\n");
	fprintf(stream, "    " "features    = ");
	fprintf(stream, "%cAVX ", stat.have_avx ? '+' : '-');
	fprintf(stream, "%cSSE2 ", stat.have_sse2 ? '+' : '-');
	fprintf(stream, "%cCTZ ", stat.have_ctz ? '+' : '-');
	fprintf(stream, "%cPOPCNT ", stat.have_popcnt ? '+' : '-');
	fprintf(stream, "\n");
	fprintf(stream, "    " "word_bit    = %-20d\n", stat.word_bit);
	fprintf(stream, "    " "page_bit    = %-20d * %d = %d\n",
		stat.words_per_page,
		stat.word_bit,
		stat.page_bit);

	fprintf(stream, "    " "size        = %-20zu"
			" // number of values in the index\n",
		bitmap_index_size(index));

	fprintf(stream, "    " "/* bitmaps */\n");
	fprintf(stream, "    "
		"// bits i nbitmap #0 set for each value in the index\n");
	fprintf(stream, "    "
		"// bitmap #(i) => bit #(i-1) in key\n");

	/* can overflow, but what can i do? */
	size_t total_capacity = 0;
	size_t total_cardinality = 0;
	size_t total_mem_pages = 0;
	size_t total_mem_other = 0;
	size_t total_pages = 0;

	for (size_t b = 0; b < index->bitmaps_size; b++) {
		if (index->bitmaps[b] == NULL) {
			continue;
		}

		bitmap_stat(index->bitmaps[b], &stat);
		fprintf(stream, "    " "bitmap #%-4zu {", b);
		if (stat.capacity > 0) {
			fprintf(stream,
				"utilization   = %8.4f%% (%zu / %zu) ",
				(float) stat.cardinality*1e2 / (stat.capacity),
				stat.cardinality,
				stat.capacity
			);
		} else {
			fprintf(stream,
				"utilization   = undefined (%zu / %zu) ",
				stat.cardinality,
				stat.capacity
			);
		}
		fprintf(stream, "pages = %zu", stat.pages);

		fprintf(stream, "}\n");

		total_cardinality += stat.cardinality;
		total_capacity += stat.capacity;
		total_mem_pages += stat.mem_pages;
		total_mem_other += stat.mem_other;
		total_pages += stat.pages;
	}
	fprintf(stream, "    " "/* end of bitmaps */\n");

	total_mem_other += sizeof(struct bitmap_index) +
			sizeof(struct bitmap*) * index->bitmaps_size;

	fprintf(stream, "    " "pages       = %zu\n", total_pages);
	fprintf(stream, "    " "capacity    = %zu (%zu * %d)\n",
		total_capacity,
		total_pages,
		stat.page_bit);
	fprintf(stream, "    "
		"cardinality = %zu // total for all bitmaps\n",
		total_cardinality);
	if (total_capacity > 0) {
		fprintf(stream, "    "
			"utilization = %4f%% (%zu / %zu)\n",
			(float) total_cardinality * 100.0 / (total_capacity),
			total_cardinality,
			total_capacity
			);
	} else {
		fprintf(stream, "    "
			"utilization = undefined\n"	);
	}

	fprintf(stream, "    " "mem_pages   = %zu bytes // with tree data\n",
		total_mem_pages);
	fprintf(stream, "    " "mem_other   = %zu bytes\n",
		total_mem_other);
	fprintf(stream, "    " "mem_total   = %zu bytes\n",
		total_mem_other + total_mem_pages);

	if (total_cardinality > 0) {
		fprintf(stream, "    " "bit_per_val = %-10.4f\n",
			(float) total_cardinality / bitmap_index_size(index));
		fprintf(stream, "    "
			"density     = %-8.4f // bytes per value's bit\n",
			(float) (total_mem_other + total_mem_pages) /
			total_cardinality);
		fprintf(stream, "    "
			"density     = %-8.4f // bytes per value\n",
			(float) (total_mem_other + total_mem_pages) /
			bitmap_index_size(index));

	} else {
		fprintf(stream, "    " "bit_per_val = undefined\n");
		fprintf(stream, "    "
			"density     = undefined\n");
	}

	if (verbose > 0) {
		for (size_t b = 0; b < index->bitmaps_size; b++) {
			if (index->bitmaps[b] == NULL) {
				continue;
			}

			fprintf(stream, "Bitmap #%-6zu dump\n", b);
			bitmap_dump(index->bitmaps[b], verbose, stream);
		}
	}
	fprintf(stream, "}\n");
}

#endif /* defined(DEBUG) */
