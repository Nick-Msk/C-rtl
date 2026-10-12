#include <criterion/criterion.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include "checker.h"
#include "log.h"

/* -------------------------------------------------------------------------
 * Fixtures — silence stderr, init log
 * ------------------------------------------------------------------------- */

static char log_path[256];
static int  saved_stderr = -1;

static void setup(void) {
    snprintf(log_path, sizeof(log_path),
             "/tmp/test_checker_%d.log", (int)getpid());
    log_init(log_path, false, LOG_FORMAT_EMPTY);

    fflush(stderr);
    saved_stderr = dup(fileno(stderr));
    freopen("/dev/null", "w", stderr);
}

static void teardown(void) {
    log_close();
    remove(log_path);

    fflush(stderr);
    if (saved_stderr >= 0) {
        dup2(saved_stderr, fileno(stderr));
        close(saved_stderr);
        saved_stderr = -1;
    }
}

TestSuite(checker, .init = setup, .fini = teardown);

/* -------------------------------------------------------------------------
 * inv() — returning boolean invariant
 * ------------------------------------------------------------------------- */

Test(checker, inv_true) {
    cr_assert(inv(1 == 1, "must hold"));
}

Test(checker, inv_false) {
    cr_assert_not(inv(1 == 2, "must not hold"));
}

Test(checker, inv_with_expression) {
    int x = 5;
    cr_assert(inv(x == 5, "x = %d", x));
    cr_assert_not(inv(x > 10, "x = %d must be > 10", x));
}

Test(checker, inv_with_pointer) {
    int y = 0;
    int *p = &y;
    cr_assert(inv(p != NULL, "p is null"));
}

/* -------------------------------------------------------------------------
 * invraise() — abort on failure
 * ------------------------------------------------------------------------- */

Test(checker, invraise_true_ok) {
    /* must not abort */
    cr_assert(invraise(1 == 1, "should hold"));
}

Test(checker, invraise_false_aborts, .signal = SIGABRT) {
    invraise(1 == 2, "must abort here");
}

Test(checker, invraise_with_pointer_ok) {
    int y = 0;
    int *p = &y;
    cr_assert(invraise(p != NULL, "p is null"));
}

Test(checker, invraise_with_pointer_aborts, .signal = SIGABRT) {
    int *p = NULL;
    invraise(p != NULL, "p must not be null");
}

/* -------------------------------------------------------------------------
 * inv2() — returning two-value comparison
 * ------------------------------------------------------------------------- */

Test(checker, inv2_int_match) {
    cr_assert(inv2(42, 42, "must match"));
}

Test(checker, inv2_int_mismatch) {
    cr_assert_not(inv2(41, 42, "got %d", 41));
}

Test(checker, inv2_negative_match) {
    cr_assert(inv2(-1, -1, "negative match"));
}

Test(checker, inv2_zero_match) {
    cr_assert(inv2(0, 0, "zero match"));
}

Test(checker, inv2_zero_mismatch) {
    cr_assert_not(inv2(5, 0, "expected 0, got 5"));
}

Test(checker, inv2_long_match) {
    long a = 1234567890L;
    long b = 1234567890L;
    cr_assert(inv2(a, b, "long match"));
}

Test(checker, inv2_double_match) {
    cr_assert(inv2(3.14, 3.14, "double match"));
}

Test(checker, inv2_double_mismatch) {
    cr_assert_not(inv2(3.14, 3.15, "double mismatch"));
}

Test(checker, inv2_str_match) {
    cr_assert(inv2("abc", "abc", "string match"));
}

Test(checker, inv2_str_mismatch) {
    cr_assert_not(inv2("abc", "abd", "string mismatch"));
}

Test(checker, inv2_str_both_null) {
    const char *a = NULL;
    const char *b = NULL;
    cr_assert(inv2(a, b, "both null"));
}

Test(checker, inv2_str_one_null) {
    const char *a = NULL;
    const char *b = "x";
    cr_assert_not(inv2(a, b, "one null"));
}

