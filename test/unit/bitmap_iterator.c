#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#include "src/box/bitmap/bitmap.h"
#include "src/box/bitmap/index.h"
#include "unit.h"

static const size_t NUM_SIZE = 1<<13;

static
int size_compator(const void *a, const void *b)
{
	size_t *aa = (size_t *) a;
	size_t *bb = (size_t *) b;

	if (*aa < *bb) {
		return -1;
	} else if (*aa > *bb) {
		return 1;
	} else {
		return 0;
	}
}

static
void test_empty()
{
	header();

	struct bitmap *bm1;
	bitmap_new(&bm1);

	bitmap_set(bm1, 1, 1);
	bitmap_set(bm1, 2, 1);
	bitmap_set(bm1, 3, 1);
	bitmap_set(bm1, 193, 1);
	bitmap_set(bm1, 1024, 1);

	bitmap_set(bm1, 1025, 1);
	bitmap_set(bm1, 16384, 1);
	bitmap_set(bm1, 16385, 1);

	struct bitmap *bm2;
	bitmap_new(&bm2);
	bitmap_set(bm2, 17, 1);
	bitmap_set(bm2, 194, 1);
	bitmap_set(bm2, 1023, 1);

	struct bitmap_expr *expr = bitmap_expr_new();
	fail_if(expr == NULL);

	fail_unless(bitmap_expr_add_group(expr,
		BITMAP_OP_AND, BITMAP_OP_NULL) == 0);

	fail_unless(bitmap_expr_group_add_bitmap(expr,
		0, bm1, BITMAP_OP_NULL) == 0);
	fail_unless(bitmap_expr_group_add_bitmap(expr,
		0, bm2, BITMAP_OP_NULL) == 0);

	struct bitmap_iterator *it;
	fail_unless(bitmap_iterator_new(&it) == 0);
	fail_unless(bitmap_iterator_set_expr(it, expr) == 0);

	size_t pos = bitmap_iterator_next(it);
	fail_unless(pos == SIZE_MAX);

	bitmap_iterator_free(&it);
	bitmap_expr_free(expr);

	bitmap_free(&bm1);
	bitmap_free(&bm2);

	footer();
}

static
void test_first()
{
	header();

	struct bitmap *bm1;
	bitmap_new(&bm1);
	bitmap_set(bm1, 0, 1);
	bitmap_set(bm1, 1023, 1);

	struct bitmap *bm2;
	bitmap_new(&bm2);
	bitmap_set(bm2, 0, 1);
	bitmap_set(bm2, 1025, 1);

	struct bitmap_expr *expr = bitmap_expr_new();
	fail_if(expr == NULL);
	fail_unless(bitmap_expr_add_group(expr,
		BITMAP_OP_AND, BITMAP_OP_NULL) == 0);

	fail_unless(bitmap_expr_group_add_bitmap(expr,
		0, bm1, BITMAP_OP_NULL) == 0);
	fail_unless(bitmap_expr_group_add_bitmap(expr,
		0, bm2, BITMAP_OP_NULL) == 0);

	struct bitmap_iterator *it;
	fail_unless(bitmap_iterator_new(&it) == 0);
	fail_unless(bitmap_iterator_set_expr(it, expr) == 0);

	size_t pos = bitmap_iterator_next(it);

	fail_unless(pos == 0);
	fail_unless(bitmap_iterator_next(it) == SIZE_MAX);

	bitmap_iterator_free(&it);
	bitmap_expr_free(expr);

	bitmap_free(&bm1);
	bitmap_free(&bm2);

	footer();
}

static
void test_and() {
	header();

	struct bitmap *bm1;
	fail_if (bitmap_new(&bm1) < 0);

	struct bitmap *bm2;
	fail_if (bitmap_new(&bm2) < 0);

	size_t *nums = malloc(sizeof(*nums) * NUM_SIZE);
	fail_if(nums == NULL);

	for (size_t i = 0; i < NUM_SIZE; i++) {
		nums[i] = rand();
	}

	for (size_t i = 0; i < NUM_SIZE; i++) {
		size_t a = ((size_t) rand()) << 1;
		size_t b = a + 1;

		bitmap_set(bm1, a, 1);
		bitmap_set(bm2, b, 1);
	}

	for (size_t i = 0; i < NUM_SIZE; i++) {
		bitmap_set(bm1, nums[i], 1);
		bitmap_set(bm2, nums[i], 1);
	}

	struct bitmap_expr *expr = bitmap_expr_new();
	fail_if(expr == NULL);
	fail_unless(bitmap_expr_add_group(expr,
		BITMAP_OP_AND, BITMAP_OP_NULL) == 0);

	fail_unless(bitmap_expr_group_add_bitmap(expr,
		0, bm1, BITMAP_OP_NULL) == 0);
	fail_unless(bitmap_expr_group_add_bitmap(expr,
		0, bm2, BITMAP_OP_NULL) == 0);

	qsort(nums, NUM_SIZE, sizeof(size_t), size_compator);

	struct bitmap_iterator *it;
	fail_unless(bitmap_iterator_new(&it) == 0);
	fail_unless(bitmap_iterator_set_expr(it, expr) == 0);

	for (size_t i = 0; i < NUM_SIZE; i++) {
		size_t pos = bitmap_iterator_next(it);
		fail_unless(pos == nums[i]);
	}

	size_t pos = bitmap_iterator_next(it);
	fail_unless(pos == SIZE_MAX);

	free(nums);

	bitmap_iterator_free(&it);
	bitmap_expr_free(expr);

	bitmap_free(&bm1);
	bitmap_free(&bm2);

	footer();
}

