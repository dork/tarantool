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

#include "iterator.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <assert.h>

#include "bitmap.h"
#include "bitmap_p.h"

/**
 * BitmapIterator definition
 */
struct bitmap_iterator {
	size_t groups_size;
	struct bitmap_iterator_group **groups;
	struct bitmap_iterator_state **states;

	bitmap_word_t cur_word;
	size_t cur_pos;
};

/* like bitmap_iterator_group */
struct bitmap_iterator_state {
	int data; // reserved for future use
	struct bitmap_iterator_state_element {
		struct bitmap_page *page;
		size_t offset;
	} elements[];
};

static
int next_word(struct bitmap_iterator *it);
static
int next_word_in_group(struct bitmap_iterator_group *group,
		     struct bitmap_iterator_state *states,
		     size_t *cur_pos, bitmap_word_t *pword);
static
void next_word_in_bitmap(struct bitmap *bitmap,
			 struct bitmap_page **ppage, size_t *poffset,
			 int bitmap_ops, bitmap_word_t *word);

int bitmap_iterator_new(struct bitmap_iterator **pit,
			struct bitmap *bitmap,
			enum bitmap_unary_op pre_op) {

	size_t bitmaps_size = 1;
	struct bitmap_iterator_group *group;
	size_t size = sizeof(*group) + sizeof(*(group->elements)) *
			bitmaps_size;
	group = calloc(1, size);
	if (!group) {
		return -1;
	}

	group->reduce_op = BITMAP_OP_AND;
	group->post_op = BITMAP_OP_NULL;
	group->elements_size = bitmaps_size;
	group->elements[0].bitmap = bitmap;
	group->elements[0].pre_op = pre_op;

	int rc = bitmap_iterator_newgroup(pit, &group, 1);

	free(group);

	return rc;
}

int bitmap_iterator_newn(struct bitmap_iterator **pit,
			 struct bitmap **bitmaps, size_t bitmaps_size,
			 int *bitmaps_ops,
			 int result_ops) {

	struct bitmap_iterator_group *group;
	size_t size = sizeof(*group) + sizeof(*(group->elements)) *
			bitmaps_size;
	group = calloc(1, size);
	if (!group) {
		return -1;
	}


	group->reduce_op = BITMAP_OP_AND;
	group->post_op = result_ops;
	group->elements_size = bitmaps_size;

	for(size_t b = 0; b < bitmaps_size; b++) {
		group->elements[b].bitmap = bitmaps[b];
		group->elements[b].pre_op = bitmaps_ops[b];
	}

	int rc = bitmap_iterator_newgroup(pit, &group, 1);

	free(group);

	return rc;

}

