#include "bitmap.h"
#include <errno.h>
#include <stdio.h>

void
set_bit(bitmap_t *bitmap, uint32_t bitno)
{
	bitmap->words[WORD_OFFSET(bitno)] |= ((word_t) 1 << BIT_OFFSET(bitno));
}

void
clear_bit(bitmap_t *bitmap, uint32_t bitno)
{
	bitmap->words[WORD_OFFSET(bitno)] &= ~((word_t) 1 << BIT_OFFSET(bitno));
}

bool
is_bit_on(bitmap_t *bitmap, uint32_t bitno)
{
	word_t bit = bitmap->words[WORD_OFFSET(bitno)] &
	             ((word_t) 1 << BIT_OFFSET(bitno));
	return bit != 0;
}

long
get_free_bit(bitmap_t *bitmap)
{
	const word_t umask = (word_t) ~0;
	uint32_t shift = BITS_PER_WORD / 2;

	for (int i = 0; i < bitmap->nwords; i++) {
		word_t w = bitmap->words[i];
		if ((w ^ umask) != 0) {
			word_t umask_lo = umask >> shift,
			       umask_hi = umask << shift;
			printf("[debug] word: %u\n", w);
			while ((shift /= 2) != 0) {
				printf("[debug] umask_lo: %u\n", umask_lo);
				printf("[debug] umask_hi: %u\n", umask_hi);

				if ((w & umask_lo) != umask_lo) {
					printf("[debug] Located free bit in "
					       "lower part!\n");
					umask_hi = (umask_lo << shift) & umask_lo;
					umask_lo = (umask_lo >> shift) & umask_lo;
				} else if ((w & umask_hi) != umask_hi) {
					printf("[debug] Located free bit in "
					       "higher part!\n");
					;
					umask_lo = (umask_hi >> shift) & umask_hi;
					umask_hi = (umask_hi << shift) & umask_hi;
				}
			}
			printf("[debug] umask_lo: %u\n", umask_lo);
			printf("[debug] umask_hi: %u\n", umask_hi);
			printf("[debug] Found free bit!\n");
			word_t zero_mask;
			if ((w & umask_lo) == 0)
				zero_mask = umask_lo;
			else if ((w & umask_hi) == 0)
				zero_mask = umask_hi;
			else
				// This should never happen, but just in case return error
				return -ENOSPC;

			int bit_position = -1;
			while (zero_mask != 0) {
				zero_mask >>= 1;
				bit_position++;
			}
			printf("[debug] bit position: %d\n", bit_position);
			uint32_t bitno = i * BITS_PER_WORD + bit_position;
			return bitno;
		}
	}

	return -ENOSPC;
}