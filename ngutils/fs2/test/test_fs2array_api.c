#include <criterion/criterion.h>

#include "test_fs2.h"

#include "fs2array.h"
#include "fs2array_api.h"
#include "fs2_api.h"

TestSuite(fs2_array_api, .init = test_setup, .fini = test_teardown);

/* =========================================================================
 * fs2_split_toarray — nominal
 * ========================================================================= */

Test(fs2_array_api, split_basic_three_parts) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("a,b,c");
    fs2 d = fs2_fromstr(",");

    fs2array arr = fs2_split_toarray(&s, &d);

    CHECK_EQ  ("count", 3, arr.count);
    CHECK_EQ  ("hwm",   3, arr.hwm);            /* packed */
    CHECK_STR_EQ("[0]", "a", arr.arr[0].v);
    CHECK_STR_EQ("[1]", "b", arr.arr[1].v);
    CHECK_STR_EQ("[2]", "c", arr.arr[2].v);
    CHECK_FALSE("[0] not hole", fs2_is_hole(&arr.arr[0]));

    fs2arr_free(&arr);
    fs2_free(&s);       // must free that
    fs2_free(&d);       // and that
    assert_balanced();
}

Test(fs2_array_api, split_no_delimiter_single_part) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");
    fs2 d = fs2_fromstr(",");

    fs2array arr = fs2_split_toarray(&s, &d);

    CHECK_EQ  ("count", 1, arr.count);
    CHECK_STR_EQ("[0]", "hello", arr.arr[0].v);

    fs2arr_free(&arr);
    fs2_free(&s);
    fs2_free(&d);
    assert_balanced();
}

Test(fs2_array_api, split_multi_char_delimiter) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("one::two::three");
    fs2 d = fs2_fromstr("::");

    fs2array arr = fs2_split_toarray(&s, &d);

    CHECK_EQ  ("count", 3, arr.count);
    CHECK_STR_EQ("[0]", "one",   arr.arr[0].v);
    CHECK_STR_EQ("[1]", "two",   arr.arr[1].v);
    CHECK_STR_EQ("[2]", "three", arr.arr[2].v);

    fs2arr_free(&arr);
    fs2_free(&s);
    fs2_free(&d);
    assert_balanced();
}

/* =========================================================================
 * fs2_split_toarray — empty parts preserved
 * ========================================================================= */

Test(fs2_array_api, split_consecutive_delims_empty_parts) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("a,,b");
    fs2 d = fs2_fromstr(",");

    fs2array arr = fs2_split_toarray(&s, &d);

    CHECK_EQ  ("count", 3, arr.count);
    CHECK_STR_EQ("[0]", "a", arr.arr[0].v);
    CHECK_STR_EQ("[1]", "",  arr.arr[1].v);     /* empty middle */
    CHECK_STR_EQ("[2]", "b", arr.arr[2].v);
    CHECK_FALSE("[1] not hole", fs2_is_hole(&arr.arr[1]));  /* live empty */
    CHECK_TRUE ("[1] FS2EMPTY", fs2_eq_empty(&arr.arr[1]));

    fs2arr_free(&arr);
    fs2_free(&s);
    fs2_free(&d);
    assert_balanced();
}

Test(fs2_array_api, split_leading_delimiter_empty_first) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr(",abc");
    fs2 d = fs2_fromstr(",");

    fs2array arr = fs2_split_toarray(&s, &d);

    CHECK_EQ  ("count", 2, arr.count);
    CHECK_STR_EQ("[0]", "",    arr.arr[0].v);
    CHECK_STR_EQ("[1]", "abc", arr.arr[1].v);

    fs2arr_free(&arr);
    fs2_free(&s);
    fs2_free(&d);
    assert_balanced();
}

Test(fs2_array_api, split_trailing_delimiter_empty_last) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc,");
    fs2 d = fs2_fromstr(",");

    fs2array arr = fs2_split_toarray(&s, &d);

    CHECK_EQ  ("count", 2, arr.count);
    CHECK_STR_EQ("[0]", "abc", arr.arr[0].v);
    CHECK_STR_EQ("[1]", "",    arr.arr[1].v);

    fs2arr_free(&arr);
    fs2_free(&s);
    fs2_free(&d);
    assert_balanced();
}

Test(fs2_array_api, split_only_delimiters) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr(",,");
    fs2 d = fs2_fromstr(",");

    fs2array arr = fs2_split_toarray(&s, &d);
    /* Python: ",".split(",") → ["",""]; ",,".split(",") → ["","",""] */

    CHECK_EQ  ("count", 3, arr.count);
    CHECK_STR_EQ("[0]", "", arr.arr[0].v);
    CHECK_STR_EQ("[1]", "", arr.arr[1].v);
    CHECK_STR_EQ("[2]", "", arr.arr[2].v);

    fs2arr_free(&arr);
    fs2_free(&s);
    fs2_free(&d);
    assert_balanced();
}

/* =========================================================================
 * fs2_split_toarray — empty source / empty delimiter
 * ========================================================================= */

Test(fs2_array_api, split_empty_src_nonempty_delim_one_empty_part) {
    __fs2_checkreset();
    fs2 s = FS2EMPTY;
    fs2 d = fs2_fromstr(",");

    fs2array arr = fs2_split_toarray(&s, &d);

    /* Python: "".split(",") → [""] */
    CHECK_EQ  ("count", 1, arr.count);
    CHECK_EQ  ("[0] len", arr.arr[0].len, 0);

    fs2arr_free(&arr);
    fs2_free(&d);
    assert_balanced();
}

Test(fs2_array_api, split_canon_src_nonempty_delim) {
    __fs2_checkreset();
    fs2 s = FS2();                            /* v == NULL */
    fs2 d = fs2_fromstr(",");

    fs2array arr = fs2_split_toarray(&s, &d);

    CHECK_EQ  ("count", 1, arr.count);
    CHECK_TRUE("[0] empty", fs2_is_empty(&arr.arr[0]));

    fs2arr_free(&arr);
    fs2_free(&s);
    fs2_free(&d);
    assert_balanced();
}

Test(fs2_array_api, split_empty_delim_no_split) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");
    fs2 d = FS2EMPTY;

    fs2array arr = fs2_split_toarray(&s, &d);

    CHECK_EQ  ("count", 1, arr.count);
    CHECK_STR_EQ("[0]", "abc", arr.arr[0].v);

    fs2arr_free(&arr);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_array_api, split_empty_delim_empty_src) {
    __fs2_checkreset();
    fs2 s = FS2EMPTY;
    fs2 d = FS2EMPTY;

    fs2array arr = fs2_split_toarray(&s, &d);

    CHECK_EQ("count", 1, arr.count);
    CHECK_TRUE("[0] empty", fs2_is_empty(&arr.arr[0]));

    fs2arr_free(&arr);
    assert_balanced();
}

/* =========================================================================
 * fs2_split_toarray — non-overlapping semantics
 * ========================================================================= */

Test(fs2_array_api, split_non_overlapping_aaa) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("aaa");
    fs2 d = fs2_fromstr("aa");

    fs2array arr = fs2_split_toarray(&s, &d);

    /* Python: 'aaa'.split('aa') → ['', 'a'] */
    CHECK_EQ  ("count", 2, arr.count);
    CHECK_STR_EQ("[0]", "",  arr.arr[0].v);
    CHECK_STR_EQ("[1]", "a", arr.arr[1].v);

    fs2arr_free(&arr);
    fs2_free(&s);
    fs2_free(&d);
    assert_balanced();
}

Test(fs2_array_api, split_non_overlapping_aaaa) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("aaaa");
    fs2 d = fs2_fromstr("aa");

    fs2array arr = fs2_split_toarray(&s, &d);

    /* Python: 'aaaa'.split('aa') → ['', '', ''] */
    CHECK_EQ("count", 3, arr.count);
    CHECK_STR_EQ("[0]", "", arr.arr[0].v);
    CHECK_STR_EQ("[1]", "", arr.arr[1].v);
    CHECK_STR_EQ("[2]", "", arr.arr[2].v);

    fs2arr_free(&arr);
    fs2_free(&s);
    fs2_free(&d);
    assert_balanced();
}

/* =========================================================================
 * fs2_split_toarray — content independence (deep copy)
 * ========================================================================= */

Test(fs2_array_api, split_parts_are_independent) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("aa,bb");
    fs2 d = fs2_fromstr(",");

    fs2array arr = fs2_split_toarray(&s, &d);
    cr_assert_eq(arr.count, 2);

    CHECK_TRUE("[0] independent", arr.arr[0].v != s.v);
    CHECK_TRUE("[0] buf != [1] buf", arr.arr[0].v != arr.arr[1].v);

    /* Mutating src doesn't affect parts */
    s.v[0] = 'X';
    CHECK_STR_EQ("[0] intact", "aa", arr.arr[0].v);
    CHECK_STR_EQ("[1] intact", "bb", arr.arr[1].v);

    fs2arr_free(&arr);
    fs2_free(&s);
    fs2_free(&d);
    assert_balanced();
}

/* =========================================================================
 * fs2_split_toarray — consistency with fs2_split_part
 * ========================================================================= */

Test(fs2_array_api, split_toarray_matches_split_part) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("a,bb,,ccc");
    fs2 d = fs2_fromstr(",");

    fs2array arr = fs2_split_toarray(&s, &d);

    cr_assert_eq(arr.count, 4);               /* ["a","bb","","ccc"] */

    for (size_t i = 0; i < arr.count; i++) {
        fs2 part = fs2_split_part(&s, &d, (ptrdiff_t)(i + 1));
        CHECK_STR_EQ("split_part matches", arr.arr[i].v, part.v);
        fs2_free(&part);
    }

    fs2arr_free(&arr);
    fs2_free(&s);
    fs2_free(&d);
    assert_balanced();
}

/* =========================================================================
 * fs2_split_toarray_str — C-string delimiter
 * ========================================================================= */

Test(fs2_array_api, split_str_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("x,y,z");

    fs2array arr = fs2_split_toarray_str(&s, ",");

    CHECK_EQ  ("count", 3, arr.count);
    CHECK_STR_EQ("[0]", "x", arr.arr[0].v);
    CHECK_STR_EQ("[2]", "z", arr.arr[2].v);

    fs2arr_free(&arr);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_array_api, split_str_null_delim_single_part) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");

    fs2array arr = fs2_split_toarray_str(&s, NULL);  /* NULL → FS2EMPTY → single part */

    CHECK_EQ  ("count", 1, arr.count);
    CHECK_STR_EQ("[0]", "abc", arr.arr[0].v);

    fs2arr_free(&arr);
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_split_toarray — guards
 * ========================================================================= */

Test(fs2_array_api, split_null_s_aborts, .signal = SIGABRT) {
    fs2 d = fs2_fromstr(",");
    fs2_split_toarray(NULL, &d);
    fs2_free(&d);
}

Test(fs2_array_api, split_null_delim_aborts, .signal = SIGABRT) {
    fs2 s = fs2_fromstr("a");
    fs2_split_toarray(&s, NULL);
    fs2_free(&s);
}

/* =========================================================================
 * fs2_split_toarray — invariant
 * ========================================================================= */

Test(fs2_array_api, split_result_is_valid_array) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("x,y,,z");
    fs2 d = fs2_fromstr(",");

    fs2array arr = fs2_split_toarray(&s, &d);

    /* Packed and valid */
    CHECK_TRUE("valid", fs2arr_validate(NULL, &arr));
    CHECK_EQ  ("hwm == count", arr.hwm, arr.count);

    fs2arr_free(&arr);
    fs2_free(&s);
    fs2_free(&d);
    assert_balanced();
}

TestSuite(fs2_array_sort, .init = test_setup, .fini = test_teardown);

/* v == NULL live slot — can only appear via manual insertion.
 * Normal fs2arr_push uses fs2_move which promotes canonical → FS2EMPTY. */
static void
push_null_manual(fs2array *a) {
    fs2arr_reserve(a, a->hwm + 1);
    a->arr[a->hwm] = FS2();          /* canonical: v=NULL */
    a->hwm++;
    a->count++;
}

/* Custom comparator: length asc, lex tiebreak — proves typed API works. */
static int
cmp_by_len(const void *pa, const void *pb) {
    const fs2 *a = pa, *b = pb;
    if (a->len < b->len) return -1;
    if (a->len > b->len) return +1;
    return fs2_cmp(a, b);
}

/* =========================================================================
 * Base fs2arr_sort — typed comparator, asc/desc on packed
 * ========================================================================= */

Test(fs2_array_sort, sortasc_basic) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    const char *vals[] = { "c", "a", "b" };
    for (int i = 0; i < 3; i++) { fs2 v = fs2_fromstr(vals[i]); fs2arr_push(&a, &v); }

    fs2arr_sortasc(&a);

    CHECK_STR_EQ("[0]", "a", a.arr[0].v);
    CHECK_STR_EQ("[1]", "b", a.arr[1].v);
    CHECK_STR_EQ("[2]", "c", a.arr[2].v);

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array_sort, sortdesc_basic) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    const char *vals[] = { "b", "c", "a" };
    for (int i = 0; i < 3; i++) { fs2 v = fs2_fromstr(vals[i]); fs2arr_push(&a, &v); }

    fs2arr_sortdesc(&a);

    CHECK_STR_EQ("[0]", "c", a.arr[0].v);
    CHECK_STR_EQ("[1]", "b", a.arr[1].v);
    CHECK_STR_EQ("[2]", "a", a.arr[2].v);

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array_sort, sort_custom_cmp_len) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    const char *vals[] = { "ccc", "a", "bb", "dddd" };
    for (int i = 0; i < 4; i++) { fs2 v = fs2_fromstr(vals[i]); fs2arr_push(&a, &v); }

    fs2arr_sort(&a, cmp_by_len);

    CHECK_STR_EQ("[0]", "a",    a.arr[0].v);
    CHECK_STR_EQ("[1]", "bb",   a.arr[1].v);
    CHECK_STR_EQ("[2]", "ccc",  a.arr[2].v);
    CHECK_STR_EQ("[3]", "dddd", a.arr[3].v);

    fs2arr_free(&a);
    assert_balanced();
}

/* =========================================================================
 * fs2arr_sort — compact before sort (holes at end after)
 * ========================================================================= */

Test(fs2_array_sort, sortasc_nullafter_packs_holes) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    const char *vals[] = { "e", "d", "c", "b", "a" };
    for (int i = 0; i < 5; i++) { fs2 v = fs2_fromstr(vals[i]); fs2arr_push(&a, &v); }
    fs2 t = fs2arr_detach_new(&a, 2);   /* hole at [2] */
    fs2_free(&t);
    /* [e, d, hole, b, a], hwm=5, count=4 */

    fs2arr_sortasc_nullafter(&a);       /* holes → [count, hwm), live → [0, count) sorted */

    CHECK_EQ  ("hwm",   5, a.hwm);
    CHECK_EQ  ("count", 4, a.count);
    CHECK_STR_EQ("[0]", "a", a.arr[0].v);
    CHECK_STR_EQ("[1]", "b", a.arr[1].v);
    CHECK_STR_EQ("[2]", "d", a.arr[2].v);
    CHECK_STR_EQ("[3]", "e", a.arr[3].v);
    CHECK_TRUE ("[4] hole", fs2_is_hole(&a.arr[4]));

    /* After: packed state ([0, count) live, [count, hwm) holes) */
    for (size_t i = 0; i < a.count; i++)
        CHECK_FALSE("[i] live", fs2_is_hole(&a.arr[i]));

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array_sort, sortdesc_nullbefore_packs_holes) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    const char *vals[] = { "e", "d", "c", "b", "a" };
    for (int i = 0; i < 5; i++) { fs2 v = fs2_fromstr(vals[i]); fs2arr_push(&a, &v); }
    fs2 t = fs2arr_detach_new(&a, 2);
    fs2_free(&t);
    /* [e, d, hole, b, a], hwm=5, count=4 */

    fs2arr_sortdesc_nullbefore(&a);     /* desc, holes → end */

    CHECK_EQ  ("hwm",   5, a.hwm);
    CHECK_EQ  ("count", 4, a.count);
    CHECK_STR_EQ("[0]", "e", a.arr[0].v);
    CHECK_STR_EQ("[1]", "d", a.arr[1].v);
    CHECK_STR_EQ("[2]", "b", a.arr[2].v);
    CHECK_STR_EQ("[3]", "a", a.arr[3].v);
    CHECK_TRUE ("[4] hole", fs2_is_hole(&a.arr[4]));

    fs2arr_free(&a);
    assert_balanced();
}

/* =========================================================================
 * fs2arr_sort — no-op cases
 * ========================================================================= */

Test(fs2_array_sort, sort_canonical_noop) {
    fs2array a = FS2ARR();
    fs2arr_sortasc(&a);                 /* no crash, no alloc */
    CHECK_PTR_EQ("arr", NULL, a.arr);
    CHECK_EQ   ("count", 0, a.count);
}

Test(fs2_array_sort, sort_empty_capacity_noop) {
    fs2array a = fs2arr_init(16);       /* arr != NULL, count=0 */
    fs2arr_sortasc(&a);
    CHECK_EQ("count", 0, a.count);
    fs2arr_free(&a);
}

Test(fs2_array_sort, sort_single_element_noop) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(4);
    fs2 v = fs2_fromstr("x");
    fs2arr_push(&a, &v);

    fs2arr_sortasc(&a);

    CHECK_STR_EQ("[0]", "x", a.arr[0].v);
    CHECK_EQ  ("count", 1, a.count);

    fs2arr_free(&a);
    assert_balanced();
}

/* =========================================================================
 * Duplicates — multiset preserved
 * ========================================================================= */

Test(fs2_array_sort, sort_duplicates_preserved) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    const char *vals[] = { "b", "a", "b", "a", "b" };
    for (int i = 0; i < 5; i++) { fs2 v = fs2_fromstr(vals[i]); fs2arr_push(&a, &v); }

    fs2arr_sortasc(&a);

    CHECK_STR_EQ("[0]", "a", a.arr[0].v);
    CHECK_STR_EQ("[1]", "a", a.arr[1].v);
    CHECK_STR_EQ("[2]", "b", a.arr[2].v);
    CHECK_STR_EQ("[3]", "b", a.arr[3].v);
    CHECK_STR_EQ("[4]", "b", a.arr[4].v);

    fs2arr_free(&a);
    assert_balanced();
}

/* =========================================================================
 * Size / hwm invariants
 * ========================================================================= */

Test(fs2_array_sort, sort_preserves_size_hwm) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(16);
    fs2 v1 = fs2_fromstr("c"), v2 = fs2_fromstr("a"), v3 = fs2_fromstr("b");
    fs2arr_push(&a, &v1); fs2arr_push(&a, &v2); fs2arr_push(&a, &v3);
    size_t sz0 = a.size, hwm0 = a.hwm, cnt0 = a.count;

    fs2arr_sortasc(&a);

    CHECK_EQ("size",  sz0,  a.size);
    CHECK_EQ("hwm",   hwm0, a.hwm);
    CHECK_EQ("count", cnt0, a.count);

    fs2arr_free(&a);
    assert_balanced();
}

