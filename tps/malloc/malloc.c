#define _DEFAULT_SOURCE

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <limits.h>

#include "malloc.h"
#include "printfmt.h"
#include "mem.h"

#define ALIGN4(s) (((((s) -1) >> 2) << 2) + 4)
#define REGION2PTR(r) ((r) + 1)
#define PTR2REGION(ptr) ((region_t *) (ptr) -1)
#define REGIONOFFSET(ptr, offset)                                              \
	((region_t *) ((char *) REGION2PTR(ptr) + offset))
#define PREVREGION(ptr)                                                        \
	((region_t *) ((char *) ptr - (ptr->prev_size + sizeof(region_t))))
#define MEMERROR_AND_RETURN                                                    \
	{                                                                      \
		errno = ENOMEM;                                                \
		return NULL;                                                   \
	}

// prototypes
void initialize_cell(void *cell, size_t size);

region_t *coalesce(region_t *curr);

region_t *region_free_list = NULL;

int amount_of_mallocs = 0;
int amount_of_frees = 0;
int requested_memory = 0;

static void
append_region(region_t *region)
{
	region->next = region_free_list;
	region_free_list = region;
}

static void
unlink_region(region_t *target)
{
	region_t **head = &region_free_list;
	if (target == *head) {
		*head = (*head)->next;
		target->next = NULL;
	} else {
		region_t *curr = *head;
		bool found = false;
		while (curr->next && !found) {
			if (curr->next == target) {
				curr->next = target->next;
				target->next = NULL;
				found = true;
			} else {
				curr = curr->next;
			}
		}
	}
}

// Check if this pointer points to a (non-free) region
// allocated by malloc.
bool
is_valid_pointer(void *ptr)
{
	region_t *region = PTR2REGION(ptr);
	return (!region->free && (region->has_next || region->has_prev));
}

void
print_region(region_t *region)
{
	logfmt("------\n");
	logfmt("addr: 			%p\n", region);
	if (region) {
		logfmt("region size: 		%d\n", region->size);
		logfmt("prev region size: 		%d\n", region->prev_size);
		logfmt("was freed: 		%s\n",
		       region->free ? "yes" : "no");
		logfmt("has next: 		%s\n",
		       region->next ? "yes" : "no");
		logfmt("has next neighbour: 		%s\n",
		       region->has_next ? "yes" : "no");
		logfmt("has prev neighbour: 		%s\n",
		       region->has_prev ? "yes" : "no");
	}
	logfmt("\n");
}

static void
print_statistics(void)
{
	printfmt("mallocs:   %d\n", amount_of_mallocs);
	printfmt("frees:     %d\n", amount_of_frees);
	printfmt("requested: %d\n", requested_memory);
}

// finds the next free region
// that holds the requested size
//
static region_t *
find_free_region(size_t size)
{
	region_t *next = region_free_list;
	size_t target_size = sizeof(region_t) + size;
	logfmt("Looking for region of size>=%d\n", target_size);

#ifdef FIRST_FIT
	// Your code here for "first fit"
	while (next && next->size < target_size) {
		next = next->next;
	}
#endif

#ifdef BEST_FIT
	// Your code here for "best fit"
	region_t *best_fit = NULL;
	while (next) {
		if (next->size >= target_size &&
		    (!best_fit || next->size < best_fit->size))
			best_fit = next;
		next = next->next;
	}
	if (best_fit)  // next is always NULL here, so check if a best_fit was found
		next = best_fit;
#endif

	return next;
}

static region_t *
grow_heap(size_t size)
{
	// allocates the requested size
	region_t *curr = (region_t *) mmap(NULL,
	                                   sizeof(region_t) + size,
	                                   PROT_READ | PROT_WRITE,
	                                   MAP_PRIVATE | MAP_ANONYMOUS,
	                                   -1,
	                                   0);

	// verifies that the allocation
	// is successful
	//
	if (curr == ALLOC_FAILED) {
		return NULL;
	}

	curr->size = size;
	curr->next = NULL;

	curr->free = true;  // aggo esto [tema in_use]
	curr->has_next = false;
	curr->has_prev = false;
	curr->prev_size = 0;

	return curr;
}

