#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <stdarg.h>

#include "bool.h"
#include "log.h"
#include "error.h"

/********************************************************************
 * @file    error.c
 * @brief   Per-thread error stack and signal-based exception mechanism.
 *
 * Implements a growable, thread-local stack of error records and a
 * setjmp/longjmp-based "exception" facility that lets a signal handler
 * unwind to the most recent try() call-site.
 ********************************************************************/

/* ─────────────────────────────────────────────────────────────────────────
 * Version
 * ───────────────────────────────────────────────────────────────────────── */

static const char 				*const versions[] = {
    ERROR_VERSION,   /* current — always computed from macros */
    NULL
};

const char *const *
err_versions(void) {
    return versions;
}

const char *
err_version(void) {
    return versions[0];
}

// static globals

/** Size of the static (stack-allocated) TOTAL buffer. */
enum { ERROR_INIT_COUNT = 128 };

/** Jump code passed via longjmp from the default signal handler. */
static const int                    	ERR_DEFHANDLER_JUMP_CODE = 10;

static const int						ERR_DEFAULT_INCREMENT  	 = 16;

// internal types

/**
 * @brief A single error record in the per-thread stack.
 */
typedef struct {
    ErrorType   type;                            ///< Error class (ERR_USER / ERR_SYS).
    int         code;                            ///< Numeric code (app code or errno).
    char        msg[ERR_MESSAGE_MAX_LENGTH];     ///< Formatted human-readable message.
} Error;

/** @brief Static initial buffer (one per thread). */
static _Thread_local Error            g_error_init[ERROR_INIT_COUNT];

/** @brief Pointer to the active buffer (static or heap). */
static _Thread_local Error           *g_error = NULL;

/** @brief Number of slots in use / total allocated capacity. */
static _Thread_local int              g_currerr = 0, g_allocerr = ERROR_INIT_COUNT;

/** @brief Thread-local setjmp/longjmp environment. */
static _Thread_local ExceptionData    g_env;

// ---------- pseudo-header for utility procedures -----------------

// ------------------------------ Utilities ------------------------

/**
 * @brief Ensure g_error points to a valid buffer (first call per thread).
 *
 * _Thread_local pointers start as NULL; this lazily binds to the static
 * initial buffer. Call at the top of any function that dereferences g_error.
 */
static inline void
err_ensurebuf(void)
{
    if (g_error == NULL) {
        g_error    = g_error_init;
        g_currerr  = 0;
        g_allocerr = ERROR_INIT_COUNT;
    }
}

/**
 * @brief Convert an ErrorType to a printable string.
 * @param t  The error type.
 * @return   "ERR_USER", "ERR_SYS", or "Unknown".
 */
static inline const char *
err_type_text(ErrorType t)
{
	switch (t){
		case ERR_USER	: return "ERR_USER";
		case ERR_SYS	: return "ERR_SYS";
		default			: return "Unknown";
	}
}

/**
 * @brief Check whether the thread still uses the static initial buffer.
 * @return true if g_error points to g_error_init.
 */
static inline bool
err_isinit(void)
{
	err_ensurebuf();
	return g_error == g_error_init;
}

static int
err_increase(void)
{
	if (g_currerr < g_allocerr)
		return g_allocerr;

    Error   *err;
    int      newalloc = g_allocerr + ERR_DEFAULT_INCREMENT;

    if (err_isinit())
    {
        if ( (err = malloc(newalloc * sizeof(Error))) == 0)
            return logsimple(0, "Unable to init alloc of %d elements", newalloc);
        // copy prev
        memcpy(err, g_error_init, g_currerr * sizeof(Error));
    }
    else {
        if ( (err = realloc(g_error, newalloc * sizeof(Error))) == 0)
            return logsimpleerr(0, "Unable to extend error array to %d", newalloc);
    }

    g_error    = err;
    g_allocerr = newalloc;
    return logsimpleret(g_allocerr, "Error array is increased to %d", g_allocerr);
}


/**
 * @brief Push a new error record onto the top of the per-thread stack.
 *
 * Message rendering depends on @p tp:
 *  - **ERR_USER** – `msg`/`ap` formatted directly.
 *  - **ERR_SYS**  – `strerror(errno)` + `": "` + `msg`/`ap`.
 *
 * Caller (err_raise) must ensure capacity before calling this function.
 *
 * @param tp       Error class.
 * @param errcode  Application-level code (ignored for ERR_SYS; errno used).
 * @param msg      printf-style format string.
 * @param ap       Already-started va_list matching msg.
 */
