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

#include "bitmap.h"
#include "bitmap_p.h"

#include <malloc.h>
#include <errno.h>

#if defined(ENABLE_SSE2) || defined(ENABLE_AVX)
bitmap_word_t word_bitmask(int offset)
{
	/*
	 * SSE2/AVX does not have BIT shift instruction
	 * so we prepare mask from the parts
	 */
#if defined(ENABLE_AVX) && __SIZEOF_SIZE_T__ == 8
	switch(offset / 64) {
	case 0:
		return _mm256_set_epi64x(0, 0, 0, 1UL << (offset % 64));
	case 1:
		return _mm256_set_epi64x(0, 0, 1UL << (offset % 64), 0);
	case 2:
		return _mm256_set_epi64x(0, 1UL << (offset % 64), 0, 0);
	case 3:
		return _mm256_set_epi64x(1UL << (offset % 64), 0, 0, 0);
	}

	return word_set_zeros();
#elif defined(ENABLE_AVX) && __SIZEOF_SIZE_T__ == 4
	switch(offset / 32) {
	case 0: return _mm256_set_epi32(0, 0, 0, 0,
					0, 0, 0, 1U << (offset % 32));
	case 1: return _mm256_set_epi32(0, 0, 0, 0,
					0, 0, 1U << (offset % 32), 0);
	case 2: return _mm256_set_epi32(0, 0, 0, 0,
					0, 1U << (offset % 32), 0, 0);
	case 3: return _mm256_set_epi32(0, 0, 0, 0,
					1U << (offset % 32), 0, 0, 0);
	case 4: return _mm256_set_epi32(0, 0, 0, 1U << (offset % 32),
					0, 0, 0, 0);
	case 5: return _mm256_set_epi32(0, 0, 1U << (offset % 32), 0,
					0, 0, 0, 0);
	case 6: return _mm256_set_epi32(0, 1U << (offset % 32), 0, 0,
					0, 0, 0, 0);
	case 7: return _mm256_set_epi32(1U << (offset % 32), 0, 0, 0,
					0, 0, 0, 0);
	}

	return word_set_zeros();
#elif defined(ENABLE_SSE2) && __SIZEOF_SIZE_T__ == 8
	switch (offset / 64) {
	case 0:
		return _mm_set_epi64x(0, 1UL << (offset % 64));
	case 1:
		return _mm_set_epi64x(1UL << (offset % 64), 0);
	}

	return word_set_zeros();
#elif defined(ENABLE_SSE2) && __SIZEOF_SIZE_T__ == 4
	switch (offset / 32) {
	case 0:
		return _mm_set_epi32(0, 0, 0, 1U << (offset % 32));
	case 1:
		return _mm_set_epi32(0, 0, 1U << (offset % 32), 0);
	case 2:
		return _mm_set_epi32(0, 1U << (offset % 32), 0, 0);
	case 3:
		return _mm_set_epi32(1U << (offset % 32), 0, 0, 0);
	}

	return word_set_zeros();
#else
#error SSE2/AVX with given __SIZEOF_SIZE_T__ is not supported
#endif
}
#endif /* defined(ENABLE_SSE2) || defined(ENABLE_AVX) */

#if   defined(ENABLE_AVX)
static inline
int *bit_index_u256(const u256 x, int *indexes, int offset)
{
#if   WORD_BIT == 64
	union __cast {
		u256 u256;
		u64  u64[4];
	} w;
	w.u256 = x;
	indexes = bit_index_u64(w.u64[0], indexes, offset + 0);
	indexes = bit_index_u64(w.u64[1], indexes, offset + 64);
	indexes = bit_index_u64(w.u64[2], indexes, offset + 128);
	indexes = bit_index_u64(w.u64[3], indexes, offset + 192);
	return indexes;
#elif WORD_BIT == 32
	union __cast {
		u256 u256;
		u32 u32[8];
	} w;
	w.u256 = x;
	indexes = bit_index_u32(w.u32[0], indexes, offset + 0);
	indexes = bit_index_u32(w.u32[1], indexes, offset + 32);
	indexes = bit_index_u32(w.u32[2], indexes, offset + 64);
	indexes = bit_index_u32(w.u32[3], indexes, offset + 96);
	indexes = bit_index_u32(w.u32[4], indexes, offset + 128);
	indexes = bit_index_u32(w.u32[5], indexes, offset + 160);
	indexes = bit_index_u32(w.u32[6], indexes, offset + 192);
	indexes = bit_index_u32(w.u32[7], indexes, offset + 224);
	return indexes;
#else
#error Unsupported WORD_BIT
#endif
}
#endif

