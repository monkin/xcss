
#include "parser.h"
#include "syntree.h"
#include "maylib/err.h"
#include "maylib/str.h"
#include "maylib/heap.h"
#include "maylib/map.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>



ERR_DECLARE(e_xcss_io_error);
ERR_DEFINE(e_xcss_io, "IO error.", 0);


struct xcss_rule_s {
	str_t name;
	map_t values;
	struct xcss_rule_s *next;
};

typedef struct xcss_rule_s *xcss_rule_t;

struct xcss_context_s {
	heap_t heap;
	str_t name;
	map_t vars;
	xcss_rule_t rules;
	str_t file_name;
	struct xcss_context_s *parent;
	struct xcss_context_s *first_child;
	struct xcss_context_s *last_child;
	struct xcss_context_s *next;
};

typedef struct xcss_context_s *xcss_context_t;

static str_t read_file(heap_t h, str_t fname) {
	size_t sz;
	str_t content;
	err_reset();
	FILE *f = fopen(str_begin(fname), "r");
	if(!f) {
		err_set(e_xcss_io);
		goto clean;
	}
	fseek(f, 0L, SEEK_END);
	sz = ftell(f);
	fseek(f, 0L, SEEK_SET);
	content = str_create(h, sz);
	if(err())
		goto clean;
	if(fread(str_begin(content), sz, 1, f)!=sz)
		err_set(e_xcss_io);
clean:
	fclose(f);
	return err() ? 0 : content;
}

static void xcss_process_namespace(xcss_context_t ctx, syntree_node_t nd) {
	
}

static void xcss_process_rule(xcss_context_t ctx, syntree_node_t nd) {

}

static void xcss_process_class(xcss_context_t ctx, syntree_node_t nd) {

}

static void xcss_process_include(xcss_context_t ctx, syntree_node_t nd) {
	str_t fname, content;
	nd = syntree_child(nd);
	assert(syntree_name(nd)==XCSS_NODE_INCLUDE_NAME);
	
	/*read_file();*/
}

static void xcss_process(xcss_context_t ctx, syntree_node_t nd) {
	for(; nd; nd=syntree_next(nd)) {
		switch(syntree_name(nd)) {
			case XCSS_NODE_NAMESPACE:
				xcss_process_namespace(ctx, nd);
				break;
			case XCSS_NODE_CLASS:
				xcss_process_class(ctx, nd);
				break;
			case XCSS_NODE_RULE:
				xcss_process_rule(ctx, nd);
				break;
			case XCSS_NODE_INCLUDE:
				xcss_process_include(ctx, nd);
				break;
		}
	}	
}



static xcss_context_t xcss_process_file(heap_t h, xcss_context_t parent, str_t fname) {
	syntree_t st;
	str_t xcss = read_file(h, fname);
	if(err())
		return 0;
	st = xcss_to_syntree(h, xcss);
	if(err()) {
		return 0;
	}
}



int main() {
	return 0;
}

