#ifndef BITMAP_BITMAP_P_H_INCLUDED
#define BITMAP_BITMAP_P_H_INCLUDED

/**
 * @file Private header file, please don't use directly.
 */

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

/* TODO(roman): update CMakeLists.txt */
#define ENABLE_SSE2 1
#define HAVE_FFSDI2 1
#define HAVE_FFSTI2 1

#include "third_party/tree.h"

#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <assert.h>

#if defined(ENABLE_SSE2)
#include <emmintrin.h>
#endif /* defined(ENABLE_SSE2) */

#if defined(ENABLE_SSE2)
typedef __m128i bitmap_word_t;
#define __BITMAP_WORD_BIT 128
#else /* !defined(ENABLE_SSE2) */
typedef size_t bitmap_word_t;
#define __BITMAP_WORD_BIT (__SIZEOF_SIZE_T__ * CHAR_BIT)
#endif

/** Numbers of words in one page */
#define BITMAP_WORDS_PER_PAGE 16 /* works good on my i5 */

/** Number of bits in one word */
#define BITMAP_WORD_BIT __BITMAP_WORD_BIT

/** Number of bits in one page */
#define BITMAP_PAGE_BIT (BITMAP_WORDS_PER_PAGE * BITMAP_WORD_BIT)

/*
 * Operations on words
 */

/* TODO(roman): implement word_xxx methods using SSE2 */

static inline
bitmap_word_t word_set_zeros()
{
#if defined(ENABLE_SSE2)
	return _mm_setzero_si128();
#else /* !defined(ENABLE_SSE2) */
	return 0;
#endif
}

static inline
bitmap_word_t word_set_ones()
{
#if defined(ENABLE_SSE2)
	/* like _mm_setzero_si128, but with -1 */
	return __extension__(__m128i)(__v4si){-1,-1,-1,-1};
#else /* !defined(ENABLE_SSE2) */
	return (bitmap_word_t) -1;
#endif
}

static inline
bool word_test_zeros(const bitmap_word_t word)
{
#if defined(ENABLE_SSE2)
	return (_mm_movemask_epi8(_mm_cmpeq_epi8(word, word_set_zeros()))
			== 0xFFFF);
#else /* !defined(ENABLE_SSE2) */
	return (word == 0);
#endif
}

static inline
bool word_test_ones(const bitmap_word_t word)
{
#if defined(ENABLE_SSE2)
	return (_mm_movemask_epi8(_mm_cmpeq_epi8(word, word_set_ones()))
			== 0xFFFF);
#else /* !defined(ENABLE_SSE2) */
	return (word == -1);
#endif
}

static inline
bitmap_word_t word_not(const bitmap_word_t word)
{
	return ~word;
}

static inline
bitmap_word_t word_and(const bitmap_word_t word1, const bitmap_word_t word2)
{
	return (word1 & word2);
}

static inline
bitmap_word_t word_nand(const bitmap_word_t word1, const bitmap_word_t word2)
{
	return ~(word1 & word2);
}

static inline
bitmap_word_t word_or(const bitmap_word_t word1, const bitmap_word_t word2)
{
	return (word1 | word2);
}

static inline
bitmap_word_t word_nor(bitmap_word_t word1, bitmap_word_t word2)
{
	return ~(word1 | word2);
}

static inline
bitmap_word_t word_xor(const bitmap_word_t word1, const bitmap_word_t word2)
{
	return (word1 ^ word2);
}

static inline
bitmap_word_t word_xnor(const bitmap_word_t word1, const bitmap_word_t word2)
{
	return ~(word1 ^ word2);
}

