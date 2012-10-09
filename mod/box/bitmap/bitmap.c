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

#include "bitmap.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>

#include "say.h"

typedef size_t bitmap_word_t;
/* defines how many words will be stored in one page */
#define BITMAP_WORDS_PER_PAGE 48

/** Number of bits in one word */
static const size_t BITMAP_WORD_BIT = sizeof(bitmap_word_t) * CHAR_BIT;

/** Maximum number of elements in bitmap */
static const size_t BITMAP_WORD_MIN = (bitmap_word_t) 0;

/** Maximum number of elements in bitmap */
static const size_t BITMAP_WORD_MAX = (bitmap_word_t) SIZE_MAX;


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

/* bitmap_set/bitmap_get helpers */
static void bitmap_set_in_page(struct bitmap_page *page, size_t pos, bool val);
static bool bitmap_get_from_page(struct bitmap_page *page, size_t pos);

int bitmap_new(struct bitmap **pbitmap)
{
	*pbitmap = calloc(1, sizeof(struct bitmap));
	if (*pbitmap == NULL) {
		return -1;
	}

	return 0;
}

void bitmap_free(struct bitmap **pbitmap)
{
	struct bitmap *bitmap = *pbitmap;
	if (bitmap != NULL) {
		/* cleanup pages */
		struct slist_node *node = bitmap->pages.next;
		while (node != NULL) {
			struct slist_node *next = node->next;
			struct bitmap_page *page = slist_member_of(node,
						struct bitmap_page, node);
			free(page);
			node = next;
		}
		free(bitmap);
	}
	*pbitmap = NULL;
}


bool bitmap_get(struct bitmap *bitmap, size_t pos)
{
	struct slist_node *node = &(bitmap->pages);
	for (; node->next != NULL; node = node->next) {
		struct bitmap_page *page = slist_member_of(node->next,
						struct bitmap_page, node);

		size_t page_first_pos = page->first_pos;
		size_t page_last_pos = page->first_pos +
				BITMAP_WORDS_PER_PAGE * BITMAP_WORD_BIT;

		/*
		printf("Iter pos=%zu: [%zu, %zu)\n",
		       pos, page_first_pos, page_last_pos);
		*/

		if (pos < page_first_pos) {
			break;
		}

		if (pos >= page_last_pos) {
			continue;
		}

		return bitmap_get_from_page(page, pos - page->first_pos);
	}

	return false;
}

static
bool bitmap_get_from_page(struct bitmap_page *page, size_t pos)
{
	size_t w = pos / BITMAP_WORD_BIT;
	size_t offset = pos % BITMAP_WORD_BIT;
	return (page->words[w] & ((bitmap_word_t) 1 << offset)) != 0;
}

static
int bitmap_add_page(struct slist_node *node, size_t first_pos)
{
	/* resize bitmap */
	struct bitmap_page *page = calloc(1, sizeof(struct bitmap_page));
	if (page == NULL) {
		return -1;
	}

	page->first_pos = first_pos;

	page->node.next = node->next;
	node->next = &(page->node);

	return 0;
}

int bitmap_set(struct bitmap *bitmap, size_t pos, bool val)
{
	struct slist_node *node = &(bitmap->pages);
	size_t page_first_pos = 0;
	size_t page_last_pos = 0;

	for(; node->next != NULL; node = node->next) {
		struct bitmap_page *page = slist_member_of(node->next,
						struct bitmap_page, node);

		page_first_pos = page->first_pos;
		page_last_pos = page_first_pos +
				BITMAP_WORDS_PER_PAGE * BITMAP_WORD_BIT;

		if (pos < page_first_pos) {
			break;
		}

		if (pos < page_last_pos) {
			break;
		}
	}

	if (node->next == NULL || pos < page_first_pos) {
		size_t first_pos = pos -
				(pos % (BITMAP_WORDS_PER_PAGE * BITMAP_WORD_BIT));
		if (bitmap_add_page(node, first_pos) < 0) {
			return -1;
		}
	}

	struct bitmap_page *page = slist_member_of(node->next,
					struct bitmap_page, node);
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
	size_t offset = pos % BITMAP_WORD_BIT;
	if (val) {
		page->words[w] |= ((bitmap_word_t) 1 << offset);
	} else {
		page->words[w] ^= ((bitmap_word_t) 1 << offset);
	}
}

