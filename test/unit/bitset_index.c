#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>

#include "src/box/bitset/bitset.h"
#include "src/box/bitset/index.h"
#include "unit.h"

static
void test_resize()
{
	header();

	struct bitset_index *index = bitset_index_new();
	fail_unless(index);

	size_t key = 23411111;
	size_t value = (size_t) index; // mostly random number

	bitset_index_insert(index, &key, sizeof(key), value);

	struct bitset_iterator *it= bitset_iterator_new();
	fail_unless(it);
	struct bitset_expr *expr = bitset_expr_new();
	fail_unless(expr);

	fail_unless(bitset_index_iterate_equals(index, expr,
						&key, sizeof(key)) == 0);
	fail_unless(bitset_iterator_set_expr(it, expr) == 0);
	fail_unless(bitset_iterator_next(it) == value);
	fail_unless(bitset_iterator_next(it) == SIZE_MAX);

	bitset_iterator_delete(it);
	bitset_expr_delete(expr);

	bitset_index_delete(index);

	footer();
}

static
void test_size()
{
	size_t key = 656906;
	header();

	struct bitset_index *index = bitset_index_new2(
				sizeof(size_t) * CHAR_BIT);
	fail_unless(index);

	const size_t SET_SIZE = 1 << 10;

	for(size_t i = 0; i < SET_SIZE; i++) {
		bitset_index_insert(index, &key, sizeof(key), i);
	}

	fail_unless(bitset_index_size(index) == SET_SIZE);

	bitset_index_delete(index);

	footer();
}

static
void check_keys(struct bitset_index *index,
		size_t *keys, size_t *values, size_t size)
{
	struct bitset_expr *expr = bitset_expr_new();
	fail_if(expr == NULL);

	struct bitset_iterator *it = bitset_iterator_new();
	fail_unless(it);

	printf("Checking keys... ");
	for (size_t i = 0; i < size; i++) {
		/* ignore removed keys */
		if (keys[i] == SIZE_MAX) {
			continue;
		}

		fail_unless (bitset_index_iterate_all_set(index, expr,
				&(keys[i]), sizeof(keys[i])) == 0);

		fail_unless(bitset_iterator_set_expr(it, expr) == 0);
		size_t pos;

		bool pair_found = false;
		while ( (pos = bitset_iterator_next(it)) != SIZE_MAX) {
			if (pos == values[i]) {
				pair_found = true;
				break;
			}
		}
		fail_unless(pair_found);
	}
	printf("ok\n");

	bitset_iterator_delete(it);
	bitset_expr_delete(expr);
}

static
void test_all_set()
{
	header();

	struct bitset_index *index = bitset_index_new2(
				sizeof(size_t) * CHAR_BIT);
	fail_unless(index);

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
		bitset_index_insert(index, &(keys[i]),
			sizeof(keys[i]), values[i]);

	}
	printf("ok\n");

	check_keys(index, keys, values, SET_SIZE);

	printf("Removing random pairs... ");
	for(size_t i = 0; i < SET_SIZE; i++) {
		if (rand() % 5 == 0) {
			bitset_index_remove_value(index, values[i]);
			keys[i] = SIZE_MAX;
		}
	}
	printf("ok\n");

	check_keys(index, keys, values, SET_SIZE);

	bitset_index_delete(index);

	footer();
}

static
void test_any_set() {
	header();

	struct bitset_index *index = bitset_index_new2(
				sizeof(size_t) * CHAR_BIT);
	fail_unless(index);

	const size_t NUM_SIZE = (1 << 10);
	for (size_t key = 0; key < NUM_SIZE; key++) {
		bitset_index_insert(index, &key,
			sizeof(key), key);
	}

	size_t key1 = 66; /* 0b1000010 */

	struct bitset_expr *expr = bitset_expr_new();
	fail_if(expr == NULL);
	bitset_index_iterate_any_set(index, expr, &key1, sizeof(key1));

	struct bitset_iterator *it = bitset_iterator_new();
	fail_unless(it);
	fail_unless(bitset_iterator_set_expr(it, expr) == 0);
	for (size_t key = 0; key < NUM_SIZE; key++) {
		if ((key & key1) == 0) {
			continue;
		}

		fail_unless(bitset_iterator_next(it) == key);
	}
	fail_unless(bitset_iterator_next(it) == SIZE_MAX);

	bitset_iterator_delete(it);
	bitset_expr_delete(expr);

	bitset_index_delete(index);
	footer();
}

static
void test_equals() {
	header();

	struct bitset_index *index = bitset_index_new2(
				sizeof(size_t) * CHAR_BIT);
	fail_unless(index);

	size_t mask = ~((size_t ) 7);
	const size_t NUM_SIZE = (1 << 17);
	for (size_t i = 0; i < NUM_SIZE; i++) {
		size_t key = i & mask;
		size_t value = i;
		bitset_index_insert(index, &key,
			sizeof(key), value);
	}

	printf("Insert done\n");

	size_t key1 = (rand() % NUM_SIZE) & mask;
	struct bitset_expr *expr = bitset_expr_new();
	fail_if(expr == NULL);
	struct bitset_iterator *it = bitset_iterator_new();
	fail_unless(it);
	fail_unless(bitset_index_iterate_equals(index, expr,
			&key1, sizeof(key1)) == 0);
	fail_unless(bitset_iterator_set_expr(it, expr) == 0);

	for (size_t i = key1; i <= (key1 + ~mask); i++) {
		fail_unless(i == bitset_iterator_next(it));
	}
	fail_unless(bitset_iterator_next(it) == SIZE_MAX);

	printf("Check done\n");

	bitset_index_delete(index);
	footer();

}

int main(int argc, char *argv[])
{
	setbuf(stdout, NULL);

	test_resize();
	test_size();

	test_all_set();
	test_any_set();
	test_equals();
}
