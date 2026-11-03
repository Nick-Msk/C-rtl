#include <criterion/criterion.h>

#include "test_fs2.h"
#include "fs2_api.h"
#include <locale.h>

TestSuite(fs2_api_any_class, .init = test_setup, .fini = test_teardown);

/* =========================================================================
 * fs2_anyof — nominal (strcmp comparator)
 * ========================================================================= */

static const char *fruits[] = { "apple", "banana", "cherry", NULL };

Test(fs2_api, anyof_found_at_index_0) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("apple");
    CHECK_EQ("apple → 0", fs2_anyof(&s, fruits, strcmp), 0);
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, anyof_found_at_index_1) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("banana");
    CHECK_EQ("banana → 1", fs2_anyof(&s, fruits, strcmp), 1);
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, anyof_found_at_last) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("cherry");
    CHECK_EQ("cherry → 2", fs2_anyof(&s, fruits, strcmp), 2);
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, anyof_not_found) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("mango");
    CHECK_EQ("mango → -1",    fs2_anyof(&s, fruits, strcmp), -1);
    CHECK_EQ("Apple → -1",    fs2_anyof(&s, fruits, strcmp), -1);  /* не mango; проверка case */
    fs2_free(&s);

    fs2 t = fs2_fromstr("Apple");                                   /* case-sensitive */
    CHECK_EQ("Apple vs apple", fs2_anyof(&t, fruits, strcmp), -1);
    CHECK_FS2VALID(1, &t);
    fs2_free(&t);
    assert_balanced();
}

/* =========================================================================
 * fs2_anyof — пустой массив / пустая строка в массиве
 * ========================================================================= */

Test(fs2_api, anyof_empty_array) {
    __fs2_checkreset();
    static const char *empty[] = { NULL };
    fs2 s = fs2_fromstr("apple");
    CHECK_EQ("empty list → -1", fs2_anyof(&s, empty, strcmp), -1);
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, anyof_empty_string_in_array) {
    __fs2_checkreset();
    static const char *with_empty[] = { "", "a", NULL };

    fs2 s1 = FS2EMPTY;
    CHECK_EQ("FS2EMPTY matches \"\"", fs2_anyof(&s1, with_empty, strcmp), 0);

    fs2 s2 = fs2_fromstr("");
    CHECK_EQ("fromstr(\"\") matches \"\"", fs2_anyof(&s2, with_empty, strcmp), 0);

    fs2 s3 = fs2_fromstr("a");
    CHECK_EQ("'a' → 1", fs2_anyof(&s3, with_empty, strcmp), 1);
    CHECK_FS2VALID(1, &s1);
    CHECK_FS2VALID(2, &s2);
    CHECK_FS2VALID(3, &s3);

    fs2_free(&s2);
    fs2_free(&s3);
    assert_balanced();
}

/* =========================================================================
 * fs2_anyof — FS2() (v==NULL) → нормализация в ""
 * ========================================================================= */