static
void test_and1() {
	header();

	const size_t BITMAP_SIZE = 32;

	struct bitmap **bitmaps = calloc(BITMAP_SIZE, sizeof(*bitmaps));
	fail_if(bitmaps == NULL);

	struct bitmap_expr *expr = bitmap_expr_new();
	fail_if(expr == NULL);
	fail_unless(bitmap_expr_add_group(expr,
		BITMAP_OP_AND, BITMAP_OP_NULL) == 0);

	size_t **pnumbers = malloc(sizeof(*pnumbers) * BITMAP_SIZE);

	for(size_t b = 0; b < BITMAP_SIZE; b++) {
		struct bitmap *bitmap;
		bitmap_new(&bitmap);
		fail_unless(bitmap_expr_group_add_bitmap(expr,
			0, bitmap, BITMAP_OP_NULL) == 0);
		bitmaps[b] = bitmap;
	}

	printf("Generating test set... ");
	for(size_t b = 0; b < BITMAP_SIZE; b++) {
		pnumbers[b] = malloc(sizeof(**pnumbers) * NUM_SIZE);
		for (size_t n = 0; n < NUM_SIZE; n++) {
			size_t pos = (size_t) rand();
			pnumbers[b][n] = pos;
		}
	}
	printf("ok\n");

	printf("Setting bits... ");
	for(size_t b = 0; b < BITMAP_SIZE; b++) {
		for (size_t n = 0; n < NUM_SIZE; n++) {
			bitmap_set(bitmaps[b],
				   pnumbers[b][n], 1);
		}
	}
	printf("ok\n");

	printf("Iterating... ");
	struct bitmap_iterator *it;
	fail_unless(bitmap_iterator_new(&it) == 0);
	fail_unless(bitmap_iterator_set_expr(it, expr) == 0);

	size_t pos;
	while ((pos = bitmap_iterator_next(it)) != SIZE_MAX) {
		for(size_t b = 0; b < BITMAP_SIZE; b++) {
			struct bitmap *bitmap = bitmaps[b];
			bool bit_is_set = bitmap_get(bitmap, pos);

			if (bit_is_set) {
				continue;
			}

			printf("Failed: bitmap %zu pos = %zu\n", b, pos);
			printf("Test set:\n");
			for(size_t bb = 0; bb < BITMAP_SIZE; bb++) {
				printf("%zu: ", bb);
				for (size_t nn = 0; nn < NUM_SIZE; nn++) {
					printf("%zu, ", pnumbers[bb][nn]);
				}
				printf("\n");
			}
			printf("\n");

			// always fail here
			fail_unless(bit_is_set);
			return;
		}
	}
	printf("ok\n");

	bitmap_iterator_free(&it);
	bitmap_expr_free(expr);

	for(size_t b = 0; b < BITMAP_SIZE; b++) {
		bitmap_free(&(bitmaps[b]));
	}

	for(size_t b = 0; b < BITMAP_SIZE; b++) {
		free(pnumbers[b]);
	}

	free(pnumbers);

	footer();
}

