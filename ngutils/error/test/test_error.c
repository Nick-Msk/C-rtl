#include <criterion/criterion.h>

#include "error.h"
#include "log.h"

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

/* -------------------------------------------------------------------------
 * Fixtures
 * ------------------------------------------------------------------------- */

static char log_path[256];
static int  g_saved_stderr = -1;

static void setup(void) {
    snprintf(log_path, sizeof(log_path),
             "/tmp/test_error_%d.log", (int)getpid());
    log_init(log_path, false, LOG_FORMAT_EMPTY);
    err_clean(true);
    err_resetenv();

    /* Silence stderr: the raise macros print diagnostics there by design.
     * Without this, every err_raise in a test floods Criterion output. */
    fflush(stderr);
    g_saved_stderr = dup(fileno(stderr));
    freopen("/dev/null", "w", stderr);
}

static void teardown(void) {
    err_clean(true);
    err_resetenv();
    log_close();
    remove(log_path);

    fflush(stderr);
    if (g_saved_stderr >= 0) {
        dup2(g_saved_stderr, fileno(stderr));
        close(g_saved_stderr);
        g_saved_stderr = -1;
    }
}

TestSuite(error_try,   .init = setup, .fini = teardown);
TestSuite(error_trace, .init = setup, .fini = teardown);
TestSuite(error_sys,   .init = setup, .fini = teardown);
TestSuite(error,       .init = setup, .fini = teardown);   /* version tests */

/* -------------------------------------------------------------------------
 * Version
 * ------------------------------------------------------------------------- */

