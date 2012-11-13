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

#include <util.h>

#include <stdlib.h>
#include <limits.h>

#include <third_party/tree.h>

#include "bit.h"

#if defined(ENABLE_SSE2) || defined(ENABLE_AVX)
#include <immintrin.h>
#endif /* defined(ENABLE_SSE2) || defined(ENABLE_AVX) */

#if   defined(ENABLE_AVX) && !defined(ENABLE_SSE2)
#error ENABLE_SSE2 must be also defined since ENABLE_AVX is defined
#endif

#if   defined(ENABLE_AVX)
typedef __m256i u256;
#endif

#if   defined(ENABLE_SSE2)
typedef __m128i u128;
#endif

#if   defined(ENABLE_AVX)
typedef u256 bitmap_word_t;
#define __BITMAP_WORD_BIT 256
#define BITMAP_WORD_ALIGNMENT 32
#elif defined(ENABLE_SSE2)
typedef u128 bitmap_word_t;
#define __BITMAP_WORD_BIT 128
#define BITMAP_WORD_ALIGNMENT 16
#else /* !defined(ENABLE_SSE2) && !defined(ENABLE_AVX) */
typedef size_t bitmap_word_t; /* is always size_t if sse is disabled */
#define __BITMAP_WORD_BIT (__SIZEOF_SIZE_T__ * CHAR_BIT)
#define BITMAP_WORD_ALIGNMENT 8
#endif

/** Number of bits in one word */
#define BITMAP_WORD_BIT __BITMAP_WORD_BIT

/** Number of bits in one page */
#define BITMAP_PAGE_BIT 1024 /* works good on our hardware */

/** Numbers of words in one page */
#define BITMAP_WORDS_PER_PAGE (BITMAP_PAGE_BIT / BITMAP_WORD_BIT)

#if ((BITMAP_PAGE_BIT % BITMAP_WORD_BIT) != 0)
#error Invalid BITMAP_PAGE_BIT or BITMAP_WORD_BIT!
#endif

/*
 * Operations on words
 */

/* TODO(roman): implement word_xxx methods using SSE2 */

static inline
bitmap_word_t word_set_zeros()
{
#if defined(ENABLE_AVX)
	return _mm256_setzero_si256();
#elif defined(ENABLE_SSE2)
	return _mm_setzero_si128();
#else /* !defined(ENABLE_SSE2) */
	return 0;
#endif
}

static inline
bitmap_word_t word_set_ones()
{
#if defined(ENABLE_AVX)
	return _mm256_setr_epi32(-1, -1, -1, -1, -1, -1, -1, -1);
#elif defined(ENABLE_SSE2)
	/* like _mm_setzero_si128, but with -1 */
	return _mm_set_epi32(-1, -1, -1, -1);
#else /* !defined(ENABLE_SSE2) && !defined(ENABLE_AVX) */
	return SIZE_MAX;
#endif
}

static inline
bool word_test_zeros(const bitmap_word_t word)
{
#if   defined(ENABLE_AVX)
	return _mm256_testz_si256(word, word) != 0;
#elif defined(ENABLE_SSE2)
	return (_mm_movemask_epi8(_mm_cmpeq_epi8(word, word_set_zeros()))
			== 0xFFFF);
#else /* !defined(ENABLE_SSE2) */
	return (word == 0);
#endif
}

static inline
bool word_test_ones(const bitmap_word_t word)
{
#if   defined(ENABLE_AVX)
	return _mm256_testc_si256(word, word) != 0;
#elif defined(ENABLE_SSE2)
	return (_mm_movemask_epi8(_mm_cmpeq_epi8(word, word_set_ones()))
			== 0xFFFF);
#else /* !defined(ENABLE_SSE2) */
	return (word == SIZE_MAX);
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

#if defined(ENABLE_SSE2) || defined(ENABLE_AVX)
/* don't inline this method if SSE2/AVX is enabled (too much code) */
bitmap_word_t word_bitmask(int offset);
#else /* !defined(ENABLE_SSE2) && !defined(ENABLE_AVX)) */
static inline bitmap_word_t word_bitmask(int offset)
{
	return (bitmap_word_t) 1 << offset;
}
#endif

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

/**
 * @brief Finds the index of the least significant 1-bit in this word
 * starting from start_pos
 * @param word
 * @return the index+1 of the least significant 1-bit in this word or
 * 0 if word is zero.
 */

int word_find_set_bit(const bitmap_word_t word, int start_pos);
int *word_index(const bitmap_word_t word, int *indexes, int offset);

#if defined(DEBUG)
const char *word_str(bitmap_word_t word);
void word_str_r(bitmap_word_t word, char *str, size_t len);
#endif /* !defined(DEBUG) */

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
	bitmap_word_t words[BITMAP_WORDS_PER_PAGE]
		__attribute__ ((aligned(BITMAP_WORD_ALIGNMENT)));
	RB_ENTRY(bitmap_page) node;
	size_t first_pos;
} __attribute__ ((aligned(BITMAP_WORD_ALIGNMENT)));

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
