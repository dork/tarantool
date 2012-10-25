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

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <malloc.h>
#include <assert.h>
#include <errno.h>

#include "bitmap_p.h"

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

int word_find_set_bit(const bitmap_word_t word, int start_pos)
{
#if !defined(ENABLE_SSE2) && (__SIZEOF_SIZE_T__ == 8) && HAVE_CTZLL
	if (start_pos < 64 && !word_test_zeros(word >> start_pos)) {
		return 1 + start_pos + __builtin_ctzll(word >> start_pos);
	}

	return 0;
#elif   !defined(ENABLE_SSE2) && (__SIZEOF_SIZE_T__ == 4) && HAVE_CTZ
	if (start_pos < 32 && !word_test_zeros(word >> start_pos)) {
		return 1 + start_pos + __builtin_ctz(word >> start_pos);
	}

	return 0;
#elif  defined(ENABLE_AVX) && (__SIZEOF_SIZE_T__ == 8) && HAVE_CTZLL
	union __cast {
		__m256i m256;
		uint64_t ui64[4];
	} w;
	w.m256 = word;

	if (start_pos < 64 && (w.ui64[0] >> start_pos)) {
		return 1 + start_pos +
				__builtin_ctzll(w.ui64[0] >> (start_pos));
	}

	if(start_pos < 64) {
		start_pos = 64;
	}

	if (start_pos < 128 && (w.ui64[1] >> (start_pos % 64))) {
		return 1 + 64 + (start_pos % 64) +
				__builtin_ctzll(w.ui64[1] >> (start_pos % 64));
	}

	if(start_pos < 128) {
		start_pos = 128;
	}

	if (start_pos < 192 && (w.ui64[2] >> (start_pos % 64))) {
		return 1 + 128 + (start_pos % 64) +
				__builtin_ctzll(w.ui64[2] >> (start_pos % 64));
	}

	if(start_pos < 192) {
		start_pos = 192;
	}

	if (start_pos < 256 && (w.ui64[3] >> (start_pos % 64))) {
		return 1 + 192 + (start_pos % 64) +
				__builtin_ctzll(w.ui64[3] >> (start_pos % 64));
	}

	return 0;
#elif  defined(ENABLE_SSE2) && (__SIZEOF_SIZE_T__ == 8) && HAVE_CTZLL
	/*
	if (word_test_zeros(word)) {
		return 0;
	}
	*/

	union __cast {
		__m128i m128;
		uint64_t ui64[2];
	} w;
	w.m128 = word;

	if (start_pos < 64 && (w.ui64[0] >> start_pos)) {
		return 1 + start_pos +
				__builtin_ctzll(w.ui64[0] >> (start_pos));
	}

	if(start_pos < 64) {
		start_pos = 64;
	}

	if (start_pos < 128 && (w.ui64[1] >> (start_pos % 64))) {
		return 1 + 64 + (start_pos % 64) +
				__builtin_ctzll(w.ui64[1] >> (start_pos % 64));
	}

	return 0;
#elif  defined(ENABLE_SSE2) && (__SIZEOF_SIZE_T__ == 4) && HAVE_CTZ
	if (word_test_zeros(word)) {
		return 0;
	}

	union __cast {
		__m128i m128;
		uint32_t ui32[4];
	} w;
	w.m128 = word;

	if (start_pos < 32 && (w.ui32[0] >> start_pos)) {
		return 1 + start_pos +
				__builtin_ctz(w.ui32[0] >> (start_pos));
	}

	if (start_pos < 32) {
		start_pos = 32;
	}

	if (start_pos < 64 && (w.ui32[1] >> (start_pos % 32))) {
		return 1 + 32 + (start_pos % 32) +
				__builtin_ctz(w.ui32[1] >> (start_pos % 32));
	}

	if (start_pos < 64) {
		start_pos = 64;
	}

	if (start_pos < 96 && (w.ui32[2] >> (start_pos % 32))) {
		return 1 + 64 + (start_pos % 32) +
				__builtin_ctz(w.ui32[2] >> (start_pos % 32));
	}

	if (start_pos < 96) {
		start_pos = 96;
	}

	if (start_pos < 128 && (w.ui32[3] >> (start_pos % 32))) {
		return 1 + 96 + (start_pos % 32) +
				__builtin_ctz(w.ui32[3] >> (start_pos % 32));
	}

	return 0;
#else
	for (int pos = start_pos; pos < BITMAP_WORD_BIT; pos++) {
		if (word_test_bit(word, pos)) {
			return pos + 1;
		}
	}

	return 0;
#endif
}

