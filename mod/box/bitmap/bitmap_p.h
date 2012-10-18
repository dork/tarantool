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

#include "third_party/tree.h"

typedef size_t bitmap_word_t;

/** Numbers of words in one page */
#define BITMAP_WORDS_PER_PAGE 64

/** Number of bits in one word */
static const size_t BITMAP_WORD_BIT = (sizeof(bitmap_word_t) * CHAR_BIT);

/** Number of bits in one page */
static const size_t BITMAP_PAGE_BIT = (BITMAP_WORDS_PER_PAGE *
					(sizeof(bitmap_word_t) * CHAR_BIT));

/*
 * Operations on words
 */

/* TODO(roman): implement word_xxx methods using SSE2 */

static inline
bitmap_word_t word_zeros() {
	return 0;
}

static inline
bitmap_word_t word_ones() {
	return (bitmap_word_t) -1;
}

static inline
bitmap_word_t word_not(bitmap_word_t word) {
	return ~word;
}

static inline
bitmap_word_t word_and(bitmap_word_t word1, bitmap_word_t word2) {
	return (word1 & word2);
}

static inline
bitmap_word_t word_nand(bitmap_word_t word1, bitmap_word_t word2) {
	return ~(word1 & word2);
}

static inline
bitmap_word_t word_or(bitmap_word_t word1, bitmap_word_t word2) {
	return (word1 | word2);
}

static inline
bitmap_word_t word_nor(bitmap_word_t word1, bitmap_word_t word2) {
	return ~(word1 | word2);
}

static inline
bitmap_word_t word_xor(bitmap_word_t word1, bitmap_word_t word2) {
	return (word1 ^ word2);
}

static inline
bitmap_word_t word_xnor(bitmap_word_t word1, bitmap_word_t word2) {
	return ~(word1 ^ word2);
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
