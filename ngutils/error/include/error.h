#ifndef ERROR_H
#define ERROR_H
// ---------------------------------------------------------------------------------
// @file    error.h
// @brief   Public API for the per-thread error stack and signal-based exceptions.
//
// Provides:
//  - A growable per-thread error record stack (err_raise, err_clean,
//    err_fprintstacktrace).
//  - A sigsetjmp/siglongjmp "exception" facility via the TRY() macro.
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
#define ERROR_VERSION_MINOR 2
#define ERROR_VERSION_PATCH 0

#define ERROR_STRINGIFY_(x) #x
#define ERROR_STRINGIFY(x)  ERROR_STRINGIFY_(x)

/**
 * @brief Current version as a string literal, e.g. @c "0.2.0" .
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
enum { ERR_MESSAGE_MAX_LENGTH = 1024 };

/** Maximum number of nested TRY() blocks (static, thread-safe, no heap). */
enum { ERR_CYCLE_CNT = 128 };

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
 * Supports up to @ref ERR_CYCLE_CNT nested TRY() blocks via a static
 * stack of sigjmp_bufs (thread-safe, no heap allocation). `err_raise()`
 * always longjmps to the most recent (innermost) capture site.
 *
 * Usage pattern (single level):
 * @code
 *   TRY() {
 *       do_work_that_may_raise();
 *   } else {
 *       handle_exception();
 *   }
 * @endcode
 *
 * Usage pattern (nested):
 * @code
 *   TRY() {                    // depth 0
 *       outer_work();
 *       TRY() {                // depth 1
 *           inner_work();      // longjmp lands at innermost (depth 1) first
 *       } else {
 *           // inner catch
 *       }
 *   } else {
 *       // outer catch
 *   }
 * @endcode
 *
 * `depth` tracks the number of currently active TRY() blocks.
 * `overallcnt` tracks how many times depth wrapped around
 * `ERR_CYCLE_CNT` (a very large number of nested blocks).
 */
typedef struct ErrorExceptionData
{
    sigjmp_buf              env[ERR_CYCLE_CNT];   ///< Stack of setjmp buffers.
    int                     depth;                  ///< Number of active TRY() blocks (0 = none).
    int                     overallcnt;             ///< Number of crossing ERR_CYCLE_CNT.
} ErrorExceptionData;

/**
 * @brief Type and code of a single error record (no message).
 */
typedef struct {
    ErrorType   type;   ///< ERR_USER or ERR_SYS.
    int         code;   ///< App code or errno, depending on type.
} ErrorInfo;


/**
 * @brief Convenience accessor for the thread-local exception environment.
 *
 * Expands to `(*err_getexception_info())`, giving lvalue access to the
 * `ErrorExceptionData` fields ( `.env[]`, `.depth` ).
 */
#define errenv              (*err_getexception_info())

// ------------- CONSTRUCTORS / DESTRUCTORS ----------

// -------------- ACCESS AND MODIFICATION ----------

/**
 * @brief Record an error on the per-thread stack and longjmp to the
 *        innermost active TRY() block.
 *
 * This is the central entry-point for all error reporting in the library.
 * The formatted message is pushed onto the per-thread error stack, then:
 *  - If an active TRY() block exists (`errenv.depth > 0`), the function
 *    performs `siglongjmp` to the innermost capture site.
 *  - If **no** TRY() block is active, the function raises `SIGINT` as a
 *    last-resort so a debugger or core dump can show what happened.
 *
 * @param tp       Error class: ERR_USER or ERR_SYS.
 * @param errcode  Application-level error code from ErrorCode.
 *                 Ignored when @p tp is ERR_SYS (errno is used instead).
 * @param msg      printf-style format string for the error message.
 * @param ...      Variadic arguments matching @p msg.
 *
 * @note The format string is validated at compile-time via
 *       `__attribute__((format(printf, 3, 4)))`.
 *
 * @note This function does **not** return normally when a TRY() block
 *       is active (it longjmps). When no TRY() is active it returns
 *       after raising SIGINT.
 */
extern void
err_raise(ErrorType tp, int errcode, const char *msg, ...)  __attribute__ ((format (printf, 3, 4)));

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
 * @return Pointer to the thread-local `ErrorExceptionData`.
 */
extern ErrorExceptionData*
err_getexception_info(void);

/**
 * @brief Reset the exception environment flag (inline convenience).
 *
 * Winds the try-stack depth to zero, effectively cancelling all active TRY() blocks.
 *
 */
static inline void
err_resetenv()
{
	errenv.depth = 0;
    errenv.overallcnt = 0;
}

/**
 * @brief Number of records currently on the per-thread error stack.
 * @return Count (>= 0).
 */
extern int
err_count(void);

/**
 * @brief Fetch type and code of the most recent record.
 *
 * @param[out] out  Destination; may be NULL if only existence matters.
 * @return true if a record exists, false if the stack is empty.
 */
extern bool
err_last(ErrorInfo *out);

/**
 * @brief Remove the most recent record from the stack.
 * @return true if a record was removed, false if the stack was empty.
 */
extern bool
err_pop(void);

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

static inline int
err_prevcnt(void) {
	if (--errenv.depth < 0) {
		errenv.depth = ERR_CYCLE_CNT - 1;
        errenv.overallcnt--;
    }
	return errenv.depth;
}

