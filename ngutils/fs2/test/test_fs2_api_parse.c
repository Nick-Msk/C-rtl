 #include <criterion/criterion.h>

#include "test_fs2.h"
#include "fs2_api.h"
#include <locale.h>
#include <limits.h>

TestSuite(fs2_api_parse, .init = test_setup, .fini = test_teardown);

/* =========================================================================
 * fs2_pos_parse_llong — nominal
 * ========================================================================= */

Test(fs2_api, pos_parse_llong_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42");
    size_t np = 999; long long v = 0;
    cr_assert(fs2_pos_parse_llong(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  42);
    CHECK_EQ("newpos", np, 2);
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_llong_negative) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("-42");
    size_t np = 999; long long v = 0;
    cr_assert(fs2_pos_parse_llong(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  -42);
    CHECK_EQ("newpos", np, 3);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_llong_plus_sign) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("+42");
    size_t np = 999; long long v = 0;
    cr_assert(fs2_pos_parse_llong(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  42);
    CHECK_EQ("newpos", np, 3);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_llong_zero) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("0");
    size_t np = 999; long long v = 123;
    cr_assert(fs2_pos_parse_llong(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  0);
    CHECK_EQ("newpos", np, 1);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_llong_leading_zeros) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("007");
    size_t np = 999; long long v = 0;
    cr_assert(fs2_pos_parse_llong(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  7);
    CHECK_EQ("newpos", np, 3);   /* all digits */
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_llong — spaces / trailing garbage
 * ========================================================================= */

Test(fs2_api, pos_parse_llong_leading_spaces) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("   42");
    size_t np = 999; long long v = 0;
    cr_assert(fs2_pos_parse_llong(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  42);
    CHECK_EQ("newpos", np, 5);   /* 3 spaces + 2 digits */
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_llong_trailing_garbage) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42abc");
    size_t np = 999; long long v = 0;
    cr_assert(fs2_pos_parse_llong(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  42);
    CHECK_EQ("newpos", np, 2);   /* only "42" */
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_llong_trailing_spaces) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42   ");
    size_t np = 999; long long v = 0;
    cr_assert(fs2_pos_parse_llong(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  42);
    CHECK_EQ("newpos", np, 2);   /* trailing spaces are not counted */
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_llong_leading_spaces_and_trailing_garbage) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("  -42xyz");
    size_t np = 999; long long v = 0;
    cr_assert(fs2_pos_parse_llong(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  -42);
    CHECK_EQ("newpos", np, 5);   /* 2 spaces + '-' + 2 digits */
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_llong — pos > 0
 * ========================================================================= */

Test(fs2_api, pos_parse_llong_pos_mid_string) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc123def");
    size_t np = 999; long long v = 0;
    cr_assert(fs2_pos_parse_llong(&s, 3, &np, &v));
    CHECK_EQ("value",  v,  123);
    CHECK_EQ("newpos", np, 6);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_llong_pos_after_space) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("a 42");
    size_t np = 999; long long v = 0;
    cr_assert(fs2_pos_parse_llong(&s, 1, &np, &v));
    CHECK_EQ("value",  v,  42);
    CHECK_EQ("newpos", np, 4);   /* 1 space + 2 digits from pos=1 */
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_llong_pos_last_digit) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("x5");
    size_t np = 999; long long v = 0;
    cr_assert(fs2_pos_parse_llong(&s, 1, &np, &v));
    CHECK_EQ("value",  v,  5);
    CHECK_EQ("newpos", np, 2);
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_llong — boundary values LLONG
 * ========================================================================= */

Test(fs2_api, pos_parse_llong_llong_max) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("9223372036854775807");
    size_t np = 999; long long v = 0;
    cr_assert(fs2_pos_parse_llong(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  LLONG_MAX);
    CHECK_EQ("newpos", np, 19);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_llong_llong_min) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("-9223372036854775808");
    size_t np = 999; long long v = 0;
    cr_assert(fs2_pos_parse_llong(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  LLONG_MIN);
    CHECK_EQ("newpos", np, 20);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_llong_overflow_returns_false) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("9223372036854775808");   /* LLONG_MAX + 1 */
    size_t np = 12345; long long v = 999;
    cr_assert_not(fs2_pos_parse_llong(&s, 0, &np, &v));
    CHECK_EQ("out untouched",    v,  999);
    CHECK_EQ("newpos untouched", np, 12345);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_llong_underflow_returns_false) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("-9223372036854775809");  /* LLONG_MIN - 1 */
    size_t np = 12345; long long v = 999;
    cr_assert_not(fs2_pos_parse_llong(&s, 0, &np, &v));
    CHECK_EQ("out untouched",    v,  999);
    CHECK_EQ("newpos untouched", np, 12345);
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_llong — optional out / newpos
 * ========================================================================= */

Test(fs2_api, pos_parse_llong_out_null_ok) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42");
    size_t np = 999;
    cr_assert(fs2_pos_parse_llong(&s, 0, &np, NULL));
    CHECK_EQ("newpos", np, 2);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_llong_newpos_null_ok) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42");
    long long v = 0;
    cr_assert(fs2_pos_parse_llong(&s, 0, NULL, &v));
    CHECK_EQ("value", v, 42);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_llong_both_null_ok) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("-42");
    cr_assert(fs2_pos_parse_llong(&s, 0, NULL, NULL));
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_llong — failures (false, no crash, no write)
 * ========================================================================= */

Test(fs2_api, pos_parse_llong_empty_fs2empty) {
    __fs2_checkreset();
    fs2 s = FS2EMPTY;   /* len=0 */
    size_t np = 12345; long long v = 999;
    cr_assert_not(fs2_pos_parse_llong(&s, 0, &np, &v));   /* pos >= len */
    CHECK_EQ("out untouched",    v,  999);
    CHECK_EQ("newpos untouched", np, 12345);
    assert_balanced();
}

Test(fs2_api, pos_parse_llong_empty_canon_v_null) {
    __fs2_checkreset();
    fs2 s = FS2();   /* v=NULL */
    size_t np = 12345; long long v = 999;
    cr_assert_not(fs2_pos_parse_llong(&s, 0, &np, &v));   /* !s->v */
    CHECK_EQ("out untouched",    v,  999);
    CHECK_EQ("newpos untouched", np, 12345);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_llong_null_s_returns_false) {
    size_t np = 12345; long long v = 999;
    cr_assert_not(fs2_pos_parse_llong(NULL, 0, &np, &v));   /* soft false, no abort */
    CHECK_EQ("out untouched",    v,  999);
    CHECK_EQ("newpos untouched", np, 12345);
}

Test(fs2_api, pos_parse_llong_pos_eq_len) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42");
    size_t np = 12345; long long v = 999;
    cr_assert_not(fs2_pos_parse_llong(&s, 2, &np, &v));   /* pos == len */
    CHECK_EQ("out untouched",    v,  999);
    CHECK_EQ("newpos untouched", np, 12345);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_llong_pos_gt_len) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42");
    cr_assert_not(fs2_pos_parse_llong(&s, 100, NULL, NULL));
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_llong_only_spaces) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("   ");
    size_t np = 12345; long long v = 999;
    cr_assert_not(fs2_pos_parse_llong(&s, 0, &np, &v));   /* Only spaces */
    CHECK_EQ("out untouched",    v,  999);
    CHECK_EQ("newpos untouched", np, 12345);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_llong_letters_rejected) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");
    size_t np = 12345; long long v = 999;
    cr_assert_not(fs2_pos_parse_llong(&s, 0, &np, &v));   /* Incorrect sym */
    CHECK_EQ("out untouched",    v,  999);
    CHECK_EQ("newpos untouched", np, 12345);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_llong_only_plus_rejected) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("+");
    cr_assert_not(fs2_pos_parse_llong(&s, 0, NULL, NULL));
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_llong_only_minus_rejected) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("-");
    cr_assert_not(fs2_pos_parse_llong(&s, 0, NULL, NULL));
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_llong_plus_then_garbage_rejected) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("+abc");
    cr_assert_not(fs2_pos_parse_llong(&s, 0, NULL, NULL));
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_llong_minus_then_garbage_rejected) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("-abc");
    cr_assert_not(fs2_pos_parse_llong(&s, 0, NULL, NULL));
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_llong_double_sign_rejected) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("+-5");
    cr_assert_not(fs2_pos_parse_llong(&s, 0, NULL, NULL));

    fs2 t = fs2_fromstr("--5");
    cr_assert_not(fs2_pos_parse_llong(&t, 0, NULL, NULL));

    fs2_free(&s);
    fs2_free(&t);
    assert_balanced();
}

Test(fs2_api, pos_parse_llong_sign_then_spaces_rejected) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("+  ");
    cr_assert_not(fs2_pos_parse_llong(&s, 0, NULL, NULL));
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_llong — special formats
 * ========================================================================= */

/* base 10: "0x10" → parses "0", endptr at 'x' */
Test(fs2_api, pos_parse_llong_hex_prefix_parsed_as_zero) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("0x10");
    size_t np = 999; long long v = 123;
    cr_assert(fs2_pos_parse_llong(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  0);     /* base 10 stopped at '0' */
    CHECK_EQ("newpos", np, 1);     /* only '0' consumed */
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_llong_negative_zero) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("-0");
    size_t np = 999; long long v = 123;
    cr_assert(fs2_pos_parse_llong(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  0);
    CHECK_EQ("newpos", np, 2);
    fs2_free(&s);
    assert_balanced();
}

