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
#include "salloc.h"

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#include "third_party/valgrind/memcheck.h"
#include <third_party/queue.h>
#include <util.h>
#include <tbuf.h>
#include <say.h>
#include "exception.h"

#define SLAB_ALIGN_PTR(ptr) (void *)((uintptr_t)(ptr) & ~(SLAB_SIZE - 1))

#ifdef SLAB_DEBUG
#undef NDEBUG
u8 red_zone[4] = { 0xfa, 0xfa, 0xfa, 0xfa };
#else
u8 red_zone[0] = { };
#endif

const u32 SLAB_MAGIC = 0x51abface;
const size_t SLAB_SIZE = 1 << 22;
const size_t MAX_SLAB_ITEM = 1 << 20;

/* maximum number of items in one slab */
/* updated in slab_classes_init, depends on salloc_init params */
size_t MAX_SLAB_ITEM_COUNT;

struct slab_item {
	struct slab_item *next;
} __packed__;

struct slab {
	u32 magic;
	size_t used;
	size_t items;
	struct slab_item *free;
	struct slab_class *class;
	void *brk;
	 SLIST_ENTRY(slab) link;
	 SLIST_ENTRY(slab) free_link;
	 TAILQ_ENTRY(slab) class_free_link;
	 TAILQ_ENTRY(slab) class_link;
};

SLIST_HEAD(slab_slist_head, slab);
TAILQ_HEAD(slab_tailq_head, slab);

struct slab_class {
	size_t item_size;
	struct slab_tailq_head slabs, free_slabs;
};

struct arena {
	void *mmap_base;
	size_t mmap_size;

	void *base;
	size_t size;
	size_t used;
};

size_t slab_active_classes;
struct slab_class slab_classes[256];
struct arena arena;

struct slab_slist_head slabs, free_slabs;

static struct slab *
slab_header(void *ptr)
{
	struct slab *slab = (struct slab *)SLAB_ALIGN_PTR(ptr);
	assert(slab->magic == SLAB_MAGIC);
	return slab;
}

static void
slab_classes_init(size_t minimal, double factor)
{
	int i, size;
	const size_t ptr_size = sizeof(void *);

	for (i = 0, size = minimal; i < nelem(slab_classes) && size <= MAX_SLAB_ITEM; i++) {
		slab_classes[i].item_size = size - sizeof(red_zone);
		TAILQ_INIT(&slab_classes[i].free_slabs);

		size = MAX((size_t)(size * factor) & ~(ptr_size - 1),
			   (size + ptr_size) & ~(ptr_size - 1));
	}

	SLIST_INIT(&slabs);
	SLIST_INIT(&free_slabs);
	slab_active_classes = i;

	MAX_SLAB_ITEM_COUNT = (size_t) (SLAB_SIZE - sizeof(struct slab)) /
			slab_classes[0].item_size;
}

