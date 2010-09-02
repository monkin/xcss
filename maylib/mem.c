#include "mem.h"
#include <stdlib.h>

ERR_DEFINE(e_out_of_memory, "Out of memory", 0);

void *mem_alloc(size_t sz) {
	void *res;
	err_reset();
	if(!sz)
		return 0;
	res = malloc(sz);
	if(!res)
		err_set(e_out_of_memory);
	return res;
}

void *mem_free(void *p) {
	free(p);
	return 0;
}

void *mem_realloc(void *p, size_t sz) {
	err_reset();
	if(!sz) {
		free(p);
		return 0;
	} else {
		void *res = realloc(p, sz);
		if(!res) {
			err_set(e_out_of_memory);
			return p;
		}
		return res;
	}
}
