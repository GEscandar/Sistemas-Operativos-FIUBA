#include <stdio.h>
#include <string.h>
#include <unistd.h>
// #include <stdlib.h>
#include <stdbool.h>
#include <limits.h>

#include "printfmt.h"
#include "mem.h"
#include "malloc.h"

#include <assert.h>

#define HELLO "hello from test"
#define TEST_STRING "FISOP malloc is working!"

#define ANSI_COLOR_RED "\x1b[1m\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[1m\x1b[32m"
#define ANSI_COLOR_RESET "\x1b[0m"

#define TEST_PREFIX "Test"
#define TEST_PASSED                                                            \
	"\x1b[1m\x1b[32m"                                                      \
	"PASSED"                                                               \
	"\x1b[0m"
#define TEST_FAILED                                                            \
	"\x1b[1m\x1b[31m"                                                      \
	"FAILED"                                                               \
	"\x1b[0m"

#define ALIGN4(s) (((((s) -1) >> 2) << 2) + 4)
#define BLOCK_BIG 32 * 1024 * 1024

int ran = 0, passed = 0, failed = 0;
extern int amount_of_frees;

// only for testing with Clib malloc
// #ifndef _MALLOC_H_
// region_t *region_free_list = NULL;
// #endif

// prototypes:
void ASSERT(char *description, bool condition);
char *stralloc(int size, char *src);

// tests
void free_after_malloc_frees_the_entire_block(void);

void allocating_the_entire_block_empties_the_free_list(void);

void freeing_after_allocating_two_small_blocks_empties_the_free_list(void);

void allocating_too_little_returns_a_region_of_min_size(void);

void allocating_zero_size_returns_NULL(void);

void allocating_3_regions_4alligned_returns_3_directions_without_fragmentation(void);

void
allocating_3_regions_not_alligned_returns_3_directions_without_fragmentation(void);

void free_NULL_region_does_nothing(void);

void requesting_a_size_larger_than_the_biggest_block_with_malloc_returns_NULL(void);

void freeing_after_allocating_1000_pointers_empties_the_free_list(void);

void freeing_after_allocating_10000_pointers_empties_the_free_list(void);

void allocate_array_return_an_initialized_array(void);

void allocating_zero_size_with_calloc_returns_NULL(void);

void reallocating_a_null_pointer_allocates_memory_of_requested_size(void);

void reallocating_a_null_pointer_of_size_0_returns_a_null_pointer(void);

void reallocating_a_pointer_of_size_0_frees_the_pointer(void);

void reallocating_a_pointer_to_a_region_of_the_same_size_returns_the_pointer(void);

void reallocating_a_pointer_to_a_region_of_less_size_resizes_the_pointer(void);

void reallocating_a_valid_pointer_copies_its_contents(void);

void reallocating_a_reusable_pointer_does_not_allocate_a_new_pointer(void);

void freeing_an_invalid_pointer_does_nothing(void);

void freeing_a_pointer_twice_does_nothing_the_second_time(void);

void allocate_array_outs_of_int_limits_make_calloc_return_NULL(void);

// more malloc tests
void
two_consecutive_mallocs_of_small_size_return_pointers_in_contiguous_memory(void);
void free_works(void);
void coalesce_works(void);
void request_that_doesnt_fit_in_current_block_gets_allocated_in_a_new_one(void);
void request_that_doesnt_fit_in_current_block_gets_allocated_in_a_new_block_of_correct_size_SMALL(
        void);
void coalescing_happens_only_inside_of_a_given_block(void);

void
ASSERT(char *description, bool condition)
{
	ran++;
	if (condition) {
		passed++;
		printfmt("%s:	'%s' - %s\n", TEST_PREFIX, description, TEST_PASSED);
	} else {
		failed++;
		printfmt("%s:	'%s' - %s\n", TEST_PREFIX, description, TEST_FAILED);
	}
}


// Allocate a pointer of size 'size', copy string
// src into it, and return it
char *
stralloc(int size, char *src)
{
	char *dest = malloc(size);

	if (dest)
		strcpy(dest, src);

	return dest;
}