static void
err_put(ErrorType tp, int errcode, const char *msg, va_list ap)
{
	int saved_errno = errno;
	err_ensurebuf();

    if (!err_increase()) {
		logsimple("Unable to increase err buffer");
        return; 
    }

	Error 	*err = g_error + g_currerr++;
	err->type = tp;

	switch (tp){
		case ERR_USER:
			err->code = errcode;
			vsnprintf(err->msg, sizeof err->msg, msg, ap);
		break;
		case ERR_SYS:
			err->code = saved_errno;		// system error number
			if (strerror_r(err->code, err->msg, sizeof err->msg) != ERANGE)		// enough space in the buffer
			{
				int		 len = strlen(err->msg);	// length of system error message
				char 	*pos 	= err->msg + len;
				int		 sz 	= sizeof err->msg - len - 1;
				if (sz > 0){							// TODO: not sure if this is good implementation, better to use faststring!
					len = snprintf(pos, sz, ": ");
					pos += len;
					sz  -= len;
					if (sz > 0)
						vsnprintf(pos, sz, msg, ap);		// TODO: not sure about -1, it depends on how exactly sprintf works
				} else
					logsimple("Not enough space for user message (%zu, offset %ld)", sizeof err->msg, pos - err->msg);
			} else
				logsimple("Not enough space for system message strerror (%zu)", sizeof err->msg);
		break;
		default:
			snprintf(err->msg, sizeof err->msg, "Unknown error type (%d)", tp);
		break;
	}

}

int
err_msg(int code, char *buf, size_t sz)
{
    if (!buf || sz == 0)
        return 0;
#if defined(__GLIBC__) && defined(_GNU_SOURCE)
    const char *s = strerror_r(code, buf, sz);
    if (s != buf) {
        size_t n = strlen(s);
        if (n >= sz) n = sz - 1;
        memcpy(buf, s, n);
        buf[n] = '\0';
        return (int)n;
    }
    return (int)strlen(buf);
#else
    if (strerror_r(code, buf, sz) != 0) {
        int n = snprintf(buf, sz, "errno %d", code);
        return n < 0 ? 0 : (n < (int)sz ? n : (int)sz - 1);
    }
    return (int)strlen(buf);
#endif
}

// -------------------------- (Utility) printers -------------------

/**
 * @brief Format and write a single error record to a stream.
 *
 * Output: `TYPE: [code] message\n`
 *
 * @param[out] out  Destination stream (must be non-NULL).
 * @param[in]  err  Error record to print.
 * @return Number of characters written, or negative on I/O error.
 */
static inline int
err_fprinterr(FILE *restrict out, const Error *err)
{
	return fprintf(out, "%s: [%d] %s\n", err_type_text(err->type), err->code, err->msg);		// TODO: think about mapping between code and message!
}

// --------------------------- API ---------------------------------

/**
 * @brief Reset the per-thread error stack to its initial (empty) state.
 *
 * @param force  When true and a heap buffer is in use, it is freed.
 *               When false, indices are rewound but heap memory is kept.
 */
void
err_clean(bool force){
	if (force && !err_isinit())
    	free(g_error);	// if NULL then ok
    g_error = g_error_init;
    g_currerr = 0;
	g_allocerr = ERROR_INIT_COUNT;
    logsimple("Error stack is cleaned");
}

/**
 * @brief Record an error and optionally raise a siglongjmp.
 *
 * Main entry-point for all error reporting. The message is pushed onto the
 * per-thread stack; if @p raise is non-zero the corresponding signal is
 * delivered (interceptable by a try() block).
 *
 * @param tp       Error class (ERR_USER / ERR_SYS).
 * @param raise    Signal number to raise, or 0 to suppress.
 * @param errcode  Application error code (see ErrorCode); ignored for ERR_SYS.
 * @param msg      printf-style format string.
 * @param ...      Variadic arguments matching @p msg.
 */
extern void
err_raise(ErrorType tp, int errcode, const char *msg, ...)
{
	va_list		ap;
	va_start(ap, msg);

	err_put(tp, errcode, msg, ap);
	va_end(ap);


   	errenv.depth--;
    siglongjmp(errenv.env[errenv.depth], ERR_DEFHANDLER_JUMP_CODE);
}

// setjmp/longjmp API

/**
 * @brief Obtain a pointer to the current thread's exception environment.
 * @return Pointer to the thread-local ExceptionData (valid for thread lifetime).
 */
ExceptionData*
err_getexception_info(void)
{
    return &g_env;      // access to global
}

// -------------------------- (API) printers -----------------------

/**
 * @brief Dump the full per-thread error stack to a stream.
 *
 * Each entry is printed as `[index]: TYPE: [code] message`.
 *
 * @param[out] out  Destination stream.
 * @return Total characters written, or negative on I/O error.
 */
extern int
err_fprintstacktrace(FILE *out)
{
	int		res = 0;
	err_ensurebuf();
	logauto(g_currerr);

	res += fprintf(out, "\n------------- PRINT STACK TRACE START -----------\n\n");
	for (int i = 0; i < g_currerr; i++)
	{
		res += fprintf(out, "[%4d]: ", i);					// имя функции???
		res += err_fprinterr(out, g_error +i);
	}

	res += fprintf(out, "\n------------- PRINT STACK TRACE END -------------\n\n");
	return logautoret(res);
}
