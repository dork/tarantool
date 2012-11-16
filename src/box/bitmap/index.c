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

#include <errno.h>
#include <limits.h>

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

int bitmap_index_iterate_equals(
			struct bitmap_index *index,
			struct bitmap_expr *expr,
			void *key, size_t key_size)
{
	assert (index != NULL);
	assert (expr != NULL);

	bitmap_expr_clear(expr);

	if (bitmap_expr_add_group(expr, BITMAP_OP_AND, BITMAP_OP_NULL) != 0) {
		return -1;
	}

	for (size_t pos = 0; pos < key_size * CHAR_BIT; pos++) {
		size_t b = pos + 1;
		if (test_bit(key, pos)) {
			if (index->bitmaps[b] == NULL) {
				bitmap_expr_clear(expr);
				break;
			}

			if (bitmap_expr_group_add_bitmap(expr, 0,
				index->bitmaps[b], BITMAP_OP_NULL) != 0) {
				return -1;
			}
		} else {
			if (index->bitmaps[b] == NULL) {
				continue;
			}

			if (bitmap_expr_group_add_bitmap(expr, 0,
				index->bitmaps[b], BITMAP_OP_NOT) != 0) {
				return -1;
			}
		}
	}

	return 0;
}

int bitmap_index_iterate_all_set(
			struct bitmap_index *index,
			struct bitmap_expr *expr,
			void *key, size_t key_size)
{
	assert (index != NULL);
	assert (expr != NULL);

	bitmap_expr_clear(expr);

	if (bitmap_expr_add_group(expr, BITMAP_OP_AND, BITMAP_OP_NULL) != 0) {
		return -1;
	}

	if (bitmap_expr_group_add_bitmap(expr, 0,
		index->bitmaps[0], BITMAP_OP_NULL) != 0) {
		return -1;
	}

	size_t pos = 0;
	while(find_next_set_bit(key, key_size, &pos) == 0) {
		size_t b = pos + 1;
		if (index->bitmaps[b] == NULL || b >= index->bitmaps_size) {
			bitmap_expr_clear(expr);
			break;
		}
		if (bitmap_expr_group_add_bitmap(expr, 0,
			index->bitmaps[b], BITMAP_OP_NULL) != 0) {
			return -1;
		}

		pos++;
	}

	return 0;
}

int bitmap_index_iterate_any_set(
			struct bitmap_index *index,
			struct bitmap_expr *expr,
			void *key, size_t key_size)
{
	assert (index != NULL);
	assert (expr != NULL);

	bitmap_expr_clear(expr);

	if (bitmap_expr_add_group(expr, BITMAP_OP_OR, BITMAP_OP_NULL) != 0) {
		return -1;
	}

	for (size_t pos = 0;
	     find_next_set_bit(key, key_size, &pos) == 0;
	     pos++) {
		size_t b = pos + 1;
		if (index->bitmaps[b] == NULL) {
			/* do not have bitmap for this bit */
			continue;
		}

		if (bitmap_expr_group_add_bitmap(expr, 0,
			index->bitmaps[b], BITMAP_OP_NULL) != 0) {
			return -1;
		}
	}

	if (bitmap_expr_add_group(expr, BITMAP_OP_AND, BITMAP_OP_NULL) != 0) {
		return -1;
	}

	if (bitmap_expr_group_add_bitmap(expr, 1,
		index->bitmaps[0], BITMAP_OP_NULL) != 0) {
		return -1;
	}

	return 0;
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
