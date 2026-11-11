#include <criterion/criterion.h>

#include "test_fs2.h"
#include "fs2.h"
#include "fs2_api.h"
#include "fs2slice.h"

TestSuite(fs2slice, .init = test_setup, .fini = test_teardown);

/* =========================================================================
 * Helpers
 * ========================================================================= */

/* Compare slice content (bounded, no NUL assumption) */
static bool
slice_eq(const fs2slice *s, const char *expected) {
    size_t n = fs2slice_len(s);
    return n == strlen(expected) &&
           memcmp(fs2slice_data(s), expected, n) == 0;
}

/* Build a manual fs2arrslice (until fs2arrslice_split exists).
 * Consumes ONE borrow on parent (same as split would). */
static fs2arrslice
mk_arrslice(fs2 *parent, const fs2sliceview *views, size_t n) {
    fs2arrslice a = FS2ARRSLICE();
    a.parent = parent;
    a.ranges = malloc(n * sizeof(fs2sliceview));
    cr_assert_not_null(a.ranges);
    memcpy(a.ranges, views, n * sizeof(fs2sliceview));
    a.hwm = a.size = a.count = n;
    fs2slice_parentinc(parent);            /* one borrow for whole array */
    return a;
}

/* =========================================================================
 * fs2slice_new — nominal
 * ========================================================================= */

Test(fs2slice, new_basic) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("hello world");
    CHECK_EQ("borrow before", 0, parent.borrow);

    fs2slice s = fs2slice_new(&parent, 6, 11);   /* "world" */

    CHECK_EQ  ("borrow after", 1, parent.borrow);
    CHECK_PTR_EQ("parent", &parent, s.parent);
    CHECK_EQ  ("from", 6, s.range.from);
    CHECK_EQ  ("to",   11, s.range.to);
    CHECK_EQ  ("len",  5, fs2slice_len(&s));
    CHECK_TRUE("data", slice_eq(&s, "world"));

    fs2slice_free(&s);
    CHECK_EQ("borrow released", 0, parent.borrow);

    fs2_free(&parent);
    assert_balanced();
}

Test(fs2slice, new_full_range) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("abc");

    fs2slice s = fs2slice_new(&parent, 0, 3);

    CHECK_EQ  ("borrow", 1, parent.borrow);
    CHECK_TRUE("full", slice_eq(&s, "abc"));

    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

Test(fs2slice, new_last_byte) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("abc");

    fs2slice s = fs2slice_new(&parent, 2, 3);

    CHECK_EQ  ("borrow", 1, parent.borrow);
    CHECK_TRUE("'c'", slice_eq(&s, "c"));

    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

Test(fs2slice, new_first_byte) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("abc");

    fs2slice s = fs2slice_new(&parent, 0, 1);

    CHECK_TRUE("'a'", slice_eq(&s, "a"));

    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

/* =========================================================================
 * fs2slice_new — errors
 * ========================================================================= */

Test(fs2slice, new_null_parent_aborts, .signal = SIGABRT) {
    fs2slice_new(NULL, 0, 1);
}

Test(fs2slice, new_empty_range_raise, .signal = SIGINT) {
    fs2 parent = fs2_fromstr("abc");
    /* from == to → from >= to → raise */
    fs2slice_new(&parent, 1, 1);
    fs2_free(&parent);
}

Test(fs2slice, new_zero_range_raise, .signal = SIGINT) {
    fs2 parent = fs2_fromstr("abc");
    fs2slice_new(&parent, 0, 0);
    fs2_free(&parent);
}

Test(fs2slice, new_reversed_raise, .signal = SIGINT) {
    fs2 parent = fs2_fromstr("abc");
    fs2slice_new(&parent, 2, 1);
    fs2_free(&parent);
}

Test(fs2slice, new_out_of_range_raise, .signal = SIGINT) {
    fs2 parent = fs2_fromstr("abc");             /* len=3 */
    fs2slice_new(&parent, 0, 100);
    fs2_free(&parent);
}

Test(fs2slice, new_on_empty_parent_raise, .signal = SIGINT) {
    fs2 parent = FS2EMPTY;                       /* len=0 */
    fs2slice_new(&parent, 0, 1);                 /* to > 0 → raise */
}

Test(fs2slice, new_on_canonical_parent_raise, .signal = SIGINT) {
    fs2 parent = FS2();                          /* v=NULL, len=0 */
    fs2slice_new(&parent, 0, 1);
    fs2_free(&parent);
}

/* =========================================================================
 * fs2slice_free — idempotency, NULL, borrow accounting
 * ========================================================================= */

Test(fs2slice, free_null_noop) {
    fs2slice_free(NULL);                          /* no crash */
}

Test(fs2slice, free_twice_idempotent) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("abc");
    fs2slice s = fs2slice_new(&parent, 0, 3);
    cr_assert_eq(parent.borrow, 1);

    fs2slice_free(&s);
    CHECK_EQ("borrow after first", 0, parent.borrow);

    fs2slice_free(&s);                            /* second call */
    CHECK_EQ("borrow after second", 0, parent.borrow);   /* no underflow */

    fs2_free(&parent);
    assert_balanced();
}

Test(fs2slice, free_releases_range) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("abc");
    fs2slice s = fs2slice_new(&parent, 0, 2);

    fs2slice_free(&s);

    CHECK_PTR_EQ("parent nulled", NULL, s.parent);
    CHECK_EQ  ("from reset", 0, s.range.from);
    CHECK_EQ  ("to reset",   0, s.range.to);

    fs2_free(&parent);
    assert_balanced();
}

/* =========================================================================
 * Multiple slices on same parent
 * ========================================================================= */

Test(fs2slice, multiple_slices_increment_borrow) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("hello world");

    fs2slice a = fs2slice_new(&parent, 0, 5);     /* "hello" */
    fs2slice b = fs2slice_new(&parent, 6, 11);    /* "world" */
    fs2slice c = fs2slice_new(&parent, 1, 3);     /* "el" */

    CHECK_EQ("borrow", 3, parent.borrow);
    CHECK_TRUE("a", slice_eq(&a, "hello"));
    CHECK_TRUE("b", slice_eq(&b, "world"));
    CHECK_TRUE("c", slice_eq(&c, "el"));

    fs2slice_free(&a);
    CHECK_EQ("borrow after a", 2, parent.borrow);
    CHECK_TRUE("b still", slice_eq(&b, "world"));

    fs2slice_free(&b);
    CHECK_EQ("borrow after b", 1, parent.borrow);

    fs2slice_free(&c);
    CHECK_EQ("borrow after c", 0, parent.borrow);

    fs2_free(&parent);
    assert_balanced();
}

/* =========================================================================
 * Data/len — read after realloc (leech behaviour)
 * ========================================================================= */

Test(fs2slice, data_survives_parent_realloc) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("hello");
    fs2slice s = fs2slice_new(&parent, 0, 5);
    CHECK_TRUE("before grow", slice_eq(&s, "hello"));

    /* Grow parent: may realloc; slice must still read via parent->v */
    fs2_catstr(&parent, " world");
    CHECK_TRUE("after grow, slice still 'hello'", slice_eq(&s, "hello"));

    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

Test(fs2slice, data_reflects_in_place_mutation) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("hello");
    fs2slice s = fs2slice_new(&parent, 0, 5);
    CHECK_TRUE("lower", slice_eq(&s, "hello"));

    /* toupper mutates bytes, len unchanged → allowed */
    fs2_toupper(&parent);
    CHECK_TRUE("upper via slice (leech)", slice_eq(&s, "HELLO"));

    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

/* =========================================================================
 * fs2arrslice — canonical / free
 * ========================================================================= */

Test(fs2slice, arrslice_canonical_noop) {
    fs2arrslice a = FS2ARRSLICE();
    CHECK_PTR_EQ("parent", NULL, a.parent);
    CHECK_PTR_EQ("ranges", NULL, a.ranges);
    CHECK_EQ   ("count", 0, a.count);

    fs2arrslice_free(&a);                        /* no crash */
    CHECK_PTR_EQ("still NULL", NULL, a.parent);
}

Test(fs2slice, arrslice_free_null_noop) {
    fs2arrslice_free(NULL);
}