/* Receives a region 'reg' (which has a given reg->size) and a requested 'size'.
Takes 'size' from the last portion of memory of 'reg', for a new region called
'splitted'. Returns 'splitted', wich has size 'size'. Leaves the rest, 'reg'
with size 'excess', remains in its original state (as free memory if it was free memory, or used if it was used). */
static region_t *
split(region_t *reg, size_t size)
{
	print_region(reg);
	region_t *splitted = NULL;
	size_t excess =
	        reg->size - (sizeof(region_t) + size);  // allocated - requested

	if (excess >= MIN_REGION_SIZE) {
		// split the region, returning a new region of
		// the requested size and resizing region 'reg'
		// as necessary
		// Perform the split.
		splitted = REGIONOFFSET(reg, excess);
		splitted->size = size;
		splitted->next = NULL;
		splitted->free = false;

		// coalescing related fields
		splitted->has_next = reg->has_next;
		splitted->has_prev = true;
		splitted->prev_size = excess;
		if (reg->has_next) {
			// update the last splitted region's prev_size
			region_t *next_region = REGIONOFFSET(reg, reg->size);
			next_region->prev_size = size;
		}
		reg->has_next = true;

		// finish the split. This line must be down here (below those coalescing related lines)
		reg->size = excess;

		logfmt("Splitted region: \n");
		print_region(reg);
		logfmt("Returned region: \n");
		print_region(splitted);
	}

	return splitted;
}


/* Grows heap in a block of one of our three possible block sizes. Chooses,
between our possible sizes, the smallest one that can contain 'size'. Returns
this new block, in the form of a new node that has all its size block.*/
static region_t *
grow_heap_in_appropiate_block_size(size_t size)
{
	region_t *node_of_whole_block_size = NULL;
	logfmt("No free region found. Creating new block...\n");
	if (size <= BLOCK_SMALL)
		node_of_whole_block_size = grow_heap(BLOCK_SMALL);
	else if (size <= BLOCK_MEDIUM)
		node_of_whole_block_size = grow_heap(BLOCK_MEDIUM);
	else if (size <= BLOCK_BIG)
		node_of_whole_block_size = grow_heap(BLOCK_BIG);
	return node_of_whole_block_size;
}

/* Allocates  size  bytes  and returns a pointer to the allocated memory.
 * The memory is not initialized. If size is 0, or if it is greater than
 * BLOCK_BIG, or if an error occurs, then malloc() returns NULL.
 */
void *
malloc(size_t size)
{
	// si quiero allocar algo que no entra en el bloque más grande
	if ((size == 0) || (size >= BLOCK_BIG))
		return NULL;

	// updates statistics
	if (amount_of_mallocs == 0) {
		// first time here
		atexit(print_statistics);
	}
	amount_of_mallocs++;
	requested_memory += size;

	if (size < MIN_REGION_SIZE)
		size = MIN_REGION_SIZE;

	region_t *next = find_free_region(size);
	bool new_block = false;

	if (!next) {
		// If there is no region of sufficient size available,
		// map a new memory block
		next = grow_heap_in_appropiate_block_size(size);
		if (!next)
			MEMERROR_AND_RETURN;  // size too big or mmap failed
		new_block = true;
	} else {
		logfmt("Found available region! \n");
		print_region(next);
	}

	// Your code here
	// split free regions
	region_t *splitted = split(next, size);
	if (splitted != NULL) {
		if (new_block) {
			// only append the new block if we can split it
			append_region(next);
		}
		return REGION2PTR(splitted);
	}

	// If the region can't be splitted, unlink it
	// and return the whole thing
	if (!new_block)
		unlink_region(next);

	next->free = false;
	logfmt("Unsplitted region:\n");
	print_region(next);

	return REGION2PTR(next);
}

