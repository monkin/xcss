#ifndef MAY_UTF_H
#define MAY_UTF_H

#include "heap.h"
#include "err.h"

#define UTF_8     1
#define UTF_16_LE 2
#define UTF_16_BE 3
#define UTF_32_LE 4
#define UTF_32_BE 5

void *utf_convert(heap_t h, void *src, int src_enc, int dest_enc);
/**
 * Return UTF_??? or zero if can't detect encoding type.
 */
int utf_detect(void *, size_t);
size_t utf_length(void *s, int enc);
size_t utf_char_length(void *s, int enc);

ERR_DECLARE(e_utf_conversion);



#endif /* MAY_UTF_H */