/* =========================================================================
 * nullbefore: null < everything
 *   sortasc_nullbefore  → nulls FIRST,  values asc
 *   sortdesc_nullbefore → nulls LAST,   values desc
 * ========================================================================= */

Test(fs2_array_sort, sortasc_nullbefore) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    fs2 v1 = fs2_fromstr("m"), v2 = fs2_fromstr("a"), v3 = fs2_fromstr("z");
    fs2arr_push(&a, &v1);
    push_null_manual(&a);
    fs2arr_push(&a, &v2);
    fs2arr_push(&a, &v3);
    /* [m, null, a, z] */

    fs2arr_sortasc_nullbefore(&a);

    CHECK_TRUE ("[0] null",  a.arr[0].v == NULL);
    CHECK_STR_EQ("[1]", "a", a.arr[1].v);
    CHECK_STR_EQ("[2]", "m", a.arr[2].v);
    CHECK_STR_EQ("[3]", "z", a.arr[3].v);

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array_sort, sortdesc_nullbefore) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    fs2 v1 = fs2_fromstr("m"), v2 = fs2_fromstr("a"), v3 = fs2_fromstr("z");
    fs2arr_push(&a, &v1);
    push_null_manual(&a);
    fs2arr_push(&a, &v2);
    fs2arr_push(&a, &v3);
    /* [m, null, a, z] */

    fs2arr_sortdesc_nullbefore(&a);

    /* desc = nulls LAST, values desc */
    CHECK_STR_EQ("[0]", "z", a.arr[0].v);
    CHECK_STR_EQ("[1]", "m", a.arr[1].v);
    CHECK_STR_EQ("[2]", "a", a.arr[2].v);
    CHECK_TRUE ("[3] null",  a.arr[3].v == NULL);

    fs2arr_free(&a);
    assert_balanced();
}

/* =========================================================================
 * nullafter: null > everything
 *   sortasc_nullafter  → nulls LAST,  values asc
 *   sortdesc_nullafter → nulls FIRST, values desc
 * ========================================================================= */

Test(fs2_array_sort, sortasc_nullafter) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    fs2 v1 = fs2_fromstr("m"), v2 = fs2_fromstr("a"), v3 = fs2_fromstr("z");
    push_null_manual(&a);
    fs2arr_push(&a, &v1);
    fs2arr_push(&a, &v2);
    fs2arr_push(&a, &v3);
    /* [null, m, a, z] */

    fs2arr_sortasc_nullafter(&a);

    /* asc nulls LAST */
    CHECK_STR_EQ("[0]", "a", a.arr[0].v);
    CHECK_STR_EQ("[1]", "m", a.arr[1].v);
    CHECK_STR_EQ("[2]", "z", a.arr[2].v);
    CHECK_TRUE ("[3] null",  a.arr[3].v == NULL);

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array_sort, sortdesc_nullafter) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    fs2 v1 = fs2_fromstr("m"), v2 = fs2_fromstr("a"), v3 = fs2_fromstr("z");
    push_null_manual(&a);
    fs2arr_push(&a, &v1);
    fs2arr_push(&a, &v2);
    fs2arr_push(&a, &v3);
    /* [null, m, a, z] */

    fs2arr_sortdesc_nullafter(&a);

    /* desc = nulls FIRST, values desc */
    CHECK_TRUE ("[0] null",  a.arr[0].v == NULL);
    CHECK_STR_EQ("[1]", "z", a.arr[1].v);
    CHECK_STR_EQ("[2]", "m", a.arr[2].v);
    CHECK_STR_EQ("[3]", "a", a.arr[3].v);

    fs2arr_free(&a);
    assert_balanced();
}

/* =========================================================================
 * Multiple nulls — stability of null-to-null comparisons
 * ========================================================================= */

Test(fs2_array_sort, sortasc_nullbefore_multiple_nulls) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    push_null_manual(&a);
    fs2 v1 = fs2_fromstr("m"); fs2arr_push(&a, &v1);
    push_null_manual(&a);
    fs2 v2 = fs2_fromstr("a"); fs2arr_push(&a, &v2);
    push_null_manual(&a);
    fs2 v3 = fs2_fromstr("z"); fs2arr_push(&a, &v3);
    /* [null, m, null, a, null, z] — 3 nulls, 3 values */

    fs2arr_sortasc_nullbefore(&a);

    CHECK_TRUE ("[0] null", a.arr[0].v == NULL);
    CHECK_TRUE ("[1] null", a.arr[1].v == NULL);
    CHECK_TRUE ("[2] null", a.arr[2].v == NULL);
    CHECK_STR_EQ("[3]", "a", a.arr[3].v);
    CHECK_STR_EQ("[4]", "m", a.arr[4].v);
    CHECK_STR_EQ("[5]", "z", a.arr[5].v);

    fs2arr_free(&a);
    assert_balanced();
}

/* =========================================================================
 * Guards
 * ========================================================================= */

Test(fs2_array_sort, sort_null_arr_aborts, .signal = SIGABRT) {
    fs2arr_sort(NULL, fs2_cmp_wrap);
}

Test(fs2_array_sort, sort_null_cmp_aborts, .signal = SIGABRT) {
    fs2array a = fs2arr_init(2);
    fs2arr_sort(&a, NULL);
    fs2arr_free(&a);
}

/* =========================================================================
 * Idempotency — sorting twice gives same result
 * ========================================================================= */

Test(fs2_array_sort, sortasc_idempotent) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    const char *vals[] = { "c", "a", "b", "b", "a" };
    for (int i = 0; i < 5; i++) { fs2 v = fs2_fromstr(vals[i]); fs2arr_push(&a, &v); }

    fs2arr_sortasc(&a);
    fs2array snapshot = fs2arr_cloneexact(&a);

    fs2arr_sortasc(&a);

    CHECK_TRUE("idempotent", fs2arr_equalexact(&a, &snapshot));

    fs2arr_free(&a);
    fs2arr_free(&snapshot);
    assert_balanced();
}

/* =========================================================================
 * fs2arr_compact — nominal
 * ========================================================================= */

Test(fs2_array_api, compact_packs_live_to_front) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    const char *vals[] = { "A", "B", "C", "D", "E" };
    for (int i = 0; i < 5; i++) { fs2 v = fs2_fromstr(vals[i]); fs2arr_push(&a, &v); }
    /* [A, B, C, D, E], hwm=5, count=5 */

    /* detach [1] and [3] — holes at mid */
    fs2 t1 = fs2arr_detach_new(&a, 3);   /* hole at [3], live [4]="E" */
    fs2 t2 = fs2arr_detach_new(&a, 1);   /* hole at [1], live [2]="C" */
    fs2_free(&t1); fs2_free(&t2);
    /* [A, hole, C, hole, E], hwm=5, count=3 */

    fs2arr_compact(&a);

    CHECK_EQ  ("hwm",   3, a.hwm);          
    CHECK_EQ  ("size",  a.size, a.size);
    CHECK_EQ  ("count", 3, a.count);

    /* [A, C, E, hole, hole] — stable order preserved */
    CHECK_STR_EQ("[0]", "A", a.arr[0].v);
    CHECK_STR_EQ("[1]", "C", a.arr[1].v);
    CHECK_STR_EQ("[2]", "E", a.arr[2].v);
    CHECK_TRUE ("[3] hole", fs2_is_hole(&a.arr[3]));
    CHECK_TRUE ("[4] hole", fs2_is_hole(&a.arr[4]));

    fs2arr_free(&a);
    assert_balanced();
}

/* No-op when already packed */
Test(fs2_array_api, compact_noop_on_packed) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    for (int i = 0; i < 3; i++) { fs2 v = fs2_from_new(i); fs2arr_push(&a, &v); }
    fs2 *orig = a.arr;
    size_t hwm0 = a.hwm, cnt0 = a.count;

    fs2arr_compact(&a);

    CHECK_PTR_EQ("arr unchanged", orig, a.arr);
    CHECK_EQ   ("hwm",   hwm0, a.hwm);
    CHECK_EQ   ("count", cnt0, a.count);
    CHECK_EQ   ("hwm",   3, a.hwm);            /* write == hwm, no change */
    CHECK_EQ   ("count", 3, a.count);
    CHECK_STR_EQ("[0]", "0", a.arr[0].v);
    CHECK_STR_EQ("[2]", "2", a.arr[2].v);

    fs2arr_free(&a);
    assert_balanced();
}

/* All holes: hwm unchanged, all [0, hwm) remain holes */
Test(fs2_array_api, compact_all_holes) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    fs2 v1 = fs2_fromstr("A");
    fs2 v2 = fs2_fromstr("B");
    fs2arr_push(&a, &v1);
    fs2arr_push(&a, &v2);
    fs2 t1 = fs2arr_detach_new(&a, 1);
    fs2 t2 = fs2arr_detach_new(&a, 0);
    fs2_free(&t1); fs2_free(&t2);
    /* [hole, hole], hwm=2, count=0 */

    fs2arr_compact(&a);

    CHECK_EQ  ("hwm",   0, a.hwm);
    CHECK_EQ  ("count", 0, a.count);
    CHECK_TRUE("[0] hole", fs2_is_hole(&a.arr[0]));
    CHECK_TRUE("[1] hole", fs2_is_hole(&a.arr[1]));

    fs2arr_free(&a);
    assert_balanced();
}

/* Canonical / empty with capacity — no-op */
Test(fs2_array_api, compact_canonical_noop) {
    fs2array canon = FS2ARR();
    fs2arr_compact(&canon);
    CHECK_PTR_EQ("arr", NULL, canon.arr);
}

Test(fs2_array_api, compact_empty_with_capacity_noop) {
    fs2array empty = fs2arr_init(16);
    fs2arr_compact(&empty);
    CHECK_EQ("hwm",   0, empty.hwm);
    CHECK_EQ("count", 0, empty.count);
    fs2arr_free(&empty);
}

/* Single hole at start, one live at end → live moves to [0] */
Test(fs2_array_api, compact_moves_single_live_from_end) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    fs2 v1 = fs2_fromstr("x");
    fs2 v2 = fs2_fromstr("y");
    fs2arr_push(&a, &v1);
    fs2arr_push(&a, &v2);
    fs2 t = fs2arr_detach_new(&a, 0);
    fs2_free(&t);
    /* [hole, "y"], hwm=2, count=1 */

    fs2arr_compact(&a);

    CHECK_EQ  ("hwm",   1, a.hwm);
    CHECK_STR_EQ("[0]", "y", a.arr[0].v);    /* live moved to front */
    CHECK_TRUE ("[1] hole", fs2_is_hole(&a.arr[1]));

    fs2arr_free(&a);
    assert_balanced();
}

/* =========================================================================
 * fs2arr_compact — integration: compact + synchwm + shrink = packed tight
 * ========================================================================= */

Test(fs2_array_api, compact_then_sync_then_shrink) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(16);
    const char *vals[] = { "A", "B", "C", "D", "E" };
    for (int i = 0; i < 5; i++) { fs2 v = fs2_fromstr(vals[i]); fs2arr_push(&a, &v); }
    fs2 t1 = fs2arr_detach_new(&a, 3);
    fs2 t2 = fs2arr_detach_new(&a, 1);
    fs2_free(&t1); fs2_free(&t2);
    /* [A, hole, C, hole, E], hwm=5, count=3, size>=16 */

    fs2arr_compact(&a);
    /* [A, C, E, hole, hole], hwm=5, count=3 */

    fs2arr_synchwm(&a);
    /* hwm trimmed to 3 (trailing holes [3],[4]) */

    fs2arr_shrink(&a);
    /* size → hwm = 3 */

    CHECK_EQ  ("hwm",   3, a.hwm);
    CHECK_EQ  ("size",  3, a.size);
    CHECK_EQ  ("count", 3, a.count);
    CHECK_STR_EQ("[0]", "A", a.arr[0].v);
    CHECK_STR_EQ("[1]", "C", a.arr[1].v);
    CHECK_STR_EQ("[2]", "E", a.arr[2].v);

    CHECK_TRUE("valid after cycle", fs2arr_validate(NULL, &a));

    fs2arr_free(&a);
    assert_balanced();
}

/* =========================================================================
 * fs2arr_compact — anti-double-free: no duplicate ownership
 * ========================================================================= */

/* If compact forgets fs2arr_set_hole on source, we'd get two slots pointing
 * at the same buffer. fs2arr_free would then double-free → ASan/crash.
 * This test exercises the exact path (multiple holes to skip). */
Test(fs2_array_api, compact_no_duplicate_ownership) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    for (int i = 0; i < 6; i++) { fs2 v = fs2_from_new(i); fs2arr_push(&a, &v); }
    /* [0,1,2,3,4,5] */

    /* detach [1], [2], [4] → 3 holes, 3 live */
    fs2 t1 = fs2arr_detach_new(&a, 4);   /* hole at [4] */
    fs2 t2 = fs2arr_detach_new(&a, 2);   /* hole at [2] */
    fs2 t3 = fs2arr_detach_new(&a, 1);   /* hole at [1] */
    fs2_free(&t1); fs2_free(&t2); fs2_free(&t3);
    /* [0, hole, hole, 3, hole, 5], hwm=6, count=3 */

    fs2arr_compact(&a);
    /* [0, 3, 5, hole, hole, hole] */

    /* Check no duplicate v pointers */
    for (size_t i = 0; i < a.count; i++) {
        cr_assert_not_null(a.arr[i].v, "[%zu] live slot must have v", i);
        for (size_t j = i + 1; j < a.count; j++) {
            cr_assert_neq(a.arr[i].v, a.arr[j].v,
                          "duplicate v at [%zu] and [%zu] — double-free risk", i, j);
        }
    }
    for (size_t i = a.count; i < a.hwm; i++)
        CHECK_TRUE("[i] hole", fs2_is_hole(&a.arr[i]));

    /* fs2arr_free would double-free if ownership was duplicated — the
     * absence of a crash / ASan report proves single ownership per slot. */
    fs2arr_free(&a);
    assert_balanced();
}

/* =========================================================================
 * fs2arr_compact — makes sort's plain comparators safe
 * ========================================================================= */

Test(fs2_array_api, compact_enables_sortasc_on_former_holes) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    const char *vals[] = { "e", "d", "c", "b", "a" };
    for (int i = 0; i < 5; i++) { fs2 v = fs2_fromstr(vals[i]); fs2arr_push(&a, &v); }
    fs2 t = fs2arr_detach_new(&a, 2);   /* hole at [2] */
    fs2_free(&t);
    /* [e, d, hole, b, a], hwm=5, count=4 */

    fs2arr_compact(&a);                 /* [e, d, b, a, hole] */
    /* Now [0, count) = [e, d, b, a] — all live, no holes */

    /* Sort only the live region [0, count) — but our fs2arr_sort uses
     * [0, hwm)... use explicit qsort or wait until synchwm */
    fs2arr_synchwm(&a);                 /* hwm → 4, trailing hole dropped */
    fs2arr_sortasc(&a);                 /* safe: hwm == count == 4 */

    CHECK_STR_EQ("[0]", "a", a.arr[0].v);
    CHECK_STR_EQ("[1]", "b", a.arr[1].v);
    CHECK_STR_EQ("[2]", "d", a.arr[2].v);
    CHECK_STR_EQ("[3]", "e", a.arr[3].v);

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array_api, compact_all_holes_becomes_empty) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    fs2 v1 = fs2_fromstr("A");
    fs2 v2 = fs2_fromstr("B");
    fs2arr_push(&a, &v1);
    fs2arr_push(&a, &v2);
    fs2 t1 = fs2arr_detach_new(&a, 1);
    fs2 t2 = fs2arr_detach_new(&a, 0);
    fs2_free(&t1); fs2_free(&t2);
    /* [hole, hole], hwm=2, count=0 */
    fs2 *orig = a.arr;

    fs2arr_compact(&a);

    CHECK_PTR_EQ("no realloc", orig, a.arr);   /* capacity unchanged */
    CHECK_EQ   ("hwm",   0, a.hwm);            /* ← было 2, теперь 0 */
    CHECK_EQ   ("count", 0, a.count);

    /* all [0, size) holes (padding) */
    CHECK_TRUE("[0] hole", fs2_is_hole(&a.arr[0]));
    CHECK_TRUE("[1] hole", fs2_is_hole(&a.arr[1]));
    CHECK_TRUE("valid", fs2arr_validate(NULL, &a));

    fs2arr_free(&a);
    assert_balanced();
}

/* compact now subsumes synchwm for the trailing-trim case */
Test(fs2_array_api, compact_then_synchwm_is_noop) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(16);
    const char *vals[] = { "A", "B", "C", "D", "E" };
    for (int i = 0; i < 5; i++) { fs2 v = fs2_fromstr(vals[i]); fs2arr_push(&a, &v); }
    fs2 t1 = fs2arr_detach_new(&a, 3);
    fs2 t2 = fs2arr_detach_new(&a, 1);
    fs2_free(&t1); fs2_free(&t2);

    fs2arr_compact(&a);
    size_t hwm_after_compact = a.hwm;
    cr_assert_eq(hwm_after_compact, 3);

    fs2arr_synchwm(&a);                        /* noop — no trailing holes */
    CHECK_EQ("hwm unchanged", hwm_after_compact, a.hwm);

    fs2arr_shrink(&a);                         /* size → hwm=3 */
    CHECK_EQ("size",  3, a.size);
    CHECK_EQ("hwm",   3, a.hwm);
    CHECK_EQ("count", 3, a.count);
    CHECK_STR_EQ("[0]", "A", a.arr[0].v);
    CHECK_STR_EQ("[2]", "E", a.arr[2].v);

    fs2arr_free(&a);
    assert_balanced();
}

