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
    CHECK_EQ("Apple → -1",    fs2_anyof(&s, fruits, strcmp), -1);  /* not mango; case check */
    fs2_free(&s);

    fs2 t = fs2_fromstr("Apple");                                   /* case-sensitive */
    CHECK_EQ("Apple vs apple", fs2_anyof(&t, fruits, strcmp), -1);
    CHECK_FS2VALID(1, &t);
    fs2_free(&t);
    assert_balanced();
}

/* =========================================================================
 * fs2_anyof — empty array / empty string in array
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
 * fs2_anyof — FS2() (v==NULL) → normalized to ""
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
 * fs2_anyof — cyclic scan: correct index
 * ========================================================================= */

Test(fs2_api, anyof_returns_first_match_only) {
    __fs2_checkreset();
    /* duplicate "b" at indices 1 and 3 — must return 1 (first) */
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
 * fs2_anyof — custom comparator: verify cmp is actually used
 * ========================================================================= */

/* Comparator that always returns 0 — "found at index 0" */
static int always_equal(const char *a, const char *b) { (void)a; (void)b; return 0; }
/* Comparator that never equals — "not found" */
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
    fs2 s = fs2_fromstr("apple");   /* present in the array */
    CHECK_EQ("never_equal → -1", fs2_anyof(&s, fruits, never_equal), -1);
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_in / fs2_in_ci — thin wrappers (strcmp / strcasecmp)
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
 * FS2_IN / FS2_NOT_IN macros — smoke tests
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

/* =========================================================================
 * FS2_IN — case-sensitive
 * ========================================================================= */

Test(fs2_api, macro_in_hit) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("apple");
    CHECK_TRUE("apple in {apple,banana,cherry}",
               FS2_IN(&s, "apple", "banana", "cherry"));
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, macro_in_hit_not_first) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("cherry");
    CHECK_TRUE("cherry — last in the list",
               FS2_IN(&s, "apple", "banana", "cherry"));
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, macro_in_miss) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("mango");
    CHECK_FALSE("mango not in {apple,banana,cherry}",
                FS2_IN(&s, "apple", "banana", "cherry"));
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

/* case-sensitive: "Apple" ≠ "apple" */
Test(fs2_api, macro_in_case_mismatch) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("Apple");
    CHECK_FALSE("'Apple' ∉ {apple} (cs)",
                FS2_IN(&s, "apple"));
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, macro_in_single_element) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("only");
    fs2 b = fs2_fromstr("other");

    CHECK_TRUE ("'only'  ∈ {only}",  FS2_IN(&a, "only"));
    CHECK_FALSE("'other' ∉ {only}",  FS2_IN(&b, "only"));

    fs2_free(&a);
    fs2_free(&b);
    assert_balanced();
}

Test(fs2_api, macro_in_empty_string_member) {
    __fs2_checkreset();
    fs2 empty = FS2EMPTY;
    fs2 canon = FS2();
    fs2 non   = fs2_fromstr("x");

    CHECK_TRUE ("FS2EMPTY matches \"\"",  FS2_IN(&empty, "", "a"));
    CHECK_TRUE ("FS2()     matches \"\"",  FS2_IN(&canon, "", "a"));
    CHECK_FALSE("'x' ∉ {\"\",\"a\"}",       FS2_IN(&non,   "", "a"));

    fs2_free(&canon);
    fs2_free(&non);
    assert_balanced();
}

Test(fs2_api, macro_in_null_s_aborts, .signal = SIGABRT) {
    FS2_IN(NULL, "apple", "banana");
}

/* =========================================================================
 * FS2_IN_CI — case-insensitive
 * ========================================================================= */

Test(fs2_api, macro_in_ci_hit_exact) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("apple");
    CHECK_TRUE("apple ∈ {apple} (ci)",
               FS2_IN_CI(&s, "apple", "banana"));
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

/* CI: any case should match */
Test(fs2_api, macro_in_ci_case_insensitive_hit) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("APPLE");
    fs2 b = fs2_fromstr("Apple");
    fs2 c = fs2_fromstr("aPpLe");

    CHECK_TRUE("APPLE ∈ {apple}", FS2_IN_CI(&a, "apple"));
    CHECK_TRUE("Apple ∈ {apple}", FS2_IN_CI(&b, "apple"));
    CHECK_TRUE("aPpLe ∈ {apple}", FS2_IN_CI(&c, "apple"));

    CHECK_FS2VALID(1, &a);
    CHECK_FS2VALID(2, &b);
    CHECK_FS2VALID(3, &c);

    fs2_free(&a);
    fs2_free(&b);
    fs2_free(&c);
    assert_balanced();
}

