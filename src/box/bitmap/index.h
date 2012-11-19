#ifndef BITMAP_INDEX_H_INCLUDED
#define BITMAP_INDEX_H_INCLUDED

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
 * @brief Bit index.
 * @see bitmap.h
 * @author Roman Tsisyk
 */

#include <util.h>

#include "bitmap.h"
#include "iterator.h"

struct bitmap_index;

/**
 * @brief Creates new index object
 * @param pindex
 * @param initial_size initial number of used bitmaps
 * @return zero on success and non-zero otherwise
 */
int bitmap_index_new(struct bitmap_index **pindex, size_t initial_size);

/**
 * @brief Destroys index object
 * @param pindex
 */
void bitmap_index_free(struct bitmap_index **pindex);

/**
 * @brief Inserts (key, value) pair into the index.
 * @param index object
 * @param key key
 * @param key_size size of key
 * @param value value
 * @return zero on success and non-zero otherwise
 */
int bitmap_index_insert(struct bitmap_index *index,
			void *key, size_t key_size,
			size_t value);

/**
 * @brief Removes (key, value) pair from the index.
 * @param index object
 * @param key key
 * @param key_size size of key
 * @param value value
 * @return zero on success and non-zero otherwise
 */
int bitmap_index_remove(struct bitmap_index *index,
			void *key, size_t key_size,
			size_t value);

/**
 * @brief Equals iteration. Matches all pairs in the index, where
 * @a key exacttly equals pair.key ((@a key == pair.key).
 * Initialized @a expr can be used with @a bitmap_iterator_init function.
 * @param index
 * @param expr expression
 * @param key key
 * @param key_size size of key
 * @return zero on success and non-zero otherwise
 * @see expr.h
 */
int bitmap_index_iterate_equals(
			struct bitmap_index *index,
			struct bitmap_expr *expr,
			void *key, size_t key_size);

/**
 * @brief All-Bits-Set iteration. Matches all pairs in the index, where
 * all bits from @a key is set in pair.key ((@a key & pair.key) == @a key).
 * Initialized @a expr can be used with @a bitmap_iterator_init function.
 * @param index
 * @param expr expression
 * @param key key
 * @param key_size size of key
 * @return zero on success and non-zero otherwise
 * @see expr.h
 */
int bitmap_index_iterate_all_set(
			struct bitmap_index *index,
			struct bitmap_expr *expr,
			void *key, size_t key_size);

/**
 * @brief Any-Bits-Set iteration. Matches all pairs in the index, where
 * at least on bit from @a key is set in pair.key ((@a key & pair.key) != 0).
 * Initialized @a expr can be used with @a bitmap_iterator_init function.
 * @param index object
 * @param expr expression
 * @param key key
 * @param key_size size of key
 * @return zero on success and non-zero otherwise
 * @see expr.h
 */
int bitmap_index_iterate_any_set(
			struct bitmap_index *index,
			struct bitmap_expr *expr,
			void *key, size_t key_size);

/**
 * @brief Checks if a pairs with @a value exists in the index
 * @param index object
 * @param value
 * @return true if the index contains pair with the @a value
 */
bool bitmap_index_contains_value(struct bitmap_index *index, size_t value);

/**
 * @brief Returns number of pairs in the index.
 * @param index object
 * @return number of pairs in this index
 */
size_t bitmap_index_size(struct bitmap_index *index);


#if defined(DEBUG)
/* TODO: implement bitmap_index_stat */
void bitmap_index_dump(struct bitmap_index *index,
		       int verbose, FILE *stream);

#endif /* defined(DEBUG) */

#endif // BITMAP_INDEX_H_INCLUDED