#ifndef BITMAP_BITMAP_H_INCLUDED
#define BITMAP_BITMAP_H_INCLUDED

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
 * @brief Module to work with arrays of bits (bitmaps)
 * @author Roman Tsisyk
 */
#include <stddef.h>
#include <stdbool.h>

/**
 * Bitmap object
 * Bitmap is an array of bits where each bit can be set or unset independently.
 * It supports special compression scheme in order to save memory.
 * You can use any values in range [0,SIZE_MAX) without worrying about resizing.
 */
struct bitmap;

/**
 * BitmapIterator object
 * Iterator can be used for iterating over bits in a bitmap of group of bitmaps.
 */
struct bitmap_iterator;

/* please use struct pointers everywhere */

/**
 * @brief Allocates and construct new bitmap and sets value of *pbitmap.
 * @param pbitmap
 */
int bitmap_new(struct bitmap **pbitmap);

/**
 * @brief Destruct and deallocates bitmaps and sets *pbitmap to NULL.
 * @param pbitmap
 */
void bitmap_free(struct bitmap **pbitmap);

/**
 * @brief Gets a bit from the bitmap
 * @param bitmap object
 * @param pos position (index)
 * @return bit value
 */
bool bitmap_get(struct bitmap *bitmap, size_t pos);

/**
 * @brief Sets a bit in the bitmap
 * @param bitmap object
 * @param pos position (index)
 * @return 0 on success and -1 on error
 */
int bitmap_set(struct bitmap *bitmap, size_t pos, bool val);


/**
 * @brief Returns the number of bits set to true in this bitmap.
 * @param bitmap object
 * @return returns the number of bits set to true in this bitmap.
 */
size_t bitmap_cardinality(struct bitmap *bitmap);

/**
 * @brief BitmapOp is used as parameter to bitmap_iterator_newn function.
 * @see bitmap_iterator_newn
 */
enum BitmapUnaryOp {
	BITMAP_OP_NULL = 0,
	/* inverse (apply bitwise NOT) bits in the bitmap  */
	BITMAP_OP_NOT   = 0x1
};

enum BitmapBinaryOp {
	BITMAP_OP_AND   = 0x1 << 8,
	BITMAP_OP_NAND  = 0x2 << 8,
	BITMAP_OP_OR    = 0x3 << 8,
	BITMAP_OP_NOR   = 0x4 << 8,
	BITMAP_OP_XOR   = 0x5 << 8,
	BITMAP_OP_XNOR  = 0x6 << 8
};

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

#endif // BITMAP_BITMAP_H_INCLUDED
