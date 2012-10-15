#ifndef BITMAP_ITERATOR_H_INCLUDED
#define BITMAP_ITERATOR_H_INCLUDED

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
 * @brief Iterator for bitmap object.
 * @author Roman Tsisyk
 */
#include "bitmap.h"

/**
 * BitmapIterator object
 * Iterator can be used for iterating over bits in a bitmap of group of bitmaps.
 */
struct bitmap_iterator;

struct bitmap_iterator_group {
	/** operation to combine (reduce) bitmaps during iterations **/
	enum bitmap_binary_op reduce_op; /* only AND, OR, XOR supported */
	/** operation to apply after combining */
	enum bitmap_unary_op post_op; /* not supported yet */
	/** number of elements on this group */
	size_t elements_size;
	/** elements */
	struct bitmap_iterator_group_element {
		/** operation to apply to source bitmap before reducing */
		enum bitmap_unary_op pre_op;
		/** source bitmap */
		struct bitmap *bitmap;
	} elements[];
};

/**
 * @brief Simplified version of bitmap_iterator_newgroup for one bitmap
 * @param pit object
 * @param bitmap source bitmap
 * @param pre_op operation applied to bitmap during iteration
 * @return 0 on success and < 0 on error
 * @see bitmap_iterator_newgroup
 */
int bitmap_iterator_new(struct bitmap_iterator **pit,
			struct bitmap *bitmap,
			enum bitmap_unary_op pre_op);

/**
 * @deprecated Replaced with bitmap_iterator_newgroup
 * @brief Simplified version of bitmap_iterator_newgroup
 * @param pit object
 * @param bitmaps list of bitmaps
 * @param bitmaps_size size of bitmaps parameters
 * @param bitmaps_ops operations that applied to each bitmap before ANDing
 * @param result_ops operations that applied to result bitmap
 * @return 0 on success and < 0 on error
 * @see bitmap_iterator_newgroup
 */
__attribute__ ((deprecated))
int bitmap_iterator_newn(struct bitmap_iterator **pit,
			 struct bitmap **bitmaps, size_t bitmaps_size,
			 int *bitmaps_ops,
			 int result_ops);

/**
 * @brief Creates new iterator for a list of bitmap groups.
 * Iterator can apply logical operations on bitmaps on the fly, without
 * producing temporary bitmaps. At first stage, element.pre_op is applied to
 * each bitmap of the group. Then reduce_op is cumulatively applied to the items
 * from left to right (like foldl), so as to reduce the group to a single value.
 * Example: group1 = b1 & ~b2 & b3 & ~b4. After that group.post_op is applied
 * on the result after that. On next step, iterator combines results from all
 * groups using binary AND operation.
 * Example: (group1) & ~(group2) & ~(group3)
 *
 * Please note that reduce operations on both cases are left-associate.
 *
 * TODO(roman): group.post_op other than OP_NULL is not implemented yet
 *
 * @param pit pointer to object
 * @param groups list of groups
 * @param groups_size size of list groups
 * @return 0 on success and < 0 on error
 * @see bitmap_iterator_group
 */
int bitmap_iterator_newgroup(struct bitmap_iterator **pit,
			struct bitmap_iterator_group **groups,
			size_t groups_size);

/**
 * @brief Destroys iterator
 * @param pit pointer to object
 */
void bitmap_iterator_free(struct bitmap_iterator **pit);

/**
 * @brief Iterates over the resulting bitmap
 * @see bitmap_iterator_newn
 * @param it object
 * @return offset where next bit is set or SIZE_MAX if no more bits
 */
size_t bitmap_iterator_next(struct bitmap_iterator *it);

#endif /* BITMAP_ITERATOR_H_INCLUDED */
