#ifndef ERROR_H
#define ERROR_H
// ---------------------------------------------------------------------------------
// @file    error.h
// @brief   Public API for the per-thread error stack and signal-based exceptions.
//
// Provides:
//  - A growable per-thread error record stack (err_raise, err_clean,
//    err_fprintstacktrace).
//  - A setjmp/longjmp "exception" facility via try() and err_sethandler().
//  - Convenience macro families: userraise / sysraise / *act* / *int*.
//
// @see     error.c for implementation details.
// ---------------------------------------------------------------------------------

#include <stdio.h>
#include <stdbool.h>
#include <signal.h>
#include <setjmp.h>
#include <string.h>
#include <sys/errno.h>

#include "log.h"

/* ─────────────────────────────────────────────────────────────────────────
 * Version
 * ───────────────────────────────────────────────────────────────────────── */

#define ERROR_VERSION_MAJOR 0
#define ERROR_VERSION_MINOR 1
#define ERROR_VERSION_PATCH 0

#define ERROR_STRINGIFY_(x) #x
#define ERROR_STRINGIFY(x)  ERROR_STRINGIFY_(x)

/**
 * @brief Current version as a string literal, e.g. @c "0.1.0" .
 */
#define ERROR_VERSION \
    ERROR_STRINGIFY(ERROR_VERSION_MAJOR) "." \
    ERROR_STRINGIFY(ERROR_VERSION_MINOR) "." \
    ERROR_STRINGIFY(ERROR_VERSION_PATCH)

/**
 * @brief Current version string, e.g. @c "0.1.0" .
 *
 * Equivalent to @c *err_versions() .
 *
 * @return Pointer to a static string literal; do not free.
 */
extern const char *err_version(void);

/**
 * @brief Published versions, newest first, terminated by @c NULL .
 *
 * @c versions[0] is always the current version. Array and elements are
 * read-only; do not free.
 *
 * @return Pointer to a static, NULL-terminated array of strings.
 */
extern const char *const *err_versions(void);

// ----------- CONSTANTS AND GLOBALS ---------------

/** Maximum length (including NUL terminator) of a single formatted error message. */
enum { ERROR_MESSAGE_MAX_LENGTH = 512 };

/** Maximum nesting depth of try() blocks (static, thread-safe, no heap). */
enum { ERR_MAX_TRY_CNT = 16 };

// ------------------- TYPES -----------------------

/**
 * @brief Application-level error and warning codes.
 *
 * Grouped by numeric range:
 *  - 10 – 30    : core / parameter validation
 *  - 50 – 70    : file & stream I/O
 *  - 100 – 200  : invariants / unimplemented features
 *  - 1001       : warnings (non-fatal)
 *  - 10001+     : domain-specific (fs, generators, interfaces)
 */