static
void test_or() {
	header();

	struct bitmap *bm1;
	fail_if (bitmap_new(&bm1) < 0);

	struct bitmap *bm2;
	fail_if (bitmap_new(&bm2) < 0);

	size_t *nums = malloc(sizeof(*nums) * NUM_SIZE);
	fail_if(nums == NULL);

	for (size_t i = 0; i < NUM_SIZE; i++) {
		nums[i] = rand();
	}


	for (size_t i = 0; i < NUM_SIZE; i++) {
		if (nums[i] & 1) {
			bitmap_set(bm1, nums[i], 1);
		} else {
			bitmap_set(bm2, nums[i], 1);
		}
	}

	struct bitmap_expr *expr = bitmap_expr_new();
	fail_if(expr == NULL);
	fail_unless(bitmap_expr_add_group(expr,
		BITMAP_OP_OR, BITMAP_OP_NULL) == 0);

	fail_unless(bitmap_expr_group_add_bitmap(expr,
		0, bm1, BITMAP_OP_NULL) == 0);
	fail_unless(bitmap_expr_group_add_bitmap(expr,
		0, bm2, BITMAP_OP_NULL) == 0);

	struct bitmap_iterator *it;
	fail_unless(bitmap_iterator_new(&it) == 0);
	fail_unless(bitmap_iterator_set_expr(it, expr) == 0);

	qsort(nums, NUM_SIZE, sizeof(size_t), size_compator);

	size_t prev_num = SIZE_MAX;
	for (size_t i = 0; i < NUM_SIZE; i++) {
		if (nums[i] == prev_num) {
			continue;
		}
		prev_num = nums[i];

		size_t pos = bitmap_iterator_next(it);
		fail_unless(pos == nums[i]);
	}

	size_t pos = bitmap_iterator_next(it);
	fail_unless(pos == SIZE_MAX);

	free(nums);

	bitmap_iterator_free(&it);
	bitmap_expr_free(expr);

	bitmap_free(&bm1);
	bitmap_free(&bm2);

	footer();
}

static
void test_xor() {
	header();

	struct bitmap *bm1;
	fail_if (bitmap_new(&bm1) < 0);

	struct bitmap *bm2;
	fail_if (bitmap_new(&bm2) < 0);

	size_t *nums = malloc(sizeof(*nums) * NUM_SIZE);
	fail_if(nums == NULL);

	for (size_t i = 0; i < NUM_SIZE; i++) {
		nums[i] = rand();
	}

	for (size_t i = 0; i < NUM_SIZE; i++) {
		if (nums[i] & 1) {
			bitmap_set(bm1, nums[i], 1);
		} else {
			bitmap_set(bm2, nums[i], 1);
		}

		if (nums[i] % 5 == 0) {
			bitmap_set(bm1, nums[i], 1);
			bitmap_set(bm2, nums[i], 1);
		}
	}

	struct bitmap_expr *expr = bitmap_expr_new();
	fail_if(expr == NULL);
	fail_unless(bitmap_expr_add_group(expr,
		BITMAP_OP_XOR, BITMAP_OP_NULL) == 0);

	fail_unless(bitmap_expr_group_add_bitmap(expr,
		0, bm1, BITMAP_OP_NULL) == 0);
	fail_unless(bitmap_expr_group_add_bitmap(expr,
		0, bm2, BITMAP_OP_NULL) == 0);

	struct bitmap_iterator *it;
	fail_unless(bitmap_iterator_new(&it) == 0);
	fail_unless(bitmap_iterator_set_expr(it, expr) == 0);

	qsort(nums, NUM_SIZE, sizeof(size_t), size_compator);

	for (size_t i = 0; i < NUM_SIZE; i++) {
		if (nums[i] % 5 == 0) {
			continue;
		}
		size_t pos = bitmap_iterator_next(it);
		fail_unless(pos == nums[i]);
	}

	size_t pos = bitmap_iterator_next(it);
	fail_unless(pos == SIZE_MAX);

	free(nums);

	bitmap_iterator_free(&it);
	bitmap_expr_free(expr);

	bitmap_free(&bm1);
	bitmap_free(&bm2);

	footer();
}

static
void test_intergroup() {
	header();

	struct bitmap *bm1;
	fail_if (bitmap_new(&bm1) < 0);

	struct bitmap *bm2;
	fail_if (bitmap_new(&bm2) < 0);

	size_t *nums = malloc(sizeof(*nums) * NUM_SIZE);
	fail_if(nums == NULL);

	for (size_t i = 0; i < NUM_SIZE; i++) {
		nums[i] = rand();
	}

	for (size_t i = 0; i < NUM_SIZE; i++) {
		bitmap_set(bm1, nums[i], 1);
		bitmap_set(bm2, nums[i], 1);
	}

	for (size_t i = 0; i < NUM_SIZE; i++) {
		size_t a = ((size_t) rand()) << 1;
		size_t b = a + 1;

		bitmap_set(bm1, a, 1);
		bitmap_set(bm2, b, 1);
	}

	struct bitmap_expr *expr = bitmap_expr_new();
	fail_if(expr == NULL);

	fail_unless(bitmap_expr_add_group(expr,
		BITMAP_OP_AND, BITMAP_OP_NULL) == 0);
	fail_unless(bitmap_expr_group_add_bitmap(expr,
		0, bm1, BITMAP_OP_NULL) == 0);

	fail_unless(bitmap_expr_add_group(expr,
		BITMAP_OP_AND, BITMAP_OP_NULL) == 0);
	fail_unless(bitmap_expr_group_add_bitmap(expr,
		1, bm2, BITMAP_OP_NULL) == 0);

	struct bitmap_iterator *it;
	fail_unless(bitmap_iterator_new(&it) == 0);
	fail_unless(bitmap_iterator_set_expr(it, expr) == 0);

	qsort(nums, NUM_SIZE, sizeof(size_t), size_compator);

	for (size_t i = 0; i < NUM_SIZE; i++) {
		size_t pos = bitmap_iterator_next(it);
		fail_unless(pos == nums[i]);
	}

	size_t pos = bitmap_iterator_next(it);
	fail_unless(pos == SIZE_MAX);

	free(nums);

	bitmap_iterator_free(&it);
	bitmap_expr_free(expr);

	bitmap_free(&bm1);
	bitmap_free(&bm2);

	footer();
}