Test(fs2_api, anyof_canon_v_null_normalized_to_empty) {
    __fs2_checkreset();
    fs2 s = FS2();
    cr_assert_null(s.v, "precondition: FS2() has v == NULL");

    static const char *with_empty[] = { "", "a", NULL };
    static const char *no_empty[]   = { "a", "b", NULL };

    CHECK_EQ("FS2() == \"\" (in list)",   fs2_anyof(&s, with_empty, strcmp), 0);
    CHECK_EQ("FS2() == \"\" (not in list)", fs2_anyof(&s, no_empty,   strcmp), -1);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_anyof — циклический обход: правильный индекс
 * ========================================================================= */

Test(fs2_api, anyof_returns_first_match_only) {
    __fs2_checkreset();
    /* дубликат "b" на индексах 1 и 3 — должен вернуть 1 (первый) */
    static const char *dups[] = { "a", "b", "c", "b", NULL };
    fs2 s = fs2_fromstr("b");
    CHECK_EQ("first 'b' → 1", fs2_anyof(&s, dups, strcmp), 1);
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, anyof_index_reflects_array_position) {
    __fs2_checkreset();
    static const char *big[] = { "a", "b", "c", "d", "e", "f", "g", NULL };

    for (size_t i = 0; i < 7; i++) {
        fs2 s = fs2_fromstr(big[i]);
        CHECK_EQ("position", fs2_anyof(&s, big, strcmp), (ptrdiff_t)i);
        CHECK_FS2VALID(1, &s);
        fs2_free(&s);
    }
    assert_balanced();
}

/* =========================================================================
 * fs2_anyof — guards
 * ========================================================================= */

Test(fs2_api, anyof_null_s_aborts, .signal = SIGABRT) {
    fs2_anyof(NULL, fruits, strcmp);
}

Test(fs2_api, anyof_null_strs_aborts, .signal = SIGABRT) {
    fs2 s = fs2_fromstr("apple");
    fs2_anyof(&s, NULL, strcmp);
    fs2_free(&s);
}

Test(fs2_api, anyof_both_null_aborts, .signal = SIGABRT) {
    fs2_anyof(NULL, NULL, strcmp);
}

/* =========================================================================
 * fs2_anyof — кастомный компаратор: проверка, что cmp реально используется
 * ========================================================================= */

/* Компаратор, который всегда возвращает 0 — «найдено на индексе 0» */
static int always_equal(const char *a, const char *b) { (void)a; (void)b; return 0; }
/* Компаратор, который никогда не равен — «не найдено» */
static int never_equal(const char *a, const char *b) { (void)a; (void)b; return 1; }

Test(fs2_api, anyof_uses_custom_cmp_always_equal) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("something");
    CHECK_EQ("always_equal → 0", fs2_anyof(&s, fruits, always_equal), 0);
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, anyof_uses_custom_cmp_never_equal) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("apple");   /* есть в массиве */
    CHECK_EQ("never_equal → -1", fs2_anyof(&s, fruits, never_equal), -1);
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_in / fs2_in_ci — тонкие обёртки (strcmp / strcasecmp)
 * ========================================================================= */

Test(fs2_api, in_case_sensitive) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("apple");
    fs2 b = fs2_fromstr("Apple");
    fs2 c = fs2_fromstr("mango");

    CHECK_TRUE ("apple in",     fs2_in(&a, fruits));
    CHECK_FALSE("Apple not in", fs2_in(&b, fruits));   /* case-sensitive */
    CHECK_FALSE("mango not in", fs2_in(&c, fruits));

    CHECK_FS2VALID(1, &a);
    CHECK_FS2VALID(2, &b);
    CHECK_FS2VALID(3, &c);

    fs2_free(&a);
    fs2_free(&b);
    fs2_free(&c);
    assert_balanced();
}

Test(fs2_api, in_ci_case_insensitive) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("APPLE");
    fs2 b = fs2_fromstr("Apple");
    fs2 c = fs2_fromstr("BaNaNa");
    fs2 d = fs2_fromstr("mango");

    CHECK_TRUE ("APPLE in_ci",   fs2_in_ci(&a, fruits));
    CHECK_TRUE ("Apple in_ci",   fs2_in_ci(&b, fruits));
    CHECK_TRUE ("BaNaNa in_ci",  fs2_in_ci(&c, fruits));
    CHECK_FALSE("mango not in_ci", fs2_in_ci(&d, fruits));

    CHECK_FS2VALID(1, &a);
    CHECK_FS2VALID(2, &b);
    CHECK_FS2VALID(3, &c);
    CHECK_FS2VALID(4, &d);

    fs2_free(&a);
    fs2_free(&b);
    fs2_free(&c);
    fs2_free(&d);
    assert_balanced();
}

/* =========================================================================
 * FS2_IN / FS2_NOT_IN макросы — smoke-тесты
 * ========================================================================= */

Test(fs2_api, macro_in_case_sensitive) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("apple");

    CHECK_TRUE ("apple in",    FS2_IN(&s, "apple", "banana", "cherry"));
    CHECK_FALSE("mango in",    FS2_IN(&s, "mango", "banana"));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, macro_in_ci_case_insensitive) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("APPLE");

    CHECK_TRUE ("APPLE in_ci", FS2_IN_CI(&s, "apple", "banana"));
    CHECK_FALSE("APPLE in",    FS2_IN(&s, "apple", "banana"));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, macro_not_in_semantics) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("mango");

    CHECK_TRUE ("mango NOT_IN",    FS2_NOT_IN(&s, "apple", "banana"));
    CHECK_FALSE("mango NOT_IN dup", FS2_NOT_IN(&s, "mango", "banana"));

    fs2_free(&s);
    assert_balanced();
}