typedef enum {
    // --- Core / Standard Errors ---
    ERR_NULLABLE_PTR            = 10,   ///< A pointer argument is NULL where non-NULL is required.
    ERR_OUT_OF_RANGE            = 11,   ///< A numeric value is outside the valid range.
    ERR_OUT_OF_BUFFER           = 12,   ///< An operation would exceed a buffer boundary.
    ERR_UNABLE_ALLOCATE         = 15,   ///< Memory allocation (malloc/calloc/realloc) failed.
    ERR_NULL_OUTPUT             = 16,   ///< Output pointer/stream is NULL.
    ERR_NULL_INPUT              = 17,   ///< Input pointer/data is NULL.

    ERR_WRONG_INPUT_FORMAT      = 20,   ///< Input does not match the expected format.
    ERR_NOT_ENOUGH_VALUES        = 21,   ///< Not enough values were provided.
    ERR_WRONG_PARAMETER         = 22,   ///< A parameter value is semantically wrong.
    ERR_TYPES_MISMATCH          = 23,   ///< Unexpected type was passed.
    ERR_INVALID_BINARY_DATA     = 24,   ///< Binary data failed integrity validation.

    // --- File / Stream Errors ---
    ERR_UNABLE_OPEN_FILE        = 50,   ///< Could not open a file (generic).
    ERR_UNABLE_OPEN_FILE_READ   = 51,   ///< Could not open a file for reading.
    ERR_UNABLE_OPEN_FILE_WRITE  = 52,   ///< Could not open a file for writing.
    ERR_STREAM_ERROR            = 53,   ///< Generic stream I/O error.
    ERR_IRREGULAR_STREAM        = 54,   ///< Stream is in an inconsistent state.
    ERR_UNABLE_SET_FILE_PARAM   = 55,   ///< Could not set a file/stream parameter.
    ERR_CANT_GET_STAT           = 56,   ///< stat() / fstat() failed.

    ERR_WRONG_INPUT_PARAMETERS  = 60,   ///< Combination of input parameters is invalid.
    ERR_SHELL_NOT_AVAILABLE     = 65,   ///< Required shell / command is not available.

    ERRNUM_INVARIANT_VIOLATION  = 100,  ///< Internal invariant was violated (logic bug).
    ERR_NOT_IMPLEMENTED_FEATURE = 200,  ///< Requested feature is not yet implemented.

    // --- Warnings (non-fatal) ---
    WARN_MEM_LEAK_DETECTED      = 1001,     ///< Memory leak detected during diagnostics.

    // --- Domain-specific ---
    ERR_FS_NOT_ALLOC_FLAG       = 10001,    ///< FastString alloc flag is not set.
    ERR_TOO_LONG_LINE           = 10010,    ///< Input line exceeds the maximum allowed length.
    ERR_GUARD_RAISE             = 10100,    ///< A guard / assertion was triggered.
    ERR_ACTION_NOT_APPLICABLE   = 10200,    ///< Requested action is not applicable in current state.
    ERR_UNABLE_ALLOCATE_SEQ     = 10201,    ///< Could not allocate a sequence container.
    ERR_UNABLE_LOAD_FSARRAY     = 10220,    ///< Could not load a FastStringArray.

    ERR_UNSUPPORTED_TYPE        = 10230,    ///< Requested type is not supported by the subsystem.
    ERR_UNSUPPORTED_TYPE_CONV   = 10231,    ///< Type conversion for the given type is not supported.
    ERR_INVALID_CONVERSION      = 10232,    ///< Type conversion produced an invalid result.
    ERR_UNKNOWN_TYPE            = 10233,    ///< Encountered an unrecognised type tag.
    ERR_UNABLE_PARSE_DATA       = 10234,    ///< Could not parse the supplied data.
    ERR_VALIDATION_FAILED       = 10235,    ///< Data failed validation checks.
    ERR_UNSUPPORTED_GENERATOR   = 10236,    ///< Requested generator type is not supported.
    ERR_UNSUPPORTED_INTERFACE   = 10237,    ///< Requested interface is not supported.

    ERR_UNABLE_TO_EXEC_FILE     = 10300,    ///< Could not execute the specified file.
    ERR_UNABLE_TO_RUN_MAKE      = 10301,    ///< Could not invoke the build system (make).
} ErrorCode;

/**
 * @brief Error class: user-defined (application) vs. system (errno).
 *
 * Determines how the message is rendered in err_put():
 *  - ERR_USER → the format string is used as-is.
 *  - ERR_SYS  → strerror(errno) is prepended, then ": " + format string.
 */
typedef enum {ERR_USER = 1, ERR_SYS} ErrorType;

/**
 * @brief Thread-local environment for the setjmp/longjmp "exception" mechanism.
 *
 * Supports up to @ref ERR_MAX_TRY_CNT nested try() blocks via a static
 * stack of jmp_bufs (thread-safe, no heap allocation). The signal handler
 * always longjmps to the most recent (innermost) capture site.
 *
 * Usage pattern (single level):
 * @code
 *   if (try() == 0) {
 *       do_work_that_may_raise();
 *   } else {
 *       handle_exception();
 *   }
 * @endcode
 *
 * Usage pattern (nested):
 * @code
 *   if (try() == 0) {          // depth 0
 *       outer_work();
 *       if (try() == 0) {      // depth 1
 *           inner_work();      // longjmp lands at innermost (depth 1) first
 *       } else {
 *           // inner catch
 *       }
 *   } else {
 *       // outer catch
 *   }
 * @endcode
 *
 * `depth` tracks the number of currently active try() blocks. The signal
 * handler checks this before longjmp'ing to `env[depth-1]`.
 */
