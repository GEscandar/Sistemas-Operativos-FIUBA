#ifndef _MEM_H
#define _MEM_H

#include <stdio.h>
#include <stdbool.h>


#define MIN_REGION_SIZE 256
#define ALLOC_FAILED (region_t *) -1

enum block_size {
    BLOCK_SMALL = 16*1024,
    BLOCK_MEDIUM = 1024*1024,
    BLOCK_BIG = 32*BLOCK_MEDIUM
};

typedef struct region {
    size_t size;
    struct region *next;
    bool free;
    bool has_next;
    bool has_prev;
    size_t prev_size;
} region_t;

// Helper functions
extern void print_region(region_t *region);
extern bool is_valid_pointer(void *ptr);

extern region_t *region_free_list;

#endif