// ---------------- TESTS ----------------
void
free_after_malloc_frees_the_entire_block()
{
	void *ptr = malloc(100);
	if (!ptr) {
		perror("Error allocating pointer");
		failed++;
		return;
	}
	ASSERT("After allocating a small region, the free list is not empty",
	       region_free_list != NULL);
	free(ptr);
	ASSERT("Freeing inmediately after malloc frees the entire block",
	       region_free_list == NULL);
}

void
allocating_the_entire_block_empties_the_free_list()
{
	void *ptr1 = malloc(BLOCK_SMALL / 2);
	void *ptr2 = malloc(BLOCK_SMALL / 2 - sizeof(region_t) * 2);
	if (!ptr1 || !ptr2) {
		perror("Error allocating pointer");
		failed++;
		return;
	}

	printfmt("\nEntire block test\n");
	ASSERT("Allocating an entire block of size BLOCK_SMALL empties the "
	       "free list",
	       region_free_list == NULL);

	free(ptr1);
	free(ptr2);
	ASSERT("Freeing all pointers of the only allocated block empties the "
	       "free list",
	       region_free_list == NULL);
}

void
freeing_after_allocating_two_small_blocks_empties_the_free_list()
{
	void *ptr1 = malloc(BLOCK_SMALL / 2);
	void *ptr2 = malloc(BLOCK_SMALL / 2);
	if (!ptr1 || !ptr2) {
		perror("Error allocating pointer");
		failed++;
		return;
	}

	free(ptr1);
	free(ptr2);

	ASSERT("Freeing after allocating 2 small blocks empties the free list",
	       region_free_list == NULL);
}

void
allocating_too_little_returns_a_region_of_min_size()
{
	void *ptr = malloc(10);
	if (!ptr) {
		perror("Error allocating pointer");
		failed++;
		return;
	}

	printfmt("\nSmall blocks test\n");

	region_t *region = (region_t *) ptr - 1;
	ASSERT("When requested size is too small, allocated size is bigger "
	       "than requested size",
	       region->size > 10);
	ASSERT("When requested size is too small, the block gets splitted into "
	       "2 regions",
	       (region_free_list->size + region->size + sizeof(region_t)) ==
	               BLOCK_SMALL);

	free(ptr);
}

void
allocating_zero_size_returns_NULL()
{
	void *ptr = malloc(0);

	ASSERT("When requested size is 0, malloc return NULL", ptr == NULL);
	free(ptr);
}

void
free_NULL_region_does_nothing()
{
	char *ptr = NULL;

	free(ptr);

	ASSERT("Freeing a NULL regions does nothing", ptr == NULL);
}

void
allocating_3_regions_4alligned_returns_3_directions_without_fragmentation()
{
	size_t regionSize = 1024;
	char string1[] = "Hola1";
	char string2[] = "Hola2";
	char string3[] = "Hola3";

	void *a1 = stralloc(regionSize, string1);
	void *a2 = stralloc(regionSize, string2);
	void *a3 = stralloc(regionSize, string3);

	printfmt("\nIts posible allocate %d regions of %ld size (4-alligned) "
	         "whitout fragmentantion..\n",
	         3,
	         regionSize);

	ASSERT("  The region: 1 is not NULL",
	       (a1 != NULL) && (strcmp(a1, string1) == 0));

	ASSERT("  The region: 2 is not NULL", a2 != NULL);
	ASSERT("\tThe region: 2 is not internally fragmented",
	       (a1 - a2) == (long int) (regionSize + sizeof(region_t)) &&
	               (strcmp(a2, string2) == 0));

	ASSERT("  The region: 3 is not NULL", a3 != NULL);
	ASSERT("\tThe region: 3 is not internally fragmented",
	       ((a2 - a3) == (long int) (regionSize + sizeof(region_t))) &&
	               (strcmp(a3, string3) == 0));

	free(a1);
	free(a2);
	free(a3);

	printfmt("\n");
}

