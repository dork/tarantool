#ifndef BITMAP_ITERATOR_H_INCLUDED
#define BITMAP_ITERATOR_H_INCLUDED

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
	enum BitmapBinaryOp reduce_op; /* only AND, OR, XOR supported */
	/** operation to apply after combining */
	enum BitmapUnaryOp post_op; /* not supported yet */
	/** number of elements on this group */
	size_t elements_size;
	/** elements */
	struct bitmap_iterator_group_element {
		/** operation to apply to source bitmap before reducing */
		enum BitmapUnaryOp pre_op;
		/** source bitmap */
		struct bitmap *bitmap;
	} elements[];
};

int bitmap_iterator_new(struct bitmap_iterator **pit,
			struct bitmap *group,
			enum BitmapUnaryOp pre_op);

/**
 * @brief Creates new allocator for group of bitmaps
 *	Iterator performs logical AND operation on the group of bitmaps and
 *	returns next position where bit in resulting bitmap is set
 * @param pit object
 * @param bitmaps list of bitmaps
 * @param bitmaps_size size of bitmaps parameters
 * @param bitmaps_ops operations that applied to each bitmap before ANDing
 * @param result_ops operations that applied to result bitmap
 */
__attribute__ ((deprecated))
int bitmap_iterator_newn(struct bitmap_iterator **pit,
			 struct bitmap **bitmaps, size_t bitmaps_size,
			 int *bitmaps_ops,
			 int result_ops);

int bitmap_iterator_newgroup(struct bitmap_iterator **pit,
			struct bitmap_iterator_group **groups,
			size_t groups_size);

/**
 * @brief Destroys iterator
 * @param pit objects
 */
void bitmap_iterator_free(struct bitmap_iterator **pit);

/**
 * @brief Iterates over the bitmap
 * @see bitmap_iterator_newn
 * @param it object
 * @return offset or SIZE_MAX if no more bits in the bitmap group
 */
size_t bitmap_iterator_next(struct bitmap_iterator *it);

#endif /* BITMAP_ITERATOR_H_INCLUDED */