Test(fs2_api, macro_in_ci_miss) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("MANGO");
    CHECK_FALSE("MANGO ∉ {apple,banana,cherry}",
                FS2_IN_CI(&s, "apple", "banana", "cherry"));
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, macro_in_ci_single_element) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("ONLY");
    fs2 b = fs2_fromstr("OTHER");

    CHECK_TRUE ("'ONLY'  ∈ {only} (ci)",  FS2_IN_CI(&a, "only"));
    CHECK_FALSE("'OTHER' ∉ {only} (ci)",  FS2_IN_CI(&b, "only"));

    fs2_free(&a);
    fs2_free(&b);
    assert_balanced();
}

Test(fs2_api, macro_in_ci_null_s_aborts, .signal = SIGABRT) {
    FS2_IN_CI(NULL, "apple");
}

/* =========================================================================
 * FS2_NOT_IN — case-sensitive (logical negation of FS2_IN)
 * ========================================================================= */

Test(fs2_api, macro_not_in_hit) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("mango");
    CHECK_TRUE("mango ∉ {apple,banana,cherry}",
               FS2_NOT_IN(&s, "apple", "banana", "cherry"));
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, macro_not_in_miss) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("apple");
    CHECK_FALSE("apple ∉ {apple,banana} → false",
                FS2_NOT_IN(&s, "apple", "banana"));
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

/* CS: "Apple" not in {apple}, so NOT_IN → true */
Test(fs2_api, macro_not_in_case_mismatch) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("Apple");
    CHECK_TRUE("'Apple' ∉ {apple} (cs) → NOT_IN true",
               FS2_NOT_IN(&s, "apple"));
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, macro_not_in_single_element) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("only");
    fs2 b = fs2_fromstr("other");

    CHECK_FALSE("'only'  ∉ {only} → false", FS2_NOT_IN(&a, "only"));
    CHECK_TRUE ("'other' ∉ {only} → true",  FS2_NOT_IN(&b, "only"));

    fs2_free(&a);
    fs2_free(&b);
    assert_balanced();
}

Test(fs2_api, macro_not_in_null_s_aborts, .signal = SIGABRT) {
    FS2_NOT_IN(NULL, "apple");
}

/* =========================================================================
 * FS2_NOT_IN_CI — case-insensitive
 * ========================================================================= */

Test(fs2_api, macro_not_in_ci_hit) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("MANGO");
    CHECK_TRUE("MANGO ∉ {apple,banana} (ci)",
               FS2_NOT_IN_CI(&s, "apple", "banana"));
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, macro_not_in_ci_case_insensitive_miss) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("APPLE");
    fs2 b = fs2_fromstr("Apple");
    fs2 c = fs2_fromstr("aPpLe");

    CHECK_FALSE("APPLE ∈ {apple} → NOT_IN_CI false", FS2_NOT_IN_CI(&a, "apple"));
    CHECK_FALSE("Apple ∈ {apple} → NOT_IN_CI false", FS2_NOT_IN_CI(&b, "apple"));
    CHECK_FALSE("aPpLe ∈ {apple} → NOT_IN_CI false", FS2_NOT_IN_CI(&c, "apple"));

    CHECK_FS2VALID(1, &a);
    CHECK_FS2VALID(2, &b);
    CHECK_FS2VALID(3, &c);

    fs2_free(&a);
    fs2_free(&b);
    fs2_free(&c);
    assert_balanced();
}

Test(fs2_api, macro_not_in_ci_single_element) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("ONLY");
    fs2 b = fs2_fromstr("OTHER");

    CHECK_FALSE("'ONLY' ∈ {only} → false", FS2_NOT_IN_CI(&a, "only"));
    CHECK_TRUE ("'OTHER' ∉ {only} → true", FS2_NOT_IN_CI(&b, "only"));

    fs2_free(&a);
    fs2_free(&b);
    assert_balanced();
}

Test(fs2_api, macro_not_in_ci_null_s_aborts, .signal = SIGABRT) {
    FS2_NOT_IN_CI(NULL, "apple");
}

/* =========================================================================
 * Paired semantics: IN ⇔ !NOT_IN (on the same cmp)
 * ========================================================================= */

Test(fs2_api, macro_in_not_in_pair_cs) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("apple");
    fs2 b = fs2_fromstr("Apple");   /* case mismatch → ∉ {apple} (cs) */
    fs2 c = fs2_fromstr("mango");

    /* For each input exactly one of IN/NOT_IN returns true */
    CHECK_TRUE ("apple  IN",          FS2_IN(&a, "apple", "banana"));
    CHECK_FALSE("apple  NOT_IN",      FS2_NOT_IN(&a, "apple", "banana"));

    CHECK_FALSE("Apple  IN",          FS2_IN(&b, "apple", "banana"));
    CHECK_TRUE ("Apple  NOT_IN",      FS2_NOT_IN(&b, "apple", "banana"));

    CHECK_FALSE("mango  IN",          FS2_IN(&c, "apple", "banana"));
    CHECK_TRUE ("mango  NOT_IN",      FS2_NOT_IN(&c, "apple", "banana"));

    fs2_free(&a);
    fs2_free(&b);
    fs2_free(&c);
    assert_balanced();
}

