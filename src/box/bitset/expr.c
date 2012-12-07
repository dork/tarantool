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

#include "expr.h"
#include "expr_p.h"

#include <malloc.h>

const size_t EXPR_DEFAULT_CAPACITY = 2;
const size_t EXPR_GROUP_DEFAULT_CAPACITY = 32;

struct bitset_expr *
bitset_expr_new()
{
	struct bitset_expr *expr = calloc(1, sizeof(*expr));
	if (expr == NULL) {
		goto error_0;
	}

	expr->groups = calloc(EXPR_DEFAULT_CAPACITY, sizeof(*expr->groups));
	if (expr->groups == NULL) {
		goto error_1;
	}

	expr->groups_capacity = EXPR_DEFAULT_CAPACITY;

	for (size_t g = 0; g < EXPR_DEFAULT_CAPACITY; g++) {
		struct bitset_expr_group *group;
		/* allocate memory for structure with flexible array */
		size_t size = sizeof(*group) +
			sizeof(*(group->elements)) * EXPR_GROUP_DEFAULT_CAPACITY;
		group = malloc(size);
		if (group == NULL) {
			goto error_2;
		}

		group->elements_capacity = EXPR_GROUP_DEFAULT_CAPACITY;
		group->post_op = BITSET_OP_NULL;
		group->reduce_op = BITSET_OP_OR;
		expr->groups[g] = group;
	}

	bitset_expr_clear(expr);

	return expr;
error_2:
	for (size_t g = 0; g < expr->groups_capacity; g++) {
		if (expr->groups[g] != NULL) {
			free(expr->groups[g]);
		}
	}
	free(expr->groups);
error_1:
	free(expr);
error_0:
	return NULL;
}

void
bitset_expr_delete(struct bitset_expr *expr)
{
	if (!expr)
		return;

	for (size_t g = 0; g < expr->groups_capacity; g++) {
		free(expr->groups[g]);
	}

	free(expr->groups);
	free(expr);
}

void
bitset_expr_clear(struct bitset_expr *expr)
{
	assert(expr != NULL);
	for (size_t g = 0; g < expr->groups_capacity; g++) {
		expr->groups[g]->elements_size = 0;
	}

	expr->groups_size = 0;
}

static int
expr_reserve(struct bitset_expr *expr, size_t capacity)
{
	if (expr->groups_capacity >= capacity) {
		return 0;
	}

	struct bitset_expr_group **groups =
		realloc(expr->groups, capacity * sizeof(*expr->groups));

	if (groups == NULL) {
		return 0;
	}

	expr->groups = groups;
	for (size_t g = expr->groups_capacity; g < capacity; g++) {
		struct bitset_expr_group *group;
		size_t size = sizeof(*group) +
			sizeof(*(group->elements)) * EXPR_GROUP_DEFAULT_CAPACITY;
		group = calloc(1, size);
		if (group == NULL) {
			return -1;
		}
		expr->groups[g] = group;
		expr->groups_capacity++;
	}

	return 0;
}

static int
expr_reserve_group(struct bitset_expr *expr, size_t group_id, size_t capacity)
{
	assert(expr->groups_capacity > group_id);

	struct bitset_expr_group *orig_group = expr->groups[group_id];

	if (orig_group->elements_capacity >= capacity) {
		return 0;
	}

	size_t capacity2 = orig_group->elements_capacity;
	while (capacity2 < capacity) {
		capacity2 *= 2;
	}

	struct bitset_expr_group *new_group;
	size_t size = sizeof(*new_group) +
		sizeof(*new_group->elements) * capacity2;
	new_group = calloc(1, size);
	if (new_group == NULL) {
		return -1;
	}

	memcpy(new_group,
	       orig_group,
	       sizeof(*orig_group) +
	       sizeof(*orig_group->elements) * orig_group->elements_capacity);

	new_group->elements_capacity = capacity2;
	expr->groups[group_id] = new_group;

	free(orig_group);

	return 0;
}

int
bitset_expr_add_group(struct bitset_expr *expr,
		      enum bitset_binary_op reduce_op,
		      enum bitset_unary_op post_op)
{
	assert(expr != NULL);

	const size_t group_id = expr->groups_size;
	if (expr_reserve(expr, group_id + 1) != 0) {
		return -1;
	}

	expr->groups[group_id]->elements_size = 0;
	expr->groups[group_id]->reduce_op = reduce_op;
	expr->groups[group_id]->post_op = post_op;
	expr->groups_size++;

	return 0;
}

size_t
bitset_expr_size(struct bitset_expr *expr)
{
	return expr->groups_size;
}

int
bitset_expr_group_add_bitset(struct bitset_expr *expr,
			     size_t group_id,
			     struct bitset *bitset,
			     enum bitset_unary_op pre_op)
{
	assert(expr != NULL);
	assert(expr->groups_size > group_id);
	assert(bitset != NULL);

	const size_t bitset_id = expr->groups[group_id]->elements_size;
	if (expr_reserve_group(expr, group_id, bitset_id + 1) != 0) {
		return -1;
	}

	expr->groups[group_id]->elements[bitset_id].bitset = bitset;
	expr->groups[group_id]->elements[bitset_id].pre_op = pre_op;
	expr->groups[group_id]->elements_size++;

	return 0;
}

void
bitset_expr_group_clear(struct bitset_expr *expr,
			size_t group_id)
{
	assert(expr != NULL);
	assert(expr->groups_size > group_id);
	expr->groups[group_id]->elements_size = 0;
}

size_t
bitset_expr_group_size(struct bitset_expr *expr,
		       size_t group_id)
{
	assert(expr != NULL);
	assert(expr->groups_size > group_id);

	return expr->groups[group_id]->elements_size;
}
