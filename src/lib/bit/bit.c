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

#include <lib/bit/bit.h>

__attribute__((const)) extern inline int
bit_ctz_u32(uint32_t x);

__attribute__((const))  extern inline int
bit_ctz_u64(uint64_t x);

__attribute__((const)) extern inline int
bit_clz_u32(uint32_t x);

__attribute__((const)) extern inline int
bit_clz_u64(uint64_t x);

__attribute__((const)) extern inline int
bit_count_u32(uint32_t x);

__attribute__((const)) extern inline int
bit_count_u64(uint64_t x);

__attribute__ ((const)) extern inline uint32_t
bit_rotl_u32(uint32_t x, int r);

__attribute__ ((const)) extern inline uint64_t
bit_rotl_u64(uint64_t x, int r);

__attribute__ ((const)) extern inline uint32_t
bit_rotr_u32(uint32_t x, int r);

__attribute__ ((const)) extern inline uint64_t
bit_rotr_u64(uint64_t x, int r);

__attribute__ ((const)) extern inline uint32_t
bswap_u32(uint32_t x);

__attribute__ ((const)) extern inline uint64_t
bswap_u64(uint64_t x);

#define BITINDEX_NAIVE(x, bitsize) {					\
	/* naive generic implementation, worst case */			\
	typeof(x) bit = 1;						\
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
bit_index_u32(uint32_t x, int *indexes, int offset)
{
#if  defined(HAVE_CTZ)
	int prev_pos = 0;
	int i = 0;

#if defined(HAVE_POPCOUNT)
	/* fast implementation using built-in popcount function */
	const int count = bit_count_u32(x);
	while (i < count) {
#else
	/* sligtly slower implementation without using built-in popcount */
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
	BITINDEX_NAIVE(x, sizeof(uint32_t) * CHAR_BIT);
#endif
}

int *
bit_index_u64(uint64_t x, int *indexes, int offset) {
#if  defined(HAVE_CTZLL)
	int prev_pos = 0;
	int i = 0;

#if defined(HAVE_POPCOUNTLL)
	/* fast implementation using built-in popcount function */
	const int count = bit_count_u64(x);
	while (i < count) {
#else
	/* sligtly slower implementation without using built-in popcount */
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
	BITINDEX_NAIVE(x, sizeof(uint64_t) * CHAR_BIT);
#endif
}

#undef BITINDEX_NAIVE