Test(fs2slice, arrslice_free_twice_idempotent) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("a,b,c");
    fs2sliceview v[] = { {0,1}, {2,3}, {4,5} };
    fs2arrslice a = mk_arrslice(&parent, v, 3);
    cr_assert_eq(parent.borrow, 1);

    fs2arrslice_free(&a);
    CHECK_EQ("borrow after first", 0, parent.borrow);

    fs2arrslice_free(&a);                        /* second call */
    CHECK_EQ("borrow after second", 0, parent.borrow);  /* no underflow */

    fs2_free(&parent);
    assert_balanced();
}

Test(fs2slice, arrslice_free_resets_shape) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("a,b");
    fs2sliceview v[] = { {0,1}, {2,3} };
    fs2arrslice a = mk_arrslice(&parent, v, 2);

    fs2arrslice_free(&a);

    CHECK_PTR_EQ("parent", NULL, a.parent);
    CHECK_PTR_EQ("ranges", NULL, a.ranges);
    CHECK_EQ   ("hwm",   0, a.hwm);
    CHECK_EQ   ("size",  0, a.size);
    CHECK_EQ   ("count", 0, a.count);

    fs2_free(&parent);
    assert_balanced();
}

/* =========================================================================
 * fs2arrslice_get — bounds + independent slice (adds borrow)
 * ========================================================================= */

Test(fs2slice, arrslice_get_valid) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("a,bb,ccc");
    fs2sliceview v[] = { {0,1}, {2,4}, {5,8} };
    fs2arrslice a = mk_arrslice(&parent, v, 3);
    cr_assert_eq(parent.borrow, 1);

    fs2slice s = fs2arrslice_get(&a, 1);         /* "bb" */

    CHECK_EQ  ("borrow after get", 2, parent.borrow);
    CHECK_TRUE("bb", slice_eq(&s, "bb"));

    fs2slice_free(&s);
    CHECK_EQ("borrow back to 1", 1, parent.borrow);

    fs2arrslice_free(&a);
    CHECK_EQ("borrow 0", 0, parent.borrow);

    fs2_free(&parent);
    assert_balanced();
}

Test(fs2slice, arrslice_get_all_indices) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("a,b,c");
    fs2sliceview v[] = { {0,1}, {2,3}, {4,5} };
    fs2arrslice a = mk_arrslice(&parent, v, 3);

    const char *expected[] = { "a", "b", "c" };
    for (size_t i = 0; i < 3; i++) {
        fs2slice s = fs2arrslice_get(&a, i);
        CHECK_TRUE("element", slice_eq(&s, expected[i]));
        fs2slice_free(&s);
    }

    fs2arrslice_free(&a);
    fs2_free(&parent);
    assert_balanced();
}

Test(fs2slice, arrslice_get_null_arr_aborts, .signal = SIGABRT) {
    fs2arrslice_get(NULL, 0);
}

Test(fs2slice, arrslice_get_out_of_range_raise, .signal = SIGINT) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("abc");
    fs2sliceview v[] = { {0,3} };
    fs2arrslice a = mk_arrslice(&parent, v, 1);

    fs2arrslice_get(&a, 5);                      /* idx >= count → SIGINT */

    /* unreachable cleanup — process aborted */
}

/* =========================================================================
 * Borrow leak detection via fs2slice_sum
 * ========================================================================= */

Test(fs2slice, sum_starts_at_zero_or_tracks) {
    /* g_borrow_sum is global; we can't assume 0 at test start (other tests
     * may not have cleaned up in theory — but our tests always clean).
     * Compare before/after within this test. */
    long before = fs2slice_sum();

    fs2 parent = fs2_fromstr("abc");
    fs2slice a = fs2slice_new(&parent, 0, 1);
    fs2slice b = fs2slice_new(&parent, 1, 2);

    CHECK_EQ("sum grew by 2", before + 2, fs2slice_sum());

    fs2slice_free(&a);
    fs2slice_free(&b);

    CHECK_EQ("sum restored", before, fs2slice_sum());
    fs2_free(&parent);
    assert_balanced();
}

Test(fs2slice, sum_arrslice_counts_one) {
    long before = fs2slice_sum();

    fs2 parent = fs2_fromstr("a,b,c");
    fs2sliceview v[] = { {0,1}, {2,3}, {4,5} };
    fs2arrslice a = mk_arrslice(&parent, v, 3);

    CHECK_EQ("arrslice = 1 borrow in sum", before + 1, fs2slice_sum());

    fs2arrslice_free(&a);
    CHECK_EQ("restored", before, fs2slice_sum());
    fs2_free(&parent);
    assert_balanced();
}

/* =========================================================================
 * fs2slice_validate
 * ========================================================================= */

Test(fs2slice, validate_null) {
    cr_assert_not(fs2slice_validate(NULL, NULL));
}

Test(fs2slice, validate_canonical_is_zombie) {
    fs2slice s = FS2SLICE();                /* parent=NULL */
    cr_assert_not(fs2slice_validate(NULL, &s));
}

Test(fs2slice, validate_basic_ok) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("hello");
    fs2slice s = fs2slice_new(&parent, 1, 4);
    CHECK_TRUE("valid", fs2slice_validate(NULL, &s));
    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

Test(fs2slice, validate_zombie_after_free) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("hello");
    fs2slice s = fs2slice_new(&parent, 0, 5);
    fs2slice_free(&s);
    CHECK_FALSE("zombie", fs2slice_validate(NULL, &s));
    fs2_free(&parent);
    assert_balanced();
}

Test(fs2slice, validate_empty_range_fails) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("hello");
    fs2slice bad = { .parent = &parent, .range = {2, 2} };   /* [2,2) forbidden */
    CHECK_FALSE("empty range", fs2slice_validate(NULL, &bad));
    fs2_free(&parent);
    assert_balanced();
}

Test(fs2slice, validate_reversed_range_fails) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("hello");
    fs2slice bad = { .parent = &parent, .range = {4, 2} };   /* reversed */
    CHECK_FALSE("reversed", fs2slice_validate(NULL, &bad));
    fs2_free(&parent);
    assert_balanced();
}

Test(fs2slice, validate_out_of_parent_range_fails) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("hi");                         /* len=2 */
    fs2slice bad = { .parent = &parent, .range = {0, 100} }; /* to > len */
    CHECK_FALSE("out of range", fs2slice_validate(NULL, &bad));
    fs2_free(&parent);
    assert_balanced();
}

Test(fs2slice, validate_borrow_zero_fails) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("hello");
    fs2slice s = fs2slice_new(&parent, 0, 3);
    cr_assert_eq(parent.borrow, 1);

    /* Simulate corruption: forget to increment borrow */
    parent.borrow = 0;
    CHECK_FALSE("borrow == 0", fs2slice_validate(NULL, &s));

    parent.borrow = 1;                                      /* restore for free */
    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

/* =========================================================================
 * fs2arrslice_validate
 * ========================================================================= */

Test(fs2slice, arrslice_validate_null) {
    cr_assert_not(fs2arrslice_validate(NULL, NULL));
}

Test(fs2slice, arrslice_validate_canonical_ok) {
    fs2arrslice a = FS2ARRSLICE();
    CHECK_TRUE("canonical", fs2arrslice_validate(NULL, &a));
}

Test(fs2slice, arrslice_validate_basic_ok) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("a,bb,ccc");
    fs2sliceview v[] = { {0,1}, {2,4}, {5,8} };
    fs2arrslice a = mk_arrslice(&parent, v, 3);

    CHECK_TRUE("valid", fs2arrslice_validate(NULL, &a));

    fs2arrslice_free(&a);
    fs2_free(&parent);
    assert_balanced();
}

Test(fs2slice, arrslice_validate_null_parent_nonzero_fails) {
    fs2arrslice bad = { .parent = NULL, .ranges = (void*)0xdeadbeef,
                        .hwm = 1, .size = 1, .count = 1 };
    CHECK_FALSE("bad canonical", fs2arrslice_validate(NULL, &bad));
}

Test(fs2slice, arrslice_validate_count_gt_hwm_fails) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("abc");
    fs2sliceview v[] = { {0,3} };
    fs2arrslice a = mk_arrslice(&parent, v, 1);
    a.count = 5;                                            /* corrupt */
    CHECK_FALSE("count > hwm", fs2arrslice_validate(NULL, &a));
    a.count = 1;
    fs2arrslice_free(&a);
    fs2_free(&parent);
    assert_balanced();
}