typedef struct ExceptionData
{
    jmp_buf                 env[ERR_MAX_TRY_CNT];   ///< Stack of setjmp buffers.
    volatile sig_atomic_t   depth;                  ///< Number of active try() blocks (0 = none).
} ExceptionData;

// ------------- CONSTRUCTORS / DESTRUCTORS ----------

// -------------- ACCESS AND MODIFICATION ----------

/**
 * @brief Record an error on the per-thread stack and optionally raise a signal.
 *
 * This is the central entry-point for all error reporting in the library.
 * The formatted message is pushed onto the per-thread error stack.
 * If @p raise is non-zero the corresponding signal is delivered to the
 * current process (which may be intercepted by an active `try()` block).
 *
 * @param tp       Error class: ERR_USER or ERR_SYS.
 * @param raise    Signal number to raise (e.g. SIGINT, SIGTERM), or 0 to
 *                 suppress the signal (record-only).
 * @param errcode  Application-level error code from ErrorCode.
 *                 Ignored when @p tp is ERR_SYS (errno is used instead).
 * @param msg      printf-style format string for the error message.
 * @param ...      Variadic arguments matching @p msg.
 *
 * @note The format string is validated at compile-time via
 *       `__attribute__((format(printf, 4, 5)))`.
 */
extern void
err_raise(ErrorType tp, int raise, int errcode, const char *msg, ...)  __attribute__ ((format (printf, 4, 5)));

/**
 * @brief Reset the per-thread error stack to its initial (empty) state.
 *
 * @param force  When `true` and a heap-allocated buffer is in use, it is
 *               freed. When `false`, indices are rewound but the heap
 *               memory is retained for future use.
 */
extern void
err_clean(bool force);

/**
 * @brief Obtain a pointer to the current thread's exception environment.
 *
 * The returned pointer is valid for the lifetime of the calling thread.
 * Use the `errenv` macro for convenient access.
 *
 * @return Pointer to the thread-local `ExceptionData`.
 */
extern ExceptionData*
err_getexception_info();

/**
 * @brief Install a signal handler for SIGINT.
 *
 * Passing NULL (0) installs the built-in default handler which performs a
 * `longjmp` back to the most recent `try()` call-site.
 *
 * @param handler  User-supplied handler, or 0/NULL for the built-in default.
 * @return `true` on success; `false` (and a system error is raised) on failure.
 */
extern bool
err_sethandler(sig_t handler);

/**
 * @brief Reset the exception environment flag (inline convenience).
 *
 * Wounds the try-stack depth to zero, effectively cancelling all active try() blocks.
 *
 * @return Always `false` (depth is now 0).
 */
static inline bool
err_resetenv()
{
	return (err_getexception_info()->depth = 0);
}

// ----------------- PRINTERS ----------------------

/**
 * @brief Dump the full per-thread error stack (stack trace) to a stream.
 *
 * Each entry is printed as: `[index]: TYPE: [code] message\n`
 *
 * @param[out] out  Destination stream (e.g. stderr, stdout, a file).
 * @return Total number of characters written, or a negative value on I/O error.
 */
extern int
err_fprintstacktrace(FILE *out);

/**
 * @brief Print the error stack to stderr (convenience wrapper).
 * @return Same as err_fprintstacktrace(stderr).
 */
static inline int
err_printstacktrace(void)
{
	return err_fprintstacktrace(stderr);
}

// -------------- SIGNAL HELPERS --------------------