void
allocating_3_regions_not_alligned_returns_3_directions_without_fragmentation()
{
	long unsigned int regionSize = 1000;
	char string1[] = "Hola1";
	char string2[] = "Hola2";
	char string3[] = "Hola3";

	void *a1 = stralloc(regionSize, string1);
	void *a2 = stralloc(regionSize, string2);
	void *a3 = stralloc(regionSize, string3);

	regionSize = ALIGN4(regionSize);
	printfmt("\nIts posible allocate %d regions of %ld size "
	         "(not-4-alligned), whitout fragmentantion..\n",
	         3,
	         regionSize);

	ASSERT("  The region: 1 is not NULL",
	       (a1 != NULL) && (strcmp(a1, string1) == 0));

	ASSERT("  The region: 2 is not NULL", a2 != NULL);
	ASSERT("\tThe region: 2 is not internally fragmented",
	       ((a1 - a2) == (long int) (regionSize + sizeof(region_t))) &&
	               (strcmp(a2, string2) == 0));

	ASSERT("  The region: 3 is not NULL", a3 != NULL);
	ASSERT("\tThe region: 3 is not internally fragmented",
	       ((a2 - a3) == (long int) (regionSize + sizeof(region_t))) &&
	               (strcmp(a3, string3) == 0));

	free(a1);
	free(a2);
	free(a3);

	printfmt("\n");
}

void
requesting_a_size_larger_than_the_biggest_block_with_malloc_returns_NULL()
{
	char string[] = "larger as limit block size\n";
	char *ptr = stralloc(BLOCK_BIG, string);

	ASSERT("Allocating a size larger than the block with malloc returns "
	       "NULL",
	       ptr == NULL);

	free(ptr);
}
void
freeing_after_allocating_10000_pointers_empties_the_free_list()
{
	size_t amount_of_pointers = 10000;
	void *ptrs[amount_of_pointers];
	size_t regionSize = 100;

	for (size_t i = 0; i < amount_of_pointers; i++) {
		ptrs[i] = malloc(regionSize);
		if (!ptrs[i]) {
			perror("Error allocating pointer");
			failed++;
			return;
		}
	}
	ASSERT("Allocating 10000 pointers populates the free list",
	       region_free_list != NULL);
	// DO NOT free everything in a single for loop,
	// we want to test that coalescing works correctly.
	for (size_t i = 0; i < amount_of_pointers / 2; i++) {
		for (size_t j = i; j < amount_of_pointers;
		     j += amount_of_pointers / 2) {
			free(ptrs[j]);
		}
	}
	ASSERT("Freeing the 10000 allocated pointers empties the free list",
	       region_free_list == NULL);
	// if (region_free_list){
	// 	int remaining_nodes = 0;
	// 	region_t *curr = region_free_list;
	// 	while (curr){
	// 		print_region(curr);
	// 		remaining_nodes++;
	// 		curr = curr->next;
	// 	}
	// 	printfmt("Remaining nodes: %d\n", remaining_nodes);
	// }
}

/* ----- more black box tests ----- */
// split (contiguous memory), free, coalesce _ single block
void
two_consecutive_mallocs_of_small_size_return_pointers_in_contiguous_memory()
{  // aka split works
	void *ptr1 = malloc(MIN_REGION_SIZE);
	void *ptr2 = malloc(20);
	// printfmt("ptr1: %p, prt2: %p\n",ptr1,ptr2);
	// ASSERT("Two consecutive small mallocs return pointers in contiguous
	// memory", ptr2 == (void *)((char *) ptr1 + MIN_REGION_SIZE)); ASSERT("Two
	// consecutive small mallocs return pointers in contiguous memory", ptr2
	// == (void *)((char *) ptr1 + MIN_REGION_SIZE+48));
	ASSERT("Two consecutive small mallocs return pointers in contiguous "
	       "memory",
	       ptr1 == (void *) ((char *) ptr2 + MIN_REGION_SIZE +
	                         sizeof(region_t)));
	free(ptr1);
	free(ptr2);
}

void
free_works()
{
	void *ptr1 = malloc(MIN_REGION_SIZE);
	free(ptr1);

	void *ptr2 = malloc(MIN_REGION_SIZE);
	ASSERT("Freed region can be used again for new same size malloc "
	       "request",
	       ptr2 == ptr1);

	free(ptr2);
}

void
coalesce_works()
{
	void *ptr1 = malloc(MIN_REGION_SIZE);
	void *ptr2 = malloc(MIN_REGION_SIZE + MIN_REGION_SIZE + 20);
	free(ptr1);
	free(ptr2);
	void *ptr3 = malloc(MIN_REGION_SIZE);
	ASSERT("Coalesced region can be used for bigger malloc request",
	       ptr3 == ptr1);
	free(ptr3);
}

