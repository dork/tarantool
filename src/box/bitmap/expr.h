#ifndef BITMAP_EXPR_H_INCLUDED
#define BITMAP_EXPR_H_INCLUDED

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
 * @brief Expressions for bitmap iterator.
 *
 * Iterator can apply logical operations on bitmaps on the fly, without
 * producing temporary bitmaps. At first stage, bitmap.pre_op is applied to
 * each bitmap of the group. Then reduce_op is cumulatively applied to the items
 * from left to right (like foldl), so as to reduce the group to a single value.
 * Example: group1 = b1 || ~b2 || b3 || ~b4. After that group.post_op is applied
 * on the result after that. On next step, iterator combines results from all
 * groups using binary AND operation.
 * Example: (group1) & ~(group2) & ~(group3)
 * Due to fact that every propositional formula can be represented using CNF,
 * you can construct any logical expression you want using scheme above.
 *
 * @link http://en.wikipedia.org/wiki/Conjunctive_normal_form @endlink
 * @note Reduce operations in both cases are left-associate.
 *
 * @see bitmap_iterator_set_expr
 * @author Roman Tsisyk
 */

#include "bitmap.h"

/**
 * @brief  Unary operation
 * @see bitmap_expr
 */
enum bitmap_unary_op {
	/** return bitmaps as is, without applying any unary operation */
	BITMAP_OP_NULL = 0,
	/** inverse (apply bitwise NOT) bits in the bitmap  */
	BITMAP_OP_NOT  = 0x1
};

/**
 * @brief Binary operation type
 * @see bitmap_expr
 */
enum bitmap_binary_op {
	/** bitwise AND */
	BITMAP_OP_AND   = 0x1 << 8,
	/** bitwise NOT AND */
	BITMAP_OP_NAND  = 0x2 << 8,
	/** bitwise OR */
	BITMAP_OP_OR    = 0x3 << 8,
	/** bitwise NOT OR */
	BITMAP_OP_NOR   = 0x4 << 8,
	/** bitwise XOR */
	BITMAP_OP_XOR   = 0x5 << 8,
	/** bitwise NOT XOR */
	BITMAP_OP_XNOR  = 0x6 << 8
};

struct bitmap_expr;

/**
 * @brief Create new bitmap expression
 * @return Pointer to expression object or NULL if memory allocation has failed
 */
struct bitmap_expr *
bitmap_expr_new();

/**
 * @brief Destroy @expr object
 * @param expr
 */
void
bitmap_expr_free(struct bitmap_expr *expr);


/**
 * @brief Clear @a expression (remove all groups)
 * @param expr object
 */
void
bitmap_expr_clear(struct bitmap_expr *expr);


/**
 * @brief Add a new group to @a expression.
 * Resulting group is look like (post_op (b1 reduce_op ~b2 reduce_op b3)).
 * @param expr object
 * @param reduce_op operation applied to pairs of bitmaps in the expr
 * @param post_op operation applied to result
 * @return zero on success and non-zero otherwise
 * @see bitmap_iterator_set_expr()
 */
int
bitmap_expr_add_group(struct bitmap_expr *expr,
		      enum bitmap_binary_op reduce_op,
		      enum bitmap_unary_op post_op);

/**
 * @brief Returns number of groups in the @a expr
 * @param expr object
 * @return number of groups in the @a expr
 */
size_t
bitmap_expr_size(struct bitmap_expr *expr);


/**
 * @brief Adds @a bitmap to group with id @a group_id
 * @param expr object
 * @param group_id group identifier
 * @param bitmap bitmap
 * @param pre_op operation that applied to bitmap
 * @return zero on success and non-zero otherwise
 */
int
bitmap_expr_group_add_bitmap(struct bitmap_expr *expr,
			     size_t group_id,
			     struct bitmap *bitmap,
			     enum bitmap_unary_op pre_op);

/**
 * @brief Clear group with identifier = @a group_id
 * @param expr object
 * @param group_id  group identifier
 */
void
bitmap_expr_group_clear(struct bitmap_expr *expr,
			size_t group_id);

/**
 * @brief Returns number of bitmaps in the @a group
 * @param expr object
 * @param group_id group identifier
 * @return Number of bitmaps in the @a group
 */
size_t
bitmap_expr_group_size(struct bitmap_expr *expr,
		       size_t group_id);

#endif /* BITMAP_EXPR_H_INCLUDED */
