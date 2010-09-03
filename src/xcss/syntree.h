#ifndef MAY_SYNTREE_H
#define MAY_SYNTREE_H

#include "maylib/str.h"
#include "maylib/heap.h"

struct syntree_node_s {
	int is_start;
	str_it_t position;
	heap_t heap;
	struct syntree_node_s *next;
	int name;
	str_t value;
};

typedef struct syntree_node_s *syntree_node_t;

struct syntree_s {
	heap_t heap;
	syntree_node_t first;
	syntree_node_t last;
	struct syntree_s *parent;
	str_t str;
	str_it_t position;
	str_it_t max_position;
};

typedef struct syntree_s *syntree_t;

syntree_t syntree_create(heap_t, str_t);

syntree_t syntree_transaction(syntree_t);
syntree_t syntree_commit(syntree_t);
syntree_t syntree_rollback(syntree_t);
syntree_t syntree_named_start(syntree_t, int);
syntree_t syntree_named_end(syntree_t);

#define syntree_position(st) ((st)->position)
#define syntree_str(st) ((st)->str)
syntree_t syntree_seek(syntree_t, str_it_t);

syntree_node_t syntree_begin(syntree_t);
syntree_node_t syntree_next(syntree_node_t);
syntree_node_t syntree_child(syntree_node_t);
/*int syntree_name(syntree_node_t);*/
#define syntree_name(stn) ((stn)->name)
str_t syntree_value(syntree_node_t);

#endif /* MAY_SYNTREE_H */
