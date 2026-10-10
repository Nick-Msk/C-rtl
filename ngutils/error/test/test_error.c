#include <criterion/criterion.h>

#include "error.h"
#include "log.h"

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* -------------------------------------------------------------------------
 * Version
 * ------------------------------------------------------------------------- */

Test(error, version_string_matches_macro) {
    cr_assert_str_eq(err_version(), ERROR_VERSION);
    cr_assert_str_eq(err_version(), "0.1.0");
}

Test(error, version_is_top_of_list) {
    const char *const *v = err_versions();
    cr_assert_not_null(v);
    cr_assert_not_null(v[0]);
    cr_assert_str_eq(v[0], err_version());
    cr_assert_str_eq(v[0], ERROR_VERSION);
}

Test(error, versions_terminated_by_null) {
    const char *const *v = err_versions();
    size_t n = 0;
    while (v[n]) ++n;
    cr_assert_geq(n, 1);
    cr_assert_null(v[n]);
}

/* -------------------------------------------------------------------------
 * Fixtures
 * ------------------------------------------------------------------------- */

static char log_path[256];

static void setup(void) {
    snprintf(log_path, sizeof(log_path),
             "/tmp/test_error_%d.log", (int)getpid());
    log_init(log_path, false, LOG_FORMAT_EMPTY);
    err_clean(true);
    err_resetenv();
}

static void teardown(void) {
    err_clean(true);
    err_resetenv();
    log_close();
    remove(log_path);
}

TestSuite(error_try, .init = setup, .fini = teardown);

/* -------------------------------------------------------------------------
 * TRY: normal path, catch on raise, loop stability
 * ------------------------------------------------------------------------- */

Test(error_try, normal_path) {
    volatile int ran = 0;
    TRY() {
        ran = 1;
    } else {
        cr_assert_fail("should not reach catch");
    }
    cr_assert_eq(ran, 1);
    cr_assert_eq(errenv.depth, 0);
    cr_assert_eq(errenv.overallcnt, 0);
}

Test(error_try, catch_on_raise) {
    volatile int caught = 0;
    TRY() {
        err_raise(ERR_USER, ERR_NULLABLE_PTR, "boom");
        cr_assert_fail("unreachable after err_raise");
    } else {
        caught = 1;
    }
    cr_assert_eq(caught, 1);
    cr_assert_eq(errenv.depth, 0);
    cr_assert_eq(errenv.overallcnt, 0);
}

Test(error_try, loop_1000_no_raise) {
    for (int i = 0; i < 1000; i++) {
        TRY() {
            /* nothing */
        } else {
            cr_assert_fail("should not catch at i=%d", i);
        }
    }
    cr_assert_eq(errenv.depth, 0);
    cr_assert_eq(errenv.overallcnt, 0);
}

Test(error_try, loop_1000_half_raise) {
    volatile int catches = 0;
    for (int i = 0; i < 1000; i++) {
        TRY() {
            if (i % 2 == 0)
                err_raise(ERR_USER, ERR_NULLABLE_PTR, "even %d", i);
        } else {
            catches++;
        }
    }
    cr_assert_eq(catches, 500);
    cr_assert_eq(errenv.depth, 0);
    cr_assert_eq(errenv.overallcnt, 0);
}

/* -------------------------------------------------------------------------
 * TRY: return / break from body
 *
 * These leak (for-increment is skipped). Design accepts the leak; the
 * next TRY() must still work correctly.
 * ------------------------------------------------------------------------- */

static int helper_return(int trigger) {
    TRY() {
        if (trigger) return 42;
    } else {
        return -1;
    }
    return 0;
}

Test(error_try, return_from_body) {
    err_resetenv();
    cr_assert_eq(helper_return(0), 0);
    err_resetenv();
    cr_assert_eq(helper_return(1), 42);   /* leaks depth=1 */
    err_resetenv();

    volatile int caught = 0;
    TRY() {
        err_raise(ERR_USER, ERR_NULLABLE_PTR, "after leak");
    } else {
        caught = 1;
    }
    cr_assert_eq(caught, 1);
    cr_assert_eq(errenv.depth, 0);
    cr_assert_eq(errenv.overallcnt, 0);
}

static int helper_break(void) {
    volatile int rv = 0;
    TRY() {
        rv = 1;
        break;
    } else {
        rv = -1;
    }
    return rv;
}

Test(error_try, break_from_body) {
    err_resetenv();
    cr_assert_eq(helper_break(), 1);
    err_resetenv();

    volatile int caught = 0;
    TRY() {
        err_raise(ERR_USER, ERR_NULLABLE_PTR, "after break leak");
    } else {
        caught = 1;
    }
    cr_assert_eq(caught, 1);
    cr_assert_eq(errenv.depth, 0);
    cr_assert_eq(errenv.overallcnt, 0);
}

/* -------------------------------------------------------------------------
 * Nested TRY
 * ------------------------------------------------------------------------- */

