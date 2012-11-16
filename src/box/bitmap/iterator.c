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


#include "bitmap.h"
#include "bitmap_p.h"
#include "expr_p.h"

/**
 * BitmapIterator definition
 */

/* like bitmap_expr */
struct bitmap_itstate {
	/** groups */
	struct bitmap_itstate_group **groups;
	/** number of active groups in the expr */
	size_t groups_capacity;
};

struct bitmap_itstate_group {
	/** number of allocated elements in the state */
	size_t elements_capacity;
	struct bitmap_iterator_state_element {
		struct bitmap_page *page;
		size_t offset;
	} elements[];
};

struct bitmap_iterator {
	struct bitmap_expr *expr;
	struct bitmap_itstate state;

	size_t cur_pos;
	int indexes[BITMAP_WORD_BIT + 1]; /* used for word_index */
	int indexes_pos;
};

static int
itstate_reserve(struct bitmap_itstate *itstate, size_t capacity)
{
	if (itstate->groups_capacity >= capacity) {
		return 0;
	}

	struct bitmap_itstate_group **groups =
		realloc(itstate->groups, capacity * sizeof(*itstate->groups));

	if (groups == NULL) {
		return 0;
	}

	itstate->groups = groups;
	for (size_t g = itstate->groups_capacity; g < capacity; g++) {
		struct bitmap_itstate_group *group;
		size_t size = sizeof(*group) +
			sizeof(*(group->elements)) * EXPR_GROUP_DEFAULT_CAPACITY;
		group = calloc(1, size);
		if (group == NULL) {
			return -1;
		}

		itstate->groups[g] = group;
		itstate->groups_capacity++;
	}

	return 0;
}

static int
itstate_reserve_group(struct bitmap_itstate *itstate,
		      size_t group_id, size_t capacity)
{
	assert(itstate->groups_capacity > group_id);

	struct bitmap_itstate_group *orig_group = itstate->groups[group_id];

	if (orig_group->elements_capacity >= capacity) {
		return 0;
	}

	size_t capacity2 = orig_group->elements_capacity;
	while (capacity2 < capacity) {
		capacity2 *= 2;
	}

	struct bitmap_itstate_group *new_group;
	size_t size = sizeof(*new_group) +
		sizeof(*new_group->elements) * capacity2;
	new_group = calloc(1, size);
	if (new_group == NULL) {
		return -1;
	}

	memcpy(new_group->elements,
	       orig_group->elements,
	       sizeof(*orig_group->elements) * orig_group->elements_capacity);

	new_group->elements_capacity = capacity2;
	itstate->groups[group_id] = new_group;

	free(orig_group);
	return 0;
}


static
int next_word(struct bitmap_iterator *it);
static
int next_word_in_group(struct bitmap_expr_group *group,
		       struct bitmap_itstate_group *state,
		       size_t *cur_pos, bitmap_word_t *pword);
static
void next_word_in_bitmap(struct bitmap *bitmap,
			 struct bitmap_page **ppage, size_t *poffset,
			 int bitmap_ops, bitmap_word_t *word);

int bitmap_iterator_new(struct bitmap_iterator **pit)
{
	int rc = -1;
	struct bitmap_iterator *it = calloc(1, sizeof(struct bitmap_iterator));
	if (it == NULL) {
		goto error_0;
	}

	it->state.groups = calloc(EXPR_DEFAULT_CAPACITY,
		sizeof(*it->state.groups));
	if (it->state.groups == NULL) {
		goto error_1;
	}

	it->state.groups_capacity = EXPR_DEFAULT_CAPACITY;

	for (size_t g = 0; g < EXPR_DEFAULT_CAPACITY; g++) {
		struct bitmap_itstate_group *group;
		/* allocate memory for structure with flexible array */
		size_t size = sizeof(*group) +
			sizeof(*(group->elements)) * EXPR_GROUP_DEFAULT_CAPACITY;
		group = malloc(size);
		if (group == NULL) {
			goto error_2;
		}

		group->elements_capacity = EXPR_GROUP_DEFAULT_CAPACITY;
		it->state.groups[g] = group;
	}

	*pit = it;
	return 0;

error_2:
	for (size_t g = 0; g < it->state.groups_capacity; g++) {
		if (it->state.groups[g] != NULL) {
			free(it->state.groups[g]);
		}
	}
	free(it->state.groups);
error_1:
	free(it);
error_0:
	*pit = NULL;
	return rc;
}