/**
 * @brief Convert a signal number to its symbolic name.
 * @param signal  Signal number (e.g. SIGINT).
 * @return Static string like "SIGINT", "SIGSEGV", or "Unknown sig".
 */
static inline const char *
sig_str(int signal)
{
    switch(signal){
        case SIGHUP     : return "SIGHUP";
        case SIGINT     : return "SIGINT";
        case SIGQUIT    : return "SIGQUIT";
        case SIGILL     : return "SIGILL";
        case SIGTRAP    : return "SIGTRAP";
        case SIGABRT    : return "SIGABRT";
#ifdef SIGEMT
        case SIGEMT     : return "SIGEMT";
#endif        
        case SIGFPE     : return "SIGFPE";
        case SIGKILL    : return "SIGKILL";
        case SIGBUS     : return "SIGBUS";
        case SIGSEGV    : return "SIGSEGV";
        case SIGSYS     : return "SIGSYS";
        case SIGPIPE    : return "SIGPIPE";
        case SIGALRM    : return "SIGALRM";
        case SIGTERM    : return "SIGTERM";
        case SIGURG     : return "SIGURG";
        case SIGSTOP    : return "SIGSTOP";
        case SIGTSTP    : return "SIGTSTP";
        case SIGCONT    : return "SIGCONT";
        case SIGCHLD    : return "SIGCHLD";
        case SIGTTIN    : return "SIGTTIN";
        case SIGTTOU    : return "SIGTTOU";
        case SIGIO      : return "SIGIO";
        case SIGXCPU    : return "SIGXCPU";
        case SIGXFSZ    : return "SIGXFSZ";
        case SIGVTALRM  : return "SIGVTALRM";
        case SIGPROF    : return "SIGPROF";
        case SIGWINCH   : return "SIGWINCH";
#ifdef SIGINFO
        case SIGINFO    : return "SIGINFO";
#endif        
        case SIGUSR1    : return "SIGUSR1";
        case SIGUSR2    : return "SIGUSR2";
        default         : return "Unknown sig";
    }
}

/**
 * @brief Convert a signal number to a human-readable description.
 * @param signal  Signal number.
 * @return Short description string (e.g. "interrupt program").
 */
static inline const char *
sig_str_desc(int signal)
{
    switch(signal){
        case SIGHUP     : return "terminal line hangup";
        case SIGINT     : return "interrupt program";
        case SIGQUIT    : return "quit program";
        case SIGILL     : return "illegal instruction";
        case SIGTRAP    : return "trace trap";
        case SIGABRT    : return "abort program (formerly SIGIOT)";
#ifdef SIGEMT
        case SIGEMT     : return "emulate instruction executed";
#endif        
        case SIGFPE     : return "floating-point exception";
        case SIGKILL    : return "kill program";
        case SIGBUS     : return "bus error";
        case SIGSEGV    : return "segmentation violation";
        case SIGSYS     : return "non-existent system call invoked";
        case SIGPIPE    : return "write on a pipe with no reader";
        case SIGALRM    : return "real-time timer expired";
        case SIGTERM    : return "software termination signal";
        case SIGURG     : return "urgent condition present on socket";
        case SIGSTOP    : return "stop (cannot be caught or ignored)";
        case SIGTSTP    : return "stop signal generated from keyboard";
        case SIGCONT    : return "continue after stop";
        case SIGCHLD    : return "child status has changed";
        case SIGTTIN    : return "background read attempted from control terminal";
        case SIGTTOU    : return "background write attempted to control terminal";
        case SIGIO      : return "I/O is possible on a descriptor";
        case SIGXCPU    : return "cpu time limit exceeded";
        case SIGXFSZ    : return "file size limit exceeded";
        case SIGVTALRM  : return "virtual time alarm";
        case SIGPROF    : return "profiling timer alarm";
        case SIGWINCH   : return "Window size change";
#ifdef SIGINFO
        case SIGINFO    : return "status request from keyboard";
#endif        
        case SIGUSR1    : return "User defined signal 1";
        case SIGUSR2    : return "User defined signal 2";
        default         : return "";
    }
}