// more than one block
void
request_that_doesnt_fit_in_current_block_gets_allocated_in_a_new_one()
{
	size_t size = MIN_REGION_SIZE + MIN_REGION_SIZE;
	void *ptr1 = malloc(BLOCK_SMALL - MIN_REGION_SIZE);
	void *ptr2 = malloc(size);

	ASSERT("Request that doesn't fit in current block gets allocated in a "
	       "new one",
	       (ptr2 != ptr1) && (ptr2 != ptr1 + size + sizeof(region_t)));
	free(ptr1);
	free(ptr2);
}

void
request_that_doesnt_fit_in_current_block_gets_allocated_in_a_new_block_of_correct_size_SMALL()
{
	void *ptr1 = malloc(BLOCK_SMALL / 3);
	void *ptr2 = malloc(BLOCK_SMALL / 3);
	void *ptr3 = malloc(BLOCK_SMALL / 3);
	// Alloco un bloque, ahora un segundo bloque que segpun lo pedido debe ser small
	void *ptrSecondSmall1 = malloc(BLOCK_SMALL / 3);
	void *ptrSecondSmall2 =
	        malloc(BLOCK_SMALL / 2);  // ya el siguiente malloc no entra acá

	void *ptrThirdBlock = malloc(BLOCK_SMALL / 2);

	printfmt("ptrSecondSmall1: %d, ptrSecondSmall2: %d \n",
	         ptrSecondSmall1,
	         ptrSecondSmall2);

	ASSERT("Request that doesn't fit in current block gets allocated in a "
	       "new block of correct size (SMALL)",
	       ptrSecondSmall2 !=
	               ptrThirdBlock + BLOCK_SMALL / 2 + sizeof(region_t));

	free(ptr1);
	free(ptr2);
	free(ptr3);
	free(ptrSecondSmall1);
	free(ptrSecondSmall2);
	free(ptrThirdBlock);
}

// coalesce múltiples bloques
void
coalescing_happens_only_inside_of_a_given_block()
{
	size_t size = BLOCK_SMALL / 3;
	void *ptrFirst1 = malloc(size);
	void *ptrFirst2 = malloc(size);

	void *ptrSecond1 = malloc(size);
	void *ptrSecond2 = malloc(size);

	free(ptrFirst1);
	free(ptrSecond1);
	// block1: | free | used | free | ;  block2: | free | used | free |
	// el nuevo malloc         ^ no empieza acá,                 ^ ni empieza acá.
	void *ptr = malloc(2 * size);

	ASSERT("Coalescing is only inside of a given block\n",
	       (ptrFirst2 != ptr + size + sizeof(region_t)) &&
	               (ptrSecond2 != ptr + size + sizeof(region_t)));

	free(ptrFirst2);
	free(ptrSecond2);
	free(ptr);
}

//******************CALLOC--TESTS**********************//

void
allocate_array_return_an_initialized_array()
{
	size_t cells = 10, cellSize = 5;
	bool initialized = true;
	char *ptr = NULL;

	printfmt(
	        "\nIt possible create an array of %d elements, with size: %d\n",
	        cells,
	        cellSize);

	ptr = calloc(cells, cellSize);

	ASSERT("  The calloc array is not NULL", ptr != NULL);

	if (!ptr)
		return;

	for (size_t i = 0; i < cells; ++i) {
		if (ptr[i] != 0) {
			printfmt(" Fail at cell: %d", i);
			ASSERT("", false);
			initialized = false;
		}
	}

	if (initialized)
		ASSERT("  All cells are initilizated", true);

	free(ptr);

	ASSERT("Freeing the only allocated (by calloc) pointer empties the "
	       "free list",
	       region_free_list == NULL);
	printfmt("\n");
}

void
allocating_zero_size_with_calloc_returns_NULL()
{
	void *ptr = calloc(10, 0);

	ASSERT("When requested size is 0, calloc return NULL", ptr == NULL);

	void *ptr2 = calloc(0, 10);
	ASSERT("When requested 0 elements, calloc return NULL", ptr2 == NULL);

	free(ptr);
	free(ptr2);
}

