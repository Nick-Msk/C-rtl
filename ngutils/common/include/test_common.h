/**
 * @file test_common.h
 * @brief Shared test helpers: sanitizer detection, stderr muting,
 *        file capture utilities.
 *
 * Not a production header — include only from test files.
 */

#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <stddef.h>

#include "log.h"

/* ─────────────────────────────────────────────────────────────────────────
 * Sanitizer detection
 *
 * GCC defines __SANITIZE_ADDRESS__ and __SANITIZE_UNDEFINED__.
 * Clang uses __has_feature; the __has_feature macro itself is only
 * defined for Clang, so we guard its use.
 * ───────────────────────────────────────────────────────────────────────── */

#if defined(__SANITIZE_ADDRESS__)
#  define TEST_ASAN 1
#elif defined(__has_feature)
#  if __has_feature(address_sanitizer)
#    define TEST_ASAN 1
#  endif
#endif

#ifndef TEST_ASAN
#  define TEST_ASAN 0
#endif

#if defined(__SANITIZE_UNDEFINED__)
#  define TEST_UBSAN 1
#elif defined(__has_feature)
#  if __has_feature(undefined_behavior_sanitizer)
#    define TEST_UBSAN 1
#  endif
#endif

#ifndef TEST_UBSAN
#  define TEST_UBSAN 0
#endif

/**
 * @brief Guard for tests that must not run under any sanitizer.
 *
 * Typical case: `.signal = SIGINT` tests. ASan and UBSan install their
 * own signal handlers and abort on signals before the test framework
 * can see them, which turns a passing test into a "Failing" report.
 *
 * Usage:
 * @code
 *   #if TEST_NOT_SANITIZED
 *   Test(foo, dies_on_sigint, .signal = SIGINT) { ... }
 *   #endif
 * @endcode
 */
#if TEST_ASAN || TEST_UBSAN
#  define TEST_NOT_SANITIZED 0
#else
#  define TEST_NOT_SANITIZED 1
#endif

/* ─────────────────────────────────────────────────────────────────────────
 * stderr muting
 *
 * Many tests exercise error paths where the library prints to stderr on
 * purpose (invariant violations, log diagnostics). Criterion captures
 * stdout but not stderr, so those messages flood the terminal and hide
 * the actual test results.
 *
 * Typical use in a fixture:
 *
 *     static int saved_stderr = -1;
 *     static void setup(void) {
 *         saved_stderr = test_mute_stderr();
 *     }
 *     static void teardown(void) {
 *         test_restore_stderr(saved_stderr);
 *     }
 *
 * Both functions are no-ops when the process is not attached to a
 * terminal (CI), and the muting is skipped.
 * ───────────────────────────────────────────────────────────────────────── */

/**
 * @brief Redirect stderr to /dev/null.
 * @return Saved descriptor to pass to test_restore_stderr, or -1.
 */
static inline int
test_mute_stderr(void)
{
    fflush(stderr);
    int saved = dup(fileno(stderr));
    if (saved < 0)
        return -1;
    if (!freopen("/dev/null", "w", stderr)) {
        close(saved);
        return -1;
    }
    return saved;
}

/**
 * @brief Restore stderr to the descriptor returned by test_mute_stderr.
 * @param saved  Descriptor from test_mute_stderr, or -1 (no-op).
 */
static inline void
test_restore_stderr(int saved)
{
    if (saved < 0)
        return;
    fflush(stderr);
    dup2(saved, fileno(stderr));
    close(saved);
}

/* ─────────────────────────────────────────────────────────────────────────
 * File capture
 *
 * Slurp a file into a caller-provided buffer. Useful for tests that
 * verify output written to a temporary file by the library.
 * ───────────────────────────────────────────────────────────────────────── */

/**
 * @brief Read up to @p cap - 1 bytes of @p path into @p buf .
 *
 * Always NUL-terminates. Returns the number of bytes read, or 0 on
 * error (file missing, permission denied).
 *
 * @param path  File path.
 * @param buf   Destination buffer.
 * @param cap   Size of @p buf in bytes (must be > 0).
 * @return      Number of bytes read (excluding NUL), 0 on failure.
 */
static inline size_t
test_read_file(const char *path, char *buf, size_t cap)
{
    if (!path || !buf || cap == 0)
        return 0;
    buf[0] = '\0';

    FILE *f = fopen(path, "rb");
    if (!f)
        return 0;

    size_t n = fread(buf, 1, cap - 1, f);
    fclose(f);
    buf[n] = '\0';
    return n;
}

