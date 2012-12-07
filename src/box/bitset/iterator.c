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


#include "bitset.h"
#include "bitset_p.h"
#include "expr_p.h"

/**
 * BitmapIterator definition
 */

/* like bitset_expr */
struct bitset_itstate {
	/** groups */
	struct bitset_itstate_group **groups;
	/** number of active groups in the expr */
	size_t groups_capacity;
};

/* like bitset_expr_group */
struct bitset_itstate_group {
	/** number of allocated elements in the state */
	size_t elements_capacity;
	struct bitset_iterator_state_element {
		struct bitset_page *page;
		size_t offset;
	} elements[];
};

struct bitset_iterator {
	struct bitset_expr *expr;
	struct bitset_itstate state;

	size_t cur_pos;
	/* temporary buffer for word_index data */
	int indexes[BITSET_WORD_BIT + 1];
	/* position in the indexes buffer */
	int indexes_pos;
};

static int
itstate_reserve(struct bitset_itstate *itstate, size_t capacity)
{
	if (itstate->groups_capacity >= capacity) {
		return 0;
	}

	struct bitset_itstate_group **groups =
		realloc(itstate->groups, capacity * sizeof(*itstate->groups));

	if (groups == NULL) {
		return 0;
	}

	itstate->groups = groups;
	for (size_t g = itstate->groups_capacity; g < capacity; g++) {
		struct bitset_itstate_group *group;
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
itstate_reserve_group(struct bitset_itstate *itstate,
		      size_t group_id, size_t capacity)
{
	assert(itstate->groups_capacity > group_id);

	struct bitset_itstate_group *orig_group = itstate->groups[group_id];

	if (orig_group->elements_capacity >= capacity) {
		return 0;
	}

	size_t capacity2 = orig_group->elements_capacity;
	while (capacity2 < capacity) {
		capacity2 *= 2;
	}

	struct bitset_itstate_group *new_group;
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


static int
next_word(struct bitset_iterator *it);

static int
next_word_in_group(struct bitset_expr_group *group,
		   struct bitset_itstate_group *state,
		   size_t *cur_pos, bitset_word_t *pword);

static void
next_word_in_bitset(struct bitset *bitset,
		    struct bitset_page **ppage, size_t *poffset,
		    int bitset_ops, bitset_word_t *word);

struct bitset_iterator *
bitset_iterator_new(void)
{
	struct bitset_iterator *it = calloc(1, sizeof(*it));
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
		struct bitset_itstate_group *group;
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

	return it;

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
	return NULL;
}

void
bitset_iterator_delete(struct bitset_iterator *it)
{
	if (!it)
		return;

	for (size_t s = 0; s < it->state.groups_capacity; s++) {
		assert(it->state.groups[s] != NULL);
		free(it->state.groups[s]);
	}
	free(it->state.groups);
	free(it);
}

#if !defined(NDEBUG)
static inline void
check_expr(struct bitset_expr *expr)
{
	/* sanity checks */
	for (size_t g = 0; g < expr->groups_size; g++) {
		assert(expr->groups[g] != NULL);
		assert(expr->groups[g]->reduce_op == BITSET_OP_AND ||
		       expr->groups[g]->reduce_op == BITSET_OP_OR  ||
		       expr->groups[g]->reduce_op == BITSET_OP_XOR);

		assert(expr->groups[g]->post_op == BITSET_OP_NULL);
		/* TODO(roman): support for OP_NOT post_op */
	}
	for (size_t g = 0; g < expr->groups_size; g++) {
		struct bitset_expr_group *group = expr->groups[g];
		for (size_t b = 0; b < group->elements_size; b++) {
			assert(group->elements[b].bitset != NULL);
			assert(group->elements[b].pre_op == BITSET_OP_NULL ||
			       group->elements[b].pre_op == BITSET_OP_NOT);
		}
	}
}
#endif /* !defined(DEBUG) */

int
bitset_iterator_set_expr(struct bitset_iterator *it,
			 struct bitset_expr *expr)
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
	bitset_iterator_rewind(it);

	return 0;
}

struct bitset_expr *
bitset_iterator_get_expr(struct bitset_iterator *it)
{
	assert(it != NULL);
	return it->expr;
}



void
bitset_iterator_rewind(struct bitset_iterator *it)
{
	assert(it != NULL);
	assert(it->state.groups_capacity >= it->expr->groups_size);

	if (it->expr->groups_size == 0) {
		it->cur_pos = SIZE_MAX;
		return;
	}

	for (size_t g = 0; g < it->expr->groups_size; g++) {
		struct bitset_expr_group *group = it->expr->groups[g];
		struct bitset_itstate_group *state = it->state.groups[g];

		for (size_t b = 0; b < group->elements_size; b++) {
			struct bitset *bitset = group->elements[b].bitset;
			state->elements[b].offset = 0;
			/* can be NULL, RB_MIN is an optimization */
			state->elements[b].page =
				RB_MIN(bitset_pages_tree, &(bitset->pages));
		}
	}

	it->cur_pos = 0;
	it->indexes[0] = 0;
	it->indexes_pos = 0;
}

size_t
bitset_iterator_next(struct bitset_iterator *it)
{
	assert(it->expr != NULL);

	if (it->cur_pos == SIZE_MAX) {
		return SIZE_MAX;
	}

	while (true) {
		if ((it->cur_pos % BITSET_WORD_BIT) == 0) {
			if (next_word(it) < 0) {
				break;
			}
		}

		const size_t word_first_pos = it->cur_pos -
				(it->cur_pos % BITSET_WORD_BIT);

		if (it->indexes[it->indexes_pos] != 0) {
			it->cur_pos = word_first_pos +
					it->indexes[it->indexes_pos++];
			return (it->cur_pos - 1);
		} else {
			/* not more bits in current word */
			it->cur_pos = word_first_pos + BITSET_WORD_BIT;
			continue;
		}
	}

	return SIZE_MAX;
}

static int
next_word(struct bitset_iterator *it)
{
	bitset_word_t word;

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
	 * Gets next words from each bitset group and apply final AND reduction
	 */
	word = word_set_ones();
	while(true) {
		size_t offset_max = it->cur_pos;
		for (size_t s = 0; s < it->expr->groups_size; s++) {
			bitset_word_t tmp_word = word_set_zeros();

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


static int
next_word_in_group_and(struct bitset_expr_group *group,
		       struct bitset_itstate_group *state,
		       size_t *pcur_pos, bitset_word_t *pword);
static int
next_word_in_group_or_xor(struct bitset_expr_group *group,
			  struct bitset_itstate_group *state,
			  size_t *pcur_pos, bitset_word_t *pword);

static int
next_word_in_group(struct bitset_expr_group *group,
		   struct bitset_itstate_group *state,
		   size_t *pcur_pos, bitset_word_t *pword)
{
	*pcur_pos = *pcur_pos - (*pcur_pos % BITSET_WORD_BIT);

	if (group->elements_size < 1) {
		/* empty group */
		*pcur_pos = SIZE_MAX;
		*pword = word_set_zeros();
		return -1;
	}

	switch(group->reduce_op) {
	case BITSET_OP_AND:
		return next_word_in_group_and(group, state, pcur_pos, pword);
	case BITSET_OP_OR:
	case BITSET_OP_XOR:
		return next_word_in_group_or_xor(group, state, pcur_pos, pword);
	default:
		/* not implemented */
		return -2;
	}
}

static int
next_word_in_group_and(struct bitset_expr_group *group,
		       struct bitset_itstate_group *state,
		       size_t *pcur_pos, bitset_word_t *pword)
{
	assert(group->reduce_op == BITSET_OP_AND);

#if 0
	/*
	 * Try to sync all bitsets and find first pos for that
	 * pages && words exist in all bitsets in this group
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
	size_t page_first_pos = bitset_page_first_pos(*pcur_pos);
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
			bitset_word_t tmp_word = word_set_zeros();

			state->elements[b].offset = next_pos;
			next_word_in_bitset(group->elements[b].bitset,
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
		bitset_word_t tmp_word = word_set_zeros();

		state->elements[b].offset = next_pos;
		next_word_in_bitset(group->elements[b].bitset,
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

static int
next_word_in_group_or_xor(struct bitset_expr_group *group,
			  struct bitset_itstate_group *state,
			  size_t *pcur_pos, bitset_word_t *pword)
{
	assert(group->reduce_op == BITSET_OP_OR ||
	       group->reduce_op == BITSET_OP_XOR);

	/* get minimum position from all bitsets */
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
		bitset_word_t tmp_word = word_set_zeros();

		if (state->elements[b].offset <= *pcur_pos) {
			state->elements[b].offset = *pcur_pos;
			next_word_in_bitset(group->elements[b].bitset,
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
		bitset_word_t tmp_word = word_set_zeros();

		if (state->elements[b].offset == *pcur_pos) {
			next_word_in_bitset(group->elements[b].bitset,
				  &(state->elements[b].page),
				  &(state->elements[b].offset),
				  group->elements[b].pre_op,
				  &tmp_word);
			assert(state->elements[b].offset == *pcur_pos);
		} else {
			tmp_word = word_set_zeros();
		}

		if (group->reduce_op == BITSET_OP_OR) {
			*pword = word_or(*pword, tmp_word);
		} else /* BITSET_OP_XOR */ {
			*pword = word_xor(*pword, tmp_word);
		}
	}

	return 0;
}

#define USE_FINDFROM 1
#ifdef USE_FINDFROM
static struct bitset_page *
bitset_pages_tree_RB_FIND_FROM(struct bitset_page *page, struct bitset_page *key)
{
	/* at next time try to lookup starting from current node */
	while ((page != NULL) && (page->first_pos < key->first_pos)) {
		page =  bitset_pages_tree_RB_NEXT(page);
	}

	return page;
}
#endif

static void
next_word_in_bitset(struct bitset *bitset,
		    struct bitset_page **ppage, size_t *poffset,
		    int bitset_ops, bitset_word_t *word)
{

	struct bitset_page key;
	key.first_pos = bitset_page_first_pos(*poffset);

	struct bitset_page *page = *ppage;
	if (page == NULL) {
		/* at first time perform full tree lookup */
		page = RB_NFIND(bitset_pages_tree,
				&(bitset->pages), &key);
	} else {
#ifndef USE_FINDFROM
		if (page->first_pos != key.first_pos) {
			page = RB_NFIND(bitset_pages_tree,
					&(bitset->pages), &key);
		}
#else
		page = bitset_pages_tree_RB_FIND_FROM(page, &key);
#endif /* USE_FINDFROM */
	}

	if (page == NULL) {
		/*
		 * No more data in the bitset
		 */

		if (bitset_ops == BITSET_OP_NOT) {
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

		size_t w = (*poffset - page->first_pos) / BITSET_WORD_BIT;
		if (bitset_ops == BITSET_OP_NOT) {
			*word = word_not(page->words[w]);
		} else {
			*word = page->words[w];
		}

		/* offset has not changed */
		*ppage = page;

		return;
	}

	assert(page->first_pos > key.first_pos);

	if (bitset_ops == BITSET_OP_NOT) {
		/*
		 * Return word_set_ones() until offset < page->first_pos.
		 * Offset && page must not be changed!
		 */
		*word = word_set_ones();
	} else {
		/*
		 * We look for a first page with first_pos >= *poffset.
		 * Since we haven't found a page where first_pos == *poffset,
		 * we can add some hints here in order to speedup upper levels.
		 */

		*poffset = page->first_pos;
		*ppage = page;
		*word = word_set_zeros();
	}

	return;
}