#if   defined(ENABLE_SSE2)
static inline
int *bit_index_u128(const u128 x, int *indexes, int offset)
{
#if   WORD_BIT == 64
	union __cast {
		u128 u128;
		u64 u64[2];
	} w;
	w.u128 = x;
	indexes = bit_index_u64(w.u64[0], indexes, offset + 0);
	indexes = bit_index_u64(w.u64[1], indexes, offset + 64);
	return indexes;
#elif WORD_BIT == 32
	union __cast {
		u128 u128;
		u32 u32[4];
	} w;
	w.u128 = x;
	indexes = bit_index_u32(w.u32[0], indexes, offset + 0);
	indexes = bit_index_u32(w.u32[1], indexes, offset + 32);
	indexes = bit_index_u32(w.u32[2], indexes, offset + 64);
	indexes = bit_index_u32(w.u32[3], indexes, offset + 96);
	return indexes;
#else
#error Unsupported WORD_BIT
#endif
}
#endif

int *word_index(const bitmap_word_t word, int *indexes, int offset)
{
#if     BITMAP_WORD_BIT == 32
	return bit_index_u32(word, indexes, offset);
#elif   BITMAP_WORD_BIT == 64
	return bit_index_u64(word, indexes, offset);
#elif   BITMAP_WORD_BIT == 128
	return bit_index_u128(word, indexes, offset);
#elif   BITMAP_WORD_BIT == 256
	return bit_index_u256(word, indexes, offset);
#else
#error Unsupported WORD_BIT
#endif
}

#if defined(DEBUG)
void word_str_r(bitmap_word_t word, char *str, size_t len)
{
	if (len <= BITMAP_WORD_BIT) {
		str[0] = '\0';
		return;
	}

	memset(str, '0', len);
	str[BITMAP_WORD_BIT] = '\0';

	int indexes[BITMAP_WORD_BIT + 1];
	word_index(page->words[w], indexes, 0);
	for (size_t i = 0; i < BITMAP_WORD_BIT && indexes[i] != 0; i++) {
		str[indexes[i] - 1] = '1';
	}
}

const char *word_str(bitmap_word_t word)
{
	static char str[BITMAP_WORD_BIT + 1];
	word_str_r(word, str, BITMAP_WORD_BIT + 1);
	return str;
}
#endif /* defined(DEBUG) */

RB_GENERATE(bitmap_pages_tree, bitmap_page, node, bitmap_page_cmp);

bool test_bit(const void *data, size_t pos)
{
	const char *cdata = (const char *) data;

	const size_t cur_pos = pos / CHAR_BIT;
	const size_t cur_offset = pos % CHAR_BIT;

	return (cdata[cur_pos] >> cur_offset) & 0x1;
}

int find_next_set_bit(const void *data, size_t data_size, size_t *ppos) {
	const char *cdata = (const char *) data;

	size_t cur_pos = *ppos / CHAR_BIT;
	size_t cur_offset = *ppos % CHAR_BIT;

	while (cur_pos < data_size) {
		char c = cdata[cur_pos] >> cur_offset;
		if (c & 0x1) {
			*ppos = cur_pos * CHAR_BIT + cur_offset;
			return 0;
		}

		cur_offset++;
		if (cur_offset >= CHAR_BIT) {
			cur_offset = 0;
			cur_pos++;
			continue;
		}
	}

	*ppos = SIZE_MAX;
	return -1;
}

/* bitmap_set/bitmap_get helpers */
static void bitmap_set_in_page(struct bitmap_page *page, size_t pos, bool val);
static bool bitmap_get_from_page(struct bitmap_page *page, size_t pos);

int bitmap_new(struct bitmap **pbitmap)
{
	*pbitmap = calloc(1, sizeof(struct bitmap));
	if (*pbitmap == NULL) {
		return -1;
	}

	struct bitmap *bitmap = *pbitmap;
	RB_INIT(&(bitmap->pages));

	return 0;
}

