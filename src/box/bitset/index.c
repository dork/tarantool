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

#include "bitset_p.h"

struct bitset_index {
	struct bitset **bitsets;
	size_t bitsets_size;
};

struct bitset_index *
bitset_index_new()
{
	return bitset_index_new2(32);
}

struct bitset_index *
bitset_index_new2(size_t initial_size)
{
	struct bitset_index *index = calloc(1, sizeof(*index));
	if (index == NULL)
		goto error_0;

	index->bitsets = calloc(initial_size + 1, sizeof(*index->bitsets));
	if (index->bitsets == NULL)
		goto error_1;

	index->bitsets_size = initial_size + 1;
	index->bitsets[0] = bitset_new();
	if (index->bitsets[0] == NULL)
		goto error_2;

	return index;

	/* error handling */
error_2:
	free(index->bitsets);
error_1:
	free(index);
error_0:
	return NULL;
}

void
bitset_index_delete(struct bitset_index *index)
{
	if (!index)
		return;

	for(size_t b = 0; b < index->bitsets_size; b++) {
		if (index->bitsets[b] == NULL)
			continue;

		bitset_delete(index->bitsets[b]);
	}

	free(index);
}

static int
bitset_index_reserve(struct bitset_index *index, size_t bits_required)
{
	if (bits_required <= index->bitsets_size)
		return 0;

	/* Resize the index */
	size_t bitsets_size_new = index->bitsets_size;
	while (bitsets_size_new < bits_required)
		bitsets_size_new *= 2;

	struct bitset **bitsets = realloc(index->bitsets,
			bitsets_size_new * sizeof(*index->bitsets));

	if (bitsets == NULL)
		return -1;

	memset(bitsets + index->bitsets_size, 0, sizeof(*index->bitsets) *
	       (bitsets_size_new - index->bitsets_size));
	/*
	for (size_t b = index->bitsets_size; b < bitsets_size_new; b++)
		bitsets[b] = NULL;
	*/

	index->bitsets = bitsets;
	index->bitsets_size = bitsets_size_new;

	return 0;
}

int
bitset_index_insert(struct bitset_index *index, void *key, size_t key_size,
		    size_t value)
{
	const size_t bits_required = 1 + key_size * CHAR_BIT;
	if (bitset_index_reserve(index, bits_required) != 0)
		return -1;

	/* Set bits in bitsets */
	for(size_t pos = 0; find_next_set_bit(key, key_size, &pos) == 0;pos++) {
		if (index->bitsets[pos+1] == NULL) {
			index->bitsets[pos+1] = bitset_new();
			if (index->bitsets[pos+1] == NULL)
				goto rollback;
		}

		if (bitset_set(index->bitsets[pos+1], value, 1) < 0)
			goto rollback;
	}

	if (bitset_set(index->bitsets[0], value, 1) < 0)
		goto rollback;

	return 0;

rollback:
	/* Rollback changes */
	for(size_t pos = 0; find_next_set_bit(key, key_size, &pos) == 0;pos++) {
		if (index->bitsets[pos+1] == NULL)
			continue;

		bitset_set(index->bitsets[pos+1], value, 0);
	}

	bitset_set(index->bitsets[0], value, 0);

	return -1;
}

void
bitset_index_remove_value(struct bitset_index *index, size_t value)
{
	for (size_t b = 0; b < index->bitsets_size; b++) {
		if (index->bitsets[b] == NULL)
			continue;

		/* Ignore all errors here */
		bitset_set(index->bitsets[b], value, 0);
	}
}

bool
bitset_index_contains_value(struct bitset_index *index, size_t value)
{
	return bitset_get(index->bitsets[0], value);
}

int
bitset_index_iterate_all(struct bitset_index *index,
			 struct bitset_expr *expr)
{
	assert (index != NULL);
	assert (expr != NULL);

	bitset_expr_clear(expr);

	if (bitset_expr_add_group(expr, BITSET_OP_AND, BITSET_OP_NULL) != 0) {
		return -1;
	}

	if (bitset_expr_group_add_bitset(expr, 0, index->bitsets[0],
					 BITSET_OP_NULL) != 0) {
		return -1;
	}

	return 0;
}

int
bitset_index_iterate_equals(struct bitset_index *index,
			    struct bitset_expr *expr,
			    void *key, size_t key_size)
{
	assert (index != NULL);
	assert (expr != NULL);

	bitset_expr_clear(expr);

	if (bitset_expr_add_group(expr, BITSET_OP_AND, BITSET_OP_NULL) != 0) {
		return -1;
	}

	for (size_t pos = 0; pos < key_size * CHAR_BIT; pos++) {
		size_t b = pos + 1;
		if (test_bit(key, pos)) {
			if (index->bitsets[b] == NULL) {
				bitset_expr_group_clear(expr, 0);
				break;
			}

			if (bitset_expr_group_add_bitset(expr, 0,
				index->bitsets[b], BITSET_OP_NULL) != 0) {
				return -1;
			}
		} else {
			if (index->bitsets[b] == NULL) {
				continue;
			}

			if (bitset_expr_group_add_bitset(expr, 0,
				index->bitsets[b], BITSET_OP_NOT) != 0) {
				return -1;
			}
		}
	}

	if (bitset_expr_group_add_bitset(expr, 0,
		index->bitsets[0], BITSET_OP_NULL) != 0) {
		return -1;
	}

	return 0;
}

static inline int
bitset_index_iterate_all_set2(struct bitset_index *index,
			      struct bitset_expr *expr,
			      void *key, size_t key_size,
			      enum bitset_unary_op pre_op)
{
	assert (index != NULL);
	assert (expr != NULL);

