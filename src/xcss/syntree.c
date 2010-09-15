
#include "syntree.h"

syntree_t syntree_create(heap_t h, str_t s) {
	syntree_t r = heap_alloc(h, sizeof(struct syntree_node_s));
	if(r) {
		r->heap = h;
		r->first = r->last = 0;
		r->parent = 0;
		r->max_position = r->position = str_begin(s);
		r->str = s;
	}
	return r;
}

syntree_t syntree_transaction(syntree_t st) {
	syntree_t r = heap_alloc(st->heap, sizeof(struct syntree_node_s));
	if(r) {
		r->heap = st->heap;
		r->position = st->position;
		r->max_position = st->max_position;
		r->parent = st;
		r->first = r->last = 0;
	}
	return r;
}

syntree_t syntree_commit(syntree_t st) {
	if(st->parent->last) {
		st->parent->last->next = st->first;
		st->parent->last = st->last;
		st->parent->position = st->position;
		st->parent->max_position = st->max_position;
	} else {
		st->parent->first = st->first;
		st->parent->last = st->last;
	}
	return st->parent;
}

syntree_t syntree_rollback(syntree_t st) {
	return st->parent;
}

syntree_t syntree_named_start(syntree_t st, int nm) {
	syntree_node_t nd = heap_alloc(st->heap, sizeof(struct syntree_node_s));
	if(nd) {
		nd->is_start = 1;
		nd->name = nm;
		nd->value = 0;
		nd->next = 0;
		nd->heap = st->heap;
		nd->position = st->position;
		if(st->last) {
			st->last->next = nd;
			st->last = nd;
		} else
			st->last = st->first = nd;
	}
	return st;
}

syntree_t syntree_named_end(syntree_t st) {
	syntree_node_t nd = heap_alloc(st->heap, sizeof(struct syntree_node_s));
	if(nd) {
		nd->is_start = 0;
		nd->name = 0;
		nd->value = 0;
		nd->next = 0;
		nd->heap = st->heap;
		nd->position = st->position;
		if(st->last) {
			st->last->next = nd;
			st->last = nd;
		} else
			st->last = st->first = nd;
	}
	return st;
}

syntree_node_t syntree_begin(syntree_t st) {
	return st->first;
}

syntree_node_t syntree_next(syntree_node_t stn) {
	int level = 0;
	for(; level>=0 && stn; stn=stn->next) {
		level += stn->is_start ? 1 : -1;
		if(level==0 && (stn->next ? stn->next->is_start : 0))
			return stn->next;
	}
	return 0;
}

syntree_node_t syntree_child(syntree_node_t stn) {
	if(stn->next ? stn->next->is_start : 0)
		return stn->next;
	return 0;
}

str_t syntree_value(syntree_node_t stn) {
	if(stn->value)
		return stn->value;
	else {
		syntree_node_t i;
		int level = 1;
		for(i=stn->next; level>0 && i; i=i->next) {
			if(i->is_start)
				level++;
			else {
				level--;
				if(level==0)
					return stn->value = str_interval(stn->heap, stn->position, i->position);
			}
		}
	}
	assert(0);
	return 0;
}

syntree_t syntree_seek(syntree_t st, str_it_t pos) {
	assert(pos<=str_end(st->str));
	st->position = pos; 
	if(pos>st->max_position)
		st->max_position = pos;
	return st;
}



