#include "utf.h"
#include "str.h"
#include "mem.h"
#include "err.h"
#include <assert.h>

ERR_DEFINE(e_utf_conversion, "Invalid UTF string or encoding.", 0);

#define C_BETWEEN(p, c1, c2) (*((unsigned char *)(p))>=(c1) && *((unsigned char *)(p))<=(c2))
#define C_OFFSET(p,s) (((unsigned char*)(p))+(s))
#define C_LESS(p,c) (*((unsigned char *)(p))<(c))
#define C_TO_LONG(p, offset) ((unsigned long)(*((unsigned char *)((p)+(offset)))))

#define CHAR_LEN(p, enc) (((enc)==UTF_32_LE || (enc)==UTF_32_BE) ? 4 : (               \
		((enc)==UTF_16_BE) ? (C_BETWEEN((p), 0xD8, 0xDB)?4:2) : (                      \
			((enc)==UTF_16_LE) ? (C_BETWEEN(C_OFFSET((p),1), 0xD8, 0xDB)?4:2) : (      \
				C_LESS((p),0x80)? 1 : (                                                \
					C_LESS((p),0xE0) ? 2 : (                                           \
						C_LESS((p),0xF0) ? 3 : 4                                       \
					)                                                                  \
				)                                                                      \
			)                                                                          \
		)                                                                              \
	)                                                                                  \
)

#define CHAR_IS_LAST(p, enc) (							\
	((enc)==UTF_32_LE || (enc)==UTF_32_BE) ? (			\
		(*((long *)(p)))==0								\
	) : (												\
		((enc)==UTF_16_LE || (enc)==UTF_16_BE) ? (		\
			(*((short *)(p)))==0						\
		) : (											\
			(*((char *)(p)))==0							\
		)												\
	)													\
)

#define CHAR_TO_LONG(p, enc) (((enc)==UTF_32_BE) ? (													\
		(C_TO_LONG((p),0)<<24)|(C_TO_LONG((p),1)<<16)|(C_TO_LONG((p),2)<<8)|C_TO_LONG((p),3)			\
	) : (																								\
		(enc==UTF_32_LE) ? (																			\
			(C_TO_LONG((p),3)<<24)|(C_TO_LONG((p),2)<<16)|(C_TO_LONG((p),1)<<8)|C_TO_LONG((p),0)		\
		) : (																							\
			(enc==UTF_16_LE) ? (																		\
				C_BETWEEN(C_OFFSET((p),1),0xD8,0xDB) ? (												\
					(((C_TO_LONG((p),0) | (C_TO_LONG((p),1)<<8))-0xD800)<<10)							\
						+ ((C_TO_LONG((p),2) | (C_TO_LONG((p),3)<<8))-0xD800)							\
						+ 0x10000																		\
				) : (																					\
					C_TO_LONG((p),0) | (C_TO_LONG((p),1)<<8)											\
				)																						\
			) : (																						\
				(enc==UTF_16_BE) ? (																	\
					C_BETWEEN((p),0xD8,0xDB) ? (														\
						(((C_TO_LONG((p),1) | (C_TO_LONG((p),0)<<8))-0xD800)<<10)						\
							+ ((C_TO_LONG((p),3) | (C_TO_LONG((p),2)<<8))-0xD800)						\
							+ 0x10000																	\
					) : (																				\
						C_TO_LONG((p),1) | (C_TO_LONG((p),0)<<8)										\
					)																					\
				) : (																					\
					C_LESS((p),0x80) ? C_TO_LONG((p),0) : (												\
						C_LESS((p),0xE0) ? (															\
							((C_TO_LONG((p),0)&0x1F)<<6) + (C_TO_LONG((p),1)&0x3F)						\
						) : (																			\
							C_LESS((p),0xF0) ? ( /*3*/													\
								((C_TO_LONG((p),0)&0x0F)<<12) 											\
									| ((C_TO_LONG((p),1)&0x3F)<<6) 										\
									| (C_TO_LONG((p),2)&0x3F)											\
							) : ( /*4*/																	\
								((C_TO_LONG((p),0)&0x07)<<18) 											\
									| ((C_TO_LONG((p),1)&0x3F)<<12)										\
									| ((C_TO_LONG((p),2)&0x3F)<<6) 										\
									| (C_TO_LONG((p),2)&0x3F)											\
							)																			\
						)																				\
					)  																					\
				)																						\
			)																							\
		)																								\
	)																									\
)