/* compact now makes sortasc directly safe — no separate synchwm needed */
Test(fs2_array_api, compact_enables_sortasc_directly) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    const char *vals[] = { "e", "d", "c", "b", "a" };
    for (int i = 0; i < 5; i++) { fs2 v = fs2_fromstr(vals[i]); fs2arr_push(&a, &v); }
    fs2 t = fs2arr_detach_new(&a, 2);
    fs2_free(&t);
    /* [e, d, hole, b, a], hwm=5, count=4 */

    fs2arr_compact(&a);
    /* [e, d, b, a], hwm=4, count=4 — packed */

    cr_assert_eq(a.hwm, a.count);              /* precondition of sortasc */

    fs2arr_sortasc(&a);                        /* safe: hwm == count */

    CHECK_STR_EQ("[0]", "a", a.arr[0].v);
    CHECK_STR_EQ("[1]", "b", a.arr[1].v);
    CHECK_STR_EQ("[2]", "d", a.arr[2].v);
    CHECK_STR_EQ("[3]", "e", a.arr[3].v);

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array_api, compact_idempotent_eqexact) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    const char *vals[] = { "A", "B", "C", "D", "E" };
    for (int i = 0; i < 5; i++) { fs2 v = fs2_fromstr(vals[i]); fs2arr_push(&a, &v); }
    /* detach mid + tail to create a rich hole pattern */
    fs2 t1 = fs2arr_detach_new(&a, 3);
    fs2 t2 = fs2arr_detach_new(&a, 1);
    fs2_free(&t1); fs2_free(&t2);
    /* [A, hole, C, hole, E], hwm=5, count=3 */

    fs2arr_compact(&a);
    /* [A, C, E], hwm=3, count=3, size >= 8 */

    fs2array snapshot = fs2arr_cloneexact(&a);
    cr_assert_eq(snapshot.hwm, 3);
    cr_assert_eq(snapshot.count, 3);

    fs2arr_compact(&a);          /* second compact — must be pure no-op */

    CHECK_EQ  ("hwm",   snapshot.hwm,   a.hwm);
    CHECK_EQ  ("count", snapshot.count, a.count);
    CHECK_EQ  ("size",  snapshot.size,  a.size);
    CHECK_TRUE("eqexact after double compact", fs2arr_equalexact(&a, &snapshot));
    CHECK_TRUE("validate",                      fs2arr_validate(NULL, &a));

    fs2arr_free(&a);
    fs2arr_free(&snapshot);
    assert_balanced();
}

Test(fs2_array_api, compact_idempotent_all_holes) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    fs2 v1 = fs2_fromstr("x");
    fs2 v2 = fs2_fromstr("y");
    fs2arr_push(&a, &v1);
    fs2arr_push(&a, &v2);
    fs2 t1 = fs2arr_detach_new(&a, 1);
    fs2 t2 = fs2arr_detach_new(&a, 0);
    fs2_free(&t1); fs2_free(&t2);
    /* [hole, hole], hwm=2, count=0 */

    fs2arr_compact(&a);
    /* [hole, hole], hwm=0, count=0 */

    fs2array snapshot = fs2arr_cloneexact(&a);
    /* cloneexact of hwm=0 → canonical FS2ARR() (both sides empty) */

    fs2arr_compact(&a);          /* idempotent — hwm=0, no-op */

    CHECK_TRUE("eqexact after double compact (all holes)",
               fs2arr_equalexact(&a, &snapshot));
    CHECK_EQ  ("hwm",   0, a.hwm);
    CHECK_EQ  ("count", 0, a.count);

    fs2arr_free(&a);
    fs2arr_free(&snapshot);
    assert_balanced();
}

Test(fs2_array_api, compact_idempotent_canonical) {
    fs2array a = FS2ARR();
    fs2arr_compact(&a);                /* no-op */
    fs2arr_compact(&a);                /* no-op */
    CHECK_PTR_EQ("arr", NULL, a.arr);
    CHECK_EQ   ("hwm",  0, a.hwm);
    CHECK_EQ   ("count",0, a.count);
    CHECK_EQ   ("size", 0, a.size);
}

Test(fs2_array_api, compact_idempotent_packed) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    for (int i = 0; i < 3; i++) { fs2 v = fs2_from_new(i); fs2arr_push(&a, &v); }
    /* already packed: [0,1,2], hwm=3 */

    fs2array snapshot = fs2arr_cloneexact(&a);

    fs2arr_compact(&a);                /* no-op: write == hwm */

    CHECK_TRUE("packed → compact == no-op",
               fs2arr_equalexact(&a, &snapshot));

    fs2arr_free(&a);
    fs2arr_free(&snapshot);
    assert_balanced();
}

/* =========================================================================
 * fs2arr_compact — guards
 * ========================================================================= */

Test(fs2_array_api, compact_null_aborts, .signal = SIGABRT) {
    fs2arr_compact(NULL);
}

/* Helper: build sorted array from string array */
static fs2array
mk_sorted(const char *vals[], size_t n, pointer_comparator_t cmp) {
    fs2array a = fs2arr_init(n);
    for (size_t i = 0; i < n; i++) {
        fs2 v = fs2_fromstr(vals[i]);
        fs2arr_push(&a, &v);
    }
    fs2arr_sort(&a, cmp);
    return a;
}

/* =========================================================================
 * fs2arr_mergesorted_new — nominal
 * ========================================================================= */

Test(fs2_array_sort, merge_basic_interleaved) {
    __fs2_checkreset();
    const char *av[] = { "a", "c", "e" };
    const char *bv[] = { "b", "d", "f" };
    fs2array a = mk_sorted(av, 3, fs2_cmp_wrap);
    fs2array b = mk_sorted(bv, 3, fs2_cmp_wrap);

    fs2array r = fs2arr_mergesorted_new(&a, &b, fs2_cmp_wrap);

    CHECK_EQ  ("count", 6, r.count);
    CHECK_EQ  ("hwm",   6, r.hwm);              /* packed */
    CHECK_GE  ("size",  r.size, 6);
    CHECK_STR_EQ("[0]", "a", r.arr[0].v);
    CHECK_STR_EQ("[1]", "b", r.arr[1].v);
    CHECK_STR_EQ("[2]", "c", r.arr[2].v);
    CHECK_STR_EQ("[3]", "d", r.arr[3].v);
    CHECK_STR_EQ("[4]", "e", r.arr[4].v);
    CHECK_STR_EQ("[5]", "f", r.arr[5].v);
    CHECK_TRUE ("valid", fs2arr_validate(NULL, &r));

    fs2arr_free(&a);
    fs2arr_free(&b);
    fs2arr_free(&r);
    assert_balanced();
}

Test(fs2_array_sort, merge_one_empty_left) {
    __fs2_checkreset();
    fs2array a = FS2ARR();                      /* empty */
    const char *bv[] = { "x", "y", "z" };
    fs2array b = mk_sorted(bv, 3, fs2_cmp_wrap);

    fs2array r = fs2arr_mergesorted_new(&a, &b, fs2_cmp_wrap);

    CHECK_EQ  ("count", 3, r.count);
    CHECK_STR_EQ("[0]", "x", r.arr[0].v);
    CHECK_STR_EQ("[2]", "z", r.arr[2].v);

    fs2arr_free(&b);
    fs2arr_free(&r);
    assert_balanced();
}

Test(fs2_array_sort, merge_one_empty_right) {
    __fs2_checkreset();
    const char *av[] = { "x", "y" };
    fs2array a = mk_sorted(av, 2, fs2_cmp_wrap);
    fs2array b = FS2ARR();

    fs2array r = fs2arr_mergesorted_new(&a, &b, fs2_cmp_wrap);

    CHECK_EQ  ("count", 2, r.count);
    CHECK_STR_EQ("[0]", "x", r.arr[0].v);
    CHECK_STR_EQ("[1]", "y", r.arr[1].v);

    fs2arr_free(&a);
    fs2arr_free(&r);
    assert_balanced();
}

Test(fs2_array_sort, merge_both_empty) {
    fs2array a = FS2ARR();
    fs2array b = FS2ARR();

    fs2array r = fs2arr_mergesorted_new(&a, &b, fs2_cmp_wrap);

    CHECK_PTR_EQ("arr",   NULL, r.arr);
    CHECK_EQ   ("count", 0, r.count);
    /* canonical — no free needed */
}

Test(fs2_array_sort, merge_disjoint_ranges) {
    __fs2_checkreset();
    const char *av[] = { "a", "b", "c" };
    const char *bv[] = { "x", "y", "z" };
    fs2array a = mk_sorted(av, 3, fs2_cmp_wrap);
    fs2array b = mk_sorted(bv, 3, fs2_cmp_wrap);

    fs2array r = fs2arr_mergesorted_new(&a, &b, fs2_cmp_wrap);

    CHECK_EQ  ("count", 6, r.count);
    CHECK_STR_EQ("[0]", "a", r.arr[0].v);
    CHECK_STR_EQ("[2]", "c", r.arr[2].v);
    CHECK_STR_EQ("[3]", "x", r.arr[3].v);
    CHECK_STR_EQ("[5]", "z", r.arr[5].v);

    fs2arr_free(&a);
    fs2arr_free(&b);
    fs2arr_free(&r);
    assert_balanced();
}

/* =========================================================================
 * fs2arr_mergesorted_new — stability (equal elements: a before b)
 * ========================================================================= */

Test(fs2_array_sort, merge_stable_equal_elements) {
    __fs2_checkreset();
    /* Both arrays contain "b" — merged result should keep a's "b" first */
    const char *av[] = { "a", "b", "c" };
    const char *bv[] = { "b", "b", "d" };
    fs2array a = mk_sorted(av, 3, fs2_cmp_wrap);
    fs2array b = mk_sorted(bv, 3, fs2_cmp_wrap);

    fs2array r = fs2arr_mergesorted_new(&a, &b, fs2_cmp_wrap);

    CHECK_EQ  ("count", 6, r.count);
    CHECK_STR_EQ("[0]", "a", r.arr[0].v);
    /* Three "b"s in positions [1..3] — order among them is not observable
     * by value, but stability guarantees a's "b" comes before b's "b"s.
     * Since all are "b", this test verifies only that count is right and
     * that "d" is last, "c" is before "d". */
    CHECK_STR_EQ("[1]", "b", r.arr[1].v);
    CHECK_STR_EQ("[2]", "b", r.arr[2].v);
    CHECK_STR_EQ("[3]", "b", r.arr[3].v);
    CHECK_STR_EQ("[4]", "c", r.arr[4].v);
    CHECK_STR_EQ("[5]", "d", r.arr[5].v);

    fs2arr_free(&a);
    fs2arr_free(&b);
    fs2arr_free(&r);
    assert_balanced();
}

/* Stability with distinguishable equal-key elements: use cmp by length */
static int cmp_len_only(const void *pa, const void *pb) {
    const fs2 *a = pa, *b = pb;
    return (a->len > b->len) - (a->len < b->len);
}

Test(fs2_array_sort, merge_stable_by_length_key) {
    __fs2_checkreset();
    /* Both sorted by length. a: [a, bb], b: [c, dd] */
    const char *av[] = { "a", "bb" };
    const char *bv[] = { "c", "dd" };
    fs2array a = mk_sorted(av, 2, cmp_len_only);
    fs2array b = mk_sorted(bv, 2, cmp_len_only);

    fs2array r = fs2arr_mergesorted_new(&a, &b, cmp_len_only);
    /* Stable: equal-length elements — a's first, then b's */
    CHECK_EQ  ("count", 4, r.count);
    CHECK_STR_EQ("[0]", "a", r.arr[0].v);   /* len 1 from a */
    CHECK_STR_EQ("[1]", "c", r.arr[1].v);   /* len 1 from b */
    CHECK_STR_EQ("[2]", "bb", r.arr[2].v);  /* len 2 from a */
    CHECK_STR_EQ("[3]", "dd", r.arr[3].v);  /* len 2 from b */

    fs2arr_free(&a);
    fs2arr_free(&b);
    fs2arr_free(&r);
    assert_balanced();
}

/* =========================================================================
 * fs2arr_mergesorted_new — deep copy, inputs untouched
 * ========================================================================= */

Test(fs2_array_sort, merge_deep_copy_inputs_untouched) {
    __fs2_checkreset();
    const char *av[] = { "a", "c" };
    const char *bv[] = { "b", "d" };
    fs2array a = mk_sorted(av, 2, fs2_cmp_wrap);
    fs2array b = mk_sorted(bv, 2, fs2_cmp_wrap);
    void *a0 = a.arr[0].v, *b0 = b.arr[0].v;

    fs2array r = fs2arr_mergesorted_new(&a, &b, fs2_cmp_wrap);

    /* Inputs unchanged */
    CHECK_PTR_EQ("a[0].v unchanged", a.arr[0].v, a0);
    CHECK_PTR_EQ("b[0].v unchanged", b.arr[0].v, b0);
    CHECK_STR_EQ("a content",        "a", a.arr[0].v);
    CHECK_STR_EQ("b content",        "b", b.arr[0].v);
    CHECK_EQ  ("a.count", 2, a.count);
    CHECK_EQ  ("b.count", 2, b.count);

    /* Result is independent: mutate result[0], source unaffected */
    CHECK_TRUE("independent buffer", r.arr[0].v != a0);
    r.arr[0].v[0] = 'X';
    CHECK_STR_EQ("a still 'a'", "a", a.arr[0].v);
    CHECK_STR_EQ("r is 'X'",    "X", r.arr[0].v);

    fs2arr_free(&a);
    fs2arr_free(&b);
    fs2arr_free(&r);
    assert_balanced();
}

/* =========================================================================
 * fs2arr_mergesorted_new — same pointer (a == b)
 * ========================================================================= */

Test(fs2_array_sort, merge_self_doubles_elements) {
    __fs2_checkreset();
    const char *av[] = { "a", "b", "c" };
    fs2array a = mk_sorted(av, 3, fs2_cmp_wrap);

    fs2array r = fs2arr_mergesorted_new(&a, &a, fs2_cmp_wrap);

    CHECK_EQ  ("count", 6, r.count);
    CHECK_STR_EQ("[0]", "a", r.arr[0].v);
    CHECK_STR_EQ("[1]", "a", r.arr[1].v);
    CHECK_STR_EQ("[2]", "b", r.arr[2].v);
    CHECK_STR_EQ("[3]", "b", r.arr[3].v);
    CHECK_STR_EQ("[4]", "c", r.arr[4].v);
    CHECK_STR_EQ("[5]", "c", r.arr[5].v);

    fs2arr_free(&a);
    fs2arr_free(&r);
    assert_balanced();
}

/* =========================================================================
 * fs2arr_mergesorted_new — precondition: not packed → abort
 * ========================================================================= */

Test(fs2_array_sort, merge_a_not_packed_aborts, .signal = SIGINT) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    fs2 v1 = fs2_fromstr("a"); fs2arr_push(&a, &v1);
    fs2 v2 = fs2_fromstr("b"); fs2arr_push(&a, &v2);
    fs2 t = fs2arr_detach_new(&a, 0);
    fs2_free(&t);
    /* a: [hole, "b"], hwm=2, count=1 — NOT packed */

    fs2array b = fs2arr_init(2);
    fs2 w = fs2_fromstr("c"); fs2arr_push(&b, &w);

    fs2arr_mergesorted_new(&a, &b, fs2_cmp_wrap);
    /* unreachable */
}

Test(fs2_array_sort, merge_b_not_packed_aborts, .signal = SIGINT) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(2);
    fs2 v1 = fs2_fromstr("a"); fs2arr_push(&a, &v1);

    fs2array b = fs2arr_init(8);
    fs2 w1 = fs2_fromstr("b"); fs2arr_push(&b, &w1);
    fs2 w2 = fs2_fromstr("c"); fs2arr_push(&b, &w2);
    fs2 t = fs2arr_detach_new(&b, 0);
    fs2_free(&t);

    fs2arr_mergesorted_new(&a, &b, fs2_cmp_wrap);
    /* unreachable */
}

/* After compact, same arrays work fine */
Test(fs2_array_sort, merge_after_compact_works) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    fs2 v1 = fs2_fromstr("a"); fs2arr_push(&a, &v1);
    fs2 v2 = fs2_fromstr("b"); fs2arr_push(&a, &v2);
    fs2 t = fs2arr_detach_new(&a, 0);
    fs2_free(&t);
    fs2arr_compact(&a);                         /* [b], hwm=count=1 */

    fs2array b = fs2arr_init(2);
    fs2 w = fs2_fromstr("c"); fs2arr_push(&b, &w);

    fs2array r = fs2arr_mergesorted_new(&a, &b, fs2_cmp_wrap);

    CHECK_EQ  ("count", 2, r.count);
    CHECK_STR_EQ("[0]", "b", r.arr[0].v);
    CHECK_STR_EQ("[1]", "c", r.arr[1].v);

    fs2arr_free(&a);
    fs2arr_free(&b);
    fs2arr_free(&r);
    assert_balanced();
}

/* =========================================================================
 * fs2arr_mergesorted_new — guards
 * ========================================================================= */

Test(fs2_array_sort, merge_null_a_aborts, .signal = SIGABRT) {
    fs2array b = FS2ARR();
    fs2arr_mergesorted_new(NULL, &b, fs2_cmp_wrap);
}

Test(fs2_array_sort, merge_null_b_aborts, .signal = SIGABRT) {
    fs2array a = FS2ARR();
    fs2arr_mergesorted_new(&a, NULL, fs2_cmp_wrap);
}

Test(fs2_array_sort, merge_null_cmp_aborts, .signal = SIGABRT) {
    fs2array a = FS2ARR();
    fs2array b = FS2ARR();
    fs2arr_mergesorted_new(&a, &b, NULL);
}

/* =========================================================================
 * fs2arr_mergesorted_new — desc comparator
 * ========================================================================= */

Test(fs2_array_sort, merge_desc) {
    __fs2_checkreset();
    const char *av[] = { "c", "a" };            /* sorted desc after mk_sorted */
    const char *bv[] = { "d", "b" };
    fs2array a = mk_sorted(av, 2, fs2_cmp_desc_wrap);
    fs2array b = mk_sorted(bv, 2, fs2_cmp_desc_wrap);

    fs2array r = fs2arr_mergesorted_new(&a, &b, fs2_cmp_desc_wrap);

    CHECK_EQ  ("count", 4, r.count);
    CHECK_STR_EQ("[0]", "d", r.arr[0].v);
    CHECK_STR_EQ("[1]", "c", r.arr[1].v);
    CHECK_STR_EQ("[2]", "b", r.arr[2].v);
    CHECK_STR_EQ("[3]", "a", r.arr[3].v);

    fs2arr_free(&a);
    fs2arr_free(&b);
    fs2arr_free(&r);
    assert_balanced();
}

/* =========================================================================
 * fs2arr_issorted — helper
 * ========================================================================= */

Test(fs2_array_sort, is_sorted_positive) {
    __fs2_checkreset();
    const char *vals[] = { "a", "b", "c", "c", "d" };
    fs2array a = mk_sorted(vals, 5, fs2_cmp_wrap);
    CHECK_TRUE("sorted", fs2arr_issorted(&a, fs2_cmp_wrap));
    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array_sort, is_sorted_negative) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(3);
    fs2 v1 = fs2_fromstr("b"); fs2arr_push(&a, &v1);
    fs2 v2 = fs2_fromstr("a"); fs2arr_push(&a, &v2);
    fs2 v3 = fs2_fromstr("c"); fs2arr_push(&a, &v3);
    CHECK_FALSE("b,a,c not sorted", fs2arr_issorted(&a, fs2_cmp_wrap));
    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array_sort, is_sorted_empty_and_single) {
    fs2array canon = FS2ARR();
    CHECK_TRUE("canonical sorted", fs2arr_issorted(&canon, fs2_cmp_wrap));

    __fs2_checkreset();
    fs2array single = fs2arr_init(2);
    fs2 v = fs2_fromstr("x"); fs2arr_push(&single, &v);
    CHECK_TRUE("single sorted", fs2arr_issorted(&single, fs2_cmp_wrap));
    fs2arr_free(&single);
    assert_balanced();
}