static inline
bitmap_word_t word_bitmask(int offset)
{
#if defined(ENABLE_SSE2)
	assert(offset >= 0 && offset < 128);
	/* SSE2 does not have BIT shift instruction, so
	 * we prepareed bitmask by filling 4 x 32 or 2 x 64 words
	 */
#if __WORDSIZE == 32
	if (offset < 32) {
		return _mm_set_epi32(0, 0, 0, (uint32_t) 1 << (offset & 0x1f));
	} else if (offset < 64) {
		return _mm_set_epi32(0, 0, (uint32_t) 1 << (offset & 0x1f), 0);
	} else if (offset < 96) {
		return _mm_set_epi32(0, (uint32_t) 1 << (offset & 0x1f), 0, 0);
	} else /* offset < 128 */ {
		return _mm_set_epi32((uint32_t) 1 << (offset & 0x1f), 0, 0, 0);
	}
#elif __WORDSIZE == 64
	if (offset < 64) {
		return _mm_set_epi64x(0, (uint64_t) 1 << (offset & 0x3f));
	} else /* offset < 128 */ {
		return _mm_set_epi64x((uint64_t) 1 << (offset & 0x3f), 0);
	}
#else
#error SSE2 with given WORDSIZE is not supported
#endif /* WORDSIZE */
#else /* !defined(ENABLE_SSE2) */
	return (bitmap_word_t) 1 << offset;
#endif
}

static inline
bool word_test_bit(const bitmap_word_t word, int offset)
{
	const bitmap_word_t mask = word_bitmask(offset);
	return !word_test_zeros(word & mask);
}

static inline
bitmap_word_t word_set_bit(const bitmap_word_t word, int offset)
{
	const bitmap_word_t mask = word_bitmask(offset);
	return (word | mask);
}

static inline
bitmap_word_t word_clear_bit(const bitmap_word_t word, int offset)
{
	const bitmap_word_t mask = word_bitmask(offset);
	return (word & (~mask));
}

#if defined(HAVE_FFSDI2)
	int __ffsdi2 (long);
#endif
#if defined(HAVE_FFSTI2)
	int __ffsti2 (long long);
#endif
/**
 * @brief Finds the index of the least significant 1-bit in this word
 * starting from start_pos
 * @param word
 * @return the index+1 of the least significant 1-bit in this word or
 * 0 if word is zero.
 */
static inline
int word_find_set_bit(const bitmap_word_t word, int start_pos)
{
	assert(start_pos < BITMAP_WORD_BIT);

#if   !defined(ENABLE_SSE2) && ( \
	 ((BITMAP_WORD_BIT == 32) && HAVE_FFSDI2) || \
	 ((BITMAP_WORD_BIT == 64) && HAVE_FFSTI2))

	/* HACK(roman): according to documentation, __ffsdi2 / __ffsti2
	 * must return 0 if argument is zero, but they does not.
	 */
	const bitmap_word_t word1 = word >> start_pos;
	if (word_test_zeros(word1)) {
		return 0;
	}
#if   BITMAP_WORD_BIT == 32
	int pos = __extension__(__ffsdi2 (word1));
#elif BITMAP_WORD_BIT == 64
	int pos = __extension__(__ffsti2 (word1));
#endif
	if (pos == 0) {
		return pos;
	} else {
		return start_pos + pos;
	}
#else
	/* TODO(roman): implement an optimized version for SSE2 */
	for (int pos = start_pos; pos < BITMAP_WORD_BIT; pos++) {
		if (word_test_bit(word, pos)) {
			return pos + 1;
		}
	}

	return 0;
#endif
}

/*
 * General-purpose bit-manipulation functions
 */
bool test_bit(const void *data, size_t pos);
int find_next_set_bit(const void *data, size_t data_size, size_t *ppos);
/**
 * Bitmap definition
 */
struct bitmap {
	RB_HEAD(bitmap_pages_tree, bitmap_page) pages;
	size_t cardinality;
};

struct bitmap_page {
	RB_ENTRY(bitmap_page) node;
	size_t first_pos;
	bitmap_word_t words[BITMAP_WORDS_PER_PAGE];
};

static inline
size_t bitmap_page_first_pos(size_t pos) {
	return (pos - (pos % BITMAP_PAGE_BIT));
}

#ifdef DEBUG
void bitmap_debug_print(struct bitmap *bitmap);
#endif /* def DEBUG */

/*
 * Red-Black Tree for pages
 */

static inline int
bitmap_page_cmp(const struct bitmap_page *a, const struct bitmap_page *b)
{
	if (a->first_pos < b->first_pos) {
		return -1;
	} else if (a->first_pos == b->first_pos) {
		return 0;
	} else {
		return 1;
	}
}

RB_PROTOTYPE(bitmap_pages_tree, bitmap_page, node, bitmap_page_cmp);

#endif /* BITMAP_BITMAP_P_H_INCLUDED */
