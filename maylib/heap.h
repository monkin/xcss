#ifndef MAY_HEAP_H
#define MAY_HEAP_H

#include "mem.h"
#include <stddef.h>
#include <stdio.h>

typedef struct heap_block_s {
	size_t size;
	size_t used;
	struct heap_block_s *next;
	char data[1];
} heap_block_t;

typedef struct {
	size_t block_size;
	heap_block_t *last;
	heap_block_t first;
} heap_s;

typedef heap_s *heap_t;

heap_t heap_create(size_t block_size);
heap_t heap_delete(heap_t);

/* void *heap_alloc(heap_t, size_t); */
void *heap_slow_alloc(heap_t, size_t);
#define heap_alloc(h, sz) ((((h)->last->size - (h)->last->used)>=(sz)) \
	? (((h)->last->used+=(sz)),&((h)->last->data[(h)->last->used-(sz)])) \
	: heap_slow_alloc(h,(sz)))


#endif /* MAY_HEAP_H */