/* =========================================================================
 * Integration: sort → merge → sorted
 * ========================================================================= */

Test(fs2_array_sort, sort_merge_sort_pipeline) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    const char *av[] = { "e", "a", "c" };
    for (int i = 0; i < 3; i++) { fs2 v = fs2_fromstr(av[i]); fs2arr_push(&a, &v); }
    fs2arr_sortasc(&a);                         /* [a, c, e] */

    fs2array b = fs2arr_init(8);
    const char *bv[] = { "d", "b", "f" };
    for (int i = 0; i < 3; i++) { fs2 v = fs2_fromstr(bv[i]); fs2arr_push(&b, &v); }
    fs2arr_sortasc(&b);                         /* [b, d, f] */

    fs2array r = fs2arr_mergesorted_new(&a, &b, fs2_cmp_wrap);

    CHECK_TRUE("result sorted", fs2arr_issorted(&r, fs2_cmp_wrap));
    CHECK_EQ  ("count", 6, r.count);
    CHECK_STR_EQ("[0]", "a", r.arr[0].v);
    CHECK_STR_EQ("[1]", "b", r.arr[1].v);
    CHECK_STR_EQ("[2]", "c", r.arr[2].v);
    CHECK_STR_EQ("[3]", "d", r.arr[3].v);
    CHECK_STR_EQ("[4]", "e", r.arr[4].v);
    CHECK_STR_EQ("[5]", "f", r.arr[5].v);

    fs2arr_free(&a);
    fs2arr_free(&b);
    fs2arr_free(&r);
    assert_balanced();
}

/* Helper: build array from string list */
static fs2array
mk_arr(const char *vals[], size_t n) {
    fs2array a = fs2arr_init(n);
    for (size_t i = 0; i < n; i++) {
        fs2 v = fs2_fromstr(vals[i]);
        fs2arr_push(&a, &v);
    }
    return a;
}

/* =========================================================================
 * fs2arr_concat — in-place, dst += src
 * ========================================================================= */

Test(fs2_array_api, concat_basic) {
    __fs2_checkreset();
    fs2array dst = mk_arr((const char*[]){ "a", "b" }, 2);
    fs2array src = mk_arr((const char*[]){ "x", "y", "z" }, 3);

    fs2arr_concat(&dst, &src);

    CHECK_EQ  ("hwm",   5, dst.hwm);
    CHECK_EQ  ("count", 5, dst.count);
    CHECK_STR_EQ("[0]", "a", dst.arr[0].v);
    CHECK_STR_EQ("[1]", "b", dst.arr[1].v);
    CHECK_STR_EQ("[2]", "x", dst.arr[2].v);
    CHECK_STR_EQ("[3]", "y", dst.arr[3].v);
    CHECK_STR_EQ("[4]", "z", dst.arr[4].v);
    CHECK_TRUE ("valid", fs2arr_validate(NULL, &dst));

    fs2arr_free(&dst);
    fs2arr_free(&src);
    assert_balanced();
}

Test(fs2_array_api, concat_empty_src_noop) {
    __fs2_checkreset();
    fs2array dst = mk_arr((const char*[]){ "a" }, 1);
    fs2array src = FS2ARR();

    fs2arr_concat(&dst, &src);

    CHECK_EQ  ("hwm",   1, dst.hwm);
    CHECK_EQ  ("count", 1, dst.count);
    CHECK_STR_EQ("[0]", "a", dst.arr[0].v);

    fs2arr_free(&dst);
}

Test(fs2_array_api, concat_empty_dst_clones_src) {
    __fs2_checkreset();
    fs2array dst = FS2ARR();
    fs2array src = mk_arr((const char*[]){ "x", "y" }, 2);

    fs2arr_concat(&dst, &src);

    CHECK_EQ  ("hwm",   2, dst.hwm);
    CHECK_EQ  ("count", 2, dst.count);
    CHECK_STR_EQ("[0]", "x", dst.arr[0].v);
    CHECK_STR_EQ("[1]", "y", dst.arr[1].v);
    /* deep copy: distinct buffers */
    CHECK_TRUE("independent", dst.arr[0].v != src.arr[0].v);

    fs2arr_free(&dst);
    fs2arr_free(&src);
    assert_balanced();
}

Test(fs2_array_api, concat_both_empty) {
    fs2array dst = FS2ARR();
    fs2array src = FS2ARR();

    fs2arr_concat(&dst, &src);

    CHECK_PTR_EQ("arr", NULL, dst.arr);
    CHECK_EQ   ("count", 0, dst.count);
}

/* src with holes: only live appended */
Test(fs2_array_api, concat_skips_holes_in_src) {
    __fs2_checkreset();
    fs2array dst = mk_arr((const char*[]){ "d" }, 1);
    fs2array src = fs2arr_init(8);
    for (int i = 0; i < 4; i++) {
        fs2 v = fs2_from_new(i);                 /* "0","1","2","3" */
        fs2arr_push(&src, &v);
    }
    fs2 t = fs2arr_detach_new(&src, 1);          /* hole at [1] */
    fs2_free(&t);
    /* src: [0, hole, 2, 3], hwm=4, count=3 */

    fs2arr_concat(&dst, &src);

    CHECK_EQ  ("hwm",   4, dst.hwm);             /* 1 + 3 (holes skipped) */
    CHECK_EQ  ("count", 4, dst.count);
    CHECK_STR_EQ("[0]", "d", dst.arr[0].v);
    CHECK_STR_EQ("[1]", "0", dst.arr[1].v);
    CHECK_STR_EQ("[2]", "2", dst.arr[2].v);
    CHECK_STR_EQ("[3]", "3", dst.arr[3].v);

    fs2arr_free(&dst);
    fs2arr_free(&src);
    assert_balanced();
}

/* dst with holes: push goes to hwm, holes stay */
Test(fs2_array_api, concat_preserves_holes_in_dst) {
    __fs2_checkreset();
    fs2array dst = fs2arr_init(8);
    for (int i = 0; i < 3; i++) {
        fs2 v = fs2_fromstr((const char*[]){ "A", "B", "C" }[i]);
        fs2arr_push(&dst, &v);
    }
    fs2 t = fs2arr_detach_new(&dst, 1);          /* hole at [1] */
    fs2_free(&t);
    /* dst: [A, hole, C], hwm=3, count=2 */

    fs2array src = mk_arr((const char*[]){ "X" }, 1);

    fs2arr_concat(&dst, &src);

    CHECK_EQ  ("hwm",   4, dst.hwm);             /* push appends at hwm=3 */
    CHECK_EQ  ("count", 3, dst.count);
    CHECK_STR_EQ("[0]", "A", dst.arr[0].v);
    CHECK_TRUE ("[1] hole preserved", fs2_is_hole(&dst.arr[1]));
    CHECK_STR_EQ("[2]", "C", dst.arr[2].v);
    CHECK_STR_EQ("[3]", "X", dst.arr[3].v);

    fs2arr_free(&dst);
    fs2arr_free(&src);
    assert_balanced();
}

/* Self-concat doubles live elements, preserves holes */
Test(fs2_array_api, concat_self_doubles_packed) {
    __fs2_checkreset();
    fs2array a = mk_arr((const char*[]){ "a", "b", "c" }, 3);

    fs2arr_concat(&a, &a);

    CHECK_EQ  ("count", 6, a.count);
    CHECK_EQ  ("hwm",   6, a.hwm);
    CHECK_STR_EQ("[0]", "a", a.arr[0].v);
    CHECK_STR_EQ("[1]", "b", a.arr[1].v);
    CHECK_STR_EQ("[2]", "c", a.arr[2].v);
    CHECK_STR_EQ("[3]", "a", a.arr[3].v);
    CHECK_STR_EQ("[4]", "b", a.arr[4].v);
    CHECK_STR_EQ("[5]", "c", a.arr[5].v);

    /* Deep copies: [3..5] distinct from [0..2] */
    CHECK_TRUE("[0] != [3] buf", a.arr[0].v != a.arr[3].v);
    CHECK_TRUE("[1] != [4] buf", a.arr[1].v != a.arr[4].v);
    CHECK_TRUE("[2] != [5] buf", a.arr[2].v != a.arr[5].v);

    CHECK_TRUE("valid", fs2arr_validate(NULL, &a));

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array_api, concat_self_with_holes) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    for (int i = 0; i < 4; i++) {
        fs2 v = fs2_fromstr((const char*[]){ "a", "b", "c", "d" }[i]);
        fs2arr_push(&a, &v);
    }
    fs2 t = fs2arr_detach_new(&a, 1);   /* hole at [1] */
    fs2_free(&t);
    /* [a, hole, c, d], hwm=4, count=3 */

    fs2arr_concat(&a, &a);

    CHECK_EQ  ("hwm",   7, a.hwm);
    CHECK_EQ  ("count", 6, a.count);
    CHECK_STR_EQ("[0]", "a", a.arr[0].v);
    CHECK_TRUE ("[1] hole preserved", fs2_is_hole(&a.arr[1]));
    CHECK_STR_EQ("[2]", "c", a.arr[2].v);
    CHECK_STR_EQ("[3]", "d", a.arr[3].v);
    CHECK_STR_EQ("[4]", "a", a.arr[4].v);
    CHECK_STR_EQ("[5]", "c", a.arr[5].v);
    CHECK_STR_EQ("[6]", "d", a.arr[6].v);
    CHECK_TRUE ("valid", fs2arr_validate(NULL, &a));

    fs2arr_free(&a);
    assert_balanced();
}

/* Empty self-concat is no-op */
Test(fs2_array_api, concat_self_empty_noop) {
    fs2array a = FS2ARR();
    fs2arr_concat(&a, &a);
    CHECK_PTR_EQ("arr", NULL, a.arr);
    CHECK_EQ("count", 0, a.count);
}

/* Self-concat of single */
Test(fs2_array_api, concat_self_single) {
    __fs2_checkreset();
    fs2array a = mk_arr((const char*[]){ "x" }, 1);
    fs2arr_concat(&a, &a);
    CHECK_EQ  ("count", 2, a.count);
    CHECK_STR_EQ("[0]", "x", a.arr[0].v);
    CHECK_STR_EQ("[1]", "x", a.arr[1].v);
    CHECK_TRUE("[0] != [1] buf", a.arr[0].v != a.arr[1].v);
    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array_api, concat_null_dst_aborts, .signal = SIGABRT) {
    fs2array src = FS2ARR();
    fs2arr_concat(NULL, &src);
}

Test(fs2_array_api, concat_null_src_aborts, .signal = SIGABRT) {
    fs2array dst = FS2ARR();
    fs2arr_concat(&dst, NULL);
}

/* =========================================================================
 * fs2arr_concat_new — packed, tight, deep copy
 * ========================================================================= */

Test(fs2_array_api, concat_new_basic) {
    __fs2_checkreset();
    fs2array a = mk_arr((const char*[]){ "a", "b" }, 2);
    fs2array b = mk_arr((const char*[]){ "x", "y" }, 2);

    fs2array r = fs2arr_concat_new(&a, &b);

    CHECK_EQ  ("hwm",   4, r.hwm);
    CHECK_EQ  ("count", 4, r.count);
    CHECK_GE  ("size",  r.size, 4);              /* tight */
    CHECK_STR_EQ("[0]", "a", r.arr[0].v);
    CHECK_STR_EQ("[1]", "b", r.arr[1].v);
    CHECK_STR_EQ("[2]", "x", r.arr[2].v);
    CHECK_STR_EQ("[3]", "y", r.arr[3].v);
    CHECK_TRUE ("valid", fs2arr_validate(NULL, &r));

    /* independent buffers */
    CHECK_TRUE("a-buf independent", r.arr[0].v != a.arr[0].v);
    CHECK_TRUE("b-buf independent", r.arr[2].v != b.arr[0].v);

    fs2arr_free(&a); fs2arr_free(&b); fs2arr_free(&r);
    assert_balanced();
}

Test(fs2_array_api, concat_new_a_empty) {
    __fs2_checkreset();
    fs2array a = FS2ARR();
    fs2array b = mk_arr((const char*[]){ "x", "y" }, 2);

    fs2array r = fs2arr_concat_new(&a, &b);

    CHECK_EQ  ("count", 2, r.count);
    CHECK_GE  ("size",  r.size, 2);
    CHECK_STR_EQ("[0]", "x", r.arr[0].v);

    fs2arr_free(&b);
    fs2arr_free(&r);
    assert_balanced();
}

Test(fs2_array_api, concat_new_b_empty) {
    __fs2_checkreset();
    fs2array a = mk_arr((const char*[]){ "a", "b" }, 2);
    fs2array b = FS2ARR();

    fs2array r = fs2arr_concat_new(&a, &b);

    CHECK_EQ  ("count", 2, r.count);
    CHECK_GE  ("size",  r.size, 2);
    CHECK_STR_EQ("[0]", "a", r.arr[0].v);

    fs2arr_free(&a);
    fs2arr_free(&r);
    assert_balanced();
}

Test(fs2_array_api, concat_new_both_empty) {
    fs2array a = FS2ARR();
    fs2array b = FS2ARR();

    fs2array r = fs2arr_concat_new(&a, &b);

    CHECK_PTR_EQ("arr", NULL, r.arr);
    CHECK_EQ   ("count", 0, r.count);
}

/* holes in a → packed in result (clone drops holes) */
Test(fs2_array_api, concat_new_packs_holes_in_a) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    for (int i = 0; i < 4; i++) {
        fs2 v = fs2_from_new(i);
        fs2arr_push(&a, &v);
    }
    fs2 t = fs2arr_detach_new(&a, 1);            /* hole at [1] */
    fs2_free(&t);
    /* a: [0, hole, 2, 3], hwm=4, count=3 */

    fs2array b = mk_arr((const char*[]){ "x" }, 1);

    fs2array r = fs2arr_concat_new(&a, &b);

    /* clone(a) packs → [0, 2, 3], then +x → [0, 2, 3, x] */
    CHECK_EQ  ("count", 4, r.count);
    CHECK_EQ  ("hwm",   4, r.hwm);
    CHECK_GE  ("size",  r.size, 4);
    CHECK_STR_EQ("[0]", "0", r.arr[0].v);
    CHECK_STR_EQ("[1]", "2", r.arr[1].v);
    CHECK_STR_EQ("[2]", "3", r.arr[2].v);
    CHECK_STR_EQ("[3]", "x", r.arr[3].v);

    fs2arr_free(&a); fs2arr_free(&b); fs2arr_free(&r);
    assert_balanced();
}

Test(fs2_array_api, concat_new_skips_holes_in_b) {
    __fs2_checkreset();
    fs2array a = mk_arr((const char*[]){ "a" }, 1);
    fs2array b = fs2arr_init(8);
    for (int i = 0; i < 3; i++) {
        fs2 v = fs2_fromstr((const char*[]){ "X", "Y", "Z" }[i]);
        fs2arr_push(&b, &v);
    }
    fs2 t = fs2arr_detach_new(&b, 1);            /* hole at [1] */
    fs2_free(&t);
    /* b: [X, hole, Z], hwm=3, count=2 */

    fs2array r = fs2arr_concat_new(&a, &b);

    CHECK_EQ  ("count", 3, r.count);
    CHECK_GE  ("size",  r.size, 3);
    CHECK_STR_EQ("[0]", "a", r.arr[0].v);
    CHECK_STR_EQ("[1]", "X", r.arr[1].v);
    CHECK_STR_EQ("[2]", "Z", r.arr[2].v);

    fs2arr_free(&a); fs2arr_free(&b); fs2arr_free(&r);
    assert_balanced();
}

/* Invariance: concat_new(a, b) ≡ concat_new(b, a) for disjoint-content inputs */
Test(fs2_array_api, concat_new_preserves_multiset) {
    __fs2_checkreset();
    fs2array a = mk_arr((const char*[]){ "c", "a" }, 2);
    fs2array b = mk_arr((const char*[]){ "b", "a" }, 2);

    fs2array r1 = fs2arr_concat_new(&a, &b);
    fs2array r2 = fs2arr_concat_new(&b, &a);

    /* order differs, content multiset identical */
    CHECK_EQ("count r1", 4, r1.count);
    CHECK_EQ("count r2", 4, r2.count);

    /* sort both, compare */
    fs2arr_sortasc(&r1);
    fs2arr_sortasc(&r2);
    CHECK_TRUE("same multiset", fs2arr_equalexact(&r1, &r2));

    fs2arr_free(&a); fs2arr_free(&b);
    fs2arr_free(&r1); fs2arr_free(&r2);
    assert_balanced();
}

Test(fs2_array_api, concat_new_null_a_aborts, .signal = SIGABRT) {
    fs2array b = FS2ARR();
    fs2arr_concat_new(NULL, &b);
}

Test(fs2_array_api, concat_new_null_b_aborts, .signal = SIGABRT) {
    fs2array a = FS2ARR();
    fs2arr_concat_new(&a, NULL);
}

/* =========================================================================
 * Integration: split → concat → original reconstruction
 * ========================================================================= */

Test(fs2_array_api, split_concat_roundtrip) {
    __fs2_checkreset();

    fs2 src = fs2_fromstr("alpha,beta,gamma,delta");
    fs2 comma = fs2_fromstr(",");

    fs2array parts = fs2_split_toarray(&src, &comma);
    cr_assert_eq(parts.count, 4);

    /* split into two halves */
    fs2array left = fs2arr_init(2);
    fs2array right = fs2arr_init(2);
    for (size_t i = 0; i < 2; i++) {
        fs2 v = fs2_clone(&parts.arr[i]); fs2arr_push(&left, &v);
    }
    for (size_t i = 2; i < 4; i++) {
        fs2 v = fs2_clone(&parts.arr[i]); fs2arr_push(&right, &v);
    }

    /* concat back */
    fs2array recon = fs2arr_concat_new(&left, &right);

    CHECK_EQ  ("count", 4, recon.count);
    CHECK_GE  ("size",  recon.size, 4);          /* tight */
    CHECK_TRUE("eqexact with original parts",
               fs2arr_equalexact(&parts, &recon));

    fs2_free(&src); fs2_free(&comma);
    fs2arr_free(&parts); fs2arr_free(&left); fs2arr_free(&right);
    fs2arr_free(&recon);
    assert_balanced();
}

/* =========================================================================
 * Integration: repeated concat = incremental build
 * ========================================================================= */

Test(fs2_array_api, concat_incremental_build) {
    __fs2_checkreset();

    fs2array acc = FS2ARR();
    for (int i = 0; i < 10; i++) {
        fs2array chunk = fs2arr_init(2);
        fs2 v1 = fs2_from_new(i * 2);
        fs2 v2 = fs2_from_new(i * 2 + 1);
        fs2arr_push(&chunk, &v1);
        fs2arr_push(&chunk, &v2);

        fs2arr_concat(&acc, &chunk);
        fs2arr_free(&chunk);
    }

    CHECK_EQ("final count", 20, acc.count);
    CHECK_STR_EQ("[0]",  "0",  acc.arr[0].v);
    CHECK_STR_EQ("[19]", "19", acc.arr[19].v);

    fs2arr_free(&acc);
    assert_balanced();
}

