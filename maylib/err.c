
#include "err.h"

__thread const err_t *err_ = 0;
__thread int err_line_ = 0;
__thread const char *err_file_ = 0;

const err_t err_0_realisation = { "e_error", "Error", __FILE__, __LINE__, 0 };

int err_is(const err_t *x) {
	const err_t *p = err_;
	if(err_ && !x)
		return 1;
	while(p) {
		if(p==x)
			return 1;
		p = p->parent;
	}
	return 0;
}

ERR_DEFINE(e_arguments, "Invalid arguments.", 0);