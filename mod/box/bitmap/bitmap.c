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

#include "bitmap.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>

#include "bitmap_p.h"

/** Number of bits in one word */
const size_t BITMAP_WORD_BIT = sizeof(bitmap_word_t) * CHAR_BIT;

bool test_bit(const void *data, size_t pos)
{
	const char *cdata = (const char *) data;

	const size_t cur_pos = pos / CHAR_BIT;
	const size_t cur_offset = pos % CHAR_BIT;

	return (cdata[cur_pos] >> cur_offset) & 0x1;
}

int find_next_set_bit(const void *data, size_t data_size, size_t *ppos) {
	const char *cdata = (const char *) data;

	size_t cur_pos = *ppos / CHAR_BIT;
	size_t cur_offset = *ppos % CHAR_BIT;

	while (cur_pos < data_size) {
		char c = cdata[cur_pos] >> cur_offset;
		if (c & 0x1) {
			*ppos = cur_pos * CHAR_BIT + cur_offset;
			return 0;
		}

		cur_offset++;
		if (cur_offset >= CHAR_BIT) {
			cur_offset = 0;
			cur_pos++;
			continue;
		}
	}

	*ppos = SIZE_MAX;
	return -1;
}

/* bitmap_set/bitmap_get helpers */
static void bitmap_set_in_page(struct bitmap_page *page, size_t pos, bool val);
static bool bitmap_get_from_page(struct bitmap_page *page, size_t pos);

int bitmap_new(struct bitmap **pbitmap)
{
	*pbitmap = calloc(1, sizeof(struct bitmap));
	if (*pbitmap == NULL) {
		return -1;
	}

	return 0;
}

void bitmap_free(struct bitmap **pbitmap)
{
	struct bitmap *bitmap = *pbitmap;
	if (bitmap != NULL) {
		/* cleanup pages */
		struct slist_node *node = bitmap->pages.next;
		while (node != NULL) {
			struct slist_node *next = node->next;
			struct bitmap_page *page = slist_member_of(node,
						struct bitmap_page, node);
			free(page);
			node = next;
		}
		free(bitmap);
	}
	*pbitmap = NULL;
}


bool bitmap_get(struct bitmap *bitmap, size_t pos)
{
	struct slist_node *node = &(bitmap->pages);
	for (; node->next != NULL; node = node->next) {
		struct bitmap_page *page = slist_member_of(node->next,
						struct bitmap_page, node);

		size_t page_first_pos = page->first_pos;
		size_t page_last_pos = page->first_pos +
				BITMAP_WORDS_PER_PAGE * BITMAP_WORD_BIT;

		/*
		printf("Iter pos=%zu: [%zu, %zu)\n",
		       pos, page_first_pos, page_last_pos);
		*/

		if (pos < page_first_pos) {
			break;
		}

		if (pos >= page_last_pos) {
			continue;
		}

		return bitmap_get_from_page(page, pos - page->first_pos);
	}

	return false;
}

static
bool bitmap_get_from_page(struct bitmap_page *page, size_t pos)
{
	size_t w = pos / BITMAP_WORD_BIT;
	size_t offset = pos % BITMAP_WORD_BIT;
	return (page->words[w] & ((bitmap_word_t) 1 << offset)) != 0;
}

static
int bitmap_add_page(struct slist_node *node, size_t first_pos)
{
	/* resize bitmap */
	struct bitmap_page *page = calloc(1, sizeof(struct bitmap_page));
	if (page == NULL) {
		return -1;
	}

	page->first_pos = first_pos;

	page->node.next = node->next;
	node->next = &(page->node);

	return 0;
}

int bitmap_set(struct bitmap *bitmap, size_t pos, bool val)
{
	struct slist_node *node = &(bitmap->pages);
	size_t page_first_pos = 0;
	size_t page_last_pos = 0;

	for(; node->next != NULL; node = node->next) {
		struct bitmap_page *page = slist_member_of(node->next,
						struct bitmap_page, node);

		page_first_pos = page->first_pos;
		page_last_pos = page_first_pos +
				BITMAP_WORDS_PER_PAGE * BITMAP_WORD_BIT;

		if (pos < page_first_pos) {
			break;
		}

		if (pos < page_last_pos) {
			break;
		}
	}

	if (node->next == NULL || pos < page_first_pos) {
		size_t first_pos = pos -
				(pos % (BITMAP_WORDS_PER_PAGE * BITMAP_WORD_BIT));
		if (bitmap_add_page(node, first_pos) < 0) {
			return -1;
		}
	}

	struct bitmap_page *page = slist_member_of(node->next,
					struct bitmap_page, node);
	if (bitmap_get_from_page(page, pos - page->first_pos) != val) {
		bitmap_set_in_page(page, pos - page->first_pos, val);

		/* update size counter of the bitmap */
		if (val) {
			bitmap->cardinality++;
		} else {
			bitmap->cardinality--;
		}
	}

	return 0;
}

static
void bitmap_set_in_page(struct bitmap_page *page, size_t pos, bool val)
{
	size_t w = pos / BITMAP_WORD_BIT;
	size_t offset = pos % BITMAP_WORD_BIT;
	if (val) {
		page->words[w] |= ((bitmap_word_t) 1 << offset);
	} else {
		page->words[w] ^= ((bitmap_word_t) 1 << offset);
	}
}

size_t bitmap_cardinality(struct bitmap *bitmap) {
	return bitmap->cardinality;
}


#ifdef DEBUG
void bitmap_debug_print(struct bitmap *bitmap) {
	printf("Bitmap {\n");
	struct slist_node *node = &(bitmap->pages);
	for(; node->next != NULL; node = node->next) {
		struct bitmap_page *page = slist_member_of(node->next,
						struct bitmap_page, node);

		size_t page_first_pos = page->first_pos;
		size_t page_last_pos = page_first_pos +
				BITMAP_WORDS_PER_PAGE * BITMAP_WORD_BIT;

		printf("    [%zu, %zu) ", page_first_pos, page_last_pos);

		size_t size = BITMAP_WORDS_PER_PAGE * sizeof(bitmap_word_t);
		size_t pos = 0;
		while (find_next_set_bit(page->words, size, &pos) != 0) {
			printf("%zu ", page_first_pos + pos);
			pos++;
		}

		printf("\n");
	}

	printf("}\n");
}
#endif /* def DEBUG */
/* }}} */