Test(fs2_api, macro_in_not_in_pair_ci) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("APPLE");   /* ci → ∈ {apple} */
    fs2 b = fs2_fromstr("Apple");
    fs2 c = fs2_fromstr("MANGO");

    CHECK_TRUE ("APPLE  IN_CI",       FS2_IN_CI(&a, "apple", "banana"));
    CHECK_FALSE("APPLE  NOT_IN_CI",   FS2_NOT_IN_CI(&a, "apple", "banana"));

    CHECK_TRUE ("Apple  IN_CI",       FS2_IN_CI(&b, "apple", "banana"));
    CHECK_FALSE("Apple  NOT_IN_CI",   FS2_NOT_IN_CI(&b, "apple", "banana"));

    CHECK_FALSE("MANGO  IN_CI",       FS2_IN_CI(&c, "apple", "banana"));
    CHECK_TRUE ("MANGO  NOT_IN_CI",   FS2_NOT_IN_CI(&c, "apple", "banana"));

    fs2_free(&a);
    fs2_free(&b);
    fs2_free(&c);
    assert_balanced();
}

/* =========================================================================
 * Consistency with the functional API
 * ========================================================================= */

Test(fs2_api, macro_in_matches_function) {
    __fs2_checkreset();
    static const char *fruits[] = { "apple", "banana", "cherry", NULL };

    fs2 hit  = fs2_fromstr("banana");
    fs2 miss = fs2_fromstr("mango");

    CHECK_EQ("banana IN == fs2_in",
             FS2_IN(&hit,  "apple", "banana", "cherry"),
             fs2_in(&hit,  fruits));
    CHECK_EQ("mango IN == fs2_in",
             FS2_IN(&miss, "apple", "banana", "cherry"),
             fs2_in(&miss, fruits));

    CHECK_EQ("banana IN_CI == fs2_in_ci",
             FS2_IN_CI(&hit,  "apple", "banana", "cherry"),
             fs2_in_ci(&hit,  fruits));
    CHECK_EQ("mango IN_CI == fs2_in_ci",
             FS2_IN_CI(&miss, "apple", "banana", "cherry"),
             fs2_in_ci(&miss, fruits));

    fs2_free(&hit);
    fs2_free(&miss);
    assert_balanced();
}

Test(fs2_api, macro_not_in_matches_function) {
    __fs2_checkreset();
    static const char *fruits[] = { "apple", "banana", NULL };

    fs2 hit  = fs2_fromstr("apple");
    fs2 miss = fs2_fromstr("mango");

    CHECK_EQ("apple NOT_IN == fs2_not_in",
             FS2_NOT_IN(&hit,  "apple", "banana"),
             fs2_not_in(&hit,  fruits));
    CHECK_EQ("mango NOT_IN == fs2_not_in",
             FS2_NOT_IN(&miss, "apple", "banana"),
             fs2_not_in(&miss, fruits));

    CHECK_EQ("apple NOT_IN_CI == fs2_not_in_ci",
             FS2_NOT_IN_CI(&hit,  "apple", "banana"),
             fs2_not_in_ci(&hit,  fruits));
    CHECK_EQ("mango NOT_IN_CI == fs2_not_in_ci",
             FS2_NOT_IN_CI(&miss, "apple", "banana"),
             fs2_not_in_ci(&miss, fruits));

    fs2_free(&hit);
    fs2_free(&miss);
    assert_balanced();
}

/* =========================================================================
 * Multi-element list: long, with duplicates
 * ========================================================================= */

Test(fs2_api, macro_in_long_list) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hotpink");

    CHECK_TRUE("hotpink in long list",
               FS2_IN(&s,
                      "red", "orange", "yellow", "green", "cyan",
                      "blue", "violet", "black", "white", "gray",
                      "pink", "hotpink", "magenta"));
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, macro_not_in_long_list_miss) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("silver");

    CHECK_TRUE("silver ∉ long list",
               FS2_NOT_IN(&s,
                          "red", "orange", "yellow", "green", "cyan",
                          "blue", "violet", "black", "white", "gray"));

    fs2_free(&s);
    assert_balanced();
}

/* duplicates in the list — IN is still true (first match), NOT_IN — false */
Test(fs2_api, macro_in_with_duplicates) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("b");

    CHECK_TRUE ("'b' IN {a,b,b,c}",     FS2_IN(&s, "a", "b", "b", "c"));
    CHECK_FALSE("'b' NOT_IN {a,b,b,c}", FS2_NOT_IN(&s, "a", "b", "b", "c"));

    fs2_free(&s);
    assert_balanced();
}

