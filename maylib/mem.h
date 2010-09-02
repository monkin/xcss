#ifndef MAY_MEM_H
#define MAY_MEM_H

#include "err.h"
#include <stddef.h>

ERR_DECLARE(e_out_of_memory);

void *mem_alloc(size_t);
void *mem_free(void *);
void *mem_realloc(void *, size_t);


#endif /* MAY_MEM_H */
