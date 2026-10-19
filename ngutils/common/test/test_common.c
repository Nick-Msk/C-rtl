#include <criterion/criterion.h>

#include "common.h"
#include "test_common.h"

#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------------------
 * Macros
 * ------------------------------------------------------------------------- */

Test(common, min_int) {
    cr_assert_eq(MIN(3, 5), 3);
    cr_assert_eq(MIN(5, 3), 3);
    cr_assert_eq(MIN(-1, 1), -1);
}

Test(common, max_int) {
    cr_assert_eq(MAX(3, 5), 5);
    cr_assert_eq(MAX(-1, -5), -1);
}

Test(common, min_evaluates_once) {
    int calls = 0;
    int v = MIN(calls++, 5);
    cr_assert_eq(calls, 1);
    cr_assert_eq(v, 0);
}

/* -------------------------------------------------------------------------
 * round_up_2 / calcnewsize
 * ------------------------------------------------------------------------- */

Test(common, round_up_2_basic) {
    cr_assert_eq(round_up_2(0), 0);
    cr_assert_eq(round_up_2(1), 2);
    cr_assert_eq(round_up_2(3), 4);
    cr_assert_eq(round_up_2(4), 8);
    cr_assert_eq(round_up_2(7), 8);
    cr_assert_eq(round_up_2(8), 16);
}

Test(common, calcnewsize_none) {
    cr_assert_eq(calcnewsize(SIZE_NONE, 42), 42);
    cr_assert_eq(calcnewsize(SIZE_NONE, 0), 0);
}

Test(common, calcnewsize_min10) {
    cr_assert_eq(calcnewsize(SIZE_MIN10, 5), 10);
    cr_assert_eq(calcnewsize(SIZE_MIN10, 10), 10);
    cr_assert_eq(calcnewsize(SIZE_MIN10, 42), 42);
}

Test(common, calcnewsize_power2) {
    cr_assert_eq(calcnewsize(SIZE_POWER2, 3), 4);
    cr_assert_eq(calcnewsize(SIZE_POWER2, 4), 8);
    cr_assert_eq(calcnewsize(SIZE_POWER2, 7), 8);
}

/* -------------------------------------------------------------------------
 * clower / cupper
 * ------------------------------------------------------------------------- */

Test(common, clower) {
    cr_assert_eq(clower('A', true),  'a');
    cr_assert_eq(clower('A', false), 'A');
    cr_assert_eq(clower('a', true),  'a');
}

Test(common, cupper) {
    cr_assert_eq(cupper('a', true),  'A');
    cr_assert_eq(cupper('a', false), 'a');
}

/* -------------------------------------------------------------------------
 * rnd* — just check ranges
 * ------------------------------------------------------------------------- */

Test(common, rndlowchar_range) {
    for (int i = 0; i < 100; i++) {
        char c = rndlowchar();
        cr_assert(c >= 'a' && c <= 'z', "got %c", c);
    }
}

Test(common, rndupperchar_range) {
    for (int i = 0; i < 100; i++) {
        char c = rndupperchar();
        cr_assert(c >= 'A' && c <= 'Z', "got %c", c);
    }
}

Test(common, rnddigitchar_range) {
    for (int i = 0; i < 100; i++) {
        char c = rnddigitchar();
        cr_assert(c >= '0' && c <= '9', "got %c", c);
    }
}

/* -------------------------------------------------------------------------
 * reverse
 * ------------------------------------------------------------------------- */

Test(common, reverse_even) {
    char buf[] = "abcd";
    cr_assert_str_eq(reverse(buf, 4), "dcba");
}

Test(common, reverse_odd) {
    char buf[] = "abcde";
    cr_assert_str_eq(reverse(buf, 5), "edcba");
}

Test(common, reverse_empty) {
    char buf[] = "";
    cr_assert_str_eq(reverse(buf, 0), "");
}

Test(common, reverse_single) {
    char buf[] = "x";
    cr_assert_str_eq(reverse(buf, 1), "x");
}

/* -------------------------------------------------------------------------
 * try_parse_int
 * ------------------------------------------------------------------------- */

Test(common, parse_int_valid) {
    int v = 0;
    cr_assert(try_parse_int("42", &v));
    cr_assert_eq(v, 42);

    cr_assert(try_parse_int("-17", &v));
    cr_assert_eq(v, -17);

    cr_assert(try_parse_int("0", &v));
    cr_assert_eq(v, 0);
}

Test(common, parse_int_invalid) {
    int v = 0;
    cr_assert_not(try_parse_int("", &v));
    cr_assert_not(try_parse_int("abc", &v));
    cr_assert_not(try_parse_int("42x", &v));
    cr_assert_not(try_parse_int("42 ", &v));
    cr_assert_not(try_parse_int(NULL, &v));
    cr_assert_not(try_parse_int("42", NULL));
}

Test(common, parse_int_overflow) {
    int v = 0;
    cr_assert_not(try_parse_int("999999999999", &v));
}

/* -------------------------------------------------------------------------
 * try_parse_long / try_parse_ulong / try_parse_double
 * ------------------------------------------------------------------------- */

Test(common, parse_long) {
    long v = 0;
    cr_assert(try_parse_long("123456789", &v));
    cr_assert_eq(v, 123456789L);

    cr_assert_not(try_parse_long("12x", &v));
}