Test(fs2slice, arrslice_validate_bad_range_fails) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("abc");
    fs2sliceview v[] = { {0,3}, {2, 2} };                   /* second is empty */
    fs2arrslice a = mk_arrslice(&parent, v, 2);
    CHECK_FALSE("empty range", fs2arrslice_validate(NULL, &a));

    a.ranges[1] = (fs2sliceview){0, 100};                    /* out of range */
    CHECK_FALSE("out of range", fs2arrslice_validate(NULL, &a));

    a.ranges[1] = (fs2sliceview){1, 3};                      /* restore */
    CHECK_TRUE("restored", fs2arrslice_validate(NULL, &a));

    fs2arrslice_free(&a);
    fs2_free(&parent);
    assert_balanced();
}

/* =========================================================================
 * fs2slice_techfprint
 * ========================================================================= */

Test(fs2slice, techfprint_basic) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("hello world");
    fs2slice s = fs2slice_new(&parent, 6, 11);              /* "world" */

    FILE *f = test_tmpfile(__func__);
    cr_assert_not_null(f);

    long w = fs2slice_techfprint(f, &s, "test");
    cr_assert_gt(w, 0);

    rewind(f);
    char buf[512] = {0};
    size_t rd = test_fread(buf, sizeof buf - 1, f, __func__);
    fclose(f);
    buf[rd] = '\0';

    cr_assert(strstr(buf, "FS2SLICE: test:") != NULL, "got: %s", buf);
    cr_assert(strstr(buf, "count[1]")        != NULL, "got: %s", buf);
    cr_assert(strstr(buf, "size[1]")         != NULL, "got: %s", buf);
    cr_assert(strstr(buf, "[6, 11)")         != NULL, "got: %s", buf);   /* ← убрал "range" */
    cr_assert(strstr(buf, "[world]")         != NULL, "got: %s", buf);

    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

Test(fs2slice, techfprint_null_s) {
    FILE *f = test_tmpfile(__func__);
    long w = fs2slice_techfprint(f, NULL, "n");
    cr_assert_gt(w, 0);
    rewind(f);
    char buf[128] = {0};
    test_fread(buf, sizeof buf - 1, f, __func__);
    fclose(f);
    cr_assert(strstr(buf, "<NULL>") != NULL);
}

Test(fs2slice, techfprint_zombie) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("x");
    fs2slice s = fs2slice_new(&parent, 0, 1);
    fs2slice_free(&s);

    FILE *f = test_tmpfile(__func__);
    long w = fs2slice_techfprint(f, &s, "z");
    cr_assert_gt(w, 0);
    rewind(f);
    char buf[128] = {0};
    test_fread(buf, sizeof buf - 1, f, __func__);
    fclose(f);
    cr_assert(strstr(buf, "<zombie>") != NULL);

    fs2_free(&parent);
    assert_balanced();
}

Test(fs2slice, techfprint_null_out_returns_zero) {
    fs2slice s = FS2SLICE();
    CHECK_EQ("null out", 0, fs2slice_techfprint(NULL, &s, "x"));
}

/* =========================================================================
 * fs2arrslice_techfprint
 * ========================================================================= */

Test(fs2slice, arrslice_techfprint_basic) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("a,bb,ccc");
    fs2sliceview v[] = { {0,1}, {2,4}, {5,8} };
    fs2arrslice a = mk_arrslice(&parent, v, 3);

    FILE *f = test_tmpfile(__func__);
    long w = fs2arrslice_techfprint(f, &a, "cells");
    cr_assert_gt(w, 0);

    rewind(f);
    char buf[1024] = {0};
    test_fread(buf, sizeof buf - 1, f, __func__);
    fclose(f);

    cr_assert(strstr(buf, "FS2ARRSLICE: cells:") != NULL, "got: %s", buf);
    cr_assert(strstr(buf, "count[3]")            != NULL);
    cr_assert(strstr(buf, "[0] [0, 1) [a]")      != NULL);
    cr_assert(strstr(buf, "[1] [2, 4) [bb]")     != NULL);
    cr_assert(strstr(buf, "[2] [5, 8) [ccc]")    != NULL);

    fs2arrslice_free(&a);
    fs2_free(&parent);
    assert_balanced();
}

Test(fs2slice, arrslice_techfprint_canonical) {
    fs2arrslice a = FS2ARRSLICE();
    FILE *f = test_tmpfile(__func__);
    long w = fs2arrslice_techfprint(f, &a, "empty");
    cr_assert_gt(w, 0);
    rewind(f);
    char buf[128] = {0};
    test_fread(buf, sizeof buf - 1, f, __func__);
    fclose(f);
    cr_assert(strstr(buf, "<empty>") != NULL);
}

Test(fs2slice, arrslice_techfprint_truncation) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("0123456789ABCDEF");           /* 16 bytes */
    /* 12 single-char ranges — more than FS2SLICE_TECH_PRINT_COUNT (8) */
    fs2sliceview v[12];
    for (int i = 0; i < 12; i++)
        v[i] = (fs2sliceview){ (size_t)i, (size_t)i + 1 };
    fs2arrslice a = mk_arrslice(&parent, v, 12);

    FILE *f = test_tmpfile(__func__);
    fs2arrslice_techfprint(f, &a, "many");
    rewind(f);
    char buf[2048] = {0};
    test_fread(buf, sizeof buf - 1, f, __func__);
    fclose(f);

    cr_assert(strstr(buf, "more") != NULL, "truncation marker: %s", buf);

    /* exactly FS2SLICE_TECH_PRINT_COUNT range lines */
    size_t n_lines = 0;
    for (const char *p = buf; (p = strstr(p, "  [")) != NULL; p++)
        n_lines++;
    cr_assert_eq(n_lines, FS2SLICE_TECH_PRINT_COUNT);

    fs2arrslice_free(&a);
    fs2_free(&parent);
    assert_balanced();
}

/* macros — smoke */
Test(fs2slice, techfprint_macros) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("hi");
    fs2slice s = fs2slice_new(&parent, 0, 2);

    FILE *f = test_tmpfile(__func__);
    long w = FS2SLICETECHFPRINT(f, &s);           /* name = "&s" */
    cr_assert_gt(w, 0);
    rewind(f);
    char buf[256] = {0};
    test_fread(buf, sizeof buf - 1, f, __func__);
    fclose(f);
    cr_assert(strstr(buf, "FS2SLICE: &s:") != NULL, "got: %s", buf);

    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

TestSuite(fs2_freeze, .init = test_setup, .fini = test_teardown);

/* =========================================================================
 * Helper: compare slice content (bounded, no NUL assumption)
 * ========================================================================= */


/* =========================================================================
 * ALLOWED — len not reduced; slice sees live changes (leech)
 * ========================================================================= */

Test(fs2_freeze, toupper_allowed_slice_sees_change) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("hello");
    fs2slice s  = fs2slice_new(&parent, 0, 5);
    cr_assert_eq(parent.borrow, 1);

    fs2_toupper(&parent);                       /* bytes mutate, len unchanged */

    CHECK_TRUE("slice sees uppercase", slice_eq(&s, "HELLO"));
    CHECK_EQ  ("borrow unchanged",     1, parent.borrow);
    CHECK_TRUE("slice valid",          fs2slice_validate(NULL, &s));

    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

Test(fs2_freeze, tolower_allowed) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("HELLO");
    fs2slice s  = fs2slice_new(&parent, 0, 5);

    fs2_tolower(&parent);

    CHECK_TRUE("slice sees lowercase", slice_eq(&s, "hello"));

    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

Test(fs2_freeze, sort_allowed_slice_sees_change) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("12345cba56789");
    fs2slice s  = fs2slice_new(&parent, 5, 8);

    fs2_sort(&parent, true);

    CHECK_TRUE("slice sees sorted", slice_eq(&s, "567"));

    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

Test(fs2_freeze, partsort_allowed_slice_sees_change) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("12345cba56789");
    fs2slice s  = fs2slice_new(&parent, 5, 8);   /* "cba" */

    fs2_partialsort(&parent, 5, 8, true);          /* ← только область slice’а */

    CHECK_TRUE("slice sees sorted", slice_eq(&s, "abc"));
    CHECK_STR_EQ("prefix intact", "12345", ( (char[]) {parent.v[0], parent.v[1], parent.v[2], parent.v[3], parent.v[4],0} ) );
    CHECK_STR_EQ("suffix intact", "56789", parent.v + 8);

    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

Test(fs2_freeze, flip_allowed_slice_sees_change) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("abc");
    fs2slice s  = fs2slice_new(&parent, 0, 3);

    fs2_flip(&parent);

    CHECK_TRUE("slice sees reversed", slice_eq(&s, "cba"));

    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

