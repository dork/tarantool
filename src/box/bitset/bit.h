#ifndef UTIL_H
#define UTIL_H

/**
 * @file
 * @brief Bit manipulation utils
 */

/** @todo move this file to Tarantool src/ directory */

#include <util.h>

/*
 * bit_ctz
 */

/**
 * @brief Naive implementation of ctz.
 * @cond false
 */
#define CTZ_NAIVE(x, bitsize) {					\
	if (x == 0) {							\
		return (bitsize);					\
	}								\
									\
	int r = 0;							\
	for (; (x & 1) == 0; r++) {					\
		x >>= 1;						\
	}								\
									\
	return r;							\
}
/** @endcond */

/**
 * @brief Count Trailing Zeros.
 * Returns the number of trailing 0-bits in @a x, starting at the least
 * significant bit position. If @a x is 0, the result is undefined.
 * @param x integer
 * @see __builtin_ctz()
 * @return the number trailing 0-bits
 */
static inline int
bit_ctz_u32(u32 x)
{
#if defined(HAVE_CTZ)
	return __builtin_ctz(x);
#else /* !defined(HAVE_CTZ) */
	CTZ_NAIVE(x, __SIZEOF_INT__ * CHAR_BIT);
#endif
}

/**
 * @copydoc bit_ctz_u32
 */
static inline int
bit_ctz_u64(u64 x)
{
#if   defined(HAVE_CTZLL)
	return __builtin_ctzll(x);
#else /* !defined(HAVE_CTZLL) */
	CTZ_NAIVE(x, __SIZEOF_LONG_LONG__ * CHAR_BIT);
#endif
}

#undef CTZ_NAIVE


/*
 * bit_clz
 */

/**
 * @brief Naive implementation of clz.
 * @cond false
 */
#define CLZ_NAIVE(x, bitsize) {					\
	if (x == 0) {							\
		return  (bitsize);					\
	}								\
									\
	int r = (bitsize);						\
	for (; x; r--) {						\
		x >>= 1;						\
	}								\
									\
	return r;							\
}
/** @endcond */

/**
 * @brief Count Leading Zeros.
 * Returns the number of leading 0-bits in @a x, starting at the most
 * significant bit position. If @a x is 0, the result is undefined.
 * @param x integer
 * @see __builtin_clz()
 * @return the number of leading 0-bits
 */
static inline int
bit_clz_u32(u32 x)
{
#if   defined(HAVE_CLZ)
	return __builtin_clz(x);
#else /* !defined(HAVE_CLZ) */
	CLZ_NAIVE(x, __SIZEOF_INT__ * CHAR_BIT);
#endif
}

/**
 * @copydoc bit_clz_u32
 */
static inline int
bit_clz_u64(u64 x)
{
#if   defined(HAVE_CLZLL)
	return __builtin_clzll(x);
#else /* !defined(HAVE_CLZLL) */
	CLZ_NAIVE(x, __SIZEOF_LONG_LONG__ * CHAR_BIT);
#endif
}
#undef CLZ_NAIVE

/*
 * bit_count
 */

/**
 * @brief Naive implementation of popcount.
 * @cond false
 */
#define POPCOUNT_NAIVE(x, bitsize)  {					\
	int r;								\
	for (r = 0; x; r++) {						\
		x &= (x-1);						\
	}								\
									\
	return r;							\
}
/** @endcond */

/**
 * @brief Returns the number of 1-bits in x.
 * @param x integer
 * @see __builtin_popcount()
 * @return the number of 1-bits in x
 */
static inline int
bit_count_u32(u32 x)
{
#if   defined(HAVE_POPCOUNT)
	return __builtin_popcount(x);
#else /* !defined(HAVE_POPCOUNT) */
	POPCOUNT_NAIVE(x, __SIZEOF_INT__ * CHAR_BIT);
#endif
}

/**
 * @copydoc bit_count_u32
 */
static inline int
bit_count_u64(u64 x)
{
#if   defined(HAVE_POPCOUNTLL)
	return __builtin_popcountll(x);
#else /* !defined(HAVE_POPCOUNTLL) */
	POPCOUNT_NAIVE(x, __SIZEOF_LONG_LONG__ * CHAR_BIT);
#endif
}
#undef POPCOUNT_NAIVE

/*
 * bit_index
 */

/**
 * @brief Index bits in the @a x, i.e. find all positions where bits are set.
 * This method fills @a indexes array with found positions in increasing order.
 * @a offset is added to each index before putting it into @a indexes.
 * @param x integer
 * @param indexes memory array where found indexes are stored
 * @param offset a number added to each index
 * @return pointer to last+1 element in indexes array
 */
int *
bit_index_u32(u32 x, int *indexes, int offset);

/**
 * @copydoc bit_index_u32
 */
int *
bit_index_u64(u64 x, int *indexes, int offset);

#endif // UTIL_H
