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

#include "index_bitset.h"

#include <string.h>
#include <errno.h> // for tnt_raise messages

#include "salloc.h"
#include "tuple.h"
#include "space.h"
#include "exception.h"
#include "pickle.h"
#include "bitset/bitset.h"
#include "bitset/index.h"

/* {{{ BitsetIndex ************************************************/

static struct index_traits bitset_index_traits = {
	.allows_partial_key = false,
};

inline
size_t tuple_to_value(struct tuple *tuple) {
	size_t value = salloc_ptr_to_index(tuple);
	assert(salloc_ptr_from_index(value) == tuple);

	return value;
}

inline
struct tuple *value_to_tuple(size_t value) {
	return salloc_ptr_from_index(value);
}

inline
void make_bitset_key(void *key, void **bitset_key, size_t *bitset_key_size) {
	assert(key != NULL);

	u32 size = load_varint32(&key);
	*bitset_key = key;
	*bitset_key_size = (size_t) size;
#ifdef DEBUG
	if (size == 4) {
		u32 kk;
		memcpy(&kk, key, size);
		say_debug("BitsetIndex: make_bitset_key (u32) %u", kk);
	} else if(size == 8) {
		u64 kk;
		memcpy(&kk, key, size);
		say_debug("BitsetIndex: make_bitset_key (u64) %lu", kk);
	}
#endif /* DEBUG */
}

/* wraps bitset_iterator insed tarantool's iterator */
struct iterator_wrapper {
	struct iterator base; /* Must be the first member. */
	struct bitset_iterator *bitset_it;
	struct bitset_expr *bitset_expr;
};

static struct iterator_wrapper *
iterator_wrapper(struct iterator *it)
{
	return (struct iterator_wrapper *) it;
}

struct tuple *
iterator_wrapper_next(struct iterator *iterator)
{
	assert(iterator->next == iterator_wrapper_next);
	struct iterator_wrapper *it = iterator_wrapper(iterator);
	size_t value = bitset_iterator_next(it->bitset_it);

	if (value == SIZE_MAX) {
		return NULL;
	} else {
		struct tuple *tuple = value_to_tuple(value);
#ifdef DEBUG
		say_debug("BitsetIndex: iterator_next = %zu (%p)",
			  value, tuple);
#endif /* DEBUG */
		return tuple;
	}
}

void
iterator_wrapper_free(struct iterator *iterator)
{
	assert(iterator->next == iterator_wrapper_next);
	struct iterator_wrapper *it = iterator_wrapper(iterator);

	bitset_iterator_free(&(it->bitset_it));
	bitset_expr_free(it->bitset_expr);
	free(it);
}

@implementation BitsetIndex;

+ (struct index_traits *) traits
{
	return &bitset_index_traits;
}

- (id) init: (struct key_def *) key_def_arg :(struct space *) space_arg
{
	say_info("BitsetIndex: init");

	position_expr = bitset_expr_new();
	if (position_expr == NULL) {
		tnt_raise(SystemError, :"bitset_expr_new: %s",
			strerror(errno));
	}

	self = [super init: key_def_arg :space_arg];
	if (self) {
		/* get optimal initial size of the bitset */
		size_t size = 128;
		switch (key_def->parts[0].type) {
		case NUM:
			size = 32;
			break;
		case NUM64:
			size = 64;
			break;
		default:
			size = 128;
			break;
		}

		if (bitset_index_new(&index, size) < 0) {
			bitset_expr_free(position_expr);
			tnt_raise(SystemError, :"bitset_index_new: %s",
				strerror(errno));
		}
	}
	return self;
}

- (void) free
{
	bitset_expr_free(position_expr);
	bitset_index_free(&index);
	[super free];
}

- (void) beginBuild
{

}

- (void) buildNext: (struct tuple *)tuple
{
	[self replace: NULL :tuple];
}

- (void) endBuild
{
	/* nothing */
}

- (void) build: (Index *) pk
{
	struct iterator *it = pk->position;
	struct tuple *tuple;
	[pk initIterator: it :ITER_ALL :NULL :0];

	while ((tuple = it->next(it)))
		[self replace: NULL :tuple];
}

- (size_t) size
{
	return bitset_index_size(index);
}

- (struct tuple *) min
{
	tnt_raise(ClientError, :ER_UNSUPPORTED, "BitsetIndex", "min()");
	return NULL;
}

- (struct tuple *) max
{
	tnt_raise(ClientError, :ER_UNSUPPORTED, "BitsetIndex", "max()");
	return NULL;
}

- (struct tuple *) findByTuple: (struct tuple *) tuple
{
	/* Bitmap index currently is always single-part. */
	void *field = tuple_field(tuple, key_def->parts[0].fieldno);
	if (field == NULL)
		tnt_raise(ClientError, :ER_NO_SUCH_FIELD,
			  key_def->parts[0].fieldno);
	return [self findUnsafe :field :1];
}

- (struct iterator *) allocIterator
{
	struct iterator_wrapper *it = malloc(sizeof(struct iterator_wrapper));
	if (it) {
		memset(it, 0, sizeof(struct iterator_wrapper));
		it->base.next = iterator_wrapper_next;
		it->base.free = iterator_wrapper_free;

		it->bitset_expr = bitset_expr_new();
		if (it->bitset_expr == NULL) {
			tnt_raise(SystemError, :"bitset_expr_new: %s",
				strerror(errno));
		}

		if (bitset_iterator_new(&it->bitset_it) != 0) {
			bitset_expr_free(it->bitset_expr);
			tnt_raise(SystemError, :"bitset_iterator_new: %s",
				strerror(errno));
		}

	}
	return (struct iterator *) it;
}