Test(common, parse_ulong_rejects_negative) {
    unsigned long v = 0;
    cr_assert_not(try_parse_ulong("-1", &v));
    cr_assert(try_parse_ulong("42", &v));
    cr_assert_eq(v, 42UL);
    cr_assert_not(try_parse_ulong("42u", &v));
}

Test(common, parse_double) {
    double v = 0.0;
    cr_assert(try_parse_double("3.14", &v));
    cr_assert(v > 3.13 && v < 3.15);

    cr_assert(try_parse_double("-1e3", &v));
    cr_assert(v > -1001.0 && v < -999.0);

    cr_assert_not(try_parse_double("abc", &v));
}

/* -------------------------------------------------------------------------
 * pchar_cmp / pchar_revcmp
 * ------------------------------------------------------------------------- */

Test(common, pchar_cmp_order) {
    char a = 'a', b = 'b', c = 'a';
    cr_assert_lt(pchar_cmp(&a, &b), 0);
    cr_assert_gt(pchar_cmp(&b, &a), 0);
    cr_assert_eq(pchar_cmp(&a, &c), 0);
}

Test(common, pchar_revcmp_reversed) {
    char a = 'a', b = 'b';
    cr_assert_gt(pchar_revcmp(&a, &b), 0);
    cr_assert_lt(pchar_revcmp(&b, &a), 0);
}

Test(common, qsort_with_pchar_cmp) {
    char buf[] = "cab";
    qsort(buf, 3, 1, pchar_cmp);
    cr_assert_str_eq(buf, "abc");

    qsort(buf, 3, 1, pchar_revcmp);
    cr_assert_str_eq(buf, "cba");
}

/* -------------------------------------------------------------------------
 * Version
 * ------------------------------------------------------------------------- */

Test(common, version_string_matches_macro) {
    cr_assert_str_eq(common_version(), COMMON_VERSION);
    cr_assert_str_eq(common_version(), "0.1.0");
}

Test(common, version_is_top_of_list) {
    const char *const *v = common_versions();
    cr_assert_not_null(v);
    cr_assert_not_null(v[0]);
    cr_assert_str_eq(v[0], common_version());
    cr_assert_str_eq(v[0], COMMON_VERSION);
}

Test(common, versions_terminated_by_null) {
    const char *const *v = common_versions();
    size_t n = 0;
    while (v[n]) ++n;
    cr_assert_geq(n, 1);
    cr_assert_null(v[n]);
}

/* -------------------------------------------------------------------------
 * Exchangers
 * ------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------
 * Exchangers
 * ------------------------------------------------------------------------- */

Test(common, char_exch) {
    char a = 'A';
    char b = 'B';
    char_exch(&a, &b);
    CHECK_EQ("a", a, 'B');
    CHECK_EQ("b", b, 'A');
}

Test(common, int_exch) {
    int a = 10;
    int b = 20;
    int_exch(&a, &b);
    CHECK_EQ("a", a, 20);
    CHECK_EQ("b", b, 10);
}

Test(common, long_exch) {
    long a = 100L;
    long b = -200L;
    long_exch(&a, &b);
    CHECK_EQ("a", a, -200L);
    CHECK_EQ("b", b, 100L);
}

Test(common, dbl_exch) {
    double a = 1.5;
    double b = -2.5;
    dbl_exch(&a, &b);
    CHECK_EQ("a", a, -2.5);
    CHECK_EQ("b", b, 1.5);
}

Test(common, ptr_exch) {
    int x = 1, y = 2;
    void *p1 = &x;
    void *p2 = &y;
    
    ptr_exch(&p1, &p2);
    
    CHECK_PTR_EQ("p1", p1, &y);
    CHECK_PTR_EQ("p2", p2, &x);
}

Test(common, cstr_exch) {
    const char *s1 = "hello";
    const char *s2 = "world";
    
    cstr_exch(&s1, &s2);
    
    CHECK_STR_EQ("s1", s1, "world");
    CHECK_STR_EQ("s2", s2, "hello");
}

/* -------------------------------------------------------------------------
 * item_exch
 * ------------------------------------------------------------------------- */

Test(common, item_exch_small_stack_buffer) {
    struct { int a; int b; } v1 = { 1, 2 };
    struct { int a; int b; } v2 = { 3, 4 };

    item_exch(&v1, &v2, sizeof(v1));

    CHECK_EQ("v1.a", v1.a, 3);
    CHECK_EQ("v1.b", v1.b, 4);
    CHECK_EQ("v2.a", v2.a, 1);
    CHECK_EQ("v2.b", v2.b, 2);
}

Test(common, item_exch_large_heap_buffer) {
    char v1[300];
    char v2[300];
    memset(v1, 'A', sizeof(v1));
    memset(v2, 'B', sizeof(v2));

    item_exch(v1, v2, sizeof(v1));

    CHECK_EQ("v1[0]",   v1[0],   'B');
    CHECK_EQ("v1[299]", v1[299], 'B');
    CHECK_EQ("v2[0]",   v2[0],   'A');
    CHECK_EQ("v2[299]", v2[299], 'A');
}

Test(common, item_exch_same_pointer) {
    char v1[300];
    memset(v1, 'A', sizeof(v1));

    item_exch(v1, v1, sizeof(v1));

    CHECK_EQ("v1[0]",   v1[0],   'A');
    CHECK_EQ("v1[299]", v1[299], 'A');
}