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

#include "index_bitmap.h"

#include <string.h>
#include <errno.h> // for tnt_raise messages

#include "tuple.h"
#include "space.h"
#include "exception.h"
#include "pickle.h"
#include "bitmap/bitmap.h"
#include "bitmap/index.h"

/* {{{ BitmapIndex ************************************************/

static struct index_traits bitmap_index_traits = {
	.allows_partial_key = false,
};

static const size_t PTR_SHIFT = (CHAR_BIT == 8) ? 3 : 0;

inline
size_t tuple_to_value(struct tuple *tuple) {
	/* TODO: shift pointer address */
	return (size_t) tuple >> PTR_SHIFT;
}

inline
struct tuple *value_to_tuple(size_t value) {
	return (struct tuple *) (value << PTR_SHIFT);
}

inline
void make_bitmap_key(void *key, void **bitmap_key, size_t *bitmap_key_size) {
	assert(key != NULL);

	u32 size = load_varint32(&key);
	*bitmap_key = key;
	*bitmap_key_size = (size_t) size;
#ifdef DEBUG
	if (size == 4) {
		u32 kk;
		memcpy(&kk, key, size);
		say_debug("BitmapIndex: make_bitmap_key (u32) %u", kk);
	} else if(size == 8) {
		u64 kk;
		memcpy(&kk, key, size);
		say_debug("BitmapIndex: make_bitmap_key (u64) %lu", kk);
	}
#endif /* DEBUG */
}

/* wraps bitmap_iterator insed tarantool's iterator */
struct iterator_wrapper {
	struct iterator base; /* Must be the first member. */
	struct bitmap_iterator *bitmap_it;
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
	size_t value = bitmap_iterator_next(it->bitmap_it);

	if (value == SIZE_MAX) {
#ifdef DEBUG
	say_debug("BitmapIndex: iter not found\n");
#endif
		return NULL;
	} else {
		struct tuple *tuple = value_to_tuple(value);
#ifdef DEBUG
	say_debug("BitmapIndex: iter found value = %zu (%p)\n", value, tuple);
#endif
		return tuple;
	}
}

void
iterator_wrapper_free(struct iterator *iterator)
{
	say_debug("iterator_wrapper_free");

	assert(iterator->next == iterator_wrapper_next);
	struct iterator_wrapper *it = iterator_wrapper(iterator);

	bitmap_iterator_free(&(it->bitmap_it));
	free(it);
}


@implementation BitmapIndex;

+ (struct index_traits *) traits
{
	return &bitmap_index_traits;
}

- (id) init: (struct key_def *) key_def_arg :(struct space *) space_arg
{
	say_info("BitmapIndex: init");

	self = [super init: key_def_arg :space_arg];
	if (self) {
		/* get optimal initial size of the bitmap */
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

		if (bitmap_index_new(&index, size) < 0) {
			tnt_raise(SystemError, :"bitmap_index_new: %s",
				strerror(errno));
		}
	}
	return self;
}

- (void) free
{
	bitmap_index_free(&index);
	[super free];
}

- (void) beginBuild
{
#ifdef DEBUG
	say_debug("BitmapIndex beginBuild");
#endif // DEBUG
}

- (void) buildNext: (struct tuple *)tuple
{
	say_debug("BitmapIndex beginNext");
	[self replace: NULL :tuple];
}

- (void) endBuild
{
#ifdef DEBUG
	say_debug("BitmapIndex endBuild");
#endif // DEBUG
}

- (void) build: (Index *) pk
{
#ifdef DEBUG
	say_debug("BitmapIndex beginBuild from pk");
#endif // DEBUG

	struct iterator *it = pk->position;
	struct tuple *tuple;
	[pk initIterator: it :ITER_FORWARD];

	while ((tuple = it->next(it)))
		[self replace: NULL :tuple];

#ifdef DEBUG
	say_debug("BitmapIndex endBuild from pk");
#endif // DEBUG
}

- (size_t) size
{
	return bitmap_index_size(index);
}

- (struct tuple *) min
{
	tnt_raise(ClientError, :ER_UNSUPPORTED, "BitmapIndex", "min()");
	return NULL;
}