int bitmap_iterator_newgroup(struct bitmap_iterator **pit,
			struct bitmap_iterator_group **groups,
			size_t groups_size)
{
#ifndef NDEBUG
	/* sanity checks */
	assert(groups != NULL);
	assert(groups_size > 0);
	for (size_t s = 0; s < groups_size; s++) {
		assert(groups[s] != NULL);
		assert(groups[s]->reduce_op == BITMAP_OP_AND ||
		       groups[s]->reduce_op == BITMAP_OP_OR  ||
		       groups[s]->reduce_op == BITMAP_OP_XOR);

		assert(groups[s]->post_op == BITMAP_OP_NULL);
		/* TODO(roman): support for OP_NOT post_op */
	}
	for (size_t s = 0; s < groups_size; s++) {
		for (size_t b = 0; b < groups[s]->elements_size; b++) {
			assert(groups[s]->elements[b].bitmap != NULL);
			assert(groups[s]->elements[b].pre_op == BITMAP_OP_NULL ||
			       groups[s]->elements[b].pre_op == BITMAP_OP_NOT);
		}
	}
#endif /* ndef NDEBUG */

	int rc = -1;
	struct bitmap_iterator *it = calloc(1, sizeof(struct bitmap_iterator));
	if (it == NULL) {
		goto error_0;
	}

	/* Allocate groups */
	it->groups_size = groups_size;
	it->groups = calloc(groups_size, sizeof(*(it->groups)));
	if (it->groups == NULL) {
		goto error_1;
	}

	for (size_t g = 0; g < groups_size; g++) {
		struct bitmap_iterator_group *group;
		/* allocate memory for structure with flexible array */
		size_t size = sizeof(*group) +
			sizeof(*(group->elements)) * groups[g]->elements_size;
		group = malloc(size);
		if (group == NULL) {
			goto error_2;
		}

		memcpy(group, groups[g], size);
		it->groups[g] = group;
	}


	/* Allocate set states */
	it->states = calloc(groups_size, sizeof(*it->states));
	if (it->groups == NULL) {
		goto error_2;
	}

	for (size_t g = 0; g < groups_size; g++) {
		struct bitmap_iterator_state *state;
		size_t size = sizeof(*state) +
			sizeof(*(state->elements)) * groups[g]->elements_size;
		state = malloc(size);
		if (state == NULL) {
			goto error_3;
		}

		for (size_t b = 0; b < groups[g]->elements_size; b++) {
			struct bitmap *bitmap = groups[g]->elements[b].bitmap;
			state->elements[b].offset = 0;
			/* can be NULL, RB_MIN is an optimization */
			state->elements[b].page =
				RB_MIN(bitmap_pages_tree, &(bitmap->pages));
		}

		it->states[g] = state;
	}

	it->cur_pos = 0;

	next_word(it);

	*pit = it;

	return 0;

/* error handling */
error_3:
	for (size_t s = 0; s < it->groups_size; s++) {
		if (it->states[s] != NULL) {
			free(it->states[s]);
		}
	}
	free(it->states);
error_2:
	for (size_t s = 0; s < it->groups_size; s++) {
		if (it->groups[s] != NULL) {
			free(it->groups[s]);
		}
	}
	free(it->groups);
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
		for (size_t s = 0; s < it->groups_size; s++) {
			if (it->states[s] != NULL) {
				free(it->states[s]);
			}
		}
		free(it->states);

		for (size_t s = 0; s < it->groups_size; s++) {
			if (it->groups[s] != NULL) {
				free(it->groups[s]);
			}
		}
		free(it->groups);

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

		next_word_ret = next_word(it);
	}

	return result;
}

static
int next_word(struct bitmap_iterator *it) {
	if (it->groups_size == 1) {
		/* optimization for case when only one group exist */
		return next_word_in_group(it->groups[0], it->states[0],
				&(it->cur_pos), &(it->cur_word));
	}

	/*
	 * Gets next words from each bitmap group and apply final AND reduction
	 */
	it->cur_word = word_ones();
	while(true) {
		size_t offset_max = it->cur_pos;
		for (size_t s = 0; s < it->groups_size; s++) {
			bitmap_word_t tmp_word = word_zeros();

			if (next_word_in_group(it->groups[s], it->states[s],
					&(offset_max), &(tmp_word)) < 0) {
				it->cur_pos = SIZE_MAX;
				it->cur_word = word_zeros();
				return -1;
			}

			if (offset_max != it->cur_pos) {
				continue;
			}

			it->cur_word = word_and(it->cur_word, tmp_word);
		}

		/* Exit if all groups are in same position */
		if (offset_max == it->cur_pos) {
			return 0;
		}

		/* Otherwise try next word */
		it->cur_pos = offset_max;
	}

	return -1;
}


static
int next_word_in_group_and(struct bitmap_iterator_group *group,
			      struct bitmap_iterator_state *state,
			      size_t *pcur_pos, bitmap_word_t *pword);
static
int next_word_in_group_or_xor(struct bitmap_iterator_group *group,
			      struct bitmap_iterator_state *state,
			      size_t *pcur_pos, bitmap_word_t *pword);