// ------------------ ETC. -------------------------

// setjmp/longjmp API

/**
 * @brief Convenience accessor for the thread-local exception environment.
 *
 * Expands to `(*err_getexception_info())`, giving lvalue access to the
 * `ExceptionData` fields ( `.env[]`, `.depth` ).
 */
#define errenv              (*err_getexception_info())

/**
 * @brief Install the default signal handler (shorthand for err_sethandler(0)).
 * @return Same as err_sethandler(0): true on success, false on failure.
 */
#define errsethandler()     err_sethandler(0)

/**
 * @brief Begin a "try" block (setjmp-based exception catch point).
 *
 * This GCC statement-expression macro captures the current PC in
 * `errenv.env` via `setjmp`. It returns:
 *  - `0`  on the **first** pass (normal execution, "try" body).
 *  - A **non-zero** jump code on return from a `longjmp` in the signal
 *    handler (entering the "catch" / else branch).
 *
 * **Usage:**
 * @code
 *   errsethandler();  // install handler once
 *   if (try() == 0) {
 *       // normal path – code that may trigger a signal
 *       risky_operation();
 *   } else {
 *       // catch path – reached after longjmp
 *       err_printstacktrace();
 *   }
 * @endcode
 *
 * @note If `errenv.depth >= ERR_MAX_TRY_CNT` (too many nested try blocks),
 *       the macro returns a sentinel (9999) to signal a programming error.
 *
 * @return 0 on first entry; non-zero (jump code) when returning from longjmp.
 */
#define try() ({\
	int res;\
    if (errenv.depth >= ERR_MAX_TRY_CNT)\
        logsimpleact(res = 9999, "Env buf is already activated");\
	else {\
    	res = setjmp(errenv.env[errenv.depth]);\
    	if (res == 0)\
    		errenv.depth++;\
    	else\
    		errenv.depth--;\
    }\
    res;\
})


/**
 * @brief Log a formatted message to the internal log AND to stderr.
 *
 * Used internally by the raise macros to ensure the error is visible both
 * in the application log and immediately on the terminal.
 *
 * @param msg  printf-style format string.
 * @param ...  Variadic arguments matching @p msg.
 */
#define _log_and_print(msg, ...) { logsimple(msg, ##__VA_ARGS__); fprintf(stderr, msg, ##__VA_ARGS__); /* fprintf(stderr, "\n"); */}

/**
 * @brief Internal: generic raise with ACTION and signal.
 *
 * Shared implementation for all public raise macros. Performs:
 *  1. If ERR_SYS – prints `strerror(errno)` to log + stderr.
 *  2. Executes the ACTION statement (cleanup / rollback).
 *  3. Prints the user message to log + stderr.
 *  4. Calls `err_raise()` to record the error and optionally raise a signal.
 *  5. Evaluates to @p retcode.
 *
 * @param retcode  Value the macro expands to (return / assignment target).
 * @param TYPE     ErrorType (ERR_USER or ERR_SYS).
 * @param ACTION   Statement to execute before raising (e.g. `free(p);`).
 * @param sig      Signal to raise, or 0 to suppress.
 * @param errcode  ErrorCode value.
 * @param msg      printf-style format string.
 * @param ...      Variadic arguments for @p msg.
 *
 * @note This is an internal macro; use the public wrappers below.
 */
#define	_generalraiseactsig(retcode, TYPE, ACTION, sig, errcode, msg, ...)	({ 	typeof(retcode) _RETCODE = (retcode);\
                                                                                if (TYPE == ERR_SYS){\
                                                                                    _log_and_print("%s\t", strerror(errno));\
                                                                                    _log_and_print("%s", "\n");\
                                                                                }\
																				ACTION;\
                                                                                _log_and_print(msg,  ##__VA_ARGS__);\
                                                                                _log_and_print("%s", "\n");\
																				err_raise(TYPE, sig, errcode, msg, ##__VA_ARGS__);\
																				_RETCODE;\
																			})

// USER block
// user with ACTION and signal (common)

