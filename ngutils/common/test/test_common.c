#include <criterion/criterion.h>

#include "common.h"
#include "test_common.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

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

/* -------------------------------------------------------------------------
 * Linear Comparators
 * ------------------------------------------------------------------------- */

Test(common, compare_int_works) {
    CHECK_EQ("1 vs 2", compare_int(1, 2), -1);
    CHECK_EQ("2 vs 1", compare_int(2, 1),  1);
    CHECK_EQ("1 vs 1", compare_int(1, 1),  0);
    CHECK_EQ("neg vs pos", compare_int(-5, 10), -1);
}

Test(common, compare_uint_works) {
    CHECK_EQ("1 vs 2", compare_uint(1U, 2U), -1);
    CHECK_EQ("2 vs 1", compare_uint(2U, 1U),  1);
    CHECK_EQ("1 vs 1", compare_uint(1U, 1U),  0);
}

Test(common, compare_long_works) {
    CHECK_EQ("1 vs 2", compare_long(100L, 200L), -1);
    CHECK_EQ("2 vs 1", compare_long(200L, 100L),  1);
    CHECK_EQ("1 vs 1", compare_long(100L, 100L),  0);
}

Test(common, compare_ulong_works) {
    CHECK_EQ("1 vs 2", compare_ulong(100UL, 200UL), -1);
    CHECK_EQ("2 vs 1", compare_ulong(200UL, 100UL),  1);
    CHECK_EQ("1 vs 1", compare_ulong(100UL, 100UL),  0);
}

Test(common, compare_char_works) {
    CHECK_EQ("'a' vs 'b'", compare_char('a', 'b'), -1);
    CHECK_EQ("'b' vs 'a'", compare_char('b', 'a'),  1);
    CHECK_EQ("'a' vs 'a'", compare_char('a', 'a'),  0);
}

Test(common, compare_bool_works) {
    CHECK_EQ("false vs true", compare_bool(false, true), -1);
    CHECK_EQ("true vs false", compare_bool(true, false),  1);
    CHECK_EQ("true vs true",  compare_bool(true, true),   0);
}

Test(common, compare_dbl_works) {
    CHECK_EQ("1.0 vs 2.0", compare_dbl(1.0, 2.0), -1);
    CHECK_EQ("2.0 vs 1.0", compare_dbl(2.0, 1.0),  1);
    CHECK_EQ("1.0 vs 1.0", compare_dbl(1.0, 1.0),  0);
}

Test(common, compare_dbl_handles_nan) {
    /* В C: NAN больше любого числа, но NAN == NAN ложно.
       Твой код: любая NAN считается "меньше" числа, и NAN == NAN. */
    CHECK_EQ("NaN vs 1.0", compare_dbl(NAN, 1.0), -1);
    CHECK_EQ("1.0 vs NaN", compare_dbl(1.0, NAN),  1);
    CHECK_EQ("NaN vs NaN", compare_dbl(NAN, NAN),  0);
    CHECK_EQ("-Inf vs +Inf", compare_dbl(-INFINITY, INFINITY), -1);
}

Test(common, compare_ptr_works) {
    int arr[2] = {0, 1};
    /* Убедимся, что &arr[0] < &arr[1] */
    CHECK_EQ("lower vs higher", compare_ptr(&arr[0], &arr[1]), -1);
    CHECK_EQ("higher vs lower", compare_ptr(&arr[1], &arr[0]),  1);
    CHECK_EQ("same ptr",        compare_ptr(&arr[0], &arr[0]),  0);
}

/* -------------------------------------------------------------------------
 * Pointer comparators (qsort style)
 *
 * These take pointers to the values, so we pass &val.
 * ------------------------------------------------------------------------- */

Test(common, pchar_cmp_works) {
    char a = 'a', b = 'b';
    CHECK_EQ("a vs b", pchar_cmp(&a, &b), -1);
    CHECK_EQ("b vs a", pchar_revcmp(&a, &b),  1); /* reverse: a vs b => 1 */
}

Test(common, pint_cmp_works) {
    int a = 10, b = 20;
    CHECK_EQ("10 vs 20", pint_cmp(&a, &b), -1);
    CHECK_EQ("10 vs 20 rev", pint_revcmp(&a, &b), 1);
}

Test(common, plong_cmp_works) {
    long a = 10L, b = 20L;
    CHECK_EQ("10 vs 20", plong_cmp(&a, &b), -1);
    CHECK_EQ("10 vs 20 rev", plong_revcmp(&a, &b), 1);
}

Test(common, pptr_cmp_works) {
    /* Используем массив, чтобы гарантировать, что &arr[0] < &arr[1] */
    int arr[2] = {9, 8};
    void *p1 = &arr[0];
    void *p2 = &arr[1];
    
    /* pptr_cmp ожидает указатели на указатели */
    CHECK_EQ("p1 vs p2", pptr_cmp(&p1, &p2), -1); 
    CHECK_EQ("p1 vs p2 rev", pptr_revcmp(&p1, &p2), 1);
}

