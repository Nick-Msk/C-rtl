#ifndef CHECKER_H
#define CHECKER_H
// ---------------------------------------------------------------------------------
// --------------------------- Public Checker API ----------------------------------
// ---------------------------------------------------------------------------------

#include <stdio.h>
#include <stdbool.h>

#include "log.h"
#include "error.h"

/* ─────────────────────────────────────────────────────────────────────────
 * Version
 * ───────────────────────────────────────────────────────────────────────── */

#define CHECKER_VERSION_MAJOR 0
#define CHECKER_VERSION_MINOR 1
#define CHECKER_VERSION_PATCH 0

#define CHECKER_STRINGIFY_(x) #x
#define CHECKER_STRINGIFY(x)  CHECKER_STRINGIFY_(x)

/**
 * @brief Current version as a string literal, e.g. @c "0.1.0" .
 */
#define CHECKER_VERSION \
    CHECKER_STRINGIFY(CHECKER_VERSION_MAJOR) "." \
    CHECKER_STRINGIFY(CHECKER_VERSION_MINOR) "." \
    CHECKER_STRINGIFY(CHECKER_VERSION_PATCH)

/**
 * @brief Current version string, e.g. @c "0.1.0" .
 *
 * Equivalent to @c *checker_versions() .
 *
 * @return Pointer to a static string literal; do not free.
 */
extern const char *checker_version(void);

/**
 * @brief Published versions, newest first, terminated by @c NULL .
 *
 * @c versions[0] is always the current version. Array and elements are
 * read-only; do not free.
 *
 * @return Pointer to a static, NULL-terminated array of strings.
 */
extern const char *const *checker_versions(void);

// -------------- ACCESS AND MODIFICATION ----------

/**
 * @brief Print the fact that an invariant was violated.
 *
 * Only handles the "skeleton" of the failure. Values are printed by the
 * per-type converters below (inv_int_bool, inv_double_bool, ...).
 *
 * @param out   Destination stream (may be NULL — then only logs).
 * @param expr  Stringified invariant expression.
 * @param res   Result of the invariant (false = violated).
 * @param msg   printf-style message.
 * @param ...   Args for @p msg.
 * @return      Number of characters written to @p out .
 */
int inv_fprintf_int(FILE *restrict out, const char *restrict expr,
                    bool res, const char *restrict msg, ...)
    __attribute__((format(printf, 4, 5)));

// ----------------- PRINTERS ----------------------


// ------------- PER-TYPE CONVERTERS ---------------

/**
 * @brief Equality converter for @c int pairs.
 * @return true if equal; otherwise prints both values and returns false.
 */
bool inv_char_bool(FILE *out, char res, char val);

bool inv_int_bool(FILE *out, int res, int val);

bool inv_uint_bool(FILE *out, unsigned res, unsigned val);

bool inv_long_bool(FILE *out, long res, long val);

bool inv_ulong_bool(FILE *out, unsigned long res, unsigned long val);

bool inv_double_bool(FILE *out, double res, double val);

bool inv_str_bool(FILE *out, const char *res, const char *val);

bool inv_ptr_bool(FILE *out, const void *res, const void *val);

/**
 * @brief _Generic dispatcher: picks a converter by the static type of @p a .
 *
 * Both arguments must have compatible types; the dispatcher looks at @p a .
 */
#define inv_any_bool(out, a, b) _Generic((a),     \
    char:               inv_char_bool,            \
    int:                inv_int_bool,             \
    long:               inv_long_bool,            \
    long long:          inv_long_bool,            \
    unsigned:           inv_uint_bool,            \
    unsigned long:      inv_ulong_bool,           \
    unsigned long long: inv_long_bool,            \
    float:              inv_double_bool,          \
    double:             inv_double_bool,          \
    char *:             inv_str_bool,             \
    const char *:       inv_str_bool,             \
    void *:             inv_ptr_bool,             \
    const void *:       inv_ptr_bool,              \
    default:            inv_ptr_bool              \
)(out, (a), (b))

// ------------------ ETC. -------------------------

/** @internal Boolean invariant engine (inv / invraise). */
#define _invraise(exception, do_raise, expr, msg, ...)                \
    ({  bool _INV_RES = (expr);                                       \
        if (!_INV_RES) {                                              \
            inv_fprintf_int(stderr, #expr, _INV_RES,                  \
                            msg, ##__VA_ARGS__);                      \
            if (do_raise) {                                           \
                fflush(stderr);                                       \
                fflush(stdout);                                       \
                if ( (exception) == ERR_GENERAL_ERROR) abort();       \
                else userraise(0, (exception), "%s", #exception);     \
            }                                                         \
        }                                                             \
        _INV_RES; })

/** @internal Two-value invariant engine (inv2 / inv2raise). */
#define _inv2raise(exception, do_raise, a, b, msg, ...)               \
    ({  bool _INV_OK = inv_any_bool(stderr, (a), (b));                \
        if (!_INV_OK) {                                               \
            inv_fprintf_int(stderr, #a " == " #b, _INV_OK,            \
                            msg, ##__VA_ARGS__);                      \
            if (do_raise) {                                           \
                fflush(stderr);                                       \
                fflush(stdout);                                       \
                abort();                                              \
            }                                                         \
        }                                                             \
        _INV_OK; })

#ifndef NOINVARIANT

/**
 * @brief Boolean check; on failure log and return false.
 *
 * @code
 *   if (!inv(ptr != NULL, "ptr is NULL in %s", __func__))
 *       return -1;
 * @endcode
 */
#define inv(expr, msg, ...)      _invraise(ERR_GENERAL_ERROR, false, (expr), (msg), ##__VA_ARGS__)

/**
 * @brief Boolean check; on failure log and raise (err_raise).
 */
#define invraise(expr, msg, ...) _invraise(ERR_GENERAL_ERROR, true, (expr), (msg), ##__VA_ARGS__)

/**
 * @brief Two-value check; on mismatch log and return false.
 *
 * @code
 *   if (!inv2(count, expected, "count mismatch"))
 *       return -1;
 * @endcode
 */
#define inv2(a, b, msg, ...)      _inv2raise(ERR_GENERAL_ERROR, false, (a), (b), (msg), ##__VA_ARGS__)

/**
 * @brief Two-value check; on mismatch log and raise.
 */
#define inv2raise(a, b, msg, ...) _inv2raise(ERR_GENERAL_ERROR, true, (a), (b), (msg), ##__VA_ARGS__)


#else /* NOINVARIANT */

#define inv(expr, msg, ...)       (expr)
#define invraise(expr, msg, ...)  (expr)
#define inv2(a, b, msg, ...)      ((a) == (b))
#define inv2raise(a, b, msg, ...) ((a) == (b))

#endif /* NOINVARIANT */

// ── Argument-count validators ────────────────────────────────────────

/**
 * @brief Check that `argc >= min_argc`.
 *
 * Expects `argc` to be in scope (typically inside main()).
 * Returns 1 if enough arguments were provided, 0 otherwise (and prints
 * the message to stderr).
 */
#define check_arg(min_argc, msg, ...)                                 \
    ( (argc) < (min_argc)                                             \
        ? (fprintf(stderr, msg, ##__VA_ARGS__), 0)                    \
        : 1 )

/**
 * @brief Boolean check; on failure log and raise the given exception
 *        via userraise() (not abort).
 *
 * Use for *expected* failure conditions (user error, bad state) as
 * opposed to invraise which aborts on invariant violation.
 * See error.h for details about exception.
 */
#define checkraise(exception, expr, msg, ...)      _invraise(exception, true, (expr), (msg), ##__VA_ARGS__)

#endif /* !CHECKER_H */