/**
 * @brief Byte-wise substring search.
 *
 * Like strstr, but works on buffers that may contain NUL bytes (log
 * output does — the `%*c` preamble padding writes zeros).
 *
 * @param buf     Buffer to search.
 * @param buflen  Length of @p buf in bytes.
 * @param needle  NUL-terminated substring to look for.
 * @return        true if @p needle occurs in @p buf .
 */
static inline bool
test_buf_contains(const char *buf, size_t buflen, const char *needle)
{
    if (!buf || !needle)
        return false;
    size_t nlen = strlen(needle);
    if (nlen == 0 || nlen > buflen)
        return false;
    for (size_t i = 0; i + nlen <= buflen; i++) {
        if (memcmp(buf + i, needle, nlen) == 0)
            return true;
    }
    return false;
}

/* ─────────────────────────────────────────────────────────────────────────
 * Temp file naming
 *
 * Per-pid / per-suffix temp name under /tmp. Avoids collision between
 * parallel test processes (Criterion forks) and between test files.
 * ───────────────────────────────────────────────────────────────────────── */

/**
 * @brief Build a unique temp path into @p out .
 *
 * Format: /tmp/<prefix>_<pid>[_<suffix>].<ext>
 *
 * @param out     Destination buffer.
 * @param cap     Size of @p out .
 * @param prefix  Test-specific prefix (e.g. "test_fs2").
 * @param suffix  Optional suffix (may be NULL).
 * @param ext     Extension without dot (e.g. "log", "txt").
 * @return        Number of chars written, excluding NUL.
 */
static inline int
test_tmpname(char *out, size_t cap,
             const char *prefix, const char *suffix, const char *ext)
{
    if (!out || cap == 0)
        return 0;
    if (suffix && *suffix) {
        return snprintf(out, cap, "/tmp/%s_%d_%s.%s",
                        prefix, (int)getpid(), suffix, ext);
    }
    return snprintf(out, cap, "/tmp/%s_%d.%s",
                    prefix, (int)getpid(), ext);
}

/* -------------------------------------------------------------------------
 * CHECK_* — assertions with diagnostic logging
 *
 * On failure, log test name, metric, expected and actual to the log
 * file (which survives the run), then delegate to Criterion.
 *
 * CHECK_FMT picks the printf specifier by the type of the expression.
 * Small integer types (char, short, bool) are promoted to int by
 * variadic argument passing; printf's %hhd / %hd / %d consume the low
 * bytes correctly, so this stays safe.
 * ------------------------------------------------------------------------- */

/* _Generic only picks the format string. It is used as the format
 * argument to snprintf at runtime, not concatenated into a literal. */

#define CHECK_FMT(x) _Generic((x),    \
    bool:               "%d",   \
    char:               "%c",   \
    signed char:        "%hhd", \
    unsigned char:      "%hhu", \
    short:              "%hd",  \
    unsigned short:     "%hu",  \
    int:                "%d",   \
    unsigned int:       "%u",   \
    long:               "%ld",  \
    unsigned long:      "%lu",  \
    long long:          "%lld", \
    unsigned long long: "%llu", \
    float:              "%f",   \
    double:             "%f",   \
    long double:        "%Lf"   \
)

/* Internal: format both numbers into stack buffers, then log. */
static inline void
_check_log_num(const char *func, const char *metric,
               const char *expected, const char *actual)
{
    logsimple("FAIL %s: %s expected=%s got=%s",
              func, metric, expected, actual);
}

#define CHECK_EQ(metric, actual, expected)                                \
    do {                                                                  \
        typeof((actual) + (expected)) _a = (actual);                                   \
        typeof((actual) + (expected)) _e = (expected);                                 \
        if (_a != _e) {                                                   \
            char _eb[64], _ab[64];                                        \
            snprintf(_eb, sizeof _eb, CHECK_FMT(_e), _e);                 \
            snprintf(_ab, sizeof _ab, CHECK_FMT(_a), _a);                 \
            _check_log_num(__func__, (metric), _eb, _ab);                 \
        }                                                                 \
        cr_assert_eq(_a, _e);                                             \
    } while (0)

#define CHECK_NEQ(metric, actual, expected)                               \
    do {                                                                  \
        typeof((actual) + (expected)) _a = (actual);                                   \
        typeof((actual) + (expected)) _e = (expected);                                 \
        if (_a == _e) {                                                   \
            char _eb[64], _ab[64];                                        \
            snprintf(_eb, sizeof _eb, CHECK_FMT(_e), _e);                 \
            snprintf(_ab, sizeof _ab, CHECK_FMT(_a), _a);                 \
            _check_log_num(__func__, (metric), _eb, _ab);                 \
        }                                                                 \
        cr_assert_neq(_a, _e);                                            \
    } while (0)

