
#include "map.h"
#include "mem.h"
#include <assert.h>

#define branch_i(p,ch) ((p)->children[0]==(ch) ? 0 : 1)

map_t map_create(heap_t h) {
	map_t res = (map_t) heap_alloc(h, sizeof(map_s));
	res->heap = h;
	res->node = 0;
	return res;
}

map_node_t map_begin(map_t m) {
	return m->node;
}

map_node_t map_next(map_node_t n) {
	assert(n);
	if(n->children[0])
		return n->children[0];
	else if(n->children[1])
		return n->children[1];
	else {
		while(n->parent) {
			if(n->parent->children[0]==n && n->parent->children[1])
				return n->parent->children[1];
			else
				n = n->parent;
		}
		return 0;
	}
}

void *map_get(map_t m, str_t key) {
	map_node_t i = m->node;
	while(i) {
		int cmp_res = str_compare(key, i->key);
		if(cmp_res==0)
			return i->value;
		i = i->children[(cmp_res>0) ? 1 : 0];
	}
	return 0;
}

map_t map_set(map_t m, str_t key, void *value) {
	map_node_t i;
	assert(m && key);
	i = m->node;
	if(i) {
		while(1) {
			int cmp_res = str_compare(key, i->key);
			if(cmp_res==0) {
				i->value = value;
				break;
			} else {
				int ci = cmp_res<0 ? 0 : 1;
				if(i->children[ci]) {
					i = i->children[ci];
					continue;
				} else {
					map_node_t j = i->children[ci] = heap_alloc(m->heap, sizeof(map_node_s));
					j->length = 1;
					j->parent = i;
					j->children[0] = 0;
					j->children[1] = 0;
					j->key = key;
					j->value = value;
					for(; i; i=i->parent)
						i->length++;
					break;
				}
			}
		}
	} else {
		i = m->node = heap_alloc(m->heap, sizeof(map_node_s));
		i->length = 1;
		i->parent = 0;
		i->children[0] = 0;
		i->children[1] = 0;
		i->key = key;
		i->value = value;
	}
	return m;
}

map_t map_remove(map_t m, str_t key) {
	map_node_t i = m->node;
	while(i) {
		int cr = str_compare(key, i->key);
		if(cr==0) {
			map_node_t j;
			for(j=i; j; j=j->parent)
				j->length--;
			if(i->children[0] && i->children[1]) {
				j=i->children[1];
				do {
					j->length += i->children[0]->length;
					if(j->children[0])
						j=j->children[0];
					else
						break;
				} while(1);
				j->children[0] = i->children[0];
				i->children[1]->parent = i->parent;
				if(i->parent)
					i->parent->children[branch_i(i, i->parent)] = i->children[1];
				else
					m->node = i->children[1];
			} else if(i->children[0] || i->children[1]) {
				int ch_i = i->children[0] ? 0 : 1;
				i->children[ch_i]->parent = i->parent;
				if(i->parent)
					i->parent->children[branch_i(i, i->parent)] = i->children[ch_i];
				else
					m->node = i->children[ch_i];
			} else {
				if(i->parent)
					i->parent->children[branch_i(i, i->parent)] = 0;
				else
					m->node = 0;
			}
		} else
			i = i->children[cr<0 ? 0 : 1];
	}
	return m;
}

typedef struct node_list_ss {
	map_node_t first;
	map_node_t last;
} node_list_s;

static node_list_s to_list(map_node_t nd) {
	node_list_s res;
	if(nd->children[0] && nd->children[1]) {
		node_list_s l1 = to_list(nd->children[0]);
		res = to_list(nd->children[1]);
		nd->children[0] = 0;
		nd->children[1] = 0;
		l1.last->parent = nd;
		nd->parent = res.first;
		res.first = l1.first;
	} else if(nd->children[0]) {
		res = to_list(nd->children[0]);
		nd->children[0] = 0;
		nd->parent = 0;
		res.last->parent = nd;
		res.last = nd;
	} else if(nd->children[1]) {
		res = to_list(nd->children[1]);
		nd->children[1] = 0;
		nd->parent = res.first;
		res.first = nd;
	} else {
		nd->parent = 0;
		res.first = nd;
		res.last = nd;
	}
	return res;
}

static map_node_t to_tree(node_list_s *l, int len) {
	int tmp_c = 0;
	map_node_t tmp_x;
	for(tmp_x = l->first; tmp_x; tmp_x = tmp_x->parent)
		tmp_c++;
	if(tmp_c!=len)
		return 0;
	if(len==1) {
		l->first->length = 1;
		return l->first;
	} else if(len==2) {
		l->last->children[0] = l->first;
		l->first->parent = l->last;
		l->first->length = 1;
		l->last->length = 2;
		return l->last;
	} else {
		int fl = len/2;
		map_node_t i = l->first;
		int c;
		node_list_s l1, l2;
		for(c=0; c<(fl-1); c++)
			i = i->parent;
		l1.first = l->first;
		l1.last = i;
		l2.first = i->parent->parent;
		l2.last = l->last;
		i = i->parent;
		l2.last->parent = 0;
		l1.last->parent = 0;
		i->children[0] = to_tree(&l1, fl);
		i->children[1] = to_tree(&l2, len-fl-1);
		i->children[0]->parent = i;
		i->children[1]->parent = i;
		i->length = len;
		return i;
	}
}

map_t map_optimize(map_t m) {
	if(m->node) {
		int len = m->node->length;
		node_list_s l = to_list(m->node);
		m->node = to_tree(&l, len);
		m->node->parent = 0;
	}
	return m;
}