/* High byte (>= 0x80) — must not cause UB in isdigit */
Test(fs2_api, pos_parse_llong_high_byte_rejected_no_ub) {
    __fs2_checkreset();
    char *raw = strdup("\xE9" "42");   /* first byte 0xE9 */
    cr_assert(raw != NULL);
    fs2 s = fs2_adopt(&raw);
    cr_assert(raw == NULL);

    /* 0xE9 is not a digit, not '+'/'-' → "Incorrect sym" */
    cr_assert_not(fs2_pos_parse_llong(&s, 0, NULL, NULL));
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_llong — STATIC / LOCAL
 * ========================================================================= */

Test(fs2_api, pos_parse_llong_static_src) {
    __fs2_checkreset();
    fs2 s = FS2LITERAL("  -42");
    size_t np = 999; long long v = 0;
    cr_assert(fs2_pos_parse_llong(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  -42);
    CHECK_EQ("newpos", np, 5);
    CHECK_FS2VALID(1, &s);
    assert_balanced();
}

Test(fs2_api, pos_parse_llong_local_src) {
    __fs2_checkreset();
    FS2LOCAL(buf, 16);
    fs2_memcpy_str(&buf, 0, "123", 3);
    fs2_setlen(&buf, 3);

    size_t np = 999; long long v = 0;
    cr_assert(fs2_pos_parse_llong(&buf, 0, &np, &v));
    CHECK_EQ("value",  v,  123);
    CHECK_EQ("newpos", np, 3);
    CHECK_FS2VALID(1, &buf);
    assert_balanced();
}

/* =========================================================================
 * Invariant: newpos is ALWAYS > pos on success
 * ========================================================================= */

Test(fs2_api, pos_parse_llong_newpos_always_advances) {
    __fs2_checkreset();
    static const char *inputs[] = { "0", "+5", "-7", "  42", "+99abc", NULL };

    for (size_t i = 0; inputs[i]; i++) {
        fs2 s = fs2_fromstr(inputs[i]);
        size_t np = 0;
        cr_assert(fs2_pos_parse_llong(&s, 0, &np, NULL),
                  "failed to parse '%s'", inputs[i]);
        cr_assert(np > 0, "'%s' newpos=%zu not advanced", inputs[i], np);
        fs2_free(&s);
    }
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_long — nominal (delegation + writing out/newpos)
 * ========================================================================= */

Test(fs2_api, pos_parse_long_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42");
    size_t np = 999; long v = 0;
    cr_assert(fs2_pos_parse_long(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  42L);
    CHECK_EQ("newpos", np, 2);
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_long_negative_and_spaces) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("  -42xyz");
    size_t np = 999; long v = 0;
    cr_assert(fs2_pos_parse_long(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  -42L);
    CHECK_EQ("newpos", np, 5);   /* 2 spaces + '-' + 2 digits */
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_long_pos_mid_string) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc123");
    size_t np = 999; long v = 0;
    cr_assert(fs2_pos_parse_long(&s, 3, &np, &v));
    CHECK_EQ("value",  v,  123L);
    CHECK_EQ("newpos", np, 6);
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_long — optional out / newpos
 * ========================================================================= */

Test(fs2_api, pos_parse_long_out_null) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42");
    size_t np = 999;
    cr_assert(fs2_pos_parse_long(&s, 0, &np, NULL));
    CHECK_EQ("newpos", np, 2);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_long_newpos_null) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42");
    long v = 0;
    cr_assert(fs2_pos_parse_long(&s, 0, NULL, &v));
    CHECK_EQ("value", v, 42L);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_long_both_null) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("-42");
    cr_assert(fs2_pos_parse_long(&s, 0, NULL, NULL));
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_long — LONG boundaries (accepted)
 * ========================================================================= */

Test(fs2_api, pos_parse_long_long_max) {
    __fs2_checkreset();
    char buf[64];
    snprintf(buf, sizeof buf, "%ld", LONG_MAX);
    fs2 s = fs2_fromstr(buf);

    size_t np = 999; long v = 0;
    cr_assert(fs2_pos_parse_long(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  LONG_MAX);
    CHECK_EQ("newpos", np, strlen(buf));
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_long_long_min) {
    __fs2_checkreset();
    char buf[64];
    snprintf(buf, sizeof buf, "%ld", LONG_MIN);
    fs2 s = fs2_fromstr(buf);

    size_t np = 999; long v = 0;
    cr_assert(fs2_pos_parse_long(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  LONG_MIN);
    CHECK_EQ("newpos", np, strlen(buf));
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_long — range-violation (active only on LLP64 / ILP32)
 *   On LP64 (macOS/Linux) long == long long → dead branch, tests skipped.
 * ========================================================================= */

#if LONG_MAX < LLONG_MAX
Test(fs2_api, pos_parse_long_above_long_max_returns_false) {
    __fs2_checkreset();
    /* LLONG_MAX is guaranteed > LONG_MAX under this #if */
    fs2 s = fs2_fromstr("9223372036854775807");   /* LLONG_MAX */
    size_t np = 12345; long v = 999;
    cr_assert_not(fs2_pos_parse_long(&s, 0, &np, &v));
    CHECK_EQ("out untouched",    v,  999L);
    CHECK_EQ("newpos untouched", np, 12345);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_long_below_long_min_returns_false) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("-9223372036854775808");   /* LLONG_MIN */
    size_t np = 12345; long v = 999;
    cr_assert_not(fs2_pos_parse_long(&s, 0, &np, &v));
    CHECK_EQ("out untouched",    v,  999L);
    CHECK_EQ("newpos untouched", np, 12345);
    fs2_free(&s);
    assert_balanced();
}
#endif /* LONG_MAX < LLONG_MAX */

/* =========================================================================
 * fs2_pos_parse_long — failures: out/newpos are NOT touched
 *   (fail-path from _llong delegation)
 * ========================================================================= */

Test(fs2_api, pos_parse_long_invalid_format_false_no_write) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");
    size_t np = 12345; long v = 999;
    cr_assert_not(fs2_pos_parse_long(&s, 0, &np, &v));
    CHECK_EQ("out untouched",    v,  999L);
    CHECK_EQ("newpos untouched", np, 12345);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_long_overflow_false_no_write) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("99999999999999999999999999");
    size_t np = 12345; long v = 999;
    cr_assert_not(fs2_pos_parse_long(&s, 0, &np, &v));   /* ERANGE in _llong */
    CHECK_EQ("out untouched",    v,  999L);
    CHECK_EQ("newpos untouched", np, 12345);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_long_pos_out_of_range_false) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42");
    size_t np = 12345; long v = 999;
    cr_assert_not(fs2_pos_parse_long(&s, 100, &np, &v));
    CHECK_EQ("out untouched",    v,  999L);
    CHECK_EQ("newpos untouched", np, 12345);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_long_empty_fs2empty_false) {
    __fs2_checkreset();
    fs2 s = FS2EMPTY;
    size_t np = 12345; long v = 999;
    cr_assert_not(fs2_pos_parse_long(&s, 0, &np, &v));
    CHECK_EQ("out untouched",    v,  999L);
    CHECK_EQ("newpos untouched", np, 12345);
    assert_balanced();
}

Test(fs2_api, pos_parse_long_canon_v_null_false) {
    __fs2_checkreset();
    fs2 s = FS2();
    size_t np = 12345; long v = 999;
    cr_assert_not(fs2_pos_parse_long(&s, 0, &np, &v));
    CHECK_EQ("out untouched",    v,  999L);
    CHECK_EQ("newpos untouched", np, 12345);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_long_null_s_false) {
    size_t np = 12345; long v = 999;
    cr_assert_not(fs2_pos_parse_long(NULL, 0, &np, &v));   /* soft false */
    CHECK_EQ("out untouched",    v,  999L);
    CHECK_EQ("newpos untouched", np, 12345);
}

/* =========================================================================
 * Delegation: long and llong give the same result on the same input
 * (LP64 path — range check passes everything llong already passed)
 * ========================================================================= */

Test(fs2_api, pos_parse_long_delegates_to_llong) {
    __fs2_checkreset();
    static const char *inputs[] = {
        "0", "42", "-42", "+7", "  123", "1x", "   -9abc", NULL
    };
    for (size_t i = 0; inputs[i]; i++) {
        fs2 s = fs2_fromstr(inputs[i]);
        size_t np_long = 0, np_ll = 0;
        long      v_long = 0;
        long long v_ll   = 0;

        bool ok_long = fs2_pos_parse_long (&s, 0, &np_long, &v_long);
        bool ok_ll   = fs2_pos_parse_llong(&s, 0, &np_ll,   &v_ll);

        cr_assert_eq(ok_long, ok_ll, "divergence on '%s'", inputs[i]);
        if (ok_long) {
            CHECK_EQ("value",  (long long)v_long, v_ll);
            CHECK_EQ("newpos", np_long, np_ll);
        }
        fs2_free(&s);
    }
    assert_balanced();
}

/* =========================================================================
 * STATIC / LOCAL — behave the same
 * ========================================================================= */

Test(fs2_api, pos_parse_long_static_src) {
    __fs2_checkreset();
    fs2 s = FS2LITERAL("  -42");
    size_t np = 999; long v = 0;
    cr_assert(fs2_pos_parse_long(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  -42L);
    CHECK_EQ("newpos", np, 5);
    CHECK_FS2VALID(1, &s);
    assert_balanced();
}

Test(fs2_api, pos_parse_long_local_src) {
    __fs2_checkreset();
    FS2LOCAL(buf, 16);
    fs2_memcpy_str(&buf, 0, "123", 3);
    fs2_setlen(&buf, 3);

    size_t np = 999; long v = 0;
    cr_assert(fs2_pos_parse_long(&buf, 0, &np, &v));
    CHECK_EQ("value",  v,  123L);
    CHECK_EQ("newpos", np, 3);
    CHECK_FS2VALID(1, &buf);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_int — nominal
 * ========================================================================= */

Test(fs2_api, pos_parse_int_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42");
    size_t np = 999; int v = 0;
    cr_assert(fs2_pos_parse_int(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  42);
    CHECK_EQ("newpos", np, 2);
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_int_negative_and_spaces) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("  -42xyz");
    size_t np = 999; int v = 0;
    cr_assert(fs2_pos_parse_int(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  -42);
    CHECK_EQ("newpos", np, 5);   /* 2 spaces + '-' + 2 digits */
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_int_pos_mid_string) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc123");
    size_t np = 999; int v = 0;
    cr_assert(fs2_pos_parse_int(&s, 3, &np, &v));
    CHECK_EQ("value",  v,  123);
    CHECK_EQ("newpos", np, 6);
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_int — optional out / newpos
 * ========================================================================= */

Test(fs2_api, pos_parse_int_out_null) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42");
    size_t np = 999;
    cr_assert(fs2_pos_parse_int(&s, 0, &np, NULL));
    CHECK_EQ("newpos", np, 2);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_int_newpos_null) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42");
    int v = 0;
    cr_assert(fs2_pos_parse_int(&s, 0, NULL, &v));
    CHECK_EQ("value", v, 42);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_int_both_null) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("-42");
    cr_assert(fs2_pos_parse_int(&s, 0, NULL, NULL));
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_int — INT boundaries (accepted)
 * ========================================================================= */

Test(fs2_api, pos_parse_int_int_max) {
    __fs2_checkreset();
    char buf[32];
    snprintf(buf, sizeof buf, "%d", INT_MAX);
    fs2 s = fs2_fromstr(buf);

    size_t np = 999; int v = 0;
    cr_assert(fs2_pos_parse_int(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  INT_MAX);
    CHECK_EQ("newpos", np, strlen(buf));
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_int_int_min) {
    __fs2_checkreset();
    char buf[32];
    snprintf(buf, sizeof buf, "%d", INT_MIN);
    fs2 s = fs2_fromstr(buf);

    size_t np = 999; int v = 0;
    cr_assert(fs2_pos_parse_int(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  INT_MIN);
    CHECK_EQ("newpos", np, strlen(buf));
    fs2_free(&s);
    assert_balanced();
}

/* INT_MAX + 1 — beyond int, but within long long */
Test(fs2_api, pos_parse_int_above_int_max_false_no_write) {
    __fs2_checkreset();
    char buf[32];
    snprintf(buf, sizeof buf, "%lld", (long long)INT_MAX + 1);
    fs2 s = fs2_fromstr(buf);

    size_t np = 12345; int v = 999;
    cr_assert_not(fs2_pos_parse_int(&s, 0, &np, &v));
    CHECK_EQ("out untouched",    v,  999);
    CHECK_EQ("newpos untouched", np, 12345);
    fs2_free(&s);
    assert_balanced();
}

/* INT_MIN - 1 */
Test(fs2_api, pos_parse_int_below_int_min_false_no_write) {
    __fs2_checkreset();
    char buf[32];
    snprintf(buf, sizeof buf, "%lld", (long long)INT_MIN - 1);
    fs2 s = fs2_fromstr(buf);

    size_t np = 12345; int v = 999;
    cr_assert_not(fs2_pos_parse_int(&s, 0, &np, &v));
    CHECK_EQ("out untouched",    v,  999);
    CHECK_EQ("newpos untouched", np, 12345);
    fs2_free(&s);
    assert_balanced();
}

/* LLONG_MAX (guaranteed outside int) — rejected via range-check, not via ERANGE */
Test(fs2_api, pos_parse_int_llong_max_rejected_by_range) {
    __fs2_checkreset();
    char buf[64];
    snprintf(buf, sizeof buf, "%lld", LLONG_MAX);
    fs2 s = fs2_fromstr(buf);

    size_t np = 12345; int v = 999;
    cr_assert_not(fs2_pos_parse_int(&s, 0, &np, &v));
    CHECK_EQ("out untouched",    v,  999);
    CHECK_EQ("newpos untouched", np, 12345);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_int_llong_min_rejected_by_range) {
    __fs2_checkreset();
    char buf[64];
    snprintf(buf, sizeof buf, "%lld", LLONG_MIN);
    fs2 s = fs2_fromstr(buf);

    size_t np = 12345; int v = 999;
    cr_assert_not(fs2_pos_parse_int(&s, 0, &np, &v));
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_int — failures: out/newpos are NOT touched
 * ========================================================================= */

Test(fs2_api, pos_parse_int_invalid_format_false_no_write) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");
    size_t np = 12345; int v = 999;
    cr_assert_not(fs2_pos_parse_int(&s, 0, &np, &v));
    CHECK_EQ("out untouched",    v,  999);
    CHECK_EQ("newpos untouched", np, 12345);
    fs2_free(&s);
    assert_balanced();
}

/* ERANGE in strtoll (not int range-check, but beyond llong) */
Test(fs2_api, pos_parse_int_llong_overflow_false_no_write) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("99999999999999999999999999");   /* > LLONG_MAX */
    size_t np = 12345; int v = 999;
    cr_assert_not(fs2_pos_parse_int(&s, 0, &np, &v));
    CHECK_EQ("out untouched",    v,  999);
    CHECK_EQ("newpos untouched", np, 12345);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_int_pos_out_of_range_false) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42");
    size_t np = 12345; int v = 999;
    cr_assert_not(fs2_pos_parse_int(&s, 100, &np, &v));
    CHECK_EQ("out untouched",    v,  999);
    CHECK_EQ("newpos untouched", np, 12345);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_int_empty_fs2empty_false) {
    __fs2_checkreset();
    fs2 s = FS2EMPTY;
    size_t np = 12345; int v = 999;
    cr_assert_not(fs2_pos_parse_int(&s, 0, &np, &v));
    CHECK_EQ("out untouched",    v,  999);
    CHECK_EQ("newpos untouched", np, 12345);
    assert_balanced();
}

Test(fs2_api, pos_parse_int_canon_v_null_false) {
    __fs2_checkreset();
    fs2 s = FS2();
    size_t np = 12345; int v = 999;
    cr_assert_not(fs2_pos_parse_int(&s, 0, &np, &v));
    CHECK_EQ("out untouched",    v,  999);
    CHECK_EQ("newpos untouched", np, 12345);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_int_null_s_false) {
    size_t np = 12345; int v = 999;
    cr_assert_not(fs2_pos_parse_int(NULL, 0, &np, &v));
    CHECK_EQ("out untouched",    v,  999);
    CHECK_EQ("newpos untouched", np, 12345);
}

/* =========================================================================
 * Delegation + _llong equivalence for small values
 * ========================================================================= */

Test(fs2_api, pos_parse_int_delegates_to_llong) {
    __fs2_checkreset();
    static const char *inputs[] = {
        "0", "42", "-42", "+7", "  123", "1x", "   -9abc", NULL
    };
    for (size_t i = 0; inputs[i]; i++) {
        fs2 s = fs2_fromstr(inputs[i]);
        size_t np_int = 0, np_ll = 0;
        int       v_int = 0;
        long long v_ll  = 0;

        bool ok_int = fs2_pos_parse_int  (&s, 0, &np_int, &v_int);
        bool ok_ll  = fs2_pos_parse_llong(&s, 0, &np_ll,  &v_ll);

        cr_assert_eq(ok_int, ok_ll, "divergence on '%s'", inputs[i]);
        if (ok_int) {
            CHECK_EQ("value",  (long long)v_int, v_ll);
            CHECK_EQ("newpos", np_int, np_ll);
        }
        fs2_free(&s);
    }
    assert_balanced();
}

/* =========================================================================
 * STATIC / LOCAL
 * ========================================================================= */

Test(fs2_api, pos_parse_int_static_src) {
    __fs2_checkreset();
    fs2 s = FS2LITERAL("  -42");
    size_t np = 999; int v = 0;
    cr_assert(fs2_pos_parse_int(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  -42);
    CHECK_EQ("newpos", np, 5);
    CHECK_FS2VALID(1, &s);
    assert_balanced();
}

Test(fs2_api, pos_parse_int_local_src) {
    __fs2_checkreset();
    FS2LOCAL(buf, 16);
    fs2_memcpy_str(&buf, 0, "123", 3);
    fs2_setlen(&buf, 3);

    size_t np = 999; int v = 0;
    cr_assert(fs2_pos_parse_int(&buf, 0, &np, &v));
    CHECK_EQ("value",  v,  123);
    CHECK_EQ("newpos", np, 3);
    CHECK_FS2VALID(1, &buf);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_ullong — nominal
 * ========================================================================= */

Test(fs2_api, pos_parse_ullong_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42");
    size_t np = 999; unsigned long long v = 0;
    cr_assert(fs2_pos_parse_ullong(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  42ULL);
    CHECK_EQ("newpos", np, 2);
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ullong_zero) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("0");
    size_t np = 999; unsigned long long v = 123;
    cr_assert(fs2_pos_parse_ullong(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  0ULL);
    CHECK_EQ("newpos", np, 1);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ullong_leading_zeros) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("007");
    size_t np = 999; unsigned long long v = 0;
    cr_assert(fs2_pos_parse_ullong(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  7ULL);
    CHECK_EQ("newpos", np, 3);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ullong_plus_sign) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("+42");
    size_t np = 999; unsigned long long v = 0;
    cr_assert(fs2_pos_parse_ullong(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  42ULL);
    CHECK_EQ("newpos", np, 3);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ullong_leading_spaces) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("   42");
    size_t np = 999; unsigned long long v = 0;
    cr_assert(fs2_pos_parse_ullong(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  42ULL);
    CHECK_EQ("newpos", np, 5);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ullong_trailing_garbage) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42abc");
    size_t np = 999; unsigned long long v = 0;
    cr_assert(fs2_pos_parse_ullong(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  42ULL);
    CHECK_EQ("newpos", np, 2);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ullong_pos_mid_string) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc123");
    size_t np = 999; unsigned long long v = 0;
    cr_assert(fs2_pos_parse_ullong(&s, 3, &np, &v));
    CHECK_EQ("value",  v,  123ULL);
    CHECK_EQ("newpos", np, 6);
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_ullong — rejection of '-' (key difference from strtoull)
 * ========================================================================= */

Test(fs2_api, pos_parse_ullong_rejects_negative) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("-42");
    size_t np = 12345; unsigned long long v = 999;
    cr_assert_not(fs2_pos_parse_ullong(&s, 0, &np, &v));
    CHECK_EQ("out untouched",    v,  999ULL);
    CHECK_EQ("newpos untouched", np, 12345);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ullong_rejects_negative_with_spaces) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("  -42");
    cr_assert_not(fs2_pos_parse_ullong(&s, 0, NULL, NULL));
    fs2_free(&s);
    assert_balanced();
}

/* -0 → also rejected: the code makes no exception for zero */
Test(fs2_api, pos_parse_ullong_rejects_negative_zero) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("-0");
    size_t np = 12345; unsigned long long v = 999;
    cr_assert_not(fs2_pos_parse_ullong(&s, 0, &np, &v));
    CHECK_EQ("out untouched",    v,  999ULL);
    CHECK_EQ("newpos untouched", np, 12345);
    fs2_free(&s);
    assert_balanced();
}

/* -1 does not become ULLONG_MAX (as with bare strtoull) */
Test(fs2_api, pos_parse_ullong_negative_one_not_ullong_max) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("-1");
    size_t np = 0; unsigned long long v = 0;
    cr_assert_not(fs2_pos_parse_ullong(&s, 0, &np, &v));
    CHECK_NEQ("not ULLONG_MAX", v, ULLONG_MAX);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ullong_rejects_minus_then_plus) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("-+5");
    cr_assert_not(fs2_pos_parse_ullong(&s, 0, NULL, NULL));
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ullong_rejects_double_minus) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("--5");
    cr_assert_not(fs2_pos_parse_ullong(&s, 0, NULL, NULL));
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ullong_rejects_only_minus) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("-");
    cr_assert_not(fs2_pos_parse_ullong(&s, 0, NULL, NULL));
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_ullong — ULLONG boundaries
 * ========================================================================= */