/* =========================================================================
 * fs2arr_fromstrarr — basic
 * ========================================================================= */

Test(fs2_array_api, fromstrarr_new_basic) {
    __fs2_checkreset();
    const char *const src[] = { "a", "b", "c", NULL };

    fs2array a = fs2arr_fromstrarr_new(src);

    CHECK_EQ  ("count", 3, a.count);
    CHECK_EQ  ("hwm",   3, a.hwm);             /* packed: hwm == count */
    CHECK_TRUE("size >= count", a.size >= a.count);
    CHECK_STR_EQ("[0]", "a", a.arr[0].v);
    CHECK_STR_EQ("[1]", "b", a.arr[1].v);
    CHECK_STR_EQ("[2]", "c", a.arr[2].v);
    CHECK_TRUE("valid", fs2arr_validate(NULL, &a));

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array_api, fromstrarr_single) {
    __fs2_checkreset();
    const char *const src[] = { "only", NULL };

    fs2array a = fs2arr_fromstrarr_new(src);

    CHECK_EQ  ("count", 1, a.count);
    CHECK_GE  ("size",  a.size, 1);
    CHECK_STR_EQ("[0]", "only", a.arr[0].v);

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array_api, fromstrarr_empty_list_canonical) {
    const char *const src[] = { NULL };

    fs2array a = fs2arr_fromstrarr_new(src);

    CHECK_PTR_EQ("arr",   NULL, a.arr);
    CHECK_EQ   ("count", 0, a.count);
    CHECK_EQ   ("hwm",   0, a.hwm);
    CHECK_EQ   ("size",  0, a.size);
    /* canonical — no free needed */
}

/* Empty string "" → live FS2EMPTY (not a hole) */
Test(fs2_array_api, fromstrarr_empty_strings_are_live) {
    __fs2_checkreset();
    const char *const src[] = { "", "a", "", "b", "", NULL };

    fs2array a = fs2arr_fromstrarr_new(src);

    CHECK_EQ  ("count", 5, a.count);
    CHECK_EQ  ("hwm",   5, a.hwm);
    for (size_t i = 0; i < 5; i++)
        CHECK_FALSE("[i] not hole", fs2_is_hole(&a.arr[i]));

    CHECK_TRUE ("[0] empty", fs2_eq_empty(&a.arr[0]));
    CHECK_STR_EQ("[1]", "a", a.arr[1].v);
    CHECK_TRUE ("[2] empty", fs2_eq_empty(&a.arr[2]));
    CHECK_STR_EQ("[3]", "b", a.arr[3].v);
    CHECK_TRUE ("[4] empty", fs2_eq_empty(&a.arr[4]));

    fs2arr_free(&a);
    assert_balanced();
}

/* Deep copy: result buffers are independent from src */
Test(fs2_array_api, fromstrarr_deep_copy) {
    __fs2_checkreset();
    char buf[] = "hello";
    const char *const src[] = { buf, NULL };

    fs2array a = fs2arr_fromstrarr_new(src);
    cr_assert_eq(a.count, 1);
    CHECK_STR_EQ("[0]", "hello", a.arr[0].v);

    /* mutate source */
    buf[0] = 'X';
    CHECK_STR_EQ("src mutated",          "Xello", buf);
    CHECK_STR_EQ("array still original", "hello", a.arr[0].v);

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array_api, fromstrarr_null_src_aborts, .signal = SIGABRT) {
    fs2arr_fromstrarr_new(NULL);
}

/* =========================================================================
 * FS2ARRFROMSTR macro
 * ========================================================================= */

Test(fs2_array_api, fromstr_macro_basic) {
    __fs2_checkreset();

    fs2array a = FS2ARRFROMSTR("red", "green", "blue");

    CHECK_EQ  ("count", 3, a.count);
    CHECK_GE  ("size",  a.size, 3);
    CHECK_STR_EQ("[0]", "red",   a.arr[0].v);
    CHECK_STR_EQ("[1]", "green", a.arr[1].v);
    CHECK_STR_EQ("[2]", "blue",  a.arr[2].v);
    CHECK_TRUE ("valid", fs2arr_validate(NULL, &a));

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array_api, fromstr_macro_single) {
    __fs2_checkreset();

    fs2array a = FS2ARRFROMSTR("solo");

    CHECK_EQ  ("count", 1, a.count);
    CHECK_STR_EQ("[0]", "solo", a.arr[0].v);

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array_api, fromstr_macro_mixed_empty) {
    __fs2_checkreset();

    fs2array a = FS2ARRFROMSTR("", "x", "");

    CHECK_EQ  ("count", 3, a.count);
    CHECK_TRUE ("[0] empty", fs2_eq_empty(&a.arr[0]));
    CHECK_STR_EQ("[1]", "x", a.arr[1].v);
    CHECK_TRUE ("[2] empty", fs2_eq_empty(&a.arr[2]));

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array_api, fromstr_macro_matches_function) {
    __fs2_checkreset();
    const char *const src[] = { "a", "b", "c", NULL };

    fs2array via_func = fs2arr_fromstrarr_new(src);
    fs2array via_macro = FS2ARRFROMSTR("a", "b", "c");

    CHECK_TRUE("eqexact", fs2arr_equalexact(&via_func, &via_macro));

    fs2arr_free(&via_func);
    fs2arr_free(&via_macro);
    assert_balanced();
}

/* =========================================================================
 * Integration: fromstrarr → sort → merge
 * ========================================================================= */

Test(fs2_array_api, fromstrarr_sort_merge_pipeline) {
    __fs2_checkreset();

    fs2array left  = FS2ARRFROMSTR("delta", "alpha", "charlie");
    fs2array right = FS2ARRFROMSTR("echo", "bravo", "fox");

    fs2arr_sortasc(&left);
    fs2arr_sortasc(&right);

    fs2array merged = fs2arr_mergesorted_new(&left, &right, fs2_cmp_wrap);

    const char *expected[] = {
        "alpha", "bravo", "charlie", "delta", "echo", "fox"
    };
    for (size_t i = 0; i < 6; i++)
        CHECK_STR_EQ("merged", expected[i], merged.arr[i].v);

    fs2arr_free(&left);
    fs2arr_free(&right);
    fs2arr_free(&merged);
    assert_balanced();
}

/* =========================================================================
 * fromarr_new — deep copy
 * ========================================================================= */

Test(fs2_array_api, fromarr_new_basic) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("a");
    fs2 b = fs2_fromstr("b");
    fs2 c = fs2_fromstr("c");
    const fs2 *src[] = { &a, &b, &c };

    fs2array arr = fs2arr_fromarr_new(src, 3);

    CHECK_EQ  ("count", 3, arr.count);
    CHECK_EQ  ("hwm",   3, arr.hwm);
    CHECK_TRUE("size >= 3", arr.size >= 3);
    CHECK_STR_EQ("[0]", "a", arr.arr[0].v);
    CHECK_STR_EQ("[1]", "b", arr.arr[1].v);
    CHECK_STR_EQ("[2]", "c", arr.arr[2].v);
    CHECK_TRUE("valid", fs2arr_validate(NULL, &arr));

    fs2arr_free(&arr);
    fs2_free(&a); fs2_free(&b); fs2_free(&c);
    assert_balanced();
}

Test(fs2_array_api, fromarr_new_deep_copy) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("hello");
    const fs2 *src[] = { &a };

    fs2array arr = fs2arr_fromarr_new(src, 1);
    cr_assert_eq(arr.count, 1);
    CHECK_STR_EQ("[0]", "hello", arr.arr[0].v);

    /* independent buffer */
    CHECK_TRUE("[0] != src buf", arr.arr[0].v != a.v);

    /* mutate source, array unaffected */
    a.v[0] = 'X';
    CHECK_STR_EQ("src mutated",       "Xello", a.v);
    CHECK_STR_EQ("array still hello", "hello", arr.arr[0].v);

    fs2arr_free(&arr);
    fs2_free(&a);
    assert_balanced();
}

Test(fs2_array_api, fromarr_new_zero_cnt_canonical) {
    const fs2 *src[] = { NULL };            /* src non-NULL, but cnt=0 */
    fs2array arr = fs2arr_fromarr_new(src, 0);

    CHECK_PTR_EQ("arr",   NULL, arr.arr);
    CHECK_EQ   ("count", 0, arr.count);
    CHECK_EQ   ("hwm",   0, arr.hwm);
}

Test(fs2_array_api, fromarr_new_null_src_aborts, .signal = SIGABRT) {
    fs2arr_fromarr_new(NULL, 3);
}

Test(fs2_array_api, fromarr_new_null_entry_aborts, .signal = SIGINT) {
    const fs2 *src[] = { NULL, NULL };
    fs2arr_fromarr_new(src, 2);
}

/* =========================================================================
 * fromarr_move — move, source reset
 * ========================================================================= */

Test(fs2_array_api, fromarr_move_basic) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("a");
    fs2 b = fs2_fromstr("b");
    fs2 c = fs2_fromstr("c");
    fs2 *src[] = { &a, &b, &c };

    fs2array arr = fs2arr_fromarr_move(src, 3);

    CHECK_EQ  ("count", 3, arr.count);
    CHECK_EQ  ("hwm",   3, arr.hwm);
    CHECK_STR_EQ("[0]", "a", arr.arr[0].v);
    CHECK_STR_EQ("[1]", "b", arr.arr[1].v);
    CHECK_STR_EQ("[2]", "c", arr.arr[2].v);

    /* Sources reset to FS2EMPTY by fs2_move */
    CHECK_TRUE("a FS2EMPTY", fs2_eq_empty(&a));
    CHECK_TRUE("b FS2EMPTY", fs2_eq_empty(&b));
    CHECK_TRUE("c FS2EMPTY", fs2_eq_empty(&c));

    /* fs2_free on reset sources — no-op */
    fs2_free(&a); fs2_free(&b); fs2_free(&c);
    fs2arr_free(&arr);
    assert_balanced();
}

/* Source buffers moved, no copies */
Test(fs2_array_api, fromarr_move_transfers_buffers) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("hello");
    void *orig = a.v;
    fs2 *src[] = { &a };

    fs2array arr = fs2arr_fromarr_move(src, 1);

    /* Same buffer, moved (not cloned) */
    CHECK_PTR_EQ("buffer transferred", orig, arr.arr[0].v);

    /* source reset */
    CHECK_TRUE("a FS2EMPTY", fs2_eq_empty(&a));

    fs2arr_free(&arr);
    assert_balanced();
}

/* Heap source: struct shell stays alive, caller must free */
Test(fs2_array_api, fromarr_move_heap_source_shell_remains) {
    __fs2_checkreset();
    fs2 *p = fs2_heap_fromstr("heap");
    fs2 *src[] = { p };

    fs2array arr = fs2arr_fromarr_move(src, 1);

    CHECK_STR_EQ("[0]", "heap", arr.arr[0].v);
    CHECK_FALSE("[0] not bodyalloc", fs2_is_bodyalloc(&arr.arr[0]));

    /* p still points to the shell (struct alive, buffer moved) */
    cr_assert_not_null(p);
    CHECK_TRUE("p FS2EMPTY",    fs2_is_empty(p));
    CHECK_TRUE("p BODYALLOC",   fs2_is_bodyalloc(p));

    fs2_free(p);                            /* caller frees shell */
    fs2arr_free(&arr);
    assert_balanced();
}

Test(fs2_array_api, fromarr_move_zero_cnt_canonical) {
    fs2 *src[] = { NULL };
    fs2array arr = fs2arr_fromarr_move(src, 0);

    CHECK_PTR_EQ("arr",   NULL, arr.arr);
    CHECK_EQ   ("count", 0, arr.count);
}

Test(fs2_array_api, fromarr_move_null_src_aborts, .signal = SIGABRT) {
    fs2arr_fromarr_move(NULL, 3);
}

Test(fs2_array_api, fromarr_move_null_entry_aborts, .signal = SIGINT) {
    fs2 *src[] = { NULL, NULL };
    fs2arr_fromarr_move(src, 2);
}

/* =========================================================================
 * new vs move — difference in source state
 * ========================================================================= */

Test(fs2_array_api, fromarr_new_vs_move_source_state) {
    __fs2_checkreset();
    fs2 a1 = fs2_fromstr("x");
    fs2 a2 = fs2_fromstr("x");

    const fs2 *src_new[] = { &a1 };
    fs2       *src_move[] = { &a2 };

    fs2array arr_new  = fs2arr_fromarr_new(src_new, 1);
    fs2array arr_move = fs2arr_fromarr_move(src_move, 1);

    /* Both arrays hold "x" */
    CHECK_STR_EQ("[0] new",  "x", arr_new.arr[0].v);
    CHECK_STR_EQ("[0] move", "x", arr_move.arr[0].v);

    /* new: source untouched; move: source reset */
    CHECK_STR_EQ("a1 untouched", "x", a1.v);
    CHECK_TRUE ("a2 reset",      fs2_eq_empty(&a2));

    /* new: independent buffer (clone); move: transferred buffer */
    CHECK_TRUE("[0] new is independent", arr_new.arr[0].v != a1.v);

    fs2arr_free(&arr_new);
    fs2arr_free(&arr_move);
    fs2_free(&a1);
    fs2_free(&a2);
    assert_balanced();
}

/* =========================================================================
 * Integration
 * ========================================================================= */

Test(fs2_array_api, fromarr_new_then_sort) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("c");
    fs2 b = fs2_fromstr("a");
    fs2 c = fs2_fromstr("b");
    const fs2 *src[] = { &a, &b, &c };

    fs2array arr = fs2arr_fromarr_new(src, 3);
    fs2arr_sortasc(&arr);

    CHECK_STR_EQ("[0]", "a", arr.arr[0].v);
    CHECK_STR_EQ("[1]", "b", arr.arr[1].v);
    CHECK_STR_EQ("[2]", "c", arr.arr[2].v);

    /* sources untouched */
    CHECK_STR_EQ("a untouched", "c", a.v);
    CHECK_STR_EQ("b untouched", "a", b.v);
    CHECK_STR_EQ("c untouched", "b", c.v);

    fs2arr_free(&arr);
    fs2_free(&a); fs2_free(&b); fs2_free(&c);
    assert_balanced();
}

Test(fs2_array_api, fromarr_move_then_compact_noop) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("a");
    fs2 b = fs2_fromstr("b");
    fs2 *src[] = { &a, &b };

    fs2array arr = fs2arr_fromarr_move(src, 2);
    /* already packed */
    fs2arr_compact(&arr);                   /* no-op */

    CHECK_EQ  ("hwm",   2, arr.hwm);
    CHECK_EQ  ("count", 2, arr.count);

    fs2arr_free(&arr);
    assert_balanced();
}

/* =========================================================================
 * fs2arr_unique — nominal (packed, sorted input)
 * ========================================================================= */

Test(fs2_array_api, unique_removes_adjacent_dups) {
    __fs2_checkreset();
    fs2array a = FS2ARRFROMSTR("a", "a", "b", "b", "b", "c");
    /* packed, sorted: [a,a,b,b,b,c] */

    fs2array *r = fs2arr_unique(&a, fs2_cmp_wrap);

    CHECK_PTR_EQ("chaining", r, &a);
    CHECK_EQ  ("count", 3, a.count);
    CHECK_EQ  ("hwm",   3, a.hwm);             /* packed: hwm == count */
    CHECK_TRUE("size >= 3", a.size >= 3);
    CHECK_STR_EQ("[0]", "a", a.arr[0].v);
    CHECK_STR_EQ("[1]", "b", a.arr[1].v);
    CHECK_STR_EQ("[2]", "c", a.arr[2].v);

    /* slots [3, old_count) — holes (freed) */
    for (size_t i = 3; i < 6; i++)
        CHECK_TRUE("[i] hole", fs2_is_hole(&a.arr[i]));

    CHECK_TRUE("valid", fs2arr_validate(NULL, &a));

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array_api, unique_no_duplicates_noop) {
    __fs2_checkreset();
    fs2array a = FS2ARRFROMSTR("a", "b", "c");
    /* write == read every step → no movement */

    fs2arr_unique(&a, fs2_cmp_wrap);

    CHECK_EQ  ("count", 3, a.count);
    CHECK_EQ  ("hwm",   3, a.hwm);
    CHECK_STR_EQ("[0]", "a", a.arr[0].v);
    CHECK_STR_EQ("[1]", "b", a.arr[1].v);
    CHECK_STR_EQ("[2]", "c", a.arr[2].v);

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array_api, unique_all_same) {
    __fs2_checkreset();
    fs2array a = FS2ARRFROMSTR("x", "x", "x", "x");

    fs2arr_unique(&a, fs2_cmp_wrap);

    CHECK_EQ  ("count", 1, a.count);
    CHECK_EQ  ("hwm",   1, a.hwm);
    CHECK_STR_EQ("[0]", "x", a.arr[0].v);

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array_api, unique_long_run_in_middle) {
    __fs2_checkreset();
    fs2array a = FS2ARRFROMSTR("a", "b", "b", "b", "b", "c", "d", "d");

    fs2arr_unique(&a, fs2_cmp_wrap);

    CHECK_EQ("count", 4, a.count);
    CHECK_STR_EQ("[0]", "a", a.arr[0].v);
    CHECK_STR_EQ("[1]", "b", a.arr[1].v);
    CHECK_STR_EQ("[2]", "c", a.arr[2].v);
    CHECK_STR_EQ("[3]", "d", a.arr[3].v);

    fs2arr_free(&a);
    assert_balanced();
}

/* =========================================================================
 * fs2arr_unique — edge-cases
 * ========================================================================= */

Test(fs2_array_api, unique_empty_and_single) {
    fs2array empty = FS2ARR();
    fs2arr_unique(&empty, fs2_cmp_wrap);
    CHECK_PTR_EQ("arr", NULL, empty.arr);
    CHECK_EQ   ("count", 0, empty.count);

    __fs2_checkreset();
    fs2array single = FS2ARRFROMSTR("x");
    fs2arr_unique(&single, fs2_cmp_wrap);
    CHECK_EQ  ("count", 1, single.count);
    CHECK_STR_EQ("[0]", "x", single.arr[0].v);
    fs2arr_free(&single);
    assert_balanced();
}

/* =========================================================================
 * fs2arr_unique — custom comparator (ci: "a" and "A" are duplicates)
 * ========================================================================= */