Test(fs2_freeze, cat_grows_slice_follows_realloc) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("hello");
    fs2slice s  = fs2slice_new(&parent, 0, 5);

    fs2_catstr(&parent, " world");              /* may realloc */

    CHECK_TRUE("slice still 'hello'", slice_eq(&s, "hello"));
    CHECK_EQ  ("borrow",              1, parent.borrow);

    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

Test(fs2_freeze, reserve_realloc_slice_follows) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("hi");
    fs2slice s  = fs2slice_new(&parent, 0, 2);

    fs2_reserve(&parent, 1000);                 /* forced realloc */

    CHECK_TRUE("slice still valid", slice_eq(&s, "hi"));

    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

Test(fs2_freeze, setlen_same_len_allowed) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("abc");
    fs2slice s  = fs2slice_new(&parent, 0, 3);

    fs2_setlen(&parent, 3);                     /* no-op (pos == len) */

    CHECK_TRUE("slice still 'abc'", slice_eq(&s, "abc"));

    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

Test(fs2_freeze, read_only_allowed) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("hello");
    fs2slice s  = fs2slice_new(&parent, 0, 5);
    fs2 needle  = fs2_fromstr("ll");

    CHECK_EQ("chr",   fs2_chr(&parent, 'l'),       2);
    CHECK_EQ("instr", fs2_instr(&parent, &needle), 2);
    CHECK_EQ("cmp",   fs2_cmpstr(&parent, "hello"), 0);

    fs2slice_free(&s);
    fs2_free(&parent);
    fs2_free(&needle);
    assert_balanced();
}

/* =========================================================================
 * FORBIDDEN — len shrinks or parent dies; userraise → SIGINT
 * ========================================================================= */

Test(fs2_freeze, free_blocked, .signal = SIGINT) {
    fs2 parent = fs2_fromstr("hello");
    fs2slice s  = fs2slice_new(&parent, 0, 5);
    (void) s;
    fs2_free(&parent);
    /* unreachable */
}

Test(fs2_freeze, move_src_blocked, .signal = SIGINT) {
    fs2 src = fs2_fromstr("hello");
    fs2slice s = fs2slice_new(&src, 0, 5);
    (void) s;
    fs2 dst = FS2();
    fs2_move(&dst, &src);                       /* src borrowed → SIGINT */
}

Test(fs2_freeze, move_dst_blocked, .signal = SIGINT) {
    fs2 dst = fs2_fromstr("world");
    fs2slice s = fs2slice_new(&dst, 0, 5);
    (void) s;
    fs2 src = fs2_fromstr("hello");
    fs2_move(&dst, &src);                       /* dst borrowed → SIGINT */
}

Test(fs2_freeze, exch_a_blocked, .signal = SIGINT) {
    fs2 a = fs2_fromstr("aaa");
    fs2slice s = fs2slice_new(&a, 0, 3);
    (void) s;
    fs2 b = fs2_fromstr("bbb");
    fs2_exch(&a, &b);
}

Test(fs2_freeze, exch_b_blocked, .signal = SIGINT) {
    fs2 a = fs2_fromstr("aaa");
    fs2 b = fs2_fromstr("bbb");
    fs2slice s = fs2slice_new(&b, 0, 3);
    (void) s;
    fs2_exch(&a, &b);
}

Test(fs2_freeze, movefrom_blocked, .signal = SIGINT) {
    fs2 src = fs2_fromstr("hello");
    fs2slice s = fs2slice_new(&src, 0, 5);
    fs2 x = fs2_movefrom(&src);                 /* src borrowed → SIGINT */
    (void) s; (void) x;
}

Test(fs2_freeze, setlen_shrink_blocked, .signal = SIGINT) {
    fs2 parent = fs2_fromstr("hello");
    fs2slice s = fs2slice_new(&parent, 0, 5);
    (void) s;
    fs2_setlen(&parent, 3);                     /* shrink → SIGINT */
}

Test(fs2_freeze, clear_blocked, .signal = SIGINT) {
    fs2 parent = fs2_fromstr("hello");
    fs2slice s = fs2slice_new(&parent, 0, 5);
    (void) s;
    fs2_clear(&parent);                         /* setlen(0) → SIGINT */
}

Test(fs2_freeze, substr_blocked, .signal = SIGINT) {
    fs2 parent = fs2_fromstr("hello");
    fs2slice s = fs2slice_new(&parent, 0, 5);
    (void) s;
    fs2_substr(&parent, 1, 3);                  /* memmove shrink → SIGINT */
}

Test(fs2_freeze, left_blocked, .signal = SIGINT) {
    fs2 parent = fs2_fromstr("hello");
    fs2slice s = fs2slice_new(&parent, 0, 5);
    (void) s;
    fs2_left(&parent, 3);
}

Test(fs2_freeze, right_blocked, .signal = SIGINT) {
    fs2 parent = fs2_fromstr("hello");
    fs2slice s = fs2slice_new(&parent, 0, 5);
    (void) s;
    fs2_right(&parent, 3);
}

Test(fs2_freeze, trim_blocked, .signal = SIGINT) {
    fs2 parent = fs2_fromstr("  hi  ");
    fs2slice s = fs2slice_new(&parent, 0, 6);
    (void) s;
    fs2 pat = fs2_fromstr(" ");
    fs2_trim(&parent, &pat);
    fs2_free(&pat);
}

Test(fs2_freeze, strip_prefix_blocked, .signal = SIGINT) {
    fs2 parent = fs2_fromstr("hello");
    fs2slice s = fs2slice_new(&parent, 0, 5);
    (void) s;
    fs2 prefix = fs2_fromstr("he");
    fs2_strip_prefix(&parent, &prefix);
    fs2_free(&prefix);
}

Test(fs2_freeze, strip_suffix_blocked, .signal = SIGINT) {
    fs2 parent = fs2_fromstr("hello");
    fs2slice s = fs2slice_new(&parent, 0, 5);
    (void) s;
    fs2 suffix = fs2_fromstr("lo");
    fs2_strip_suffix(&parent, &suffix);
    fs2_free(&suffix);
}

Test(fs2_freeze, cpy_blocked, .signal = SIGINT) {
    fs2 dst = fs2_fromstr("hello");
    fs2slice s = fs2slice_new(&dst, 0, 5);
    (void) s;
    fs2 src = fs2_fromstr("hi");
    fs2_cpy(&dst, &src);                        /* clear → setlen(0) → SIGINT */
}

Test(fs2_freeze, cpystr_blocked, .signal = SIGINT) {
    fs2 dst = fs2_fromstr("hello");
    fs2slice s = fs2slice_new(&dst, 0, 5);
    (void) s;
    fs2_cpystr(&dst, "hi");
}

Test(fs2_freeze, sprintf_shrink_blocked, .signal = SIGINT) {
    fs2 parent = fs2_fromstr("hello world");    /* 11 chars */
    fs2slice s = fs2slice_new(&parent, 0, 11);
    (void) s;
    fs2_sprintf(&parent, "%d", 42);             /* result "42" (2 < 11) → SIGINT */
}

/* =========================================================================
 * TRY-catch: userraise is RECOVERABLE
 *   Demonstrate that catch + free-slice + retry works.
 * ========================================================================= */

Test(fs2_freeze, try_free_catch_and_retry) {
    __fs2_checkreset();
    err_resetenv();

    fs2 parent = fs2_fromstr("hello");
    fs2slice s  = fs2slice_new(&parent, 0, 5);
    bool caught = false;

    TRY() {
        fs2_free(&parent);
        cr_assert_fail("expected userraise");
    } else {
        caught = true;
    }
    err_resetenv();

    cr_assert(caught, "free should raise on borrowed fs2");

    /* parent intact: guard fires BEFORE free(v) */
    CHECK_NOT_NULL("parent alive", parent.v);
    CHECK_EQ  ("len unchanged",   5, parent.len);
    CHECK_EQ  ("borrow",          1, parent.borrow);
    CHECK_TRUE("slice valid",     fs2slice_validate(NULL, &s));

    /* free slices, retry succeeds */
    fs2slice_free(&s);
    cr_assert_eq(parent.borrow, 0);
    fs2_free(&parent);

    assert_balanced();
}