Test(fs2_api, pos_parse_ullong_ullong_max) {
    __fs2_checkreset();
    char buf[64];
    snprintf(buf, sizeof buf, "%llu", ULLONG_MAX);
    fs2 s = fs2_fromstr(buf);

    size_t np = 999; unsigned long long v = 0;
    cr_assert(fs2_pos_parse_ullong(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  ULLONG_MAX);
    CHECK_EQ("newpos", np, strlen(buf));
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ullong_ullong_max_plus_one_false) {
    __fs2_checkreset();
    /* 2^64 — guaranteed > ULLONG_MAX on any platform */
    fs2 s = fs2_fromstr("18446744073709551616");
    size_t np = 12345; unsigned long long v = 999;
    cr_assert_not(fs2_pos_parse_ullong(&s, 0, &np, &v));   /* ERANGE */
    CHECK_EQ("out untouched",    v,  999ULL);
    CHECK_EQ("newpos untouched", np, 12345);
    fs2_free(&s);
    assert_balanced();
}

/* 2^63 = LLONG_MAX + 1 — does not fit in long long, but fits in unsigned long long */
Test(fs2_api, pos_parse_ullong_accepts_2_63) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("9223372036854775808");   /* 2^63 */

    size_t np = 999; unsigned long long v = 0;
    cr_assert(fs2_pos_parse_ullong(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  ((unsigned long long)LLONG_MAX + 1ULL));
    CHECK_EQ("newpos", np, 19);
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

/* Contrast: _llong on 2^63 → false (ERANGE), _ullong → true. Unsigned semantics. */
Test(fs2_api, pos_parse_ullong_vs_llong_above_llong_max) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("9223372036854775808");   /* 2^63 */

    size_t np = 0; long long v_ll = 0; unsigned long long v_ull = 0;

    cr_assert_not(fs2_pos_parse_llong(&s, 0, &np, &v_ll));   /* overflow */
    cr_assert(    fs2_pos_parse_ullong(&s, 0, &np, &v_ull)); /* fits */

    CHECK_EQ("ullong value", v_ull, ((unsigned long long)LLONG_MAX + 1ULL));

    fs2_free(&s);
    assert_balanced();
}

/* LLONG_MAX — fits in both long long and unsigned long long */
Test(fs2_api, pos_parse_ullong_accepts_llong_max) {
    __fs2_checkreset();
    char buf[64];
    snprintf(buf, sizeof buf, "%lld", LLONG_MAX);
    fs2 s = fs2_fromstr(buf);

    size_t np = 999; unsigned long long v = 0;
    cr_assert(fs2_pos_parse_ullong(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  (unsigned long long)LLONG_MAX);
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_ullong — format failures
 * ========================================================================= */

Test(fs2_api, pos_parse_ullong_empty_fs2empty_false) {
    __fs2_checkreset();
    fs2 s = FS2EMPTY;
    size_t np = 12345; unsigned long long v = 999;
    cr_assert_not(fs2_pos_parse_ullong(&s, 0, &np, &v));
    CHECK_EQ("out untouched",    v,  999ULL);
    CHECK_EQ("newpos untouched", np, 12345);
    assert_balanced();
}

Test(fs2_api, pos_parse_ullong_canon_v_null_false) {
    __fs2_checkreset();
    fs2 s = FS2();
    size_t np = 12345; unsigned long long v = 999;
    cr_assert_not(fs2_pos_parse_ullong(&s, 0, &np, &v));
    CHECK_EQ("out untouched",    v,  999ULL);
    CHECK_EQ("newpos untouched", np, 12345);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ullong_null_s_false) {
    size_t np = 12345; unsigned long long v = 999;
    cr_assert_not(fs2_pos_parse_ullong(NULL, 0, &np, &v));
    CHECK_EQ("out untouched",    v,  999ULL);
    CHECK_EQ("newpos untouched", np, 12345);
}

Test(fs2_api, pos_parse_ullong_only_spaces) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("   ");
    cr_assert_not(fs2_pos_parse_ullong(&s, 0, NULL, NULL));
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ullong_letters_rejected) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");
    size_t np = 12345; unsigned long long v = 999;
    cr_assert_not(fs2_pos_parse_ullong(&s, 0, &np, &v));
    CHECK_EQ("out untouched",    v,  999ULL);
    CHECK_EQ("newpos untouched", np, 12345);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ullong_only_plus) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("+");
    cr_assert_not(fs2_pos_parse_ullong(&s, 0, NULL, NULL));
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ullong_plus_then_garbage) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("+abc");
    cr_assert_not(fs2_pos_parse_ullong(&s, 0, NULL, NULL));
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ullong_plus_then_space) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("+  ");
    cr_assert_not(fs2_pos_parse_ullong(&s, 0, NULL, NULL));
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ullong_double_plus) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("++5");
    cr_assert_not(fs2_pos_parse_ullong(&s, 0, NULL, NULL));
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ullong_pos_out_of_range) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42");
    cr_assert_not(fs2_pos_parse_ullong(&s, 100, NULL, NULL));
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_ullong — optional out / newpos
 * ========================================================================= */

Test(fs2_api, pos_parse_ullong_out_null) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42");
    size_t np = 999;
    cr_assert(fs2_pos_parse_ullong(&s, 0, &np, NULL));
    CHECK_EQ("newpos", np, 2);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ullong_newpos_null) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42");
    unsigned long long v = 0;
    cr_assert(fs2_pos_parse_ullong(&s, 0, NULL, &v));
    CHECK_EQ("value", v, 42ULL);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ullong_both_null) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42");
    cr_assert(fs2_pos_parse_ullong(&s, 0, NULL, NULL));
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_ullong — STATIC / LOCAL
 * ========================================================================= */

Test(fs2_api, pos_parse_ullong_static_src) {
    __fs2_checkreset();
    fs2 s = FS2LITERAL("  42");
    size_t np = 999; unsigned long long v = 0;
    cr_assert(fs2_pos_parse_ullong(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  42ULL);
    CHECK_EQ("newpos", np, 4);
    CHECK_FS2VALID(1, &s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ullong_local_src) {
    __fs2_checkreset();
    FS2LOCAL(buf, 16);
    fs2_memcpy_str(&buf, 0, "123", 3);
    fs2_setlen(&buf, 3);

    size_t np = 999; unsigned long long v = 0;
    cr_assert(fs2_pos_parse_ullong(&buf, 0, &np, &v));
    CHECK_EQ("value",  v,  123ULL);
    CHECK_EQ("newpos", np, 3);
    CHECK_FS2VALID(1, &buf);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_ulong — nominal
 * ========================================================================= */

Test(fs2_api, pos_parse_ulong_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42");
    size_t np = 999; unsigned long v = 0;
    cr_assert(fs2_pos_parse_ulong(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  42UL);
    CHECK_EQ("newpos", np, 2);
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ulong_leading_spaces) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("   42");
    size_t np = 999; unsigned long v = 0;
    cr_assert(fs2_pos_parse_ulong(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  42UL);
    CHECK_EQ("newpos", np, 5);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ulong_plus_sign) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("+42");
    size_t np = 999; unsigned long v = 0;
    cr_assert(fs2_pos_parse_ulong(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  42UL);
    CHECK_EQ("newpos", np, 3);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ulong_trailing_garbage) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42abc");
    size_t np = 999; unsigned long v = 0;
    cr_assert(fs2_pos_parse_ulong(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  42UL);
    CHECK_EQ("newpos", np, 2);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ulong_pos_mid_string) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc123");
    size_t np = 999; unsigned long v = 0;
    cr_assert(fs2_pos_parse_ulong(&s, 3, &np, &v));
    CHECK_EQ("value",  v,  123UL);
    CHECK_EQ("newpos", np, 6);
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_ulong — inherited reject "-"
 * ========================================================================= */

Test(fs2_api, pos_parse_ulong_rejects_negative) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("-42");
    size_t np = 12345; unsigned long v = 999;
    cr_assert_not(fs2_pos_parse_ulong(&s, 0, &np, &v));
    CHECK_EQ("out untouched",    v,  999UL);
    CHECK_EQ("newpos untouched", np, 12345);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ulong_rejects_negative_one) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("-1");
    size_t np = 0; unsigned long v = 0;
    cr_assert_not(fs2_pos_parse_ulong(&s, 0, &np, &v));
    CHECK_NEQ("not ULONG_MAX", v, ULONG_MAX);   /* contrast with bare strtoul */
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_ulong — ULONG boundaries (accepted)
 * ========================================================================= */

Test(fs2_api, pos_parse_ulong_ulong_max) {
    __fs2_checkreset();
    char buf[64];
    snprintf(buf, sizeof buf, "%lu", ULONG_MAX);
    fs2 s = fs2_fromstr(buf);

    size_t np = 999; unsigned long v = 0;
    cr_assert(fs2_pos_parse_ulong(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  ULONG_MAX);
    CHECK_EQ("newpos", np, strlen(buf));
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

/* 2^63 = LLONG_MAX+1 — fits in both unsigned long long and unsigned long on LP64 */
Test(fs2_api, pos_parse_ulong_accepts_2_63_on_lp64) {
#if ULONG_MAX >= (1ULL << 63)
    __fs2_checkreset();
    fs2 s = fs2_fromstr("9223372036854775808");   /* 2^63 */

    size_t np = 999; unsigned long v = 0;
    cr_assert(fs2_pos_parse_ulong(&s, 0, &np, &v));
    CHECK_EQ("value", v, (unsigned long)((unsigned long long)LLONG_MAX + 1ULL));
    CHECK_EQ("newpos", np, 19);
    fs2_free(&s);
    assert_balanced();
#endif
}

/* =========================================================================
 * fs2_pos_parse_ulong — range-violation (LLP64 / ILP32 only)
 * ========================================================================= */

#if ULONG_MAX < ULLONG_MAX
Test(fs2_api, pos_parse_ulong_above_ulong_max_false_no_write) {
    __fs2_checkreset();
    /* ULLONG_MAX is necessarily > ULONG_MAX under this #if */
    char buf[64];
    snprintf(buf, sizeof buf, "%llu", ULLONG_MAX);
    fs2 s = fs2_fromstr(buf);

    size_t np = 12345; unsigned long v = 999;
    cr_assert_not(fs2_pos_parse_ulong(&s, 0, &np, &v));
    CHECK_EQ("out untouched",    v,  999UL);
    CHECK_EQ("newpos untouched", np, 12345);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ulong_ulong_max_plus_one_false) {
    __fs2_checkreset();
    char buf[64];
    snprintf(buf, sizeof buf, "%llu", (unsigned long long)ULONG_MAX + 1ULL);
    fs2 s = fs2_fromstr(buf);

    size_t np = 12345; unsigned long v = 999;
    cr_assert_not(fs2_pos_parse_ulong(&s, 0, &np, &v));
    CHECK_EQ("out untouched",    v,  999UL);
    CHECK_EQ("newpos untouched", np, 12345);
    fs2_free(&s);
    assert_balanced();
}
#endif /* ULONG_MAX < ULLONG_MAX */

/* =========================================================================
 * fs2_pos_parse_ulong — failures from delegation, out/newpos are NOT written
 * ========================================================================= */

Test(fs2_api, pos_parse_ulong_empty_fs2empty_false) {
    __fs2_checkreset();
    fs2 s = FS2EMPTY;
    size_t np = 12345; unsigned long v = 999;
    cr_assert_not(fs2_pos_parse_ulong(&s, 0, &np, &v));
    CHECK_EQ("out untouched",    v,  999UL);
    CHECK_EQ("newpos untouched", np, 12345);
    assert_balanced();
}

Test(fs2_api, pos_parse_ulong_canon_v_null_false) {
    __fs2_checkreset();
    fs2 s = FS2();
    size_t np = 12345; unsigned long v = 999;
    cr_assert_not(fs2_pos_parse_ulong(&s, 0, &np, &v));
    CHECK_EQ("out untouched",    v,  999UL);
    CHECK_EQ("newpos untouched", np, 12345);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ulong_null_s_false) {
    size_t np = 12345; unsigned long v = 999;
    cr_assert_not(fs2_pos_parse_ulong(NULL, 0, &np, &v));
    CHECK_EQ("out untouched",    v,  999UL);
    CHECK_EQ("newpos untouched", np, 12345);
}

Test(fs2_api, pos_parse_ulong_invalid_format_false_no_write) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");
    size_t np = 12345; unsigned long v = 999;
    cr_assert_not(fs2_pos_parse_ulong(&s, 0, &np, &v));
    CHECK_EQ("out untouched",    v,  999UL);
    CHECK_EQ("newpos untouched", np, 12345);
    fs2_free(&s);
    assert_balanced();
}

/* ERANGE in strtoull — not ulong range-check, but beyond ullong */
Test(fs2_api, pos_parse_ulong_ullong_overflow_false) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("18446744073709551616");   /* 2^64 */
    cr_assert_not(fs2_pos_parse_ulong(&s, 0, NULL, NULL));
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ulong_pos_out_of_range_false) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42");
    cr_assert_not(fs2_pos_parse_ulong(&s, 100, NULL, NULL));
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ulong_only_spaces) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("   ");
    cr_assert_not(fs2_pos_parse_ulong(&s, 0, NULL, NULL));
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ulong_only_plus) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("+");
    cr_assert_not(fs2_pos_parse_ulong(&s, 0, NULL, NULL));
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_ulong — optional out / newpos
 * ========================================================================= */

Test(fs2_api, pos_parse_ulong_out_null) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42");
    size_t np = 999;
    cr_assert(fs2_pos_parse_ulong(&s, 0, &np, NULL));
    CHECK_EQ("newpos", np, 2);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ulong_newpos_null) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42");
    unsigned long v = 0;
    cr_assert(fs2_pos_parse_ulong(&s, 0, NULL, &v));
    CHECK_EQ("value", v, 42UL);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ulong_both_null) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42");
    cr_assert(fs2_pos_parse_ulong(&s, 0, NULL, NULL));
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * Delegation: _ulong ≡ _ullong for small values
 * ========================================================================= */

