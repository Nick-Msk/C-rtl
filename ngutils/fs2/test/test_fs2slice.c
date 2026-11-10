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