/* Helper function. Coalesce one region with another.
Parameter NEW_SIZE should usually be 0, and some given value only for a special case (ie usage from realloc functions). */
static void
coalesce_region_with_its_next_region(region_t *curr,
                                     region_t *next_region,
                                     size_t new_size)
{
	// unlink next from list and merge it to curr
	logfmt("Coalescing next region...\n");
	unlink_region(next_region);
	curr->has_next = next_region->has_next;
	if (!new_size) {
		curr->size +=
		        next_region->size + sizeof(region_t);  // most cases.
	} else {
		curr->size = new_size;  // only for special usage from realloc.
	}

	// update neighbour
	if (curr->has_next) {
		next_region = REGIONOFFSET(curr, curr->size);
		next_region->prev_size = curr->size;
	}
}

region_t *
coalesce(region_t *curr)
{
	logfmt("Coalescing...\n");
	print_region(curr);
	region_t *next_region = REGIONOFFSET(curr, curr->size);
	bool has_next = curr->has_next && next_region->free;

	if (has_next) {
		coalesce_region_with_its_next_region(curr, next_region, 0);
	}

	region_t *prev_region = PREVREGION(curr);
	bool has_prev = curr->has_prev && (prev_region->free);

	if (has_prev) {
		coalesce_region_with_its_next_region(prev_region, curr, 0);
		curr = prev_region;  // to return
	}

	return curr;
}


/* Frees the memory space pointed to by ptr, which must have been returned by a
 * previous call to malloc(), calloc(), or realloc(). Otherwise, or if free(ptr)
 * has already been called before, or if ptr is NULL, no operation occurs.
 */
void
free(void *ptr)
{
	// updates statistics
	amount_of_frees++;

	region_t *reg = PTR2REGION(ptr);
	// double free and invalid free protection
	if (ptr == NULL || !is_valid_pointer(ptr)) {
		errno = ENOMEM;
		return;
	}

	reg->free = true;
	logfmt("Freeing region...\n");
	print_region(region_free_list);

	append_region(reg);
	region_t *curr = coalesce(reg);

	if (!curr->has_prev && !curr->has_next) {
		// this region represents the entire free block
		logfmt("Unmapping block...\n");
		unlink_region(curr);
		munmap(curr, curr->size + sizeof(region_t));
	}
}

void
initialize_cell(void *cell, size_t size)
{
	for (size_t i = 0; i < ALIGN4(size); ++i) {
		*(char *) (cell + sizeof(cell) + i) = 0;
	}
}


/* Allocates memory for an array of nmemb elements of size bytes each and
 * returns a pointer to the allocated memory The memory is set to zero.  If
 * nmemb or size is 0, then calloc() returns either NULL, or a unique pointer
 * value that can later be successfully passed to free().  If the multiplication
 * of nmemb and size would result in integer overflow, then calloc() returns an
 * error.  By contrast, an integer overflow would not be detected in the
 * following  call  to malloc(), with the result that an incorrectly sized block
 * of memory would be allocated:
 *           malloc(nmemb * size);
 */
void *
calloc(size_t nmemb, size_t size)
{
	// Your code here
	// Initial checks
	logfmt("Calloc: nmemb=%d, size=%d, n=%d\n", nmemb, size, nmemb * size);
	if ((nmemb == 0) || (size == 0))
		return NULL;

	size_t regionSize = nmemb * size;
	if (regionSize >= INT_MAX)
		MEMERROR_AND_RETURN;

	// Calloc
	void *initial = malloc(regionSize);
	if (!initial)
		MEMERROR_AND_RETURN;

	for (size_t i = 0; i < nmemb; ++i) {
		initialize_cell(initial + i, size);
	}

	return initial;
}