Test(fs2_api, pos_parse_ulong_delegates_to_ullong) {
    __fs2_checkreset();
    static const char *inputs[] = {
        "0", "42", "+7", "  123", "1x", "   -9abc", "abc", NULL
    };
    for (size_t i = 0; inputs[i]; i++) {
        fs2 s = fs2_fromstr(inputs[i]);
        size_t np_ul = 0, np_ull = 0;
        unsigned long      v_ul  = 0;
        unsigned long long v_ull = 0;

        bool ok_ul  = fs2_pos_parse_ulong (&s, 0, &np_ul,  &v_ul);
        bool ok_ull = fs2_pos_parse_ullong(&s, 0, &np_ull, &v_ull);

        cr_assert_eq(ok_ul, ok_ull, "divergence on '%s'", inputs[i]);
        if (ok_ul) {
            CHECK_EQ("value",  (unsigned long long)v_ul, v_ull);
            CHECK_EQ("newpos", np_ul, np_ull);
        }
        fs2_free(&s);
    }
    assert_balanced();
}

/* =========================================================================
 * STATIC / LOCAL
 * ========================================================================= */

Test(fs2_api, pos_parse_ulong_static_src) {
    __fs2_checkreset();
    fs2 s = FS2LITERAL("  42");
    size_t np = 999; unsigned long v = 0;
    cr_assert(fs2_pos_parse_ulong(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  42UL);
    CHECK_EQ("newpos", np, 4);
    CHECK_FS2VALID(1, &s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ulong_local_src) {
    __fs2_checkreset();
    FS2LOCAL(buf, 16);
    fs2_memcpy_str(&buf, 0, "123", 3);
    fs2_setlen(&buf, 3);

    size_t np = 999; unsigned long v = 0;
    cr_assert(fs2_pos_parse_ulong(&buf, 0, &np, &v));
    CHECK_EQ("value",  v,  123UL);
    CHECK_EQ("newpos", np, 3);
    CHECK_FS2VALID(1, &buf);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_uint — nominal
 * ========================================================================= */

Test(fs2_api, pos_parse_uint_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42");
    size_t np = 999; unsigned v = 0;
    cr_assert(fs2_pos_parse_uint(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  42u);
    CHECK_EQ("newpos", np, 2);
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_uint_zero) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("0");
    size_t np = 999; unsigned v = 123;
    cr_assert(fs2_pos_parse_uint(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  0u);
    CHECK_EQ("newpos", np, 1);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_uint_leading_spaces_and_plus) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("   +42");
    size_t np = 999; unsigned v = 0;
    cr_assert(fs2_pos_parse_uint(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  42u);
    CHECK_EQ("newpos", np, 6);   /* 3 spaces + '+' + 2 digits */
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_uint_trailing_garbage) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42abc");
    size_t np = 999; unsigned v = 0;
    cr_assert(fs2_pos_parse_uint(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  42u);
    CHECK_EQ("newpos", np, 2);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_uint_pos_mid_string) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc123");
    size_t np = 999; unsigned v = 0;
    cr_assert(fs2_pos_parse_uint(&s, 3, &np, &v));
    CHECK_EQ("value",  v,  123u);
    CHECK_EQ("newpos", np, 6);
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_uint — reject "-" (inherited via _ulong/_ullong)
 * ========================================================================= */

Test(fs2_api, pos_parse_uint_rejects_negative) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("-42");
    size_t np = 12345; unsigned v = 999;
    cr_assert_not(fs2_pos_parse_uint(&s, 0, &np, &v));
    CHECK_EQ("out untouched",    v,  999u);
    CHECK_EQ("newpos untouched", np, 12345);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_uint_rejects_negative_one) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("-1");
    size_t np = 0; unsigned v = 0;
    cr_assert_not(fs2_pos_parse_uint(&s, 0, &np, &v));
    CHECK_NEQ("not UINT_MAX", v, UINT_MAX);   /* contrast with bare strtoul */
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_uint — UINT boundaries (accepted)
 * ========================================================================= */

Test(fs2_api, pos_parse_uint_uint_max) {
    __fs2_checkreset();
    char buf[32];
    snprintf(buf, sizeof buf, "%u", UINT_MAX);
    fs2 s = fs2_fromstr(buf);

    size_t np = 999; unsigned v = 0;
    cr_assert(fs2_pos_parse_uint(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  UINT_MAX);
    CHECK_EQ("newpos", np, strlen(buf));
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_uint — UINT_MAX + 1 → false (all platforms)
 * ========================================================================= */

Test(fs2_api, pos_parse_uint_uint_max_plus_one_false_no_write) {
    __fs2_checkreset();
    char buf[32];
    snprintf(buf, sizeof buf, "%llu", (unsigned long long)UINT_MAX + 1ULL);
    fs2 s = fs2_fromstr(buf);

    size_t np = 12345; unsigned v = 999;
    cr_assert_not(fs2_pos_parse_uint(&s, 0, &np, &v));
    CHECK_EQ("out untouched",    v,  999u);
    CHECK_EQ("newpos untouched", np, 12345);
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_uint — ACTIVE range-check in _uint
 *   Only when ULONG_MAX > UINT_MAX (LP64): value > UINT_MAX,
 *   but fits in ulong → _ulong parses successfully, _uint rejects by range.
 *   On LLP64 (ULONG_MAX == UINT_MAX) _ulong rejects earlier → dead.
 * ========================================================================= */

#if ULONG_MAX > UINT_MAX
Test(fs2_api, pos_parse_uint_range_check_active_on_lp64) {
    __fs2_checkreset();
    /* value fits in ulong (LP64: 64-bit), but > UINT_MAX */
    char buf[32];
    snprintf(buf, sizeof buf, "%llu", (unsigned long long)UINT_MAX + 1ULL);
    fs2 s = fs2_fromstr(buf);

    /* _ulong accepts successfully */
    unsigned long ul = 0;
    size_t np_ul = 0;
    cr_assert(fs2_pos_parse_ulong(&s, 0, &np_ul, &ul),
              "precondition: _ulong must accept this value on this platform");
    cr_assert(ul > UINT_MAX, "precondition: ul > UINT_MAX");

    /* _uint rejects by range */
    size_t np = 12345; unsigned v = 999;
    cr_assert_not(fs2_pos_parse_uint(&s, 0, &np, &v));
    CHECK_EQ("out untouched",    v,  999u);
    CHECK_EQ("newpos untouched", np, 12345);

    fs2_free(&s);
    assert_balanced();
}
#endif /* ULONG_MAX > UINT_MAX */

/* =========================================================================
 * fs2_pos_parse_uint — failures from delegation, out/newpos are NOT written
 * ========================================================================= */

Test(fs2_api, pos_parse_uint_empty_fs2empty_false) {
    __fs2_checkreset();
    fs2 s = FS2EMPTY;
    size_t np = 12345; unsigned v = 999;
    cr_assert_not(fs2_pos_parse_uint(&s, 0, &np, &v));
    CHECK_EQ("out untouched",    v,  999u);
    CHECK_EQ("newpos untouched", np, 12345);
    assert_balanced();
}

Test(fs2_api, pos_parse_uint_canon_v_null_false) {
    __fs2_checkreset();
    fs2 s = FS2();
    size_t np = 12345; unsigned v = 999;
    cr_assert_not(fs2_pos_parse_uint(&s, 0, &np, &v));
    CHECK_EQ("out untouched",    v,  999u);
    CHECK_EQ("newpos untouched", np, 12345);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_uint_null_s_false) {
    size_t np = 12345; unsigned v = 999;
    cr_assert_not(fs2_pos_parse_uint(NULL, 0, &np, &v));
    CHECK_EQ("out untouched",    v,  999u);
    CHECK_EQ("newpos untouched", np, 12345);
}

Test(fs2_api, pos_parse_uint_invalid_format_false_no_write) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");
    size_t np = 12345; unsigned v = 999;
    cr_assert_not(fs2_pos_parse_uint(&s, 0, &np, &v));
    CHECK_EQ("out untouched",    v,  999u);
    CHECK_EQ("newpos untouched", np, 12345);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_uint_only_spaces_false) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("   ");
    cr_assert_not(fs2_pos_parse_uint(&s, 0, NULL, NULL));
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_uint_only_plus_false) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("+");
    cr_assert_not(fs2_pos_parse_uint(&s, 0, NULL, NULL));
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_uint_pos_out_of_range_false) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42");
    cr_assert_not(fs2_pos_parse_uint(&s, 100, NULL, NULL));
    fs2_free(&s);
    assert_balanced();
}

/* ERANGE in strtoull — not uint range-check, but beyond ullong */
Test(fs2_api, pos_parse_uint_ullong_overflow_false) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("18446744073709551616");   /* 2^64 */
    cr_assert_not(fs2_pos_parse_uint(&s, 0, NULL, NULL));
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_uint — optional out / newpos
 * ========================================================================= */

Test(fs2_api, pos_parse_uint_out_null) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42");
    size_t np = 999;
    cr_assert(fs2_pos_parse_uint(&s, 0, &np, NULL));
    CHECK_EQ("newpos", np, 2);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_uint_newpos_null) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42");
    unsigned v = 0;
    cr_assert(fs2_pos_parse_uint(&s, 0, NULL, &v));
    CHECK_EQ("value", v, 42u);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_uint_both_null) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42");
    cr_assert(fs2_pos_parse_uint(&s, 0, NULL, NULL));
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * Delegation: _uint ≡ _ulong for values in uint range
 * ========================================================================= */

Test(fs2_api, pos_parse_uint_delegates_to_ulong) {
    __fs2_checkreset();
    static const char *inputs[] = {
        "0", "42", "+7", "  123", "1x", "abc", NULL
    };
    for (size_t i = 0; inputs[i]; i++) {
        fs2 s = fs2_fromstr(inputs[i]);
        size_t np_u = 0, np_ul = 0;
        unsigned      v_u  = 0;
        unsigned long v_ul = 0;

        bool ok_u  = fs2_pos_parse_uint (&s, 0, &np_u,  &v_u);
        bool ok_ul = fs2_pos_parse_ulong(&s, 0, &np_ul, &v_ul);

        cr_assert_eq(ok_u, ok_ul, "divergence on '%s'", inputs[i]);
        if (ok_u) {
            CHECK_EQ("value",  (unsigned long)v_u, v_ul);
            CHECK_EQ("newpos", np_u, np_ul);
        }
        fs2_free(&s);
    }
    assert_balanced();
}

/* =========================================================================
 * STATIC / LOCAL
 * ========================================================================= */

