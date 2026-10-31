#include <criterion/criterion.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include "checker.h"
#include "log.h"
#include "test_common.h" 

/* -------------------------------------------------------------------------
 * Fixtures — silence stderr, init log
 * ------------------------------------------------------------------------- */

TestSuite(checker, .init = test_setup, .fini = test_teardown);

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
 * checkraise() — raise via userraise (SIGINT) or abort (ERR_GENERAL_ERROR)
 * ------------------------------------------------------------------------- */

Test(checker, checkraise_true_no_signal) {
    /* expr is true → returns true, no raise */
    cr_assert(checkraise(ERR_NULLABLE_PTR, 1 == 1, "should hold"));
}

Test(checker, checkraise_true_various_codes) {
    cr_assert(checkraise(ERR_GENERAL_ERROR,  1, "ok"));
    cr_assert(checkraise(ERR_OUT_OF_RANGE,   1, "ok"));
    cr_assert(checkraise(ERR_NULL_INPUT,     1, "ok"));
}

Test(checker, checkraise_general_error_aborts, .signal = SIGABRT) {
    /* ERR_GENERAL_ERROR → abort() path, identical to invraise */
    checkraise(ERR_GENERAL_ERROR, 1 == 2, "must abort");
}

Test(checker, checkraise_null_ptr_sigint, .signal = SIGINT) {
    /* non-GENERAL_ERROR + no TRY → userraise → SIGINT */
    const char *p = NULL;
    checkraise(ERR_NULL_INPUT, p != NULL, "p is NULL in test");
}

Test(checker, checkraise_out_of_range_sigint, .signal = SIGINT) {
    checkraise(ERR_OUT_OF_RANGE, 0, "value out of range");
}

Test(checker, checkraise_printf_args_sigint, .signal = SIGINT) {
    int x = 0;
    checkraise(ERR_OUT_OF_RANGE, x > 0, "x=%d must be positive", x);
}

Test(checker, checkraise_catched_by_try) {
    /* Active TRY() → err_raise longjmps back, no SIGINT */
    int caught = 0;
    TRY() {
        checkraise(ERR_NULLABLE_PTR, 0, "should be caught");
        cr_assert_not(1, "expected longjmp, not normal return");
    } else {
        caught = 1;
    }
    cr_assert(caught, "TRY else-branch was not reached");
}

Test(checker, checkraise_try_true_no_longjmp) {
    /* true expr inside TRY → completes normally, no longjmp */
    int caught = 0;
    TRY() {
        cr_assert(checkraise(ERR_NULLABLE_PTR, 1, "ok"));
    } else {
        caught = 1;
    }
    cr_assert_not(caught, "unexpected longjmp for true expr");
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
    test_tmpname(path, sizeof path, "test_checker_fprintf", NULL, "txt");

    FILE *f = fopen(path, "w");
    cr_assert_not_null(f);

    int rc = inv_fprintf_int(f, "ptr != NULL", false, "ptr was null");
    fclose(f);

    cr_assert_gt(rc, 0, "expected positive write count");

    char buf[1024];
    size_t n = test_read_file(path, buf, sizeof buf);
    remove(path);

    cr_assert_gt(n, 0);
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