#if !defined(NDEBUG)
static inline
void check_expr(struct bitmap_expr *expr)
{
	/* sanity checks */
	for (size_t g = 0; g < expr->groups_size; g++) {
		assert(expr->groups[g] != NULL);
		assert(expr->groups[g]->reduce_op == BITMAP_OP_AND ||
		       expr->groups[g]->reduce_op == BITMAP_OP_OR  ||
		       expr->groups[g]->reduce_op == BITMAP_OP_XOR);

		assert(expr->groups[g]->post_op == BITMAP_OP_NULL);
		/* TODO(roman): support for OP_NOT post_op */
	}
	for (size_t g = 0; g < expr->groups_size; g++) {
		struct bitmap_expr_group *group = expr->groups[g];
		for (size_t b = 0; b < group->elements_size; b++) {
			assert(group->elements[b].bitmap != NULL);
			assert(group->elements[b].pre_op == BITMAP_OP_NULL ||
			       group->elements[b].pre_op == BITMAP_OP_NOT);
		}
	}
}
#endif /* !defined(DEBUG) */

int bitmap_iterator_set_expr(struct bitmap_iterator *it,
			     struct bitmap_expr *expr)
{
	assert(it != NULL);
	assert(expr != NULL);

#if !defined(NDEBUG)
	check_expr(expr);
#endif /* !defined(NDEBUG) */

	if (itstate_reserve(&it->state, expr->groups_size) != 0) {
		return -1;
	}

	for (size_t g = 0; g < expr->groups_size; g++) {
		if (itstate_reserve_group(
			&it->state, g, expr->groups[g]->elements_size) != 0) {
			return -1;
		}
	}

	it->expr = expr;
	bitmap_iterator_rewind(it);

	return 0;
}

struct bitmap_expr *bitmap_iterator_get_expr(struct bitmap_iterator *it)
{
	assert(it != NULL);
	return it->expr;
}

void bitmap_iterator_free(struct bitmap_iterator **pit)
{
	struct bitmap_iterator *it = *pit;

	if (it != NULL) {
		for (size_t s = 0; s < it->state.groups_capacity; s++) {
			assert(it->state.groups[s] != NULL);
			free(it->state.groups[s]);
		}
		free(it->state.groups);
		free(it);
	}

	*pit = NULL;
}

void
bitmap_iterator_rewind(struct bitmap_iterator *it)
{
	assert(it != NULL);
	assert(it->state.groups_capacity >= it->expr->groups_size);

	if (it->expr->groups_size == 0) {
		it->cur_pos = SIZE_MAX;
		return;
	}

	for (size_t g = 0; g < it->expr->groups_size; g++) {
		struct bitmap_expr_group *group = it->expr->groups[g];
		struct bitmap_itstate_group *state = it->state.groups[g];

		for (size_t b = 0; b < group->elements_size; b++) {
			struct bitmap *bitmap = group->elements[b].bitmap;
			state->elements[b].offset = 0;
			/* can be NULL, RB_MIN is an optimization */
			state->elements[b].page =
				RB_MIN(bitmap_pages_tree, &(bitmap->pages));
		}
	}

	it->cur_pos = 0;
	it->indexes[0] = 0;
	it->indexes_pos = 0;
}

size_t bitmap_iterator_next(struct bitmap_iterator *it)
{
	assert(it->expr != NULL);

	if (it->cur_pos == SIZE_MAX) {
		return SIZE_MAX;
	}

	while (true) {
		if ((it->cur_pos % BITMAP_WORD_BIT) == 0) {
			if (next_word(it) < 0) {
				break;
			}
		}

		const size_t word_first_pos = it->cur_pos -
				(it->cur_pos % BITMAP_WORD_BIT);

		if (it->indexes[it->indexes_pos] != 0) {
			it->cur_pos = word_first_pos +
					it->indexes[it->indexes_pos++];
			return (it->cur_pos - 1);
		} else {
			/* not more bits in current word */
			it->cur_pos = word_first_pos + BITMAP_WORD_BIT;
			continue;
		}
	}

	return SIZE_MAX;
}