/**
 * @brief Internal: user error with ACTION and explicit signal.
 *
 * @param retcode  Value to return/assign after the raise.
 * @param ACTION   Cleanup statement (e.g. `free(buf);`).
 * @param sig      Signal number to raise (e.g. SIGINT), or 0.
 * @param errcode  ErrorCode value.
 * @param msg      printf-style format string.
 * @param ...      Variadic arguments.
 */
#define _userraiseactsig(retcode, ACTION, sig, errcode, msg, ...)	_generalraiseactsig(retcode, ERR_USER, ACTION, sig	, errcode, msg, ##__VA_ARGS__)

/**
 * @brief Raise a user error with a cleanup ACTION, no signal.
 *
 * Records the error, executes @p ACTION, and evaluates to @p retcode.
 * No signal is raised; control returns normally.
 *
 * **Usage:**
 * @code
 *   int *p = malloc(n);
 *   if (!p) return userraiseact(0, free(p), ERR_UNABLE_ALLOCATE, "alloc %d failed", n);
 * @endcode
 *
 * @param retcode  Value the expression evaluates to.
 * @param ACTION   Statement executed before the raise (cleanup / rollback).
 * @param errcode  ErrorCode value.
 * @param msg      printf-style format string.
 * @param ...      Variadic arguments for @p msg.
 * @return         @p retcode (after side-effects).
 */
#define	userraiseact(retcode, ACTION, errcode, msg, ...)			_userraiseactsig(retcode, ACTION, 0					, errcode, msg, ##__VA_ARGS__)

/**
 * @brief Raise a user error with cleanup ACTION and SIGINT (interrupt/exception).
 *
 * Same as userraiseact but raises SIGINT, which will be intercepted by
 * an active `try()` block (longjmp).
 *
 * **Usage:**
 * @code
 *   if (try() == 0) {
 *       userraiseactint(free(p), ERR_GUARD_RAISE, "invariant broken");
 *       // normal path...
 *   } else {
 *       // caught here
 *   }
 * @endcode
 *
 * @param ACTION   Cleanup statement.
 * @param errcode  ErrorCode value.
 * @param msg      printf-style format string.
 * @param ...      Variadic arguments.
 * @return         0 (always; signal causes the actual control transfer).
 */
#define userraiseactint(ACTION, errcode, msg, ...)         			_userraiseactsig(0, ACTION, SIGINT					, errcode, msg, ##__VA_ARGS__)

/**
 * @brief Internal: user error with signal, no ACTION.
 *
 * @param retcode  Value to return/assign after the raise.
 * @param sig      Signal number to raise, or 0.
 * @param errcode  ErrorCode value.
 * @param msg      printf-style format string.
 * @param ...      Variadic arguments.
 */
#define _userraisesig(retcode, sig, errcode, msg, ...)				_userraiseactsig(retcode, , sig						, errcode, msg, ##__VA_ARGS__)

/**
 * @brief Raise a user error, no ACTION, no signal.
 *
 * Records the error and evaluates to @p retcode. Control returns normally.
 *
 * **Usage:**
 * @code
 *   if (!ptr) return userraise(0, ERR_NULLABLE_PTR, "ptr is NULL in %s", __func__);
 * @endcode
 *
 * @param retcode  Value the expression evaluates to.
 * @param errcode  ErrorCode value.
 * @param msg      printf-style format string.
 * @param ...      Variadic arguments.
 * @return         @p retcode.
 */
#define userraise(retcode, errcode, msg, ...)          				_userraisesig(retcode, 0							, errcode, msg, ##__VA_ARGS__)

/**
 * @brief Raise a user error with SIGINT (exception / interrupt), no ACTION.
 *
 * Records the error and raises SIGINT. If a `try()` is active, control
 * transfers to the catch block via longjmp.
 *
 * **Usage:**
 * @code
 *   if (invalid) userraiseint(ERR_INVALID_BINARY_DATA, "bad magic at offset %d", off);
 * @endcode
 *
 * @param errcode  ErrorCode value.
 * @param msg      printf-style format string.
 * @param ...      Variadic arguments.
 * @return         0 (signal causes actual control transfer).
 */