void bitmap_free(struct bitmap **pbitmap)
{
	struct bitmap *bitmap = *pbitmap;
	if (bitmap != NULL) {
		/* cleanup pages */
		struct bitmap_page *page = NULL, *next = NULL;
		RB_FOREACH_SAFE(page, bitmap_pages_tree,
				&(bitmap->pages), next) {
			RB_REMOVE(bitmap_pages_tree, &(bitmap->pages), page);
			free(page);
		}

		free(bitmap);
	}

	*pbitmap = NULL;
}


bool bitmap_get(struct bitmap *bitmap, size_t pos)
{
	assert(pos < SIZE_MAX);

	struct bitmap_page key;
	key.first_pos = bitmap_page_first_pos(pos);

	struct bitmap_page *page = RB_FIND(bitmap_pages_tree,
					&(bitmap->pages), &key);

	if (page == NULL) {
		return false;
	}

	return bitmap_get_from_page(page, pos - page->first_pos);
}

static
bool bitmap_get_from_page(struct bitmap_page *page, size_t pos)
{
	size_t w = pos / BITMAP_WORD_BIT;
	int offset = pos % BITMAP_WORD_BIT;
	return word_test_bit(page->words[w], offset);
}

int bitmap_set(struct bitmap *bitmap, size_t pos, bool val)
{
	assert(pos < SIZE_MAX);

	struct bitmap_page key;
	key.first_pos = bitmap_page_first_pos(pos);

	struct bitmap_page *page = RB_FIND(bitmap_pages_tree,
					&(bitmap->pages), &key);

	if (page == NULL) {
		if (!val) {
			/* trying to unset bit, but page does not exist */
			return 0;
		}

		/* resize bitmap */

		/* align pages for SSE/AVX */
		page = memalign(BITMAP_WORD_ALIGNMENT,
				sizeof(struct bitmap_page));
		if (page == NULL) {
			return -1;
		}
		memset(page, 0, sizeof(*page));

		page->first_pos = key.first_pos;
		RB_INSERT(bitmap_pages_tree,
			  &(bitmap->pages), page);
	}

	if (bitmap_get_from_page(page, pos - page->first_pos) != val) {
		bitmap_set_in_page(page, pos - page->first_pos, val);

		/* update size counter of the bitmap */
		if (val) {
			bitmap->cardinality++;
		} else {
			bitmap->cardinality--;
		}
	}

	return 0;
}

static
void bitmap_set_in_page(struct bitmap_page *page, size_t pos, bool val)
{
	size_t w = pos / BITMAP_WORD_BIT;
	int offset = pos % BITMAP_WORD_BIT;
	if (val) {
		page->words[w] = word_set_bit(page->words[w], offset);
	} else {
		page->words[w] = word_clear_bit(page->words[w], offset);
	}
}

size_t bitmap_cardinality(struct bitmap *bitmap) {
	return bitmap->cardinality;
}

#if defined(DEBUG)
void bitmap_stat(struct bitmap *bitmap, struct bitmap_stat *stat) {
	memset(stat, 0, sizeof(*stat));

#if	defined(ENABLE_AVX)
	stat->have_avx = true;
#endif
#if	defined(ENABLE_SSE2)
	stat->have_sse2 = true;
#endif
#if	(defined(HAVE_CTZLL) && __SIZEOF_SIZE_T__ == 8) || \
	(defined(HAVE_CTZ) && __SIZEOF_SIZE_T__ == 4)
	stat->have_ctz = true;
#endif
#if	(defined(HAVE_POPCOUNTLL) && __SIZEOF_SIZE_T__ == 8) || \
	(defined(HAVE_POPCOUNT) && __SIZEOF_SIZE_T__ == 4)
	stat->have_popcnt = true;
#endif
	stat->word_bit = BITMAP_WORD_BIT;
	stat->page_bit = BITMAP_PAGE_BIT;
	stat->words_per_page = BITMAP_WORDS_PER_PAGE;
	stat->cardinality = bitmap->cardinality;

	stat->capacity = 0;
	stat->pages = 0;
	struct bitmap_page *page = NULL;
	RB_FOREACH(page, bitmap_pages_tree, &(bitmap->pages)) {
		stat->pages++;
	}
	stat->capacity = stat->pages * BITMAP_PAGE_BIT;

	stat->mem_pages = stat->pages * sizeof(struct bitmap_page);
	stat->mem_other = sizeof(struct bitmap);
}