Test(fs2_api, pos_parse_uint_static_src) {
    __fs2_checkreset();
    fs2 s = FS2LITERAL("  42");
    size_t np = 999; unsigned v = 0;
    cr_assert(fs2_pos_parse_uint(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  42u);
    CHECK_EQ("newpos", np, 4);
    CHECK_FS2VALID(1, &s);
    assert_balanced();
}

Test(fs2_api, pos_parse_uint_local_src) {
    __fs2_checkreset();
    FS2LOCAL(buf, 16);
    fs2_memcpy_str(&buf, 0, "123", 3);
    fs2_setlen(&buf, 3);

    size_t np = 999; unsigned v = 0;
    cr_assert(fs2_pos_parse_uint(&buf, 0, &np, &v));
    CHECK_EQ("value",  v,  123u);
    CHECK_EQ("newpos", np, 3);
    CHECK_FS2VALID(1, &buf);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_double — nominal
 * ========================================================================= */

Test(fs2_api, pos_parse_double_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("3.14");
    size_t np = 999; double v = 0;
    cr_assert(fs2_pos_parse_double(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  3.14);
    CHECK_EQ("newpos", np, 4);
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_double_negative) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("-2.5");
    size_t np = 999; double v = 0;
    cr_assert(fs2_pos_parse_double(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  -2.5);
    CHECK_EQ("newpos", np, 4);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_double_plus_sign) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("+1.0");
    size_t np = 999; double v = 0;
    cr_assert(fs2_pos_parse_double(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  1.0);
    CHECK_EQ("newpos", np, 4);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_double_zero) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("0");
    size_t np = 999; double v = 999;
    cr_assert(fs2_pos_parse_double(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  0.0);
    CHECK_EQ("newpos", np, 1);
    fs2_free(&s);
    assert_balanced();
}

/* "42" is parsed as float 42.0 (not a separate int path) */
Test(fs2_api, pos_parse_double_int_via_double) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42");
    size_t np = 999; double v = 0;
    cr_assert(fs2_pos_parse_double(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  42.0);
    CHECK_EQ("newpos", np, 2);
    fs2_free(&s);
    assert_balanced();
}

/* ".5" → 0.5, newpos = 2 */
Test(fs2_api, pos_parse_double_leading_dot) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr(".5");
    size_t np = 999; double v = 0;
    cr_assert(fs2_pos_parse_double(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  0.5);
    CHECK_EQ("newpos", np, 2);
    fs2_free(&s);
    assert_balanced();
}

/* "5." → 5.0, newpos = 2 (C99: "5." is a valid decimal-floating-constant) */
Test(fs2_api, pos_parse_double_trailing_dot) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("5.");
    size_t np = 999; double v = 0;
    cr_assert(fs2_pos_parse_double(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  5.0);
    CHECK_EQ("newpos", np, 2);
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_double — scientific notation
 * ========================================================================= */

Test(fs2_api, pos_parse_double_scientific_lower) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("1.5e3");
    size_t np = 999; double v = 0;
    cr_assert(fs2_pos_parse_double(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  1500.0);
    CHECK_EQ("newpos", np, 5);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_double_scientific_upper) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("1.5E3");
    size_t np = 999; double v = 0;
    cr_assert(fs2_pos_parse_double(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  1500.0);
    CHECK_EQ("newpos", np, 5);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_double_scientific_neg_exp) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("1.5e-3");
    size_t np = 999; double v = 0;
    cr_assert(fs2_pos_parse_double(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  1.5e-3);
    CHECK_EQ("newpos", np, 6);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_double_scientific_pos_exp) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("1.5e+3");
    size_t np = 999; double v = 0;
    cr_assert(fs2_pos_parse_double(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  1500.0);
    CHECK_EQ("newpos", np, 6);
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_double — spaces / pos > 0 / trailing
 * ========================================================================= */

Test(fs2_api, pos_parse_double_leading_spaces) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("   3.14");
    size_t np = 999; double v = 0;
    cr_assert(fs2_pos_parse_double(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  3.14);
    CHECK_EQ("newpos", np, 7);   /* 3 spaces + 4 characters */
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_double_trailing_garbage) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("3.14abc");
    size_t np = 999; double v = 0;
    cr_assert(fs2_pos_parse_double(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  3.14);
    CHECK_EQ("newpos", np, 4);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_double_pos_mid_string) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc-2.5");
    size_t np = 999; double v = 0;
    cr_assert(fs2_pos_parse_double(&s, 3, &np, &v));
    CHECK_EQ("value",  v,  -2.5);
    CHECK_EQ("newpos", np, 7);
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_double — inf / nan (case-insensitive per C99)
 * ========================================================================= */

Test(fs2_api, pos_parse_double_inf_lower) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("inf");
    size_t np = 999; double v = 0;
    cr_assert(fs2_pos_parse_double(&s, 0, &np, &v));
    CHECK_EQ("is inf",   isinf(v), true);
    CHECK_EQ("positive", v > 0,    true);
    CHECK_EQ("newpos",   np,       3);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_double_inf_upper) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("INF");
    size_t np = 999; double v = 0;
    cr_assert(fs2_pos_parse_double(&s, 0, &np, &v));
    CHECK_EQ("is inf", isinf(v), true);
    CHECK_EQ("newpos", np, 3);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_double_infinity) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("infinity");
    size_t np = 999; double v = 0;
    cr_assert(fs2_pos_parse_double(&s, 0, &np, &v));
    CHECK_EQ("is inf", isinf(v), true);
    CHECK_EQ("newpos", np, 8);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_double_neg_inf) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("-inf");
    size_t np = 999; double v = 0;
    cr_assert(fs2_pos_parse_double(&s, 0, &np, &v));
    CHECK_NEQ("is inf",   isinf(v), 0); 
    CHECK_EQ("negative", v < 0,    true);
    CHECK_EQ("newpos",   np,       4);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_double_nan_lower) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("nan");
    size_t np = 999; double v = 0;
    cr_assert(fs2_pos_parse_double(&s, 0, &np, &v));
    CHECK_EQ("is nan", isnan(v), true);
    CHECK_EQ("newpos", np,       3);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_double_nan_upper) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("NAN");
    size_t np = 999; double v = 0;
    cr_assert(fs2_pos_parse_double(&s, 0, &np, &v));
    CHECK_EQ("is nan", isnan(v), true);
    CHECK_EQ("newpos", np,       3);
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_double — overflow → ERANGE → false
 * ========================================================================= */

Test(fs2_api, pos_parse_double_overflow_false_no_write) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("1e999");   /* >> DBL_MAX */
    size_t np = 12345; double v = 999;
    cr_assert_not(fs2_pos_parse_double(&s, 0, &np, &v));
    CHECK_EQ("out untouched",    v,  999.0);
    CHECK_EQ("newpos untouched", np, 12345);
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_double — fail-cases: "." / "+." / "-" / letters
 * ========================================================================= */

Test(fs2_api, pos_parse_double_only_dot_false) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr(".");
    size_t np = 12345; double v = 999;
    cr_assert_not(fs2_pos_parse_double(&s, 0, &np, &v));   /* no digits */
    CHECK_EQ("out untouched",    v,  999.0);
    CHECK_EQ("newpos untouched", np, 12345);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_double_only_plus_false) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("+");
    cr_assert_not(fs2_pos_parse_double(&s, 0, NULL, NULL));
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_double_only_minus_false) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("-");
    cr_assert_not(fs2_pos_parse_double(&s, 0, NULL, NULL));
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_double_sign_dot_false) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("+.");
    cr_assert_not(fs2_pos_parse_double(&s, 0, NULL, NULL));

    fs2 t = fs2_fromstr("-.");
    cr_assert_not(fs2_pos_parse_double(&t, 0, NULL, NULL));

    fs2_free(&s);
    fs2_free(&t);
    assert_balanced();
}

/* 'e' as first char — rejected by pre-check (e5 is not a float) */
Test(fs2_api, pos_parse_double_e_alone_rejected) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("e5");
    cr_assert_not(fs2_pos_parse_double(&s, 0, NULL, NULL));
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_double_letters_rejected) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");
    cr_assert_not(fs2_pos_parse_double(&s, 0, NULL, NULL));
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_double_only_spaces_false) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("   ");
    cr_assert_not(fs2_pos_parse_double(&s, 0, NULL, NULL));
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_double_pos_out_of_range) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("3.14");
    cr_assert_not(fs2_pos_parse_double(&s, 100, NULL, NULL));
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_double — empty / canon / NULL
 * ========================================================================= */

Test(fs2_api, pos_parse_double_empty_fs2empty) {
    __fs2_checkreset();
    fs2 s = FS2EMPTY;
    size_t np = 12345; double v = 999;
    cr_assert_not(fs2_pos_parse_double(&s, 0, &np, &v));
    CHECK_EQ("out untouched",    v,  999.0);
    CHECK_EQ("newpos untouched", np, 12345);
    assert_balanced();
}

Test(fs2_api, pos_parse_double_canon_v_null) {
    __fs2_checkreset();
    fs2 s = FS2();
    size_t np = 12345; double v = 999;
    cr_assert_not(fs2_pos_parse_double(&s, 0, &np, &v));
    CHECK_EQ("out untouched",    v,  999.0);
    CHECK_EQ("newpos untouched", np, 12345);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_double_null_s) {
    size_t np = 12345; double v = 999;
    cr_assert_not(fs2_pos_parse_double(NULL, 0, &np, &v));
    CHECK_EQ("out untouched",    v,  999.0);
    CHECK_EQ("newpos untouched", np, 12345);
}

/* =========================================================================
 * fs2_pos_parse_double — optional out / newpos
 * ========================================================================= */

Test(fs2_api, pos_parse_double_out_null) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("3.14");
    size_t np = 999;
    cr_assert(fs2_pos_parse_double(&s, 0, &np, NULL));
    CHECK_EQ("newpos", np, 4);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_double_newpos_null) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("3.14");
    double v = 0;
    cr_assert(fs2_pos_parse_double(&s, 0, NULL, &v));
    CHECK_EQ("value", v, 3.14);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_double_both_null) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("-2.5");
    cr_assert(fs2_pos_parse_double(&s, 0, NULL, NULL));
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_double — high byte (no UB)
 * ========================================================================= */

Test(fs2_api, pos_parse_double_high_byte_rejected_no_ub) {
    __fs2_checkreset();
    char *raw = strdup("\xE9" "3.14");   /* first byte 0xE9 */
    cr_assert(raw != NULL);
    fs2 s = fs2_adopt(&raw);
    cr_assert(raw == NULL);

    /* 0xE9 is not digit/dot/sign/inf/nan → "Incorrect sym" */
    cr_assert_not(fs2_pos_parse_double(&s, 0, NULL, NULL));
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_double — STATIC / LOCAL
 * ========================================================================= */

Test(fs2_api, pos_parse_double_static_src) {
    __fs2_checkreset();
    fs2 s = FS2LITERAL("  -2.5");
    size_t np = 999; double v = 0;
    cr_assert(fs2_pos_parse_double(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  -2.5);
    CHECK_EQ("newpos", np, 6);
    CHECK_FS2VALID(1, &s);
    assert_balanced();
}

Test(fs2_api, pos_parse_double_local_src) {
    __fs2_checkreset();
    FS2LOCAL(buf, 16);
    fs2_memcpy_str(&buf, 0, "3.14", 4);
    fs2_setlen(&buf, 4);

    size_t np = 999; double v = 0;
    cr_assert(fs2_pos_parse_double(&buf, 0, &np, &v));
    CHECK_EQ("value",  v,  3.14);
    CHECK_EQ("newpos", np, 4);
    CHECK_FS2VALID(1, &buf);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_char — nominal
 * ========================================================================= */

Test(fs2_api, pos_parse_char_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");
    size_t np = 999; char c = 0;
    cr_assert(fs2_pos_parse_char(&s, 0, &np, &c));
    CHECK_EQ("char",   c,  'a');
    CHECK_EQ("newpos", np, 1);
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_char_last) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");
    size_t np = 999; char c = 0;
    cr_assert(fs2_pos_parse_char(&s, 2, &np, &c));
    CHECK_EQ("char",   c,  'c');
    CHECK_EQ("newpos", np, 3);   /* == s.len */
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_char_mid) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");
    size_t np = 999; char c = 0;
    cr_assert(fs2_pos_parse_char(&s, 2, &np, &c));
    CHECK_EQ("char",   c,  'l');
    CHECK_EQ("newpos", np, 3);
    fs2_free(&s);
    assert_balanced();
}

/* Single character */
Test(fs2_api, pos_parse_char_single_char_string) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("X");
    size_t np = 999; char c = 0;
    cr_assert(fs2_pos_parse_char(&s, 0, &np, &c));
    CHECK_EQ("char",   c,  'X');
    CHECK_EQ("newpos", np, 1);
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_char — any byte is valid (no skip_spaces, no reject)
 * ========================================================================= */

Test(fs2_api, pos_parse_char_space_is_char) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("  ");
    size_t np = 999; char c = 0;
    cr_assert(fs2_pos_parse_char(&s, 0, &np, &c));
    CHECK_EQ("char", c,  ' ');   /* space — a valid character */
    CHECK_EQ("newpos", np, 1);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_char_sign_is_char) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("+-.");
    size_t np = 0; char c = 0;

    cr_assert(fs2_pos_parse_char(&s, 0, &np, &c));
    CHECK_EQ("'+'", c,  '+');
    CHECK_EQ("newpos", np, 1);

    cr_assert(fs2_pos_parse_char(&s, 1, &np, &c));
    CHECK_EQ("'-'", c,  '-');

    cr_assert(fs2_pos_parse_char(&s, 2, &np, &c));
    CHECK_EQ("'.'", c,  '.');

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_char_digit_is_char) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("7");
    size_t np = 999; char c = 0;
    cr_assert(fs2_pos_parse_char(&s, 0, &np, &c));
    CHECK_EQ("char", c,  '7');
    fs2_free(&s);
    assert_balanced();
}

/* High byte (>= 0x80) — just a byte, no isdigit/isspace */
Test(fs2_api, pos_parse_char_high_byte) {
    __fs2_checkreset();
    char *raw = strdup("A\xE9");
    cr_assert(raw != NULL);
    fs2 s = fs2_adopt(&raw);
    cr_assert(raw == NULL);
    cr_assert_eq(s.len, 2);

    size_t np = 999; char c = 0;
    cr_assert(fs2_pos_parse_char(&s, 1, &np, &c));
    CHECK_EQ("high byte", (unsigned char)c, 0xE9u);
    CHECK_EQ("newpos", np, 2);
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_char — optional out / newpos
 * ========================================================================= */

Test(fs2_api, pos_parse_char_out_null) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");
    size_t np = 999;
    cr_assert(fs2_pos_parse_char(&s, 0, &np, NULL));
    CHECK_EQ("newpos", np, 1);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_char_newpos_null) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");
    char c = 0;
    cr_assert(fs2_pos_parse_char(&s, 0, NULL, &c));
    CHECK_EQ("char", c, 'a');
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_char_both_null) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");
    cr_assert(fs2_pos_parse_char(&s, 0, NULL, NULL));
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_char — fail-cases: out/newpos are NOT written
 * ========================================================================= */

Test(fs2_api, pos_parse_char_pos_eq_len_false) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");
    size_t np = 12345; char c = 'Z';
    cr_assert_not(fs2_pos_parse_char(&s, 3, &np, &c));   /* pos == len */
    CHECK_EQ("out untouched",    c,  'Z');
    CHECK_EQ("newpos untouched", np, 12345);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_char_pos_gt_len_false) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");
    size_t np = 12345; char c = 'Z';
    cr_assert_not(fs2_pos_parse_char(&s, 100, &np, &c));
    CHECK_EQ("out untouched",    c,  'Z');
    CHECK_EQ("newpos untouched", np, 12345);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_char_empty_fs2empty_false) {
    __fs2_checkreset();
    fs2 s = FS2EMPTY;   /* v="", len=0 */
    size_t np = 12345; char c = 'Z';
    cr_assert_not(fs2_pos_parse_char(&s, 0, &np, &c));   /* pos >= len */
    CHECK_EQ("out untouched",    c,  'Z');
    CHECK_EQ("newpos untouched", np, 12345);
    assert_balanced();
}

