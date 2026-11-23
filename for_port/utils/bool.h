#ifndef _BOOL_H_
#define _BOOL_H_

#include <stdbool.h>

static inline const char*       bool_str(bool v){
	return v ? "true" : "false";
}

#endif /* !BOOL_H */