static
int next_word_in_group(struct bitmap_iterator_group *group,
		     struct bitmap_iterator_state *state,
		     size_t *pcur_pos, bitmap_word_t *pword)
{
	*pcur_pos = *pcur_pos - (*pcur_pos % BITMAP_WORD_BIT);

	if (group->elements_size < 1) {
		/* empty group */
		*pcur_pos = SIZE_MAX;
		*pcur_pos = word_zeros();
		return -1;
	}

	switch(group->reduce_op) {
	case BITMAP_OP_AND:
		return next_word_in_group_and(group, state, pcur_pos, pword);
	case BITMAP_OP_OR:
	case BITMAP_OP_XOR:
		return next_word_in_group_or_xor(group, state, pcur_pos, pword);
	default:
		/* not implemented */
		return -2;
	}
}

static
int next_word_in_group_and(struct bitmap_iterator_group *group,
			      struct bitmap_iterator_state *state,
			      size_t *pcur_pos, bitmap_word_t *pword) {
	assert(group->reduce_op == BITMAP_OP_AND);

#if 0
	/*
	 * Try to sync all bitmaps and find first pos for that
	 * pages && words exist in all bitmaps in this group
	 */
	size_t next_pos = *pcur_pos;
	for(size_t b = 0; b < group->elements_size; b++) {
		if (next_pos < state->elements[b].offset) {
			next_pos = state->elements[b].offset;
		}
	}
#endif

	size_t next_pos = *pcur_pos;

#if 0
	bool flag = false;
	size_t page_first_pos = bitmap_page_first_pos(*pcur_pos);
	for(size_t b = 0; b < group->elements_size; b++) {
		if (state->elements[b].page == NULL ||
		    state->elements[b].page->first_pos != page_first_pos) {
			flag = true;
			break;
		}
	}
#endif
	while (true) {
		bool synced = true;

		if (next_pos == SIZE_MAX) {
			/* no more elements */
			*pcur_pos = SIZE_MAX;
			*pword = word_zeros();
			return -1;
		}

		*pword = word_ones();
		for(size_t b = 0; b < group->elements_size; b++) {
			bitmap_word_t tmp_word = word_zeros();

			state->elements[b].offset = next_pos;
			next_word_in_bitmap(group->elements[b].bitmap,
				  &(state->elements[b].page),
				  &(state->elements[b].offset),
				  group->elements[b].pre_op,
				  &tmp_word);

			if (state->elements[b].offset > next_pos) {
				synced = false;
				next_pos = state->elements[b].offset;
				break;
			}

			*pword = word_and(*pword, tmp_word);
		}

		if (synced) {
			break;
		}
	}

	*pcur_pos = next_pos;

#if 0
	*pword = word_ones();
	for(size_t b = 0; b < group->elements_size; b++) {
		bitmap_word_t tmp_word = word_zeros();

		state->elements[b].offset = next_pos;
		next_word_in_bitmap(group->elements[b].bitmap,
			  &(state->elements[b].page),
			  &(state->elements[b].offset),
			  group->elements[b].pre_op,
			  &tmp_word);

		*pword = word_and(*pword, tmp_word);
	}

	*pcur_pos = next_pos;
#endif

	return 0;
}