Test(fs2_array_api, unique_ci_keeps_first_of_ci_equal) {
    __fs2_checkreset();
    /* Sorted with ci comparator: "A" and "a" are equal (adjacent).
     * Sorted input: "A", "a", "B", "b" */
    fs2array a = FS2ARRFROMSTR("A", "a", "B", "b");
    /* Ensure ci sorting: swap test — build manually sorted-by-ci */
    /* "A" < "a" < "B" < "b" in ASCII; ci-cmp treats A==a, B==b */
    fs2arr_sort(&a, fs2_cmp_ci_wrap);
    /* After ci sort — "A","a","B","b" (stable not guaranteed but equal run) */

    fs2arr_unique(&a, fs2_cmp_ci_wrap);

    /* Result: 2 distinct under ci — first of each run.
     * First of run "A"/"a" — which one is "first in sorted order" depends
     * on the sort comparator's tiebreak. With plain fs2_cmp_ci_wrap, ties
     * keep qsort's order — non-deterministic across libc. Test only count
     * and that result is case-insensitive-unique. */
    CHECK_EQ("count", 2, a.count);
    CHECK_EQ("cmp-distinct [0] vs [1]",
             fs2_cmp_ci(&a.arr[0], &a.arr[1]) != 0, true);

    fs2arr_free(&a);
    assert_balanced();
}

/* =========================================================================
 * fs2arr_unique — precondition violation
 * ========================================================================= */

Test(fs2_array_api, unique_not_packed_aborts, .signal = SIGINT) {
    __fs2_checkreset();
    fs2array a = FS2ARRFROMSTR("a", "b", "c");
    fs2 t = fs2arr_detach_new(&a, 1);
    fs2_free(&t);
    /* hwm=3, count=2 — has hole */
    fs2arr_unique(&a, fs2_cmp_wrap);
    /* unreachable */
}

Test(fs2_array_api, unique_null_arr_aborts, .signal = SIGABRT) {
    fs2arr_unique(NULL, fs2_cmp_wrap);
}

Test(fs2_array_api, unique_null_cmp_aborts, .signal = SIGABRT) {
    fs2array a = FS2ARRFROMSTR("a");
    fs2arr_unique(&a, NULL);
}

/* =========================================================================
 * fs2arr_unique_new — deep copy, src untouched
 * ========================================================================= */

Test(fs2_array_api, unique_new_src_untouched) {
    __fs2_checkreset();
    fs2array src = FS2ARRFROMSTR("a", "a", "b");
    void *src0 = src.arr[0].v;

    fs2array r = fs2arr_unique_new(&src, fs2_cmp_wrap);

    CHECK_EQ  ("r.count", 2, r.count);
    CHECK_STR_EQ("r[0]",  "a", r.arr[0].v);
    CHECK_STR_EQ("r[1]",  "b", r.arr[1].v);

    /* src unchanged */
    CHECK_EQ  ("src.count", 3, src.count);
    CHECK_PTR_EQ("src[0].v unchanged", src.arr[0].v, src0);
    CHECK_STR_EQ("src[0]", "a", src.arr[0].v);

    /* independent buffers */
    CHECK_TRUE("r[0] != src[0]", r.arr[0].v != src.arr[0].v);

    fs2arr_free(&src);
    fs2arr_free(&r);
    assert_balanced();
}

Test(fs2_array_api, unique_new_packs_holes_in_src) {
    __fs2_checkreset();
    fs2array src = FS2ARRFROMSTR("a", "x", "a", "b");
    fs2arr_sortasc(&src);                       /* [a, a, b, x] */
    fs2 t = fs2arr_detach_new(&src, 3);         /* hole at [3] */
    fs2_free(&t);
    /* src: [a, a, b, hole], hwm=4, count=3 — not packed */

    fs2array r = fs2arr_unique_new(&src, fs2_cmp_wrap);

    /* clone packs → [a, a, b], then unique → [a, b] */
    CHECK_EQ  ("r.count", 2, r.count);
    CHECK_STR_EQ("r[0]", "a", r.arr[0].v);
    CHECK_STR_EQ("r[1]", "b", r.arr[1].v);

    fs2arr_free(&src);
    fs2arr_free(&r);
    assert_balanced();
}

/* =========================================================================
 * Integration: sort → unique → expected set
 * ========================================================================= */

Test(fs2_array_api, sort_unique_pipeline) {
    __fs2_checkreset();
    fs2array a = FS2ARRFROMSTR(
        "cherry", "apple", "banana", "apple", "cherry", "apple");

    fs2arr_sortasc(&a);
    /* [apple, apple, apple, banana, cherry, cherry] */

    fs2arr_unique(&a, fs2_cmp_wrap);

    CHECK_EQ  ("count", 3, a.count);
    CHECK_STR_EQ("[0]", "apple",  a.arr[0].v);
    CHECK_STR_EQ("[1]", "banana", a.arr[1].v);
    CHECK_STR_EQ("[2]", "cherry", a.arr[2].v);
    CHECK_TRUE("valid", fs2arr_validate(NULL, &a));

    fs2arr_free(&a);
    assert_balanced();
}

/* split → sort → unique → concat-back gives canonical csv */
Test(fs2_array_api, split_sort_unique_roundtrip) {
    __fs2_checkreset();
    fs2 csv = fs2_fromstr("a,c,a,b,c,c,a,b");
    fs2 comma = fs2_fromstr(",");

    fs2array parts = fs2_split_toarray(&csv, &comma);
    fs2arr_sortasc(&parts);
    fs2arr_unique(&parts, fs2_cmp_wrap);

    CHECK_EQ  ("unique count", 3, parts.count);
    CHECK_STR_EQ("[0]", "a", parts.arr[0].v);
    CHECK_STR_EQ("[1]", "b", parts.arr[1].v);
    CHECK_STR_EQ("[2]", "c", parts.arr[2].v);

    fs2_free(&csv); fs2_free(&comma);
    fs2arr_free(&parts);
    assert_balanced();
}

/* =========================================================================
 * fs2arr_join_delim — basic
 * ========================================================================= */

Test(fs2_array_api, join_delim_basic) {
    __fs2_checkreset();
    fs2array src = FS2ARRFROMSTR("a", "b", "c");
    fs2 dst = FS2();
    fs2 d = fs2_fromstr("|");

    fs2 *r = fs2arr_join_delim(&dst, &src, &d);

    CHECK_PTR_EQ("chaining", r, &dst);
    CHECK_STR_EQ("a|b|c",    dst.v, "a|b|c");
    CHECK_EQ  ("len",        5, dst.len);
    CHECK_FS2VALID(1, &dst);

    fs2arr_free(&src);
    fs2_free(&d);
    fs2_free(&dst);
    assert_balanced();
}

Test(fs2_array_api, join_delim_single_element_no_delim) {
    __fs2_checkreset();
    fs2array src = FS2ARRFROMSTR("solo");
    fs2 dst = FS2();
    fs2 d = fs2_fromstr("|");

    fs2arr_join_delim(&dst, &src, &d);

    CHECK_STR_EQ("solo", dst.v, "solo");   /* no separator for single */

    fs2arr_free(&src);
    fs2_free(&d);
    fs2_free(&dst);
    assert_balanced();
}

/* =========================================================================
 * fs2arr_join_delim — dst as first element
 * ========================================================================= */

Test(fs2_array_api, join_delim_dst_as_first_element) {
    __fs2_checkreset();
    fs2array src = FS2ARRFROMSTR("a", "b");
    fs2 dst = fs2_fromstr("x");
    fs2 d = fs2_fromstr("|");

    fs2arr_join_delim(&dst, &src, &d);

    CHECK_STR_EQ("x|a|b", dst.v, "x|a|b");

    fs2arr_free(&src);
    fs2_free(&d);
    fs2_free(&dst);
    assert_balanced();
}

Test(fs2_array_api, join_delim_empty_dst_no_leading_delim) {
    __fs2_checkreset();
    fs2array src = FS2ARRFROMSTR("a", "b");
    fs2 dst = FS2();                       /* empty, v == NULL */
    fs2 d = fs2_fromstr("|");

    fs2arr_join_delim(&dst, &src, &d);

    CHECK_STR_EQ("a|b", dst.v, "a|b");     /* no leading | */

    fs2arr_free(&src);
    fs2_free(&d);
    fs2_free(&dst);
    assert_balanced();
}

/* =========================================================================
 * fs2arr_join_delim — NULL / empty delim (no separator)
 * ========================================================================= */

Test(fs2_array_api, join_delim_null_no_separator) {
    __fs2_checkreset();
    fs2array src = FS2ARRFROMSTR("a", "b", "c");
    fs2 dst = FS2();

    fs2arr_join_delim(&dst, &src, NULL);

    CHECK_STR_EQ("abc", dst.v, "abc");

    fs2arr_free(&src);
    fs2_free(&dst);
    assert_balanced();
}

Test(fs2_array_api, join_delim_empty_no_separator) {
    __fs2_checkreset();
    fs2array src = FS2ARRFROMSTR("a", "b");
    fs2 dst = FS2();
    fs2 d = FS2EMPTY;

    fs2arr_join_delim(&dst, &src, &d);

    CHECK_STR_EQ("ab", dst.v, "ab");

    fs2arr_free(&src);
    fs2_free(&dst);
    assert_balanced();
}

/* =========================================================================
 * fs2arr_join_delim — empty live elements preserved
 * ========================================================================= */

Test(fs2_array_api, join_delim_empty_strings_kept) {
    __fs2_checkreset();
    fs2array src = FS2ARRFROMSTR("", "a", "");
    fs2 dst = FS2();
    fs2 d = fs2_fromstr(",");

    fs2arr_join_delim(&dst, &src, &d);

    /* Python: ",".join(["", "a", ""]) == ",a," */
    CHECK_STR_EQ(",a,", dst.v, ",a,");

    fs2arr_free(&src);
    fs2_free(&d);
    fs2_free(&dst);
    assert_balanced();
}

/* =========================================================================
 * fs2arr_join_delim — holes in src skipped
 * ========================================================================= */

Test(fs2_array_api, join_delim_skips_holes) {
    __fs2_checkreset();
    fs2array src = FS2ARRFROMSTR("a", "b", "c", "d");
    fs2 t = fs2arr_detach_new(&src, 1);   /* hole at [1] */
    fs2_free(&t);
    fs2 t2 = fs2arr_detach_new(&src, 2);  /* hole at [2] (was "c") */
    fs2_free(&t2);
    /* src: [a, hole, hole, d], hwm=4, count=2 */

    fs2 dst = FS2();
    fs2 d = fs2_fromstr("|");

    fs2arr_join_delim(&dst, &src, &d);

    CHECK_STR_EQ("a|d", dst.v, "a|d");     /* only live elements joined */

    fs2arr_free(&src);
    fs2_free(&d);
    fs2_free(&dst);
    assert_balanced();
}

/* =========================================================================
 * fs2arr_join_delim — empty / canonical src no-op
 * ========================================================================= */

Test(fs2_array_api, join_delim_canonical_src_noop) {
    __fs2_checkreset();
    fs2array src = FS2ARR();
    fs2 dst = fs2_fromstr("x");
    fs2 d = fs2_fromstr("|");

    fs2arr_join_delim(&dst, &src, &d);

    CHECK_STR_EQ("x", dst.v, "x");

    fs2_free(&d);
    fs2_free(&dst);
    assert_balanced();
}

Test(fs2_array_api, join_delim_all_holes_src_noop) {
    __fs2_checkreset();
    fs2array src = fs2arr_init(4);
    fs2 v = fs2_fromstr("x");
    fs2arr_push(&src, &v);
    fs2 t = fs2arr_detach_new(&src, 0);
    fs2_free(&t);
    /* src: [hole], hwm=1, count=0 */

    fs2 dst = fs2_fromstr("y");
    fs2 d = fs2_fromstr("|");

    fs2arr_join_delim(&dst, &src, &d);

    CHECK_STR_EQ("y", dst.v, "y");

    fs2arr_free(&src);
    fs2_free(&d);
    fs2_free(&dst);
    assert_balanced();
}

/* =========================================================================
 * fs2arr_join — no-delimiter wrapper
 * ========================================================================= */

Test(fs2_array_api, join_no_delim) {
    __fs2_checkreset();
    fs2array src = FS2ARRFROMSTR("a", "b", "c");
    fs2 dst = FS2();

    fs2 *r = fs2arr_join(&dst, &src);

    CHECK_PTR_EQ("chaining", r, &dst);
    CHECK_STR_EQ("abc", dst.v, "abc");

    fs2arr_free(&src);
    fs2_free(&dst);
    assert_balanced();
}

Test(fs2_array_api, join_no_delim_with_existing_dst) {
    __fs2_checkreset();
    fs2array src = FS2ARRFROMSTR("b", "c");
    fs2 dst = fs2_fromstr("a");

    fs2arr_join(&dst, &src);

    CHECK_STR_EQ("abc", dst.v, "abc");     /* no separator */

    fs2arr_free(&src);
    fs2_free(&dst);
    assert_balanced();
}

/* =========================================================================
 * fs2arr_join_delim_str — C-string delimiter (lenient NULL)
 * ========================================================================= */

Test(fs2_array_api, join_delim_str_basic) {
    __fs2_checkreset();
    fs2array src = FS2ARRFROMSTR("a", "b", "c");
    fs2 dst = FS2();

    fs2arr_join_delim_str(&dst, &src, ", ");

    CHECK_STR_EQ("a, b, c", dst.v, "a, b, c");

    fs2arr_free(&src);
    fs2_free(&dst);
    assert_balanced();
}

Test(fs2_array_api, join_delim_str_null_no_sep) {
    __fs2_checkreset();
    fs2array src = FS2ARRFROMSTR("a", "b");
    fs2 dst = FS2();

    fs2arr_join_delim_str(&dst, &src, NULL);   /* NULL → empty → no sep */

    CHECK_STR_EQ("ab", dst.v, "ab");

    fs2arr_free(&src);
    fs2_free(&dst);
    assert_balanced();
}

/* =========================================================================
 * Guards
 * ========================================================================= */

Test(fs2_array_api, join_null_dst_aborts, .signal = SIGABRT) {
    fs2array src = FS2ARRFROMSTR("a");
    fs2arr_join(NULL, &src);
    fs2arr_free(&src);
}

Test(fs2_array_api, join_null_src_aborts, .signal = SIGABRT) {
    fs2 dst = FS2();
    fs2arr_join(&dst, NULL);
    fs2_free(&dst);
}

/* =========================================================================
 * Roundtrip: split → join → original
 * ========================================================================= */

Test(fs2_array_api, split_join_roundtrip) {
    __fs2_checkreset();
    fs2 orig = fs2_fromstr("alpha,beta,gamma");
    fs2 comma = fs2_fromstr(",");

    fs2array parts = fs2_split_toarray(&orig, &comma);
    cr_assert_eq(parts.count, 3);

    fs2 rebuilt = FS2();
    fs2arr_join_delim(&rebuilt, &parts, &comma);

    CHECK_STR_EQ("roundtrip", orig.v, rebuilt.v);
    CHECK_FS2VALID(1, &rebuilt);

    fs2_free(&orig);
    fs2_free(&comma);
    fs2arr_free(&parts);
    fs2_free(&rebuilt);
    assert_balanced();
}

/* =========================================================================
 * Integration: sort → unique → join (canonical set as CSV)
 * ========================================================================= */

Test(fs2_array_api, sort_unique_join_csv) {
    __fs2_checkreset();
    fs2array src = FS2ARRFROMSTR("c", "a", "b", "a", "c", "a");

    fs2arr_sortasc(&src);
    fs2arr_unique(&src, fs2_cmp_wrap);

    fs2 out = FS2();
    fs2arr_join_delim_str(&out, &src, ",");

    CHECK_STR_EQ("a,b,c", out.v, "a,b,c");

    fs2arr_free(&src);
    fs2_free(&out);
    assert_balanced();
}

/* =========================================================================
 * LOCAL dst — works if capacity fits
 * ========================================================================= */

Test(fs2_array_api, join_delim_local_dst) {
    __fs2_checkreset();
    fs2array src = FS2ARRFROMSTR("a", "b");
    FS2LOCAL(buf, 32);
    fs2 d = fs2_fromstr("|");

    fs2arr_join_delim(&buf, &src, &d);

    CHECK_STR_EQ("a|b",        buf.v, "a|b");
    CHECK_TRUE ("still LOCAL", fs2_is_local(&buf));
    CHECK_FS2VALID(1, &buf);

    fs2arr_free(&src);
    fs2_free(&d);
    assert_balanced();
}

/* =========================================================================
 * fs2arr_reverse — nominal
 * ========================================================================= */

Test(fs2_array_api, reverse_odd_count) {
    __fs2_checkreset();
    fs2array a = FS2ARRFROMSTR("a", "b", "c", "d", "e");

    fs2array *r = fs2arr_reverse(&a);

    CHECK_PTR_EQ("chaining", r, &a);
    CHECK_STR_EQ("[0]", "e", a.arr[0].v);
    CHECK_STR_EQ("[1]", "d", a.arr[1].v);
    CHECK_STR_EQ("[2]", "c", a.arr[2].v);
    CHECK_STR_EQ("[3]", "b", a.arr[3].v);
    CHECK_STR_EQ("[4]", "a", a.arr[4].v);

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array_api, reverse_even_count) {
    __fs2_checkreset();
    fs2array a = FS2ARRFROMSTR("a", "b", "c", "d");

    fs2arr_reverse(&a);

    CHECK_STR_EQ("[0]", "d", a.arr[0].v);
    CHECK_STR_EQ("[1]", "c", a.arr[1].v);
    CHECK_STR_EQ("[2]", "b", a.arr[2].v);
    CHECK_STR_EQ("[3]", "a", a.arr[3].v);

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array_api, reverse_single_and_empty) {
    fs2array empty = FS2ARR();
    fs2arr_reverse(&empty);                 /* no-op */
    CHECK_PTR_EQ("arr", NULL, empty.arr);

    __fs2_checkreset();
    fs2array single = FS2ARRFROMSTR("x");
    fs2arr_reverse(&single);
    CHECK_STR_EQ("[0]", "x", single.arr[0].v);
    fs2arr_free(&single);
    assert_balanced();
}

/* =========================================================================
 * fs2arr_reverse — involution (reverse × 2 == identity)
 * ========================================================================= */

Test(fs2_array_api, reverse_involution) {
    __fs2_checkreset();
    fs2array a = FS2ARRFROMSTR("a", "b", "c", "d", "e");
    fs2array orig = fs2arr_cloneexact(&a);

    fs2arr_reverse(&a);
    CHECK_FALSE("reversed ≠ original", fs2arr_equalexact(&a, &orig));

    fs2arr_reverse(&a);
    CHECK_TRUE("reverse×2 == identity", fs2arr_equalexact(&a, &orig));

    fs2arr_free(&a);
    fs2arr_free(&orig);
    assert_balanced();
}

/* =========================================================================
 * fs2arr_reverse — buffers moved, not copied
 * ========================================================================= */

Test(fs2_array_api, reverse_preserves_buffers) {
    __fs2_checkreset();
    fs2array a = FS2ARRFROMSTR("a", "b");
    void *b0 = a.arr[0].v;
    void *b1 = a.arr[1].v;

    fs2arr_reverse(&a);

    /* Pointers swapped, no new allocation */
    CHECK_PTR_EQ("[0] holds old [1] buf", a.arr[0].v, b1);
    CHECK_PTR_EQ("[1] holds old [0] buf", a.arr[1].v, b0);

    fs2arr_free(&a);
    assert_balanced();
}

