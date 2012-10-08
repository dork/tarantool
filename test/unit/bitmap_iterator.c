#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#include "mod/box/bitmap/bitmap.h"
#include "mod/box/bitmap/index.h"
#include "unit.h"

static
void test_empty()
{
	struct bitmap *bm1;
	bitmap_new(&bm1);
	bitmap_set(bm1, 0, 1);
	bitmap_set(bm1, 10, 1);
	bitmap_set(bm1, 11, 1);
	bitmap_set(bm1, 191, 1);
	bitmap_set(bm1, 192, 1);
	bitmap_set(bm1, 1024, 1);

	bitmap_set(bm1, 1025, 1);
	bitmap_set(bm1, 34555523, 1);
	bitmap_set(bm1, 34555524, 1);

	struct bitmap *bm2;
	bitmap_new(&bm2);
	bitmap_set(bm2, 0, 1);
	bitmap_set(bm2, 14, 1);
	bitmap_set(bm2, 191, 1);
	bitmap_set(bm2, 192, 1);
	bitmap_set(bm2, 1024, 1);

	struct bitmap *bitmaps[] = {bm1, bm2};
	int flags[] = { 0, 0};

	struct bitmap_iterator *it;
	bitmap_iterator_newn(&it, bitmaps, 2, (int *) &flags, 0);

	fail_unless(bitmap_iterator_next(it) != SIZE_MAX);

	bitmap_iterator_free(&it);

	bitmap_free(&bm1);
	bitmap_free(&bm2);
}

static
void test_first()
{
	struct bitmap *bm1;
	bitmap_new(&bm1);
	bitmap_set(bm1, 0, 1);
	bitmap_set(bm1, 1023, 1);

	struct bitmap *bm2;
	bitmap_new(&bm2);
	bitmap_set(bm2, 0, 1);
	bitmap_set(bm2, rand(), 1);
	bitmap_set(bm2, 1025, 1);

	struct bitmap *bitmaps[] = { bm1, bm2 };
	int flags[] = { 0, 0 };

	struct bitmap_iterator *it;
	bitmap_iterator_newn(&it, bitmaps, 2, (int *) &flags, 0);

	fail_unless(bitmap_iterator_next(it) == 0);
	fail_unless(bitmap_iterator_next(it) == SIZE_MAX);

	bitmap_iterator_free(&it);

	bitmap_free(&bm1);
	bitmap_free(&bm2);

}

static
void test_regular() {
	header();

	const size_t NUM_SIZE = 1<<13;
	const size_t BITMAP_SIZE = 32;

	srand(time(NULL));

	struct bitmap **bitmaps = malloc(sizeof(*bitmaps) * BITMAP_SIZE);
	int *flags = malloc(sizeof(*flags) * BITMAP_SIZE);
	size_t **pnumbers = malloc(sizeof(*pnumbers) * BITMAP_SIZE);

	for(size_t b = 0; b < BITMAP_SIZE; b++) {
		bitmap_new(&bitmaps[b]);
		flags[b] = 0;
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
			bitmap_set(bitmaps[b], pnumbers[b][n], 1);
		}
	}
	printf("ok\n");

	printf("Iterating... ");
	struct bitmap_iterator *it;
	bitmap_iterator_newn(&it, bitmaps, BITMAP_SIZE, flags, 0);

	size_t pos;
	while ((pos = bitmap_iterator_next(it)) != SIZE_MAX) {
		for(size_t b = 0; b < BITMAP_SIZE; b++) {
			bool bit_is_set = bitmap_get(bitmaps[b], pos);

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

	for(size_t i = 0; i < BITMAP_SIZE; i++) {
		bitmap_free(&bitmaps[i]);
	}

	for(size_t b = 0; b < BITMAP_SIZE; b++) {
		free(pnumbers[b]);
	}

	free(pnumbers);
	free(bitmaps);
	free(flags);

	footer();
}

int main(int argc, char *argv[])
{
	setbuf(stdout, NULL);
	test_empty();
	test_first();
	test_regular();
}