#define UTF_WRITE1(dest, c1) ( \
	((*C_OFFSET(dest, 0))=c1), \
	C_OFFSET(dest, 1)          \
)

#define UTF_WRITE2(dest, c1, c2) (      \
	((*C_OFFSET(dest, 0))=c1),          \
	((*C_OFFSET(dest, 1))=c2),          \
	C_OFFSET(dest, 2)                   \
)

#define UTF_WRITE3(dest, c1, c2, c3) (  \
	((*C_OFFSET(dest, 0))=c1),		 	\
	((*C_OFFSET(dest, 1))=c2),			\
	((*C_OFFSET(dest, 2))=c3),			\
	C_OFFSET(dest, 3)					\
)

#define UTF_WRITE4(dest, c1, c2, c3, c4) (		\
	((*C_OFFSET(dest, 0))=c1), 					\
	((*C_OFFSET(dest, 1))=c2),					\
	((*C_OFFSET(dest, 2))=c3),					\
	((*C_OFFSET(dest, 3))=c4),					\
	C_OFFSET(dest, 4)							\
)

#define LONG_TO_UTF(c, dest, enc)									\
	((enc)==UTF_32_LE) ? (											\
		UTF_WRITE4(													\
			(dest),													\
			(c)&0xFF,												\
			((c)>>8)&0xFF,											\
			((c)>>16)&0xFF,											\
			((c)>>24)&0xFF											\
		)															\
	) : (															\
		((enc)==UTF_32_BE) ? (										\
			UTF_WRITE4(												\
				(dest), 											\
				((c)>>24)&0xFF,										\
				((c)>>16)&0xFF, 									\
				((c)>>8)&0xFF, 										\
				(c)&0xFF											\
			)														\
		) : (														\
			((enc)==UTF_16_LE) ? (									\
				UTF_WRITE2((dest), (c)&0xFF, ((c)>>8)&0xFF)			\
			) : (													\
				((enc)==UTF_16_BE) ? (								\
					UTF_WRITE2((dest), ((c)>>8)&0xFF, (c)&0xFF)		\
				) : (												\
					((c)<=0x7f) ? (									\
						UTF_WRITE1((dest), (c))						\
					) : (											\
						((c)<=0x07FF) ? (							\
							UTF_WRITE2(								\
								(dest), 							\
								(((c)>>6)&0x1F)|0xC0, 				\
								((c)&0x3F) | 0x80					\
							)										\
						) : (										\
							((c)<=0xFFFF) ? (						\
								UTF_WRITE3(							\
									(dest),							\
									(((c)>>12)&0x0F)|0xE0,			\
									(((c)>>6)&0x3F) | 0x80,			\
									((c)&0x3F) | 0x80				\
								)									\
							) : (									\
								UTF_WRITE4(							\
									(dest), 						\
									(((c)>>18)&0x07)|0xF0, 			\
									(((c)>>12)&0x3F) | 0x80, 		\
									(((c)>>6)&0x3F) | 0x80, 		\
									((c)&0x3F) | 0x80				\
								)									\
							)										\
						)											\
					)												\
				)													\
			)														\
		)															\
	)


size_t utf_length(void *s, int enc) {
	int res = 0;
	if(!s)
		return 0;
	for(; !CHAR_IS_LAST(s, enc); s = C_OFFSET(s, CHAR_LEN(s,enc)), res++);
	return res;
}

size_t utf_char_length(void *s, int enc) {
	assert(s);
	return CHAR_LEN(s,enc);
}

/*
define UTF_8     1
define UTF_16_LE 2
define UTF_16_BE 3
define UTF_32_LE 4
define UTF_32_BE 5
*/


void *utf_convert(heap_t h, void *src, int src_enc, int dest_enc) {
	void *res;
	void *i;
	int len;
	if(!src)
		return 0;
	err_reset();
	len = utf_length(src, src_enc) + 1;
	i = res = heap_alloc(h, len*4);
	if(err())
		return 0;
	while(!CHAR_IS_LAST(src, src_enc)) {
		long c = CHAR_TO_LONG(src, src_enc);
		src = C_OFFSET(src, CHAR_LEN(src, src_enc));
		i = LONG_TO_UTF(c, i, dest_enc);
	}
	i = UTF_WRITE4(i, 0, 0, 0, 0);
	return res;
}

int utf_detect(void *s, size_t sz) {
	return 0;
}