Test(error, version_string_matches_macro) {
    cr_assert_str_eq(err_version(), ERROR_VERSION);
    cr_assert_str_eq(err_version(), "0.2.0");
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
 * Ring overflow: leaking ERR_CYCLE_CNT frames wraps cleanly
 * ------------------------------------------------------------------------- */

Test(error_try, leak_full_ring_then_recover) {
    err_resetenv();

    /* Leak exactly one full ring via break (for-increment skipped) */
    for (int i = 0; i < ERR_CYCLE_CNT; i++) {
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
    const int n = ERR_CYCLE_CNT * 10;

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
    const int n = ERR_CYCLE_CNT + ERR_CYCLE_CNT / 2;   /* 1.5 rings */

    for (int i = 0; i < n; i++) {
        TRY() { break; } else { cr_assert_fail("at %d", i); }
    }
    cr_assert_eq(errenv.depth,      ERR_CYCLE_CNT / 2);
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

/* -------------------------------------------------------------------------
 * err_fprintstacktrace
 * ------------------------------------------------------------------------- */

static char trace_path[256];

static int capture_trace(char *buf, size_t cap) {
    snprintf(trace_path, sizeof(trace_path),
             "/tmp/test_error_trace_%d.txt", (int)getpid());
    FILE *f = fopen(trace_path, "w");
    cr_assert_not_null(f);
    int rc = err_fprintstacktrace(f);
    fclose(f);

    FILE *r = fopen(trace_path, "r");
    cr_assert_not_null(r);
    size_t n = fread(buf, 1, cap - 1, r);
    buf[n] = '\0';
    fclose(r);
    remove(trace_path);
    return rc;
}

Test(error_trace, empty_stack_no_entries) {
    err_clean(true);

    char buf[1024] = {0};
    int rc = capture_trace(buf, sizeof buf);

    cr_assert_gt(rc, 0);
    cr_assert(strstr(buf, "PRINT STACK TRACE START") != NULL, "buf: %s", buf);
    cr_assert(strstr(buf, "PRINT STACK TRACE END")   != NULL, "buf: %s", buf);
    /* no numbered entries like "[   0]: ..." */
    cr_assert(strstr(buf, "[   0]:") == NULL, "buf: %s", buf);
}

Test(error_trace, one_user_raise) {
    err_clean(true);

    volatile int caught = 0;
    TRY() {
        err_raise(ERR_USER, ERR_NULLABLE_PTR, "pointer is NULL");
    } else {
        caught = 1;
    }
    cr_assert_eq(caught, 1, "should have caught");

    char buf[1024] = {0};
    capture_trace(buf, sizeof buf);

    cr_assert(strstr(buf, "ERR_USER")        != NULL, "buf: %s", buf);
    cr_assert(strstr(buf, "[10]")            != NULL, "buf: %s", buf);
    cr_assert(strstr(buf, "pointer is NULL") != NULL, "buf: %s", buf);
}

Test(error_trace, sys_raise_has_errno) {
    err_clean(true);
    errno = ENOENT;

    TRY() {
        err_raise(ERR_SYS, 0, "opening file");
    } else {}

    char buf[2048] = {0};
    capture_trace(buf, sizeof buf);

    cr_assert(strstr(buf, "ERR_SYS")      != NULL, "buf: %s", buf);
    cr_assert(strstr(buf, "opening file") != NULL, "buf: %s", buf);
}

Test(error_trace, multiple_entries_in_order) {
    err_clean(true);

    TRY() { err_raise(ERR_USER, ERR_NULLABLE_PTR, "first");  } else {}
    TRY() { err_raise(ERR_USER, ERR_OUT_OF_RANGE, "second"); } else {}
    TRY() { err_raise(ERR_USER, ERR_NULL_INPUT,   "third");  } else {}

    char buf[2048] = {0};
    capture_trace(buf, sizeof buf);

    const char *p1 = strstr(buf, "first");
    const char *p2 = strstr(buf, "second");
    const char *p3 = strstr(buf, "third");
    cr_assert_not_null(p1, "buf: %s", buf);
    cr_assert_not_null(p2, "buf: %s", buf);
    cr_assert_not_null(p3, "buf: %s", buf);
    cr_assert(p1 < p2 && p2 < p3, "entries out of order: %s", buf);

    cr_assert(strstr(buf, "[   0]:") != NULL, "buf: %s", buf);
    cr_assert(strstr(buf, "[   1]:") != NULL, "buf: %s", buf);
    cr_assert(strstr(buf, "[   2]:") != NULL, "buf: %s", buf);
}

Test(error_trace, clean_clears_entries) {
    err_clean(true);
    TRY() { err_raise(ERR_USER, ERR_NULLABLE_PTR, "will be cleared"); } else {}
    err_clean(true);

    char buf[1024] = {0};
    capture_trace(buf, sizeof buf);

    cr_assert(strstr(buf, "will be cleared") == NULL, "buf: %s", buf);
    cr_assert(strstr(buf, "[   0]:")         == NULL, "buf: %s", buf);
}

Test(error_trace, return_value_grows_with_entries) {
    err_clean(true);
    char buf[4096] = {0};

    int rc0 = capture_trace(buf, sizeof buf);

    TRY() { err_raise(ERR_USER, ERR_NULLABLE_PTR, "one"); } else {}
    int rc1 = capture_trace(buf, sizeof buf);

    TRY() { err_raise(ERR_USER, ERR_NULLABLE_PTR, "two"); } else {}
    int rc2 = capture_trace(buf, sizeof buf);

    cr_assert_gt(rc0, 0);
    cr_assert_gt(rc1, rc0, "rc1=%d rc0=%d", rc1, rc0);
    cr_assert_gt(rc2, rc1, "rc2=%d rc1=%d", rc2, rc1);
}

Test(error_trace, message_buffer_grows_beyond_initial) {
    err_clean(true);
    for (int i = 0; i < 200; i++) {
        TRY() { err_raise(ERR_USER, ERR_NULLABLE_PTR, "entry %d", i); } else {}
    }

    char buf[64 * 1024] = {0};
    int rc = capture_trace(buf, sizeof buf);

    cr_assert_gt(rc, 0);
    cr_assert(strstr(buf, "entry 0")   != NULL, "first entry missing");
    cr_assert(strstr(buf, "entry 199") != NULL, "last entry missing");
}

/* -------------------------------------------------------------------------
 * sys raise
 * ------------------------------------------------------------------------- */

Test(error_sys, sysraise_records_errno) {
    err_clean(true);
    errno = ENOENT;

    TRY() {
        sysraise(0, "cannot open %s", "/tmp/nope");
    } else {}

    char buf[2048] = {0};
    capture_trace(buf, sizeof buf);

    cr_assert(strstr(buf, "ERR_SYS")                != NULL, "buf: %s", buf);
    cr_assert(strstr(buf, "[2]")                    != NULL, "buf: %s", buf);  /* ENOENT == 2 */
    cr_assert(strstr(buf, "No such file or directory") != NULL, "buf: %s", buf);
    cr_assert(strstr(buf, "cannot open /tmp/nope")  != NULL, "buf: %s", buf);
}

Test(error_sys, sysraiseact_runs_cleanup) {
    err_clean(true);
    errno = EACCES;

    volatile int cleaned = 0;

    TRY() {
        sysraiseact(0, cleaned = 1, "permission denied");
    } else {}

    cr_assert_eq(cleaned, 1, "ACTION must run before raise");
}

Test(error_sys, sysraise_retcode_when_outside_try, .signal = SIGINT) {
    err_resetenv();
    errno = ENOENT;
    sysraise(0, "no try");
    /* killed by SIGINT — .signal flag makes this a pass */
}

Test(error_sys, sysraise_preserves_errno) {
    err_clean(true);
    errno = ENOENT;

    TRY() {
        sysraise(0, "cannot open");
    } else {}

    char buf[2048] = {0};
    capture_trace(buf, sizeof buf);

    cr_assert(strstr(buf, "[2]") != NULL,
              "errno must be ENOENT(2), buf: %s", buf);
    cr_assert(strstr(buf, "No such file or directory") != NULL,
              "strerror missing, buf: %s", buf);
}

static void clobber_errno(void) { errno = EINVAL; }

Test(error_sys, sysraiseact_action_does_not_clobber_errno) {
    err_clean(true);
    errno = ENOENT;

    TRY() {
        sysraiseact(0, clobber_errno(), "cannot open");
    } else {}

    char buf[2048] = {0};
    capture_trace(buf, sizeof buf);

    cr_assert(strstr(buf, "[2]") != NULL,
              "ACTION clobbered errno; expected ENOENT(2), buf: %s", buf);
}

/* -------------------------------------------------------------------------
 * err_count / err_last / err_pop
 * ------------------------------------------------------------------------- */

Test(error_stack, count_empty) {
    err_clean(true);
    cr_assert_eq(err_count(), 0);
}

Test(error_stack, count_after_raises) {
    err_clean(true);
    TRY() { err_raise(ERR_USER, ERR_NULLABLE_PTR, "a"); } else {}
    TRY() { err_raise(ERR_USER, ERR_OUT_OF_RANGE, "b"); } else {}
    TRY() { err_raise(ERR_USER, ERR_NULL_INPUT,   "c"); } else {}
    cr_assert_eq(err_count(), 3);
}

Test(error_stack, last_empty) {
    err_clean(true);
    ErrorInfo info = {0};
    cr_assert_not(err_last(&info));
}

Test(error_stack, last_picks_top) {
    err_clean(true);
    TRY() { err_raise(ERR_USER, ERR_NULLABLE_PTR, "first");  } else {}
    TRY() { err_raise(ERR_USER, ERR_OUT_OF_RANGE, "second"); } else {}

    ErrorInfo info = {0};
    cr_assert(err_last(&info));
    cr_assert_eq(info.type, ERR_USER);
    cr_assert_eq(info.code, ERR_OUT_OF_RANGE);
}

Test(error_stack, last_null_out_ok) {
    err_clean(true);
    TRY() { err_raise(ERR_USER, ERR_NULLABLE_PTR, "x"); } else {}
    cr_assert(err_last(NULL));
}

Test(error_stack, last_sys_reports_errno) {
    err_clean(true);
    errno = ENOENT;

    TRY() { err_raise(ERR_SYS, 0, "cannot open"); } else {}

    ErrorInfo info = {0};
    cr_assert(err_last(&info));
    cr_assert_eq(info.type, ERR_SYS);
    cr_assert_eq(info.code, ENOENT);
}

Test(error_stack, pop_empty) {
    err_clean(true);
    cr_assert_not(err_pop());
}

Test(error_stack, pop_removes_top) {
    err_clean(true);
    TRY() { err_raise(ERR_USER, ERR_NULLABLE_PTR, "a"); } else {}
    TRY() { err_raise(ERR_USER, ERR_OUT_OF_RANGE, "b"); } else {}
    cr_assert_eq(err_count(), 2);

    cr_assert(err_pop());
    cr_assert_eq(err_count(), 1);

    ErrorInfo info;
    cr_assert(err_last(&info));
    cr_assert_eq(info.code, ERR_NULLABLE_PTR);
}

Test(error_stack, pop_to_zero) {
    err_clean(true);
    TRY() { err_raise(ERR_USER, ERR_NULLABLE_PTR, "a"); } else {}
    cr_assert(err_pop());
    cr_assert_eq(err_count(), 0);
    cr_assert_not(err_pop());
}

/* -------------------------------------------------------------------------
 * Per-thread stacks
 * ------------------------------------------------------------------------- */

typedef struct {
    int id;
    int count_before;
    int count_after;
    int last_code;
} ThreadResult;

static void *
thread_worker(void *arg)
{
    ThreadResult *r = arg;

    r->count_before = err_count();
    r->count_after  = -1;
    r->last_code    = -1;

    for (int i = 0; i < 5; i++) {
        TRY() {
            err_raise(ERR_USER, ERR_NULLABLE_PTR + i,
                      "thread %d entry %d", r->id, i);
        } else {
            /* caught, continue */
        }
    }

    r->count_after = err_count();

    ErrorInfo info;
    if (err_last(&info))
        r->last_code = info.code;

    err_clean(true);
    return NULL;
}

Test(error_threads, per_thread_stack) {
    enum { N = 4 };

    pthread_t    threads[N];
    ThreadResult results[N];

    memset(results, 0, sizeof results);
    for (int i = 0; i < N; i++) {
        results[i].id = i;
        cr_assert_eq(pthread_create(&threads[i], NULL, thread_worker, &results[i]),
                     0, "pthread_create(%d) failed", i);
    }
    for (int i = 0; i < N; i++)
        pthread_join(threads[i], NULL);

    for (int i = 0; i < N; i++) {
        cr_assert_eq(results[i].count_before, 0,
                     "thread %d started with %d entries", i, results[i].count_before);
        cr_assert_eq(results[i].count_after, 5,
                     "thread %d ended with %d entries, expected 5",
                     i, results[i].count_after);
        cr_assert_eq(results[i].last_code, ERR_NULLABLE_PTR + 4,
                     "thread %d saw last code %d, expected %d",
                     i, results[i].last_code, ERR_NULLABLE_PTR + 4);
    }

    /* main thread's stack must be untouched by workers */
    cr_assert_eq(err_count(), 0);
}

static void
deep_worker(int depth)
{
    if (depth == 0) {
        err_raise(ERR_USER, ERR_OUT_OF_RANGE, "bottom reached");
        return;   /* unreachable in practice (err_raise never returns
                   * here — it either siglongjmps or raises SIGINT),
                   * but tells the compiler the path terminates. */
    }
    deep_worker(depth - 1);
}

static int
cross_function_try(int depth)
{
    volatile int caught = 0;
    TRY() {
        deep_worker(depth);
    } else {
        caught = 1;
    }
    return caught;
}

Test(error_try, cross_function_siglongjmp) {
    cr_assert_eq(cross_function_try(10), 1);
    cr_assert_eq(errenv.depth, 0);
    cr_assert_eq(errenv.overallcnt, 0);
}

Test(error_trace, long_message_truncates_safely) {
    err_clean(true);

    char big[4096];
    memset(big, 'x', sizeof big - 1);
    big[sizeof big - 1] = '\0';

    TRY() {
        err_raise(ERR_USER, ERR_NULLABLE_PTR, "%s", big);
    } else {}

    cr_assert_eq(err_count(), 1);

    char buf[8192] = {0};
    capture_trace(buf, sizeof buf);

    cr_assert(strstr(buf, "xxxx") != NULL, "buf: %s", buf);
    
    size_t trace_len = strlen(buf);
    cr_assert_lt(trace_len, sizeof buf - 1);
}

Test(error_try, try_without_else) {
    volatile int reached = 0;
    TRY() {
        err_raise(ERR_USER, ERR_NULLABLE_PTR, "boom");
        reached = 1;   /* не должно выполниться */
    }
    cr_assert_eq(reached, 0);
    cr_assert_eq(errenv.depth, 0);
}

