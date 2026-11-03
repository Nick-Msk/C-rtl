 #include <criterion/criterion.h>

#include "test_fs2.h"
#include "fs2_api.h"
#include <locale.h>
#include <limits.h>

TestSuite(test_fs2_api_parse, .init = test_setup, .fini = test_teardown);

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
    CHECK_EQ("newpos", np, 3);   /* все цифры */
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_llong — пробелы / trailing garbage
 * ========================================================================= */

Test(fs2_api, pos_parse_llong_leading_spaces) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("   42");
    size_t np = 999; long long v = 0;
    cr_assert(fs2_pos_parse_llong(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  42);
    CHECK_EQ("newpos", np, 5);   /* 3 пробела + 2 цифры */
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_llong_trailing_garbage) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42abc");
    size_t np = 999; long long v = 0;
    cr_assert(fs2_pos_parse_llong(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  42);
    CHECK_EQ("newpos", np, 2);   /* только «42» */
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_llong_trailing_spaces) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42   ");
    size_t np = 999; long long v = 0;
    cr_assert(fs2_pos_parse_llong(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  42);
    CHECK_EQ("newpos", np, 2);   /* trailing spaces не считаются */
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_llong_leading_spaces_and_trailing_garbage) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("  -42xyz");
    size_t np = 999; long long v = 0;
    cr_assert(fs2_pos_parse_llong(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  -42);
    CHECK_EQ("newpos", np, 5);   /* 2 пробела + '-' + 2 цифры */
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
    CHECK_EQ("newpos", np, 4);   /* 1 пробел + 2 цифры от pos=1 */
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
 * fs2_pos_parse_llong — граничные значения LLONG
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
 * fs2_pos_parse_llong — опциональные out / newpos
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
    cr_assert_not(fs2_pos_parse_llong(NULL, 0, &np, &v));   /* мягкий false, не abort */
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
 * fs2_pos_parse_llong — специальные форматы
 * ========================================================================= */

/* base 10: "0x10" → парсит "0", endptr на 'x' */
Test(fs2_api, pos_parse_llong_hex_prefix_parsed_as_zero) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("0x10");
    size_t np = 999; long long v = 123;
    cr_assert(fs2_pos_parse_llong(&s, 0, &np, &v));
    CHECK_EQ("value",  v,  0);     /* base 10 остановился на '0' */
    CHECK_EQ("newpos", np, 1);     /* съеден только '0' */
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

/* Высокий байт (>= 0x80) — не должен вызывать UB в isdigit */
Test(fs2_api, pos_parse_llong_high_byte_rejected_no_ub) {
    __fs2_checkreset();
    char *raw = strdup("\xE9" "42");   /* первый байт 0xE9 */
    cr_assert(raw != NULL);
    fs2 s = fs2_adopt(&raw);
    cr_assert(raw == NULL);

    /* 0xE9 не digit, не '+'/'-' → "Incorrect sym" */
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
 * Инвариант: newpos ВСЕГДА > pos на успехе
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