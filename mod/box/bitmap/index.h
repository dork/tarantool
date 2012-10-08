#ifndef BITMAP_INDEX_H_INCLUDED
#define BITMAP_INDEX_H_INCLUDED

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
 * @brief Module implements bitmap index using bitmap objects
 * @see struct bitmap
 * @author Roman Tsisyk
 */

#include <errno.h>

#include "bitmap.h"

struct bitmap_index;

int bitmap_index_new(struct bitmap_index **pindex, size_t initial_size);
void bitmap_index_free(struct bitmap_index **pindex);

/**
 * @brief Inserts entry to the index
 * @param index object
 * @param key entry's key
 * @param key_size
 * @param value entry's value
 */
int bitmap_index_insert(struct bitmap_index *index,
			void *key, size_t key_size,
			size_t value);


/**
 * @brief Removes entry from the index
 * @param index object
 * @param key entry's key
 * @param key_size
 * @param value entry's value
 */
int bitmap_index_remove(struct bitmap_index *index,
			void *key, size_t key_size,
			size_t value);

/**
 * @brief Creates new iterator to iterate over index values
 * Please free pit after using by calling bitmap_iterator_free method.
 * @param index
 * @param pit new bitmap_iterator
 * @param key
 * @param key_size
 * @see struct bitmap_iterator
 */
int bitmap_index_iterate(struct bitmap_index *index,
			 struct bitmap_iterator **pit,
			 void *key, size_t key_size);

/**
 * @brief Checks if value present in the index
 * @param index
 * @param value
 * @return true if the index contains entry with this value
 */
bool bitmap_index_contains_value(struct bitmap_index *index, size_t value);

/**
 * @brief Returns number of entries in this index.
 * @param index object
 * @return number of pairs in this index
 */
size_t bitmap_index_size(struct bitmap_index *index);

#endif // BITMAP_INDEX_H_INCLUDED