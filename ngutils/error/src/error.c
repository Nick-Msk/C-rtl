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
 * sigsetjmp/siglongjmp-based "exception" facility that lets err_raise()
 * unwind to the most recent TRY() call-site.
 ********************************************************************/

/* ─────────────────────────────────────────────────────────────────────────
 * Version
 * ───────────────────────────────────────────────────────────────────────── */

static const char 				*const versions[] = {
    ERROR_VERSION,
	"0.1.0",
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

/** Size of the static (stack-allocated) buffer. */
enum { ERROR_INIT_COUNT = 128 };

/** Jump code passed via siglongjmp from err_raise. */
static const int                    	ERR_DEFHANDLER_JUMP_CODE = 10;

/** Increment used when growing the error array on the heap. */
static const int						ERR_DEFAULT_INCREMENT  	 = 16;

// internal types

/**
 * @brief A single error record in the per-thread stack.
 */
typedef struct {
	ErrorInfo	info; 							///< Error class (ERR_USER / ERR_SYS) and error code.
    char        msg[ERR_MESSAGE_MAX_LENGTH];    ///< Formatted human-readable message.
} Error;

/** @brief Static initial buffer (one per thread). */
static _Thread_local Error            		g_error_init[ERROR_INIT_COUNT];

/** @brief Pointer to the active buffer (static or heap). */
static _Thread_local Error           	   *g_error = NULL;

/** @brief Number of slots in use / total allocated capacity. */
static _Thread_local int              		g_currerr = 0, g_allocerr = ERROR_INIT_COUNT;

/** @brief Thread-local setjmp/longjmp environment. */
static _Thread_local ErrorExceptionData    	g_env;

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

/**
 * @brief Ensure the error buffer has capacity for at least one more entry.
 *
 * If `g_currerr < g_allocerr` the buffer already has space and the
 * current capacity is returned unchanged. Otherwise the buffer is grown:
 *  - Static buffer → allocated a new heap buffer (copying existing records).
 *  - Heap buffer   → `realloc`'d to a larger size.
 *
 * @return Current total capacity (number of slots) on success, or 0 on
 *         allocation failure.
 */
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
            return logsimpleerr(0, "Unable to init alloc of %d elements", newalloc);
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
 * @brief Fill @p buf with the system message for @p code .
 *
 * Thread-safe (uses strerror_r). Falls back to "errno <N>" if the
 * message cannot be retrieved.
 *
 * @param code  System error code (errno value).
 * @param buf   Destination buffer.
 * @param sz    Size of @p buf (must be > 0).
 * @return      Number of characters written (excluding NUL).
 */
static size_t
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
        return n;
    }
    return strlen(buf);
#else
    if (strerror_r(code, buf, sz) != 0) {
        snprintf(buf, sz - 1, "Errno %d", code);
        //return n < 0 ? 0 : (n < (int) sz ? n : (int) sz - 1);
    }
    return strlen(buf);
#endif
}

/**
 * @brief Push a new error record onto the top of the per-thread stack.
 *
 * Message rendering depends on @p tp:
 *  - **ERR_USER** – `msg`/`ap` formatted directly.
 *  - **ERR_SYS**  – `strerror(saved_errno)` + `": "` + `msg`/`ap`.
 *
 * Capacity is managed internally via `err_increase()`; if the buffer
 * cannot be grown the record is silently dropped (a warning is logged).
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
	err->info.type = tp;

	switch (tp){
		case ERR_USER:
			err->info.code = errcode;
			vsnprintf(err->msg, sizeof err->msg, msg, ap);
		break;
		case ERR_SYS:
			err->info.code = saved_errno;
			size_t len = err_msg(err->info.code, err->msg, ERR_MESSAGE_MAX_LENGTH);
			if (len + 2 < sizeof err->msg) {
				err->msg[len]     = ':';
				err->msg[len + 1] = ' ';
				err->msg[len + 2] = '\0';
				vsnprintf(err->msg + len + 2,
						sizeof err->msg - len - 3, msg, ap);
				err->msg[ERR_MESSAGE_MAX_LENGTH - 1] = '\0';
			}
		break;
		default:
			snprintf(err->msg, sizeof err->msg, "Unknown error type (%d)", tp);
		break;
	}

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
	return fprintf(out, "%s: [%d] %s\n", err_type_text(err->info.type), err->info.code, err->msg);
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
    	free(g_error);
    g_error = g_error_init;
    g_currerr = 0;
	g_allocerr = ERROR_INIT_COUNT;
    logsimple("Error stack is cleaned");
}

/**
 * @brief Record an error on the per-thread stack and longjmp to the
 *        innermost active TRY() block.
 *
 * Main entry-point for all error reporting. The formatted message is
 * pushed onto the per-thread error stack, then:
 *  - If an active TRY() block exists (`errenv.depth > 0`), performs
 *    `siglongjmp` to the innermost capture site.
 *  - If **no** TRY() block is active, raises `SIGINT` as a last-resort
 *    so a debugger or core dump can show what happened.
 *
 * @param tp       Error class (ERR_USER / ERR_SYS).
 * @param errcode  Application error code (see ErrorCode); ignored for ERR_SYS.
 * @param msg      printf-style format string.
 * @param ...      Variadic arguments matching @p msg.
 *
 * @note Does not return normally when a TRY() block is active (longjmps).
 *       When no TRY() is active, returns after raising SIGINT.
 */
void
err_raise(ErrorType tp, int errcode, const char *msg, ...)
{
	va_list		ap;
	va_start(ap, msg);

	err_put(tp, errcode, msg, ap);
	va_end(ap);

	if (errenv.overallcnt == 0 && errenv.depth == 0) {
        /* No active TRY — cannot siglongjmp. Raise a real SIGINT so a
         * debugger or core dump can show what happened. */
        logsimple("no active TRY block, SIGINT will be raised!");
        raise(SIGINT);
        return;
    }

	err_prevcnt();
    siglongjmp(*err_getcurrsigbuf(), ERR_DEFHANDLER_JUMP_CODE);
}

// setjmp/longjmp API

/**
 * @brief Obtain a pointer to the current thread's exception environment.
 * @return Pointer to the thread-local ErrorExceptionData (valid for thread lifetime).
 */
ErrorExceptionData*
err_getexception_info(void)
{
    return &g_env;
}

int
err_count(void) {
    err_ensurebuf();
    return g_currerr;
}

bool
err_last(ErrorInfo *out) {
    err_ensurebuf();
    if (g_currerr == 0)
        return false;
    if (out)
		*out = g_error[g_currerr - 1].info;
       
    return true;
}

bool
err_pop(void) {
    err_ensurebuf();
    if (g_currerr == 0)
        return false;
    g_currerr--;
    return true;
}

// -------------------------- (API) printers -----------------------

/**
 * @brief Dump the full per-thread error stack to a stream.
 *
 * Each entry is printed as `[index]: TYPE: [code] message\n`.
 *
 * @param[out] out  Destination stream (e.g. stderr, stdout, a file).
 * @return Total number of characters written, or a negative value on I/O error.
 */
int
err_fprintstacktrace(FILE *out)
{
	int		res = 0;
	err_ensurebuf();
	logauto(g_currerr);

	res += fprintf(out, "\n------------- PRINT STACK TRACE START -----------\n\n");
	for (int i = 0; i < g_currerr; i++)
	{
		res += fprintf(out, "[%4d]: ", i);
		res += err_fprinterr(out, g_error + i);
	}

	res += fprintf(out, "\n------------- PRINT STACK TRACE END -------------\n\n");
	return logautoret(res);
}
