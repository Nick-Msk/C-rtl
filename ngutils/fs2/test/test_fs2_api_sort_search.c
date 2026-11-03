#include <criterion/criterion.h>

#include "test_fs2.h"
#include "fs2_api.h"
#include <locale.h>

TestSuite(test_fs2_api_sort_search, .init = test_setup, .fini = test_teardown);

/* Хелпер: сделать отсортированный массив fs2 */
static void sort_fs2(fs2 *arr, size_t n) {
    qsort(arr, n, sizeof(fs2), fs2_cmp_wrap);
}

/* =========================================================================
 * fs2_bsearch — FS2() (v==NULL) key: нормализация (не должно падать)
 * ========================================================================= */

Test(fs2_api, bsearch_canon_key_vs_nonempty_array_not_found) {
    __fs2_checkreset();
    fs2 arr[] = { fs2_fromstr("a"), fs2_fromstr("b"), fs2_fromstr("c") };
    sort_fs2(arr, 3);

    fs2 key = FS2();   /* v=NULL, len=0 → нормализуется в "" */
    cr_assert_null(key.v);

    ptrdiff_t i = fs2_bsearch(&key, arr, 3);
    CHECK_EQ("not found", i, -1);

    for (size_t j = 0; j < 3; j++) { fs2_free(&arr[j]); CHECK_FS2VALID((int)j+1, &arr[j]); }
    fs2_free(&key);
    assert_balanced();
}

/* Ключевой: пустая строка есть в массиве — должна найтись через FS2() */
Test(fs2_api, bsearch_canon_key_finds_empty_in_array) {
    __fs2_checkreset();
    /* "" < "a" < "b" в strcmp-порядке */
    fs2 arr[] = { FS2EMPTY, fs2_fromstr("a"), fs2_fromstr("b") };
    sort_fs2(arr, 3);

    fs2 key = FS2();   /* v=NULL */
    cr_assert_null(key.v);

    ptrdiff_t i = fs2_bsearch(&key, arr, 3);
    CHECK_EQ("found at 0 (FS2EMPTY element)", i, 0);

    for (size_t j = 1; j < 3; j++) fs2_free(&arr[j]);
    fs2_free(&key);
    assert_balanced();
}

Test(fs2_api, bsearch_canon_key_vs_empty_array) {
    __fs2_checkreset();
    fs2 key = FS2();
    fs2 arr[] = { FS2EMPTY };   /* только пустая */

    ptrdiff_t i = fs2_bsearch(&key, arr, 1);
    CHECK_EQ("found at 0", i, 0);

    fs2_free(&key);
    assert_balanced();
}

Test(fs2_api, bsearch_canon_key_n_zero_returns_minus1) {
    __fs2_checkreset();
    fs2 key = FS2();
    fs2 dummy = fs2_fromstr("x");
    CHECK_EQ("n=0", fs2_bsearch(&key, &dummy, 0), -1);
    fs2_free(&dummy);
    fs2_free(&key);
    assert_balanced();
}

/* =========================================================================
 * fs2_bsearch_ci — то же для ci
 * ========================================================================= */

Test(fs2_api, bsearch_ci_canon_key_vs_nonempty_array_not_found) {
    __fs2_checkreset();
    fs2 arr[] = { fs2_fromstr("A"), fs2_fromstr("B"), fs2_fromstr("C") };
    sort_fs2(arr, 3);   /* sort by cs — для массивов A,B,C порядок совпадает с ci */

    fs2 key = FS2();
    cr_assert_null(key.v);

    CHECK_EQ("not found", fs2_bsearch_ci(&key, arr, 3), -1);

    for (size_t j = 0; j < 3; j++) fs2_free(&arr[j]);
    fs2_free(&key);
    assert_balanced();
}

Test(fs2_api, bsearch_ci_canon_key_finds_empty_in_array) {
    __fs2_checkreset();
    fs2 arr[] = { FS2EMPTY, fs2_fromstr("a"), fs2_fromstr("b") };
    sort_fs2(arr, 3);

    fs2 key = FS2();
    CHECK_EQ("found at 0", fs2_bsearch_ci(&key, arr, 3), 0);

    for (size_t j = 1; j < 3; j++) fs2_free(&arr[j]);
    fs2_free(&key);
    assert_balanced();
}

/* =========================================================================
 * Нормальные попадания (регрессия: фикс не сломал обычный путь)
 * ========================================================================= */

Test(fs2_api, bsearch_found_and_not_found) {
    __fs2_checkreset();
    fs2 arr[] = {
        fs2_fromstr("apple"), fs2_fromstr("banana"),
        fs2_fromstr("cherry"), fs2_fromstr("date"),
    };
    sort_fs2(arr, 4);

    fs2 hit  = fs2_fromstr("cherry");
    fs2 miss = fs2_fromstr("grape");

    CHECK_EQ("cherry → 2", fs2_bsearch(&hit,  arr, 4), 2);
    CHECK_EQ("grape  → -1", fs2_bsearch(&miss, arr, 4), -1);

    fs2_free(&hit);
    fs2_free(&miss);
    for (size_t j = 0; j < 4; j++) fs2_free(&arr[j]);
    assert_balanced();
}

