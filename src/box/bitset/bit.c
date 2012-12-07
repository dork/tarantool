#include "bit.h"

/*
 * bit_index
 */

#define BITINDEX_NAIVE(x, bitsize) {					\
	/* naive generic implementation, worst case */			\
	size_t bit = 1;							\
	int i = 0;							\
	for (int k = 0; k < bitsize; k++) {				\
		if (x & bit) {						\
			indexes[i++] = offset + k + 1;			\
		}							\
		bit <<= 1;						\
	}								\
									\
	indexes[i] = 0;							\
	return indexes + i;						\
}

int *
bit_index_u32(u32 x, int *indexes, int offset)
{
#if  defined(HAVE_CTZ)
	int prev_pos = 0;
	int i = 0;

#if defined(HAVE_POPCOUNT)
	/* fast implementation using hardware popcount instruction */
	const int count = bit_count_u32(x);
	while (i < count) {
#else
	/* sligtly slower implementation without using hw popcnt */
	while(x) {
#endif
		/* use ctz */
		const int a = bit_ctz_u32(x);

		prev_pos += a + 1;
		x >>= a;
		x >>= 1;
		indexes[i++] = offset + prev_pos;
	}

	indexes[i] = 0;
	return indexes + i;
#else /* !defined(HAVE_CTZ) */
	BITINDEX_NAIVE(x, __SIZEOF_LONG_LONG__ * CHAR_BIT);
#endif
}

int *
bit_index_u64(u64 x, int *indexes, int offset) {
#if  defined(HAVE_CTZLL)
	int prev_pos = 0;
	int i = 0;

#if defined(HAVE_POPCOUNTLL)
	/* fast implementation using hardware popcount instruction */
	const int count = bit_count_u64(x);
	while (i < count) {
#else
	/* sligtly slower implementation without using hw popcnt */
	while(x) {
#endif
		/* use ctz */
		const int a = bit_ctz_u64(x);

		prev_pos += a + 1;
		x >>= a;
		x >>= 1;
		indexes[i++] = offset + prev_pos;
	}

	indexes[i] = 0;
	return indexes + i;
#else /* !defined(HAVE_CTZ) */
	BITINDEX_NAIVE(x, __SIZEOF_LONG_LONG__ * CHAR_BIT);
#endif
}

#undef BITINDEX_NAIVE