#define CHECK_STR_EQ(metric, actual, expected)                   \
    do {                                                                    \
        const char *_a = (actual);                                          \
        const char *_e = (expected);                                        \
        if (!_a || !_e || strcmp(_a, _e) != 0) {                            \
            logsimple("FAIL %s: %s expected=[%s] got=[%s]",                 \
                      __func__, (metric),                                   \
                      _e ? _e : "(null)", _a ? _a : "(null)");              \
        }                                                                   \
        cr_assert_str_eq(_a, _e);                                           \
    } while (0)

#define CHECK_STR_NEQ(metric, actual, expected)                   \
    do {                                                                    \
        const char *_a = (actual);                                          \
        const char *_e = (expected);                                        \
        if (!_a || !_e || strcmp(_a, _e) == 0) {                            \
            logsimple("FAIL %s: %s expected=[%s] got=[%s]",                 \
                      __func__, (metric),                                   \
                      _e ? _e : "(null)", _a ? _a : "(null)");              \
        }                                                                   \
        cr_assert_str_neq(_a, _e);                                          \
    } while (0)

#define CHECK_PTR_EQ(metric, actual, expected)                   \
    do {                                                                    \
        const void *_a = (const void *)(actual);                            \
        const void *_e = (const void *)(expected);                          \
        if (_a != _e) {                                                     \
            logsimple("FAIL %s: %s expected=%p got=%p",                     \
                      __func__, (metric), _e, _a);                          \
        }                                                                   \
        cr_assert_eq(_a, _e);                                               \
    } while (0)

#define CHECK_PTR_NEQ(metric, a, b)                              \
    do {                                                                    \
        const void *_a = (const void *)(a);                                 \
        const void *_b = (const void *)(b);                                 \
        if (_a == _b) {                                                     \
            logsimple("FAIL %s: %s both = %p (expected distinct)",          \
                      __func__, (metric), _a);                              \
        }                                                                   \
        cr_assert_neq(_a, _b);                                              \
    } while (0)

#define CHECK_TRUE(metric, cond)                                 \
    do {                                                                    \
        if (!(cond)) {                                                      \
            logsimple("FAIL %s: %s is false",                               \
                __func__, (metric));                                        \
             cr_assert_fail("%s is false", (metric));                       \
        }                                                                   \
    } while (0)

#define CHECK_FALSE(metric, cond)                                \
    do {                                                                    \
        if (cond) {                                                         \
            logsimple("FAIL %s: %s is true (expected false)",               \
                      __func__, (metric));                                  \
            cr_assert_fail("%s is true (expected false)", (metric));        \
        }                                                                   \
    } while (0)    

#define CHECK_NULL(metric, actual)                             \
    do {                                                                  \
        const void *_a = (const void *)(actual);                          \
        if (_a != NULL) {                                                 \
            logsimple("FAIL %s: %s expected NULL got %p",                 \
                      __func__, (metric), _a);                            \
        }                                                                 \
        cr_assert_null(_a);                                               \
    } while (0)

#define CHECK_NOT_NULL(metric, actual)                         \
    do {                                                                  \
        const void *_a = (const void *)(actual);                          \
        if (_a == NULL) {                                                 \
            logsimple("FAIL %s: %s is NULL",                              \
                __func__, (metric));                                      \
        }                                                                 \
        cr_assert_not_null(_a);                                           \
    } while (0)

#define CHECK_MEM_EQ(metric, a, b, n)                          \
    do {                                                                  \
        const void *_a = (a);                                             \
        const void *_b = (b);                                             \
        size_t      _n = (n);                                             \
        if (memcmp(_a, _b, _n) != 0) {                                    \
            logsimple("FAIL %s: %s differs in %zu bytes",                 \
                      __func__, (metric), _n);                            \
        }                                                                 \
        cr_assert_arr_eq(_a, _b, (int)_n);                                \
    } while (0)

/* ── Ordering ───────────────────────────────────────────────────────────
 *
 * Same _Generic + snprintf approach as CHECK_EQ / CHECK_NEQ.
 * Useful when the exact value is implementation-defined (e.g. sz
 * after power-of-two growth) but the relation must hold.
 * ──────────────────────────────────────────────────────────────────── */

