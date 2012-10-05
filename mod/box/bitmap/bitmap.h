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
void bitmap_new(struct bitmap **pbitmap);

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
 * @return
 */
void bitmap_set(struct bitmap *bitmap, size_t pos, bool val);

/**
 * @brief Creates new allocator for group of bitmaps
 *	Iterator performs logical AND operation on the group of bitmaps and
 *	returns next position where bit in resulting bitmap is set
 * @param pit object
 * @param bitmaps list of bitmaps
 * @param bitmaps_size size of bitmaps parameters
 * @param bitmaps_flags reserved for future use, must be zeros
 * @param result_flags reserved for future use, must be zeros
 */
void bitmap_iterator_newn(struct bitmap_iterator **pit,
			 struct bitmap **bitmaps, size_t bitmaps_size,
			 int *bitmaps_flags,
			 int result_flags);


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
