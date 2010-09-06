
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

ERR_DECLARE(e_xcss_class);
ERR_DEFINE(e_xcss_class, "Class not found.", 0);

ERR_DECLARE(e_xcss_variable);
ERR_DEFINE(e_xcss_variable, "Variable not found.", 0);

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

typedef struct xcss_rule_ss {
	str_t name;
	str_t value;
	struct xcss_rule_ss *next;
} xcss_rule_s;

typedef xcss_rule_s *xcss_rule_t;

typedef struct xcss_class_ss {
	str_t name;
	str_t prefix;
	heap_t heap;
	xcss_rule_t first_rule;
	xcss_rule_t last_rule;
} xcss_class_s;

typedef xcss_class_s *xcss_class_t;

static xcss_class_t class_create(heap_t h, str_t nm, str_t prefix) {
	xcss_class_t cl = heap_alloc(h, sizeof(xcss_class_s));
	if(err())
		return 0;
	cl->name = nm;
	cl->prefix = prefix;
	cl->heap = h;
	cl->first_rule = cl->last_rule = 0;
	return 0;
}

static void class_append_rule(xcss_class_t cl, str_t nm, str_t val) {
	xcss_rule_t i, prev;
	xcss_rule_t r = heap_alloc(cl->heap, sizeof(xcss_rule_s));
	if(err())
		return;
	r->name = nm;
	r->value = val;
	r->next = 0;
	for(i=cl->first_rule, prev=0; i; prev=i, i=i->next) {
		if(str_equal(i->name, nm)) {
			if(!prev)
				cl->first_rule = i->next;
			else if(!i->next) {
				cl->last_rule = prev;
				prev->next = 0;
			} else
				prev->next = i->next;
			break;
		}
	}
	if(cl->last_rule) {
		cl->last_rule->next = r;
		cl->last_rule = r;
	} else
		cl->last_rule = cl->first_rule = r;
	return;
}

void class_write(xcss_class_t cl, FILE *f) {
	xcss_rule_t i;
	fwrite(".", 1, 1, f);
	if(cl->prefix)
		fwrite(str_begin(cl->prefix), str_length(cl->prefix), 1, f);
	fwrite(str_begin(cl->name), str_length(cl->name), 1, f);
	fprintf(f, " {\n");
	for(i=cl->first_rule; i; i=i->next) {
		fwrite("\t", 1, 1, f);
		fwrite(str_begin(i->name), str_length(i->name), 1, f);
		fprintf(f, ": ");
		fwrite(str_begin(i->value), str_length(i->value), 1, f);
		fprintf(f, ";\n");
	}
	fprintf(f, "}\n\n");
}

static void class_append_class(xcss_class_t cl, xcss_class_t p) {
	if(p) {
		xcss_rule_t i;
		for(i=p->first_rule; i; i=i->next) {
			class_append_rule(cl, i->name, i->value);
			if(err())
				return;
		}
	}
}

typedef struct xcss_ns_ss {
	map_t classes;
	map_t vars;
	struct xcss_ns_ss *parent;
} xcss_ns_s;

typedef xcss_ns_s *xcss_ns_t;

static xcss_ns_t ns_create(heap_t h, xcss_ns_t p) {
	xcss_ns_t r = heap_alloc(h, sizeof(xcss_ns_s));
	r->parent = p;
	if(err())
		return 0;
	r->classes = map_create(h);
	if(err())
		return 0;
	r->vars = map_create(h);
	if(err())
		return 0;
	return r;
}

static void ns_add_var(xcss_ns_t ns, str_t nm, str_t vl) {
	map_set(ns->vars, nm, vl);
}

static str_t ns_get_var(xcss_ns_t ns, str_t nm) {
	for(; ns; ns=ns->parent) {
		str_t r = map_get(ns->vars, nm);
		if(r)
			return r;
	}
	return 0;
}

static void ns_add_class(xcss_ns_t ns, xcss_class_t vl) {
	map_set(ns->classes, vl->name, vl);
}


static xcss_class_t ns_get_class(xcss_ns_t ns, str_t nm) {
	for(; ns; ns=ns->parent) {
		xcss_class_t r = map_get(ns->classes, nm);
		if(r)
			return r;
	}
	return 0;
}


