#ifndef BITSET_EXPR_H_INCLUDED
#define BITSET_EXPR_H_INCLUDED

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
 * @brief Expressions for bitset iterator.
 *
 * Iterator can apply logical operations on bitsets on the fly, without
 * producing temporary bitsets. At first stage, bitset.pre_op is applied to
 * each bitset of the group. Then reduce_op is cumulatively applied to the items
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
 * @see bitset_iterator_set_expr
 * @author Roman Tsisyk
 */

#include "bitset.h"

/**
 * @brief  Unary operation
 * @see bitset_expr
 */
enum bitset_unary_op {
	/** return bitsets as is, without applying any unary operation */
	BITSET_OP_NULL = 0,
	/** inverse (apply bitwise NOT) bits in the bitset  */
	BITSET_OP_NOT  = 0x1
};

/**
 * @brief Binary operation type
 * @see bitset_expr
 */
enum bitset_binary_op {
	/** bitwise AND */
	BITSET_OP_AND,
	/** bitwise NOT AND */
	BITSET_OP_NAND,
	/** bitwise OR */
	BITSET_OP_OR,
	/** bitwise NOT OR */
	BITSET_OP_NOR,
	/** bitwise XOR */
	BITSET_OP_XOR,
	/** bitwise NOT XOR */
	BITSET_OP_XNOR
};

struct bitset_expr;

/**
 * @brief Create new bitset expression
 * @return Pointer to expression object or NULL if memory allocation has failed
 */
struct bitset_expr *
bitset_expr_new(void);

/**
 * @brief Destroy @a expr object
 * @param expr object
 */
void
bitset_expr_delete(struct bitset_expr *expr);


/**
 * @brief Clear @a expr (removes all groups from it)
 * @param expr object
 */
void
bitset_expr_clear(struct bitset_expr *expr);


/**
 * @brief Add a new group to @a expr.
 * Resulting group is look like (post_op (b1 reduce_op ~b2 reduce_op b3)).
 * @param expr object
 * @param reduce_op operation applied to pairs of bitsets in the expr
 * @param post_op operation applied to result
 * @return zero on success and non-zero otherwise
 * @see bitset_iterator_set_expr()
 */
int
bitset_expr_add_group(struct bitset_expr *expr,
		      enum bitset_binary_op reduce_op,
		      enum bitset_unary_op post_op);

/**
 * @brief Returns a number of groups in the @a expr
 * @param expr object
 * @return a number of groups in the @a expr
 */
size_t
bitset_expr_size(struct bitset_expr *expr);


/**
 * @brief Adds @a bitset to a group with id @a group_id
 * @param expr object
 * @param group_id group identifier
 * @param bitset bitset
 * @param pre_op operation that applied to bitset
 * @return zero on success and non-zero otherwise
 */
int
bitset_expr_group_add_bitset(struct bitset_expr *expr,
			     size_t group_id,
			     struct bitset *bitset,
			     enum bitset_unary_op pre_op);

/**
 * @brief Clear a group with id @a group_id
 * @param expr object
 * @param group_id  group identifier
 */
void
bitset_expr_group_clear(struct bitset_expr *expr,
			size_t group_id);

/**
 * @brief Returns a number of bitsets in the @a group
 * @param expr object
 * @param group_id group identifier
 * @return a number of bitsets in the @a group
 */
size_t
bitset_expr_group_size(struct bitset_expr *expr,
		       size_t group_id);

#endif /* BITSET_EXPR_H_INCLUDED */
