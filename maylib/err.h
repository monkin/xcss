#ifndef MAY_ERR_H
#define MAY_ERR_H

#include <stddef.h>
#include <assert.h>
#include <stdio.h>

typedef struct err_s {
	const char *name;
	const char *message;
	const char *file;
	int line;
	const struct err_s *parent;
} err_t;

extern __thread const err_t *err_;
extern __thread int err_line_;
extern __thread const char *err_file_;

extern const err_t err_0_realisation;

#define ERR_DECLARE(name) extern const err_t *const name; extern const err_t err_ ## name ## _realisation
#define ERR_DEFINE(name, message, parent_error) \
const err_t err_ ## name ## _realisation = { # name, message, __FILE__, __LINE__, &err_ ## parent_error ## _realisation }; \
const err_t *const name = &err_ ## name ## _realisation

#ifndef NDEBUG
#	define err_display() fprintf(stderr, "Error: %s\nMessage: %s\nFile: %s\nLine: %i\n", err_->name, err_->message, err_file_, err_line_);
#	define err_message(m) fprintf(stderr, "%s", m)
#else
#	define err_display() ;
#	define err_message(m) ;
#endif

/**
 * Return true if error exist
 */
#define err() (err_!=0)
/**
 * Show error, if it not processed
 */
#define err_reset() { if(err()) { err_message("Error not processed.\n"); err_display(); err_=0; } }
/**
 * Replase old error to new
 */
#define err_replace(name) { err_ = name; err_line_ = __LINE__; err_file_ = __FILE__; }
/**
 * Set error information
 */
#define err_set(name) { err_reset();  err_replace(name) }
/**
 * Get error
 */
#define err_get() err_
#define err_file() err_file_
#define err_line() err_line_
/**
 * Clear error flag
 */
#define err_clear() { err_ = 0; err_file_ = 0; err_line_ = 0; }

int err_is(const err_t *);

ERR_DECLARE(e_arguments);

#endif /* MAY_ERR_H */