void
allocate_array_outs_of_int_limits_make_calloc_return_NULL()
{
	void *ptr = calloc(1, INT_MAX);

	printfmt("Calloc limits test\n");
	ASSERT("Allocate an array with INT_MAX positions of size 1 generate "
	       "overflow int and makes calloc return NULL",
	       ptr == NULL);

	ptr = calloc(INT_MAX, 1);
	ASSERT("Allocate an array with 1 position of size INT_MAX generate "
	       "overflow int and makes calloc return NULL",
	       ptr == NULL);

	ptr = calloc(3, INT_MAX / 2);
	ASSERT("Allocate an array with 3 positions of size (INT_MAX/2) "
	       "generate overflow int and makes calloc return NULL",
	       ptr == NULL);

	free(ptr);
}


//****************REALLOC--TESTS**********************//
void
reallocating_a_null_pointer_allocates_memory_of_requested_size()
{
	void *ptr = realloc(NULL, 100);
	region_t *reg = (region_t *) ptr - 1;

	ASSERT("Reallocating a null pointer is equivalent to malloc", ptr != NULL);
	ASSERT("Reallocating a null pointer, the returned pointer is of "
	       "expected size",
	       reg->size == MIN_REGION_SIZE);

	free(ptr);
}

void
reallocating_a_null_pointer_of_size_0_returns_a_null_pointer()
{
	void *ptr = realloc(NULL, 0);

	ASSERT("Reallocating a null pointer with size=0 returns a null pointer",
	       ptr == NULL);
}


void
reallocating_a_pointer_of_size_0_frees_the_pointer()
{
	int prev_frees = amount_of_frees;
	void *ptr = malloc(100);
	void *newptr = realloc(ptr, 0);

	ASSERT("Reallocating a valid pointer with size=0 frees the pointer",
	       newptr == NULL && region_free_list == NULL &&
	               amount_of_frees == prev_frees + 1);
}

void
reallocating_a_pointer_to_a_region_of_the_same_size_returns_the_pointer()
{
	void *ptr = malloc(MIN_REGION_SIZE);
	void *newptr = realloc(ptr, MIN_REGION_SIZE);

	ASSERT("Reallocating a pointer with size==old_size returns the pointer",
	       ptr == newptr);
	free(ptr);
}

void
reallocating_a_pointer_to_a_region_of_less_size_resizes_the_pointer()
{
	size_t size = 2000;
	char *str = malloc(size);
	char *str2 = malloc(size / 2);
	if (!str || !str2) {
		perror("Error allocating pointer");
		failed++;
		return;
	}

	for (size_t i = 0; i < size / 4; i++) {
		str[i] = (char) i;
	}
	for (size_t i = 0; i < size / 8; i++) {
		str2[i] = (char) i;
	}
	str[size / 4] = '\0';
	str2[size / 8] = '\0';

	char *newstr = realloc(str, size / 4);
	char *newstr2 = realloc(
	        str2,
	        size / 8);  // this should not split, because 250 < MIN_REGION_SIZE

	ASSERT("Reallocating a reusable pointer with less size does NOT "
	       "allocate a new "
	       "pointer",
	       newstr == str && newstr2 == str2);
	ASSERT("Reallocating a reusable pointer with less size does not change "
	       "its contents",
	       strcmp(str, newstr) == 0 && strcmp(str2, newstr2) == 0);
	free(newstr);
	free(newstr2);
	ASSERT("Freeing the only reusable pointers with less size empties the "
	       "free list",
	       region_free_list == NULL);
}

void
reallocating_a_valid_pointer_copies_its_contents()
{
	size_t size = 1000;
	char *str = malloc(size);
	for (size_t i = 0; i < size - 1; i++) {
		str[i] = (char) i;
	}
	str[size - 1] = '\0';

	char *newstr = realloc(str, size * 2);

	ASSERT("Reallocating a valid pointer allocates a NEW pointer",
	       newstr != str);
	ASSERT("Reallocating a valid pointer copies its contents to the new "
	       "pointer",
	       strcmp(str, newstr) == 0);
	free(newstr);
	ASSERT("Reallocating a valid pointer returns a new pointer and frees "
	       "the old pointer",
	       region_free_list == NULL);
}


