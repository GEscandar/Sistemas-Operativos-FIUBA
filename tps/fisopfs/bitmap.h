#ifndef BITMAP_H
#define BITMAP_H

#include <limits.h>  // for CHAR_BIT
#include <stdint.h>
#include <stdbool.h>


#define WORD_OFFSET(bitno) ((bitno) / BITS_PER_WORD)
#define BIT_OFFSET(bitno) ((bitno) % BITS_PER_WORD)

#define END_STRING (char) 0

/* ----------BITMAP STRUCTURES --------- */

typedef uint32_t word_t;

typedef struct bitmap {
	word_t *words;
	uint32_t nwords;
} bitmap_t;

enum { BITS_PER_WORD = sizeof(word_t) * CHAR_BIT };

/* ----------  Operations ----------- */

void set_bit(bitmap_t *bitmap, uint32_t bitno);

void clear_bit(bitmap_t *bitmap, uint32_t bitno);

bool is_bit_on(bitmap_t *bitmap, uint32_t bitno);

long get_free_bit(bitmap_t *bitmap);

#endif  // BITMAP_H