#ifndef MAY_PARSER_H
#define MAY_PARSER_H

#include "maylib/err.h"
#include "maylib/heap.h"
#include "maylib/str.h"
#include "syntree.h"

ERR_DECLARE(e_xcss_syntax);

typedef enum {
	XCSS_NODE_NAME = 1,
	XCSS_NODE_NAMESPACE = 2,
	XCSS_NODE_CLASS = 3, /* CLASS_NAME ("(" (XCSS_NODE_CLASS_NAME ",")+ ")")? { RULE* } */
	XCSS_NODE_CLASS_NAME = 4,
	XCSS_NODE_CLASS_PARENT = 5, /* "(" (XCSS_NODE_CLASS_NAME ",")+ ")" */
	XCSS_NODE_RULE = 6, /* NAME ":" VALUE ";" */
	XCSS_NODE_VALUE = 7, /* (TEXT|NAME)* */
	XCSS_NODE_TEXT = 8,
	XCSS_NODE_INCLUDE = 9,
	XCSS_NODE_INCLUDE_NAME = 10,
	XCSS_NODE_COMMENT = 11
} xcss_node_type_t;

syntree_t xcss_to_syntree(heap_t, str_t);


#endif /* MAY_PARSER_H */