void
reallocating_a_reusable_pointer_does_not_allocate_a_new_pointer()
{
	size_t size = 1000;
	void *ptr = malloc(size * 2);
	char *str = malloc(size);
	if (!ptr || !str) {
		perror("Error allocating pointer");
		failed++;
		return;
	}
	free(ptr);

	for (size_t i = 0; i < size - 1; i++) {
		str[i] = (char) i;
	}
	str[size - 1] = '\0';

	char *newstr = realloc(str, size + 1);

	ASSERT("Reallocating a reusable pointer does NOT allocate a new "
	       "pointer",
	       newstr == str);
	ASSERT("Reallocating a reusable pointer does not change its contents",
	       strcmp(str, newstr) == 0);
	free(newstr);
	ASSERT("Freeing the only reusable pointer empties the free list",
	       region_free_list == NULL);
}


//****************FREE--TESTS**********************//
void
freeing_an_invalid_pointer_does_nothing()
{
	int num = 10;
	free(&num);
	ASSERT("Freeing an invalid pointer does nothing", !is_valid_pointer(&num));
}


void
freeing_a_pointer_twice_does_nothing_the_second_time()
{
	void *ptr = malloc(100);
	void *ptr2 = malloc(100);
	if (!ptr || !ptr2) {
		perror("Error");
		failed++;
		return;
	}
	free(ptr);
	free(ptr);
	ASSERT("Freeing a pointer twice does nothing the second time",
	       !is_valid_pointer(ptr));
	free(ptr2);
}


int
main(void)
{
	printfmt("----------Regions tests----------\n\n");
	allocating_zero_size_returns_NULL();
	free_NULL_region_does_nothing();
	allocating_3_regions_4alligned_returns_3_directions_without_fragmentation();
	allocating_3_regions_not_alligned_returns_3_directions_without_fragmentation();
	requesting_a_size_larger_than_the_biggest_block_with_malloc_returns_NULL();

	printfmt("-----More Malloc tests _ black box tests-----\n\n");
	two_consecutive_mallocs_of_small_size_return_pointers_in_contiguous_memory();
	free_works();
	coalesce_works();
	request_that_doesnt_fit_in_current_block_gets_allocated_in_a_new_one();
	request_that_doesnt_fit_in_current_block_gets_allocated_in_a_new_block_of_correct_size_SMALL();
	coalescing_happens_only_inside_of_a_given_block();  //

	printfmt("\n----------Blocks tests-----------\n\n");
	free_after_malloc_frees_the_entire_block();
	allocating_too_little_returns_a_region_of_min_size();
	allocating_the_entire_block_empties_the_free_list();

	freeing_after_allocating_two_small_blocks_empties_the_free_list();  //
	freeing_after_allocating_10000_pointers_empties_the_free_list();    //

	printfmt("\n----------Calloc tests-----------\n\n");
	allocating_zero_size_with_calloc_returns_NULL();
	allocate_array_return_an_initialized_array();
	allocate_array_outs_of_int_limits_make_calloc_return_NULL();

	printfmt("\n----------Realloc tests-----------\n\n");

	reallocating_a_null_pointer_allocates_memory_of_requested_size();
	reallocating_a_null_pointer_of_size_0_returns_a_null_pointer();
	reallocating_a_pointer_of_size_0_frees_the_pointer();
	reallocating_a_pointer_to_a_region_of_the_same_size_returns_the_pointer();
	reallocating_a_pointer_to_a_region_of_less_size_resizes_the_pointer();
	reallocating_a_valid_pointer_copies_its_contents();
	reallocating_a_reusable_pointer_does_not_allocate_a_new_pointer();

	printfmt("\n----------Free tests-----------\n\n");

	freeing_an_invalid_pointer_does_nothing();
	freeing_a_pointer_twice_does_nothing_the_second_time();

	printfmt("\nTests: ran=%d, %spassed=%d%s, %sfailed=%d %s\n",
	         ran,
	         ANSI_COLOR_GREEN,
	         passed,
	         ANSI_COLOR_RESET,
	         ANSI_COLOR_RED,
	         failed,
	         ANSI_COLOR_RESET);


	return 0;
}
