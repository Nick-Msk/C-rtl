#ifndef CHECKER_H
#define CHECKER_H
// ---------------------------------------------------------------------------------
// --------------------------- Public Checker API ----------------------------------
// ---------------------------------------------------------------------------------

#include <stdio.h>
#include <stdbool.h>
#include "error.h"
#include "common.h"
#include "log.h"

// ----------- CONSTANTS AND GLOBALS ---------------

// ------------------- TYPES -----------------------

// ------------- CONSTRUCTOTS/DESTRUCTORS ----------

// -------------- ACCESS AND MODIFICATION ----------

int						inv_fprintf_int(FILE *restrict out, const char *restrict expr, int res, int val, const char *restrict msg, ...)  __attribute__ ((format (printf, 5, 6)));;

// ----------------- PRINTERS ----------------------

// ------------------ ETC. -------------------------

#ifndef NOINVARIANT

// TODO: add ACTION here ???
// basic invariant checker, raise SIGINT, but returns true/false
// internal
#define	_invraise(raise, errcode, expr, msg, ...)\
	({ 	bool _INV_RES = (expr);\
	   	if (! _INV_RES){\
            logsimple("Invariant violation: %s", #expr);\
            if (raise)\
			    userraiseint(errcode, msg, ##__VA_ARGS__);\
			else {\
                logsimple(msg, ##__VA_ARGS__);\
                inv_fprintf_int(stderr, #expr, 0, 0, msg, ##__VA_ARGS__);\
            }\
	   	}\
	   	_INV_RES;\
	})

// SIGINT
#define	invraise(expr, msg, ...) _invraise(true, ERRNUM_INVARIANT_VIOLATION, (expr), (msg), ##__VA_ARGS__)

#define invraisecode(expr, errcode, msg, ...) _invraise(true, (errcode), (expr), (msg), ##__VA_ARGS__)

// just return value
#define	inv(expr, msg, ...) _invraise(false, ERRNUM_INVARIANT_VIOLATION, (expr), (msg), ##__VA_ARGS__)

// internal
#define	_inv2raise(raise, errcode, expr, val, msg, ...)\
	({	typeof(val)  _RES, _VAL = (val);\
		_RES = (expr);\
		if (_VAL != _RES){\
			inv_fprintf_int(stderr, #expr, _RES, _VAL, msg, ##__VA_ARGS__);\
            logsimple("Invariant(2) violation: %s", #expr);\
            logsimple(msg, ##__VA_ARGS__);\
			if (raise)\
                userraiseint(errcode, msg, ##__VA_ARGS__);\
		}\
		_RES == _VAL;\
	})

// SIGINT
#define inv2raise(expr, val, msg, ...) _inv2raise(true, ERRNUM_INVARIANT_VIOLATION, (expr), (val), (msg), ##__VA_ARGS__)

#define inv2raisecode(expr, errcode, val, msg, ...) _inv2raise(true, (errcode), (expr), (val), (msg), ##__VA_ARGS__)

// return value
#define inv2(expr, val, msg, ...) _inv2raise(false, ERRNUM_INVARIANT_VIOLATION, (expr), (val), (msg), ##__VA_ARGS__)

#else /* NOINVARIANT */

#define inv(expr, msg, ...)\
	(expr)

#define invraise(expr, msg, ...)\
    (expr)

#define inv2(expr, val, msg, ...)\
	(expr) == (val)

#define inv2raise(expr, val, msg, ...)\
    (expr) == (val)

#endif /* NOINVARIANT */

// NEW checker strategy (22-jul-2024)
#define             check_arg(argcmax, msg, ...)\
    ( argc < (argcmax) ? fprintf(stderr, msg, ##__VA_ARGS__), 0 : 1)

#endif /* !CHECKER_H */