#define CHECK_GT(metric, actual, expected)                                \
    do {                                                                  \
        typeof((actual) + (expected)) _a = (actual);                                   \
        typeof((actual) + (expected)) _e = (expected);                                 \
        if (!(_a > _e)) {                                                 \
            char _eb[64], _ab[64];                                        \
            snprintf(_eb, sizeof _eb, CHECK_FMT(_e), _e);                 \
            snprintf(_ab, sizeof _ab, CHECK_FMT(_a), _a);                 \
            logsimple("FAIL %s: %s expected > %s got %s",                 \
                      __func__, (metric), _eb, _ab);                      \
        }                                                                 \
        cr_assert_gt(_a, _e);                                             \
    } while (0)

#define CHECK_GE(metric, actual, expected)                                \
    do {                                                                  \
        typeof((actual) + (expected)) _a = (actual);                                   \
        typeof((actual) + (expected)) _e = (expected);                                 \
        if (!(_a >= _e)) {                                                \
            char _eb[64], _ab[64];                                        \
            snprintf(_eb, sizeof _eb, CHECK_FMT(_e), _e);                 \
            snprintf(_ab, sizeof _ab, CHECK_FMT(_a), _a);                 \
            logsimple("FAIL %s: %s expected >= %s got %s",                \
                      __func__, (metric), _eb, _ab);                      \
        }                                                                 \
        cr_assert_geq(_a, _e);                                            \
    } while (0)

#define CHECK_LT(metric, actual, expected)                                \
    do {                                                                  \
        typeof((actual) + (expected)) _a = (actual);                                   \
        typeof((actual) + (expected)) _e = (expected);                                 \
        if (!(_a < _e)) {                                                 \
            char _eb[64], _ab[64];                                        \
            snprintf(_eb, sizeof _eb, CHECK_FMT(_e), _e);                 \
            snprintf(_ab, sizeof _ab, CHECK_FMT(_a), _a);                 \
            logsimple("FAIL %s: %s expected < %s got %s",                 \
                      __func__, (metric), _eb, _ab);                      \
        }                                                                 \
        cr_assert_lt(_a, _e);                                             \
    } while (0)

#define CHECK_LE(metric, actual, expected)                                \
    do {                                                                  \
        typeof((actual) + (expected)) _a = (actual);                                   \
        typeof((actual) + (expected)) _e = (expected);                                 \
        if (!(_a <= _e)) {                                                \
            char _eb[64], _ab[64];                                        \
            snprintf(_eb, sizeof _eb, CHECK_FMT(_e), _e);                 \
            snprintf(_ab, sizeof _ab, CHECK_FMT(_a), _a);                 \
            logsimple("FAIL %s: %s expected <= %s got %s",                \
                      __func__, (metric), _eb, _ab);                      \
        }                                                                 \
        cr_assert_leq(_a, _e);                                            \
    } while (0)

/* -------------------------------------------------------------------------
 * Common test fixture
 *
 * Most tests need: a per-project log file, stderr muted (the libraries
 * under test print diagnostics there on purpose), and a clean shutdown.
 * Test files just register:
 *
 *     TestSuite(myproj, .init = test_setup, .fini = test_teardown);
 *
 * The log path is logs/test_<PROJNAME>.log, append=true. Criterion
 * forks per test, but each child has its own copy of the inode
 * registry; append + unbuffered writes make lines atomic.
 * ------------------------------------------------------------------------- */

#ifndef TEST_PROJNAME
#  error "TEST_PROJNAME must be defined; see Makefile.common"
#endif

static int test_saved_stderr = -1;

static inline void
test_setup(void)
{
    const struct criterion_test *t = criterion_current_test;
    const char *name = (t && t->name) ? t->name : "unknown";

    log_mkdir("logs");
    log_initf(false, LOG_FORMAT_ALL, "logs",
        "test_%s_%s_%d.log", TEST_PROJNAME, name, (int) getpid());    
    logsimple("\n\n========== BEGIN %s ==========\n", name);

    test_saved_stderr = test_mute_stderr();
}

static inline void
test_teardown(void)
{
    const struct criterion_test *t = criterion_current_test;
    const char *name = (t && t->name) ? t->name : "unknown";

    logsimple("\n\n========== END %s ==========\n", name);
    log_close();
    test_restore_stderr(test_saved_stderr);
    test_saved_stderr = -1;
    /* logs/test_<PROJNAME>.log stays; clean with 'make clean-logs' or
     * the automatic clean-logs-$(PROJNAME) that runs before 'test'. */
}

#endif /* !TEST_COMMON_H */
