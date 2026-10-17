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
#define CHECK_FS2VALID(n, s)                                               \
    do {                                                                  \
        if (!fs2_validate(logfile, (s))) {                               \
            logsimple("FAIL %s: fsvalid %d failed on %s",                \
                      __func__, (n), #s);                                 \
            cr_assert_fail("fsvalid %d failed on %s", (n), #s);          \
        }                                                                 \
    } while (0)

#endif /* !TEST_FS2_H */