/* =========================================================================
 * Precondition: not packed → abort
 * ========================================================================= */

Test(fs2_array_api, reverse_not_packed_aborts, .signal = SIGINT) {
    __fs2_checkreset();
    fs2array a = FS2ARRFROMSTR("a", "b", "c");
    fs2 t = fs2arr_detach_new(&a, 1);        /* hole at [1] */
    fs2_free(&t);
    fs2arr_reverse(&a);
    /* unreachable */
}

Test(fs2_array_api, reverse_null_aborts, .signal = SIGABRT) {
    fs2arr_reverse(NULL);
}

/* =========================================================================
 * fs2arr_reverse_new — deep copy, src untouched
 * ========================================================================= */

Test(fs2_array_api, reverse_new_src_untouched) {
    __fs2_checkreset();
    fs2array src = FS2ARRFROMSTR("a", "b", "c");

    fs2array r = fs2arr_reverse_new(&src);

    CHECK_STR_EQ("r[0]", "c", r.arr[0].v);
    CHECK_STR_EQ("r[1]", "b", r.arr[1].v);
    CHECK_STR_EQ("r[2]", "a", r.arr[2].v);

    /* src unchanged */
    CHECK_STR_EQ("src[0]", "a", src.arr[0].v);
    CHECK_STR_EQ("src[2]", "c", src.arr[2].v);

    /* independent buffers */
    CHECK_TRUE("r[0] != src[0]", r.arr[0].v != src.arr[0].v);

    fs2arr_free(&src);
    fs2arr_free(&r);
    assert_balanced();
}

Test(fs2_array_api, reverse_new_packs_holes_in_src) {
    __fs2_checkreset();
    fs2array src = FS2ARRFROMSTR("a", "x", "b", "c");
    fs2 t = fs2arr_detach_new(&src, 1);      /* hole at [1] */
    fs2_free(&t);
    /* src: [a, hole, b, c], hwm=4, count=3 — not packed */

    fs2array r = fs2arr_reverse_new(&src);

    /* clone packs → [a, b, c]; reverse → [c, b, a] */
    CHECK_EQ("r.count", 3, r.count);
    CHECK_STR_EQ("r[0]", "c", r.arr[0].v);
    CHECK_STR_EQ("r[1]", "b", r.arr[1].v);
    CHECK_STR_EQ("r[2]", "a", r.arr[2].v);

    fs2arr_free(&src);
    fs2arr_free(&r);
    assert_balanced();
}

/* =========================================================================
 * Sort + reverse vs sortdesc (relation, not equality for duplicates)
 * ========================================================================= */

Test(fs2_array_api, sort_then_reverse_unique_elements_eq_sortdesc) {
    __fs2_checkreset();
    /* unique elements (no tie-break ambiguity) */
    fs2array a = FS2ARRFROMSTR("c", "a", "e", "b", "d");
    fs2array b = fs2arr_clone(&a);

    fs2arr_sortasc(&a);
    fs2arr_reverse(&a);                       /* sorted asc, then reversed */

    fs2arr_sortdesc(&b);                      /* sorted desc */

    CHECK_TRUE("unique elems: same result",
               fs2arr_equalexact(&a, &b));

    fs2arr_free(&a);
    fs2arr_free(&b);
    assert_balanced();
}

/* =========================================================================
 * Palindrome check via reverse + eqexact
 * ========================================================================= */

Test(fs2_array_api, palindrome_check_via_reverse) {
    __fs2_checkreset();
    fs2array pal  = FS2ARRFROMSTR("a", "b", "c", "b", "a");
    fs2array nonp = FS2ARRFROMSTR("a", "b", "c", "d", "e");

    fs2array rpal  = fs2arr_reverse_new(&pal);
    fs2array rnonp = fs2arr_reverse_new(&nonp);

    CHECK_TRUE ("palindrome",     fs2arr_equalexact(&pal,  &rpal));
    CHECK_FALSE("not palindrome", fs2arr_equalexact(&nonp, &rnonp));

    fs2arr_free(&pal);  fs2arr_free(&rpal);
    fs2arr_free(&nonp); fs2arr_free(&rnonp);
    assert_balanced();
}

Test(fs2_array_api, palindrome_empty_and_single) {
    fs2array empty = FS2ARR();
    fs2array rempty = fs2arr_reverse_new(&empty);
    CHECK_TRUE("empty palindrome", fs2arr_equalexact(&empty, &rempty));

    __fs2_checkreset();
    fs2array single = FS2ARRFROMSTR("x");
    fs2array rsingle = fs2arr_reverse_new(&single);
    CHECK_TRUE("single palindrome", fs2arr_equalexact(&single, &rsingle));

    fs2arr_free(&single);
    fs2arr_free(&rsingle);
    assert_balanced();
}

/* Helper: sorted packed array from string list */
static fs2array
mk_sorted_arr(const char *vals[], size_t n, pointer_comparator_t cmp) {
    fs2array a = fs2arr_init(n);
    for (size_t i = 0; i < n; i++) {
        fs2 v = fs2_fromstr(vals[i]);
        fs2arr_push(&a, &v);
    }
    fs2arr_sort(&a, cmp);
    return a;
}

/* =========================================================================
 * fs2arr_bsearch — nominal hits
 * ========================================================================= */

Test(fs2_array_api, bsearch_found_first_middle_last) {
    __fs2_checkreset();
    fs2array a = mk_sorted_arr(
        (const char*[]){ "a", "b", "c", "d", "e" }, 5, fs2_cmp_wrap);

    fs2 k1 = fs2_fromstr("a");
    fs2 k2 = fs2_fromstr("c");
    fs2 k3 = fs2_fromstr("e");

    CHECK_EQ("first", 0, fs2arr_bsearch(&a, &k1, fs2_cmp_wrap));
    CHECK_EQ("mid",   2, fs2arr_bsearch(&a, &k2, fs2_cmp_wrap));
    CHECK_EQ("last",  4, fs2arr_bsearch(&a, &k3, fs2_cmp_wrap));

    fs2arr_free(&a);
    fs2_free(&k1); fs2_free(&k2); fs2_free(&k3);
    assert_balanced();
}

Test(fs2_array_api, bsearch_not_found_below_above_between) {
    __fs2_checkreset();
    fs2array a = mk_sorted_arr(
        (const char*[]){ "b", "d", "f" }, 3, fs2_cmp_wrap);

    fs2 below   = fs2_fromstr("a");         /* < "b" */
    fs2 above   = fs2_fromstr("z");         /* > "f" */
    fs2 between = fs2_fromstr("c");         /* "b" < c < "d" */
    fs2 empty_k = FS2EMPTY;                 /* "" < "b" */

    CHECK_EQ("below",   -1, fs2arr_bsearch(&a, &below,   fs2_cmp_wrap));
    CHECK_EQ("above",   -1, fs2arr_bsearch(&a, &above,   fs2_cmp_wrap));
    CHECK_EQ("between", -1, fs2arr_bsearch(&a, &between, fs2_cmp_wrap));
    CHECK_EQ("empty",   -1, fs2arr_bsearch(&a, &empty_k, fs2_cmp_wrap));

    fs2arr_free(&a);
    fs2_free(&below); fs2_free(&above);
    fs2_free(&between);
    assert_balanced();
}

/* =========================================================================
 * fs2arr_bsearch — empty / single / canonical key
 * ========================================================================= */

Test(fs2_array_api, bsearch_empty_array) {
    fs2array a = FS2ARR();
    fs2 k = fs2_fromstr("a");
    CHECK_EQ("empty array", -1, fs2arr_bsearch(&a, &k, fs2_cmp_wrap));
    fs2_free(&k);
}

Test(fs2_array_api, bsearch_empty_with_capacity) {
    fs2array a = fs2arr_init(16);           /* arr != NULL, count=0 */
    fs2 k = fs2_fromstr("a");
    CHECK_EQ("empty+cap", -1, fs2arr_bsearch(&a, &k, fs2_cmp_wrap));
    fs2arr_free(&a);
    fs2_free(&k);
}

Test(fs2_array_api, bsearch_single_element) {
    __fs2_checkreset();
    fs2array a = mk_sorted_arr((const char*[]){ "x" }, 1, fs2_cmp_wrap);

    fs2 k_hit  = fs2_fromstr("x");
    fs2 k_miss = fs2_fromstr("y");

    CHECK_EQ("hit",  0, fs2arr_bsearch(&a, &k_hit,  fs2_cmp_wrap));
    CHECK_EQ("miss", -1, fs2arr_bsearch(&a, &k_miss, fs2_cmp_wrap));

    fs2arr_free(&a);
    fs2_free(&k_hit); fs2_free(&k_miss);
    assert_balanced();
}

/* Canonical FS2() key (v=NULL) — promoted, no SEGV in strcmp */
Test(fs2_array_api, bsearch_canonical_key_promoted) {
    __fs2_checkreset();
    fs2array a = mk_sorted_arr(
        (const char*[]){ "a", "b", "c" }, 3, fs2_cmp_wrap);

    fs2 k = FS2();                          /* v=NULL, len=0 */
    cr_assert_null(k.v);

    /* "" not in array — should return -1 (not SEGV) */
    CHECK_EQ("not found", -1, fs2arr_bsearch(&a, &k, fs2_cmp_wrap));

    fs2arr_free(&a);
    fs2_free(&k);
    assert_balanced();
}

/* Canonical key when "" IS in array — should find it */
Test(fs2_array_api, bsearch_canonical_key_finds_empty) {
    __fs2_checkreset();
    fs2array a = mk_sorted_arr(
        (const char*[]){ "", "a", "b" }, 3, fs2_cmp_wrap);

    fs2 k = FS2();                          /* v=NULL, promoted to FS2EMPTY */
    CHECK_EQ("found empty at [0]", 0, fs2arr_bsearch(&a, &k, fs2_cmp_wrap));

    fs2arr_free(&a);
    fs2_free(&k);
    assert_balanced();
}

/* =========================================================================
 * fs2arr_bsearch — duplicates (some index, not guaranteed first)
 * ========================================================================= */

Test(fs2_array_api, bsearch_duplicates_returns_valid_index) {
    __fs2_checkreset();
    fs2array a = mk_sorted_arr(
        (const char*[]){ "a", "b", "b", "b", "c" }, 5, fs2_cmp_wrap);

    fs2 k = fs2_fromstr("b");
    ptrdiff_t i = fs2arr_bsearch(&a, &k, fs2_cmp_wrap);

    cr_assert_geq(i, 1, "b exists at [1..3]");
    cr_assert_leq(i, 3, "b exists at [1..3]");
    CHECK_STR_EQ("matched value", "b", a.arr[i].v);

    fs2arr_free(&a);
    fs2_free(&k);
    assert_balanced();
}

/* =========================================================================
 * fs2arr_bsearch — custom comparator (length-key)
 * ========================================================================= */

static int
cmp_len(const void *pa, const void *pb) {
    const fs2 *a = pa, *b = pb;
    return (a->len > b->len) - (a->len < b->len);
}

Test(fs2_array_api, bsearch_by_length) {
    __fs2_checkreset();
    /* Sorted by length: 1, 2, 2, 3, 4 */
    fs2array a = mk_sorted_arr(
        (const char*[]){ "a", "bb", "cc", "ddd", "eeee" }, 5, cmp_len);

    fs2 k1 = fs2_fromstr("X");              /* len 1 */
    fs2 k2 = fs2_fromstr("XY");             /* len 2 — hits one of [1] or [2] */
    fs2 k5 = fs2_fromstr("XXXXX");          /* len 5 — not present */

    CHECK_EQ("len 1 at 0", 0, fs2arr_bsearch(&a, &k1, cmp_len));

    ptrdiff_t i2 = fs2arr_bsearch(&a, &k2, cmp_len);
    cr_assert_geq(i2, 1);
    cr_assert_leq(i2, 2);

    CHECK_EQ("len 5 miss", -1, fs2arr_bsearch(&a, &k5, cmp_len));

    fs2arr_free(&a);
    fs2_free(&k1); fs2_free(&k2); fs2_free(&k5);
    assert_balanced();
}

/* =========================================================================
 * fs2arr_bsearch — desc comparator
 * ========================================================================= */

Test(fs2_array_api, bsearch_desc) {
    __fs2_checkreset();
    fs2array a = mk_sorted_arr(
        (const char*[]){ "a", "b", "c" }, 3, fs2_cmp_desc_wrap);
    /* sorted desc: c, b, a */

    fs2 kb = fs2_fromstr("b");
    fs2 ka = fs2_fromstr("a");
    fs2 kz = fs2_fromstr("z");

    CHECK_EQ("'b' at 1", 1, fs2arr_bsearch(&a, &kb, fs2_cmp_desc_wrap));
    CHECK_EQ("'a' at 2", 2, fs2arr_bsearch(&a, &ka, fs2_cmp_desc_wrap));
    CHECK_EQ("'z' miss", -1, fs2arr_bsearch(&a, &kz, fs2_cmp_desc_wrap));

    fs2arr_free(&a);
    fs2_free(&kb); fs2_free(&ka); fs2_free(&kz);
    assert_balanced();
}

/* =========================================================================
 * fs2arr_bsearch — consistency with fs2_bsearch (same array, no holes)
 * ========================================================================= */

Test(fs2_array_api, bsearch_matches_fs2_bsearch) {
    __fs2_checkreset();
    fs2array a = mk_sorted_arr(
        (const char*[]){ "alpha", "beta", "gamma", "delta", "epsilon" },
        5, fs2_cmp_wrap);

    const char *keys[] = { "alpha", "beta", "gamma", "zzz", "epsilon" };
    for (size_t i = 0; i < 5; i++) {
        fs2 k = fs2_fromstr(keys[i]);
        ptrdiff_t via_arr = fs2arr_bsearch(&a, &k, fs2_cmp_wrap);
        ptrdiff_t via_fs2 = fs2_bsearch(&k, a.arr, a.count);
        CHECK_EQ("agree", via_arr, via_fs2);
        fs2_free(&k);
    }

    fs2arr_free(&a);
    assert_balanced();
}

/* =========================================================================
 * fs2arr_bsearch — precondition: not packed
 * ========================================================================= */

Test(fs2_array_api, bsearch_not_packed_aborts, .signal = SIGINT) {
    __fs2_checkreset();
    fs2array a = mk_sorted_arr(
        (const char*[]){ "a", "b", "c" }, 3, fs2_cmp_wrap);
    fs2 t = fs2arr_detach_new(&a, 1);
    fs2_free(&t);
    /* a: [a, hole, c], hwm=3, count=2 */

    fs2 k = fs2_fromstr("a");
    fs2arr_bsearch(&a, &k, fs2_cmp_wrap);
    /* unreachable */
}

/* =========================================================================
 * fs2arr_bsearch — guards
 * ========================================================================= */

Test(fs2_array_api, bsearch_null_a_aborts, .signal = SIGABRT) {
    fs2 k = fs2_fromstr("a");
    fs2arr_bsearch(NULL, &k, fs2_cmp_wrap);
    fs2_free(&k);
}

Test(fs2_array_api, bsearch_null_key_aborts, .signal = SIGABRT) {
    fs2array a = FS2ARR();
    fs2arr_bsearch(&a, NULL, fs2_cmp_wrap);
}

Test(fs2_array_api, bsearch_null_cmp_aborts, .signal = SIGABRT) {
    fs2array a = FS2ARR();
    fs2 k = fs2_fromstr("a");
    fs2arr_bsearch(&a, &k, NULL);
    fs2_free(&k);
}

/* =========================================================================
 * Integration: sort → bsearch (used in loop)
 * ========================================================================= */

Test(fs2_array_api, sort_then_bsearch_loop) {
    __fs2_checkreset();
    fs2array a = FS2ARRFROMSTR("apple", "banana", "cherry", "date", "elderberry");
    fs2arr_sortasc(&a);

    struct { const char *key; ptrdiff_t expected; } cases[] = {
        { "apple",      0 },
        { "banana",     1 },
        { "cherry",     2 },
        { "date",       3 },
        { "elderberry", 4 },
        { "fig",       -1 },
        { "aaa",       -1 },
        { "zzz",       -1 },
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        fs2 k = fs2_fromstr(cases[i].key);
        CHECK_EQ("index", cases[i].expected,
                 fs2arr_bsearch(&a, &k, fs2_cmp_wrap));
        fs2_free(&k);
    }

    fs2arr_free(&a);
    assert_balanced();
}

/* =========================================================================
 * Integration: split → sort → unique → bsearch (canonical lookup)
 * ========================================================================= */

Test(fs2_array_api, split_sort_unique_bsearch) {
    __fs2_checkreset();
    fs2 csv = fs2_fromstr("banana,apple,cherry,apple,banana");
    fs2 comma = fs2_fromstr(",");

    fs2array parts = fs2_split_toarray(&csv, &comma);
    fs2arr_sortasc(&parts);
    fs2arr_unique(&parts, fs2_cmp_wrap);
    /* [apple, banana, cherry] */

    fs2 ka = fs2_fromstr("apple");
    fs2 kb = fs2_fromstr("banana");
    fs2 kd = fs2_fromstr("date");

    CHECK_EQ("apple at 0",  0, fs2arr_bsearch(&parts, &ka, fs2_cmp_wrap));
    CHECK_EQ("banana at 1", 1, fs2arr_bsearch(&parts, &kb, fs2_cmp_wrap));
    CHECK_EQ("date miss",  -1, fs2arr_bsearch(&parts, &kd, fs2_cmp_wrap));

    fs2_free(&csv); fs2_free(&comma);
    fs2arr_free(&parts);
    fs2_free(&ka); fs2_free(&kb); fs2_free(&kd);
    assert_balanced();
}

/* =========================================================================
 * fs2arr_find
 * ========================================================================= */

Test(fs2_array_api, find_hit_first_middle_last) {
    __fs2_checkreset();
    fs2array a = FS2ARRFROMSTR("a", "b", "c", "d", "e");

    fs2 k1 = fs2_fromstr("a");
    fs2 k3 = fs2_fromstr("c");
    fs2 k5 = fs2_fromstr("e");

    CHECK_EQ("first", 0, fs2arr_find(&a, &k1, fs2_cmp_wrap));
    CHECK_EQ("mid",   2, fs2arr_find(&a, &k3, fs2_cmp_wrap));
    CHECK_EQ("last",  4, fs2arr_find(&a, &k5, fs2_cmp_wrap));

    fs2arr_free(&a);
    fs2_free(&k1); fs2_free(&k3); fs2_free(&k5);
    assert_balanced();
}

Test(fs2_array_api, find_miss) {
    __fs2_checkreset();
    fs2array a = FS2ARRFROMSTR("a", "b", "c");

    fs2 k = fs2_fromstr("z");
    CHECK_EQ("miss", -1, fs2arr_find(&a, &k, fs2_cmp_wrap));

    fs2arr_free(&a);
    fs2_free(&k);
    assert_balanced();
}

