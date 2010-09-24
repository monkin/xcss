
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
#include <errno.h>
#include <string.h>

ERR_DECLARE(e_xcss_io_error);
ERR_DEFINE(e_xcss_io, "IO error.", 0);

ERR_DECLARE(e_xcss_class);
ERR_DEFINE(e_xcss_class, "Class not found.", 0);

ERR_DECLARE(e_xcss_variable);
ERR_DEFINE(e_xcss_variable, "Variable not found.", 0);

#define FILE_BLOCK_SIZE (1024*64)

static str_t read_stream(heap_t h, FILE *f) {
	size_t sz;
	heap_t tmph;
	sbuilder_t sb;
	str_t r = 0;
	assert(h && f);
	tmph = heap_create(1024*64*4);
	if(err())
		return 0;
	sb = sbuilder_create(tmph);
	if(err())
		return 0;
	do {
		str_t s = str_create(tmph, FILE_BLOCK_SIZE);
		if(err())
			goto clean;
		sz = fread(str_begin(s), 1, FILE_BLOCK_SIZE, f);
		if(sz) {
			str_t tmp = str_interval(tmph, str_begin(s), str_begin(s) + sz);
			if(err())
				goto clean;
			sbuilder_append(sb, tmp);
			if(err())
				goto clean;
		}
	} while(sz==FILE_BLOCK_SIZE);
	r = sbuilder_get(h, sb);
clean:
	heap_delete(tmph);
	return r;
}

static str_t read_file(heap_t h, str_t fname) {
	size_t sz;
	str_t content;
	err_reset();
	fname = str_clone(h, fname);
	if(err())
		return 0;
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
	if(fread(str_begin(content), 1, sz, f)!=sz)
		err_set(e_xcss_io);
clean:
	if(f)
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
	return cl;
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
		if(syntree_name(nd)==XCSS_NODE_TEXT) {
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
	return r;
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
			str_t tmp = str_from_cs(h, "-");
			if(err())
				return;
			nmp2 = str_cat(h, nmp2, tmp);
			if(err())
				return;
			if(name_prefix) {
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
			for(; stn; stn=syntree_next(stn)) {
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
			str_t fname, cnt;
			str_it_t si;
			syntree_t st;
			syntree_node_t i = syntree_child(stn);
			assert(syntree_name(i)==XCSS_NODE_INCLUDE_NAME);
			fname = syntree_value(i);
			if(err())
				return;
			if(fprefix)
				fname = str_cat(h, fprefix, fname);
			if(err())
				return;
			for(si=str_end(fname)-1; si>str_begin(fname); si--) {
				if(*si=='/') {
					fprefix = str_interval(h, str_begin(fname), si+1);
					if(err())
						return;
				}
			}
			cnt = read_file(h, fname);
			if(err())
				return;
			st = xcss_to_syntree(h, cnt);
			if(err())
				return;
			for(i=syntree_begin(st); i; i=syntree_next(i)) {
				xcss_process_node(h, i, ns, fprefix, name_prefix, sout, serr);
				if(err())
					return;
			}
		}
	}
}


int main(int nargs, char **args) {
	str_t cnt;
	heap_t h;
	syntree_t st;
	syntree_node_t i;
	xcss_ns_t ns;
	FILE *out;
	char *file_name = 0;
	out = stderr = stdout;
	int a;
	for(a=0; a<nargs; a++) {
		if(strcmp(args[a], "-h")==0 || strcmp(args[a], "--help")==0) {
			printf("XCSS processor\n");
			printf("Arguments:\n");
			printf("\t-h, --help     show this help and exit\n");
			printf("\t-o             output file\n");
			printf("\t-i             input file\n");
			return 0;
		} else if(strcmp(args[a], "-o")==0) {
			if((a+1)>=nargs) {
				fprintf(stderr, "Invalid argument. File name expected after -o.\nUse --help option for more information.\n");
				return -1;
			} else {
				a++;
				out = fopen(args[a], "w");
				if(!out) {
					fprintf(stderr, "Can\'t create output file \"%s\"", args[a]);
					return -1;
				}
			}
		} else if(strcmp(args[a], "-i")==0) {
			if((a+1)>=nargs) {
				fprintf(stderr, "Invalid argument. File name expected after -i.\nUse --help option for more information.\n");
				return -1;
			} else {
				a++;
				file_name = args[a];
			}
		}
	}
	h = heap_create(1024*64);
	if(err())
		goto error;
	if(!file_name) {
		cnt = read_stream(h, stdin);
		if(err())
			goto error;
	} else {
		cnt = str_from_cs(h, file_name);
		if(err())
			goto error;
		cnt = read_file(h, cnt);
		if(err())
			goto error;
	}
	st = xcss_to_syntree(h, cnt);
	if(err())
		goto error;
	ns = ns_create(h, ns);
	for(i=syntree_begin(st); i; i=syntree_next(i)) {
		xcss_process_node(h, i, ns, 0, 0, out, stderr);
		if(err())
			goto error;
	}
	h = heap_delete(h);
	if(out!=stdout)
		fclose(out);
	return 0;
error:
	err_reset();
	heap_delete(h);
	return -1;
}

