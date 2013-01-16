#ifndef BITSET_ITERATOR_H_INCLUDED
#define BITSET_ITERATOR_H_INCLUDED

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
 * @brief Iterator for @link bitset.h bitset @endlink objects
 *
 * BitsetIterator is used for iterating over a result of applying a logical
 * expression (@a struct bitset_expr) to a set of bitsets. The iterator applies
 * this expression on the fly, without producing new temporary bitsets. On each
 * iteration step (@a bitmap_iterator_next call) a next position where the
 * expression evaluates to true is returned.
 *
 * @see expr.h
 * @author Roman Tsisyk
 */

#include "bitset.h"
#include "expr.h"

/** BitsetIterator object **/
struct bitset_iterator;

/**
 * @brief Creates a new iterator object
 * The created iterator must be initialized using
 * @link bitset_iterator_set_expr @endlink method.
 * @return pointer to the bitset_iterator object on success or NULL otherwise
 */
struct bitset_iterator *
bitset_iterator_new(void);

/**
 * @brief Destroys the @a it object
 * @param it object
 * @see bitset_iterator_new
 */
void
bitset_iterator_delete(struct bitset_iterator *it);

/**
 * @brief Initialize the @a it using the @a expr.
 *
 * @a expr object is not copied and must be valid during the iterator lifetime.
 * You can use same @a expr in multiple iterators. You can modify and reuse
 * existing @a expr if no active iterators uses it.
 *
 * @todo Expressions with group.post_op != OP_NULL are not implemented yet
 * @todo Expressions with group.reduce_op != OP_AND, OP_OR, OP_XOR
 * are not implmented yet
 *
 * @param it object
 * @param expr expression
 * @return zero on success and non-zero otherwise
 * @see expr.h
 */
int
bitset_iterator_set_expr(struct bitset_iterator *it,
			 struct bitset_expr *expr);

/**
 * @brief Returns a pointer to the current @a it expression
 * @param it object
 * @return A pointer to the current @a it expression
 * @note Expression must not be changed if the iterator is active.
 * @see bitset_iterator_set_expr
 */
struct bitset_expr *
bitset_iterator_get_expr(struct bitset_iterator *it);

/**
 * @brief Rewinds the @a it to the start position.
 * @param it object
 * @see bitset_iterator_set_expr
 */
void
bitset_iterator_rewind(struct bitset_iterator *it);

/**
 * @brief Moves @a it to a next position
 * @see bitset_iterator_set_expr
 * @param it object
 * @return a next offset where the expression evaluates to true or
 * SIZE_MAX if there is no more elements
 */
size_t
bitset_iterator_next(struct bitset_iterator *it);

#endif /* BITSET_ITERATOR_H_INCLUDED */
