
#include "syntree.h"
#include "maylib/err.h"
#include "maylib/str.h"
#include "maylib/heap.h"
#include "maylib/map.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>

ERR_DECLARE(e_xcss_syntax);
ERR_DEFINE(e_xcss_syntax, "XCSS syntax error.", 0);

ERR_DECLARE(e_xcss_io_error);
ERR_DEFINE(e_xcss_io, "IO error.", 0);


struct xcss_rule_s {
	str_t name;
	map_t values;
	struct xcss_rule_s *next;
};

typedef enum {
	XCSS_NODE_NAME = 4,
	XCSS_NODE_NAMESPACE = 2,
	XCSS_NODE_CLASS = 3, /* CLASS_NAME ("(" (XCSS_NODE_CLASS_NAME ",")+ ")")? { RULE* } */
	XCSS_NODE_CLASS_NAME = 4,
	XCSS_NODE_CLASS_PARENT = 8, /* "(" (XCSS_NODE_CLASS_NAME ",")+ ")" */
	XCSS_NODE_RULE = 9, /* NAME ":" VALUE ";" */
	XCSS_NODE_VALUE = 5, /* (TEXT|NAME)* */
	XCSS_NODE_TEXT = 6,
	XCSS_NODE_INCLUDE = 10,
	XCSS_NODE_INCLUDE_NAME = 11,
	XCSS_NODE_COMMENT = 12
} xcss_node_type_t;

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

void xcss_parse(syntree_t res);
static void parse_node_comment(syntree_t st);

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

static xcss_node_type_t get_node_type(str_it_t i, str_it_t e) {
	for(; i<e; i++) {
		switch(*i) {
			case ':':
				return XCSS_NODE_RULE;
			case '[':
				return XCSS_NODE_NAMESPACE;
			case '{':
				return XCSS_NODE_CLASS;
			case '/':
				return XCSS_NODE_COMMENT;
			case ';':
				return XCSS_NODE_INCLUDE;
		}
	}
	return 0;
}

static void parse_node_name(syntree_t st) {
	syntree_named_start(st, XCSS_NODE_NAME);
	if(err())
		return;
	else {
		str_it_t i, e;
		i = syntree_position(st);
		e = str_end(syntree_str(st));
		p_skip(i, e, isalnum(*i) || (*i=='_') || (*i=='-'));
		if(i==e) {
			err_set(e_xcss_syntax);
		} else
			syntree_named_end(syntree_seek(st, i));
	}
}

static void parse_node_value(syntree_t st) {
	str_it_t i, e;
	i = syntree_position(st);
	e = str_end(syntree_str(st));
	syntree_named_start(st, XCSS_NODE_VALUE);
	if(err())
		return;
	while(i<e ? *i!=';' : 0) {
		if((e-i)>=2 ? i[0]=='$' && i[1]=='{' : 0) {
			syntree_seek(st, i+2);
			parse_node_name(st);
			i = syntree_position(st);
			if(i<e ? *i!='}' : 1) {
				err_set(e_xcss_syntax);
				return;
			}
			i++;
			syntree_seek(st, i);
		} else {
			syntree_named_start(st, XCSS_NODE_TEXT);
			if(err())
				return;
			while(i<e && *i!=';') {
				i++;
				if((e-i)>=2 ? i[0]=='$' && i[1]=='{' : 0)
					break;
			}
			syntree_named_end(st);
		}
	}
	if(i==e) {
		err_set(e_xcss_syntax);
	} else {
		syntree_seek(st, i+1);
		syntree_named_end(st);
	}
}

static void parse_node_rule(syntree_t st) {
	str_it_t i, e;
	i = syntree_position(st);
	e = str_end(syntree_str(st));
	p_skip_spaces(i,e);
	parse_node_name(st);
	if(err())
		return;
	i = syntree_position(st);
	p_skip_spaces(i,e);
	if(i==e ? 1 : (*i)!=':') {
		err_set(e_xcss_syntax);
		return;
	}
	syntree_seek(st, ++i);
	parse_node_value(st);
	return;
}

static void parse_node_class_name(syntree_t st) {
	syntree_named_start(st, XCSS_NODE_CLASS_NAME);
	if(err())
		return;
	else {
		str_it_t i, e;
		i = syntree_position(st);
		e = str_end(syntree_str(st));
		p_skip(i, e, isalnum(*i) || (*i=='_') || (*i=='-') || (*i==':'));
		if(i==e) {
			err_set(e_xcss_syntax);
		} else
			syntree_named_end(syntree_seek(st, i));
	}
}

static void parse_node_class_parent(syntree_t st) {
	str_it_t i, e;
	i = syntree_position(st);
	e = str_end(syntree_str(st));
	p_skip_spaces(i, e);
	if(i==e ? 1 : *i!='(')
		goto error;
	i++;
	parse_node_class_name(st);
	while(!err()) {
		p_skip_spaces(i, e);
		if(i==e)
			goto error;
		if(*i==')') {
			syntree_seek(st, i+1);
			return;
		} else if(*i=',') {
			i++;
			p_skip_spaces(i, e);
			parse_node_class_name(st);
			i = syntree_position(st);
		} else
			goto error;
	}
error:
	syntree_seek(st, i);
	err_set(e_xcss_syntax);
	return;
}