Test(fs2_freeze, try_substr_catch_and_retry) {
    __fs2_checkreset();
    err_resetenv();

    fs2 parent = fs2_fromstr("hello");
    fs2slice s  = fs2slice_new(&parent, 0, 5);
    bool caught = false;

    TRY() {
        fs2_substr(&parent, 1, 3);              /* "ell" — shrink to 3 */
        cr_assert_fail("expected userraise");
    } else {
        caught = true;
    }
    err_resetenv();

    cr_assert(caught);

    /* parent unchanged: memmove guard fires before any byte shift */
    CHECK_STR_EQ("content intact", "hello", parent.v);
    CHECK_EQ  ("len unchanged",    5, parent.len);
    CHECK_TRUE("slice valid",      fs2slice_validate(NULL, &s));

    fs2slice_free(&s);

    /* retry succeeds */
    fs2_substr(&parent, 1, 3);
    CHECK_STR_EQ("ell", parent.v, "ell");

    fs2_free(&parent);
    assert_balanced();
}

Test(fs2_freeze, try_catch_preserves_error_code) {
    __fs2_checkreset();
    err_resetenv();

    fs2 parent = fs2_fromstr("hello");
    fs2slice s  = fs2slice_new(&parent, 0, 5);

    TRY() {
        fs2_free(&parent);
    } else {
        /* caught */
    }
    err_resetenv();

    ErrorInfo info = {0};
    cr_assert(  err_last(&info), "expected error record");
    cr_assert_eq(info.type, ERR_USER);
    cr_assert_eq(info.code, ERR_FS2SLICE_BORROWED);

    fs2slice_free(&s);
    fs2_free(&parent);
    err_clean(true);
    assert_balanced();
}

/* =========================================================================
 * fs2slice_cmp — slice vs slice
 * ========================================================================= */

Test(fs2slice, cmp_equal_different_parents) {
    __fs2_checkreset();
    fs2 p1 = fs2_fromstr("hello");
    fs2 p2 = fs2_fromstr("xhellox");
    fs2slice a = fs2slice_new(&p1, 0, 5);   /* "hello" */
    fs2slice b = fs2slice_new(&p2, 1, 6);   /* "hello" */

    CHECK_EQ("equal content, different parents", 0, fs2slice_cmp(&a, &b));

    fs2slice_free(&a); fs2slice_free(&b);
    fs2_free(&p1);     fs2_free(&p2);
    assert_balanced();
}

Test(fs2slice, cmp_same_parent_different_ranges) {
    __fs2_checkreset();
    fs2 p = fs2_fromstr("abcabc");
    fs2slice a = fs2slice_new(&p, 0, 3);    /* "abc" */
    fs2slice b = fs2slice_new(&p, 3, 6);    /* "abc" */

    CHECK_EQ("same content same parent", 0, fs2slice_cmp(&a, &b));

    fs2slice_free(&a); fs2slice_free(&b);
    fs2_free(&p);
    assert_balanced();
}

Test(fs2slice, cmp_prefix_shorter_is_less) {
    __fs2_checkreset();
    fs2 p = fs2_fromstr("abcdef");
    fs2slice short_ = fs2slice_new(&p, 0, 3);   /* "abc" */
    fs2slice long_  = fs2slice_new(&p, 0, 6);   /* "abcdef" */

    CHECK_LT("abc < abcdef", fs2slice_cmp(&short_, &long_),  0);
    CHECK_GT("abcdef > abc", fs2slice_cmp(&long_, &short_),  0);

    fs2slice_free(&short_); fs2slice_free(&long_);
    fs2_free(&p);
    assert_balanced();
}

Test(fs2slice, cmp_byte_order) {
    __fs2_checkreset();
    fs2 p = fs2_fromstr("acb");
    fs2slice a = fs2slice_new(&p, 0, 1);   /* "a" */
    fs2slice b = fs2slice_new(&p, 1, 2);   /* "c" */
    fs2slice c = fs2slice_new(&p, 2, 3);   /* "b" */

    CHECK_LT("a < c", fs2slice_cmp(&a, &b), 0);
    CHECK_LT("b < c", fs2slice_cmp(&c, &b), 0);
    CHECK_LT("a < b", fs2slice_cmp(&a, &c), 0);

    fs2slice_free(&a); fs2slice_free(&b); fs2slice_free(&c);
    fs2_free(&p);
    assert_balanced();
}

/* =========================================================================
 * fs2slice_cmpfs2 — slice vs fs2
 * ========================================================================= */

Test(fs2slice, cmpfs2_equal) {
    __fs2_checkreset();
    fs2 p = fs2_fromstr("xhellox");
    fs2slice a = fs2slice_new(&p, 1, 6);       /* "hello" */
    fs2 s = fs2_fromstr("hello");

    CHECK_EQ("equal", 0, fs2slice_cmpfs2(&a, &s));

    fs2slice_free(&a);
    fs2_free(&p); fs2_free(&s);
    assert_balanced();
}

Test(fs2slice, cmpfs2_less_than_longer) {
    __fs2_checkreset();
    fs2 p = fs2_fromstr("abc");
    fs2slice a = fs2slice_new(&p, 0, 2);       /* "ab" */
    fs2 s = fs2_fromstr("abc");

    CHECK_LT("ab < abc", fs2slice_cmpfs2(&a, &s), 0);

    fs2slice_free(&a);
    fs2_free(&p);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2slice, cmpfs2_antisymmetric) {
    __fs2_checkreset();
    fs2 p = fs2_fromstr("abc");
    fs2slice ab  = fs2slice_new(&p, 0, 2);     /* "ab" */
    fs2slice abc = fs2slice_new(&p, 0, 3);     /* "abc" */
    fs2 s_ab  = fs2_fromstr("ab");
    fs2 s_abc = fs2_fromstr("abc");

    int r1 = fs2slice_cmpfs2(&ab,  &s_abc);    /* "ab" vs "abc"  → <0 */
    int r2 = fs2slice_cmpfs2(&abc, &s_ab);     /* "abc" vs "ab"  → >0 */
    CHECK_LT("ab < abc", r1, 0);
    CHECK_GT("abc > ab", r2, 0);

    fs2slice_free(&ab); fs2slice_free(&abc);
    fs2_free(&p);
    fs2_free(&s_ab); fs2_free(&s_abc);
    assert_balanced();
}

Test(fs2slice, cmpfs2_vs_canonical_empty) {
    __fs2_checkreset();
    fs2 p = fs2_fromstr("a");
    fs2slice a = fs2slice_new(&p, 0, 1);
    fs2 empty = FS2();                          /* len=0, v=NULL */

    CHECK_GT("nonempty > canonical empty", fs2slice_cmpfs2(&a, &empty), 0);

    fs2slice_free(&a);
    fs2_free(&p); fs2_free(&empty);
    assert_balanced();
}

Test(fs2slice, cmpfs2_vs_fs2empty) {
    __fs2_checkreset();
    fs2 p = fs2_fromstr("a");
    fs2slice a = fs2slice_new(&p, 0, 1);
    fs2 empty = FS2EMPTY;

    CHECK_GT("nonempty > FS2EMPTY", fs2slice_cmpfs2(&a, &empty), 0);

    fs2slice_free(&a);
    fs2_free(&p);
    assert_balanced();
}

/* =========================================================================
 * fs2slice_cmpstr — slice vs c-string
 * ========================================================================= */

Test(fs2slice, cmpstr_equal) {
    __fs2_checkreset();
    fs2 p = fs2_fromstr("xhellox");
    fs2slice a = fs2slice_new(&p, 1, 6);

    CHECK_EQ("hello", 0, fs2slice_cmpstr(&a, "hello"));

    fs2slice_free(&a);
    fs2_free(&p);
    assert_balanced();
}

Test(fs2slice, cmpstr_prefix_both_directions) {
    __fs2_checkreset();
    fs2 p = fs2_fromstr("abcdef");
    fs2slice a = fs2slice_new(&p, 0, 3);        /* "abc" */

    CHECK_LT("abc < abcd", fs2slice_cmpstr(&a, "abcd"), 0);
    CHECK_GT("abc > ab",   fs2slice_cmpstr(&a, "ab"),   0);

    fs2slice_free(&a);
    fs2_free(&p);
    assert_balanced();
}

Test(fs2slice, cmpstr_vs_empty_str) {
    __fs2_checkreset();
    fs2 p = fs2_fromstr("a");
    fs2slice a = fs2slice_new(&p, 0, 1);

    CHECK_GT("nonempty > \"\"", fs2slice_cmpstr(&a, ""), 0);

    fs2slice_free(&a);
    fs2_free(&p);
    assert_balanced();
}