/* FS2() (v=NULL) — guard !s->v → false, no read */
Test(fs2_api, pos_parse_char_canon_v_null_false) {
    __fs2_checkreset();
    fs2 s = FS2();
    cr_assert_null(s.v, "precondition: FS2() has v == NULL");
    size_t np = 12345; char c = 'Z';
    cr_assert_not(fs2_pos_parse_char(&s, 0, &np, &c));
    CHECK_EQ("out untouched",    c,  'Z');
    CHECK_EQ("newpos untouched", np, 12345);
    fs2_free(&s);
    assert_balanced();
}

/* NULL s — soft false (logsimpleerr with `s ? s->len : 0`) */
Test(fs2_api, pos_parse_char_null_s_false) {
    size_t np = 12345; char c = 'Z';
    cr_assert_not(fs2_pos_parse_char(NULL, 0, &np, &c));
    CHECK_EQ("out untouched",    c,  'Z');
    CHECK_EQ("newpos untouched", np, 12345);
}

/* =========================================================================
 * fs2_pos_parse_char — STATIC / LOCAL
 * ========================================================================= */

Test(fs2_api, pos_parse_char_static_src) {
    __fs2_checkreset();
    fs2 s = FS2LITERAL("xyz");
    size_t np = 999; char c = 0;
    cr_assert(fs2_pos_parse_char(&s, 1, &np, &c));
    CHECK_EQ("char",   c,  'y');
    CHECK_EQ("newpos", np, 2);
    CHECK_FS2VALID(1, &s);
    assert_balanced();
}

Test(fs2_api, pos_parse_char_local_src) {
    __fs2_checkreset();
    FS2LOCAL(buf, 16);
    fs2_memcpy_str(&buf, 0, "abc", 3);
    fs2_setlen(&buf, 3);

    size_t np = 999; char c = 0;
    cr_assert(fs2_pos_parse_char(&buf, 2, &np, &c));
    CHECK_EQ("char",   c,  'c');
    CHECK_EQ("newpos", np, 3);
    CHECK_FS2VALID(1, &buf);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_char — sequential reading of a string
 * ========================================================================= */

Test(fs2_api, pos_parse_char_sequential_reads) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");
    const char *expected = "abc";
    size_t pos = 0;
    char c;

    for (size_t i = 0; i < 3; i++) {
        cr_assert(fs2_pos_parse_char(&s, pos, &pos, &c));
        CHECK_EQ("char matches", c, expected[i]);
    }
    CHECK_EQ("pos advanced to len", pos, s.len);

    /* one more — out of bounds */
    cr_assert_not(fs2_pos_parse_char(&s, pos, &pos, &c));

    fs2_free(&s);
    assert_balanced();
}

/* pos in-place (out-only / newpos-only do not break the sequence) */
Test(fs2_api, pos_parse_char_chained_via_newpos) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hi");

    size_t np = 0;
    char c1 = 0, c2 = 0;

    cr_assert(fs2_pos_parse_char(&s, 0, &np, &c1));   /* np → 1 */
    cr_assert(fs2_pos_parse_char(&s, np, &np, &c2));  /* np → 2 */
    CHECK_EQ("c1", c1, 'h');
    CHECK_EQ("c2", c2, 'i');
    CHECK_EQ("np == len", np, 2);

    cr_assert_not(fs2_pos_parse_char(&s, np, &np, &c2));   /* beyond */

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_short — nominal
 * ========================================================================= */