Test(fs2_api, bsearch_ci_found_and_not_found) {
    __fs2_checkreset();
    fs2 arr[] = { fs2_fromstr("Apple"), fs2_fromstr("Banana"), fs2_fromstr("Cherry") };
    /* CI-сортировка для консистентности с bsearch_ci */
    qsort(arr, 3, sizeof(fs2), fs2_cmp_wrap_ci);

    fs2 hit_upper = fs2_fromstr("BANANA");
    fs2 hit_lower = fs2_fromstr("cherry");
    fs2 miss      = fs2_fromstr("date");

    CHECK_EQ("BANANA → 1", fs2_bsearch_ci(&hit_upper, arr, 3), 1);
    CHECK_EQ("cherry → 2", fs2_bsearch_ci(&hit_lower, arr, 3), 2);
    CHECK_EQ("date   → -1", fs2_bsearch_ci(&miss,      arr, 3), -1);

    fs2_free(&hit_upper);
    fs2_free(&hit_lower);
    fs2_free(&miss);
    for (size_t j = 0; j < 3; j++) fs2_free(&arr[j]);
    assert_balanced();
}

/* =========================================================================
 * Guards
 * ========================================================================= */

Test(fs2_api, bsearch_null_key_aborts, .signal = SIGABRT) {
    fs2 arr[] = { fs2_fromstr("a") };
    fs2_bsearch(NULL, arr, 1);
    fs2_free(&arr[0]);
}

Test(fs2_api, bsearch_null_arr_aborts, .signal = SIGABRT) {
    fs2 key = fs2_fromstr("a");
    fs2_bsearch(&key, NULL, 1);
    fs2_free(&key);
}

Test(fs2_api, bsearch_ci_null_key_aborts, .signal = SIGABRT) {
    fs2 arr[] = { fs2_fromstr("a") };
    fs2_bsearch_ci(NULL, arr, 1);
    fs2_free(&arr[0]);
}

Test(fs2_api, bsearch_ci_null_arr_aborts, .signal = SIGABRT) {
    fs2 key = fs2_fromstr("a");
    fs2_bsearch_ci(&key, NULL, 1);
    fs2_free(&key);
}

/* =========================================================================
 * Если нормализуешь fs2_cmp_wrap: массив с FS2() внутри тоже работает
 *   (в текущей реализации — упадёт на sort; оставь .disabled = true
 *    пока не примешь фикс fs2_cmp_wrap)
 * ========================================================================= */

#if 0
Test(fs2_api, bsearch_array_with_canon_element_sorted) {
    __fs2_checkreset();
    /* После нормализации fs2_cmp_wrap: FS2() сравняется как "" */
    fs2 arr[] = { fs2_fromstr("a"), FS2(), fs2_fromstr("b") };
    sort_fs2(arr, 3);   /* → [FS2(), "a", "b"] */

    fs2 key = FS2EMPTY;
    CHECK_EQ("empty found at 0", fs2_bsearch(&key, arr, 3), 0);

    fs2_free(&arr[0]);
    fs2_free(&arr[2]);
    assert_balanced();
}
#endif

/* =========================================================================
 * fs2_bsearch_str — nominal
 * ========================================================================= */

Test(fs2_api, bsearch_str_found_and_not_found) {
    __fs2_checkreset();
    static const char *arr[] = { "apple", "banana", "cherry", "date" };

    fs2 hit  = fs2_fromstr("cherry");
    fs2 miss = fs2_fromstr("grape");

    CHECK_EQ("cherry → 2",  fs2_bsearch_str(&hit,  arr, 4), 2);
    CHECK_EQ("grape  → -1", fs2_bsearch_str(&miss, arr, 4), -1);

    CHECK_FS2VALID(1, &hit);
    CHECK_FS2VALID(2, &miss);
    fs2_free(&hit);
    fs2_free(&miss);
    assert_balanced();
}

Test(fs2_api, bsearch_str_first_and_last) {
    __fs2_checkreset();
    static const char *arr[] = { "a", "b", "c", "d", "e" };

    fs2 a = fs2_fromstr("a");
    fs2 e = fs2_fromstr("e");

    CHECK_EQ("first → 0", fs2_bsearch_str(&a, arr, 5), 0);
    CHECK_EQ("last  → 4", fs2_bsearch_str(&e, arr, 5), 4);

    fs2_free(&a);
    fs2_free(&e);
    assert_balanced();
}

Test(fs2_api, bsearch_str_n_zero) {
    __fs2_checkreset();
    static const char *arr[] = { "only" };
    fs2 key = fs2_fromstr("only");
    CHECK_EQ("n=0 → -1", fs2_bsearch_str(&key, arr, 0), -1);
    fs2_free(&key);
    assert_balanced();
}

/* =========================================================================
 * fs2_bsearch_str — FS2() (v==NULL) key: нормализация (работает)
 * ========================================================================= */