void bitmap_dump(struct bitmap *bitmap, int verbose, FILE *stream) {
	struct bitmap_stat stat;
	bitmap_stat(bitmap, &stat);

	fprintf(stream, "Bitmap %p\n", bitmap);
	fprintf(stream, "{\n");
	fprintf(stream, "    " "features    = ");
	fprintf(stream, "%cAVX ", stat.have_avx ? '+' : '-');
	fprintf(stream, "%cSSE2 ", stat.have_sse2 ? '+' : '-');
	fprintf(stream, "%cCTZ ", stat.have_ctz ? '+' : '-');
	fprintf(stream, "%cPOPCNT ", stat.have_popcnt ? '+' : '-');
	fprintf(stream, "\n");
	fprintf(stream, "    " "word_bit    = %d\n", stat.word_bit);
	fprintf(stream, "    " "page_bit    = %d * %d = %d\n",
		stat.words_per_page,
		stat.word_bit,
		stat.page_bit);

	fprintf(stream, "    " "pages       = %zu\n", stat.pages);
	fprintf(stream, "    " "cardinality = %zu // saved\n",
		stat.cardinality);
	fprintf(stream, "    " "capacity    = %zu (%zu * %d)\n",
		stat.capacity,
		stat.pages,
		stat.page_bit);
	if (stat.capacity > 0) {
		fprintf(stream, "    "
			"utilization = %-8.4f%% (%zu / %zu)\n",
			(float) stat.cardinality * 100.0 / (stat.capacity),
			stat.cardinality,
			stat.capacity
			);
	} else {
		fprintf(stream, "    "
			"utilization = undefined\n");
	}
	fprintf(stream, "    " "mem_pages   = %zu bytes // with tree data\n",
		stat.mem_pages);
	fprintf(stream, "    " "mem_other   = %zu bytes\n", stat.mem_other);
	fprintf(stream, "    " "mem_total   = %zu bytes\n",
		stat.mem_other + stat.mem_pages);
	if (stat.cardinality > 0) {
		fprintf(stream, "    "
			"density     = %-8.4f bytes per value\n",
			(float) (stat.mem_other + stat.mem_pages) /
			stat.cardinality);
	} else {
		fprintf(stream, "    "
			"density     = undefined\n");
	}

	if (verbose > 0) {
		fprintf(stream, "    " "pages = {\n");

		size_t total_cardinality = 0;
		struct bitmap_page *page = NULL;
		RB_FOREACH(page, bitmap_pages_tree, &(bitmap->pages)) {
			size_t page_last_pos = page->first_pos +
					BITMAP_PAGE_BIT;

			fprintf(stream, "        " "[%zu, %zu) ",
				page->first_pos, page_last_pos);

			int indexes[BITMAP_PAGE_BIT + 1];
			int *iter = indexes;
			for (size_t w = 0; w < BITMAP_WORDS_PER_PAGE; w++) {
				iter = word_index(page->words[w], iter,
					page->first_pos + w * BITMAP_WORD_BIT);
			}

			size_t page_cardinality = (iter - indexes);
			total_cardinality += page_cardinality;

			fprintf(stream, "utilization = %8.4f%% (%zu/%d)",
				(float) page_cardinality*1e2 / BITMAP_PAGE_BIT,
				page_cardinality, BITMAP_PAGE_BIT);

			if (verbose <= 1) {
				fprintf(stream, "\n");
				continue;
			}
			fprintf(stream, " ");

			fprintf(stream, "vals = {");
			for (size_t i = 0; i < BITMAP_PAGE_BIT &&
			     indexes[i] != 0; i++) {
				fprintf(stream, "%zu, ",
					page->first_pos + indexes[i]);
			}
			fprintf(stream, "}\n");
		}

		fprintf(stream, "    " "}\n");
		fprintf(stream, "    " "cardinality = %zu // calculated\n",
			total_cardinality);
	}

	fprintf(stream, "}\n");
}

#endif /* defined(DEBUG) */
/* }}} */