Test(fs2_array_api, find_duplicates_returns_first_match) {
    __fs2_checkreset();
    fs2array a = FS2ARRFROMSTR("x", "a", "x", "b", "x");

    fs2 k = fs2_fromstr("x");
    CHECK_EQ("first x at [0]", 0, fs2arr_find(&a, &k, fs2_cmp_wrap));

    fs2arr_free(&a);
    fs2_free(&k);
    assert_balanced();
}

Test(fs2_array_api, find_works_on_unsorted) {
    __fs2_checkreset();
    fs2array a = FS2ARRFROMSTR("c", "a", "e", "b", "d");  /* not sorted */

    fs2 kb = fs2_fromstr("b");
    CHECK_EQ("'b' at [3]", 3, fs2arr_find(&a, &kb, fs2_cmp_wrap));

    fs2 kz = fs2_fromstr("z");
    CHECK_EQ("'z' miss", -1, fs2arr_find(&a, &kz, fs2_cmp_wrap));

    fs2arr_free(&a);
    fs2_free(&kb); fs2_free(&kz);
    assert_balanced();
}

Test(fs2_array_api, find_skips_holes) {
    __fs2_checkreset();
    fs2array a = FS2ARRFROMSTR("a", "b", "c", "d");
    fs2 t = fs2arr_detach_new(&a, 1);         /* hole at [1] */
    fs2_free(&t);
    /* [a, hole, c, d] */

    fs2 kc = fs2_fromstr("c");
    CHECK_EQ("'c' at [2]", 2, fs2arr_find(&a, &kc, fs2_cmp_wrap));

    fs2 ka = fs2_fromstr("a");
    CHECK_EQ("'a' at [0]", 0, fs2arr_find(&a, &ka, fs2_cmp_wrap));

    fs2 kb = fs2_fromstr("b");                /* was at hole */
    CHECK_EQ("'b' gone", -1, fs2arr_find(&a, &kb, fs2_cmp_wrap));

    fs2arr_free(&a);
    fs2_free(&kc); fs2_free(&ka); fs2_free(&kb);
    assert_balanced();
}

Test(fs2_array_api, find_empty_array) {
    fs2array canon = FS2ARR();
    fs2 k = fs2_fromstr("a");
    CHECK_EQ("canonical", -1, fs2arr_find(&canon, &k, fs2_cmp_wrap));

    fs2array empty = fs2arr_init(16);
    CHECK_EQ("empty+cap", -1, fs2arr_find(&empty, &k, fs2_cmp_wrap));

    fs2arr_free(&empty);
    fs2_free(&k);
}

Test(fs2_array_api, find_canonical_key_promoted) {
    __fs2_checkreset();
    fs2array a = FS2ARRFROMSTR("", "a", "b");      /* live empty at [0] */

    fs2 k = FS2();                             /* v=NULL */
    cr_assert_null(k.v);
    CHECK_EQ("finds empty", 0, fs2arr_find(&a, &k, fs2_cmp_wrap));

    fs2arr_free(&a);
    fs2_free(&k);
    assert_balanced();
}

static int cmp_len_wrap(const void *pa, const void *pb) {
        const fs2 *x = pa, *y = pb;
        return (x->len > y->len) - (x->len < y->len);
    }

Test(fs2_array_api, find_custom_cmp_length) {
    __fs2_checkreset();
    /* find by length only */
    fs2array a = FS2ARRFROMSTR("a", "bb", "ccc", "dddd");
    fs2 k2 = fs2_fromstr("XY");               /* len 2 */

    CHECK_EQ("len 2 at [1]", 1, fs2arr_find(&a, &k2, cmp_len_wrap));

    fs2arr_free(&a);
    fs2_free(&k2);
    assert_balanced();
}

Test(fs2_array_api, find_null_aborts, .signal = SIGABRT) {
    fs2 k = fs2_fromstr("a");
    fs2arr_find(NULL, &k, fs2_cmp_wrap);
    fs2_free(&k);
}

Test(fs2_array_api, find_null_key_aborts, .signal = SIGABRT) {
    fs2array a = FS2ARRFROMSTR("a");
    fs2arr_find(&a, NULL, fs2_cmp_wrap);
    fs2arr_free(&a);
}

Test(fs2_array_api, find_null_cmp_aborts, .signal = SIGABRT) {
    fs2array a = FS2ARRFROMSTR("a");
    fs2 k = fs2_fromstr("a");
    fs2arr_find(&a, &k, NULL);
    fs2_free(&k);
    fs2arr_free(&a);
}

/* =========================================================================
 * fs2arr_contains
 * ========================================================================= */

Test(fs2_array_api, contains_positive_negative) {
    __fs2_checkreset();
    fs2array a = FS2ARRFROMSTR("apple", "banana", "cherry");

    fs2 hit  = fs2_fromstr("banana");
    fs2 miss = fs2_fromstr("grape");

    CHECK_TRUE ("banana", fs2arr_contains(&a, &hit,  fs2_cmp_wrap));
    CHECK_FALSE("grape",  fs2arr_contains(&a, &miss, fs2_cmp_wrap));

    fs2arr_free(&a);
    fs2_free(&hit); fs2_free(&miss);
    assert_balanced();
}

Test(fs2_array_api, contains_skips_holes) {
    __fs2_checkreset();
    fs2array a = FS2ARRFROMSTR("a", "b", "c");
    fs2 t = fs2arr_detach_new(&a, 1);         /* hole at [1], "b" removed */
    fs2_free(&t);

    fs2 ka = fs2_fromstr("a");
    fs2 kb = fs2_fromstr("b");

    CHECK_TRUE ("'a' still", fs2arr_contains(&a, &ka, fs2_cmp_wrap));
    CHECK_FALSE("'b' gone",  fs2arr_contains(&a, &kb, fs2_cmp_wrap));

    fs2arr_free(&a);
    fs2_free(&ka); fs2_free(&kb);
    assert_balanced();
}

Test(fs2_array_api, contains_empty_array_false) {
    fs2array a = FS2ARR();
    fs2 k = fs2_fromstr("a");
    CHECK_FALSE("empty", fs2arr_contains(&a, &k, fs2_cmp_wrap));
    fs2_free(&k);
}

/* =========================================================================
 * fs2arr_count
 * ========================================================================= */

Test(fs2_array_api, count_occurrences) {
    __fs2_checkreset();
    fs2array a = FS2ARRFROMSTR("a", "b", "a", "c", "a", "b");

    fs2 ka = fs2_fromstr("a");
    fs2 kb = fs2_fromstr("b");
    fs2 kc = fs2_fromstr("c");
    fs2 kz = fs2_fromstr("z");

    CHECK_EQ("a ×3", 3, fs2arr_count(&a, &ka, fs2_cmp_wrap));
    CHECK_EQ("b ×2", 2, fs2arr_count(&a, &kb, fs2_cmp_wrap));
    CHECK_EQ("c ×1", 1, fs2arr_count(&a, &kc, fs2_cmp_wrap));
    CHECK_EQ("z ×0", 0, fs2arr_count(&a, &kz, fs2_cmp_wrap));

    fs2arr_free(&a);
    fs2_free(&ka); fs2_free(&kb); fs2_free(&kc); fs2_free(&kz);
    assert_balanced();
}

Test(fs2_array_api, count_skips_holes) {
    __fs2_checkreset();
    fs2array a = FS2ARRFROMSTR("a", "a", "a");
    fs2 t = fs2arr_detach_new(&a, 1);         /* hole at [1] — was "a" */
    fs2_free(&t);

    fs2 ka = fs2_fromstr("a");
    CHECK_EQ("a ×2 after hole", 2, fs2arr_count(&a, &ka, fs2_cmp_wrap));

    fs2arr_free(&a);
    fs2_free(&ka);
    assert_balanced();
}

Test(fs2_array_api, count_all_same) {
    __fs2_checkreset();
    fs2array a = FS2ARRFROMSTR("x", "x", "x", "x", "x");

    fs2 kx = fs2_fromstr("x");
    CHECK_EQ("x ×5", 5, fs2arr_count(&a, &kx, fs2_cmp_wrap));

    fs2arr_free(&a);
    fs2_free(&kx);
    assert_balanced();
}

Test(fs2_array_api, count_empty_array) {
    fs2array a = FS2ARR();
    fs2 k = fs2_fromstr("a");
    CHECK_EQ("empty", 0, fs2arr_count(&a, &k, fs2_cmp_wrap));
    fs2_free(&k);
}

/* =========================================================================
 * Integration: contains/count/find after sort/unique/detach
 * ========================================================================= */

Test(fs2_array_api, find_contains_count_lifecycle) {
    __fs2_checkreset();
    fs2array a = FS2ARRFROMSTR("banana", "apple", "cherry", "apple", "banana", "date");

    fs2 ka = fs2_fromstr("apple");
    fs2 kb = fs2_fromstr("banana");
    fs2 kz = fs2_fromstr("zebra");

    /* Phase 1: unsorted */
    CHECK_EQ   ("apple count pre-sort", 2, fs2arr_count(&a, &ka, fs2_cmp_wrap));
    CHECK_TRUE ("apple present",           fs2arr_contains(&a, &ka, fs2_cmp_wrap));
    ptrdiff_t p1 = fs2arr_find(&a, &kb, fs2_cmp_wrap);
    cr_assert_geq(p1, 0);
    CHECK_STR_EQ("find banana", "banana", a.arr[p1].v);

    /* Phase 2: sort — same multiset, different positions */
    fs2arr_sortasc(&a);
    CHECK_EQ   ("apple count post-sort", 2, fs2arr_count(&a, &ka, fs2_cmp_wrap));
    CHECK_TRUE ("apple still present",     fs2arr_contains(&a, &ka, fs2_cmp_wrap));
    CHECK_EQ   ("find apple",              fs2arr_find(&a, &ka, fs2_cmp_wrap), 0);
    CHECK_EQ   ("zebra miss",             fs2arr_find(&a, &kz, fs2_cmp_wrap), -1);

    /* Phase 3: unique — 1 apple, 1 banana */
    fs2arr_unique(&a, fs2_cmp_wrap);
    CHECK_EQ("apple after unique",  1, fs2arr_count(&a, &ka, fs2_cmp_wrap));
    CHECK_EQ("banana after unique", 1, fs2arr_count(&a, &kb, fs2_cmp_wrap));

    /* Phase 4: detach — remove apple */
    fs2 t = fs2arr_detach_new(&a, (size_t)fs2arr_find(&a, &ka, fs2_cmp_wrap));
    fs2_free(&t);
    CHECK_FALSE("apple gone", fs2arr_contains(&a, &ka, fs2_cmp_wrap));
    CHECK_TRUE ("banana still", fs2arr_contains(&a, &kb, fs2_cmp_wrap));

    fs2arr_free(&a);
    fs2_free(&ka); fs2_free(&kb); fs2_free(&kz);
    assert_balanced();
}

/* =========================================================================
 * _str variants — zero-alloc wrappers
 * ========================================================================= */

Test(fs2_array_api, find_str_basic) {
    __fs2_checkreset();
    fs2array a = FS2ARRFROMSTR("apple", "banana", "cherry");

    CHECK_EQ("apple",  0, fs2arr_find_str(&a, "apple",  fs2_cmp_wrap));
    CHECK_EQ("cherry", 2, fs2arr_find_str(&a, "cherry", fs2_cmp_wrap));
    CHECK_EQ("grape", -1, fs2arr_find_str(&a, "grape",  fs2_cmp_wrap));

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array_api, find_str_skips_holes) {
    __fs2_checkreset();
    fs2array a = FS2ARRFROMSTR("a", "b", "c");
    fs2 t = fs2arr_detach_new(&a, 1);
    fs2_free(&t);
    /* [a, hole, c] */

    CHECK_EQ("'a'",  0, fs2arr_find_str(&a, "a", fs2_cmp_wrap));
    CHECK_EQ("'b'", -1, fs2arr_find_str(&a, "b", fs2_cmp_wrap));
    CHECK_EQ("'c'",  2, fs2arr_find_str(&a, "c", fs2_cmp_wrap));

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array_api, find_str_null_key_searches_empty) {
    __fs2_checkreset();
    fs2array a = FS2ARRFROMSTR("", "a", "b");    /* live empty at [0] */

    /* _fs2_wrap(NULL) → FS2EMPTY (live empty) → matches slot [0] */
    CHECK_EQ("finds empty", 0, fs2arr_find_str(&a, NULL, fs2_cmp_wrap));

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array_api, find_str_matches_fs2_version) {
    __fs2_checkreset();
    fs2array a = FS2ARRFROMSTR("x", "y", "z");
    fs2 kx = fs2_fromstr("x");

    CHECK_EQ("same result",
             fs2arr_find(&a, &kx, fs2_cmp_wrap),
             fs2arr_find_str(&a, "x", fs2_cmp_wrap));

    fs2arr_free(&a);
    fs2_free(&kx);
    assert_balanced();
}

Test(fs2_array_api, contains_str_basic) {
    __fs2_checkreset();
    fs2array a = FS2ARRFROMSTR("apple", "banana");

    CHECK_TRUE ("banana", fs2arr_contains_str(&a, "banana", fs2_cmp_wrap));
    CHECK_FALSE("grape",  fs2arr_contains_str(&a, "grape",  fs2_cmp_wrap));
    CHECK_FALSE("Banana (case-sensitive)", fs2arr_contains_str(&a, "Banana", fs2_cmp_wrap));
    CHECK_TRUE ("Banana (ci)",             fs2arr_contains_str(&a, "Banana", fs2_cmp_ci_wrap));

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array_api, contains_str_empty_array) {
    fs2array a = FS2ARR();
    CHECK_FALSE("empty", fs2arr_contains_str(&a, "x", fs2_cmp_wrap));
}

Test(fs2_array_api, count_str_basic) {
    __fs2_checkreset();
    fs2array a = FS2ARRFROMSTR("a", "b", "a", "c", "a");

    CHECK_EQ("a ×3", 3, fs2arr_count_str(&a, "a", fs2_cmp_wrap));
    CHECK_EQ("b ×1", 1, fs2arr_count_str(&a, "b", fs2_cmp_wrap));
    CHECK_EQ("z ×0", 0, fs2arr_count_str(&a, "z", fs2_cmp_wrap));

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array_api, count_str_skips_holes) {
    __fs2_checkreset();
    fs2array a = FS2ARRFROMSTR("a", "a", "a");
    fs2 t = fs2arr_detach_new(&a, 1);         /* hole removes one "a" */
    fs2_free(&t);

    CHECK_EQ("a ×2 after hole", 2, fs2arr_count_str(&a, "a", fs2_cmp_wrap));

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array_api, count_str_empty_array) {
    fs2array a = FS2ARR();
    CHECK_EQ("empty", 0, fs2arr_count_str(&a, "a", fs2_cmp_wrap));
}

/* _str variants null-safe for key=NULL (via _fs2_wrap(NULL) → FS2EMPTY) */
Test(fs2_array_api, str_variants_null_key_safe) {
    __fs2_checkreset();
    fs2array a = FS2ARRFROMSTR("", "x");            /* live empty at [0] */

    CHECK_EQ   ("find NULL",  0, fs2arr_find_str(&a, NULL, fs2_cmp_wrap));
    CHECK_TRUE ("contains",      fs2arr_contains_str(&a, NULL, fs2_cmp_wrap));
    CHECK_EQ   ("count",      1, fs2arr_count_str(&a, NULL, fs2_cmp_wrap));

    fs2arr_free(&a);
    assert_balanced();
}

// fs2arr_setat

Test(fs2_array, setat_replace_live) {
    __fs2_checkreset();
    fs2array a = FS2ARRFROMSTR("old1", "old2", "old3");
    fs2 nv = fs2_fromstr("new");

    ptrdiff_t r = fs2arr_setat(&a, 1, &nv);

    CHECK_EQ("idx", 1, r);
    CHECK_EQ("count unchanged", 3, a.count);       /* was live → replace */
    CHECK_STR_EQ("[0]", "old1", a.arr[0].v);
    CHECK_STR_EQ("[1]", "new",  a.arr[1].v);
    CHECK_STR_EQ("[2]", "old3", a.arr[2].v);
    CHECK_TRUE("src reset", fs2_is_empty(&nv));

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array, setat_fill_hole) {
    __fs2_checkreset();
    fs2array a = FS2ARRFROMSTR("a", "b", "c");
    fs2 t = fs2arr_detach_new(&a, 1);              /* hole at [1] */
    fs2_free(&t);
    cr_assert_eq(a.count, 2);
    cr_assert_eq(a.hwm,   3);

    fs2 nv = fs2_fromstr("B");
    fs2arr_setat(&a, 1, &nv);

    CHECK_EQ  ("count restored", 3, a.count);
    CHECK_EQ  ("hwm unchanged",  3, a.hwm);
    CHECK_STR_EQ("[1]", "B", a.arr[1].v);
    CHECK_TRUE("slot live", !fs2_is_hole(&a.arr[1]));

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array, setat_extends_hwm) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    fs2 v = fs2_fromstr("first");
    fs2arr_push(&a, &v);                            /* hwm=1, count=1 */

    fs2 nv = fs2_fromstr("far");
    fs2arr_setat(&a, 4, &nv);                       /* extend hwm to 5 */

    CHECK_EQ  ("hwm",   5, a.hwm);
    CHECK_EQ  ("count", 2, a.count);
    CHECK_STR_EQ("[0]", "first", a.arr[0].v);
    CHECK_STR_EQ("[4]", "far",   a.arr[4].v);
    CHECK_TRUE("[1..3] holes",
               fs2_is_hole(&a.arr[1]) && fs2_is_hole(&a.arr[2]) &&
               fs2_is_hole(&a.arr[3]));

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array, setat_bounds_raise, .signal = SIGINT) {
    fs2array a = fs2arr_init(4);
    fs2 v = fs2_fromstr("x");
    fs2arr_setat(&a, 100, &v);                      /* 100 >= size → userraise */
}

Test(fs2_array, setat_null_src_aborts, .signal = SIGABRT) {
    fs2array a = fs2arr_init(4);
    fs2arr_setat(&a, 0, NULL);
    fs2arr_free(&a);
}

/* Integration: update row in place, invariants preserved */
Test(fs2_array, setat_update_row_in_table) {
    __fs2_checkreset();
    fs2array a = FS2ARRFROMSTR("row0", "row1", "row2");

    /* Update row 1 */
    fs2 v = fs2_fromstr("ROW1");
    fs2arr_setat(&a, 1, &v);

    CHECK_EQ  ("count", 3, a.count);
    CHECK_TRUE("valid", fs2arr_validate(NULL, &a));
    CHECK_STR_EQ("[1]", "ROW1", a.arr[1].v);

    /* After sort — order reflects update */
    fs2arr_sortasc(&a);
    /* "ROW1" (uppercase R) sorts before "row0" (lowercase r) in ASCII */
    CHECK_STR_EQ("[0]", "ROW1", a.arr[0].v);

    fs2arr_free(&a);
    assert_balanced();
}