Test(fs2_api, bsearch_str_canon_key_not_found) {
    __fs2_checkreset();
    static const char *arr[] = { "a", "b", "c" };

    fs2 key = FS2();
    cr_assert_null(key.v, "precondition: FS2() has v == NULL");

    CHECK_EQ("not found", fs2_bsearch_str(&key, arr, 3), -1);

    fs2_free(&key);
    assert_balanced();
}

Test(fs2_api, bsearch_str_canon_key_finds_empty) {
    __fs2_checkreset();
    /* "" < "a" < "b" */
    static const char *arr[] = { "", "a", "b" };

    fs2 key = FS2();
    cr_assert_null(key.v);

    CHECK_EQ("found at 0", fs2_bsearch_str(&key, arr, 3), 0);

    fs2_free(&key);
    assert_balanced();
}

/* FS2EMPTY (v != NULL) — тот же результат через другой путь */
Test(fs2_api, bsearch_str_fs2empty_key) {
    __fs2_checkreset();
    static const char *arr[] = { "", "a", "b" };
    fs2 key = FS2EMPTY;

    CHECK_EQ("found at 0", fs2_bsearch_str(&key, arr, 3), 0);
    assert_balanced();
}

/* =========================================================================
 * fs2_bsearch_str — guards (ловят болячку: key == NULL)
 * ========================================================================= */

Test(fs2_api, bsearch_str_null_key_aborts, .signal = SIGABRT) {
    static const char *arr[] = { "a" };
    fs2_bsearch_str(NULL, arr, 1);   /* ← без фикса — SIGSEGV, с фиксом — SIGABRT */
}

Test(fs2_api, bsearch_str_null_arr_aborts, .signal = SIGABRT) {
    fs2 key = fs2_fromstr("a");
    fs2_bsearch_str(&key, NULL, 1);
    fs2_free(&key);
}

/* =========================================================================
 * fs2_bsearch_ci_str — то же семейство
 * ========================================================================= */

Test(fs2_api, bsearch_ci_str_found_and_not_found) {
    __fs2_checkreset();
    static const char *arr[] = { "APPLE", "Banana", "cherry", "DATE" };
    /* CI-сортировка (в данном случае порядок совпадает с cs) */

    fs2 hit1 = fs2_fromstr("banana");
    fs2 hit2 = fs2_fromstr("CHERRY");
    fs2 miss = fs2_fromstr("grape");

    CHECK_EQ("banana → 1",  fs2_bsearch_ci_str(&hit1, arr, 4), 1);
    CHECK_EQ("CHERRY → 2",  fs2_bsearch_ci_str(&hit2, arr, 4), 2);
    CHECK_EQ("grape  → -1", fs2_bsearch_ci_str(&miss, arr, 4), -1);

    fs2_free(&hit1);
    fs2_free(&hit2);
    fs2_free(&miss);
    assert_balanced();
}

Test(fs2_api, bsearch_ci_str_canon_key_finds_empty) {
    __fs2_checkreset();
    static const char *arr[] = { "", "a", "b" };
    fs2 key = FS2();

    CHECK_EQ("found at 0", fs2_bsearch_ci_str(&key, arr, 3), 0);

    fs2_free(&key);
    assert_balanced();
}

Test(fs2_api, bsearch_ci_str_n_zero) {
    __fs2_checkreset();
    static const char *arr[] = { "only" };
    fs2 key = fs2_fromstr("only");
    CHECK_EQ("n=0 → -1", fs2_bsearch_ci_str(&key, arr, 0), -1);
    fs2_free(&key);
    assert_balanced();
}

Test(fs2_api, bsearch_ci_str_null_key_aborts, .signal = SIGABRT) {
    static const char *arr[] = { "a" };
    fs2_bsearch_ci_str(NULL, arr, 1);   /* ← без фикса — SIGSEGV */
}

Test(fs2_api, bsearch_ci_str_null_arr_aborts, .signal = SIGABRT) {
    fs2 key = fs2_fromstr("a");
    fs2_bsearch_ci_str(&key, NULL, 1);
    fs2_free(&key);
}

/* =========================================================================
 * Согласованность с fs2_bsearch (fs2 vs fs2): FS2() работает одинаково
 * ========================================================================= */

Test(fs2_api, bsearch_str_and_fs2_agree_on_canon_key) {
    __fs2_checkreset();
    static const char *carr[] = { "", "a", "b" };
    fs2 farr[] = { FS2EMPTY, fs2_fromstr("a"), fs2_fromstr("b") };
    qsort(farr, 3, sizeof(fs2), fs2_cmp_wrap);

    fs2 key = FS2();

    ptrdiff_t via_str = fs2_bsearch_str(&key, carr, 3);
    ptrdiff_t via_fs2 = fs2_bsearch(&key, farr, 3);

    CHECK_EQ("same result", via_str, via_fs2);
    CHECK_EQ("both found at 0", via_str, 0);

    fs2_free(&key);
    fs2_free(&farr[1]);
    fs2_free(&farr[2]);
    assert_balanced();
}
