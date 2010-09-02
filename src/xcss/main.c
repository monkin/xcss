
#include "maylib/err.h"
#include "maylib/str.h"
#include "maylib/heap.h"
#include "maylib/map.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

ERR_DECLARE(e_xcss_syntax);
ERR_DEFINE(e_xcss_syntax, "XCSS syntax error.", 0);

ERR_DECLARE(e_xcss_io_error);
ERR_DEFINE(e_xcss_io, "IO error.", 0);


struct xcss_rule_s {
	str_t name;
	map_t values;
	struct xcss_rule_s *next;
};

typedef struct xcss_rule_s *xcss_rule_t;

struct xcss_context_s {
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



static void p_send_error(heap_t h, str_t xcss, str_t fname, int pos, void (*warning)(str_t)) {
	if(warning) {
		str_t msg;
		sbuilder_t sb;
		err_reset();
		sb = sbuilder_create(h);
		if(sb) {
			sbuilder_append(sb, str_from_cs(h, "Syntax error at: "));
			sbuilder_append(sb, fname);
			sbuilder_append(sb, str_from_cs(h, ":"));
			sbuilder_append(sb, str_from_int(h, pos));
			msg = sbuilder_get(h, sb);
			if(!err())
				warning(msg);
		}
	}
}


#define p_skip(i, e, ex) \
while((i)<(e) && (ex))   \
	i++;

#define p_skip_spaces(i, e) p_skip(i,e,isspace(*i))

xcss_context_t xcss_parse(heap_t h,
		xcss_context_t ctx,
		str_t fname, 
		void (*warning)(str_t)) {
	str_t xcss = 0;
	assert(h && fname);
	do {
		size_t sz;
		FILE *f = fopen(str_begin(fname), "r");
		if(!f) {
			err_set(e_xcss_io);
			goto clean;
		}
		fseek(f, 0L, SEEK_END);
		sz = ftell(f);
		fseek(f, 0L, SEEK_SET);
		xcss = str_create(h, sz);
		if(err())
			goto clean;
		if(fread(str_begin(xcss), sz, 1, f)!=sz)
			err_set(e_xcss_io);
	clean:
		fclose(f);
		if(err())
			return ctx;
	} while(0);
	if(!err()) do {
		str_it_t se = str_end(xcss);
		str_it_t i = str_begin(xcss), j = 0;
		if(!ctx) {
			ctx = (xcss_context_t) heap_alloc(h, sizeof(struct xcss_context_s));
			if(err())
				return ctx;
			ctx->vars = map_create(h);
			if(err())
				return 0;
			ctx->name = 0;
			ctx->file_name = fname;
			ctx->parent = 0;
			ctx->first_child = 0;
			ctx->last_child = 0;
			ctx->next = 0;
		}
		for(j=i; j<str_end(xcss); j++) {
			switch(*j) {
				case '=': { /* variable definition */
					str_t name;
					str_t value;
					p_skip_spaces(i, se);
					p_skip(j, se, isalnum(*j) || *j=='_');
					name = str_interval(h,i,j);
					i=j;
					p_skip_spaces(i, se);
					if(*i!='=') {
						p_send_error(h, xcss, fname, i-str_begin(xcss), warning);
						err_set(e_xcss_syntax);
						return ctx;
					}
					i++;
					p_skip_spaces(i, se);
					j = i;
					p_skip(j, se, *j!=';');
					if(j==se) {
						p_send_error(h, xcss, fname, i-str_begin(xcss), warning);
						err_set(e_xcss_syntax);
						return ctx;
					}
					value = str_interval(h, i, j);
					if(err())
						return ctx;
					i = j+1;
					j = i;
				}
				case '[': { /* namespace */
					str_t name;
					p_skip_spaces(i, se);
					j = i;
					p_skip(j, se, isalnum(*j) || *j=='_');
					name = str_interval(h, i, j);
					p_skip_spaces(j, se);
					if(j==se) {
						p_send_error(h, xcss, fname, j-str_begin(xcss), warning);
						err_set(e_xcss_syntax);
						return ctx;
					}
					
				}
				case ']': /* namespace end */
					break;
				case '{': /* class */
					break;
				case '/': /* comment */
					break;
				case ';': /* include */
					break;
			}
		}
	} while(0);
	return ctx;
}

str_t xcss_to_str(heap_t h, xcss_rule_t rules) {
	return 0;
}

int main() {
	return 0;
}