- (struct tuple *) findUnsafe: (void *) key :(int) part_count
{
	(void) part_count;

	void *bitset_key = NULL;
	size_t bitset_key_size = 0;
	make_bitset_key(key, &bitset_key, &bitset_key_size);

	assert(position->free == iterator_wrapper_free);
	struct iterator_wrapper *it = iterator_wrapper(position);

	if (bitset_index_iterate_equals(
				index, it->bitset_expr,
				bitset_key, bitset_key_size) != 0) {
		tnt_raise(SystemError, :"bitset_index_iterate: %s",
			strerror(errno));
	}

	if (bitset_iterator_set_expr(it->bitset_it, it->bitset_expr) != 0) {
		tnt_raise(SystemError, :"bitset_iterator_init: %s",
			strerror(errno));
	}

	size_t value = bitset_iterator_next(it->bitset_it);

	if (value == SIZE_MAX) {
		return NULL;
	} else {
		struct tuple *tuple = value_to_tuple(value);
#ifdef DEBUG
		say_debug("BitsetIndex: findUnsafe value = %zu (%p)",
			  value, tuple);
#endif
		return tuple;
	}
}

- (void) replace: (struct tuple *) old_tuple
	:(struct tuple *) new_tuple
{
	void *bitset_key;
	size_t bitset_key_size;
	size_t value;

	if (old_tuple != NULL) {
		void *field = tuple_field(old_tuple, key_def->parts[0].fieldno);
		if (unlikely(field == NULL))
			tnt_raise(ClientError, :ER_NO_SUCH_FIELD,
				  key_def->parts[0].fieldno);

		make_bitset_key(field, &bitset_key, &bitset_key_size);
		value = tuple_to_value(old_tuple);
#ifdef DEBUG
		say_debug("BitsetIndex: remove value = %zu (%p)",
			  value, old_tuple);
#endif
		if (bitset_index_remove(index,bitset_key, bitset_key_size,
					value) < 0) {
			tnt_raise(SystemError, :"bitset_index_remove: %s",
				strerror(errno));
		}
	}

	if (new_tuple != NULL) {
		void *field = tuple_field(new_tuple, key_def->parts[0].fieldno);
		if (unlikely(field == NULL))
			tnt_raise(ClientError, :ER_NO_SUCH_FIELD,
				  key_def->parts[0].fieldno);

		make_bitset_key(field, &bitset_key, &bitset_key_size);
		value = tuple_to_value(new_tuple);

#ifdef DEBUG
		say_debug("BitsetIndex: insert value = %zu (%p)",
			  value, new_tuple);
#endif
		if (bitset_index_insert(index,bitset_key, bitset_key_size,
					value) < 0) {
			tnt_raise(SystemError, :"bitset_index_insert: %s",
				strerror(errno));
		}
	}
}

- (void) remove: (struct tuple *) tuple
{
	return [self replace :tuple :NULL];
}

- (void) initIterator: (struct iterator *) iterator
	:(enum iterator_type) type
	:(void *) key :(int) part_count
{
	assert(iterator->free == iterator_wrapper_free);
	struct iterator_wrapper *it = iterator_wrapper(iterator);

	void *bitset_key = NULL;
	size_t bitset_key_size = 0;

	if (type != ITER_ALL) {
		check_key_parts(key_def, part_count,
				bitset_index_traits.allows_partial_key);
		make_bitset_key(key, &bitset_key, &bitset_key_size);
	}


	int rc = 0;
	switch (type) {
	case ITER_ALL:
		bitset_expr_clear(it->bitset_expr);
		rc = 0;
		break;
	case ITER_EQ:
		rc = bitset_index_iterate_equals(
					index, it->bitset_expr,
					bitset_key, bitset_key_size);
		break;
	case ITER_BITS_ALL_SET:
		rc = bitset_index_iterate_all_set(
					index, it->bitset_expr,
					bitset_key, bitset_key_size);
		break;
	case ITER_BITS_ALL_NOTSET:
		rc = bitset_index_iterate_all_not_set(
					index, it->bitset_expr,
					bitset_key, bitset_key_size);
		break;
	case ITER_BITS_ANY_SET:
		rc = bitset_index_iterate_any_set(
					index, it->bitset_expr,
					bitset_key, bitset_key_size);
		break;
	case ITER_BITS_ANY_NOTSET:
		rc = bitset_index_iterate_any_not_set(
					index, it->bitset_expr,
					bitset_key, bitset_key_size);
		break;
	default:
		tnt_raise(ClientError, :ER_UNSUPPORTED,
			  "Bitmap index", "requested iterator type");
	}

	if (rc != 0) {
		tnt_raise(SystemError, :"bitset_index_iterate: %s",
			strerror(errno));
	}

	if (bitset_iterator_set_expr(it->bitset_it, it->bitset_expr) != 0) {
		tnt_raise(SystemError, :"bitset_iterator_init: %s",
			strerror(errno));
	}
}

#if defined(DEBUG)
- (void) debugDump:(int) verbose
{
	say_debug("Debug output for bitset index");
	bitset_index_dump(index, verbose, stderr);
}
#endif /* defined(DEBUG) */
@end

/* }}} */