Test(checker, inv2_ptr_match) {
    int x = 0;
    cr_assert(inv2(&x, &x, "same pointer"));
}

Test(checker, inv2_ptr_mismatch) {
    int x = 0, y = 0;
    cr_assert_not(inv2(&x, &y, "different pointers"));
}

/* -------------------------------------------------------------------------
 * inv2raise() — abort on mismatch
 * ------------------------------------------------------------------------- */

Test(checker, inv2raise_match_ok) {
    cr_assert(inv2raise(7, 7, "must match"));
}

Test(checker, inv2raise_mismatch_aborts, .signal = SIGABRT) {
    inv2raise(7, 8, "must abort");
}

Test(checker, inv2raise_str_match_ok) {
    cr_assert(inv2raise("k", "k", "must match"));
}

Test(checker, inv2raise_str_mismatch_aborts, .signal = SIGABRT) {
    inv2raise("k", "x", "must abort");
}

/* -------------------------------------------------------------------------
 * check_arg() — requires `argc` in scope; helper passes it explicitly
 * ------------------------------------------------------------------------- */

static int
fake_main(int argc, int min) {
    if (!check_arg(min, "need at least %d args\n", min))
        return -1;
    return 0;
}

Test(checker, check_arg_enough) {
    cr_assert_eq(fake_main(3, 2), 0);
}

Test(checker, check_arg_exact) {
    cr_assert_eq(fake_main(2, 2), 0);
}

Test(checker, check_arg_too_few) {
    cr_assert_eq(fake_main(1, 2), -1);
}

Test(checker, check_arg_zero_required) {
    cr_assert_eq(fake_main(0, 0), 0);
}

/* -------------------------------------------------------------------------
 * inv_fprintf_int — direct call
 * ------------------------------------------------------------------------- */

Test(checker, inv_fprintf_int_noop_on_success) {
    /* res == true → nothing written, return 0 */
    int rc = inv_fprintf_int(NULL, "some_expr", true, "msg");
    cr_assert_eq(rc, 0);
}

Test(checker, inv_fprintf_int_null_out_no_crash) {
    /* failure with NULL out: log-only path */
    int rc = inv_fprintf_int(NULL, "expr", false, "diagnostic");
    cr_assert_eq(rc, 0);
}

Test(checker, inv_fprintf_int_writes_to_stream) {
    char path[256];
    snprintf(path, sizeof(path),
             "/tmp/test_checker_fprintf_%d.txt", (int)getpid());

    FILE *f = fopen(path, "w");
    cr_assert_not_null(f);

    int rc = inv_fprintf_int(f, "ptr != NULL", false, "ptr was null");
    fclose(f);

    cr_assert_gt(rc, 0, "expected positive write count");

    FILE *r = fopen(path, "r");
    cr_assert_not_null(r);
    char buf[1024] = {0};
    size_t n = fread(buf, 1, sizeof(buf) - 1, r);
    fclose(r);
    remove(path);

    cr_assert_gt(n, 0);
    cr_assert(strstr(buf, "ptr != NULL") != NULL, "buf: %s", buf);
    cr_assert(strstr(buf, "ptr was null") != NULL, "buf: %s", buf);
}

/* -------------------------------------------------------------------------
 * Version
 * ------------------------------------------------------------------------- */

Test(checker, version_string_matches_macro) {
    cr_assert_str_eq(checker_version(), CHECKER_VERSION);
    cr_assert_str_eq(checker_version(), "0.1.0");
}

Test(checker, version_is_top_of_list) {
    const char *const *v = checker_versions();
    cr_assert_not_null(v);
    cr_assert_not_null(v[0]);
    cr_assert_str_eq(v[0], checker_version());
    cr_assert_str_eq(v[0], CHECKER_VERSION);
}

Test(checker, versions_terminated_by_null) {
    const char *const *v = checker_versions();
    size_t n = 0;
    while (v[n]) ++n;
    cr_assert_geq(n, 1);
    cr_assert_null(v[n]);
}