static
int *native_index_bits(size_t nword, int *indexes, int offset) {
#if  (__SIZEOF_SIZE_T__ == 8 && defined(HAVE_CTZLL)) || \
     (__SIZEOF_SIZE_T__ == 4 && defined(HAVE_CTZ))

	int prev_pos = 0;
	int i = 0;

#if   (__SIZEOF_SIZE_T__ == 8 && defined(HAVE_POPCNTLL))
	/* fast implementation using hardware popcount instruction (64 bit) */
	const int count = __builtin_popcountll(nword);
	while (i < count) {
#elif (__SIZEOF_SIZE_T__ == 4 && defined(HAVE_POPCNT))
	/* fast implementation using hardware popcount instruction (32 bit) */
	const int count = __builtin_popcount(nword);
	while (i < count) {
#else
	/* use sligtly slower implementation without popcnt */
	while(nword) {
#endif
#if __SIZEOF_SIZE_T__ == 8
		/* use hardware bsf (64 bit) */
		const int a = __builtin_ctzll(nword);
#else
		/* use hardware bsf (32 bit) */
		const int a = __builtin_ctz(nword);
#endif
		prev_pos += a + 1;
		nword >>= a;
		nword >>= 1;
		indexes[i++] = offset + prev_pos;
	}

	indexes[i] = 0;
	return indexes + i;
#else /* !defined(HAVE_CTZ) */
	/* naive generic implementation, worst case */
	size_t bit = 1;
	int i = 0;
	for (int k = 0; k < (__SIZEOF_SIZE_T__ * CHAR_BIT); k++) {
		if (nword & bit) {
			indexes[i++] = offset + k + 1;
		}
		bit <<= 1;
	}

	indexes[i] = 0;
	return indexes + i;
#endif
}

int *word_index_bits(const bitmap_word_t word, int *indexes, int offset)
{
#if   defined(ENABLE_AVX)  && __SIZEOF_SIZE_T__ == 8
	union __cast {
		__m256i m256;
		size_t ui64[4];
	} w;
	w.m256 = word;
	indexes = native_index_bits(w.ui64[0], indexes, offset + 0);
	indexes = native_index_bits(w.ui64[1], indexes, offset + 64);
	indexes = native_index_bits(w.ui64[2], indexes, offset + 128);
	indexes = native_index_bits(w.ui64[3], indexes, offset + 192);
	return indexes;
#elif defined(ENABLE_AVX)  && __SIZEOF_SIZE_T__ == 4
	union __cast {
		__m256i m256;
		size_t ui32[8];
	} w;
	w.m256 = word;
	indexes = native_index_bits(w.ui32[0], indexes, offset + 0);
	indexes = native_index_bits(w.ui32[1], indexes, offset + 32);
	indexes = native_index_bits(w.ui32[2], indexes, offset + 64);
	indexes = native_index_bits(w.ui32[3], indexes, offset + 96);
	indexes = native_index_bits(w.ui32[4], indexes, offset + 128);
	indexes = native_index_bits(w.ui32[5], indexes, offset + 160);
	indexes = native_index_bits(w.ui32[6], indexes, offset + 192);
	indexes = native_index_bits(w.ui32[7], indexes, offset + 224);
	return indexes;
#elif defined(ENABLE_SSE2) && __SIZEOF_SIZE_T__ == 8
	union __cast {
		__m128i m128;
		size_t ui64[2];
	} w;
	w.m128 = word;
	indexes = native_index_bits(w.ui64[0], indexes, offset + 0);
	indexes = native_index_bits(w.ui64[1], indexes, offset + 64);
	return indexes;
#elif defined(ENABLE_SSE2) && __SIZEOF_SIZE_T__ == 4
	union __cast {
		__m128i m128;
		size_t ui32[4];
	} w;
	w.m128 = word;
	indexes = native_index_bits(w.ui32[0], indexes, offset + 0);
	indexes = native_index_bits(w.ui32[1], indexes, offset + 32);
	indexes = native_index_bits(w.ui32[2], indexes, offset + 64);
	indexes = native_index_bits(w.ui32[3], indexes, offset + 96);
	return indexes;
#else /* !defined(HAVE_SSE2) */
	/* bitmap_word_t is size_t */
	return native_index_bits(word, indexes, offset + 0);
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

	int pos = 0;
	while ((pos = word_find_set_bit(word, pos)) != 0) {
		str[pos-1] = '1';
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
#if	(defined(HAVE_POPCNTLL) && __SIZEOF_SIZE_T__ == 8) || \
	(defined(HAVE_POPCNT) && __SIZEOF_SIZE_T__ == 4)
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
	fprintf(stream, "    " "capacity    = %zu (%zu * %d)\n",
		stat.capacity,
		stat.pages,
		stat.page_bit);
	fprintf(stream, "    " "cardinality = %zu // saved\n", stat.cardinality);
	fprintf(stream, "    " "density     = %-.4f%% (%zu / %zu)\n",
		(float) stat.cardinality * 100.0 / (stat.capacity),
		stat.cardinality,
		stat.capacity
		);
	fprintf(stream, "    " "mem_pages   = %zu bytes // with tree data\n", stat.mem_pages);
	fprintf(stream, "    " "mem_other   = %zu bytes\n", stat.mem_other);
	fprintf(stream, "    " "mem_total   = %zu bytes\n",
		stat.mem_other + stat.mem_pages);
	fprintf(stream, "    " "utilization = %.4f bytes per value bit\n",
		(float) (stat.mem_other + stat.mem_pages) / stat.cardinality
		);

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
				iter = word_index_bits(page->words[w], iter,
					page->first_pos + w * BITMAP_WORD_BIT);
			}

			size_t page_cardinality = (iter - indexes);
			total_cardinality += page_cardinality;

			fprintf(stream, "util = %zu/%d",
				page_cardinality, BITMAP_PAGE_BIT);

			if (verbose <= 1) {
				fprintf(stream, "\n");
				continue;
			}
			fprintf(stream, " ");

			fprintf(stream, "vals = {");
			for (size_t i = 0; i < BITMAP_PAGE_BIT &&
			     indexes[i] != 0; i++) {
				printf("%zu, ", page->first_pos + indexes[i]);
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
