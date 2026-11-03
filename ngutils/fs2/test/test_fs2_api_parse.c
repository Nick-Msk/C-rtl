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

/* =========================================================================
 * fs2_pos_parse_long — nominal (делегирование + запись out/newpos)
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
    CHECK_EQ("newpos", np, 5);   /* 2 пробела + '-' + 2 цифры */
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
 * fs2_pos_parse_long — опциональные out / newpos
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
 * fs2_pos_parse_long — границы LONG (принимаются)
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
 * fs2_pos_parse_long — range-violation (активен только на LLP64 / ILP32)
 *   На LP64 (macOS/Linux) long == long long → ветка мертва, тесты пропускаем.
 * ========================================================================= */

#if LONG_MAX < LLONG_MAX
Test(fs2_api, pos_parse_long_above_long_max_returns_false) {
    __fs2_checkreset();
    /* LLONG_MAX гарантированно > LONG_MAX при этом #if */
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
 * fs2_pos_parse_long — failures: out/newpos НЕ трогаются
 *   (fail-path из делегирования _llong)
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
    cr_assert_not(fs2_pos_parse_long(&s, 0, &np, &v));   /* ERANGE в _llong */
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
    cr_assert_not(fs2_pos_parse_long(NULL, 0, &np, &v));   /* мягкий false */
    CHECK_EQ("out untouched",    v,  999L);
    CHECK_EQ("newpos untouched", np, 12345);
}

/* =========================================================================
 * Делегирование: long и llong дают одинаковый результат на одном входе
 * (LP64-путь — range check пропускает всё, что llong уже пропустил)
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
 * STATIC / LOCAL — ведут себя так же
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
    CHECK_EQ("newpos", np, 5);   /* 2 пробела + '-' + 2 цифры */
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
 * fs2_pos_parse_int — опциональные out / newpos
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
 * fs2_pos_parse_int — границы INT (принимаются)
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

/* INT_MAX + 1 — уже за пределами int, но в пределах long long */
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

/* LLONG_MAX (гарантированно вне int) — rejected via range-check, не через ERANGE */
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
 * fs2_pos_parse_int — failures: out/newpos НЕ трогаются
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

/* ERANGE в strtoll (не range-check int, а вообще за пределами llong) */
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
 * Делегирование + эквивалентность _llong для малых значений
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
 * fs2_pos_parse_ullong — отказ по `-` (главное отличие от strtoull)
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

/* -0 → тоже reject: код не делает исключения для нуля */
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

/* -1 не превращается в ULLONG_MAX (как было бы с голым strtoull) */
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
 * fs2_pos_parse_ullong — границы ULLONG
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
    /* 2^64 — гарантированно > ULLONG_MAX на любой платформе */
    fs2 s = fs2_fromstr("18446744073709551616");
    size_t np = 12345; unsigned long long v = 999;
    cr_assert_not(fs2_pos_parse_ullong(&s, 0, &np, &v));   /* ERANGE */
    CHECK_EQ("out untouched",    v,  999ULL);
    CHECK_EQ("newpos untouched", np, 12345);
    fs2_free(&s);
    assert_balanced();
}

/* 2^63 = LLONG_MAX + 1 — не влезает в long long, но в unsigned long long помещается */
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

/* Контраст: _llong на 2^63 → false (ERANGE), _ullong → true. Семантика unsigned. */
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

/* LLONG_MAX — влезает и в long long, и в unsigned long long */
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
 * fs2_pos_parse_ullong — опциональные out / newpos
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
 * fs2_pos_parse_ulong — наследуемый reject "-"
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
    CHECK_NEQ("not ULONG_MAX", v, ULONG_MAX);   /* контраст с голым strtoul */
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_ulong — границы ULONG (принимаются)
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

/* 2^63 = LLONG_MAX+1 — влезает и в unsigned long long, и в unsigned long на LP64 */
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
 * fs2_pos_parse_ulong — range-violation (только LLP64 / ILP32)
 * ========================================================================= */

