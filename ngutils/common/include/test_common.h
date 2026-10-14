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

#endif /* !TEST_COMMON_H */
