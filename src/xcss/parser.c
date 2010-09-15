
#include "parser.h"

#define p_skip(i, e, ex) \
while((i)<(e) && (ex))   \
	i++;

#define p_skip_spaces(i, e) p_skip(i,e,isspace(*i))

ERR_DEFINE(e_xcss_syntax, "XCSS syntax error.", 0);

static void xcss_parse(syntree_t res);
static void parse_node_comment(syntree_t st);


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
			case '\"':
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
		} else {
			syntree_seek(st, i);
			syntree_named_end(st);
		}
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
			syntree_seek(st, i);
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
	syntree_seek(st, i);
	parse_node_name(st);
	if(err())
		return;
	i = syntree_position(st);
	p_skip_spaces(i,e);
	if(i==e ? 1 : (*i)!=':') {
		err_set(e_xcss_syntax);
		return;
	}
	i++;
	p_skip_spaces(i,e);
	syntree_seek(st, i);
	parse_node_value(st);
	return;
}

static int isclass_name_char(int c) {
	if(isalnum(c))
		return 1;
	switch(c) {
		case '_':
		case '-':
		case ':':
		case '>':
			return 1;
		default:
			return 0;
	}
}

static void parse_node_class_name(syntree_t st) {
	syntree_named_start(st, XCSS_NODE_CLASS_NAME);
	if(err())
		return;
	else {
		str_it_t i, e, j;
		i = syntree_position(st);
		e = str_end(syntree_str(st));
		while(1) {
			p_skip(i, e, isclass_name_char(*i));
			if(i==e)
				goto error;
			j = i;
			p_skip_spaces(j, e);
			if(j==e)
				goto error;
			if(!isclass_name_char(*j))
				break;
		}
		if(i!=e) {
			syntree_seek(st, i);
			syntree_named_end(st);
			return;
		}
	error:
		syntree_seek(st, i);
		err_set(e_xcss_syntax);
		return;
	}
}

static void parse_node_class_parent(syntree_t st) {
	str_it_t i, e;
	syntree_named_start(st, XCSS_NODE_CLASS_PARENT);
	i = syntree_position(st);
	e = str_end(syntree_str(st));
	p_skip_spaces(i, e);
	if(i==e ? 1 : *i!='(')
		goto error;
	i++;
	syntree_seek(st, i);
	parse_node_class_name(st);
	while(!err()) {
		i = syntree_position(st);
		p_skip_spaces(i, e);
		if(i==e)
			goto error;
		if(*i==')') {
			syntree_seek(st, i+1);
			syntree_named_end(st);
			return;
		} else if(*i==',') {
			i++;
			p_skip_spaces(i, e);
			parse_node_class_name(st);
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
	if(*i=='(') {
		parse_node_class_parent(st);
		if(err())
			return;
		i = syntree_position(st);
		p_skip_spaces(i,e);
		if(i==e)
			goto error;
	}
	if(*i=='{') {
		i++;
		while(i<e) {
			p_skip_spaces(i, e);
			if(i==e)
				goto error;
			syntree_seek(st, i);
			if(*i=='/')
				parse_node_comment(st);
			else if(*i=='}') {
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
			i = syntree_position(st);
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
	for(; i<e; i++) {
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
	i++;
	p_skip_spaces(i, e);
	if(*i!='"')
		goto error;
	i++;
	syntree_seek(st, i);
	syntree_named_start(st, XCSS_NODE_INCLUDE_NAME);
	if(err())
		return;
	p_skip(i, e, (isgraph(*i) && *i!=':' && *i!='"') || *i==' ');
	syntree_seek(st,i);
	syntree_named_end(st);
	if(err())
		return;
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
	i++;
	if(i==e)
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
	if(i==e ? 1 : *i!='[')
		goto error;
	i++;
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
		i = syntree_position(st);
		if(i==e)
			goto error;
	}
error:
	syntree_seek(st, i);
	err_set(e_xcss_syntax);
	return;
}

static void xcss_parse(syntree_t res) {
	xcss_node_type_t nd_type;
	str_it_t e = str_end(syntree_str(res));
	if(nd_type = get_node_type(syntree_position(res), e)) {
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
	} else {
		str_it_t i = syntree_position(res);
		str_it_t e = str_end(syntree_str(res));
		p_skip_spaces(i, e);
		syntree_seek(res, i);
		if(i!=e) {
			err_set(e_xcss_syntax);
			return;
		}
	}
	return;
}


syntree_t xcss_to_syntree(heap_t h, str_t xcss) {
	str_it_t i, e;
	syntree_t res = syntree_create(h, xcss);
	if(err())
		return 0;
	e = str_end(xcss);
	while(syntree_position(res)!=e) {
		xcss_parse(res);
		if(err())
			return 0;
	}
	return res;
}



