#ifndef BITSET_EXPR_P_H_INCLUDED
#define BITSET_EXPR_P_H_INCLUDED

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
 * @author Roman Tsisyk
 */

#include "expr.h"

struct bitset_expr_group {
	/** operation to combine (reduce) bitsets during iterations **/
	enum bitset_binary_op reduce_op; /* only AND, OR, XOR supported */
	/** operation to apply after combining */
	enum bitset_unary_op post_op; /* not supported yet */

	/** number of elements in the group */
	size_t elements_size;
	/** number of allocated elements in the group */
	size_t elements_capacity;

	/** elements */
	struct bitset_expr_group_element {
		/** operation to apply to source bitset before reducing */
		enum bitset_unary_op pre_op;
		/** source bitset */
		struct bitset *bitset;
	} elements[];
};

struct bitset_expr {
	/** groups */
	struct bitset_expr_group **groups;
	/** number of active groups in the expr */
	size_t groups_size;
	/** number of alloacted groups in the expr */
	size_t groups_capacity;

	bool in_group;
};

extern const size_t EXPR_DEFAULT_CAPACITY;
extern const size_t EXPR_GROUP_DEFAULT_CAPACITY;

#endif /* BITSET_EXPR_P_H_INCLUDED */
