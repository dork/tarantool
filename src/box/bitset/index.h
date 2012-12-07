#ifndef BITSET_INDEX_H_INCLUDED
#define BITSET_INDEX_H_INCLUDED

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
 * @see bitset.h
 * @author Roman Tsisyk
 */

#include <util.h>

#include "bitset.h"
#include "iterator.h"

struct bitset_index;

/**
 * @brief Creates new index object
 * @return zero on success and non-zero otherwise
 */
struct bitset_index *
bitset_index_new(void);

/**
 * @copydoc bitset_index_new
 * @param initial_size initial number of bitsets
 */
struct bitset_index *
bitset_index_new2(size_t initial_size);

/**
 * @brief Destroy an @a index object created by @a bitset_index_new
 * @param index object
 */
void
bitset_index_delete(struct bitset_index *index);

/**
 * @brief Inserts (key, value) pair into the index.
 * @param index object
 * @param key key
 * @param key_size size of key
 * @param value value
 * @return zero on success and non-zero otherwise
 */
int bitset_index_insert(struct bitset_index *index,
			void *key, size_t key_size,
			size_t value);

/**
 * @brief Removes (key, value) pair from the index.
 * @param index object
 * @param value value
 */
void
bitset_index_remove_value(struct bitset_index *index, size_t value);

/**
 * @brief Iteration over all elements in the index.
 * Initialized @a expr can be used with @a bitset_iterator_init function.
 * @param index
 * @param expr expression
 * @return zero on success and non-zero otherwise
 * @see expr.h
 */
int
bitset_index_iterate_all(struct bitset_index *index,
			 struct bitset_expr *expr);

/**
 * @brief Equals iteration. Matches all pairs in the index, where
 * @a key exacttly equals pair.key ((@a key == pair.key).
 * Initialized @a expr can be used with @a bitset_iterator_init function.
 * @param index
 * @param expr expression
 * @param key key
 * @param key_size size of key
 * @return zero on success and non-zero otherwise
 * @see expr.h
 */
int
bitset_index_iterate_equals(struct bitset_index *index,
			    struct bitset_expr *expr,
			    void *key, size_t key_size);

/**
 * @brief All-Bits-Set iteration. Matches all pairs in the index, where
 * all bits from @a key is set in pair.key ((@a key & pair.key) != @a key).
 * Initialized @a expr can be used with @a bitset_iterator_init function.
 * @param index
 * @param expr expression
 * @param key key
 * @param key_size size of key
 * @return zero on success and non-zero otherwise
 * @see expr.h
 */
int
bitset_index_iterate_all_set(struct bitset_index *index,
			     struct bitset_expr *expr,
			     void *key, size_t key_size);

/**
 * @brief All-Bits-Not-Set iteration. Matches all pairs in the index, where
 * all bits from @a key is not set in pair.key ((@a key & pair.key) != @a key).
 * Initialized @a expr can be used with @a bitset_iterator_init function.
 * @param index
 * @param expr expression
 * @param key key
 * @param key_size size of key
 * @return zero on success and non-zero otherwise
 * @see expr.h
 */
int
bitset_index_iterate_all_not_set(struct bitset_index *index,
				 struct bitset_expr *expr,
				 void *key, size_t key_size);

/**
 * @brief Any-Bits-Set iteration. Matches all pairs in the index, where
 * at least on bit from @a key is set in pair.key ((@a key & pair.key) != 0).
 * Initialized @a expr can be used with @a bitset_iterator_init function.
 * @param index object
 * @param expr expression
 * @param key key
 * @param key_size size of key
 * @return zero on success and non-zero otherwise
 * @see expr.h
 */
int
bitset_index_iterate_any_set(struct bitset_index *index,
			     struct bitset_expr *expr,
			     void *key, size_t key_size);


/**
 * @brief Any-Bits-Not-Set iteration. Matches all pairs in the index, where
 * at least on bit from @a key is not set in pair.key ((@a key & pair.key) != 0).
 * Initialized @a expr can be used with @a bitset_iterator_init function.
 * @param index object
 * @param expr expression
 * @param key key
 * @param key_size size of key
 * @return zero on success and non-zero otherwise
 * @see expr.h
 */
int
bitset_index_iterate_any_not_set(struct bitset_index *index,
				 struct bitset_expr *expr,
				 void *key, size_t key_size);

/**
 * @brief Checks if a pairs with @a value exists in the index
 * @param index object
 * @param value
 * @return true if the index contains pair with the @a value
 */
bool
bitset_index_contains_value(struct bitset_index *index, size_t value);

/**
 * @brief Returns number of pairs in the index.
 * @param index object
 * @return number of pairs in this index
 */
size_t
bitset_index_size(struct bitset_index *index);


#if defined(DEBUG)
void
bitset_index_dump(struct bitset_index *index, int verbose, FILE *stream);
#endif /* defined(DEBUG) */

#endif // BITSET_INDEX_H_INCLUDED
