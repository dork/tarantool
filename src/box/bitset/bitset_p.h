#ifndef BITSET_BITSET_P_H_INCLUDED
#define BITSET_BITSET_P_H_INCLUDED

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

/**
 * @file
 * @brief Private header file, please don't use directly.
 * @internal
 * @author Roman Tsisyk
 */

#include <util.h>

#include <stdlib.h>
#include <limits.h>

#include <third_party/tree.h>

#include "bit.h"

#if defined(ENABLE_SSE2) || defined(ENABLE_AVX)
#include <immintrin.h>
#endif /* defined(ENABLE_SSE2) || defined(ENABLE_AVX) */

#if   defined(ENABLE_AVX)
typedef __m256i u256;
#endif

#if   defined(ENABLE_SSE2)
typedef __m128i u128;
#endif

#if   defined(ENABLE_AVX)
typedef u256 bitset_word_t;
#define __BITSET_WORD_BIT 256
#define BITSET_WORD_ALIGNMENT 32
#elif defined(ENABLE_SSE2)
typedef u128 bitset_word_t;
#define __BITSET_WORD_BIT 128
#define BITSET_WORD_ALIGNMENT 16
#else /* !defined(ENABLE_SSE2) && !defined(ENABLE_AVX) */
typedef size_t bitset_word_t; /* is always size_t if sse is disabled */
#define __BITSET_WORD_BIT (__SIZEOF_SIZE_T__ * CHAR_BIT)
#define BITSET_WORD_ALIGNMENT 8
#endif

/** Number of bits in one word */
#define BITSET_WORD_BIT __BITSET_WORD_BIT

/** Number of bits in one page */
#define BITSET_PAGE_BIT 1024 /* works good on our hardware */

/** Numbers of words in one page */
#define BITSET_WORDS_PER_PAGE (BITSET_PAGE_BIT / BITSET_WORD_BIT)

#if ((BITSET_PAGE_BIT % BITSET_WORD_BIT) != 0)
#error Invalid BITSET_PAGE_BIT or BITSET_WORD_BIT!
#endif

/*
 * Operations on words
 */

/* TODO(roman): implement word_xxx methods using SSE2 */

static inline bitset_word_t
word_set_zeros(void)
{
#if defined(ENABLE_AVX)
	return _mm256_setzero_si256();
#elif defined(ENABLE_SSE2)
	return _mm_setzero_si128();
#else /* !defined(ENABLE_SSE2) */
	return 0;
#endif
}

static inline bitset_word_t
word_set_ones(void)
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

static inline bool
word_test_zeros(const bitset_word_t word)
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

static inline bool
word_test_ones(const bitset_word_t word)
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

static inline bitset_word_t
word_not(const bitset_word_t word)
{
	return ~word;
}

static inline bitset_word_t
word_and(const bitset_word_t word1, const bitset_word_t word2)
{
	return (word1 & word2);
}

static inline bitset_word_t
word_nand(const bitset_word_t word1, const bitset_word_t word2)
{
	return ~(word1 & word2);
}

static inline bitset_word_t
word_or(const bitset_word_t word1, const bitset_word_t word2)
{
	return (word1 | word2);
}

static inline bitset_word_t
word_nor(bitset_word_t word1, bitset_word_t word2)
{
	return ~(word1 | word2);
}

static inline bitset_word_t
word_xor(const bitset_word_t word1, const bitset_word_t word2)
{
	return (word1 ^ word2);
}

static inline bitset_word_t
word_xnor(const bitset_word_t word1, const bitset_word_t word2)
{
	return ~(word1 ^ word2);
}

#if defined(ENABLE_SSE2) || defined(ENABLE_AVX)
/* don't inline this method if SSE2/AVX is enabled (too much code) */
bitset_word_t
word_bitmask(int offset);
#else /* !defined(ENABLE_SSE2) && !defined(ENABLE_AVX)) */
static inline
bitset_word_t word_bitmask(int offset)
{
	return (bitset_word_t) 1 << offset;
}
#endif

static inline bool
word_test_bit(const bitset_word_t word, int offset)
{
	const bitset_word_t mask = word_bitmask(offset);
	return !word_test_zeros(word & mask);
}

static inline bitset_word_t
word_set_bit(const bitset_word_t word, int offset)
{
	const bitset_word_t mask = word_bitmask(offset);
	return (word | mask);
}

static inline bitset_word_t
word_clear_bit(const bitset_word_t word, int offset)
{
	const bitset_word_t mask = word_bitmask(offset);
	return (word & (~mask));
}

/**
 * @brief Finds the index of the least significant 1-bit in this word
 * starting from start_pos
 * @param word
 * @return the index+1 of the least significant 1-bit in this word or
 * 0 if word is zero.
 */

int
word_find_set_bit(const bitset_word_t word, int start_pos);

int *
word_index(const bitset_word_t word, int *indexes, int offset);

#if defined(DEBUG)
const char *
word_str(bitset_word_t word);

void
word_str_r(bitset_word_t word, char *str, size_t len);
#endif /* !defined(DEBUG) */

/*
 * General-purpose bit-manipulation functions
 */
bool
test_bit(const void *data, size_t pos);

int
find_next_set_bit(const void *data, size_t data_size, size_t *ppos);

/**
 * Bitset definition
 */
struct bitset {
	RB_HEAD(bitset_pages_tree, bitset_page) pages;
	size_t cardinality;
};

struct bitset_page {
	bitset_word_t words[BITSET_WORDS_PER_PAGE]
		__attribute__ ((aligned(BITSET_WORD_ALIGNMENT)));
	RB_ENTRY(bitset_page) node;
	size_t first_pos;
} __attribute__ ((aligned(BITSET_WORD_ALIGNMENT)));

static inline size_t
bitset_page_first_pos(size_t pos) {
	return (pos - (pos % BITSET_PAGE_BIT));
}

#ifdef DEBUG
void
bitset_debug_print(struct bitset *bitset);
#endif /* def DEBUG */

/*
 * Red-Black Tree for pages
 */

static inline int
bitset_page_cmp(const struct bitset_page *a, const struct bitset_page *b)
{
	if (a->first_pos < b->first_pos) {
		return -1;
	} else if (a->first_pos == b->first_pos) {
		return 0;
	} else {
		return 1;
	}
}

RB_PROTOTYPE(bitset_pages_tree, bitset_page, node, bitset_page_cmp);

#endif /* BITSET_BITSET_P_H_INCLUDED */
