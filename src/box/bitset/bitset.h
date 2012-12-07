#ifndef BITSET_BITSET_H_INCLUDED
#define BITSET_BITSET_H_INCLUDED

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

/**
 * @file
 * @brief Module to work with arrays of bits (bitsets)
 *
 * Bitset is an array of bits where each bit can be set or unset independently.
 * It supports special compression scheme in order to save memory.
 * You can use any values in range [0,SIZE_MAX) without worrying about resizing.
 *
 * @author Roman Tsisyk
 */

#include <util.h>

struct bitset;

/**
 * @brief Allocates and construct new bitset and sets value of *pbitset.
 * @param pbitset
 */
struct bitset *
bitset_new(void);

/**
 * @brief Destruct and deallocates bitsets and sets *pbitset to NULL.
 * @param pbitset
 */
void
bitset_delete(struct bitset *bitset);

/**
 * @brief Gets a bit from the bitset
 * @param bitset object
 * @param pos position (index)
 * @return bit value
 */
bool
bitset_get(struct bitset *bitset, size_t pos);

/**
 * @brief Sets a bit in the bitset
 * @param bitset object
 * @param pos position (index)
 * @return 0 on success and -1 on error
 */
int
bitset_set(struct bitset *bitset, size_t pos, bool val);


/**
 * @brief Returns the number of bits set to true in this bitset.
 * @param bitset object
 * @return returns the number of bits set to true in this bitset.
 */
size_t
bitset_cardinality(struct bitset *bitset);


#if defined(DEBUG)
struct bitset_stat {
	bool have_avx:1;
	bool have_sse2:1;
	bool have_ctz:1;
	bool have_popcnt:1;
	int word_bit;
	int page_bit;
	int words_per_page;
	size_t cardinality;
	size_t capacity;
	size_t pages;
	size_t mem_pages;
	size_t mem_other;
};

#include <stdio.h>
void
bitset_stat(struct bitset *bitset, struct bitset_stat *stat);

void
bitset_dump(struct bitset *bitset, int verbose, FILE *stream);
#endif

#endif // BITSET_BITSET_H_INCLUDED