Test(error_try, nested_inner_catches) {
    volatile int outer_body  = 0;
    volatile int inner_catch = 0;

    TRY() {
        TRY() {
            err_raise(ERR_USER, ERR_NULLABLE_PTR, "inner");
        } else {
            inner_catch = 1;
        }
        outer_body = 1;
    } else {
        cr_assert_fail("outer should not catch");
    }

    cr_assert_eq(inner_catch, 1);
    cr_assert_eq(outer_body,  1);
    cr_assert_eq(errenv.depth, 0);
    cr_assert_eq(errenv.overallcnt, 0);
}

Test(error_try, nested_outer_catches) {
    volatile int inner_ran   = 0;
    volatile int outer_catch = 0;

    TRY() {
        TRY() {
            inner_ran = 1;
        } else {
            cr_assert_fail("inner should not catch");
        }
        err_raise(ERR_USER, ERR_NULLABLE_PTR, "outer");
    } else {
        outer_catch = 1;
    }

    cr_assert_eq(inner_ran,   1);
    cr_assert_eq(outer_catch, 1);
    cr_assert_eq(errenv.depth, 0);
    cr_assert_eq(errenv.overallcnt, 0);
}

Test(error_try, nested_three_levels) {
    volatile int hits = 0;

    TRY() { TRY() { TRY() {
        err_raise(ERR_USER, ERR_NULLABLE_PTR, "deep");
    } else { hits++; } } else { cr_assert_fail("mid catch"); }
    } else { cr_assert_fail("outer catch"); }

    cr_assert_eq(hits, 1);
    cr_assert_eq(errenv.depth, 0);
    cr_assert_eq(errenv.overallcnt, 0);
}

/* -------------------------------------------------------------------------
 * Raise outside any TRY: must fall back to a real SIGINT
 * ------------------------------------------------------------------------- */

static volatile sig_atomic_t g_sigint_seen = 0;
static void sigint_flag(int sig) { (void)sig; g_sigint_seen = 1; }

Test(error_try, raise_outside_try_signals_sigint) {
    err_resetenv();
    g_sigint_seen = 0;

    struct sigaction sa, old;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = sigint_flag;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, &old);

    err_raise(ERR_USER, ERR_NULLABLE_PTR, "no try here");

    sigaction(SIGINT, &old, NULL);
    cr_assert_eq(g_sigint_seen, 1,
                 "err_raise outside TRY must call raise(SIGINT)");
}

/* -------------------------------------------------------------------------
 * Ring overflow: leaking ERR_MAX_TRY_CNT frames wraps cleanly
 * ------------------------------------------------------------------------- */

Test(error_try, leak_full_ring_then_recover) {
    err_resetenv();

    /* Leak exactly one full ring via break (for-increment skipped) */
    for (int i = 0; i < ERR_MAX_TRY_CNT; i++) {
        TRY() { break; } else { cr_assert_fail("unexpected catch at %d", i); }
    }
    cr_assert_eq(errenv.depth,      0);
    cr_assert_eq(errenv.overallcnt, 1);

    /* err_resetenv() returns to clean state */
    err_resetenv();
    cr_assert_eq(errenv.depth,      0);
    cr_assert_eq(errenv.overallcnt, 0);

    /* Regular raise works after recovery */
    volatile int caught = 0;
    TRY() {
        err_raise(ERR_USER, ERR_NULLABLE_PTR, "post-wrap");
    } else { caught = 1; }
    cr_assert_eq(caught, 1);
    cr_assert_eq(errenv.depth,      0);
    cr_assert_eq(errenv.overallcnt, 0);
}

Test(error_try, leak_many_wraps) {
    err_resetenv();
    const int n = ERR_MAX_TRY_CNT * 10;

    for (int i = 0; i < n; i++) {
        TRY() { break; } else { cr_assert_fail("at %d", i); }
    }
    /* exact multiple of ring size → depth=0, overallcnt=10 */
    cr_assert_eq(errenv.depth,      0);
    cr_assert_eq(errenv.overallcnt, 10);

    err_resetenv();
}

Test(error_try, leak_ring_plus_half) {
    err_resetenv();
    const int n = ERR_MAX_TRY_CNT + ERR_MAX_TRY_CNT / 2;   /* 1.5 rings */

    for (int i = 0; i < n; i++) {
        TRY() { break; } else { cr_assert_fail("at %d", i); }
    }
    cr_assert_eq(errenv.depth,      ERR_MAX_TRY_CNT / 2);
    cr_assert_eq(errenv.overallcnt, 1);

    err_resetenv();
}

/* -------------------------------------------------------------------------
 * Real SIGINT on userraise outside any TRY
 *
 * Criterion forks per test; `.signal = SIGINT` means the test passes
 * iff the process is terminated by SIGINT. err_raise's fallback path
 * calls raise(SIGINT) with the default handler still installed.
 * ------------------------------------------------------------------------- */

Test(error_try, err_raise_no_try_signals_sigint, .signal = SIGINT) {
    err_resetenv();
    err_raise(ERR_USER, ERR_NULLABLE_PTR, "no try");
}

Test(error_try, userraise_no_try_signals_sigint, .signal = SIGINT) {
    err_resetenv();
    userraise(0, ERR_NULLABLE_PTR, "no try via userraise");
}
