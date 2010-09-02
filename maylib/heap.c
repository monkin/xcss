#include "heap.h"
#include "mem.h"
#include <assert.h>

heap_t heap_create(size_t block_size) {
	heap_t res;
	err_reset();
	if(block_size==0)
		block_size = 64*1024;
	res = mem_alloc(sizeof(heap_s) + block_size);
	if(!err()) {
		res->block_size = block_size;
		res->last = &res->first;
		res->first.size = block_size;
		res->first.used = 0;
		res->first.next = 0;
	}
	return res;
}

heap_t heap_delete(heap_t h) {
	if(h) {
		heap_block_t *p = h->first.next;
		while(p) {
			heap_block_t *tmp = p->next;
			mem_free(p);
			p = tmp;
		}
		mem_free(h);
	}
	return 0;
}

void *heap_slow_alloc(heap_t h, size_t sz) {
	heap_block_t *b;
	size_t block_sz = (sz*16<=h->block_size) ? h->block_size : sz*16;
	b = h->last->next = mem_alloc(sizeof(heap_s) + block_sz);
	if(!err()) {
		h->last = h->last->next;
		b->size = block_sz;
		b->used = sz;
		b->next = 0;
		return &(b->data[0]);
	}
	return 0;
}




