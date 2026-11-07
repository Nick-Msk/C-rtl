/**
 * @file test_fs2.h
 * @brief fs2-specific test helpers.
 *
 * Not installed, test-only. Include after test_common.h (which pulls
 * log.h → logfile macro).
 */

#ifndef TEST_FS2_H
#define TEST_FS2_H

#include "fs2.h"
#include "fs2_internal_utils.h"
#include "log.h"
#include "test_common.h"

/**
 * @brief Validate @p s and fail the test with a numbered marker.
 *
 * On failure, logs "FAIL <func>: fsvalid N failed on <expr>" and
 * delegates to cr_assert_fail. fs2_validate itself prints the specific
 * reason to the same log file.
 *
 * @param n   Step number (1, 2, 3 …) inside the test.
 * @param s   fs2 value or pointer to validate.
 */
#define CHECK_FS2VALID(n, s)                                              \
    do {                                                                  \
        if (!fs2_validate(logfile, (s))) {                                \
            logsimple("FAIL %s: fsvalid %zu failed on %s",                \
                      __func__, (size_t)(n), #s);                         \
            cr_assert_fail("fsvalid %zu failed on %s", (size_t)(n), #s);  \
        }                                                                 \
    } while (0)

static inline void
assert_balanced(void)
{
    int a = -1, f = -1, ba = -1, bf = -1;
    bool ok = fs2_checkleak(&a, &f, &ba, &bf);
    if (!ok)
        logsimple("FAIL %s: alloc=%d freed=%d body_alloc=%d body_freed=%d",
                  __func__, a, f, ba, bf);
    cr_assert(ok, "counters not balanced");
}    

static inline int 
errno_take(void) __attribute__((always_inline));

/* errno_take: read errno (satisfies unix.Errno), reset to 0. */
static inline int
errno_take(void) {
    int e = errno;
    errno = 0;
    return e;
}

/* Drop-in replacement for tmpfile() with NULL check that analyzer understands. */
static inline FILE *
test_tmpfile(const char *where) {
    FILE *f = tmpfile();
    if (!f) {
        cr_assert_fail("%s: tmpfile() failed", where);
        /* unreachable; the `return` tells analyzer the path exits */
        return NULL;
    }
    return f;
}

/* Drop-in replacement for fread() that drains errno (unix.Errno). */
static inline size_t
test_fread(void *buf, size_t n, FILE *f, const char *where) {
    errno_take();                              /* drain stale errno before fread */
    size_t rd = fread(buf, 1, n, f);
    int    e  = errno_take();                  /* drain fread's errno */
    if (rd < n && ferror(f))
        cr_assert_fail("%s: fread %zu/%zu: errno=%d (%s)",
                       where, rd, n, e, strerror(e));
    return rd;
}

#endif /* !TEST_FS2_H */
