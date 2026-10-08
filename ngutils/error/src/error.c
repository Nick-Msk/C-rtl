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

// static globals

/** Growth step (number of Error slots) used when the heap buffer is extended. */
static const int                        ERROR_DEFAULT_INCREMENT  = 16;

/** Size of the static (stack-allocated) initial buffer. */
static const int						ERROR_INIT_COUNT		 = 10;

/** Jump code passed via longjmp from the default signal handler. */
static const int                    	ERR_DEFHANDLER_JUMP_CODE = 10;

// internal types

/**
 * @brief A single error record in the per-thread stack.
 */
typedef struct {
    ErrorType   type;                            ///< Error class (ERR_USER / ERR_SYS).
    int         code;                            ///< Numeric code (app code or errno).
    char        msg[ERROR_MESSAGE_MAX_LENGTH];   ///< Formatted human-readable message.
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
	logsimple("%s", bool_str(g_error == g_error_init));
	return g_error == g_error_init;
}

/**
 * @brief Raise a signal, falling back to SIGTERM on failure.
 * @param sig  Signal number to deliver.
 * @return     Always -1 (caller should treat this as "flow interrupted").
 */
static int
err_raisesig(int sig)
{
	logsimple("raise %d [%s: %s]", sig, sig_str(sig), sig_str_desc(sig));
	if (raise(sig) == -1)
	{
		logsimple("unable to raise %d signal, will try to raise SIGTERM %d", sig, SIGTERM);
		if (raise(SIGTERM) == -1)
			logsimple("unable to raise SIGTERM");
	}
	return -1;
}

/**
 * @brief Grow the per-thread error array by one increment block.
 *
 * First call allocates a new heap array and copies existing entries.
 * Subsequent calls use realloc.
 * @return New capacity on success, 0 on allocation failure.
 */
static int
err_increase(void)
{
    Error   *err;
    int      newalloc = g_allocerr + ERROR_DEFAULT_INCREMENT;

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
 * If the stack is full the buffer is grown; on growth failure the record
 * is silently dropped.
 *
 * @param tp       Error class.
 * @param errcode  Application-level code (ignored for ERR_SYS; errno used).
 * @param msg      printf-style format string.
 * @param ap       Already-started va_list matching msg.
 */
static void
err_put(ErrorType tp, int errcode, const char *msg, va_list ap)
{
	// TEMPORARY SOLUTION TO RESOLV thread local
	if (g_error == NULL) {
		g_error = g_error_init;
		g_currerr = 0, g_allocerr = ERROR_INIT_COUNT; 
	}

	if (g_currerr >= g_allocerr) {
        if (!err_increase()) {
            // Если не смогли расшириться — просто не записываем новую ошибку
            return; 
        }
    }

	Error 	*err = g_error + g_currerr++;		// currect error, must be valid pointer
	logauto(err->type = tp);
	logsimple("%s", msg);	// logsimple(msg, ap) ??? TODO:

	switch (tp){
		case ERR_USER:
			err->code = errcode;
			vsnprintf(err->msg, sizeof err->msg, msg, ap);
		break;
		case ERR_SYS:
			err->code = errno;		// system error number
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

//  setjmp/longsmp API

/**
 * @brief Default SIGINT handler: longjmp back to the active try() site.
 *
 * If no try() environment is active:
 *  - SIGINT → silently ignored.
 *  - any other signal → raise SIGSTOP to park the process.
 *
 * @param sig  Signal number that triggered this handler.
 */
static void
err_default_handler(int sig)
{
    logsimple("HANDLER %d", sig);
    if (!errenv.init_flag)
    {
        logsimple("Env buffer is empty");
		if (sig == SIGINT)
			logsimple("No env buffer - just working as igrone SIGINT");
		else {
			logsimpleerr(0, "terminating SIGSTOP");
        	raise(SIGSTOP);
		}
    } else {
        logsimple("make a longjmp... init flag %s, code %d", bool_str(errenv.init_flag), ERR_DEFHANDLER_JUMP_CODE);
    	longjmp(errenv.env, ERR_DEFHANDLER_JUMP_CODE);
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
 * @brief Record an error and optionally raise a signal.
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
err_raise(ErrorType tp, int raise, int errcode, const char *msg, ...)
{
	// put data into stack and raise sig (????)
	if (g_currerr >= g_allocerr)
		if (!err_increase())
		{
			logsimpleact(fprintf(stderr, "Unable to allocate more size\n"), "Unable");		// ?? TODO: logsimple ? or logstderr
			err_raisesig(SIGTERM);
		}

	va_list		ap;
	va_start(ap, msg);

	err_put(tp, errcode, msg, ap);
	va_end(ap);

	if (raise)
		err_raisesig(raise);
}

// setjmp/longjmp API

/**
 * @brief Obtain a pointer to the current thread's exception environment.
 * @return Pointer to the thread-local ExceptionData (valid for thread lifetime).
 */
ExceptionData*
err_getexception_info()
{
    return &g_env;      // access to global
}

/**
 * @brief Install a signal handler for SIGINT.
 *
 * Passing NULL installs the built-in err_default_handler (longjmp-based).
 *
 * @param handler  User handler, or 0/NULL for the default.
 * @return true on success; false (and an error is raised) on failure.
 */
bool
err_sethandler(sig_t handler)
{
    if (!handler)
        handler = err_default_handler;

    if (signal(SIGINT, handler) == SIG_ERR)
        return sysraise(false, "Unable to setup err_default_handler for SIGINT\n");

    return logsimpleret(true, "err_default_handler is activated\n");
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