// Resize the corresponding free region to a smaller size
static void
resize_node(region_t *node, size_t new_size)
{
	region_t *curr = region_free_list;
	region_t *prev = NULL;
	logfmt("Resizing region %p to size %d\n", node, new_size);
	bool found = false;
	while (curr && !found) {
		size_t diff = curr->size - new_size;
		if (curr == node) {
			region_t *splitted = (region_t *) ((char *) curr + diff);
			if (curr == region_free_list)
				region_free_list = splitted;
			else
				prev->next = splitted;

			if (diff >= sizeof(region_t)) {
				// no overlap, and this is faster
				memcpy(splitted, curr, sizeof(region_t));
			} else {
				// overlap of (sizeof(region_t) - diff) bytes
				memmove(splitted, curr, sizeof(region_t));
			}

			splitted->size = new_size;
			splitted->prev_size += diff;

			if (splitted->has_next) {
				// update neighbour
				region_t *next_region =
				        REGIONOFFSET(splitted, splitted->size);
				next_region->prev_size = splitted->size;
			}
			found = true;
		} else {
			curr = curr->next;
			prev = curr;
		}
	}
}

/* Helper function. Optimization to reuse current region, when asked for realloc
with a size bigger than that of the current region. Precludes from growing the
heap in case that the excess (diff between new requested size and current size)
fits in current block but the whole new requested size does not.*/
static region_t *
optimization_reuse_current_region(region_t *reg, size_t size)
{
	// check if we can reuse the current region
	region_t *optimized_ptr = NULL;
	if (reg->has_next) {
		region_t *next = REGIONOFFSET(reg, reg->size);
		size_t new_size = reg->size + next->size + sizeof(region_t);

		if (next->free && new_size >= size) {
			size_t excess =
			        reg->size + next->size - size;  // unused size
			// size=100
			// sizeof(region_t) = 40
			// r1: 50, r2: 306
			// new_size = 356 + 40 = 396
			// excess = 356 - 100 = 256
			// r1: 100, r2: 256
			if (excess >= MIN_REGION_SIZE) {
				resize_node(next, excess);
				reg->size = size;
			} else {
				coalesce_region_with_its_next_region(reg,
				                                     next,
				                                     new_size);
			}

			optimized_ptr = REGION2PTR(reg);
		}
	}
	return optimized_ptr;
}

/* Change the size of the memory object pointed to by ptr to the size specified
 * by size. The contents of the object shall remain unchanged up to the lesser of
 * the new and old sizes. If the new size of the memory object would require
 * movement of the object, the space for the previous instantiation of the object
 * is freed, and the contents of the newly allocated portion of the object are
 * unspecified.
 *
 * If ptr is a null pointer, realloc() shall be equivalent to malloc() for the
 * specified size.
 *
 * If size is 0 and ptr is not a null pointer, the object pointed to is freed
 * and a null pointer shall be returned.
 *
 * If the space cannot be allocated, the object shall remain unchanged and
 * a null pointer shall be returned.
 *
 * If ptr does not match a pointer returned earlier by calloc(), malloc(), or
 * realloc() or if the space has previously been deallocated by a call to free()
 * or realloc(), then the object shall remain unchanged and ptr shall be returned.
 */
void *
realloc(void *ptr, size_t size)
{
	// Your code here
	if (!ptr)
		return malloc(size);
	else if (size == 0) {  // (it got here, so ptr is not null)
		free(ptr);
		return NULL;
	}

	region_t *reg = PTR2REGION(ptr);
	void *newptr = ptr;

	if (is_valid_pointer(ptr)) {
		if (size > reg->size) {
			// don't malloc right away, first check for possibility
			// of optimization (returns null if not possible)
			void *newptr_with_optimization =
			        optimization_reuse_current_region(reg, size);
			if (newptr_with_optimization != NULL) {
				newptr = newptr_with_optimization;
			} else {  // optimization not posible
				newptr = malloc(size);
				if (newptr == NULL) {
					MEMERROR_AND_RETURN;
				}

				memcpy(newptr, ptr, reg->size);
				free(ptr);
			}
		}
		if (size < reg->size) {
			// size=500
			// sizeof(region_t) = 40
			// r1: 2000
			// split_size = 1500 - 40 = 1460
			// r1: 500, splitted: 1460
			logfmt("Reallocating pointer of lesser size\n");
			size_t split_size = reg->size - size - sizeof(region_t);
			region_t *splitted = split(reg, split_size);
			if (splitted) {
				free(REGION2PTR(
				        splitted));  // we only used split to resize, and don't want to alloc this region.
			}
		}
	}

	return newptr;
}
