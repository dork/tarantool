#ifndef BITMAP_BITMAP_H_INCLUDED
#define BITMAP_BITMAP_H_INCLUDED

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
enum bitmap_unary_op {
	BITMAP_OP_NULL = 0,
	/* inverse (apply bitwise NOT) bits in the bitmap  */
	BITMAP_OP_NOT   = 0x1
};

enum bitmap_binary_op {
	BITMAP_OP_AND   = 0x1 << 8,
	BITMAP_OP_NAND  = 0x2 << 8,
	BITMAP_OP_OR    = 0x3 << 8,
	BITMAP_OP_NOR   = 0x4 << 8,
	BITMAP_OP_XOR   = 0x5 << 8,
	BITMAP_OP_XNOR  = 0x6 << 8
};


#if defined(DEBUG)
struct bitmap_stat {
	bool have_avx:1;
	bool have_sse2:1;
	bool have_ctz:1;
	bool have_popcnt:1;
	int word_bit;
	int page_bit;
	int words_per_page;
	size_t cardinality;
	size_t capacity;
	size_t pages;
	size_t mem_pages;
	size_t mem_other;
};

#include <stdio.h>
void bitmap_stat(struct bitmap *bitmap, struct bitmap_stat *stat);
void bitmap_dump(struct bitmap *bitmap, int verbose, FILE *stream);
#endif

#endif // BITMAP_BITMAP_H_INCLUDED
