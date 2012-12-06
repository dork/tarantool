#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#include "src/box/bitset/bitset.h"
#include "src/box/bitset/index.h"
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

	struct bitset *bm1;
	bitset_new(&bm1);

	bitset_set(bm1, 1, 1);
	bitset_set(bm1, 2, 1);
	bitset_set(bm1, 3, 1);
	bitset_set(bm1, 193, 1);
	bitset_set(bm1, 1024, 1);

	bitset_set(bm1, 1025, 1);
	bitset_set(bm1, 16384, 1);
	bitset_set(bm1, 16385, 1);

	struct bitset *bm2;
	bitset_new(&bm2);
	bitset_set(bm2, 17, 1);
	bitset_set(bm2, 194, 1);
	bitset_set(bm2, 1023, 1);

	struct bitset_expr *expr = bitset_expr_new();
	fail_if(expr == NULL);

	fail_unless(bitset_expr_add_group(expr,
		BITSET_OP_AND, BITSET_OP_NULL) == 0);

	fail_unless(bitset_expr_group_add_bitset(expr,
		0, bm1, BITSET_OP_NULL) == 0);
	fail_unless(bitset_expr_group_add_bitset(expr,
		0, bm2, BITSET_OP_NULL) == 0);

	struct bitset_iterator *it;
	fail_unless(bitset_iterator_new(&it) == 0);
	fail_unless(bitset_iterator_set_expr(it, expr) == 0);

	size_t pos = bitset_iterator_next(it);
	fail_unless(pos == SIZE_MAX);

	bitset_iterator_free(&it);
	bitset_expr_free(expr);

	bitset_free(&bm1);
	bitset_free(&bm2);

	footer();
}

static
void test_first()
{
	header();

	struct bitset *bm1;
	bitset_new(&bm1);
	bitset_set(bm1, 0, 1);
	bitset_set(bm1, 1023, 1);

	struct bitset *bm2;
	bitset_new(&bm2);
	bitset_set(bm2, 0, 1);
	bitset_set(bm2, 1025, 1);

	struct bitset_expr *expr = bitset_expr_new();
	fail_if(expr == NULL);
	fail_unless(bitset_expr_add_group(expr,
		BITSET_OP_AND, BITSET_OP_NULL) == 0);

	fail_unless(bitset_expr_group_add_bitset(expr,
		0, bm1, BITSET_OP_NULL) == 0);
	fail_unless(bitset_expr_group_add_bitset(expr,
		0, bm2, BITSET_OP_NULL) == 0);

	struct bitset_iterator *it;
	fail_unless(bitset_iterator_new(&it) == 0);
	fail_unless(bitset_iterator_set_expr(it, expr) == 0);

	size_t pos = bitset_iterator_next(it);

	fail_unless(pos == 0);
	fail_unless(bitset_iterator_next(it) == SIZE_MAX);

	bitset_iterator_free(&it);
	bitset_expr_free(expr);

	bitset_free(&bm1);
	bitset_free(&bm2);

	footer();
}

static
void test_and() {
	header();

	struct bitset *bm1;
	fail_if (bitset_new(&bm1) < 0);

	struct bitset *bm2;
	fail_if (bitset_new(&bm2) < 0);

	size_t *nums = malloc(sizeof(*nums) * NUM_SIZE);
	fail_if(nums == NULL);

	for (size_t i = 0; i < NUM_SIZE; i++) {
		nums[i] = rand();
	}

	for (size_t i = 0; i < NUM_SIZE; i++) {
		size_t a = ((size_t) rand()) << 1;
		size_t b = a + 1;

		bitset_set(bm1, a, 1);
		bitset_set(bm2, b, 1);
	}

	for (size_t i = 0; i < NUM_SIZE; i++) {
		bitset_set(bm1, nums[i], 1);
		bitset_set(bm2, nums[i], 1);
	}

	struct bitset_expr *expr = bitset_expr_new();
	fail_if(expr == NULL);
	fail_unless(bitset_expr_add_group(expr,
		BITSET_OP_AND, BITSET_OP_NULL) == 0);

	fail_unless(bitset_expr_group_add_bitset(expr,
		0, bm1, BITSET_OP_NULL) == 0);
	fail_unless(bitset_expr_group_add_bitset(expr,
		0, bm2, BITSET_OP_NULL) == 0);

	qsort(nums, NUM_SIZE, sizeof(size_t), size_compator);

	struct bitset_iterator *it;
	fail_unless(bitset_iterator_new(&it) == 0);
	fail_unless(bitset_iterator_set_expr(it, expr) == 0);

	for (size_t i = 0; i < NUM_SIZE; i++) {
		size_t pos = bitset_iterator_next(it);
		fail_unless(pos == nums[i]);
	}

	size_t pos = bitset_iterator_next(it);
	fail_unless(pos == SIZE_MAX);

	free(nums);

	bitset_iterator_free(&it);
	bitset_expr_free(expr);

	bitset_free(&bm1);
	bitset_free(&bm2);

	footer();
}