static bool
arena_init(struct arena *arena, size_t size)
{
	arena->used = 0;
	arena->size = size - size % SLAB_SIZE;
	arena->mmap_size = size - size % SLAB_SIZE + SLAB_SIZE;	/* spend SLAB_SIZE bytes on align :-( */

	arena->mmap_base = mmap(NULL, arena->mmap_size,
				PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (arena->mmap_base == MAP_FAILED) {
		say_syserror("mmap");
		return false;
	}

	arena->base = (char *)SLAB_ALIGN_PTR(arena->mmap_base) + SLAB_SIZE;
	return true;
}

static void *
arena_alloc(struct arena *arena)
{
	void *ptr;
	const size_t size = SLAB_SIZE;

	if (arena->size - arena->used < size)
		return NULL;

	ptr = (char *)arena->base + arena->used;
	arena->used += size;

	return ptr;
}

bool
salloc_init(size_t size, size_t minimal, double factor)
{
	if (size < SLAB_SIZE * 2)
		return false;

	if (!arena_init(&arena, size))
		return false;

	slab_classes_init(MAX(sizeof(void *), minimal), factor);
	return true;
}

void
salloc_destroy(void)
{
	if (arena.mmap_base != NULL)
		munmap(arena.mmap_base, arena.mmap_size);

	memset(&arena, 0, sizeof(struct arena));
}

static void
format_slab(struct slab_class *class, struct slab *slab)
{
	assert(class->item_size <= MAX_SLAB_ITEM);

	slab->magic = SLAB_MAGIC;
	slab->free = NULL;
	slab->class = class;
	slab->items = 0;
	slab->used = 0;
	slab->brk = (void *)CACHEALIGN((void *)slab + sizeof(struct slab));

	TAILQ_INSERT_HEAD(&class->slabs, slab, class_link);
	TAILQ_INSERT_HEAD(&class->free_slabs, slab, class_free_link);
}

static bool
fully_formatted(struct slab *slab)
{
	return slab->brk + slab->class->item_size >= (void *)slab + SLAB_SIZE;
}

void
slab_validate(void)
{
	struct slab *slab;

	SLIST_FOREACH(slab, &slabs, link) {
		for (char *p = (char *)slab + sizeof(struct slab);
		     p + slab->class->item_size < (char *)slab + SLAB_SIZE;
		     p += slab->class->item_size + sizeof(red_zone)) {
			assert(memcmp(p + slab->class->item_size, red_zone, sizeof(red_zone)) == 0);
		}
	}
}

static struct slab_class *
class_for(size_t size)
{
	for (int i = 0; i < slab_active_classes; i++)
		if (slab_classes[i].item_size >= size)
			return &slab_classes[i];

	return NULL;
}

static struct slab *
slab_of(struct slab_class *class)
{
	struct slab *slab;

	if (!TAILQ_EMPTY(&class->free_slabs)) {
		slab = TAILQ_FIRST(&class->free_slabs);
		assert(slab->magic == SLAB_MAGIC);
		return slab;
	}

	if (!SLIST_EMPTY(&free_slabs)) {
		slab = SLIST_FIRST(&free_slabs);
		assert(slab->magic == SLAB_MAGIC);
		SLIST_REMOVE_HEAD(&free_slabs, free_link);
		format_slab(class, slab);
		return slab;
	}

	if ((slab = arena_alloc(&arena)) != NULL) {
		format_slab(class, slab);
		SLIST_INSERT_HEAD(&slabs, slab, link);
		return slab;
	}

	return NULL;
}

#ifndef NDEBUG
static bool
valid_item(struct slab *slab, void *item)
{
	return (void *)item >= (void *)(slab) + sizeof(struct slab) &&
	    (void *)item < (void *)(slab) + sizeof(struct slab) + SLAB_SIZE;
}
#endif

void *
salloc(size_t size, const char *what)
{
	struct slab_class *class;
	struct slab *slab;
	struct slab_item *item;

	if ((class = class_for(size)) == NULL ||
	    (slab = slab_of(class)) == NULL) {

		tnt_raise(LoggedError, :ER_MEMORY_ISSUE, size,
			  "slab allocator", what);
	}

	if (slab->free == NULL) {
		assert(valid_item(slab, slab->brk));
		item = slab->brk;
		memcpy((void *)item + class->item_size, red_zone, sizeof(red_zone));
		slab->brk += class->item_size + sizeof(red_zone);
	} else {
		assert(valid_item(slab, slab->free));
		item = slab->free;

		(void) VALGRIND_MAKE_MEM_DEFINED(item, sizeof(void *));
		slab->free = item->next;
		(void) VALGRIND_MAKE_MEM_UNDEFINED(item, sizeof(void *));
	}

	if (fully_formatted(slab) && slab->free == NULL)
		TAILQ_REMOVE(&class->free_slabs, slab, class_free_link);

	slab->used += class->item_size + sizeof(red_zone);
	slab->items += 1;

	VALGRIND_MALLOCLIKE_BLOCK(item, class->item_size, sizeof(red_zone), 0);
	return (void *)item;
}

void
sfree(void *ptr)
{
	if (ptr == NULL)
		return;
	struct slab *slab = slab_header(ptr);
	struct slab_class *class = slab->class;
	struct slab_item *item = ptr;

	if (fully_formatted(slab) && slab->free == NULL)
		TAILQ_INSERT_TAIL(&class->free_slabs, slab, class_free_link);

	assert(valid_item(slab, item));
	assert(slab->free == NULL || valid_item(slab, slab->free));

	item->next = slab->free;
	slab->free = item;
	slab->used -= class->item_size + sizeof(red_zone);
	slab->items -= 1;

	if (slab->items == 0) {
		TAILQ_REMOVE(&class->free_slabs, slab, class_free_link);
		TAILQ_REMOVE(&class->slabs, slab, class_link);
		SLIST_INSERT_HEAD(&free_slabs, slab, free_link);
	}

	VALGRIND_FREELIKE_BLOCK(item, sizeof(red_zone));
}


size_t
salloc_ptr_to_index(void *ptr)
{
	struct slab *slab = slab_header(ptr);
	struct slab_item *item = ptr;
	struct slab_class *clazz = slab->class;

	(void) item;
	assert(valid_item(slab, item));

	void *brk_start = (void *)CACHEALIGN((void *)slab+sizeof(struct slab));
	ptrdiff_t item_no = (ptr - brk_start) / clazz->item_size;
	assert(item_no >= 0);

	ptrdiff_t slab_no = ((void *) slab - (void *) arena.base) / SLAB_SIZE;
	assert(slab_no >= 0);

	size_t index = (size_t)slab_no * MAX_SLAB_ITEM_COUNT + (size_t) item_no;

	return index;
}

void *
salloc_ptr_from_index(size_t index)
{
	size_t slab_no = index / MAX_SLAB_ITEM_COUNT;
	size_t item_no = index % MAX_SLAB_ITEM_COUNT;

	struct slab *slab = slab_header(
		(void *) ((size_t) arena.base + SLAB_SIZE * slab_no));
	struct slab_class *clazz = slab->class;

	void *brk_start = (void *)CACHEALIGN((void *)slab+sizeof(struct slab));
	struct slab_item *item = brk_start + item_no * clazz->item_size;
	assert(valid_item(slab, item));

	return (void *) item;
}

/**
 * Collect slab allocator statistics.
 *
 * @param cb - a callback to receive statistic item
 * @param astat - a structure to fill with of arena
 * @user_data - user's data that will be sent to cb
 *
 */
int
salloc_stat(salloc_stat_cb cb, struct slab_arena_stats *astat, void *cb_ctx)
{
	if (astat) {
		astat->used = arena.used;
		astat->size = arena.size;
	}

	if (cb) {
		struct slab *slab;
		struct slab_class_stats st;

		for (int i = 0; i < slab_active_classes; i++) {
			memset(&st, 0, sizeof(st));
			TAILQ_FOREACH(slab, &slab_classes[i].slabs, class_link)
			{
				st.slabs++;
				st.items += slab->items;
				st.bytes_free += SLAB_SIZE;
				st.bytes_free -= slab->used;
				st.bytes_free -= sizeof(struct slab);
				st.bytes_used += sizeof(struct slab);
				st.bytes_used += slab->used;
			}
			st.item_size = slab_classes[i].item_size;

			if (st.slabs == 0)
				continue;
			int res = cb(&st, cb_ctx);
			if (res != 0)
				return res;
		}
	}
	return 0;
}