size_t bitmap_cardinality(struct bitmap *bitmap) {
	return bitmap->cardinality;
}

/* {{{ BitmapIterator *********************************************************/

/**
 * BitmapIterator definition
 */

struct bitmap_iterator {
	struct bitmap **bitmaps;
	size_t bitmaps_size;
	int *bitmaps_ops;
	int result_ops;

	struct slist_node **pages;
	size_t *offsets;

	bitmap_word_t cur_word;
	size_t cur_pos;
};

static
int bitmap_iterator_next_word(struct bitmap_iterator *it);
static
void iter_next(struct slist_node **pnode, size_t *poffset,
	int bitmap_ops, bitmap_word_t *word);

int bitmap_iterator_newn(struct bitmap_iterator **pit,
			 struct bitmap **bitmaps, size_t bitmaps_size,
			 int *bitmaps_ops,
			 int result_ops)
{
	int rc = -1;
	struct bitmap_iterator *it = calloc(1, sizeof(struct bitmap_iterator));
	if (it == NULL) {
		goto error_0;
	}

	it->bitmaps_size = bitmaps_size;
	it->bitmaps = malloc(sizeof(*it->bitmaps) * bitmaps_size);
	if (it->bitmaps == NULL) {
		goto error_1;
	}
	memcpy(it->bitmaps, bitmaps,
		sizeof(*it->bitmaps) * bitmaps_size);

	it->bitmaps_ops = malloc(sizeof(it->bitmaps_ops) * bitmaps_size);
	if (it->bitmaps == NULL) {
		goto error_2;
	}
	memcpy(it->bitmaps_ops, bitmaps_ops,
		sizeof(it->bitmaps_ops) * bitmaps_size);

	it->result_ops = result_ops;

	it->pages = malloc(sizeof(*it->pages) * bitmaps_size);
	if (it->pages == NULL) {
		goto error_3;
	}

	it->offsets = malloc(sizeof(*it->offsets) * bitmaps_size);
	if (it->offsets == NULL) {
		goto error_4;
	}

	for (size_t i = 0; i < bitmaps_size; i++) {
		it->pages[i] = &(bitmaps[i]->pages);
		it->offsets[i] = 0;
	}

	it->cur_pos = 0;
	bitmap_iterator_next_word(it);

	*pit = it;

	return 0;

/* error handling */
	free(it->offsets);
error_4:
	free(it->pages);
error_3:
	free(it->bitmaps_ops);
error_2:
	free(it->bitmaps);
error_1:
	free(it);
error_0:
	*pit = NULL;
	return rc;
}

void bitmap_iterator_free(struct bitmap_iterator **pit)
{
	struct bitmap_iterator *it = *pit;

	if (it != NULL) {
		free(it->bitmaps);
		free(it->bitmaps_ops);
		free(it->pages);
		free(it->offsets);

		free(it);
	}

	*pit = NULL;
}

/* TODO(roman): check similar method in bitmap_index */
static
size_t word_next_bit(bitmap_word_t *pword)
{
	bitmap_word_t bit = 1;

	for (size_t i = 0; i < BITMAP_WORD_BIT; i++) {
		if ((*pword & bit) != 0) {
			*pword ^= bit;
			return i;
		}

		bit <<= 1;
	}

	return SIZE_MAX;
}

size_t bitmap_iterator_next(struct bitmap_iterator *it)
{
	/*
	printf("==================\n");
	printf("Next: pos=%zu word=%zu\n", it->cur_pos, it->cur_word);
	*/

	size_t result = SIZE_MAX;
	int next_word_ret = 0;

	while (next_word_ret == 0) {
		result = word_next_bit(&(it->cur_word));

		if (result != SIZE_MAX) {
			result += it->cur_pos;
			break;
		}

		it->cur_pos += BITMAP_WORD_BIT;

		next_word_ret = bitmap_iterator_next_word(it);
	}


	return result;
}