static inline int
err_nextcnt(void) {
	if (++errenv.depth >= ERR_CYCLE_CNT) {
		errenv.depth = 0;
        errenv.overallcnt++;
    }
	return errenv.depth;
}

static inline sigjmp_buf *
err_getcurrsigbuf () {
    return errenv.env + errenv.depth;
}

/**
 * @brief Begin a TRY block (setjmp-based exception catch point).
 *
 * Captures the current PC in `errenv.env[depth]` via `sigsetjmp` and
 * increments `errenv.depth`. On normal (first) entry the test value is
 * `0` and the `for` loop runs exactly once; on return from a
 * `siglongjmp` the test value is non-zero (the jump code, currently
 * `ERR_DEFHANDLER_JUMP_CODE` = 10) and the `for` loop body is skipped,
 * but its cleanup (`err_prevcnt()`) still runs — so `depth` is balanced
 * in both paths.
 *
 * **Usage (single level):**
 * @code
 *   TRY() {
 *       // normal path – code that may call err_raise()
 *       risky_operation();
 *   } else {
 *       // catch path – reached after siglongjmp from err_raise()
 *       err_printstacktrace();
 *   }
 * @endcode
 *
 * **Usage (nested):**
 * @code
 *   TRY() {
 *       outer_work();
 *       TRY() {
 *           inner_work();   // longjmp lands at innermost first
 *       } else {
 *           // inner catch
 *       }
 *   } else {
 *       // outer catch
 *   }
 * @endcode
 *
 * **Mechanics:**
 *  - `sigsetjmp(env, 1)` — the `1` tells the runtime to reset signal
 *    handlers to their saved state on longjmp.
 *  - `err_nextcnt()` — increments `errenv.depth`; if it wraps past
 *    `ERR_CYCLE_CNT` it re-wraps to 0 and increments `overallcnt`.
 *  - `err_prevcnt()` — decrements `errenv.depth` (the `for` cleanup);
 *    if it underflows below 0 it re-wraps to `ERR_CYCLE_CNT - 1` and
 *    decrements `overallcnt`.
 *
 * @note The `else` branch of the enclosing `if` is the "catch" path.
 *       It is reached only when `siglongjmp` (from `err_raise`) returns
 *       a non-zero value.
 */
#define TRY() \
    if (sigsetjmp(*err_getcurrsigbuf(), 1) == 0) \
        for (int _once = (err_nextcnt(), 1); \
             _once; \
             _once = (err_prevcnt(), 0))

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
 * @brief Internal: generic raise with cleanup ACTION.
 *
 * Shared implementation for all public raise macros. Performs:
 *  1. Saves `errno` into a local (so that ACTION cannot clobber it).
 *  2. If ERR_SYS – prints `strerror(saved_errno)` to log + stderr.
 *  3. Executes the ACTION statement (cleanup / rollback).
 *  4. Prints the user message to log + stderr.
 *  5. Restores `errno` (so `err_raise` sees the original value).
 *  6. Calls `err_raise()` which records the error and longjmps to the
 *     innermost active TRY() block (or raises SIGINT if none).
 *  7. Evaluates to @p retcode.
 *
 * @param retcode  Value the macro expands to (return / assignment target).
 * @param TYPE     ErrorType (ERR_USER or ERR_SYS).
 * @param ACTION   Statement to execute before raising (e.g. `free(p);`).
 *                 May be empty (`,`) for the no-action variants.
 * @param errcode  ErrorCode value.
 * @param msg      printf-style format string.
 * @param ...      Variadic arguments for @p msg.
 *
 * @note This is an internal macro; use the public wrappers below.
 */
#define	_generalraiseact(retcode, TYPE, ACTION, errcode, msg, ...) \
	({ 	typeof(retcode) _RETCODE = (retcode);\
        int _err_save = errno; \
        if (TYPE == ERR_SYS){ \
            _log_and_print("%s\t", strerror(_err_save)); \
            _log_and_print("%s", "\n"); \
        } \
        ACTION; \
        _log_and_print(msg,  ##__VA_ARGS__); \
        _log_and_print("%s", "\n"); \
        errno = _err_save; \
        err_raise(TYPE, errcode, msg, ##__VA_ARGS__); \
        _RETCODE; \
    })

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
#define _userraiseact(retcode, ACTION, errcode, msg, ...) \
    _generalraiseact(retcode, ERR_USER, ACTION, errcode, msg, ##__VA_ARGS__)

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
#define userraise(retcode, errcode, msg, ...) \
    _userraiseact(retcode, , errcode, msg, ##__VA_ARGS__)

// SYSTEM block
// system, with action and signal (common)

#define _sysraiseact(retcode, ACTION, msg, ...) \
    _generalraiseact(retcode, ERR_SYS, ACTION, 0, msg, ##__VA_ARGS__)

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
#define sysraiseact(retcode, ACTION, msg, ...) \
    _sysraiseact(retcode, ACTION, msg, ##__VA_ARGS__)

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
#define sysraise(retcode, msg, ...) \
    _sysraiseact(retcode, , msg, ##__VA_ARGS__)

#endif /* !ERROR_H */
