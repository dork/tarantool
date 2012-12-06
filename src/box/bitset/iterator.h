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
 * @brief Iterator for bitset objects.
 *
 * The iterator can apply logical operations on bitsets on the fly, without
 * producing temporary bitsets.
 * @see expr.h
 * @author Roman Tsisyk
 */

#include <util.h>

#include "bitset.h"
#include "expr.h"

/**
 * BitsetIterator object
 * Iterator can be used for iterating over bits in a bitset of group of bitsets.
 */
struct bitset_iterator;

/**
 * @brief Allocates new iterator
 * After creation the iterator must be initialed using
 * @a bitset_iterator_set_expr method.
 * @param pit pointer to object
 * @return zero on success and non-zero otherwise
 */
int
bitset_iterator_new(struct bitset_iterator **pit);

/**
 * @brief Destroys thee iterator
 * @param pit pointer to object
 * @see bitset_iterator_new
 */
void
bitset_iterator_free(struct bitset_iterator **pit);

/**
 * @brief Initializer interator from the @a expr.
 *
 * @a expr object is not copied and must exist during the iterator lifetime.
 * You can use same @a expr in multiple iterators. You can modify and reuse
 * existing @a expr if no active iterators uses it.
 * @todo group.post_op != OP_NULL is not implemented yet
 * @todo group.reduce_op != OP_AND, OP_OR, OP_XOR is not implmented yet
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
 * @brief Returns a pointer to current iterator expression
 * @param it object
 * @return A pointer to current iterator expression
 * @note Expression must not be changed if the iterator is active.
 * @see bitset_iterator_set_expr
 */
struct bitset_expr *
bitset_iterator_get_expr(struct bitset_iterator *it);

/**
 * @brief Rewinds the iterator to the start position.
 * @param it object
 * @see bitset_iterator_set_expr
 */
void
bitset_iterator_rewind(struct bitset_iterator *it);

/**
 * @brief Moves iterator to next position.
 * @see bitset_iterator_set_expr
 * @param it object
 * @return offset where next bit is set or SIZE_MAX if no more bits
 */
size_t
bitset_iterator_next(struct bitset_iterator *it);

#endif /* BITSET_ITERATOR_H_INCLUDED */