Test(common, pitem_cmp_works) {
    int a = 0x01020304;
    int b = 0x01020305;
    CHECK_EQ("memcmp less", pitem_cmp(&a, &b, sizeof(int)), -1);
    CHECK_EQ("memcmp equal", pitem_cmp(&a, &a, sizeof(int)), 0);
}

/* -------------------------------------------------------------------------
 * qsort integration (proves the pointer comparators actually work)
 * ------------------------------------------------------------------------- */

Test(common, qsort_using_pint_cmp) {
    int arr[] = { 5, 2, 8, 1, 9, 3 };
    size_t n = sizeof(arr) / sizeof(arr[0]);

    qsort(arr, n, sizeof(int), pint_cmp);

    for (size_t i = 1; i < n; i++) {
        CHECK_TRUE("sorted ascending", arr[i - 1] <= arr[i]);
    }
    CHECK_EQ("first", arr[0], 1);
    CHECK_EQ("last",  arr[n - 1], 9);
}

Test(common, qsort_using_pint_revcmp) {
    int arr[] = { 5, 2, 8, 1, 9, 3 };
    size_t n = sizeof(arr) / sizeof(arr[0]);

    qsort(arr, n, sizeof(int), pint_revcmp);

    for (size_t i = 1; i < n; i++) {
        CHECK_TRUE("sorted descending", arr[i - 1] >= arr[i]);
    }
    CHECK_EQ("first", arr[0], 9);
    CHECK_EQ("last",  arr[n - 1], 1);
}

/* =========================================================================
 * Direct comparison tests
 * ========================================================================= */

Test(common, pcsrr_cmp_basic) {
    const char *a = "apple";
    const char *b = "banana";
    const char *c = "apple";

    /* Передаем адреса указателей, как это делает qsort */
    CHECK_TRUE("apple < banana", pcstr_cmp(&a, &b) < 0);
    CHECK_TRUE("banana > apple", pcstr_cmp(&b, &a) > 0);
    CHECK_EQ("apple == apple", pcstr_cmp(&a, &c), 0);
}

Test(common, pcsrr_icmp_basic) {
    const char *a = "Apple";
    const char *b = "apple";
    const char *c = "BANANA";

    CHECK_EQ("Apple == apple (ci)", pcstr_icmp(&a, &b), 0);
    CHECK_TRUE("apple < BANANA (ci)", pcstr_icmp(&b, &c) < 0);
    CHECK_TRUE("BANANA > Apple (ci)", pcstr_icmp(&c, &a) > 0);
}

Test(common, pcsrr_cmp_empty_strings) {
    const char *a = "";
    const char *b = "a";
    const char *c = "";

    CHECK_EQ("empty == empty", pcstr_cmp(&a, &c), 0);
    CHECK_TRUE("empty < a", pcstr_cmp(&a, &b) < 0);
    CHECK_TRUE("a > empty", pcstr_cmp(&b, &a) > 0);
}

/* =========================================================================
 * qsort integration
 * ========================================================================= */

Test(common, pcsrr_cmp_qsort) {
    const char *arr[] = { "cherry", "apple", "date", "banana" };
    size_t n = sizeof(arr) / sizeof(arr[0]);

    qsort(arr, n, sizeof(const char *), pcstr_cmp);

    CHECK_STR_EQ("0", arr[0], "apple");
    CHECK_STR_EQ("1", arr[1], "banana");
    CHECK_STR_EQ("2", arr[2], "cherry");
    CHECK_STR_EQ("3", arr[3], "date");
}

Test(common, pcsrr_icmp_qsort) {
    const char *arr[] = { "Cherry", "apple", "Date", "banana" };
    size_t n = sizeof(arr) / sizeof(arr[0]);

    qsort(arr, n, sizeof(const char *), pcstr_icmp);

    CHECK_STR_EQ("0", arr[0], "apple");
    CHECK_STR_EQ("1", arr[1], "banana");
    CHECK_STR_EQ("2", arr[2], "Cherry");
    CHECK_STR_EQ("3", arr[3], "Date");
}

/* =========================================================================
 * bsearch integration
 * ========================================================================= */

Test(common, pcsrr_cmp_bsearch) {
    /* Отсортированный массив для bsearch */
    const char *arr[] = { "apple", "banana", "cherry", "date" };
    size_t n = sizeof(arr) / sizeof(arr[0]);

    const char *key = "cherry";
    const char **found = bsearch(&key, arr, n, sizeof(const char *), pcstr_cmp);

    CHECK_NOT_NULL("found cherry", found);
    CHECK_STR_EQ("found content", *found, "cherry");

    const char *missing = "zucchini";
    const char **not_found = bsearch(&missing, arr, n, sizeof(const char *), pcstr_cmp);

    CHECK_NULL("not found", not_found);
}

Test(common, pcsrr_icmp_bsearch) {
    /* Отсортированный без учета регистра */
    const char *arr[] = { "Apple", "banana", "Cherry", "date" };
    size_t n = sizeof(arr) / sizeof(arr[0]);

    const char *key = "cherry"; // lower case
    const char **found = bsearch(&key, arr, n, sizeof(const char *), pcstr_icmp);

    CHECK_NOT_NULL("found cherry (ci)", found);
    CHECK_STR_EQ("found content", *found, "Cherry");
}