#define	userraiseint(errcode, msg, ...)								_userraisesig(0, SIGINT								, errcode, msg, ##__VA_ARGS__)


// SYSTEM block
// system, with action and signal (common)

/**
 * @brief Internal: system error with ACTION and signal.
 *
 * For system errors the numeric code is always `errno` (passed as 0 to
 * `_generalraiseactsig` which substitutes it internally).
 *
 * @param retcode  Value to return/assign after the raise.
 * @param ACTION   Cleanup statement.
 * @param sig      Signal to raise, or 0.
 * @param msg      printf-style format string (strerror is prepended automatically).
 * @param ...      Variadic arguments.
 */
#define	_sysraiseactsig(retcode, ACTION, sig, msg, ...)      		_generalraiseactsig(retcode, ERR_SYS, ACTION, sig, 0, msg, ##__VA_ARGS__)

/**
 * @brief Raise a system error with cleanup ACTION, no signal.
 *
 * Records `errno` + message, executes @p ACTION, evaluates to @p retcode.
 *
 * **Usage:**
 * @code
 *   FILE *f = fopen(path, "r");
 *   if (!f) return sysraiseact(NULL, fclose(f), "cannot open %s", path);
 * @endcode
 *
 * @param retcode  Value the expression evaluates to.
 * @param ACTION   Cleanup statement.
 * @param msg      printf-style format string (appended after strerror).
 * @param ...      Variadic arguments.
 * @return         @p retcode.
 */
#define	sysraiseact(retcode, ACTION, msg, ...)            			_sysraiseactsig(retcode, ACTION, 0					, msg, ##__VA_ARGS__)

/**
 * @brief Raise a system error with cleanup ACTION and SIGINT (exception).
 *
 * Same as sysraiseact but raises SIGINT for try() interception.
 *
 * @param ACTION   Cleanup statement.
 * @param msg      printf-style format string.
 * @param ...      Variadic arguments.
 * @return         0.
 */
#define	sysraiseactint(ACTION, msg, ...)							_sysraiseactsig(0, ACTION, SIGINT					, msg, ##__VA_ARGS__)

/**
 * @brief Internal: system error with signal, no ACTION.
 *
 * @param retcode  Value to return/assign after the raise.
 * @param sig      Signal to raise, or 0.
 * @param msg      printf-style format string.
 * @param ...      Variadic arguments.
 */
#define	_sysraisesig(retcode, sig, msg, ...)               			_sysraiseactsig(retcode, , sig						, msg, ##__VA_ARGS__)

/**
 * @brief Raise a system error, no ACTION, no signal.
 *
 * Records `errno` + message, evaluates to @p retcode. Control returns normally.
 *
 * **Usage:**
 * @code
 *   if (write(fd, buf, n) == -1) return sysraise(0, "write to fd %d failed", fd);
 * @endcode
 *
 * @param retcode  Value the expression evaluates to.
 * @param msg      printf-style format string (strerror prepended).
 * @param ...      Variadic arguments.
 * @return         @p retcode.
 */
#define sysraise(retcode, msg, ...)									_sysraisesig(retcode, 0								, msg, ##__VA_ARGS__)

/**
 * @brief Raise a system error with SIGINT (exception / interrupt), no ACTION.
 *
 * Records `errno` + message and raises SIGINT. If a `try()` is active,
 * control transfers to the catch block via longjmp.
 *
 * **Usage:**
 * @code
 *   if (read(fd, buf, sz) == -1) sysraiseint("unexpected read failure on fd %d", fd);
 * @endcode
 *
 * @param msg      printf-style format string.
 * @param ...      Variadic arguments.
 * @return         0 (signal causes actual control transfer).
 */
#define sysraiseint(msg, ...)										_sysraisesig(0, SIGINT								, msg, ##__VA_ARGS__)

#endif /* !ERROR_H */
