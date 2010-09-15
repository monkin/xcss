#include "str.h"
#include "err.h"
#include <stddef.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

#define INT_BUFFER_LEN 16
#define DOUBLE_BUFFER_LEN 128

str_t str_create(heap_t h, size_t sz) {
	assert(h);
	str_t r = heap_alloc(h, sizeof(may_str_s) + sz + 1);
	if(r) {
		r->data = ((char *)r) + sizeof(may_str_s);
		r->length = sz;
		r->data[sz] = 0;
	}
	return r;
}

str_t str_from_cs(heap_t h, const char *s) {
	str_t r = str_create(h, strlen(s));
	if(r)
		memcpy(r->data, s, r->length);
	return r;
}

str_t str_from_int(heap_t h, int i) {
	str_t r = str_create(h, INT_BUFFER_LEN);
	if(r) {
		r->length = snprintf(r->data, INT_BUFFER_LEN + 1, "%d", i);
		if(r->length > INT_BUFFER_LEN) {
			r = str_create(h, r->length);
			if(r)
				r->length = snprintf(r->data, r->length + 1, "%d", i);
		}
	}
	return r;
}

str_t str_from_double(heap_t h, double d) {
	str_t r = str_create(h, DOUBLE_BUFFER_LEN);
	if(r) {
		r->length = snprintf(r->data, DOUBLE_BUFFER_LEN + 1, "%f", d);
		if(r->length > DOUBLE_BUFFER_LEN) {
			r = str_create(h, r->length+1);
			if(r)
				r->length = snprintf(r->data, r->length + 1, "%f", d);
		}
	}
	return r;
}

str_t str_cat(heap_t h, str_t s1, str_t s2) {
	if(s1 && s2) {
		str_t r;
		r = str_create(h, s1->length + s2->length);
		if(r) {
			memcpy(r->data, s1->data, s1->length);
			memcpy(r->data+s1->length, s2->data, s2->length+1);
		}
		return r;
	} else {
		err_set(e_arguments);
		return 0;
	}
}

str_it_t str_end(str_t s) {
	return str_begin(s) + s->length;
}

str_t str_interval(heap_t h, str_it_t ps1, str_it_t ps2) {
	str_t res;
	assert(h);
	assert((ps1 && ps2) || (!ps1 && !ps2));
	res = heap_alloc(h, sizeof(may_str_s));
	if(res) {
		res->length = ps2-ps1;
		res->data = ps1;
	}
	return res;
}

str_t str_clone(heap_t h, str_t s) {
	str_t r;
	assert(s);
	r = str_create(h, s->length);
	if(err())
		return 0;
	memcpy(r->data, s->data, s->length);
	return r;
}

sbuilder_t sbuilder_create(heap_t h) {
	sbuilder_t r = (sbuilder_t) heap_alloc(h, sizeof(sbuilder_s));
	if(r) {
		r->length = 0;
		r->heap = h;
		r->first = r->last = 0;
	}
	return r;
}

sbuilder_t sbuilder_append(sbuilder_t sb, str_t s) {
	sbuilder_item_t *i;
	assert(sb);
	if(s) {
		i = heap_alloc(sb->heap, sizeof(sbuilder_item_t));
		if(i) {
			i->data = s;
			i->next = 0;
			sb->length += s->length;
			if(sb->first) {
				sb->last->next = i;
				sb->last = i;
			} else
				sb->first = sb->last = i;
		}
		return sb;
	} else {
		err_set(e_arguments);
		return sb;
	}
}

str_t sbuilder_get(heap_t h, sbuilder_t sb) {
	str_t r;
	assert(sb);
	assert(h);
	r = str_create(h, sb->length);
	if(r) {
		sbuilder_item_t *i;
		str_it_t p = str_begin(r);
		for(i = sb->first; i; i=i->next) {
			memcpy(p, i->data->data, i->data->length);
			p += i->data->length;
		}
	}
	return r;
}

int str_compare(str_t s1, str_t s2) {
	if(s1 && s2) {
		if(s1!=s2) {
			str_it_t i, j;
			int c = s1->length>s2->length ? s2->length : s1->length;
			for(i=s1->data, j=s2->data; c; c--, i++, j++)
				if(*i<*j)
					return -1;
				else if(*i>*j)
					return 1;
			if(s1->length<s2->length)
				return -1;
			else if(s1->length>s2->length)
				return 1;
		}
		return 0;
	} else {
		err_set(e_arguments);
		return 0;
	}
}

int str_equal(str_t s1, str_t s2) {
	if(s1 && s2) {
		if(s1->length!=s2->length)
			return 0;
		else
			return !str_compare(s1, s2);
	} else {
		err_set(e_arguments);
		return 0;
	}
}