/* =========================================================================
 * CI variants
 * ========================================================================= */

Test(fs2slice, cmp_ci_case_insensitive) {
    __fs2_checkreset();
    fs2 p1 = fs2_fromstr("HELLO");
    fs2 p2 = fs2_fromstr("hello");
    fs2slice a = fs2slice_new(&p1, 0, 5);
    fs2slice b = fs2slice_new(&p2, 0, 5);

    CHECK_EQ("ci: HELLO == hello", 0, fs2slice_cmp_ci(&a, &b));
    CHECK_NEQ("cs: HELLO != hello", 0, fs2slice_cmp(&a, &b));

    fs2slice_free(&a); fs2slice_free(&b);
    fs2_free(&p1);     fs2_free(&p2);
    assert_balanced();
}

Test(fs2slice, cmpfs2_ci) {
    __fs2_checkreset();
    fs2 p = fs2_fromstr("Hello");
    fs2slice a = fs2slice_new(&p, 0, 5);
    fs2 s = fs2_fromstr("HELLO");

    CHECK_EQ("ci equal", 0, fs2slice_cmpfs2_ci(&a, &s));
    CHECK_NEQ("cs differs", 0, fs2slice_cmpfs2(&a, &s));

    fs2slice_free(&a);
    fs2_free(&p); fs2_free(&s);
    assert_balanced();
}

Test(fs2slice, cmpstr_ci) {
    __fs2_checkreset();
    fs2 p = fs2_fromstr("Hello");
    fs2slice a = fs2slice_new(&p, 0, 5);

    CHECK_EQ("ci", 0, fs2slice_cmpstr_ci(&a, "HELLO"));

    fs2slice_free(&a);
    fs2_free(&p);
    assert_balanced();
}

/* =========================================================================
 * Guards
 * ========================================================================= */

Test(fs2slice, cmp_null_a_aborts, .signal = SIGABRT) {
    fs2 p = fs2_fromstr("a");
    fs2slice b = fs2slice_new(&p, 0, 1);
    fs2slice_cmp(NULL, &b);
    fs2slice_free(&b);
    fs2_free(&p);
}

Test(fs2slice, cmp_zombie_aborts, .signal = SIGABRT) {
    fs2 p = fs2_fromstr("a");
    fs2slice a = fs2slice_new(&p, 0, 1);

    fs2slice b = a;                        /* copy */
    fs2slice_free(&b);                      /* b.parent = NULL */

    fs2slice_cmp(&a, &b);

    fs2_free(&p);
}

Test(fs2slice, cmpstr_null_str_aborts, .signal = SIGABRT) {
    fs2 p = fs2_fromstr("a");
    fs2slice a = fs2slice_new(&p, 0, 1);
    fs2slice_cmpstr(&a, NULL);
    fs2slice_free(&a);
    fs2_free(&p);
}

Test(fs2slice, cmpfs2_null_s_aborts, .signal = SIGABRT) {
    fs2 p = fs2_fromstr("a");
    fs2slice a = fs2slice_new(&p, 0, 1);
    fs2slice_cmpfs2(&a, NULL);
    fs2slice_free(&a);
    fs2_free(&p);
}

/* =========================================================================
 * fs2slice_get — nominal
 * ========================================================================= */

Test(fs2slice, get_reads_within_slice) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("0123456789");
    fs2slice s = fs2slice_new(&parent, 2, 7);      /* "23456" */

    CHECK_EQ("pos 0",    '2', *fs2slice_get(&s, 0));
    CHECK_EQ("pos 2",    '4', *fs2slice_get(&s, 2));
    CHECK_EQ("pos 4",    '6', *fs2slice_get(&s, 4));

    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

Test(fs2slice, get_macro_convention) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("hello");
    fs2slice s = fs2slice_new(&parent, 1, 4);      /* "ell" */

    CHECK_EQ("s[0]", 'e', fs2sliceget(s, 0));
    CHECK_EQ("s[2]", 'l', fs2sliceget(s, 2));

    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

/* =========================================================================
 * fs2slice_get — window boundaries
 *
 * Slice [2, 7) of "0123456789" is "23456":
 *   pos 0..4 → parent[2..6] = '2'..'6'     (valid)
 *   pos 5    → parent[7]   = '7'           (outside slice → abort)
 *   pos 10   → parent[12]  = out of range  (abort)
 * ========================================================================= */

Test(fs2slice, get_pos_at_slice_len_aborts, .signal = SIGINT) {
    fs2 parent = fs2_fromstr("0123456789");
    fs2slice s = fs2slice_new(&parent, 2, 7);
    fs2slice_get(&s, 5);                            /* == slice_len */
    fs2slice_free(&s);
    fs2_free(&parent);
}

Test(fs2slice, get_pos_beyond_slice_len_aborts, .signal = SIGINT) {
    fs2 parent = fs2_fromstr("0123456789");
    fs2slice s = fs2slice_new(&parent, 2, 7);
    fs2slice_get(&s, 100);
    fs2slice_free(&s);
    fs2_free(&parent);
}

Test(fs2slice, get_pos_one_past_parent_range_aborts, .signal = SIGINT) {
    fs2 parent = fs2_fromstr("abc");                /* len=3 */
    fs2slice s = fs2slice_new(&parent, 0, 3);
    fs2slice_get(&s, 3);                            /* == slice_len == parent_len */
    fs2slice_free(&s);
    fs2_free(&parent);
}

/* =========================================================================
 * fs2slice_get — guards
 * ========================================================================= */

Test(fs2slice, get_null_slice_aborts, .signal = SIGABRT) {
    fs2slice_get(NULL, 0);
}

Test(fs2slice, get_zombie_slice_aborts, .signal = SIGINT) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("abc");
    fs2slice s = fs2slice_new(&parent, 0, 3);
    fs2slice_free(&s);
    fs2slice_get(&s, 0);                            /* s.parent == NULL */
    fs2_free(&parent);
    assert_balanced();
}

/* =========================================================================
 * fs2slice_get — leech: reads current parent->v after realloc/mutate
 * ========================================================================= */

Test(fs2slice, get_after_parent_realloc) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("hello");
    fs2slice s = fs2slice_new(&parent, 0, 5);
    char *p_before = fs2slice_get(&s, 0);
    CHECK_EQ("'h'", 'h', *p_before);

    fs2_catstr(&parent, " world");                  /* realloc */
    CHECK_EQ("'h' after realloc", 'h', *fs2slice_get(&s, 0));
    CHECK_EQ("'o' after realloc", 'o', *fs2slice_get(&s, 4));

    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

Test(fs2slice, get_sees_toupper) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("hello");
    fs2slice s = fs2slice_new(&parent, 0, 5);
    CHECK_EQ("'h'", 'h', *fs2slice_get(&s, 0));

    fs2_toupper(&parent);                           /* bytes mutate, len same */
    CHECK_EQ("'H' via leech", 'H', *fs2slice_get(&s, 0));

    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

/* =========================================================================
 * fs2slice_get — write via pointer (read-only concern)
 *
 * `fs2slice_get` returns `char *` (non-const) — matches fs2_get convention.
 * Writing through it is UB **by contract** (slice is a view, not owner).
 * We do NOT test the write itself (UB); we just verify the API shape
 * matches fs2_get (mutable pointer).
 * ========================================================================= */

Test(fs2slice, get_returns_mutable_ptr_for_api_symmetry) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("abc");
    fs2slice s = fs2slice_new(&parent, 0, 3);

    char *p = fs2slice_get(&s, 1);
    /* shape check — not dereferencing for write */
    CHECK_PTR_EQ("parent->v + from + pos",
                 parent.v + s.range.from + 1, p);

    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

/* =========================================================================
 * fs2slice_substr_new — nominal (offsets relative to slice)
 * ========================================================================= */

Test(fs2slice, substr_new_middle_of_slice) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("0123456789");
    fs2slice s = fs2slice_new(&parent, 3, 8);        /* "34567" */

    fs2 r = fs2slice_substr_new(&s, 1, 3);           /* "456" */

    CHECK_STR_EQ("456", r.v, "456");
    CHECK_EQ  ("len",   3, r.len);
    CHECK_TRUE("ALLOC", fs2_is_alloc(&r));
    CHECK_TRUE("independent", r.v != parent.v);
    CHECK_FS2VALID(1, &r);
    CHECK_FS2VALID(2, &parent);

    fs2_free(&r);
    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