Test(fs2_api, pos_parse_short_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42");
    size_t np = 999; short v = 0;
    cr_assert(fs2_pos_parse_short(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  (short)42);
    CHECK_EQ("newpos", np, 2);
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_short_negative) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("-42");
    size_t np = 999; short v = 0;
    cr_assert(fs2_pos_parse_short(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  (short)-42);
    CHECK_EQ("newpos", np, 3);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_short_zero) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("0");
    size_t np = 999; short v = 99;
    cr_assert(fs2_pos_parse_short(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  (short)0);
    CHECK_EQ("newpos", np, 1);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_short_leading_spaces_and_pos) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("   42xyz");
    size_t np = 999; short v = 0;
    cr_assert(fs2_pos_parse_short(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  (short)42);
    CHECK_EQ("newpos", np, 5);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_short_pos_mid_string) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc123");
    size_t np = 999; short v = 0;
    cr_assert(fs2_pos_parse_short(&s, 3, &np, &v));
    CHECK_EQ("value",  v,  (short)123);
    CHECK_EQ("newpos", np, 6);
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_short — SHRT boundaries
 * ========================================================================= */

Test(fs2_api, pos_parse_short_shrt_max) {
    __fs2_checkreset();
    char buf[16];
    snprintf(buf, sizeof buf, "%d", SHRT_MAX);
    fs2 s = fs2_fromstr(buf);

    size_t np = 999; short v = 0;
    cr_assert(fs2_pos_parse_short(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  SHRT_MAX);
    CHECK_EQ("newpos", np, strlen(buf));
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_short_shrt_min) {
    __fs2_checkreset();
    char buf[16];
    snprintf(buf, sizeof buf, "%d", SHRT_MIN);
    fs2 s = fs2_fromstr(buf);

    size_t np = 999; short v = 0;
    cr_assert(fs2_pos_parse_short(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  SHRT_MIN);
    CHECK_EQ("newpos", np, strlen(buf));
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_short_shrt_max_plus_one_false_no_write) {
    __fs2_checkreset();
    char buf[16];
    snprintf(buf, sizeof buf, "%lld", (long long)SHRT_MAX + 1);
    fs2 s = fs2_fromstr(buf);

    size_t np = 12345; short v = 999;
    cr_assert_not(fs2_pos_parse_short(&s, 0, &np, &v));
    CHECK_EQ("out untouched",    v,  (short)999);
    CHECK_EQ("newpos untouched", np, 12345);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_short_shrt_min_minus_one_false_no_write) {
    __fs2_checkreset();
    char buf[16];
    snprintf(buf, sizeof buf, "%lld", (long long)SHRT_MIN - 1);
    fs2 s = fs2_fromstr(buf);

    size_t np = 12345; short v = 999;
    cr_assert_not(fs2_pos_parse_short(&s, 0, &np, &v));
    CHECK_EQ("out untouched",    v,  (short)999);
    CHECK_EQ("newpos untouched", np, 12345);
    fs2_free(&s);
    assert_balanced();
}

/* LLONG_MAX — guaranteed outside short */
Test(fs2_api, pos_parse_short_llong_max_rejected_by_range) {
    __fs2_checkreset();
    char buf[64];
    snprintf(buf, sizeof buf, "%lld", LLONG_MAX);
    fs2 s = fs2_fromstr(buf);

    size_t np = 12345; short v = 999;
    cr_assert_not(fs2_pos_parse_short(&s, 0, &np, &v));
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_short — failures from delegation, out/newpos are NOT written
 * ========================================================================= */

Test(fs2_api, pos_parse_short_invalid_false_no_write) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");
    size_t np = 12345; short v = 999;
    cr_assert_not(fs2_pos_parse_short(&s, 0, &np, &v));
    CHECK_EQ("out untouched",    v,  (short)999);
    CHECK_EQ("newpos untouched", np, 12345);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_short_empty_fs2empty_false) {
    __fs2_checkreset();
    fs2 s = FS2EMPTY;
    cr_assert_not(fs2_pos_parse_short(&s, 0, NULL, NULL));
    assert_balanced();
}

Test(fs2_api, pos_parse_short_canon_v_null_false) {
    __fs2_checkreset();
    fs2 s = FS2();
    cr_assert_not(fs2_pos_parse_short(&s, 0, NULL, NULL));
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_short_null_s_false) {
    cr_assert_not(fs2_pos_parse_short(NULL, 0, NULL, NULL));
}

Test(fs2_api, pos_parse_short_llong_overflow_false) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("99999999999999999999999999");   /* > LLONG_MAX */
    cr_assert_not(fs2_pos_parse_short(&s, 0, NULL, NULL));
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_short — optional out / newpos
 * ========================================================================= */

Test(fs2_api, pos_parse_short_out_null) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42");
    size_t np = 999;
    cr_assert(fs2_pos_parse_short(&s, 0, &np, NULL));
    CHECK_EQ("newpos", np, 2);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_short_newpos_null) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42");
    short v = 0;
    cr_assert(fs2_pos_parse_short(&s, 0, NULL, &v));
    CHECK_EQ("value", v, (short)42);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_short_both_null) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("-42");
    cr_assert(fs2_pos_parse_short(&s, 0, NULL, NULL));
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_short — delegation ≡ _llong for small values
 * ========================================================================= */

Test(fs2_api, pos_parse_short_delegates_to_llong) {
    __fs2_checkreset();
    static const char *inputs[] = {
        "0", "42", "-42", "+7", "  123", "1x", "abc", NULL
    };
    for (size_t i = 0; inputs[i]; i++) {
        fs2 s = fs2_fromstr(inputs[i]);
        size_t np_sh = 0, np_ll = 0;
        short     v_sh = 0;
        long long v_ll = 0;

        bool ok_sh = fs2_pos_parse_short(&s, 0, &np_sh, &v_sh);
        bool ok_ll = fs2_pos_parse_llong(&s, 0, &np_ll, &v_ll);

        cr_assert_eq(ok_sh, ok_ll, "divergence on '%s'", inputs[i]);
        if (ok_sh) {
            CHECK_EQ("value",  (long long)v_sh, v_ll);
            CHECK_EQ("newpos", np_sh, np_ll);
        }
        fs2_free(&s);
    }
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_ushort — nominal
 * ========================================================================= */

Test(fs2_api, pos_parse_ushort_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42");
    size_t np = 999; unsigned short v = 0;
    cr_assert(fs2_pos_parse_ushort(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  (unsigned short)42);
    CHECK_EQ("newpos", np, 2);
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ushort_plus_sign) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("+42");
    size_t np = 999; unsigned short v = 0;
    cr_assert(fs2_pos_parse_ushort(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  (unsigned short)42);
    CHECK_EQ("newpos", np, 3);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ushort_zero) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("0");
    size_t np = 999; unsigned short v = 99;
    cr_assert(fs2_pos_parse_ushort(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  (unsigned short)0);
    CHECK_EQ("newpos", np, 1);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ushort_pos_mid_string) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc123");
    size_t np = 999; unsigned short v = 0;
    cr_assert(fs2_pos_parse_ushort(&s, 3, &np, &v));
    CHECK_EQ("value",  v,  (unsigned short)123);
    CHECK_EQ("newpos", np, 6);
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_ushort — reject "-" (inherited via _ullong)
 * ========================================================================= */

Test(fs2_api, pos_parse_ushort_rejects_negative) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("-42");
    size_t np = 12345; unsigned short v = 999;
    cr_assert_not(fs2_pos_parse_ushort(&s, 0, &np, &v));
    CHECK_EQ("out untouched",    v,  (unsigned short)999);
    CHECK_EQ("newpos untouched", np, 12345);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ushort_rejects_negative_one) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("-1");
    size_t np = 0; unsigned short v = 0;
    cr_assert_not(fs2_pos_parse_ushort(&s, 0, &np, &v));
    CHECK_NEQ("not USHRT_MAX", (unsigned)v, (unsigned)USHRT_MAX);
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_ushort — USHRT boundaries
 * ========================================================================= */

Test(fs2_api, pos_parse_ushort_ushrt_max) {
    __fs2_checkreset();
    char buf[16];
    snprintf(buf, sizeof buf, "%u", (unsigned)USHRT_MAX);
    fs2 s = fs2_fromstr(buf);

    size_t np = 999; unsigned short v = 0;
    cr_assert(fs2_pos_parse_ushort(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  USHRT_MAX);
    CHECK_EQ("newpos", np, strlen(buf));
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ushort_ushrt_max_plus_one_false_no_write) {
    __fs2_checkreset();
    char buf[32];
    snprintf(buf, sizeof buf, "%llu", (unsigned long long)USHRT_MAX + 1ULL);
    fs2 s = fs2_fromstr(buf);

    size_t np = 12345; unsigned short v = 999;
    cr_assert_not(fs2_pos_parse_ushort(&s, 0, &np, &v));
    CHECK_EQ("out untouched",    v,  (unsigned short)999);
    CHECK_EQ("newpos untouched", np, 12345);
    fs2_free(&s);
    assert_balanced();
}

/* ULLONG_MAX — guaranteed outside ushort */
Test(fs2_api, pos_parse_ushort_ullong_max_rejected_by_range) {
    __fs2_checkreset();
    char buf[64];
    snprintf(buf, sizeof buf, "%llu", ULLONG_MAX);
    fs2 s = fs2_fromstr(buf);

    size_t np = 12345; unsigned short v = 999;
    cr_assert_not(fs2_pos_parse_ushort(&s, 0, &np, &v));
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_ushort — failures from delegation, out/newpos are NOT written
 * ========================================================================= */

Test(fs2_api, pos_parse_ushort_invalid_false_no_write) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");
    size_t np = 12345; unsigned short v = 999;
    cr_assert_not(fs2_pos_parse_ushort(&s, 0, &np, &v));
    CHECK_EQ("out untouched",    v,  (unsigned short)999);
    CHECK_EQ("newpos untouched", np, 12345);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ushort_empty_fs2empty_false) {
    __fs2_checkreset();
    fs2 s = FS2EMPTY;
    cr_assert_not(fs2_pos_parse_ushort(&s, 0, NULL, NULL));
    assert_balanced();
}

Test(fs2_api, pos_parse_ushort_canon_v_null_false) {
    __fs2_checkreset();
    fs2 s = FS2();
    cr_assert_not(fs2_pos_parse_ushort(&s, 0, NULL, NULL));
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ushort_null_s_false) {
    cr_assert_not(fs2_pos_parse_ushort(NULL, 0, NULL, NULL));
}

Test(fs2_api, pos_parse_ushort_ullong_overflow_false) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("18446744073709551616");   /* 2^64 */
    cr_assert_not(fs2_pos_parse_ushort(&s, 0, NULL, NULL));
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_ushort — optional out / newpos
 * ========================================================================= */

Test(fs2_api, pos_parse_ushort_out_null) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42");
    size_t np = 999;
    cr_assert(fs2_pos_parse_ushort(&s, 0, &np, NULL));
    CHECK_EQ("newpos", np, 2);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ushort_newpos_null) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42");
    unsigned short v = 0;
    cr_assert(fs2_pos_parse_ushort(&s, 0, NULL, &v));
    CHECK_EQ("value", v, (unsigned short)42);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ushort_both_null) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42");
    cr_assert(fs2_pos_parse_ushort(&s, 0, NULL, NULL));
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_ushort — delegation ≡ _ullong for small values
 * ========================================================================= */

Test(fs2_api, pos_parse_ushort_delegates_to_ullong) {
    __fs2_checkreset();
    static const char *inputs[] = {
        "0", "42", "+7", "  123", "1x", "abc", NULL
    };
    for (size_t i = 0; inputs[i]; i++) {
        fs2 s = fs2_fromstr(inputs[i]);
        size_t np_us = 0, np_ull = 0;
        unsigned short     v_us  = 0;
        unsigned long long v_ull = 0;

        bool ok_us  = fs2_pos_parse_ushort(&s, 0, &np_us,  &v_us);
        bool ok_ull = fs2_pos_parse_ullong(&s, 0, &np_ull, &v_ull);

        cr_assert_eq(ok_us, ok_ull, "divergence on '%s'", inputs[i]);
        if (ok_us) {
            CHECK_EQ("value",  (unsigned long long)v_us, v_ull);
            CHECK_EQ("newpos", np_us, np_ull);
        }
        fs2_free(&s);
    }
    assert_balanced();
}

/* =========================================================================
 * STATIC / LOCAL — common for both
 * ========================================================================= */

Test(fs2_api, pos_parse_short_static_src) {
    __fs2_checkreset();
    fs2 s = FS2LITERAL("  42");
    size_t np = 999; short v = 0;
    cr_assert(fs2_pos_parse_short(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  (short)42);
    CHECK_EQ("newpos", np, 4);
    CHECK_FS2VALID(1, &s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ushort_static_src) {
    __fs2_checkreset();
    fs2 s = FS2LITERAL("  42");
    size_t np = 999; unsigned short v = 0;
    cr_assert(fs2_pos_parse_ushort(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  (unsigned short)42);
    CHECK_EQ("newpos", np, 4);
    CHECK_FS2VALID(1, &s);
    assert_balanced();
}

Test(fs2_api, pos_parse_short_local_src) {
    __fs2_checkreset();
    FS2LOCAL(buf, 16);
    fs2_memcpy_str(&buf, 0, "123", 3);
    fs2_setlen(&buf, 3);

    size_t np = 999; short v = 0;
    cr_assert(fs2_pos_parse_short(&buf, 0, &np, &v));
    CHECK_EQ("value",  v,  (short)123);
    CHECK_EQ("newpos", np, 3);
    CHECK_FS2VALID(1, &buf);
    assert_balanced();
}

Test(fs2_api, pos_parse_ushort_local_src) {
    __fs2_checkreset();
    FS2LOCAL(buf, 16);
    fs2_memcpy_str(&buf, 0, "123", 3);
    fs2_setlen(&buf, 3);

    size_t np = 999; unsigned short v = 0;
    cr_assert(fs2_pos_parse_ushort(&buf, 0, &np, &v));
    CHECK_EQ("value",  v,  (unsigned short)123);
    CHECK_EQ("newpos", np, 3);
    CHECK_FS2VALID(1, &buf);
    assert_balanced();
}

/* =========================================================================
 * tryparse_* — strict vs non-strict (numeric types)
 * ========================================================================= */

Test(fs2_api, tryparse_llong) {
    __fs2_checkreset();
    long long v = 0;

    fs2 trail = fs2_fromstr("42abc");
    cr_assert( fs2_tryparse_llong(&trail, &v, false), "nonstrict: trailing allowed");
    CHECK_EQ("value", v, 42LL);
    cr_assert_not(fs2_tryparse_llong(&trail, &v, true), "strict: trailing → false");

    fs2 clean = fs2_fromstr("-42");
    cr_assert(fs2_tryparse_llong(&clean, &v, true), "strict: clean input");
    CHECK_EQ("value", v, -42LL);

    fs2_free(&trail);
    fs2_free(&clean);
    assert_balanced();
}

Test(fs2_api, tryparse_ullong) {
    __fs2_checkreset();
    unsigned long long v = 0;

    fs2 trail = fs2_fromstr("42abc");
    cr_assert( fs2_tryparse_ullong(&trail, &v, false));
    CHECK_EQ("value", v, 42ULL);
    cr_assert_not(fs2_tryparse_ullong(&trail, &v, true));

    fs2 clean = fs2_fromstr("42");
    cr_assert(fs2_tryparse_ullong(&clean, &v, true));
    CHECK_EQ("value", v, 42ULL);

    fs2_free(&trail);
    fs2_free(&clean);
    assert_balanced();
}

Test(fs2_api, tryparse_long) {
    __fs2_checkreset();
    long v = 0;

    fs2 trail = fs2_fromstr("  42xyz");
    cr_assert( fs2_tryparse_long(&trail, &v, false));
    CHECK_EQ("value", v, 42L);
    cr_assert_not(fs2_tryparse_long(&trail, &v, true));

    fs2 clean = fs2_fromstr("  -42");
    cr_assert(fs2_tryparse_long(&clean, &v, true));
    CHECK_EQ("value", v, -42L);

    fs2_free(&trail);
    fs2_free(&clean);
    assert_balanced();
}

Test(fs2_api, tryparse_ulong) {
    __fs2_checkreset();
    unsigned long v = 0;

    fs2 trail = fs2_fromstr("42abc");
    cr_assert( fs2_tryparse_ulong(&trail, &v, false));
    CHECK_EQ("value", v, 42UL);
    cr_assert_not(fs2_tryparse_ulong(&trail, &v, true));

    fs2 clean = fs2_fromstr("+42");
    cr_assert(fs2_tryparse_ulong(&clean, &v, true));
    CHECK_EQ("value", v, 42UL);

    fs2_free(&trail);
    fs2_free(&clean);
    assert_balanced();
}

Test(fs2_api, tryparse_int) {
    __fs2_checkreset();
    int v = 0;

    fs2 trail = fs2_fromstr("42xyz");
    cr_assert( fs2_tryparse_int(&trail, &v, false));
    CHECK_EQ("value", v, 42);
    cr_assert_not(fs2_tryparse_int(&trail, &v, true));

    fs2 clean = fs2_fromstr("-42");
    cr_assert(fs2_tryparse_int(&clean, &v, true));
    CHECK_EQ("value", v, -42);

    fs2_free(&trail);
    fs2_free(&clean);
    assert_balanced();
}

Test(fs2_api, tryparse_uint) {
    __fs2_checkreset();
    unsigned v = 0;

    fs2 trail = fs2_fromstr("42abc");
    cr_assert( fs2_tryparse_uint(&trail, &v, false));
    CHECK_EQ("value", v, 42u);
    cr_assert_not(fs2_tryparse_uint(&trail, &v, true));

    fs2 clean = fs2_fromstr("42");
    cr_assert(fs2_tryparse_uint(&clean, &v, true));
    CHECK_EQ("value", v, 42u);

    fs2_free(&trail);
    fs2_free(&clean);
    assert_balanced();
}

Test(fs2_api, tryparse_double) {
    __fs2_checkreset();
    double v = 0;

    fs2 trail = fs2_fromstr("1.5xyz");
    cr_assert( fs2_tryparse_double(&trail, &v, false));
    CHECK_EQ("value", v, 1.5);
    cr_assert_not(fs2_tryparse_double(&trail, &v, true));

    fs2 clean = fs2_fromstr("-2.5");
    cr_assert(fs2_tryparse_double(&clean, &v, true));
    CHECK_EQ("value", v, -2.5);

    fs2_free(&trail);
    fs2_free(&clean);
    assert_balanced();
}

Test(fs2_api, tryparse_short) {
    __fs2_checkreset();
    short v = 0;

    fs2 trail = fs2_fromstr("42abc");
    cr_assert( fs2_tryparse_short(&trail, &v, false));
    CHECK_EQ("value", v, (short)42);
    cr_assert_not(fs2_tryparse_short(&trail, &v, true));

    fs2 clean = fs2_fromstr("-42");
    cr_assert(fs2_tryparse_short(&clean, &v, true));
    CHECK_EQ("value", v, (short)-42);

    fs2_free(&trail);
    fs2_free(&clean);
    assert_balanced();
}

Test(fs2_api, tryparse_ushort) {
    __fs2_checkreset();
    unsigned short v = 0;

    fs2 trail = fs2_fromstr("42abc");
    cr_assert( fs2_tryparse_ushort(&trail, &v, false));
    CHECK_EQ("value", v, (unsigned short)42);
    cr_assert_not(fs2_tryparse_ushort(&trail, &v, true));

    fs2 clean = fs2_fromstr("+42");
    cr_assert(fs2_tryparse_ushort(&clean, &v, true));
    CHECK_EQ("value", v, (unsigned short)42);

    fs2_free(&trail);
    fs2_free(&clean);
    assert_balanced();
}

/* =========================================================================
 * tryparse_char — SPECIAL strict semantics
 *   strict=true ONLY rejects leading whitespace (isspace(s->v[0]));
 *   it does NOT require newpos == s->len, so trailing bytes are allowed.
 * ========================================================================= */

Test(fs2_api, tryparse_char_strict_rejects_leading_whitespace_only) {
    __fs2_checkreset();
    char c = 0;

    /* strict: leading space → false */
    fs2 sp = fs2_fromstr(" a");
    cr_assert_not(fs2_tryparse_char(&sp, &c, true), "leading space, strict");

    /* non-strict: space is a regular char → true, c = ' ' */
    cr_assert(fs2_tryparse_char(&sp, &c, false), "leading space, non-strict");
    CHECK_EQ("char", c, ' ');

    /* strict: leading tab → false */
    fs2 tab = fs2_fromstr("\ta");
    cr_assert_not(fs2_tryparse_char(&tab, &c, true), "leading tab, strict");

    fs2_free(&sp);
    fs2_free(&tab);
    assert_balanced();
}

Test(fs2_api, tryparse_char_strict_accepts_trailing_garbage) {
    __fs2_checkreset();
    char c = 0;

    /* "ab" strict → true: leading 'a' is not whitespace; trailing 'b' is NOT checked */
    fs2 ab = fs2_fromstr("ab");
    cr_assert(fs2_tryparse_char(&ab, &c, true), "strict accepts 'ab'");
    CHECK_EQ("char", c, 'a');

    /* "a xyz" strict → true (leading 'a' is not whitespace) */
    fs2 ag = fs2_fromstr("a xyz");
    cr_assert(fs2_tryparse_char(&ag, &c, true));
    CHECK_EQ("char", c, 'a');

    /* "a " strict → true (trailing space is NOT checked) */
    fs2 at = fs2_fromstr("a ");
    cr_assert(fs2_tryparse_char(&at, &c, true));
    CHECK_EQ("char", c, 'a');

    fs2_free(&ab);
    fs2_free(&ag);
    fs2_free(&at);
    assert_balanced();
}

Test(fs2_api, tryparse_char_nominal) {
    __fs2_checkreset();
    char c = 0;

    fs2 s = fs2_fromstr("x");
    cr_assert(fs2_tryparse_char(&s, &c, true));
    CHECK_EQ("char", c, 'x');

    /* empty string → false (delegated to pos_parse_char) */
    fs2 e = FS2EMPTY;
    cr_assert_not(fs2_tryparse_char(&e, &c, false));
    cr_assert_not(fs2_tryparse_char(&e, &c, true));

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * NULL s — delegates a soft false (no crash) for every type
 * ========================================================================= */

Test(fs2_api, tryparse_all_null_s_returns_false) {
    long long ll = 0; unsigned long long ull = 0;
    long l = 0; unsigned long ul = 0;
    int i = 0; unsigned u = 0;
    double d = 0; char c = 0;
    short sh = 0; unsigned short ush = 0;

    cr_assert_not(fs2_tryparse_llong (NULL, &ll,  false));
    cr_assert_not(fs2_tryparse_ullong(NULL, &ull, false));
    cr_assert_not(fs2_tryparse_long  (NULL, &l,   false));
    cr_assert_not(fs2_tryparse_ulong (NULL, &ul,  false));
    cr_assert_not(fs2_tryparse_int   (NULL, &i,   false));
    cr_assert_not(fs2_tryparse_uint  (NULL, &u,   false));
    cr_assert_not(fs2_tryparse_double(NULL, &d,   false));
    cr_assert_not(fs2_tryparse_char  (NULL, &c,   false));
    cr_assert_not(fs2_tryparse_short (NULL, &sh,  false));
    cr_assert_not(fs2_tryparse_ushort(NULL, &ush, false));

    /* strict=true — same soft false */
    cr_assert_not(fs2_tryparse_llong (NULL, &ll,  true));
    cr_assert_not(fs2_tryparse_char  (NULL, &c,   true));
}

/* =========================================================================
 * parse_* — happy path + non-strict trailing
 *   Failure (raise) path is framework-specific (userraise may longjmp);
 *   underlying failure logic is covered by pos_parse_ tryparse_* tests.
 * ========================================================================= */

Test(fs2_api, parse_llong_happy_and_trailing) {
    __fs2_checkreset();
    fs2 clean = fs2_fromstr("-42");
    fs2 trail = fs2_fromstr("42abc");   /* non-strict: trailing tolerated */

    CHECK_EQ("clean",    fs2_parse_llong(&clean), -42LL);
    CHECK_EQ("trailing", fs2_parse_llong(&trail),  42LL);

    fs2_free(&clean);
    fs2_free(&trail);
    assert_balanced();
}

Test(fs2_api, parse_ullong_happy_and_trailing) {
    __fs2_checkreset();
    fs2 clean = fs2_fromstr("42");
    fs2 trail = fs2_fromstr("42abc");

    CHECK_EQ("clean",    fs2_parse_ullong(&clean), 42ULL);
    CHECK_EQ("trailing", fs2_parse_ullong(&trail), 42ULL);

    fs2_free(&clean);
    fs2_free(&trail);
    assert_balanced();
}

Test(fs2_api, parse_long_happy_and_trailing) {
    __fs2_checkreset();
    fs2 clean = fs2_fromstr("  -42");
    fs2 trail = fs2_fromstr("  42xyz");

    CHECK_EQ("clean",    fs2_parse_long(&clean), -42L);
    CHECK_EQ("trailing", fs2_parse_long(&trail),  42L);

    fs2_free(&clean);
    fs2_free(&trail);
    assert_balanced();
}

Test(fs2_api, parse_ulong_happy_and_trailing) {
    __fs2_checkreset();
    fs2 clean = fs2_fromstr("+42");
    fs2 trail = fs2_fromstr("42abc");

    CHECK_EQ("clean",    fs2_parse_ulong(&clean), 42UL);
    CHECK_EQ("trailing", fs2_parse_ulong(&trail), 42UL);

    fs2_free(&clean);
    fs2_free(&trail);
    assert_balanced();
}

Test(fs2_api, parse_int_happy_and_trailing) {
    __fs2_checkreset();
    fs2 clean = fs2_fromstr("-42");
    fs2 trail = fs2_fromstr("42xyz");

    CHECK_EQ("clean",    fs2_parse_int(&clean), -42);
    CHECK_EQ("trailing", fs2_parse_int(&trail),  42);

    fs2_free(&clean);
    fs2_free(&trail);
    assert_balanced();
}

Test(fs2_api, parse_uint_happy_and_trailing) {
    __fs2_checkreset();
    fs2 clean = fs2_fromstr("42");
    fs2 trail = fs2_fromstr("42abc");

    CHECK_EQ("clean",    fs2_parse_uint(&clean), 42u);
    CHECK_EQ("trailing", fs2_parse_uint(&trail), 42u);

    fs2_free(&clean);
    fs2_free(&trail);
    assert_balanced();
}

Test(fs2_api, parse_double_happy_and_trailing) {
    __fs2_checkreset();
    fs2 clean = fs2_fromstr("-2.5");
    fs2 trail = fs2_fromstr("1.5xyz");

    CHECK_EQ("clean",    fs2_parse_double(&clean), -2.5);
    CHECK_EQ("trailing", fs2_parse_double(&trail),  1.5);

    fs2_free(&clean);
    fs2_free(&trail);
    assert_balanced();
}

Test(fs2_api, parse_short_happy_and_trailing) {
    __fs2_checkreset();
    fs2 clean = fs2_fromstr("-42");
    fs2 trail = fs2_fromstr("42abc");

    CHECK_EQ("clean",    fs2_parse_short(&clean), (short)-42);
    CHECK_EQ("trailing", fs2_parse_short(&trail), (short)42);

    fs2_free(&clean);
    fs2_free(&trail);
    assert_balanced();
}

Test(fs2_api, parse_ushort_happy_and_trailing) {
    __fs2_checkreset();
    fs2 clean = fs2_fromstr("+42");
    fs2 trail = fs2_fromstr("42abc");

    CHECK_EQ("clean",    fs2_parse_ushort(&clean), (unsigned short)42);
    CHECK_EQ("trailing", fs2_parse_ushort(&trail), (unsigned short)42);

    fs2_free(&clean);
    fs2_free(&trail);
    assert_balanced();
}

/* =========================================================================
 * parse_char — special: reads s->v[0], non-strict = no whitespace check
 * ========================================================================= */

Test(fs2_api, parse_char_first_byte_nonstrict) {
    __fs2_checkreset();

    /* Single char */
    fs2 single = fs2_fromstr("x");
    CHECK_EQ("single", fs2_parse_char(&single), 'x');

    /* Non-strict: trailing chars are ignored, only s->v[0] is returned */
    fs2 multi = fs2_fromstr("ab");
    CHECK_EQ("multi", fs2_parse_char(&multi), 'a');

    /* Non-strict: leading space is NOT rejected (unlike strict=true) */
    fs2 sp = fs2_fromstr(" y");
    CHECK_EQ("space", fs2_parse_char(&sp), ' ');

    /* Trailing space after first char is irrelevant */
    fs2 trail_sp = fs2_fromstr("a ");
    CHECK_EQ("trail_space", fs2_parse_char(&trail_sp), 'a');

    fs2_free(&single);
    fs2_free(&multi);
    fs2_free(&sp);
    fs2_free(&trail_sp);
    assert_balanced();
}

/* =========================================================================
 * parse_* — STATIC source (checks that const path works)
 * ========================================================================= */

Test(fs2_api, parse_static_src) {
    __fs2_checkreset();
    fs2 s = FS2LITERAL("123");

    CHECK_EQ("llong",  fs2_parse_llong (&s), 123LL);
    CHECK_EQ("int",    fs2_parse_int   (&s), 123);
    CHECK_EQ("double", fs2_parse_double(&s), 123.0);
    CHECK_EQ("short",  fs2_parse_short (&s), (short)123);

    CHECK_FS2VALID(1, &s);
    assert_balanced();
}

/* =========================================================================
 * userraise behaviour for fs2_parse_*
 *
 * Flow: fs2_parse_* -> fs2_tryparse_* (returns false on failure)
 *                    -> userraise(ERR_UNABLE_PARSE_DATA, ...)
 *                    -> err_raise:
 *                         - no active TRY() -> raise(SIGINT)
 *                         - active TRY()    -> siglongjmp to innermost else
 * ========================================================================= */

/* No active TRY(): userraise hits the last-resort SIGINT so a debugger
 * (or core dump) can inspect the process. */
Test(fs2_api, parse_int_outside_try_raises_sigint, .signal = SIGINT) {
    fs2 s = fs2_fromstr("not a number");
    (void) fs2_parse_int(&s);   /* must raise -> SIGINT terminates the process */
    /* unreachable */
}

/* Inside TRY(): userraise longjmps to the else branch; the error stack
 * gets exactly one ERR_USER / ERR_UNABLE_PARSE_DATA record. */
Test(fs2_api, parse_int_inside_try_caught_by_else) {
    __fs2_checkreset();
    err_clean(true);
    err_resetenv();

    fs2 s = fs2_fromstr("not a number");
    bool caught = false;

    TRY() {
        (void) fs2_parse_int(&s);
        cr_assert_fail("expected userraise; got normal return");
    } else {
        caught = true;
    }

    cr_assert(caught, "TRY() else-branch should have been reached");
    cr_assert_eq(err_count(), 1, "one error record expected");

    ErrorInfo info = {0};
    cr_assert(err_last(&info), "err_last should succeed");
    cr_assert_eq(info.type, ERR_USER);
    cr_assert_eq(info.code, ERR_UNABLE_PARSE_DATA);

    err_clean(true);
    err_resetenv();
    fs2_free(&s);
    assert_balanced();
}


/* Inside TRY(): empty FS2EMPTY -> pos_parse_llong hits the
 * "Null input or pos out of length" guard (pos=0 >= len=0),
 * which returns false, then userraise longjmps to the else branch. */
Test(fs2_api, parse_llong_empty_inside_try_caught_by_else) {
    __fs2_checkreset();
    err_clean(true);
    err_resetenv();

    fs2 s = FS2EMPTY;
    bool caught = false;

    TRY() {
        (void) fs2_parse_llong(&s);
        cr_assert_fail("expected userraise on empty string; got normal return");
    } else {
        caught = true;
    }

    cr_assert(caught, "TRY() else-branch should have been reached");
    cr_assert_eq(err_count(), 1, "one error record expected");

    ErrorInfo info = {0};
    cr_assert(err_last(&info), "err_last should succeed");
    cr_assert_eq(info.type, ERR_USER);
    cr_assert_eq(info.code, ERR_UNABLE_PARSE_DATA);

    err_clean(true);
    err_resetenv();
    assert_balanced();
}

/* =========================================================================
 * fs2_spn — nominal
 * ========================================================================= */

Test(fs2_api, spn_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("aaabbbccc");

    CHECK_EQ("aaa",   fs2_spn(&s, 0, "a"),  3);
    CHECK_EQ("bb",    fs2_spn(&s, 3, "b"),  3);
    CHECK_EQ("ccc",   fs2_spn(&s, 6, "c"),  3);
    CHECK_EQ("empty accept", fs2_spn(&s, 0, ""),  0);
    CHECK_EQ("no match",     fs2_spn(&s, 0, "xyz"), 0);

    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, spn_from_pos) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("aaabbbccc");

    CHECK_EQ("from 0",  fs2_spn(&s, 0, "a"),  3);
    CHECK_EQ("from 1",  fs2_spn(&s, 1, "a"),  2);
    CHECK_EQ("from 2",  fs2_spn(&s, 2, "a"),  1);
    CHECK_EQ("from 3",  fs2_spn(&s, 3, "a"),  0);   /* 'b' not in accept */

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_spn — FS2() (v==NULL) — explicit guard test
 * ========================================================================= */

Test(fs2_api, spn_canon_v_null_returns_zero) {
    __fs2_checkreset();
    fs2 s = FS2();
    cr_assert_null(s.v, "precondition: FS2() has v == NULL");

    /* pos=0 — would have reached strspn(NULL + 0, ...) without the guard */
    CHECK_EQ("pos=0",       fs2_spn(&s, 0, "abc"),  0);
    CHECK_EQ("pos=5",       fs2_spn(&s, 5, "abc"),  0);
    CHECK_EQ("pos=SIZE_MAX", fs2_spn(&s, SIZE_MAX, "abc"), 0);
    CHECK_EQ("empty accept", fs2_spn(&s, 0, ""),  0);

    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, spn_fs2empty_v_not_null_returns_zero) {
    __fs2_checkreset();
    fs2 s = FS2EMPTY;
    cr_assert_not_null(s.v, "precondition: FS2EMPTY has v == \"\"");

    CHECK_EQ("pos=0", fs2_spn(&s, 0, "abc"), 0);
    CHECK_EQ("pos=5", fs2_spn(&s, 5, "abc"), 0);

    assert_balanced();
}

/* =========================================================================
 * fs2_spn — guards
 * ========================================================================= */

Test(fs2_api, spn_null_s_aborts, .signal = SIGABRT) {
    fs2_spn(NULL, 0, "abc");
}

Test(fs2_api, spn_null_accept_aborts, .signal = SIGABRT) {
    fs2 s = fs2_fromstr("abc");
    fs2_spn(&s, 0, NULL);
    fs2_free(&s);
}

/* =========================================================================
 * fs2_spn — static / local
 * ========================================================================= */

Test(fs2_api, spn_static_src) {
    __fs2_checkreset();
    fs2 s = FS2LITERAL("aaabbb");

    CHECK_EQ("aaa", fs2_spn(&s, 0, "a"), 3);
    CHECK_EQ("bb",  fs2_spn(&s, 3, "b"), 3);

    CHECK_FS2VALID(1, &s);
    assert_balanced();
}

/* =========================================================================
 * fs2_cspn — nominal
 * ========================================================================= */

Test(fs2_api, cspn_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello, world!");

    CHECK_EQ("until comma",  fs2_cspn(&s, 0, ","),  5);   /* "hello" */
    CHECK_EQ("until space",  fs2_cspn(&s, 0, " "),  6);
    CHECK_EQ("until !",      fs2_cspn(&s, 0, "!"),  12);
    CHECK_EQ("empty reject", fs2_cspn(&s, 0, ""),  13);   /* == len */
    CHECK_EQ("immediate",    fs2_cspn(&s, 0, "h"),  0);    /* first char in reject */

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, cspn_from_pos) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("aaabbbс");

    CHECK_EQ("from 0, stop at b", fs2_cspn(&s, 0, "b"), 3);
    CHECK_EQ("from 3, stop at b", fs2_cspn(&s, 3, "b"), 0);
    CHECK_EQ("from 5, no b",      fs2_cspn(&s, 7, "b"), 1); 

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, cspn_from_pos_notfound) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("aaabbbс");

    CHECK_EQ("from 0, stop at b", fs2_cspn(&s, 0, "b"), 3);
    CHECK_EQ("from 3, stop at b", fs2_cspn(&s, 3, "b"), 0);
    CHECK_EQ("from 5, no b",      fs2_cspn(&s, 5, "b"), 0);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, cspn_from_pos_no_match_in_tail) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("aaabbb");
    CHECK_EQ("tail has no 'a'", fs2_cspn(&s, 3, "a"), 3);   /* "bbb" whole */
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_cspn — FS2() (v==NULL) — explicit guard test
 * ========================================================================= */

Test(fs2_api, cspn_canon_v_null_returns_zero) {
    __fs2_checkreset();
    fs2 s = FS2();
    cr_assert_null(s.v, "precondition: FS2() has v == NULL");

    CHECK_EQ("pos=0",  fs2_cspn(&s, 0, "abc"), 0);
    CHECK_EQ("pos=5",  fs2_cspn(&s, 5, "abc"), 0);
    CHECK_EQ("empty reject", fs2_cspn(&s, 0, ""), 0);

    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, cspn_fs2empty_v_not_null_returns_zero) {
    __fs2_checkreset();
    fs2 s = FS2EMPTY;

    CHECK_EQ("pos=0", fs2_cspn(&s, 0, "abc"), 0);
    CHECK_EQ("pos=5", fs2_cspn(&s, 5, "abc"), 0);

    assert_balanced();
}

/* =========================================================================
 * fs2_cspn — guards
 * ========================================================================= */

Test(fs2_api, cspn_null_s_aborts, .signal = SIGABRT) {
    fs2_cspn(NULL, 0, "abc");
}

Test(fs2_api, cspn_null_reject_aborts, .signal = SIGABRT) {
    fs2 s = fs2_fromstr("abc");
    fs2_cspn(&s, 0, NULL);
    fs2_free(&s);
}

Test(fs2_api, cspn_static_src) {
    __fs2_checkreset();
    fs2 s = FS2LITERAL("abc 123");

    CHECK_EQ("until space", fs2_cspn(&s, 0, " "), 3);

    CHECK_FS2VALID(1, &s);
    assert_balanced();
}

/* =========================================================================
 * Согласованность: fs2_nonspace_span == fs2_cspn(..., " \t\n\r\v\f")
 * ========================================================================= */

Test(fs2_api, nonspace_span_matches_cspn) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello world");

    CHECK_EQ("from 0", fs2_nonspace_span(&s, 0),
                       fs2_cspn(&s, 0, " \t\n\r\v\f"));
    CHECK_EQ("from 5", fs2_nonspace_span(&s, 5),
                       fs2_cspn(&s, 5, " \t\n\r\v\f"));

    fs2_free(&s);
    assert_balanced();
}