static
int next_word_in_group_or_xor(struct bitmap_iterator_group *group,
			      struct bitmap_iterator_state *state,
			      size_t *pcur_pos, bitmap_word_t *pword) {
	assert(group->reduce_op == BITMAP_OP_OR ||
	       group->reduce_op == BITMAP_OP_XOR);

	/* get minimum position from all bitmaps */
	size_t next_pos = SIZE_MAX;
	for(size_t b = 0; b < group->elements_size; b++) {
		if (next_pos > state->elements[b].offset) {
			next_pos = state->elements[b].offset;
		}
	}

	if (next_pos < *pcur_pos) {
		next_pos = *pcur_pos;
	}

	for(size_t b = 0; b < group->elements_size; b++) {
		bitmap_word_t tmp_word = word_zeros();

		if (state->elements[b].offset <= *pcur_pos) {
			state->elements[b].offset = *pcur_pos;
			next_word_in_bitmap(group->elements[b].bitmap,
				  &(state->elements[b].page),
				  &(state->elements[b].offset),
				  group->elements[b].pre_op,
				  &tmp_word);
			/*
			if (state->elements[b].offset > next_pos) {
				next_pos = state->elements[b].offset;
			}
			*/
		}
	}

	if (next_pos == SIZE_MAX) {
		/* no more elements */
		*pcur_pos = SIZE_MAX;
		*pcur_pos = word_zeros();
		return -1;
	}

	*pcur_pos = next_pos;

	*pword = word_zeros();
	for(size_t b = 0; b < group->elements_size; b++) {
		bitmap_word_t tmp_word = word_zeros();

		if (state->elements[b].offset == *pcur_pos) {
			next_word_in_bitmap(group->elements[b].bitmap,
				  &(state->elements[b].page),
				  &(state->elements[b].offset),
				  group->elements[b].pre_op,
				  &tmp_word);
			assert(state->elements[b].offset == *pcur_pos);
		} else {
			tmp_word = word_zeros();
		}

		if (group->reduce_op == BITMAP_OP_OR) {
			*pword = word_or(*pword, tmp_word);
		} else /* BITMAP_OP_XOR */ {
			*pword = word_xor(*pword, tmp_word);
		}
	}

	return 0;
}

#define USE_FINDFROM 1
#ifdef USE_FINDFROM
static
struct bitmap_page *bitmap_pages_tree_RB_FIND_FROM(
		struct bitmap_page *page,
		struct bitmap_page *key) {
	/* at next time try to lookup starting from current node */
	while ((page != NULL) && (page->first_pos < key->first_pos)) {
		page =  bitmap_pages_tree_RB_NEXT(page);
	}

	return page;
}
#endif

static
void next_word_in_bitmap(struct bitmap *bitmap,
			 struct bitmap_page **ppage, size_t *poffset,
			 int bitmap_ops, bitmap_word_t *word) {

	struct bitmap_page key;
	key.first_pos = bitmap_page_first_pos(*poffset);

	struct bitmap_page *page = *ppage;
	if (page == NULL) {
		/* at first time perform full tree lookup */
		page = RB_NFIND(bitmap_pages_tree,
				&(bitmap->pages), &key);
	} else {
#ifndef USE_FINDFROM
		if (page->first_pos != key.first_pos) {
			page = RB_NFIND(bitmap_pages_tree,
					&(bitmap->pages), &key);
		}
#else
		page = bitmap_pages_tree_RB_FIND_FROM(page, &key);
#endif /* USE_FINDFROM */
	}

	if (page == NULL) {
		/*
		 * No more data in the bitmap
		 */

		if (bitmap_ops == BITMAP_OP_NOT) {
			/* no more words - return all ones */
			*word = word_ones();
		} else {
			/* no more words - stop iteration */
			*poffset = SIZE_MAX;
			*word = word_zeros();
		}

		*ppage = page;

		return;
	}

	if (page->first_pos == key.first_pos) {
		/*
		 * Word with requested offset has been found
		 */

		size_t w = (*poffset - page->first_pos) / BITMAP_WORD_BIT;
		if (bitmap_ops == BITMAP_OP_NOT) {
			*word = word_not(page->words[w]);
		} else {
			*word = page->words[w];
		}

		/* offset has not changed */
		*ppage = page;

		return;
	}

	assert(page->first_pos > key.first_pos);

	if (bitmap_ops == BITMAP_OP_NOT) {
		/*
		 * Return word_ones() until offset < page->first_pos.
		 * Offset && page must not be changed!
		 */
		*word = word_ones();
	} else {
		/*
		 * We look for first page with first_pos >= *poffset.
		 * Since we does not found page where first_pos == *poffset,
		 * we can add some hints here in order to speedup upper levels.
		 *
		 * Next time upper levels would call this method with
		 * *poffset >= page->first_pos;
		 */

		*poffset = page->first_pos;
		*ppage = page;
		*word = word_zeros();
	}

	return;
}