Test(fs2slice, substr_new_first_bytes) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("0123456789");
    fs2slice s = fs2slice_new(&parent, 3, 8);        /* "34567" */

    fs2 r = fs2slice_substr_new(&s, 0, 2);           /* "34" */

    CHECK_STR_EQ("34", r.v, "34");

    fs2_free(&r);
    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

Test(fs2slice, substr_new_full_slice) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("0123456789");
    fs2slice s = fs2slice_new(&parent, 3, 8);        /* "34567" */

    fs2 r = fs2slice_substr_new(&s, 0, fs2slice_len(&s));

    CHECK_STR_EQ("34567", r.v, "34567");
    CHECK_EQ  ("len",     5, r.len);

    fs2_free(&r);
    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

/* =========================================================================
 * fs2slice_substr_new — clamping against slice_len (NOT parent->len)
 * ========================================================================= */

Test(fs2slice, substr_new_len_overshoot_clamped_to_slice) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("0123456789");          /* parent_len = 10 */
    fs2slice s = fs2slice_new(&parent, 3, 8);        /* slice_len = 5 */

    fs2 r = fs2slice_substr_new(&s, 0, 100);         /* clamped to 5, not 10 */

    CHECK_STR_EQ("34567", r.v, "34567");             /* NOT "3456789" */
    CHECK_EQ  ("len",     5, r.len);

    fs2_free(&r);
    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

Test(fs2slice, substr_new_from_past_slice_end_empty) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("0123456789");
    fs2slice s = fs2slice_new(&parent, 3, 8);

    fs2 r1 = fs2slice_substr_new(&s, 5, 1);          /* from == slice_len */
    CHECK_TRUE("from==len empty", fs2_is_empty(&r1));
    fs2_free(&r1);

    fs2 r2 = fs2slice_substr_new(&s, 100, 1);        /* from > slice_len */
    CHECK_TRUE("from>len empty", fs2_is_empty(&r2));
    fs2_free(&r2);

    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

Test(fs2slice, substr_new_len_zero_empty) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("hello");
    fs2slice s = fs2slice_new(&parent, 1, 4);

    fs2 r = fs2slice_substr_new(&s, 1, 0);

    CHECK_TRUE("len=0 → empty", fs2_is_empty(&r));

    fs2_free(&r);
    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

Test(fs2slice, substr_new_partial_len_at_tail) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("0123456789");
    fs2slice s = fs2slice_new(&parent, 3, 8);        /* "34567" */

    fs2 r = fs2slice_substr_new(&s, 3, 100);         /* from=3, clamp len=2 → "67" */

    CHECK_STR_EQ("67", r.v, "67");
    CHECK_EQ  ("len",  2, r.len);

    fs2_free(&r);
    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

/* =========================================================================
 * fs2slice_substr_new — parent untouched / independent
 * ========================================================================= */

Test(fs2slice, substr_new_independent_from_parent) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("abcdef");
    fs2slice s = fs2slice_new(&parent, 1, 5);        /* "bcde" */

    fs2 r = fs2slice_substr_new(&s, 1, 2);           /* "cd" */

    CHECK_TRUE("independent buffer", r.v != parent.v);
    r.v[0] = 'X';
    CHECK_STR_EQ("parent intact", "abcdef", parent.v);
    CHECK_TRUE ("slice sees parent",  slice_eq(&s, "bcde"));   /* ← было CHECK_STR_EQ + fs2slice_data */
    CHECK_STR_EQ("result intact",    "Xd", r.v);

    fs2_free(&r);
    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

/* Leech: extraction reads CURRENT parent buffer (realloc-safe) */
Test(fs2slice, substr_new_after_parent_realloc) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("hello");
    fs2slice s = fs2slice_new(&parent, 1, 4);        /* "ell" */

    fs2_catstr(&parent, " world");                    /* realloc */

    fs2 r = fs2slice_substr_new(&s, 0, 3);           /* still "ell" */

    CHECK_STR_EQ("ell", r.v, "ell");

    fs2_free(&r);
    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

/* =========================================================================
 * fs2slice_substr_new — guards
 * ========================================================================= */

Test(fs2slice, substr_new_null_aborts, .signal = SIGABRT) {
    fs2slice_substr_new(NULL, 0, 1);
}

Test(fs2slice, substr_new_zombie_aborts, .signal = SIGINT) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("abc");
    fs2slice s = fs2slice_new(&parent, 0, 3);
    fs2slice_free(&s);
    fs2slice_substr_new(&s, 0, 1);
    fs2_free(&parent);
    assert_balanced();
}

/* =========================================================================
 * fs2slice_to_owned / fs2slice_clone
 * ========================================================================= */

Test(fs2slice, to_owned_basic) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("xhellox");
    fs2slice s = fs2slice_new(&parent, 1, 6);        /* "hello" */

    fs2 r = fs2slice_to_owned(&s);

    CHECK_STR_EQ("hello",     r.v, "hello");
    CHECK_EQ  ("len",         5, r.len);
    CHECK_TRUE("ALLOC",       fs2_is_alloc(&r));
    CHECK_TRUE("independent", r.v != parent.v);

    fs2_free(&r);
    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

Test(fs2slice, to_owned_equals_substr_full) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("abcdef");
    fs2slice s = fs2slice_new(&parent, 2, 5);        /* "cde" */

    fs2 a = fs2slice_to_owned(&s);
    fs2 b = fs2slice_substr_new(&s, 0, fs2slice_len(&s));

    CHECK_STR_EQ("same content", a.v, b.v);
    CHECK_EQ  ("same len",       a.len, b.len);

    fs2_free(&a);
    fs2_free(&b);
    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

Test(fs2slice, to_owned_null_aborts, .signal = SIGABRT) {
    fs2slice_to_owned(NULL);
}

/* =========================================================================
 * fs2slice_left_new
 * ========================================================================= */

Test(fs2slice, left_new_partial) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("0123456789");
    fs2slice s = fs2slice_new(&parent, 3, 8);        /* "34567" */

    fs2 r = fs2slice_left_new(&s, 3);                /* "345" */

    CHECK_STR_EQ("345", r.v, "345");

    fs2_free(&r);
    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

Test(fs2slice, left_new_full_eq_owned) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("abcdef");
    fs2slice s = fs2slice_new(&parent, 1, 4);        /* "bcd" */

    fs2 r = fs2slice_left_new(&s, 3);                /* == "bcd" */

    CHECK_STR_EQ("bcd", r.v, "bcd");

    fs2_free(&r);
    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

Test(fs2slice, left_new_overshoot_clamps) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("abcdef");
    fs2slice s = fs2slice_new(&parent, 1, 4);        /* "bcd" */

    fs2 r = fs2slice_left_new(&s, 100);              /* clamped → "bcd" */

    CHECK_STR_EQ("bcd", r.v, "bcd");

    fs2_free(&r);
    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

Test(fs2slice, left_new_zero_empty) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("abc");
    fs2slice s = fs2slice_new(&parent, 0, 3);

    fs2 r = fs2slice_left_new(&s, 0);

    CHECK_TRUE("empty", fs2_is_empty(&r));

    fs2_free(&r);
    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

/* =========================================================================
 * fs2slice_right_new
 * ========================================================================= */

Test(fs2slice, right_new_partial) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("0123456789");
    fs2slice s = fs2slice_new(&parent, 3, 8);        /* "34567" */

    fs2 r = fs2slice_right_new(&s, 3);               /* "567" */

    CHECK_STR_EQ("567", r.v, "567");

    fs2_free(&r);
    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

Test(fs2slice, right_new_full_eq_owned) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("abcdef");
    fs2slice s = fs2slice_new(&parent, 1, 4);        /* "bcd" */

    fs2 r = fs2slice_right_new(&s, 3);               /* == "bcd" */

    CHECK_STR_EQ("bcd", r.v, "bcd");

    fs2_free(&r);
    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

Test(fs2slice, right_new_overshoot_clamps) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("abcdef");
    fs2slice s = fs2slice_new(&parent, 1, 4);        /* "bcd" */

    fs2 r = fs2slice_right_new(&s, 100);             /* clamped → "bcd" */

    CHECK_STR_EQ("bcd", r.v, "bcd");

    fs2_free(&r);
    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