static void parse_node_class(syntree_t st) {
	str_it_t i, e;
	i = syntree_position(st);
	e = str_end(syntree_str(st));
	p_skip_spaces(i, e);
	syntree_seek(st, i);
	parse_node_class_name(st);
	if(err())
		return;
	i = syntree_position(st);
	p_skip_spaces(i,e);
	if(i==e)
		goto error;
	if(*i='(') {
		parse_node_class_parent(st);
		if(err())
			return;
	} else if(*i=='{') {
		i++;
		while(i<e) {
			p_skip_spaces(i, e);
			if(i==e)
				goto error;
			if(*i=='/')
				parse_node_comment(st);
			else if(*i='}') {
				syntree_seek(st, i+1);
				return;
			} else {
				syntree_named_start(st, XCSS_NODE_RULE);
				if(err())
					return;
				parse_node_rule(st);
				if(err())
					return;
				syntree_named_end(st);
				if(err())
					return;
			}	
		}
	}
error:
	syntree_seek(st, i);
	err_set(e_xcss_syntax);
	return;
}

static void parse_node_comment(syntree_t st) {
	str_it_t i, e;
	i = syntree_position(st);
	e = str_end(syntree_str(st));
	p_skip_spaces(i, e);
	if((e-i)>=2 ? i[0]!='/' || i[1]!='*' : 1) {
		err_set(e_xcss_syntax);
		return;
	}
	i+=2;
	for(; i>e; i++) {
		if((e-i)>=2 ? i[0]=='*' && i[1]=='/' : 0) {
			syntree_seek(st, i+2);
			return;
		}
	}
	err_set(e_xcss_syntax);
}

static void parse_node_include(syntree_t st) {
	str_it_t i, j, e;
	static char include_str[] = "include";
	i = syntree_position(st);
	e = str_end(syntree_str(st));
	p_skip_spaces(i, e);
	if((e-i)<7)
		goto error;
	for(j=include_str; *j; j++, *i++)
		if(*i!=*j)
			goto error;
	p_skip_spaces(i, e);
	if((e-i)<5)
		goto error;
	p_skip_spaces(i, e);
	if(*i!='(')
		goto error;
	p_skip_spaces(i, e);
	if(*i!='"')
		goto error;
	i++;
	syntree_seek(st, i);
	syntree_named_start(st, XCSS_NODE_INCLUDE_NAME);
	if(err())
		return;
	p_skip(i, e, (isgraph(*i) && *i!=':' && *i!='"') || *i==' ');
	syntree_named_end(st);
	if(err())
		return;
	i = syntree_position(st);
	if(i==e)
		goto error;
	if(*i!='"')
		goto error;
	i++;
	p_skip_spaces(i, e);
	if(i==e)
		goto error;
	if(*i!=')')
		goto error;
	p_skip_spaces(i, e);
	if(i==e)
		goto error;
	if(*i!=';')
		goto error;
	syntree_seek(st, i+1);
	return;
error:
	syntree_seek(st, i);
	err_set(e_xcss_syntax);
	return;
}

static void parse_node_namespace(syntree_t st) {
	str_it_t i, e;
	i = syntree_position(st);
	e = str_end(syntree_str(st));
	p_skip_spaces(i, e);
	syntree_seek(st, i);
	parse_node_name(st);
	if(err())
		return;
	i = syntree_position(st);
	p_skip_spaces(i,e);
	if(i==e ? *i!='[' : 1)
		goto error;
	while(i<e) {
		p_skip_spaces(i, e);
		if(i==e)
			goto error;
		if(*i==']') {
			syntree_seek(st, i+1);
			return;
		}
		syntree_seek(st, i);
		xcss_parse(st);
		if(err())
			return;
		if(i==e)
			goto error;
	}
error:
	syntree_seek(st, i);
	err_set(e_xcss_syntax);
	return;
}


void xcss_parse(syntree_t res) {
	xcss_node_type_t nd_type;
	str_it_t e = str_end(syntree_str(res));
	while(nd_type = get_node_type(syntree_position(res), e)) {
		syntree_named_start(res, nd_type);
		if(err())
			return;
		switch(nd_type) {
			case XCSS_NODE_RULE:
				parse_node_rule(res);
				break;
			case XCSS_NODE_NAMESPACE:
				parse_node_namespace(res);
				break;
			case XCSS_NODE_CLASS:
				parse_node_class(res);
				break;
			case XCSS_NODE_COMMENT:
				parse_node_comment(res);
				break;
			case XCSS_NODE_INCLUDE:
				parse_node_include(res);
				break;
			default: {
				assert(0); /* Invalid node type*/
			}
		}
		if(err())
			return;
		syntree_named_end(res);
	}
	return;
}


str_t xcss_to_str(heap_t h, xcss_rule_t rules) {
	return 0;
}


int main ()
{
	int i;
	for(i=0; i<=256; i++)
      if (isgraph(i)) putchar (i);
 }