static
void test_op_not1() {
	header();

	size_t big_i = (size_t) 1 << 15;
	struct bitmap *bm1;
	bitmap_new(&bm1);
	bitmap_set(bm1, 0, 1);
	bitmap_set(bm1, 11, 1);
	bitmap_set(bm1, 1024, 1);

	struct bitmap *bm2;
	bitmap_new(&bm2);
	bitmap_set(bm2, 0, 1);
	bitmap_set(bm2, 10, 1);
	bitmap_set(bm2, 11, 1);
	bitmap_set(bm2, 14, 1);
	bitmap_set(bm2, big_i, 1);

	struct bitmap_expr *expr = bitmap_expr_new();
	fail_if(expr == NULL);

	fail_unless(bitmap_expr_add_group(expr,
		BITMAP_OP_AND, BITMAP_OP_NULL) == 0);
	fail_unless(bitmap_expr_group_add_bitmap(expr,
		0, bm1, BITMAP_OP_NOT) == 0);
	fail_unless(bitmap_expr_group_add_bitmap(expr,
		0, bm2, BITMAP_OP_NULL) == 0);

	struct bitmap_iterator *it;
	fail_unless(bitmap_iterator_new(&it) == 0);
	fail_unless(bitmap_iterator_set_expr(it, expr) == 0);

	size_t result[] = {10, 14, big_i};
	size_t result_size = 3;

	size_t pos;
	for (size_t i = 0; i < result_size; i++) {
		pos = bitmap_iterator_next(it);
		fail_unless (result[i] == pos);
	}
	fail_unless (pos = bitmap_iterator_next(it) == SIZE_MAX);

	bitmap_iterator_free(&it);
	bitmap_expr_free(expr);

	bitmap_free(&bm1);
	bitmap_free(&bm2);

	footer();
}

#ifdef BITMAP_ITERATOR_POST_OP_FEATURE
static
void test_op_not2() {
	header();

	struct bitmap_iterator *it;
	bitmap_iterator_newn(&it, NULL, 0, NULL, BITMAP_OP_NOT);

	for(size_t i = 0; i < 10000; i++) {
		size_t pos = bitmap_iterator_next(it);
		fail_unless (pos == i);
	}

	bitmap_iterator_free(&it);

	footer();
}

static
void test_op_not3() {
	header();

	struct bitmap *bm1;
	bitmap_new(&bm1);
	struct bitmap *bm2;
	bitmap_new(&bm2);

	size_t result[] = {0, 15, 43243, 65536};
	size_t result_size = 4;

	for (size_t i = 0; i < result_size; i++) {
		bitmap_set(bm2, result[i], 1);
	}

	//bitmap_set(bm2, result[result_size - 1] + 1024, 1);

	struct bitmap *bitmaps[] = { bm1, bm2 };
	int flags[] = { BITMAP_OP_NOT, BITMAP_OP_NOT };
	int result_flags = BITMAP_OP_NOT;

	struct bitmap_iterator *it;
	bitmap_iterator_newn(&it, bitmaps, 2, flags, result_flags);


	size_t pos = SIZE_MAX;
	for (size_t i = 0; i < result_size; i++) {
		pos = bitmap_iterator_next(it);
		fail_unless (pos == result[i]);
	}

	pos = bitmap_iterator_next(it);
	fail_unless (pos == SIZE_MAX);

	bitmap_iterator_free(&it);

	bitmap_free(&bm1);
	bitmap_free(&bm2);

	footer();
}
#endif /* BITMAP_ITERATOR_POST_OP_FEATURE */

int main(int argc, char *argv[])
{
	setbuf(stdout, NULL);

	test_empty();
	test_first();
	test_and();
	test_and1();
	test_or();
	test_xor();
	test_intergroup();
	test_op_not1();
#ifdef BITMAP_ITERATOR_POST_OP_FEATURE
	test_op_not2();
	test_op_not3();
#endif /* BITMAP_ITERATOR_POST_OP_FEATURE */
	return 0;
}