Test(fs2slice, right_new_zero_empty) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("abc");
    fs2slice s = fs2slice_new(&parent, 0, 3);

    fs2 r = fs2slice_right_new(&s, 0);

    CHECK_TRUE("empty", fs2_is_empty(&r));

    fs2_free(&r);
    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

/* =========================================================================
 * Integration: split → substr → prefix/suffix processing
 * ========================================================================= */

// Test(fs2slice, split_then_substr_extract) {
//     __fs2_checkreset();
//     fs2 csv = fs2_fromstr("alpha,beta,gamma");
//     fs2 comma = fs2_fromstr(",");
//     fs2arrslice cells = fs2arrslice_split(&csv, &comma);
//     cr_assert_eq(cells.count, 3);

//     fs2slice cell1 = fs2arrslice_get(&cells, 1);     /* "beta" */

//     fs2 mid = fs2slice_substr_new(&cell1, 1, 2);     /* "et" */
//     CHECK_STR_EQ("et", mid.v, "et");

//     fs2 head = fs2slice_left_new(&cell1, 2);         /* "be" */
//     CHECK_STR_EQ("be", head.v, "be");

//     fs2 tail = fs2slice_right_new(&cell1, 2);        /* "ta" */
//     CHECK_STR_EQ("ta", tail.v, "ta");

//     fs2_free(&mid);
//     fs2_free(&head);
//     fs2_free(&tail);
//     fs2slice_free(&cell1);
//     fs2arrslice_free(&cells);
//     fs2_free(&csv);
//     fs2_free(&comma);
//     assert_balanced();
// }

/* =========================================================================
 * fs2slice_toupper_new
 * ========================================================================= */

Test(fs2slice, toupper_new_basic) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("xhellox");
    fs2slice s = fs2slice_new(&parent, 1, 6);        /* "hello" */

    fs2 r = fs2slice_toupper_new(&s);

    CHECK_STR_EQ("HELLO",     r.v, "HELLO");
    CHECK_EQ  ("len",         5, r.len);
    CHECK_TRUE("ALLOC",       fs2_is_alloc(&r));
    CHECK_TRUE("independent", r.v != parent.v);
    CHECK_FS2VALID(1, &r);

    /* parent + slice untouched */
    CHECK_STR_EQ("parent intact", "xhellox", parent.v);
    CHECK_TRUE ("slice intact",    slice_eq(&s, "hello"));
    CHECK_FS2VALID(2, &parent);

    fs2_free(&r);
    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

Test(fs2slice, toupper_new_mixed_case) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("__HeLLo WoRLD__");
    fs2slice s = fs2slice_new(&parent, 2, 13);       /* "HeLLo WoRLD" */

    fs2 r = fs2slice_toupper_new(&s);

    CHECK_STR_EQ("HELLO WORLD", r.v, "HELLO WORLD");

    fs2_free(&r);
    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

Test(fs2slice, toupper_new_already_upper_noop) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("HELLO");
    fs2slice s = fs2slice_new(&parent, 0, 5);

    fs2 r = fs2slice_toupper_new(&s);

    CHECK_STR_EQ("HELLO", r.v, "HELLO");

    fs2_free(&r);
    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

Test(fs2slice, toupper_new_nonletters_unchanged) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("a1b2!c3");
    fs2slice s = fs2slice_new(&parent, 0, 7);

    fs2 r = fs2slice_toupper_new(&s);

    CHECK_STR_EQ("A1B2!C3", r.v, "A1B2!C3");

    fs2_free(&r);
    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

Test(fs2slice, toupper_new_single_char) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("xax");
    fs2slice s = fs2slice_new(&parent, 1, 2);        /* "a" */

    fs2 r = fs2slice_toupper_new(&s);

    CHECK_STR_EQ("A", r.v, "A");
    CHECK_EQ  ("len", 1, r.len);

    fs2_free(&r);
    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

/* =========================================================================
 * fs2slice_tolower_new
 * ========================================================================= */

Test(fs2slice, tolower_new_basic) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("xHELLOx");
    fs2slice s = fs2slice_new(&parent, 1, 6);        /* "HELLO" */

    fs2 r = fs2slice_tolower_new(&s);

    CHECK_STR_EQ("hello",     r.v, "hello");
    CHECK_EQ  ("len",         5, r.len);
    CHECK_TRUE("independent", r.v != parent.v);
    CHECK_FS2VALID(1, &r);

    /* parent + slice untouched */
    CHECK_STR_EQ("parent intact", "xHELLOx", parent.v);
    CHECK_TRUE ("slice intact",    slice_eq(&s, "HELLO"));

    fs2_free(&r);
    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

Test(fs2slice, tolower_new_mixed_case) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("HeLLo WoRLD");
    fs2slice s = fs2slice_new(&parent, 0, 11);

    fs2 r = fs2slice_tolower_new(&s);

    CHECK_STR_EQ("hello world", r.v, "hello world");

    fs2_free(&r);
    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

Test(fs2slice, tolower_new_already_lower_noop) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("hello");
    fs2slice s = fs2slice_new(&parent, 0, 5);

    fs2 r = fs2slice_tolower_new(&s);

    CHECK_STR_EQ("hello", r.v, "hello");

    fs2_free(&r);
    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

/* =========================================================================
 * Consistency with fs2_toupper_new / fs2_tolower_new equivalents
 * ========================================================================= */

Test(fs2slice, toupper_new_equals_owned_then_toupper) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("abcdef");
    fs2slice s = fs2slice_new(&parent, 1, 5);        /* "bcde" */

    fs2 a = fs2slice_toupper_new(&s);                /* via slice helper */

    fs2 tmp = fs2slice_to_owned(&s);                 /* manual chain */
    fs2 b = tmp;
    fs2_toupper(&b);

    CHECK_STR_EQ("same content", a.v, b.v);
    CHECK_EQ  ("same len",       a.len, b.len);

    fs2_free(&a);
    fs2_free(&tmp);
    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

/* Leech: extraction reads CURRENT parent buffer (realloc-safe) */
Test(fs2slice, toupper_new_after_parent_realloc) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("hello");
    fs2slice s = fs2slice_new(&parent, 0, 5);

    fs2_catstr(&parent, " world");                    /* realloc */

    fs2 r = fs2slice_toupper_new(&s);                 /* still "HELLO" */

    CHECK_STR_EQ("HELLO", r.v, "HELLO");

    fs2_free(&r);
    fs2slice_free(&s);
    fs2_free(&parent);
    assert_balanced();
}

/* =========================================================================
 * Guards
 * ========================================================================= */

Test(fs2slice, toupper_new_null_aborts, .signal = SIGABRT) {
    fs2slice_toupper_new(NULL);
}

Test(fs2slice, toupper_new_zombie_aborts, .signal = SIGABRT) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("abc");
    fs2slice s = fs2slice_new(&parent, 0, 3);
    fs2slice_free(&s);
    fs2slice_toupper_new(&s);
    fs2_free(&parent);
    assert_balanced();
}

Test(fs2slice, tolower_new_null_aborts, .signal = SIGABRT) {
    fs2slice_tolower_new(NULL);
}

Test(fs2slice, tolower_new_zombie_aborts, .signal = SIGABRT) {
    __fs2_checkreset();
    fs2 parent = fs2_fromstr("abc");
    fs2slice s = fs2slice_new(&parent, 0, 3);
    fs2slice_free(&s);
    fs2slice_tolower_new(&s);
    fs2_free(&parent);
    assert_balanced();
}

/* =========================================================================
 * Integration: split → toupper cell
 * ========================================================================= */

// Test(fs2slice, split_then_toupper_cell) {
//     __fs2_checkreset();
//     fs2 csv = fs2_fromstr("alpha,beta,gamma");
//     fs2 comma = fs2_fromstr(",");
//     fs2arrslice cells = fs2arrslice_split(&csv, &comma);
//     cr_assert_eq(cells.count, 3);

//     fs2slice cell1 = fs2arrslice_get(&cells, 1);     /* "beta" */
//     fs2 upper = fs2slice_toupper_new(&cell1);

//     CHECK_STR_EQ("BETA", upper.v, "BETA");
//     CHECK_STR_EQ("original still lowercase", csv.v, "alpha,beta,gamma");

//     fs2_free(&upper);
//     fs2slice_free(&cell1);
//     fs2arrslice_free(&cells);
//     fs2_free(&csv);
//     fs2_free(&comma);
//     assert_balanced();
// }