- (struct tuple *) max
{
	tnt_raise(ClientError, :ER_UNSUPPORTED, "BitmapIndex", "max()");
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
		/* TODO(roman): ?? next vs next_equal */
		it->base.next = iterator_wrapper_next;
		it->base.next_equal = iterator_wrapper_next;
		it->base.free = iterator_wrapper_free;
	}
	return (struct iterator *) it;
}

- (struct tuple *) findUnsafe: (void *) key :(int) part_count
{
	(void) part_count;

	void *bitmap_key = NULL;
	size_t bitmap_key_size = 0;
	make_bitmap_key(key, &bitmap_key, &bitmap_key_size);

	struct bitmap_iterator *it;
	if (bitmap_index_iterate(index, &it, bitmap_key, bitmap_key_size,
				 BITMAP_INDEX_MATCH_EQUALS) < 0) {
		tnt_raise(SystemError, :"bitmap_index_iterate: %s",
			strerror(errno));
	}

	size_t value = bitmap_iterator_next(it);
	bitmap_iterator_free(&it);

	if (value == SIZE_MAX) {
#ifdef DEBUG
	say_debug("BitmapIndex: not found\n");
#endif
		return NULL;
	} else {
		struct tuple *tuple = value_to_tuple(value);
#ifdef DEBUG
	say_debug("BitmapIndex: found value = %zu (%p)\n", value, tuple);
#endif
		return tuple;
	}
}

- (void) replace: (struct tuple *) old_tuple
	:(struct tuple *) new_tuple
{
	void *bitmap_key;
	size_t bitmap_key_size;
	size_t value;

	if (old_tuple != NULL) {
		make_bitmap_key(old_tuple->data, &bitmap_key, &bitmap_key_size);
		value = tuple_to_value(old_tuple);
#ifdef DEBUG
		say_debug("BitmapIndex: remove value = %zu (%p)",
			  value, old_tuple);
#endif
		if (bitmap_index_remove(index,bitmap_key, bitmap_key_size,
					value) < 0) {
			tnt_raise(SystemError, :"bitmap_index_remove: %s",
				strerror(errno));
		}
	}

	if (new_tuple != NULL) {
		make_bitmap_key(new_tuple->data, &bitmap_key, &bitmap_key_size);
		value = tuple_to_value(new_tuple);
		make_bitmap_key(new_tuple->data, &bitmap_key, &bitmap_key_size);

#ifdef DEBUG
		say_debug("BitmapIndex: insert value = %zu (%p)",
			  value, new_tuple);
#endif
		if (bitmap_index_insert(index,bitmap_key, bitmap_key_size,
					value) < 0) {
			tnt_raise(SystemError, :"bitmap_index_insert: %s",
				strerror(errno));
		}
	}
}

- (void) remove: (struct tuple *) tuple
{
	return [self replace :tuple :NULL];
}

- (void) initIterator: (struct iterator *) iterator :(enum iterator_type) type
{
	(void) iterator;
	(void) type;
	tnt_raise(ClientError, :ER_UNSUPPORTED,
		  "BitmapIndex", "initIterator()");
}

- (void) initIteratorUnsafe: (struct iterator *) iterator :(enum iterator_type) type
			:(void *) key :(int) part_count
{
	(void) part_count;

	if (type == ITER_REVERSE) {
		tnt_raise(ClientError, :ER_UNSUPPORTED, "BitmapIndex",
			  "reverse iteration");
	}

	assert(iterator->next == iterator_wrapper_next);
	struct iterator_wrapper *it = iterator_wrapper(iterator);

	void *bitmap_key = NULL;
	size_t bitmap_key_size = 0;
	make_bitmap_key(key, &bitmap_key, &bitmap_key_size);

	if (bitmap_index_iterate(index, &(it->bitmap_it),
				 bitmap_key, bitmap_key_size,
				 BITMAP_INDEX_MATCH_EQUALS) < 0) {
		tnt_raise(SystemError, :"bitmap_index_iterate: %s",
			strerror(errno));
	}
}
@end

/* }}} */