	bitset_expr_clear(expr);

	if (bitset_expr_add_group(expr, BITSET_OP_AND, BITSET_OP_NULL) != 0) {
		return -1;
	}

	if (bitset_expr_group_add_bitset(expr, 0,
		index->bitsets[0], BITSET_OP_NULL) != 0) {
		return -1;
	}

	for (size_t pos = 0; find_next_set_bit(key, key_size, &pos) == 0;pos++){
		if ((pos+1) >= index->bitsets_size ||
		    index->bitsets[pos+1] == NULL) {
			bitset_expr_clear(expr);
			break;
		}

		if (bitset_expr_group_add_bitset(expr, 0,
			index->bitsets[pos+1], pre_op) != 0) {
			return -1;
		}

		pos++;
	}

	return 0;
}

int
bitset_index_iterate_all_set(struct bitset_index *index,
			     struct bitset_expr *expr,
			     void *key, size_t key_size)
{
	return bitset_index_iterate_all_set2(index, expr, key, key_size,
					    BITSET_OP_NULL);
}

int
bitset_index_iterate_all_not_set(struct bitset_index *index,
				 struct bitset_expr *expr,
				 void *key, size_t key_size)
{
	return bitset_index_iterate_all_set2(index, expr, key, key_size,
					    BITSET_OP_NOT);
}

static inline int
bitset_index_iterate_any_set2(struct bitset_index *index,
			      struct bitset_expr *expr,
			      void *key, size_t key_size,
			      enum bitset_unary_op pre_op)
{
	assert (index != NULL);
	assert (expr != NULL);

	bitset_expr_clear(expr);

	if (bitset_expr_add_group(expr, BITSET_OP_OR, BITSET_OP_NULL) != 0) {
		return -1;
	}

	for (size_t pos = 0; find_next_set_bit(key, key_size, &pos) == 0;pos++){
		if ((pos+1) >= index->bitsets_size ||
		    index->bitsets[pos+1] == NULL) {
			/* do not have bitset for this bit */
			continue;
		}

		if (bitset_expr_group_add_bitset(expr, 0,
			index->bitsets[pos+1], pre_op) != 0) {
			return -1;
		}
	}

	if (bitset_expr_add_group(expr, BITSET_OP_AND, BITSET_OP_NULL) != 0) {
		return -1;
	}

	if (bitset_expr_group_add_bitset(expr, 1,
		index->bitsets[0], BITSET_OP_NULL) != 0) {
		return -1;
	}

	return 0;
}

int
bitset_index_iterate_any_set(struct bitset_index *index,
			     struct bitset_expr *expr,
			     void *key, size_t key_size)
{
	return bitset_index_iterate_any_set2(index, expr, key, key_size,
					     BITSET_OP_NULL);
}

int
bitset_index_iterate_any_not_set(struct bitset_index *index,
				 struct bitset_expr *expr,
				 void *key, size_t key_size)
{
	return bitset_index_iterate_any_set2(index, expr, key, key_size,
					     BITSET_OP_NOT);
}

size_t
bitset_index_size(struct bitset_index *index)
{
	return bitset_cardinality(index->bitsets[0]);
}

#if defined(DEBUG)
void
bitset_index_dump(struct bitset_index *index,
		       int verbose, FILE *stream)
{
	struct bitset_stat stat;
	bitset_stat(index->bitsets[0], &stat);

	fprintf(stream, "BitsetIndex %p\n", index);
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
		bitset_index_size(index));

	fprintf(stream, "    " "/* bitsets */\n");
	fprintf(stream, "    "
		"// bits i nbitset #0 set for each value in the index\n");
	fprintf(stream, "    "
		"// bitset #(i) => bit #(i-1) in key\n");

	/* can overflow, but what can i do? */
	size_t total_capacity = 0;
	size_t total_cardinality = 0;
	size_t total_mem_pages = 0;
	size_t total_mem_other = 0;
	size_t total_pages = 0;

	for (size_t b = 0; b < index->bitsets_size; b++) {
		if (index->bitsets[b] == NULL) {
			continue;
		}

		bitset_stat(index->bitsets[b], &stat);
		fprintf(stream, "    " "bitset #%-4zu {", b);
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
	fprintf(stream, "    " "/* end of bitsets */\n");

	total_mem_other += sizeof(struct bitset_index) +
			sizeof(struct bitset*) * index->bitsets_size;

	fprintf(stream, "    " "pages       = %zu\n", total_pages);
	fprintf(stream, "    " "capacity    = %zu (%zu * %d)\n",
		total_capacity,
		total_pages,
		stat.page_bit);
	fprintf(stream, "    "
		"cardinality = %zu // total for all bitsets\n",
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
			(float) total_cardinality / bitset_index_size(index));
		fprintf(stream, "    "
			"density     = %-8.4f // bytes per value's bit\n",
			(float) (total_mem_other + total_mem_pages) /
			total_cardinality);
		fprintf(stream, "    "
			"density     = %-8.4f // bytes per value\n",
			(float) (total_mem_other + total_mem_pages) /
			bitset_index_size(index));

	} else {
		fprintf(stream, "    " "bit_per_val = undefined\n");
		fprintf(stream, "    "
			"density     = undefined\n");
	}

	if (verbose > 0) {
		for (size_t b = 0; b < index->bitsets_size; b++) {
			if (index->bitsets[b] == NULL) {
				continue;
			}

			fprintf(stream, "Bitmap #%-6zu dump\n", b);
			bitset_dump(index->bitsets[b], verbose, stream);
		}
	}
	fprintf(stream, "}\n");
}

#endif /* defined(DEBUG) */