static str_t get_rule_value(heap_t h, xcss_ns_t ns, syntree_node_t nd, FILE *serr) {
	str_t r;
	assert(syntree_name(nd)==XCSS_NODE_VALUE);
	r = str_from_cs(h, "");
	if(err())
		return 0;
	for(nd=syntree_child(nd); nd; nd=syntree_next(nd)) {
		str_t s = syntree_value(nd);
		if(err())
			return 0;
		if(syntree_name(nd)!=XCSS_NODE_TEXT) {
			r = str_cat(h, r, s);
			if(err())
				return 0;
		} else { /* XCSS_NODE_NAME */
			str_t c = ns_get_var(ns, s);
			if(err())
				return 0;
			if(!c) {
				fprintf(serr, "Variable \"");
				fwrite(str_begin(c), str_length(c), 1, serr);
				fprintf(serr, "\" not found.\n");
				err_set(e_xcss_variable);
				return 0;
			} else {
				r = str_cat(h, r, c);
				if(err())
					return 0;
			}
		}
	}
}

static void xcss_process_node(heap_t h,
							  syntree_node_t stn,
							  xcss_ns_t ns,
							  str_t fprefix,
							  str_t name_prefix,
							  FILE *sout,
							  FILE *serr) {
	switch(syntree_name(stn)) {
		case XCSS_NODE_NAMESPACE: {
			str_t nmp2;
			xcss_ns_t ns2 = ns_create(h, ns);
			if(err())
				return;
			stn = syntree_child(stn);
			assert(syntree_name(stn)==XCSS_NODE_NAME);
			nmp2 = syntree_value(stn);
			if(err())
				return;
			if(name_prefix) {
				str_t tmp = str_from_cs(h, "-");
				if(err())
					return;
				nmp2 = str_cat(h, tmp, nmp2);
				if(err())
					return;
				nmp2 = str_cat(h, name_prefix, nmp2);
				if(err())
					return;
			}
			for(stn=syntree_next(stn); stn; stn=syntree_next(stn)) {
				xcss_process_node(h, stn, ns2, fprefix, nmp2, sout, serr);
				if(err())
					return;
			}
			break;
		}
		case XCSS_NODE_CLASS: {
			xcss_class_t cl;
			str_t tmp;
			stn = syntree_child(stn);
			assert(syntree_name(stn)==XCSS_NODE_CLASS_NAME);
			tmp = syntree_value(stn);
			if(err())
				return;
			cl = class_create(h, tmp, name_prefix);
			if(err())
				return;
			ns_add_class(ns, cl);
			if(err())
				return;
			stn = syntree_next(stn);
			if(syntree_name(stn)==XCSS_NODE_CLASS_PARENT) {
				syntree_node_t i;
				for(i=syntree_child(stn); i; i=syntree_next(i)) {
					xcss_class_t pc;
					tmp = syntree_value(i);
					if(err())
						return;
					pc = ns_get_class(ns, tmp);
					if(pc) {
						class_append_class(cl, pc);
						if(err())
							return;
					} else {
						fprintf(serr, "Class \"");
						fwrite(str_begin(tmp), str_length(tmp), 1, serr);
						fprintf(serr, "\" not found.\n");
						err_set(e_xcss_class);
						return;
					}
				}
				stn = syntree_next(stn);
			}
			for(; stn; syntree_next(stn)) {
				str_t nm, vl;
				syntree_node_t i = syntree_child(stn);
				assert(syntree_name(i)==XCSS_NODE_NAME);
				nm = syntree_value(i);
				if(err())
					return;
				i = syntree_next(i);
				vl = get_rule_value(h, ns, i, serr);
				if(err())
					return;
				class_append_rule(cl, nm, vl);
			}
			class_write(cl, sout);
			ns_add_class(ns, cl);
			break;
		}
		case XCSS_NODE_RULE: {
			str_t nm, vl;
			syntree_node_t i = syntree_child(stn);
			assert(syntree_name(i)==XCSS_NODE_NAME);
			nm = syntree_value(i);
			if(err())
				return;
			i = syntree_next(i);
			vl = get_rule_value(h, ns, i, serr);
			if(err())
				return;
			ns_add_var(ns, nm, vl);
			break;
		}
		case XCSS_NODE_INCLUDE: {
			break;
		}
	}
}


int main() {
	return 0;
}

