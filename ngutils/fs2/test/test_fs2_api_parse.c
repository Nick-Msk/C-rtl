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