static
void test_and1() {
	header();

	const size_t BITSET_SIZE = 32;

	struct bitset **bitsets = calloc(BITSET_SIZE, sizeof(*bitsets));
	fail_if(bitsets == NULL);

	struct bitset_expr *expr = bitset_expr_new();
	fail_if(expr == NULL);
	fail_unless(bitset_expr_add_group(expr,
		BITSET_OP_AND, BITSET_OP_NULL) == 0);

	size_t **pnumbers = malloc(sizeof(*pnumbers) * BITSET_SIZE);

	for(size_t b = 0; b < BITSET_SIZE; b++) {
		struct bitset *bitset;
		bitset_new(&bitset);
		fail_unless(bitset_expr_group_add_bitset(expr,
			0, bitset, BITSET_OP_NULL) == 0);
		bitsets[b] = bitset;
	}

	printf("Generating test set... ");
	for(size_t b = 0; b < BITSET_SIZE; b++) {
		pnumbers[b] = malloc(sizeof(**pnumbers) * NUM_SIZE);
		for (size_t n = 0; n < NUM_SIZE; n++) {
			size_t pos = (size_t) rand();
			pnumbers[b][n] = pos;
		}
	}
	printf("ok\n");

	printf("Setting bits... ");
	for(size_t b = 0; b < BITSET_SIZE; b++) {
		for (size_t n = 0; n < NUM_SIZE; n++) {
			bitset_set(bitsets[b],
				   pnumbers[b][n], 1);
		}
	}
	printf("ok\n");

	printf("Iterating... ");
	struct bitset_iterator *it;
	fail_unless(bitset_iterator_new(&it) == 0);
	fail_unless(bitset_iterator_set_expr(it, expr) == 0);

	size_t pos;
	while ((pos = bitset_iterator_next(it)) != SIZE_MAX) {
		for(size_t b = 0; b < BITSET_SIZE; b++) {
			struct bitset *bitset = bitsets[b];
			bool bit_is_set = bitset_get(bitset, pos);

			if (bit_is_set) {
				continue;
			}

			printf("Failed: bitset %zu pos = %zu\n", b, pos);
			printf("Test set:\n");
			for(size_t bb = 0; bb < BITSET_SIZE; bb++) {
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

	bitset_iterator_free(&it);
	bitset_expr_free(expr);

	for(size_t b = 0; b < BITSET_SIZE; b++) {
		bitset_free(&(bitsets[b]));
	}

	for(size_t b = 0; b < BITSET_SIZE; b++) {
		free(pnumbers[b]);
	}

	free(pnumbers);

	footer();
}

static
void test_or() {
	header();

	struct bitset *bm1;
	fail_if (bitset_new(&bm1) < 0);

	struct bitset *bm2;
	fail_if (bitset_new(&bm2) < 0);

	size_t *nums = malloc(sizeof(*nums) * NUM_SIZE);
	fail_if(nums == NULL);

	for (size_t i = 0; i < NUM_SIZE; i++) {
		nums[i] = rand();
	}


	for (size_t i = 0; i < NUM_SIZE; i++) {
		if (nums[i] & 1) {
			bitset_set(bm1, nums[i], 1);
		} else {
			bitset_set(bm2, nums[i], 1);
		}
	}

	struct bitset_expr *expr = bitset_expr_new();
	fail_if(expr == NULL);
	fail_unless(bitset_expr_add_group(expr,
		BITSET_OP_OR, BITSET_OP_NULL) == 0);

	fail_unless(bitset_expr_group_add_bitset(expr,
		0, bm1, BITSET_OP_NULL) == 0);
	fail_unless(bitset_expr_group_add_bitset(expr,
		0, bm2, BITSET_OP_NULL) == 0);

	struct bitset_iterator *it;
	fail_unless(bitset_iterator_new(&it) == 0);
	fail_unless(bitset_iterator_set_expr(it, expr) == 0);

	qsort(nums, NUM_SIZE, sizeof(size_t), size_compator);

	size_t prev_num = SIZE_MAX;
	for (size_t i = 0; i < NUM_SIZE; i++) {
		if (nums[i] == prev_num) {
			continue;
		}
		prev_num = nums[i];

		size_t pos = bitset_iterator_next(it);
		fail_unless(pos == nums[i]);
	}

	size_t pos = bitset_iterator_next(it);
	fail_unless(pos == SIZE_MAX);

	free(nums);

	bitset_iterator_free(&it);
	bitset_expr_free(expr);

	bitset_free(&bm1);
	bitset_free(&bm2);

	footer();
}

static
void test_xor() {
	header();

	struct bitset *bm1;
	fail_if (bitset_new(&bm1) < 0);

	struct bitset *bm2;
	fail_if (bitset_new(&bm2) < 0);

	size_t *nums = malloc(sizeof(*nums) * NUM_SIZE);
	fail_if(nums == NULL);

	for (size_t i = 0; i < NUM_SIZE; i++) {
		nums[i] = rand();
	}

	for (size_t i = 0; i < NUM_SIZE; i++) {
		if (nums[i] & 1) {
			bitset_set(bm1, nums[i], 1);
		} else {
			bitset_set(bm2, nums[i], 1);
		}

		if (nums[i] % 5 == 0) {
			bitset_set(bm1, nums[i], 1);
			bitset_set(bm2, nums[i], 1);
		}
	}

	struct bitset_expr *expr = bitset_expr_new();
	fail_if(expr == NULL);
	fail_unless(bitset_expr_add_group(expr,
		BITSET_OP_XOR, BITSET_OP_NULL) == 0);

	fail_unless(bitset_expr_group_add_bitset(expr,
		0, bm1, BITSET_OP_NULL) == 0);
	fail_unless(bitset_expr_group_add_bitset(expr,
		0, bm2, BITSET_OP_NULL) == 0);

	struct bitset_iterator *it;
	fail_unless(bitset_iterator_new(&it) == 0);
	fail_unless(bitset_iterator_set_expr(it, expr) == 0);

	qsort(nums, NUM_SIZE, sizeof(size_t), size_compator);

	for (size_t i = 0; i < NUM_SIZE; i++) {
		if (nums[i] % 5 == 0) {
			continue;
		}
		size_t pos = bitset_iterator_next(it);
		fail_unless(pos == nums[i]);
	}

	size_t pos = bitset_iterator_next(it);
	fail_unless(pos == SIZE_MAX);

	free(nums);

	bitset_iterator_free(&it);
	bitset_expr_free(expr);

	bitset_free(&bm1);
	bitset_free(&bm2);

	footer();
}

static
void test_intergroup() {
	header();

	struct bitset *bm1;
	fail_if (bitset_new(&bm1) < 0);

	struct bitset *bm2;
	fail_if (bitset_new(&bm2) < 0);

	size_t *nums = malloc(sizeof(*nums) * NUM_SIZE);
	fail_if(nums == NULL);

	for (size_t i = 0; i < NUM_SIZE; i++) {
		nums[i] = rand();
	}

	for (size_t i = 0; i < NUM_SIZE; i++) {
		bitset_set(bm1, nums[i], 1);
		bitset_set(bm2, nums[i], 1);
	}

	for (size_t i = 0; i < NUM_SIZE; i++) {
		size_t a = ((size_t) rand()) << 1;
		size_t b = a + 1;

		bitset_set(bm1, a, 1);
		bitset_set(bm2, b, 1);
	}

	struct bitset_expr *expr = bitset_expr_new();
	fail_if(expr == NULL);

	fail_unless(bitset_expr_add_group(expr,
		BITSET_OP_AND, BITSET_OP_NULL) == 0);
	fail_unless(bitset_expr_group_add_bitset(expr,
		0, bm1, BITSET_OP_NULL) == 0);

	fail_unless(bitset_expr_add_group(expr,
		BITSET_OP_AND, BITSET_OP_NULL) == 0);
	fail_unless(bitset_expr_group_add_bitset(expr,
		1, bm2, BITSET_OP_NULL) == 0);

	struct bitset_iterator *it;
	fail_unless(bitset_iterator_new(&it) == 0);
	fail_unless(bitset_iterator_set_expr(it, expr) == 0);

	qsort(nums, NUM_SIZE, sizeof(size_t), size_compator);

	for (size_t i = 0; i < NUM_SIZE; i++) {
		size_t pos = bitset_iterator_next(it);
		fail_unless(pos == nums[i]);
	}

	size_t pos = bitset_iterator_next(it);
	fail_unless(pos == SIZE_MAX);

	free(nums);

	bitset_iterator_free(&it);
	bitset_expr_free(expr);

	bitset_free(&bm1);
	bitset_free(&bm2);

	footer();
}

static
void test_op_not1() {
	header();

	size_t big_i = (size_t) 1 << 15;
	struct bitset *bm1;
	bitset_new(&bm1);
	bitset_set(bm1, 0, 1);
	bitset_set(bm1, 11, 1);
	bitset_set(bm1, 1024, 1);

	struct bitset *bm2;
	bitset_new(&bm2);
	bitset_set(bm2, 0, 1);
	bitset_set(bm2, 10, 1);
	bitset_set(bm2, 11, 1);
	bitset_set(bm2, 14, 1);
	bitset_set(bm2, big_i, 1);

	struct bitset_expr *expr = bitset_expr_new();
	fail_if(expr == NULL);

	fail_unless(bitset_expr_add_group(expr,
		BITSET_OP_AND, BITSET_OP_NULL) == 0);
	fail_unless(bitset_expr_group_add_bitset(expr,
		0, bm1, BITSET_OP_NOT) == 0);
	fail_unless(bitset_expr_group_add_bitset(expr,
		0, bm2, BITSET_OP_NULL) == 0);

	struct bitset_iterator *it;
	fail_unless(bitset_iterator_new(&it) == 0);
	fail_unless(bitset_iterator_set_expr(it, expr) == 0);

	size_t result[] = {10, 14, big_i};
	size_t result_size = 3;

	size_t pos;
	for (size_t i = 0; i < result_size; i++) {
		pos = bitset_iterator_next(it);
		fail_unless (result[i] == pos);
	}
	fail_unless (pos = bitset_iterator_next(it) == SIZE_MAX);

	bitset_iterator_free(&it);
	bitset_expr_free(expr);

	bitset_free(&bm1);
	bitset_free(&bm2);

	footer();
}

#ifdef BITSET_ITERATOR_POST_OP_FEATURE
static
void test_op_not2() {
	header();

	struct bitset_iterator *it;
	bitset_iterator_newn(&it, NULL, 0, NULL, BITSET_OP_NOT);

	for(size_t i = 0; i < 10000; i++) {
		size_t pos = bitset_iterator_next(it);
		fail_unless (pos == i);
	}

	bitset_iterator_free(&it);

	footer();
}

static
void test_op_not3() {
	header();

	struct bitset *bm1;
	bitset_new(&bm1);
	struct bitset *bm2;
	bitset_new(&bm2);

	size_t result[] = {0, 15, 43243, 65536};
	size_t result_size = 4;

	for (size_t i = 0; i < result_size; i++) {
		bitset_set(bm2, result[i], 1);
	}

	//bitset_set(bm2, result[result_size - 1] + 1024, 1);

	struct bitset *bitsets[] = { bm1, bm2 };
	int flags[] = { BITSET_OP_NOT, BITSET_OP_NOT };
	int result_flags = BITSET_OP_NOT;

	struct bitset_iterator *it;
	bitset_iterator_newn(&it, bitsets, 2, flags, result_flags);


	size_t pos = SIZE_MAX;
	for (size_t i = 0; i < result_size; i++) {
		pos = bitset_iterator_next(it);
		fail_unless (pos == result[i]);
	}

	pos = bitset_iterator_next(it);
	fail_unless (pos == SIZE_MAX);

	bitset_iterator_free(&it);

	bitset_free(&bm1);
	bitset_free(&bm2);

	footer();
}
#endif /* BITSET_ITERATOR_POST_OP_FEATURE */

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
#ifdef BITSET_ITERATOR_POST_OP_FEATURE
	test_op_not2();
	test_op_not3();
#endif /* BITSET_ITERATOR_POST_OP_FEATURE */
	return 0;
}