static
int next_word(struct bitmap_iterator *it) {
	bitmap_word_t word;

	if (it->expr->groups_size == 1) {
		/* optimization for case when only one group exist */
		int rc = next_word_in_group(it->expr->groups[0],
					    it->state.groups[0],
					    &(it->cur_pos),
					    &(word));
		if (rc == 0) {
			word_index(word, it->indexes, 0);
			it->indexes_pos = 0;
			return 0;
		} else {
			word = word_set_zeros();
			return rc;
		}
	}

	/*
	 * Gets next words from each bitmap group and apply final AND reduction
	 */
	word = word_set_ones();
	while(true) {
		size_t offset_max = it->cur_pos;
		for (size_t s = 0; s < it->expr->groups_size; s++) {
			bitmap_word_t tmp_word = word_set_zeros();

			if (next_word_in_group(it->expr->groups[s],
					       it->state.groups[s],
					       &(offset_max),
					       &(tmp_word)) < 0) {
				it->cur_pos = SIZE_MAX;
				word = word_set_zeros();
				return -1;
			}

			if (offset_max != it->cur_pos) {
				continue;
			}

			word = word_and(word, tmp_word);
		}

		/* Exit if all groups are in same position */
		if (offset_max == it->cur_pos) {
			word_index(word, it->indexes, 0);
			it->indexes_pos = 0;
			return 0;
		}

		/* Otherwise try next word */
		it->cur_pos = offset_max;
	}

	return -1;
}


static
int next_word_in_group_and(struct bitmap_expr_group *group,
			      struct bitmap_itstate_group *state,
			      size_t *pcur_pos, bitmap_word_t *pword);
static
int next_word_in_group_or_xor(struct bitmap_expr_group *group,
			      struct bitmap_itstate_group *state,
			      size_t *pcur_pos, bitmap_word_t *pword);

static
int next_word_in_group(struct bitmap_expr_group *group,
		     struct bitmap_itstate_group *state,
		     size_t *pcur_pos, bitmap_word_t *pword)
{
	*pcur_pos = *pcur_pos - (*pcur_pos % BITMAP_WORD_BIT);

	if (group->elements_size < 1) {
		/* empty group */
		*pcur_pos = SIZE_MAX;
		*pword = word_set_zeros();
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
int next_word_in_group_and(struct bitmap_expr_group *group,
			      struct bitmap_itstate_group *state,
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
			*pword = word_set_zeros();
			return -1;
		}

		*pword = word_set_ones();
		for(size_t b = 0; b < group->elements_size; b++) {
			bitmap_word_t tmp_word = word_set_zeros();

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
	*pword = word_set_ones();
	for(size_t b = 0; b < group->elements_size; b++) {
		bitmap_word_t tmp_word = word_set_zeros();

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
int next_word_in_group_or_xor(struct bitmap_expr_group *group,
			      struct bitmap_itstate_group *state,
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
		bitmap_word_t tmp_word = word_set_zeros();

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
		*pword = word_set_zeros();
		return -1;
	}

	*pcur_pos = next_pos;

	*pword = word_set_zeros();
	for(size_t b = 0; b < group->elements_size; b++) {
		bitmap_word_t tmp_word = word_set_zeros();

		if (state->elements[b].offset == *pcur_pos) {
			next_word_in_bitmap(group->elements[b].bitmap,
				  &(state->elements[b].page),
				  &(state->elements[b].offset),
				  group->elements[b].pre_op,
				  &tmp_word);
			assert(state->elements[b].offset == *pcur_pos);
		} else {
			tmp_word = word_set_zeros();
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
			*word = word_set_ones();
		} else {
			/* no more words - stop iteration */
			*poffset = SIZE_MAX;
			*word = word_set_zeros();
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
		 * Return word_set_ones() until offset < page->first_pos.
		 * Offset && page must not be changed!
		 */
		*word = word_set_ones();
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
		*word = word_set_zeros();
	}

	return;
}