#if ULONG_MAX < ULLONG_MAX
Test(fs2_api, pos_parse_ulong_above_ulong_max_false_no_write) {
    __fs2_checkreset();
    /* ULLONG_MAX заведомо > ULONG_MAX при этом #if */
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
 * fs2_pos_parse_ulong — failures из делегирования, out/newpos НЕ пишутся
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

/* ERANGE в strtoull — не range-check ulong, а вообще за пределами ullong */
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
 * fs2_pos_parse_ulong — опциональные out / newpos
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
 * Делегирование: _ulong ≡ _ullong на малых значениях
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
    CHECK_EQ("newpos", np, 6);   /* 3 пробела + '+' + 2 цифры */
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
 * fs2_pos_parse_uint — reject "-" (наследуется через _ulong/_ullong)
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
    CHECK_NEQ("not UINT_MAX", v, UINT_MAX);   /* контраст с голым strtoul */
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_uint — границы UINT (принимаются)
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
 * fs2_pos_parse_uint — UINT_MAX + 1 → false (все платформы)
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
 * fs2_pos_parse_uint — АКТИВНЫЙ range-check в _uint
 *   Только когда ULONG_MAX > UINT_MAX (LP64): значение > UINT_MAX,
 *   но влезает в ulong → _ulong успешно парсит, _uint отвергает по range.
 *   На LLP64 (ULONG_MAX == UINT_MAX) _ulong отвергнет раньше → мёртв.
 * ========================================================================= */

#if ULONG_MAX > UINT_MAX
Test(fs2_api, pos_parse_uint_range_check_active_on_lp64) {
    __fs2_checkreset();
    /* значение влезает в ulong (LP64: 64-bit), но > UINT_MAX */
    char buf[32];
    snprintf(buf, sizeof buf, "%llu", (unsigned long long)UINT_MAX + 1ULL);
    fs2 s = fs2_fromstr(buf);

    /* _ulong примет успешно */
    unsigned long ul = 0;
    size_t np_ul = 0;
    cr_assert(fs2_pos_parse_ulong(&s, 0, &np_ul, &ul),
              "precondition: _ulong must accept this value on this platform");
    cr_assert(ul > UINT_MAX, "precondition: ul > UINT_MAX");

    /* _uint отвергнет по range */
    size_t np = 12345; unsigned v = 999;
    cr_assert_not(fs2_pos_parse_uint(&s, 0, &np, &v));
    CHECK_EQ("out untouched",    v,  999u);
    CHECK_EQ("newpos untouched", np, 12345);

    fs2_free(&s);
    assert_balanced();
}
#endif /* ULONG_MAX > UINT_MAX */

/* =========================================================================
 * fs2_pos_parse_uint — failures из делегирования, out/newpos НЕ пишутся
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

/* ERANGE в strtoull — не range-check uint, а за пределами ullong */
Test(fs2_api, pos_parse_uint_ullong_overflow_false) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("18446744073709551616");   /* 2^64 */
    cr_assert_not(fs2_pos_parse_uint(&s, 0, NULL, NULL));
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_uint — опциональные out / newpos
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
 * Делегирование: _uint ≡ _ulong на значениях в диапазоне uint
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

/* "42" парсится как float 42.0 (не отдельный int-path) */
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

/* "5." → 5.0, newpos = 2 (C99: "5." валидная decimal-floating-constant) */
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
    CHECK_EQ("newpos", np, 7);   /* 3 пробела + 4 символа */
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
    CHECK_EQ("is inf",   isinf(v), true);
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

/* 'e' как первый символ — отклоняется pre-check (e5 не float) */
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
 * fs2_pos_parse_double — опциональные out / newpos
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
    char *raw = strdup("\xE9" "3.14");   /* первый байт 0xE9 */
    cr_assert(raw != NULL);
    fs2 s = fs2_adopt(&raw);
    cr_assert(raw == NULL);

    /* 0xE9 не digit/dot/sign/inf/nan → "Incorrect sym" */
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
