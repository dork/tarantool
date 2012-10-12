#ifndef BITMAP_BITMAP_P_H_INCLUDED
#define BITMAP_BITMAP_P_H_INCLUDED

/**
 * @file Private header file, please don't use directly.
 */

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

typedef size_t bitmap_word_t;
/* defines how many words will be stored in one page */
#define BITMAP_WORDS_PER_PAGE 1

extern const size_t BITMAP_WORD_BIT;

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

/* TODO(roman): split slist from this file */

/**
 * Single-Linked List definition
 */
struct slist_node {
	struct slist_node *next;
};
/* gets pointer to outer structure */
#define slist_member_of(ptr, type, member) ( \
	(type *)( (char *) ptr - offsetof(type,member) ))

/**
 * Bitmap definition
 */
struct bitmap {
	struct slist_node pages;
	size_t cardinality;
};

struct bitmap_page {
	struct slist_node node;
	size_t first_pos;
	bitmap_word_t words[BITMAP_WORDS_PER_PAGE];
};

#endif /* BITMAP_BITMAP_P_H_INCLUDED */