static
int bitmap_iterator_next_word(struct bitmap_iterator *it)
{
	/* printf("NextWord: cur_pos1=%zu\n", it->cur_pos); */

	size_t offset_max = 0;

	for(size_t i = 0; i < it->bitmaps_size; i++) {
		if (offset_max < it->offsets[i]) {
			offset_max = it->offsets[i];
		}
	}

	if (offset_max < it->cur_pos) {
		offset_max = it->cur_pos;
	}

	/* printf("NextWord: offset_max=%zu\n", offset_max); */
	if (offset_max == SIZE_MAX || it->bitmaps_size == 0) {
		if (it->result_ops & BITMAP_OP_NOT) {
			/* just return next bit */
			it->cur_word = BITMAP_WORD_MAX;
			return 0;
		} else {
			/* no more elements */
			it->cur_pos = SIZE_MAX;
			return -1;
		}
	}
	it->cur_pos = offset_max - (offset_max % BITMAP_WORD_BIT);
	/* printf("NextWord: cur_pos2=%zu\n", it->cur_pos); */

	it->cur_word = BITMAP_WORD_MAX;
	for(size_t i = 0; i < it->bitmaps_size; i++) {
		bitmap_word_t word = 0;
		if (it->offsets[i] < it->cur_pos) {
			it->offsets[i] = offset_max;
		}

		iter_next(&it->pages[i], &it->offsets[i],
			  it->bitmaps_ops[i], &word);
		it->cur_word &= word;

		/*
		printf("NextWord iter: %zu => offset=%zu, word=%lu\n",
			i, it->offsets[i], word);
			*/
	}

	if (it->result_ops & BITMAP_OP_NOT) {
		bool not_not_flag = true;
		/*
		 * Check for special NOT-NOT case:
		 * we should stop iterating if both result and all bitmaps
		 * have OP_NOT flag and more data in bitmaps
		 */
		for(size_t i = 0; i < it->bitmaps_size; i++) {
			if (it->pages[i]->next != NULL) {
				not_not_flag = false;
				break;
			}
		}

		if (not_not_flag) {
			it->cur_pos = SIZE_MAX;
			it->cur_word  = BITMAP_WORD_MIN;
			return -1;
		}

		it->cur_word = ~(it->cur_word);
	}

	/*
	printf("NextWord result: word=%lu cur_pos=%zu\n",
		it->cur_word, it->cur_pos);
	*/

	return 0;
}

static
void iter_next(struct slist_node **pnode, size_t *poffset,
	int bitmap_ops, bitmap_word_t *word) {
	struct slist_node *node = *pnode;
	for(; node->next != NULL; node = node->next) {
		struct bitmap_page *page = slist_member_of(node->next,
						struct bitmap_page, node);

		size_t page_first_pos = page->first_pos;
		size_t page_last_pos = page_first_pos +
				BITMAP_WORDS_PER_PAGE * BITMAP_WORD_BIT;

		if (*poffset < page_first_pos) {
			*poffset = page_first_pos;
			*pnode = node;

			if (bitmap_ops & BITMAP_OP_NOT) {
				*word = BITMAP_WORD_MAX;
			} else {
				*word = BITMAP_WORD_MIN;
			}
			return;
		}

		if (*poffset >= page_last_pos) {
			continue;
		}

		size_t w = (*poffset - page_first_pos) / BITMAP_WORD_BIT;
		if (bitmap_ops & BITMAP_OP_NOT) {
			*word = ~(page->words[w]);
		} else {
			*word = page->words[w];
		}
		*pnode = node;
		(*poffset)++;
		/*
		 * TODO(roman):
		 * Check *poffset = page_first_pos + (w+1) * BITMAP_WORD_BIT;
		 */

		return;
	}

	/*
	 * No more data in the bitmap
	 */

	/* update page node pointer */
	*pnode = node;

	if ((bitmap_ops & BITMAP_OP_NOT)) {
		/* no more words - return all ones */
		*word = BITMAP_WORD_MAX;
		/* TODO(roman): (*poffset) += BITMAP_WORD_BIT ? */
		(*poffset)++;
	} else {
		/* no more words in bitmap */
		*poffset = SIZE_MAX;
		*word = BITMAP_WORD_MIN;
	}

	return;
}

/* }}} */

























