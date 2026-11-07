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
    CHECK_FALSE("[0] not hole", fs2arr_is_hole(&arr.arr[0]));

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
    CHECK_FALSE("[1] not hole", fs2arr_is_hole(&arr.arr[1]));  /* live empty */
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