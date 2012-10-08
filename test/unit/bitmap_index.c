#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>

#include "mod/box/bitmap/bitmap.h"
#include "mod/box/bitmap/index.h"
#include "unit.h"

static
void test_resize()
{
	header();

	struct bitmap_index *index;
	bitmap_index_new(&index, 0);

	size_t key = 23411111;
	size_t value = (size_t) index; // mostly random number

	bitmap_index_insert(index, &key, sizeof(key), value);

	struct bitmap_iterator *it;

	bitmap_index_iterate(index, &it, &key, sizeof(key));

	fail_unless(bitmap_iterator_next(it) == value);
	fail_unless(bitmap_iterator_next(it) == SIZE_MAX);

	bitmap_iterator_free(&it);

	bitmap_index_free(&index);

	footer();
}

static
void test_size()
{
	size_t key = 656906;
	header();

	struct bitmap_index *index;
	bitmap_index_new(&index, sizeof(size_t) * CHAR_BIT);

	fail_unless(bitmap_index_size(index) == 0);

	const size_t SET_SIZE = 1 << 10;

	for(size_t i = 0; i < SET_SIZE; i++) {
		bitmap_index_insert(index, &key, sizeof(key), i);
	}

	fail_unless(bitmap_index_size(index) == SET_SIZE);

	bitmap_index_free(&index);

	footer();
}

static
void check_keys(struct bitmap_index *index,
		size_t *keys, size_t *values, size_t size)
{
	printf("Checking keys... ");
	for (size_t i = 0; i < size; i++) {
		/* ignore removed keys */
		if (keys[i] == SIZE_MAX) {
			continue;
		}

		struct bitmap_iterator *it;
		bitmap_index_iterate(index, &it, &(keys[i]), sizeof(keys[i]));

		size_t pos;

		bool pair_found = false;
		while ( (pos = bitmap_iterator_next(it)) != SIZE_MAX) {
			if (pos == values[i]) {
				pair_found = true;
				break;
			}
		}
		fail_unless(pair_found);

		bitmap_iterator_free(&it);
	}
	printf("ok\n");

}

static
void test_random_pairs()
{
	header();

	struct bitmap_index *index;
	bitmap_index_new(&index, sizeof(size_t) * CHAR_BIT);

	const size_t SET_SIZE = (size_t) 1 << 12;
	size_t *keys = malloc(SET_SIZE * sizeof(size_t));
	size_t *values = malloc(SET_SIZE * sizeof(size_t));

	printf("Generating test set... ");
	for(size_t i = 0; i < SET_SIZE; i++) {
		keys[i] = rand();
		values[i] = rand();
	}
	printf("ok\n");

	printf("Inserting pairs... ");
	for(size_t i = 0; i < SET_SIZE; i++) {
		bitmap_index_insert(index, &(keys[i]),
			sizeof(keys[i]), values[i]);

	}
	printf("ok\n");

	check_keys(index, keys, values, SET_SIZE);

	printf("Removing random pairs... ");
	for(size_t i = 0; i < SET_SIZE; i++) {
		if (rand() % 5 == 0) {
			bitmap_index_remove(index, &(keys[i]),
					    sizeof(keys[i]), values[i]);
			keys[i] = SIZE_MAX;
		}
	}
	printf("ok\n");

	check_keys(index, keys, values, SET_SIZE);

	bitmap_index_free(&index);

	footer();
}

int main(int argc, char *argv[])
{
	setbuf(stdout, NULL);
	test_resize();
	test_size();
	test_random_pairs();
}
