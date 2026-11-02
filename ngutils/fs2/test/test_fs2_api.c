#include <criterion/criterion.h>

#include "test_fs2.h"
#include "fs2_api.h"
#include <locale.h>

TestSuite(fs2_api, .init = test_setup, .fini = test_teardown);

/* -------------------------------------------------------------------------
 * fs2_get — direct access, no capacity change
 * ------------------------------------------------------------------------- */

Test(fs2_api, get_reads_existing) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");
    CHECK_FS2VALID(1, &s);

    CHECK_EQ("char at 0", *fs2_get(&s, 0), 'h');
    CHECK_EQ("char at 4", *fs2_get(&s, 4), 'o');
    CHECK_PTR_EQ("pointer math", fs2_get(&s, 2), s.v + 2);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, get_returns_pointer_at_nul) {
    fs2 s = fs2_fromstr("hello");

    /* v[len] — NUL terminator, valid read */
    CHECK_EQ("*fs2_get(&s, s.len)", *fs2_get(&s, s.len), '\0');
    CHECK_PTR_EQ("fs2_get(&s, s.len)", fs2_get(&s, s.len), s.v + s.len);

    fs2_free(&s);
}

Test(fs2_api, get_one_past_end_pointer) {
    fs2 s = fs2_fromstr("hello");

    /* One-past-end: legal pointer value per C11 §6.5.6p8.
     * NOT dereferenced — that would be UB. */
    const char *p = fs2_get(&s, s.sz);
    CHECK_PTR_EQ("one past end", p, s.v + s.sz);

    fs2_free(&s);
}

Test(fs2_api, get_at_inside_capacity) {
    fs2 s = fs2_fromstr("hello");   /* sz >= 16, len=5 */

    /* pos between len+1 and sz — inside allocation, garbage but readable */
    const char *p = fs2_get(&s, s.len + 1);
    CHECK_TRUE("inside buffer", p >= s.v && p < s.v + s.sz);

    fs2_free(&s);
}

/* =========================================================================
 * fs2_elem — basic / FS2EMPTY promotion
 * ========================================================================= */

Test(fs2, elem_fs2empty_at_zero) {
    __fs2_checkreset();
    fs2 s = FS2EMPTY;

    char *p = fs2_elem(&s, 0);
    *p = 'x';

    CHECK_TRUE ("now ALLOC",  fs2_is_alloc(&s));
    CHECK_FALSE("no STATIC",  fs2_is_static(&s));
    CHECK_TRUE ("grew",       s.sz >= 2);
    CHECK_EQ   ("v[0]",       s.v[0], 'x');
    CHECK_EQ   ("len untouched (raw writer)", s.len, 0);
    /* v[1] НЕ проверяем здесь — uninitialized heap. */

    fs2_setlen(&s, 1);          /* ← v[1]='\0' пишется здесь */
    CHECK_EQ   ("v[1] NUL", s.v[1], '\0');
    CHECK_STR_EQ("content", s.v, "x");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, elem_fs2empty_at_nonzero_pos) {
    __fs2_checkreset();
    fs2 s = FS2EMPTY;

    char *p = fs2_elem(&s, 10);
    *p = 'z';

    CHECK_TRUE("ALLOC", fs2_is_alloc(&s));
    CHECK_TRUE("grew",  s.sz > 10);
    CHECK_EQ  ("v[0] NUL (from reserve)", s.v[0], '\0');
    CHECK_EQ  ("v[10] z",  s.v[10], 'z');
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, elem_canonical_empty) {
    __fs2_checkreset();
    fs2 s = FS2();   /* sz=0, v=NULL */

    char *p = fs2_elem(&s, 5);
    *p = 'a';

    CHECK_TRUE("ALLOC", fs2_is_alloc(&s));
    CHECK_TRUE("grew",  s.sz > 5);
    CHECK_EQ  ("v[5]",  s.v[5], 'a');
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_elem — fast path (no growth)
 * ========================================================================= */

Test(fs2, elem_alloc_within_sz_fast_path) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");   /* sz=16, len=5 */
    size_t sz_before = s.sz;

    char *p = fs2_elem(&s, 2);
    *p = 'X';

    CHECK_EQ  ("sz unchanged", s.sz, sz_before);
    CHECK_STR_EQ("content", s.v, "heXlo");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, elem_alloc_beyond_sz_grows) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hi");
    size_t sz_before = s.sz;

    char *p = fs2_elem(&s, 100);
    *p = 'x';

    CHECK_TRUE("grew",   s.sz > 100);
    CHECK_TRUE("sz>old", s.sz > sz_before);
    CHECK_EQ  ("v[100]", s.v[100], 'x');
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * ⚠ Ключевой регресс: +2 нужен для setlen(pos+1) на последнем байте
 * ========================================================================= */

Test(fs2, elem_last_valid_byte_then_setlen) {
    __fs2_checkreset();
    fs2 s = fs2_init(16);   /* sz = 32 (calcnewsize bumps exact pow2) */
    size_t sz_before = s.sz;
    CHECK_TRUE("has capacity", sz_before >= 16);

    /* Write at last valid index, then setlen(sz_before) — NUL at v[sz_before] */
    char *p = fs2_elem(&s, sz_before - 1);
    *p = 'x';

    /* +2 forced a grow: (sz-1) + 2 = sz+1 > sz */
    CHECK_TRUE("grew beyond old sz", s.sz > sz_before);
    CHECK_EQ  ("v[sz-1]", s.v[sz_before - 1], 'x');

    fs2_setlen(&s, sz_before);   /* NUL at v[sz_before], needs sz >= sz_before+1 */
    CHECK_EQ("v[sz] NUL", s.v[sz_before], '\0');

    fs2_free(&s);
    assert_balanced();
}

/* Демонстрация: если бы был +1, этот вызов был бы no-op → setlen aborts */
Test(fs2, elem_penultimate_byte_no_grow) {
    __fs2_checkreset();
    fs2 s = fs2_init(16);
    size_t sz_before = s.sz;

    for (size_t i = 0; i < sz_before; i++)
        s.v[i] = '0';

    /* pos = sz-2: (sz-2)+2 = sz > sz = false → no reserve, fast path */
    char *p = fs2_elem(&s, sz_before - 2);
    *p = 'y';

    CHECK_EQ("sz unchanged", s.sz, sz_before);
    CHECK_EQ("v[sz-2]",     s.v[sz_before - 2], 'y');

    fs2_setlen(&s, sz_before - 1);   /* NUL at v[sz-1], within sz */
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_elem — LOCAL
 * ========================================================================= */

Test(fs2, elem_local_within_sz) {
    __fs2_checkreset();
    FS2LOCAL(s, 16);
    memcpy(s.v, "hi", 2);
    fs2_setlen(&s, 2);

    char *p = fs2_elem(&s, 10);
    *p = 'x';

    CHECK_EQ("sz unchanged", s.sz, 16);
    CHECK_EQ("v[10]",        s.v[10], 'x');
    CHECK_TRUE("still LOCAL", fs2_is_local(&s));
    CHECK_FS2VALID(1, &s);
}

/* =========================================================================
 * fs2_elem — STATIC guards
 * ========================================================================= */

/* ⚠ Non-empty STATIC + pos beyond sz: reserve → prepare_growth → abort */
Test(fs2, elem_static_nonempty_beyond_sz_aborts, .signal = SIGABRT) {
    fs2 s = FS2LITERAL("hi");   /* sz=3, len=2 */
    fs2_elem(&s, 10);   /* 10+2 > 3 → reserve → prepare_growth: non-empty STATIC → abort */
}

/* Non-empty STATIC + pos within sz: fast path (caller's responsibility,
 * not our problem — write to .rodata is caller bug, no guard from us) */
Test(fs2, elem_static_nonempty_within_sz_fast_path) {
    fs2 s = FS2LITERAL("hi");   /* sz=3 */
    /* pos=0: 0+2=2 > 3 = false → no reserve → return ptr */
    char *p = fs2_elem(&s, 0);
    /* Не читаем и не пишем — только проверяем что fs2_elem вернул ptr без abort */
    CHECK_NOT_NULL("ptr returned", p);
    CHECK_TRUE("still STATIC", fs2_is_static(&s));
    CHECK_STR_EQ("content untouched", s.v, "hi");
}

/* =========================================================================
 * fs2_elem — LOCAL overflow
 * ========================================================================= */

Test(fs2, elem_local_overflow_aborts, .signal = SIGABRT) {
    __fs2_checkreset();
    FS2LOCAL(s, 8);
    /* 100+2 > 8 → reserve → prepare_growth: !ALLOC → abort */
    fs2_elem(&s, 100);
}

/* =========================================================================
 * elem() macro — deref sugar
 * ========================================================================= */

Test(fs2, elem_macro_write_build) {
    __fs2_checkreset();
    fs2 s = FS2();

    const char *word = "hello";
    for (size_t i = 0; i < 5; i++)
        elem(s, i) = word[i];
    fs2_setlen(&s, 5);

    CHECK_STR_EQ("built", s.v, "hello");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, elem_macro_read) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");

    CHECK_EQ("elem[0]", elem(s, 0), 'a');
    CHECK_EQ("elem[1]", elem(s, 1), 'b');
    CHECK_EQ("elem[2]", elem(s, 2), 'c');
    CHECK_EQ("elem[3] (NUL)", elem(s, 3), '\0');

    fs2_free(&s);
    assert_balanced();
}

/* ⚠ Регресс на исходный баг: elem(FS2EMPTY, 0) раньше писал в .rodata */
Test(fs2, elem_macro_fs2empty_no_crash) {
    __fs2_checkreset();
    fs2 s = FS2EMPTY;

    elem(s, 0) = 'x';   /* ← works with +2, crashed with +1 */

    CHECK_TRUE("ALLOC", fs2_is_alloc(&s));
    CHECK_EQ  ("v[0]",  s.v[0], 'x');

    fs2_setlen(&s, 1);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, elem_macro_grow_and_write) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("ab");

    elem(s, 20) = 'Z';   /* 20+2=22 > 16 → grow */

    CHECK_TRUE("grew", s.sz > 20);
    CHECK_EQ  ("v[20]", s.v[20], 'Z');
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_elem0 — sequential append
 * ========================================================================= */

Test(fs2, elem0_sequential_from_fs2) {
    __fs2_checkreset();
    fs2 s = FS2();

    const char *word = "hello";
    for (size_t i = 0; i < 5; i++) {
        char *p = fs2_elem0(&s, i);
        CHECK_EQ("len after elem0", s.len, i + 1);
        *p = word[i];
    }

    CHECK_STR_EQ("built", s.v, "hello");
    CHECK_EQ   ("len", s.len, 5);
    CHECK_TRUE ("ALLOC", fs2_is_alloc(&s));
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, elem0_sequential_from_fs2empty) {
    __fs2_checkreset();
    fs2 s = FS2EMPTY;

    char *p0 = fs2_elem0(&s, 0);
    *p0 = 'h';
    CHECK_TRUE("promoted to ALLOC", fs2_is_alloc(&s));
    CHECK_FALSE("no STATIC", fs2_is_static(&s));
    CHECK_EQ  ("len after 0", s.len, 1);

    char *p1 = fs2_elem0(&s, 1);
    *p1 = 'i';
    CHECK_EQ("len after 1", s.len, 2);

    CHECK_STR_EQ("built", s.v, "hi");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, elem0_append_to_existing) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");

    char *p = fs2_elem0(&s, 3);
    *p = 'd';

    CHECK_STR_EQ("appended", s.v, "abcd");
    CHECK_EQ   ("len", s.len, 4);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, elem0_pointer_equals_v_plus_pos) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    for (size_t i = 0; i < 5; i++) {
        char *p = fs2_elem0(&s, i);
        CHECK_PTR_EQ("returns v+pos", p, s.v + i);
    }

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, elem0_len_grows_to_pos_plus_one) {
    __fs2_checkreset();
    fs2 s = FS2();

    for (size_t i = 0; i < 10; i++) {
        char *p = fs2_elem0(&s, i);
        *p = 'a' + (char)i;
        CHECK_EQ("len == pos+1", s.len, i + 1);
    }
    CHECK_EQ("final len", s.len, 10);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * ⚠ Регресс на +2: pos = sz-1 требует grow (setlen(pos+1) иначе abort)
 * ========================================================================= */

Test(fs2, elem0_last_byte_of_capacity) {
    __fs2_checkreset();
    fs2 s = fs2_init(16);   /* sz = 32 (calcnewsize bumps exact pow2) */
    size_t sz_before = s.sz;

    /* Sequential fill to sz-1 */
    for (size_t i = 0; i < sz_before; i++) {
        char *p = fs2_elem0(&s, i);
        *p = 'x';
    }
    CHECK_EQ("len", s.len, sz_before);
    CHECK_STR_EQ("all x", s.v, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");   /* 32 */
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_elem0 — LOCAL
 * ========================================================================= */

Test(fs2, elem0_local_within_sz) {
    __fs2_checkreset();
    FS2LOCAL(s, 16);

    char *p0 = fs2_elem0(&s, 0);
    *p0 = 'a';
    char *p1 = fs2_elem0(&s, 1);
    *p1 = 'b';

    CHECK_STR_EQ("local built", s.v, "ab");
    CHECK_EQ   ("len", s.len, 2);
    CHECK_TRUE ("still LOCAL", fs2_is_local(&s));
    CHECK_FS2VALID(1, &s);
}

/* =========================================================================
 * fs2_elem0 — guards
 * ========================================================================= */

/* Non-empty STATIC, small pos: fs2_setlen rejects (pos != len) via userraise */
Test(fs2, elem0_static_nonempty_small_pos_aborts, .signal = SIGABRT) {
    fs2 s = FS2LITERAL("hi");   /* sz=3, len=2 */
    fs2_elem0(&s, 0);   /* setlen(1), pos(1) != len(2) → abort() */
}

/* Non-empty STATIC, pos beyond sz: reserve → prepare_growth → invfatal */
Test(fs2, elem0_static_nonempty_beyond_sz_aborts, .signal = SIGABRT) {
    fs2 s = FS2LITERAL("hi");
    fs2_elem0(&s, 10);   /* 12 > 3 → reserve → invfatal */
}

Test(fs2, elem0_local_overflow_aborts, .signal = SIGABRT) {
    __fs2_checkreset();
    FS2LOCAL(s, 8);
    fs2_elem0(&s, 100);   /* 102 > 8 → reserve → prepare_growth → invfatal */
}

/* =========================================================================
 * ⚠ fs2_elem0 — gap hazard (documented via [[not-valid-fs2]])
 * ========================================================================= */

Test(fs2, elem0_gap_from_canonical_invalid) {
    __fs2_checkreset();
    fs2 s = FS2();

    /* pos=5 > len=0: creates gap at v[1..4] (uninitialized) */
    char *p = fs2_elem0(&s, 5);
    *p = 'z';

    CHECK_EQ("len jumped", s.len, 6);
    CHECK_EQ("v[5]", s.v[5], 'z');
    CHECK_EQ("v[0] NUL (from reserve)", s.v[0], '\0');

    /* strlen stops at v[0], len=6 → mismatch → INVALID */
    CHECK_EQ ("strlen", strlen(s.v), 0);
    CHECK_EQ ("len", s.len, 6);
    CHECK_NEQ("strlen != len", strlen(s.v), s.len);

    /* fs2_validate rejects */
    CHECK_FALSE("validate rejects gap", fs2_validate(NULL, &s));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, elem0_gap_from_existing_invalid) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("ab");   /* len=2, v="ab\0" */

    /* pos=5 > len=2: gap at v[2..4]. v[2] was NUL from original content */
    char *p = fs2_elem0(&s, 5);
    *p = 'z';

    CHECK_EQ("len jumped", s.len, 6);
    CHECK_EQ("v[5]", s.v[5], 'z');
    CHECK_EQ("strlen stops at v[2]", strlen(s.v), 2);
    CHECK_NEQ("strlen != len", strlen(s.v), s.len);

    CHECK_FALSE("validate rejects", fs2_validate(NULL, &s));

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * elem0() macro — deref sugar
 * ========================================================================= */

Test(fs2, elem0_macro_build) {
    __fs2_checkreset();
    fs2 s = FS2();

    const char *word = "hi!";
    for (size_t i = 0; i < 3; i++)
        elem0(s, i) = word[i];

    CHECK_STR_EQ("built", s.v, "hi!");
    CHECK_EQ   ("len", s.len, 3);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, elem0_macro_read) {
    __fs2_checkreset();
    fs2 s = FS2();

    elem0(s, 0) = 'a';
    elem0(s, 1) = 'b';

    CHECK_EQ("elem0[0]", elem(s, 0), 'a');   /* reading via macro still works */
    CHECK_EQ("elem0[1]", elem(s, 1), 'b');
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_flip
 * ========================================================================= */

Test(fs2, flip_null_aborts, .signal = SIGABRT) {
    fs2_flip(NULL);
}

Test(fs2, flip_static_nonempty_aborts, .signal = SIGABRT) {
    fs2 s = FS2LITERAL("hi");
    fs2_flip(&s);
}

Test(fs2, flip_fs2empty_noop) {
    fs2 s = FS2EMPTY;
    fs2_flip(&s);
    CHECK_TRUE("still STATIC", fs2_is_static(&s));
    CHECK_EQ  ("len", s.len, 0);
    CHECK_FS2VALID(1, &s);
}

Test(fs2, flip_canonical_noop) {
    __fs2_checkreset();
    fs2 s = FS2();
    fs2_flip(&s);
    CHECK_EQ("len", s.len, 0);
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2, flip_alloc_with_content) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");
    fs2_flip(&s);
    CHECK_STR_EQ("flipped", s.v, "olleh");
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2, flip_local_with_content) {
    __fs2_checkreset();
    FS2LOCAL(s, 16);
    memcpy(s.v, "ab", 3);
    fs2_setlen(&s, 2);
    fs2_flip(&s);
    CHECK_STR_EQ("flipped", s.v, "ba");
    CHECK_FS2VALID(1, &s);
}

Test(fs2, flip_single_char) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("x");
    fs2_flip(&s);
    CHECK_STR_EQ("x", s.v, "x");
    fs2_free(&s);
    assert_balanced();
}

Test(fs2, flip_even_odd) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("abcd");
    fs2 b = fs2_fromstr("abcde");
    fs2_flip(&a);
    fs2_flip(&b);
    CHECK_STR_EQ("even", a.v, "dcba");
    CHECK_STR_EQ("odd",  b.v, "edcba");
    CHECK_FS2VALID(1, &a);
    CHECK_FS2VALID(2, &b);
    fs2_free(&a);
    fs2_free(&b);
    assert_balanced();
}

/* =========================================================================
 * fs2_partialsort — basic
 * ========================================================================= */

Test(fs2, partialsort_full_range) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("dcba");

    fs2_partialsort(&s, 0, 4, true);

    CHECK_STR_EQ("sorted", s.v, "abcd");
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2, partialsort_middle_slice) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("XdcbaY");   /* len=6 */

    fs2_partialsort(&s, 1, 5, true);   /* sort "dcba" → "abcd" */

    CHECK_STR_EQ("sliced sorted", s.v, "XabcdY");
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2, partialsort_desc) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("XabcdY");

    fs2_partialsort(&s, 1, 5, false);

    CHECK_STR_EQ("sliced desc", s.v, "XdcbaY");
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_partialsort — clamping
 * ========================================================================= */

Test(fs2, partialsort_to_beyond_len_clamped) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("dcba");

    fs2_partialsort(&s, 0, 100, true);   /* to=100 → clamp 4 */

    CHECK_STR_EQ("whole range", s.v, "abcd");
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2, partialsort_from_at_len_noop) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abcd");

    fs2_partialsort(&s, 4, 100, true);   /* to clamp 4, from(4) < to(4) false */

    CHECK_STR_EQ("unchanged", s.v, "abcd");
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2, partialsort_from_beyond_len_noop) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abcd");

    fs2_partialsort(&s, 10, 100, true);   /* both clamp, no-op */

    CHECK_STR_EQ("unchanged", s.v, "abcd");
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2, partialsort_from_gt_to_noop) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("dcba");

    fs2_partialsort(&s, 3, 1, true);   /* inverted range → no-op */

    CHECK_STR_EQ("unchanged", s.v, "dcba");
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2, partialsort_empty_range_noop) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("dcba");

    fs2_partialsort(&s, 2, 2, true);   /* empty range */

    CHECK_STR_EQ("unchanged", s.v, "dcba");
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_partialsort — cant_mutate semantics
 * ========================================================================= */

Test(fs2, partialsort_null_aborts, .signal = SIGABRT) {
    fs2_partialsort(NULL, 0, 5, true);
}

Test(fs2, partialsort_static_nonempty_aborts, .signal = SIGABRT) {
    fs2 s = FS2LITERAL("dcba");
    fs2_partialsort(&s, 0, 4, true);   /* write .rodata → invfatal */
}

Test(fs2, partialsort_fs2empty_noop) {
    fs2 s = FS2EMPTY;   /* cant_mutate: empty → skip */
    fs2_partialsort(&s, 0, 100, true);
    CHECK_TRUE("still STATIC", fs2_is_static(&s));
    CHECK_FS2VALID(1, &s);
}

Test(fs2, partialsort_canonical_empty_noop) {
    __fs2_checkreset();
    fs2 s = FS2();
    fs2_partialsort(&s, 0, 100, true);
    CHECK_EQ("len", s.len, 0);
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2, partialsort_local_with_content) {
    __fs2_checkreset();
    FS2LOCAL(s, 16);
    memcpy(s.v, "dcba", 5);
    fs2_setlen(&s, 4);

    fs2_partialsort(&s, 0, 4, true);

    CHECK_STR_EQ("local sorted", s.v, "abcd");
    CHECK_TRUE ("still LOCAL", fs2_is_local(&s));
    CHECK_FS2VALID(1, &s);
}

/* =========================================================================
 * fs2_partialsort — stability across two slices
 * ========================================================================= */

Test(fs2, partialsort_two_disjoint_slices) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("dcbaXYhgfe");   /* len=10 */

    fs2_partialsort(&s, 0, 4, true);    /* dcba → abcd */
    fs2_partialsort(&s, 6, 10, true);   /* hgfe → efgh */

    CHECK_STR_EQ("both sorted", s.v, "abcdXYefgh");
    CHECK_EQ("middle slice intact",
             memcmp(s.v + 4, "XY", 2), 0);   /* ← было CHECK_STR_EQ */
    CHECK_EQ("v[4] X", s.v[4], 'X');
    CHECK_EQ("v[5] Y", s.v[5], 'Y');
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_sort — thin wrapper over fs2_partialsort(0, len)
 * ========================================================================= */

Test(fs2, sort_asc) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("dcba");

    fs2_sort(&s, true);

    CHECK_STR_EQ("sorted asc", s.v, "abcd");
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2, sort_desc) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abcd");

    fs2_sort(&s, false);

    CHECK_STR_EQ("sorted desc", s.v, "dcba");
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2, sort_empty_noop) {
    fs2 s = FS2EMPTY;
    fs2_sort(&s, true);
    CHECK_TRUE("still STATIC", fs2_is_static(&s));
    CHECK_EQ  ("len", s.len, 0);
    CHECK_FS2VALID(1, &s);
}

Test(fs2, sort_canonical_noop) {
    __fs2_checkreset();
    fs2 s = FS2();
    fs2_sort(&s, true);
    CHECK_EQ("len", s.len, 0);
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2, sort_single_char) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("x");
    fs2_sort(&s, true);
    CHECK_STR_EQ("x", s.v, "x");
    fs2_free(&s);
    assert_balanced();
}

Test(fs2, sort_local) {
    __fs2_checkreset();
    FS2LOCAL(s, 16);
    memcpy(s.v, "dcba", 5);
    fs2_setlen(&s, 4);

    fs2_sort(&s, true);

    CHECK_STR_EQ("local sorted", s.v, "abcd");
    CHECK_TRUE ("still LOCAL", fs2_is_local(&s));
    CHECK_FS2VALID(1, &s);
}

/* =========================================================================
 * fs2_sort — guards (delegated to fs2_partialsort / fs2_cant_mutate)
 * ========================================================================= */

Test(fs2, sort_null_aborts, .signal = SIGABRT) {
    /* Ternary: NULL → len=0 → cant_mutate(NULL) → invfatal SIGABRT.
     * Без ternary был бы SIGSEGV на str->len. */
    fs2_sort(NULL, true);
}

Test(fs2, sort_static_nonempty_aborts, .signal = SIGABRT) {
    fs2 s = FS2LITERAL("dcba");
    fs2_sort(&s, true);
}

/* =========================================================================
 * fs2_cant_realloc — assert-style contract
 * ========================================================================= */

Test(fs2, cant_realloc_null_aborts, .signal = SIGABRT) {
    fs2_cant_realloc(NULL);
}

Test(fs2, cant_realloc_canonical_empty_proceeds) {
    fs2 s = FS2();
    CHECK_FALSE("empty → proceed", fs2_cant_realloc(&s));
}

Test(fs2, cant_realloc_fs2empty_proceeds) {
    fs2 s = FS2EMPTY;
    CHECK_FALSE("empty → proceed", fs2_cant_realloc(&s));
}

Test(fs2, cant_realloc_alloc_empty_proceeds) {
    __fs2_checkreset();
    fs2 s = fs2_init(16);
    CHECK_FALSE("ALLOC empty → proceed", fs2_cant_realloc(&s));
    fs2_free(&s);
    assert_balanced();
}

Test(fs2, cant_realloc_local_proceeds) {
    __fs2_checkreset();
    FS2LOCAL(s, 16);
    /* LOCAL → false (grow path will abort if capacity insufficient) */
    CHECK_FALSE("LOCAL → proceed", fs2_cant_realloc(&s));
}

Test(fs2, cant_realloc_alloc_nonempty_proceeds) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hi");
    CHECK_FALSE("ALLOC non-empty → proceed", fs2_cant_realloc(&s));
    fs2_free(&s);
    assert_balanced();
}

Test(fs2, cant_realloc_static_nonempty_aborts, .signal = SIGABRT) {
    fs2 s = FS2LITERAL("hi");
    fs2_cant_realloc(&s);   /* ← было SIGINT, теперь SIGABRT */
}

/* =========================================================================
 * fs2_cat
 * ========================================================================= */


Test(fs2, cat_null_src_aborts, .signal = SIGABRT) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("a");
    fs2_cat(&dst, NULL);
    fs2_free(&dst);
}

Test(fs2, cat_null_dst_aborts, .signal = SIGABRT) {
    fs2 src = fs2_fromstr("x");
    fs2_cat(NULL, &src);
    fs2_free(&src);
}

Test(fs2, cat_static_nonempty_dst_aborts, .signal = SIGABRT) {
    fs2 dst = FS2LITERAL("hi");
    fs2 src = fs2_fromstr("!");
    fs2_cat(&dst, &src);
    fs2_free(&src);
}

Test(fs2, cat_empty_src_noop) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("a");
    fs2 src = FS2EMPTY;
    fs2_cat(&dst, &src);
    CHECK_STR_EQ("unchanged", dst.v, "a");
    CHECK_FS2VALID(1, &dst);
    fs2_free(&dst);
    assert_balanced();
}

Test(fs2, cat_fs2empty_dst_promotes) {
    __fs2_checkreset();
    fs2 dst = FS2EMPTY;
    fs2 src = fs2_fromstr("hi");
    fs2_cat(&dst, &src);
    CHECK_STR_EQ("promoted", dst.v, "hi");
    CHECK_TRUE("ALLOC", fs2_is_alloc(&dst));
    CHECK_FS2VALID(1, &dst);
    fs2_free(&dst);
    fs2_free(&src);
    assert_balanced();
}

Test(fs2, cat_canonical_dst_promotes) {
    __fs2_checkreset();
    fs2 dst = FS2();
    fs2 src = fs2_fromstr("hi");
    fs2_cat(&dst, &src);
    CHECK_STR_EQ("promoted", dst.v, "hi");
    fs2_free(&dst);
    fs2_free(&src);
    assert_balanced();
}

Test(fs2, cat_alloc_with_content) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("hello");
    fs2 src = fs2_fromstr(" world");
    fs2_cat(&dst, &src);
    CHECK_STR_EQ("hello world", dst.v, "hello world");
    CHECK_FS2VALID(1, &dst);
    fs2_free(&dst);
    fs2_free(&src);
    assert_balanced();
}

Test(fs2, cat_local_within_capacity) {
    __fs2_checkreset();
    FS2LOCAL(dst, 16);
    memcpy(dst.v, "hi", 3);
    fs2_setlen(&dst, 2);
    fs2 src = fs2_fromstr("!");
    fs2_cat(&dst, &src);
    CHECK_STR_EQ("hi!", dst.v, "hi!");
    CHECK_TRUE("LOCAL", fs2_is_local(&dst));
    fs2_free(&src);
}

Test(fs2, cat_local_overflow_aborts, .signal = SIGABRT) {
    __fs2_checkreset();
    FS2LOCAL(dst, 4);
    fs2 src = fs2_fromstr("hello world");
    fs2_cat(&dst, &src);   /* reserve → prepare_growth: non-ALLOC → abort */
    fs2_free(&src);
}

/* FS2EMPTY */
Test(fs2, cat_via_nvl_null_src_noop) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("a");
    fs2_cat(&dst, &FS2EMPTY);
    CHECK_STR_EQ("unchanged", dst.v, "a");
    fs2_free(&dst);
    assert_balanced();
}

/* =========================================================================
 * fs2_catstr — nominal
 * ========================================================================= */

Test(fs2, catstr_basic) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("hello");

    fs2 *ret = fs2_catstr(&dst, " world");

    CHECK_PTR_EQ("returns dst", ret, &dst);
    CHECK_STR_EQ("content", dst.v, "hello world");
    CHECK_EQ  ("len", dst.len, 11);
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2, catstr_empty_str_noop) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("hello");

    fs2_catstr(&dst, "");

    CHECK_STR_EQ("unchanged", dst.v, "hello");
    CHECK_EQ  ("len", dst.len, 5);
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2, catstr_into_fs2empty_promotes) {
    __fs2_checkreset();
    fs2 dst = FS2EMPTY;

    fs2_catstr(&dst, "hello");

    CHECK_STR_EQ("promoted", dst.v, "hello");
    CHECK_TRUE ("ALLOC", fs2_is_alloc(&dst));
    CHECK_FALSE("no longer STATIC", fs2_is_static(&dst));
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2, catstr_into_canonical_promotes) {
    __fs2_checkreset();
    fs2 dst = FS2();

    fs2_catstr(&dst, "hello");

    CHECK_STR_EQ("promoted", dst.v, "hello");
    CHECK_TRUE ("ALLOC", fs2_is_alloc(&dst));
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2, catstr_multiple_appends) {
    __fs2_checkreset();
    fs2 dst = FS2();

    fs2_catstr(&dst, "one");
    fs2_catstr(&dst, " ");
    fs2_catstr(&dst, "two");

    CHECK_STR_EQ("content", dst.v, "one two");
    CHECK_EQ  ("len", dst.len, 7);
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2, catstr_triggers_realloc) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("x");
    size_t orig_sz = dst.sz;

    char big[1024];
    memset(big, 'A', 1023);
    big[1023] = '\0';

    fs2_catstr(&dst, big);

    CHECK_TRUE("grew", dst.sz > orig_sz);
    CHECK_EQ  ("len", dst.len, 1024);
    CHECK_EQ  ("first", dst.v[0], 'x');
    CHECK_EQ  ("last",  dst.v[1023], 'A');
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2, catstr_local_within_capacity) {
    __fs2_checkreset();
    FS2LOCAL(dst, 32);
    memcpy(dst.v, "hi", 3);
    fs2_setlen(&dst, 2);

    fs2_catstr(&dst, " there");

    CHECK_STR_EQ("content", dst.v, "hi there");
    CHECK_TRUE ("still LOCAL", fs2_is_local(&dst));
    CHECK_FS2VALID(1, &dst);
}

/* =========================================================================
 * fs2_catstr — strict NULL src (SIGABRT)
 * ========================================================================= */

Test(fs2, catstr_null_src_aborts, .signal = SIGABRT) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("a");
    fs2_catstr(&dst, NULL);   /* ← было silent no-op через wrap(NULL), теперь SIGABRT */
    fs2_free(&dst);
}

Test(fs2, catstr_null_dst_aborts, .signal = SIGABRT) {
    fs2_catstr(NULL, "x");
}

Test(fs2, catstr_static_nonempty_dst_aborts, .signal = SIGABRT) {
    fs2 dst = FS2LITERAL("hi");
    fs2_catstr(&dst, "!");
}

Test(fs2, catstr_local_overflow_aborts, .signal = SIGABRT) {
    __fs2_checkreset();
    FS2LOCAL(dst, 4);   /* sz=4 */
    fs2_catstr(&dst, "hello world");   /* 11 > 3 → reserve → abort */
}

/* =========================================================================
 * wrap pattern — opt-in tolerance
 * ========================================================================= */

Test(fs2, catstr_via_nvlstr_maybe_null) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("a");
    const char *maybe = NULL;

    fs2_catstr(&dst, str_orempty(maybe));

    CHECK_STR_EQ("unchanged", dst.v, "a");

    fs2_free(&dst);
    assert_balanced();
}

/* =========================================================================
 * ⚠ Documented hazard: catstr(&dst, dst.v) — restrict violation
 * ========================================================================= */

/* NOT tested — this is UB:
 *
 *   fs2_catstr(&dst, dst.v);
 *
 * 1. `restrict` on src / dst.v violated.
 * 2. If `fs2_reserve` inside `fs2_memcpy` triggers realloc,
 *    `s.v` (== old dst.v) becomes dangling.
 * 3. memcpy reads from freed memory → use-after-free.
 *
 * Caller must clone before self-append:
 *   fs2 clone = fs2_clone(&dst);
 *   fs2_catstr(&dst, clone.v);
 *   fs2_free(&clone);
 *
 * Or use fs2_repeat / fs2_cat(&dst, &dst) is also UB.
 */

/* =========================================================================
 * Consistency: catstr == cat(wrap_notnull)
 * ========================================================================= */

Test(fs2, catstr_matches_cat_via_wrap) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("prefix_");
    fs2 b = fs2_fromstr("prefix_");

    fs2_catstr(&a, "suffix");

    fs2 tmp = _fs2_wrap_notnull("suffix");
    fs2_cat(&b, &tmp);

    CHECK_STR_EQ("a == b", a.v, b.v);
    CHECK_EQ  ("same len", a.len, b.len);
    CHECK_FS2VALID(1, &a);
    CHECK_FS2VALID(2, &b);

    fs2_free(&a);
    fs2_free(&b);
    assert_balanced();
}

/* =========================================================================
 * fs2_rev_catstr — nominal
 * ========================================================================= */

Test(fs2, rev_catstr_basic) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("world");

    fs2 *ret = fs2_rev_catstr(&dst, "hello ");

    CHECK_PTR_EQ("returns dst", ret, &dst);
    CHECK_STR_EQ("content", dst.v, "hello world");
    CHECK_EQ  ("len", dst.len, 11);
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2, rev_catstr_empty_str_noop) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("world");

    fs2_rev_catstr(&dst, "");

    CHECK_STR_EQ("unchanged", dst.v, "world");
    CHECK_EQ  ("len", dst.len, 5);
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2, rev_catstr_into_fs2empty_promotes) {
    __fs2_checkreset();
    fs2 dst = FS2EMPTY;

    fs2_rev_catstr(&dst, "hello");

    CHECK_STR_EQ("promoted", dst.v, "hello");
    CHECK_TRUE ("ALLOC", fs2_is_alloc(&dst));
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2, rev_catstr_into_canonical_promotes) {
    __fs2_checkreset();
    fs2 dst = FS2();

    fs2_rev_catstr(&dst, "hi");

    CHECK_STR_EQ("content", dst.v, "hi");
    CHECK_TRUE ("ALLOC", fs2_is_alloc(&dst));
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2, rev_catstr_multiple) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("C");

    fs2_rev_catstr(&dst, "B");
    fs2_rev_catstr(&dst, "A");

    /* "C" + "B" → "BC", then "A" prepended → "ABC" */
    CHECK_STR_EQ("ABC", dst.v, "ABC");
    CHECK_EQ  ("len", dst.len, 3);
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2, rev_catstr_triggers_realloc) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("x");
    size_t orig_sz = dst.sz;

    char big[1024];
    memset(big, 'A', 1023);
    big[1023] = '\0';

    fs2_rev_catstr(&dst, big);

    CHECK_TRUE("grew", dst.sz > orig_sz);
    CHECK_EQ  ("len", dst.len, 1024);
    CHECK_EQ  ("first", dst.v[0], 'A');
    CHECK_EQ  ("last",  dst.v[1023], 'x');
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2, rev_catstr_local_within_capacity) {
    __fs2_checkreset();
    FS2LOCAL(dst, 32);
    memcpy(dst.v, "tail", 5);
    fs2_setlen(&dst, 4);

    fs2_rev_catstr(&dst, "head-");

    CHECK_STR_EQ("content", dst.v, "head-tail");
    CHECK_TRUE ("still LOCAL", fs2_is_local(&dst));
    CHECK_FS2VALID(1, &dst);
}

/* =========================================================================
 * fs2_rev_catstr — guards
 * ========================================================================= */
Test(fs2, rev_catstr_static_nonempty_dst_aborts, .signal = SIGABRT) {
    fs2 dst = FS2LITERAL("hi");
    fs2_rev_catstr(&dst, "!");   /* → cant_realloc OR cant_copy → invfatal → SIGABRT */
}

/* Regression: fs2_memcpy_str directly on non-empty STATIC */
Test(fs2, memcpy_str_static_nonempty_dst_aborts, .signal = SIGABRT) {
    fs2 dst = FS2LITERAL("hi");
    fs2_memcpy_str(&dst, 0, "x", 1);   /* → cant_copy → invfatal */
}

/* Regression: fs2_memmove directly on non-empty STATIC (self-move) */
Test(fs2, memmove_self_static_nonempty_aborts, .signal = SIGABRT) {
    fs2 dst = FS2LITERAL("hi");
    fs2_memmove(&dst, 1, NULL, 0, 2);   /* src=NULL → src=dst → cant_copy → invfatal */
}

/* Regression: fs2_memcpy with non-empty STATIC dst */
Test(fs2, memcpy_static_nonempty_dst_aborts, .signal = SIGABRT) {
    __fs2_checkreset();
    fs2 dst = FS2LITERAL("hi");
    fs2 src = fs2_fromstr("x");
    fs2_memcpy(&dst, 0, &src, 0, 1);
    fs2_free(&src);
}

Test(fs2, rev_catstr_null_dst_aborts, .signal = SIGABRT) {
    fs2_rev_catstr(NULL, "x");
}

Test(fs2, rev_catstr_null_beginner_aborts, .signal = SIGABRT) {
    /* ← было SIGSEGV (strlen(NULL)), теперь SIGABRT */
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("a");
    fs2_rev_catstr(&dst, NULL);
    fs2_free(&dst);
}

Test(fs2, rev_catstr_local_overflow_aborts, .signal = SIGABRT) {
    __fs2_checkreset();
    FS2LOCAL(dst, 4);
    fs2_rev_catstr(&dst, "hello world");   /* selfmove → reserve → abort */
}

/* =========================================================================
 * ⚠ Hazard: rev_catstr(&dst, dst.v) — restrict violation + data corruption
 *
 * Trace:
 *   1. len = dst->len
 *   2. selfmove shifts old content [0, len) to [len, 2*len)
 *   3. memcpy_str(v, 0, dst.v, len) — but dst.v now points to shifted content
 *      and v[0..len) contains garbage after selfmove
 *   4. Result: corrupted
 *
 * NOT tested (UB). Caller must clone first:
 *   fs2 tmp = fs2_fromstr("prefix");
 *   fs2_rev_catstr(&dst, tmp.v);
 *   fs2_free(&tmp);
 * ========================================================================= */

Test(fs2, rev_catstr_fs2empty_promotes) {
    __fs2_checkreset();
    fs2 dst = FS2EMPTY;
    fs2_rev_catstr(&dst, "hi");
    CHECK_STR_EQ("promoted", dst.v, "hi");
    CHECK_TRUE("ALLOC", fs2_is_alloc(&dst));
    CHECK_FS2VALID(1, &dst);
    fs2_free(&dst);
    assert_balanced();
}

/* =========================================================================
 * fs2_cpy — nominal
 * ========================================================================= */

Test(fs2, cpy_alloc_overwrites) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("hello");
    fs2 src = fs2_fromstr("world");

    fs2 *ret = fs2_cpy(&dst, &src);

    CHECK_PTR_EQ("returns dst", ret, &dst);
    CHECK_STR_EQ("content", dst.v, "world");
    CHECK_EQ  ("len", dst.len, 5);
    CHECK_FS2VALID(1, &dst);
    CHECK_FS2VALID(2, &src);

    fs2_free(&dst);
    fs2_free(&src);
    assert_balanced();
}

Test(fs2, cpy_shorter_src) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("longer string");
    fs2 src = fs2_fromstr("short");
    size_t orig_sz = dst.sz;

    fs2_cpy(&dst, &src);

    CHECK_STR_EQ("shorter", dst.v, "short");
    CHECK_EQ  ("sz unchanged", dst.sz, orig_sz);
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    fs2_free(&src);
    assert_balanced();
}

Test(fs2, cpy_longer_src_grows) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("short");
    fs2 src = fs2_fromstr("much much longer string");
    size_t orig_sz = dst.sz;

    fs2_cpy(&dst, &src);

    CHECK_STR_EQ("longer", dst.v, "much much longer string");
    CHECK_TRUE("grew", dst.sz > orig_sz);
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    fs2_free(&src);
    assert_balanced();
}

/* cpy semantics: empty src CLEARS dst (unlike cat = no-op) */
Test(fs2, cpy_empty_src_clears_dst) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("hello");
    fs2 src = FS2EMPTY;

    fs2_cpy(&dst, &src);

    CHECK_STR_EQ("cleared", dst.v, "");
    CHECK_EQ  ("len", dst.len, 0);
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

/* =========================================================================
 * fs2_cpy — promote variants
 * ========================================================================= */

Test(fs2, cpy_into_fs2empty_promotes) {
    __fs2_checkreset();
    fs2 dst = FS2EMPTY;
    fs2 src = fs2_fromstr("hi");

    fs2_cpy(&dst, &src);

    CHECK_STR_EQ("promoted", dst.v, "hi");
    CHECK_TRUE ("ALLOC", fs2_is_alloc(&dst));
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    fs2_free(&src);
    assert_balanced();
}

Test(fs2, cpy_into_canonical_promotes) {
    __fs2_checkreset();
    fs2 dst = FS2();
    fs2 src = fs2_fromstr("hi");

    fs2_cpy(&dst, &src);

    CHECK_STR_EQ("promoted", dst.v, "hi");
    CHECK_TRUE ("ALLOC", fs2_is_alloc(&dst));
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    fs2_free(&src);
    assert_balanced();
}

/* =========================================================================
 * fs2_cpy — self-copy
 * ========================================================================= */

Test(fs2, cpy_self_alloc_noop) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");
    void *orig_v = s.v;

    fs2_cpy(&s, &s);

    CHECK_PTR_EQ("v unchanged", s.v, orig_v);
    CHECK_STR_EQ("content unchanged", s.v, "hello");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, cpy_self_local_noop) {
    __fs2_checkreset();
    FS2LOCAL(s, 16);
    memcpy(s.v, "hi", 3);
    fs2_setlen(&s, 2);
    void *orig_v = s.v;

    fs2_cpy(&s, &s);

    CHECK_PTR_EQ("v unchanged", s.v, orig_v);
    CHECK_STR_EQ("content unchanged", s.v, "hi");
    CHECK_TRUE("still LOCAL", fs2_is_local(&s));
    CHECK_FS2VALID(1, &s);
}

Test(fs2, cpy_self_fs2empty_noop) {
    fs2 s = FS2EMPTY;

    fs2_cpy(&s, &s);   /* cant_realloc: empty → false, then dst == src → no-op */

    CHECK_TRUE("still STATIC", fs2_is_static(&s));
    CHECK_TRUE("eq_empty", fs2_eq_empty(&s));
    CHECK_FS2VALID(1, &s);
}

Test(fs2, cpy_self_canonical_noop) {
    __fs2_checkreset();
    fs2 s = FS2();

    fs2_cpy(&s, &s);

    CHECK_EQ("len", s.len, 0);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* ⚠ Self-copy на non-empty STATIC → abort (design decision) */
Test(fs2, cpy_self_static_nonempty_aborts, .signal = SIGABRT) {
    fs2 s = FS2LITERAL("hi");
    fs2_cpy(&s, &s);   /* cant_realloc → abort, even though logically no-op */
}

/* =========================================================================
 * fs2_cpy — guards
 * ========================================================================= */

Test(fs2, cpy_null_dst_aborts, .signal = SIGABRT) {
    fs2 src = fs2_fromstr("x");
    fs2_cpy(NULL, &src);
    fs2_free(&src);
}

Test(fs2, cpy_null_src_aborts, .signal = SIGABRT) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("a");
    fs2_cpy(&dst, NULL);   /* dst cleared, then cat aborts — process dies, no matter */
    fs2_free(&dst);
}

Test(fs2, cpy_static_nonempty_dst_aborts, .signal = SIGABRT) {
    fs2 dst = FS2LITERAL("hi");
    fs2 src = fs2_fromstr("!");
    fs2_cpy(&dst, &src);
    fs2_free(&src);
}

Test(fs2, cpy_local_within_capacity) {
    __fs2_checkreset();
    FS2LOCAL(dst, 16);
    memcpy(dst.v, "old", 4);
    fs2_setlen(&dst, 3);
    fs2 src = fs2_fromstr("new");

    fs2_cpy(&dst, &src);

    CHECK_STR_EQ("content", dst.v, "new");
    CHECK_TRUE ("still LOCAL", fs2_is_local(&dst));
    CHECK_FS2VALID(1, &dst);

    fs2_free(&src);
}

Test(fs2, cpy_local_overflow_aborts, .signal = SIGABRT) {
    __fs2_checkreset();
    FS2LOCAL(dst, 4);
    fs2 src = fs2_fromstr("hello world");
    fs2_cpy(&dst, &src);
    fs2_free(&src);
}

/* =========================================================================
 * fs2_cpystr — nominal
 * ========================================================================= */

Test(fs2, cpystr_basic) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("hello");

    fs2 *ret = fs2_cpystr(&dst, "world");

    CHECK_PTR_EQ("returns dst", ret, &dst);
    CHECK_STR_EQ("content", dst.v, "world");
    CHECK_EQ  ("len", dst.len, 5);
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2, cpystr_shorter) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("longer string");
    size_t orig_sz = dst.sz;

    fs2_cpystr(&dst, "short");

    CHECK_STR_EQ("shorter", dst.v, "short");
    CHECK_EQ  ("sz unchanged", dst.sz, orig_sz);
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2, cpystr_longer_grows) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("short");
    size_t orig_sz = dst.sz;

    fs2_cpystr(&dst, "much longer string");

    CHECK_TRUE("grew", dst.sz > orig_sz);
    CHECK_STR_EQ("content", dst.v, "much longer string");
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

/* Empty (non-NULL) src clears dst — not no-op like catstr */
Test(fs2, cpystr_empty_src_clears) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("hello");

    fs2_cpystr(&dst, "");

    CHECK_STR_EQ("cleared", dst.v, "");
    CHECK_EQ  ("len", dst.len, 0);
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

/* =========================================================================
 * fs2_cpystr — promote variants
 * ========================================================================= */

Test(fs2, cpystr_into_fs2empty_promotes) {
    __fs2_checkreset();
    fs2 dst = FS2EMPTY;

    fs2_cpystr(&dst, "hello");

    CHECK_STR_EQ("promoted", dst.v, "hello");
    CHECK_TRUE ("ALLOC", fs2_is_alloc(&dst));
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2, cpystr_into_canonical_promotes) {
    __fs2_checkreset();
    fs2 dst = FS2();

    fs2_cpystr(&dst, "hi");

    CHECK_STR_EQ("content", dst.v, "hi");
    CHECK_TRUE ("ALLOC", fs2_is_alloc(&dst));
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2, cpystr_local_within_capacity) {
    __fs2_checkreset();
    FS2LOCAL(dst, 16);
    memcpy(dst.v, "old", 4);
    fs2_setlen(&dst, 3);

    fs2_cpystr(&dst, "new");

    CHECK_STR_EQ("content", dst.v, "new");
    CHECK_TRUE ("still LOCAL", fs2_is_local(&dst));
    CHECK_FS2VALID(1, &dst);
}

/* =========================================================================
 * fs2_cpystr — strict NULL src
 * ========================================================================= */

/* ⚠ Было: cpystr(&dst, NULL) clears dst. Стало: SIGABRT (strict). */
Test(fs2, cpystr_null_src_aborts, .signal = SIGABRT) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("hello");
    fs2_cpystr(&dst, NULL);
    fs2_free(&dst);
}

/* Use fs2_nvlstr for opt-in tolerance */
Test(fs2, cpystr_via_nvlstr_null_clears) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("hello");

    fs2_cpystr(&dst, str_orempty(NULL));   /* NULL → "" → clear */

    CHECK_STR_EQ("cleared", dst.v, "");
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

/* =========================================================================
 * fs2_cpystr — delegated guards (via fs2_cpy)
 * ========================================================================= */

Test(fs2, cpystr_null_dst_aborts, .signal = SIGABRT) {
    fs2_cpystr(NULL, "x");
}

Test(fs2, cpystr_static_nonempty_dst_aborts, .signal = SIGABRT) {
    fs2 dst = FS2LITERAL("hi");
    fs2_cpystr(&dst, "x");
}

Test(fs2, cpystr_local_overflow_aborts, .signal = SIGABRT) {
    __fs2_checkreset();
    FS2LOCAL(dst, 4);
    fs2_cpystr(&dst, "hello world");
}

/* =========================================================================
 * fs2_sprintf — empty format on FS2EMPTY (regression)
 * ========================================================================= */

/* ⚠ Без fix — SIGSEGV (write NUL to .rodata). С fix — promote + empty. */
Test(fs2, sprintf_fs2empty_empty_format_no_crash) {
    __fs2_checkreset();
    fs2 dst = FS2EMPTY;

    long res = fs2_sprintf(&dst, "%s", "");

    CHECK_EQ("return", res, 0);
    CHECK_EQ("len", dst.len, 0);
    CHECK_TRUE("promoted or empty", fs2_is_empty(&dst));
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2, sprintf_canonical_empty_format) {
    __fs2_checkreset();
    fs2 dst = FS2();   /* v=NULL, sz=0 */

    long res = fs2_sprintf(&dst, "%s", "");

    CHECK_EQ("return", res, 0);
    CHECK_EQ("len", dst.len, 0);
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

/* =========================================================================
 * fs2_sprintf — nominal
 * ========================================================================= */

Test(fs2, sprintf_fs2empty_promotes) {
    __fs2_checkreset();
    fs2 dst = FS2EMPTY;

    long res = fs2_sprintf(&dst, "value=%d", 42);

    CHECK_EQ  ("return", res, 8);
    CHECK_STR_EQ("content", dst.v, "value=42");
    CHECK_TRUE ("ALLOC", fs2_is_alloc(&dst));
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2, sprintf_overwrites_existing) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("old content");

    long res = fs2_sprintf(&dst, "%d", 42);

    CHECK_EQ  ("return", res, 2);
    CHECK_STR_EQ("content", dst.v, "42");
    CHECK_EQ  ("len", dst.len, 2);
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2, sprintf_grows) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("x");
    size_t orig_sz = dst.sz;

    char big[1024];
    memset(big, 'A', 1023);
    big[1023] = '\0';

    long res = fs2_sprintf(&dst, "%s", big);

    CHECK_EQ  ("return", res, 1023);
    CHECK_TRUE("grew", dst.sz > orig_sz);
    CHECK_EQ  ("len", dst.len, 1023);
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

/* =========================================================================
 * fs2_sprintf — guards
 * ========================================================================= */

Test(fs2, sprintf_null_fmt_aborts, .signal = SIGABRT) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("x");
    fs2_sprintf(&dst, NULL);
    fs2_free(&dst);
}

Test(fs2, sprintf_null_dst_aborts, .signal = SIGABRT) {
    fs2_sprintf(NULL, "x");
}

Test(fs2, sprintf_static_nonempty_dst_aborts, .signal = SIGABRT) {
    fs2 dst = FS2LITERAL("hi");
    fs2_sprintf(&dst, "%d", 1);   /* cant_realloc → invfatal */
}

Test(fs2, sprintf_fs2empty_empty_format) {
    __fs2_checkreset();
    fs2 dst = FS2EMPTY;
    long res = fs2_sprintf(&dst, "%s", "");
    CHECK_EQ("return", res, 0);
    CHECK_TRUE("promoted", fs2_is_alloc(&dst));
    CHECK_NOT_NULL("v writable", dst.v);
    CHECK_FS2VALID(1, &dst);
    fs2_free(&dst);
    assert_balanced();
}

Test(fs2, sprintf_fs2empty_zero_precision) {
    __fs2_checkreset();
    fs2 dst = FS2EMPTY;
    long res = fs2_sprintf(&dst, "%.0s", "x");
    CHECK_EQ("return", res, 0);
    CHECK_TRUE("promoted", fs2_is_alloc(&dst));
    CHECK_FS2VALID(1, &dst);
    fs2_free(&dst);
    assert_balanced();
}

Test(fs2, sprintf_fs2literal_empty_empty_format) {
    __fs2_checkreset();
    fs2 dst = FS2LITERAL("");
    long res = fs2_sprintf(&dst, "%s", "");
    CHECK_EQ("return", res, 0);
    CHECK_TRUE("promoted", fs2_is_alloc(&dst));
    CHECK_FS2VALID(1, &dst);
    fs2_free(&dst);
    assert_balanced();
}

Test(fs2, sprintf_static_nonempty_aborts, .signal = SIGABRT) {
    fs2 dst = FS2LITERAL("hi");
    fs2_sprintf(&dst, "%d", 1);
}

Test(fs2, sprintf_alloc_fits_no_reserve) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("old");
    size_t orig_sz = dst.sz;
    long res = fs2_sprintf(&dst, "x");
    CHECK_EQ("return", res, 1);
    CHECK_EQ("sz unchanged (fast path)", dst.sz, orig_sz);
    CHECK_STR_EQ("content", dst.v, "x");
    CHECK_FS2VALID(1, &dst);
    fs2_free(&dst);
    assert_balanced();
}

/* =========================================================================
 * fs2_sprintf_concat — nominal append
 * ========================================================================= */

Test(fs2, sprintf_concat_basic) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("hello");

    long res = fs2_sprintf_concat(&dst, ", %s!", "world");

    CHECK_EQ("return", res, 8);   /* ", world!" = 8 */
    CHECK_STR_EQ("content", dst.v, "hello, world!");
    CHECK_EQ  ("len", dst.len, 13);
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2, sprintf_concat_empty_format_noop) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("data");
    size_t orig_sz = dst.sz;

    long res = fs2_sprintf_concat(&dst, "%s", "");

    CHECK_EQ("return", res, 0);
    CHECK_STR_EQ("unchanged", dst.v, "data");
    CHECK_EQ  ("len", dst.len, 4);
    CHECK_EQ  ("sz unchanged", dst.sz, orig_sz);
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2, sprintf_concat_chain) {
    __fs2_checkreset();
    fs2 dst = FS2();

    fs2_sprintf_concat(&dst, "%d", 1);
    fs2_sprintf_concat(&dst, ",%d", 2);
    fs2_sprintf_concat(&dst, ",%d", 3);

    CHECK_STR_EQ("content", dst.v, "1,2,3");
    CHECK_EQ  ("len", dst.len, 5);
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2, sprintf_concat_triggers_growth) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("x");
    size_t orig_sz = dst.sz;

    char big[128];
    memset(big, 'A', sizeof(big) - 1);
    big[sizeof(big) - 1] = '\0';

    fs2_sprintf_concat(&dst, "%s", big);

    CHECK_TRUE("grew", dst.sz > orig_sz);
    CHECK_EQ  ("len", dst.len, 1 + 127);
    CHECK_EQ  ("first char", dst.v[0], 'x');
    for (size_t i = 1; i < 128; i++)
        CHECK_EQ("expected 'A'", dst.v[i], 'A');
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

/* ⚠ Regression: fs2empty dst + empty format */
Test(fs2, sprintf_concat_fs2empty_empty_format) {
    __fs2_checkreset();
    fs2 dst = FS2EMPTY;

    long res = fs2_sprintf_concat(&dst, "%s", "");

    CHECK_EQ("return", res, 0);
    CHECK_TRUE("promoted", fs2_is_alloc(&dst));
    CHECK_NOT_NULL("v writable", dst.v);
    CHECK_STR_EQ("v empty", dst.v, "");
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2, sprintf_concat_fs2empty_append) {
    __fs2_checkreset();
    fs2 dst = FS2EMPTY;

    long res = fs2_sprintf_concat(&dst, "hello");

    CHECK_EQ("return", res, 5);
    CHECK_STR_EQ("content", dst.v, "hello");
    CHECK_TRUE ("promoted", fs2_is_alloc(&dst));
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2, sprintf_concat_canonical_append) {
    __fs2_checkreset();
    fs2 dst = FS2();

    long res = fs2_sprintf_concat(&dst, "%d", 42);

    CHECK_EQ("return", res, 2);
    CHECK_STR_EQ("content", dst.v, "42");
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2, sprintf_concat_local_within_capacity) {
    __fs2_checkreset();
    FS2LOCAL(dst, 64);
    memcpy(dst.v, "start:", 7);
    fs2_setlen(&dst, 6);

    fs2_sprintf_concat(&dst, "%s=%d", "key", 42);

    CHECK_STR_EQ("content", dst.v, "start:key=42");
    CHECK_TRUE ("still LOCAL", fs2_is_local(&dst));
    CHECK_FS2VALID(1, &dst);
}

/* =========================================================================
 * fs2_sprintf_concat — guards
 * ========================================================================= */

Test(fs2, sprintf_concat_null_fmt_aborts, .signal = SIGABRT) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("x");
    fs2_sprintf_concat(&dst, NULL);
    fs2_free(&dst);
}

Test(fs2, sprintf_concat_null_dst_aborts, .signal = SIGABRT) {
    fs2_sprintf_concat(NULL, "x");
}

Test(fs2, sprintf_concat_static_nonempty_aborts, .signal = SIGABRT) {
    fs2 dst = FS2LITERAL("hi");
    fs2_sprintf_concat(&dst, "%d", 1);   /* cant_realloc → invfatal */
}

Test(fs2, sprintf_concat_local_overflow_aborts, .signal = SIGABRT) {
    __fs2_checkreset();
    FS2LOCAL(dst, 4);
    fs2_sprintf_concat(&dst, "hello world");   /* > capacity → abort */
}

/* =========================================================================
 * fs2_substr — nominal
 * ========================================================================= */

Test(fs2, substr_middle) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abcdef");
    CHECK_FS2VALID(1, &s);

    fs2 *ret = fs2_substr(&s, 2, 3);

    CHECK_PTR_EQ("returns s", ret, &s);
    CHECK_STR_EQ("cde", s.v, "cde");
    CHECK_EQ  ("len", s.len, 3);
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, substr_from_zero) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello world");

    fs2_substr(&s, 0, 5);

    CHECK_STR_EQ("hello", s.v, "hello");
    CHECK_EQ  ("len", s.len, 5);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, substr_whole_string_noop) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");
    void *orig_v = s.v;

    fs2_substr(&s, 0, 5);

    CHECK_PTR_EQ("v unchanged", s.v, orig_v);
    CHECK_STR_EQ("unchanged", s.v, "hello");
    CHECK_EQ  ("len", s.len, 5);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, substr_to_end) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abcdef");

    fs2_substr(&s, 3, 100);   /* clamp to len-from = 3 */

    CHECK_STR_EQ("def", s.v, "def");
    CHECK_EQ  ("len", s.len, 3);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_substr — clamping edge cases
 * ========================================================================= */

Test(fs2, substr_from_beyond_len) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");

    fs2_substr(&s, 10, 5);   /* from > len → clamps to empty */

    CHECK_STR_EQ("empty", s.v, "");
    CHECK_EQ  ("len", s.len, 0);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, substr_from_at_len) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");

    fs2_substr(&s, 3, 5);   /* from == len → empty */

    CHECK_STR_EQ("empty", s.v, "");
    CHECK_EQ  ("len", s.len, 0);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, substr_len_zero) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");

    fs2_substr(&s, 1, 0);

    CHECK_STR_EQ("empty", s.v, "");
    CHECK_EQ  ("len", s.len, 0);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_substr — LOCAL
 * ========================================================================= */

Test(fs2, substr_local_within) {
    __fs2_checkreset();
    FS2LOCAL(s, 16);
    memcpy(s.v, "abcdef", 7);
    fs2_setlen(&s, 6);
    CHECK_FS2VALID(1, &s);

    fs2_substr(&s, 2, 3);

    CHECK_STR_EQ("cde", s.v, "cde");
    CHECK_EQ  ("len", s.len, 3);
    CHECK_TRUE("still LOCAL", fs2_is_local(&s));
    CHECK_FS2VALID(2, &s);
}

/* =========================================================================
 * fs2_substr — empty variants (skip, no abort)
 * ========================================================================= */

Test(fs2, substr_fs2empty_noop) {
    fs2 s = FS2EMPTY;

    fs2_substr(&s, 0, 5);

    CHECK_TRUE("still STATIC", fs2_is_static(&s));
    CHECK_TRUE("eq_empty", fs2_eq_empty(&s));
    CHECK_EQ  ("len", s.len, 0);
    CHECK_FS2VALID(1, &s);
}

Test(fs2, substr_canonical_noop) {
    __fs2_checkreset();
    fs2 s = FS2();

    fs2_substr(&s, 0, 5);

    CHECK_EQ("len", s.len, 0);
    CHECK_PTR_EQ("v NULL", s.v, NULL);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, substr_empty_literal_noop) {
    fs2 s = FS2LITERAL("");

    fs2_substr(&s, 0, 5);

    CHECK_TRUE("still STATIC", fs2_is_static(&s));
    CHECK_FS2VALID(1, &s);
}

/* =========================================================================
 * fs2_substr — guards
 * ========================================================================= */

Test(fs2, substr_null_aborts, .signal = SIGABRT) {
    fs2_substr(NULL, 0, 5);
}

Test(fs2, substr_static_nonempty_aborts, .signal = SIGABRT) {
    fs2 s = FS2LITERAL("hello");
    fs2_substr(&s, 1, 3);   /* write .rodata → abort */
}

/* identity: from==0, len==s->len — still aborts (mutator on readonly) */
Test(fs2, substr_static_nonempty_identity_aborts, .signal = SIGABRT) {
    fs2 s = FS2LITERAL("hello");
    fs2_substr(&s, 0, 5);   /* logically no-op, but cant_mutate aborts */
}

/* =========================================================================
 * fs2_substr — overflow / huge values
 * ========================================================================= */

Test(fs2, substr_huge_len_clamps) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");

    fs2_substr(&s, 0, SIZE_MAX);

    CHECK_STR_EQ("whole string (clamped)", s.v, "abc");
    CHECK_EQ  ("len", s.len, 3);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, substr_from_len_overflow_safe) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abcdef");

    /* from near SIZE_MAX, len huge → from clamps first, then len → 0 */
    fs2_substr(&s, SIZE_MAX, SIZE_MAX);

    CHECK_EQ("len", s.len, 0);
    CHECK_STR_EQ("empty", s.v, "");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_substr_bounds — direct
 * ========================================================================= */

Test(fs2, substr_bounds_normal) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abcdef");
    size_t from = 2, len = 3;
    fs2_substr_bounds(&s, &from, &len);
    CHECK_EQ("from", from, 2);
    CHECK_EQ("len", len, 3);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2, substr_bounds_len_clamp) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");
    size_t from = 1, len = 100;
    fs2_substr_bounds(&s, &from, &len);
    CHECK_EQ("from", from, 1);
    CHECK_EQ("len clamped", len, 2);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2, substr_bounds_from_beyond) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");
    size_t from = 10, len = 5;
    fs2_substr_bounds(&s, &from, &len);
    CHECK_EQ("from clamped to len", from, 3);
    CHECK_EQ("len zeroed", len, 0);
    fs2_free(&s);
    assert_balanced();
}

/* ⚠ Canonical empty: !s->v branch would trigger, but *from >= 0 covers it */
Test(fs2, substr_bounds_canonical_empty) {
    __fs2_checkreset();
    fs2 s = FS2();   /* v=NULL, len=0 */
    size_t from = 0, len = 5;
    fs2_substr_bounds(&s, &from, &len);
    CHECK_EQ("from", from, 0);
    CHECK_EQ("len", len, 0);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2, substr_bounds_fs2empty) {
    fs2 s = FS2EMPTY;   /* v="", len=0 */
    size_t from = 0, len = 5;
    fs2_substr_bounds(&s, &from, &len);
    CHECK_EQ("from", from, 0);
    CHECK_EQ("len", len, 0);
    CHECK_FS2VALID(1, &s);
}

Test(fs2, substr_bounds_overflow_safe) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");
    size_t from = 1, len = SIZE_MAX;
    fs2_substr_bounds(&s, &from, &len);
    CHECK_EQ("len clamped safely", len, 2);   /* не SIZE_MAX - 1 */
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_substr_new — empty result → FS2EMPTY (contract)
 * ========================================================================= */

/* ⚠ Главный регресс: раньше возвращал FS2() */
Test(fs2, substr_new_empty_returns_fs2empty) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    fs2 r = fs2_substr_new(&s, 5, 0);   /* from==len, len=0 */

    CHECK_EQ  ("len", r.len, 0);
    CHECK_TRUE("STATIC (FS2EMPTY)", fs2_is_static(&r));
    CHECK_EQ  ("sz", r.sz, 1);
    CHECK_NOT_NULL("v readable", r.v);
    CHECK_STR_EQ("v empty", r.v, "");
    CHECK_FALSE("not canon", fs2_is_canon(&r));
    CHECK_FS2VALID(1, &r);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, substr_new_from_beyond_returns_fs2empty) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");

    fs2 r = fs2_substr_new(&s, 10, 5);   /* from > len → clamp → empty */

    CHECK_TRUE("eq_empty shape", fs2_eq_empty(&r));
    CHECK_FS2VALID(1, &r);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, substr_new_len_zero_returns_fs2empty) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    fs2 r = fs2_substr_new(&s, 2, 0);

    CHECK_TRUE("eq_empty shape", fs2_eq_empty(&r));
    CHECK_FS2VALID(1, &r);

    fs2_free(&s);
    assert_balanced();
}

/* Nominal — ALLOC, не FS2EMPTY */
Test(fs2, substr_new_middle) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abcdef");

    fs2 r = fs2_substr_new(&s, 2, 3);

    CHECK_STR_EQ("cde", r.v, "cde");
    CHECK_EQ  ("len", r.len, 3);
    CHECK_TRUE("ALLOC", fs2_is_alloc(&r));
    CHECK_FS2VALID(1, &r);

    fs2_free(&s);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2, substr_new_from_static_src) {
    fs2 src = FS2LITERAL("hello");
    fs2 r = fs2_substr_new(&src, 1, 3);

    CHECK_STR_EQ("ell", r.v, "ell");
    CHECK_TRUE("ALLOC copy", fs2_is_alloc(&r));
    CHECK_FS2VALID(1, &r);
    fs2_free(&r);
}

Test(fs2, substr_new_null_aborts, .signal = SIGABRT) {
    fs2_substr_new(NULL, 0, 5);
}

/* Consistency: empty result matches fromstr("") */
Test(fs2, substr_new_empty_matches_fromstr_empty) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    fs2 a = fs2_substr_new(&s, 5, 0);
    fs2 b = fs2_fromstr("");

    CHECK_EQ  ("len",   a.len,   b.len);
    CHECK_EQ  ("sz",    a.sz,    b.sz);
    CHECK_EQ  ("flags", a.flags, b.flags);
    CHECK_STR_EQ("v",   a.v,     b.v);
    CHECK_FS2VALID(1, &a);
    CHECK_FS2VALID(2, &b);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_heap_substr — nominal
 * ========================================================================= */

Test(fs2, heap_substr_basic) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("abcdef");

    fs2 *h = fs2_heap_substr(&src, 2, 3);

    CHECK_STR_EQ("content", h->v, "cde");
    CHECK_EQ  ("len", h->len, 3);
    CHECK_TRUE ("ALLOC",     fs2_is_alloc(h));
    CHECK_TRUE ("BODYALLOC", fs2_is_bodyalloc(h));
    CHECK_FALSE("not STATIC", fs2_is_static(h));
    CHECK_TRUE ("independent buffer", h->v != src.v);
    CHECK_FS2VALID(1, h);
    CHECK_FS2VALID(2, &src);

    fs2_free(&src);
    fs2_free(h);
    assert_balanced();
}

Test(fs2, heap_substr_from_static) {
    __fs2_checkreset();
    fs2 src = FS2LITERAL("hello");

    fs2 *h = fs2_heap_substr(&src, 1, 3);

    CHECK_STR_EQ("ell", h->v, "ell");
    CHECK_TRUE ("ALLOC",     fs2_is_alloc(h));
    CHECK_TRUE ("BODYALLOC", fs2_is_bodyalloc(h));
    CHECK_FS2VALID(1, h);

    fs2_free(h);
    assert_balanced();
}

/* =========================================================================
 * fs2_heap_substr — empty result → STATIC|BODYALLOC
 * ========================================================================= */

/* ⚠ Empty result: substr_new returns FS2EMPTY (после fix) → STATIC|BODYALLOC */
Test(fs2, heap_substr_empty_from_at_len) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello");

    fs2 *h = fs2_heap_substr(&src, 5, 0);

    CHECK_EQ  ("len", h->len, 0);
    CHECK_EQ  ("sz",  h->sz, 1);
    CHECK_TRUE("STATIC axis", fs2_is_static(h));
    CHECK_TRUE("BODYALLOC",   fs2_is_bodyalloc(h));
    CHECK_FALSE("not ALLOC-string", fs2_is_alloc(h));
    CHECK_NOT_NULL("v readable", h->v);
    CHECK_STR_EQ("v empty", h->v, "");
    CHECK_FS2VALID(1, h);

    /* free освобождает только struct (v=.rodata) */
    fs2_free(h);
    fs2_free(&src);
    assert_balanced();
}

Test(fs2, heap_substr_empty_from_beyond_len) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("abc");

    fs2 *h = fs2_heap_substr(&src, 10, 5);

    CHECK_TRUE("eq_empty shape", fs2_eq_empty(h));
    CHECK_TRUE("BODYALLOC",     fs2_is_bodyalloc(h));
    CHECK_FS2VALID(1, h);

    fs2_free(h);
    fs2_free(&src);
    assert_balanced();
}

Test(fs2, heap_substr_empty_src) {
    __fs2_checkreset();
    fs2 src = FS2EMPTY;

    fs2 *h = fs2_heap_substr(&src, 0, 5);

    CHECK_TRUE("eq_empty shape", fs2_eq_empty(h));
    CHECK_TRUE("BODYALLOC",     fs2_is_bodyalloc(h));
    CHECK_FS2VALID(1, h);

    fs2_free(h);
    assert_balanced();
}

/* =========================================================================
 * fs2_heap_substr — guards
 * ========================================================================= */

Test(fs2, heap_substr_null_aborts, .signal = SIGABRT) {
    fs2_heap_substr(NULL, 0, 5);
}

/* =========================================================================
 * Round-trip через heaptostack
 * ========================================================================= */

Test(fs2, heap_substr_heaptostack_roundtrip) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("abcdef");

    fs2 *h = fs2_heap_substr(&src, 1, 4);

    fs2 s = fs2_heaptostack(&h);

    CHECK_PTR_EQ("h NULL", h, NULL);
    CHECK_STR_EQ("content", s.v, "bcde");
    CHECK_TRUE ("ALLOC", fs2_is_alloc(&s));
    CHECK_FALSE("no BODYALLOC", fs2_is_bodyalloc(&s));
    CHECK_FS2VALID(1, &s);

    fs2_free(&src);
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * Independent from src after construction
 * ========================================================================= */

Test(fs2, heap_substr_independent_from_src) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("original");
    fs2 *h = fs2_heap_substr(&src, 0, 8);

    /* Mutate src — h не задет */
    fs2_cpystr(&src, "changed");

    CHECK_STR_EQ("h intact", h->v, "original");
    CHECK_STR_EQ("src changed", src.v, "changed");
    CHECK_FS2VALID(1, h);
    CHECK_FS2VALID(2, &src);

    fs2_free(&src);
    fs2_free(h);
    assert_balanced();
}

/* =========================================================================
 * fs2_left — prefix via fs2_substr(s, 0, len)
 * ========================================================================= */

Test(fs2, left_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abcdef");

    fs2 *ret = fs2_left(&s, 3);

    CHECK_PTR_EQ("returns s", ret, &s);
    CHECK_STR_EQ("abc", s.v, "abc");
    CHECK_EQ  ("len", s.len, 3);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, left_len_zero) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    fs2_left(&s, 0);

    CHECK_STR_EQ("empty", s.v, "");
    CHECK_EQ  ("len", s.len, 0);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, left_len_eq_slen_noop) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");
    void *orig_v = s.v;

    fs2_left(&s, 5);

    CHECK_PTR_EQ("v unchanged", s.v, orig_v);
    CHECK_STR_EQ("unchanged", s.v, "hello");
    CHECK_EQ  ("len", s.len, 5);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, left_len_beyond_clamps) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");

    fs2_left(&s, 100);

    CHECK_STR_EQ("whole string (clamped)", s.v, "abc");
    CHECK_EQ  ("len", s.len, 3);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, left_single_char) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    fs2_left(&s, 1);

    CHECK_STR_EQ("h", s.v, "h");
    CHECK_EQ  ("len", s.len, 1);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, left_local) {
    __fs2_checkreset();
    FS2LOCAL(s, 16);
    memcpy(s.v, "abcdef", 7);
    fs2_setlen(&s, 6);

    fs2_left(&s, 3);

    CHECK_STR_EQ("abc", s.v, "abc");
    CHECK_TRUE("still LOCAL", fs2_is_local(&s));
    CHECK_FS2VALID(1, &s);
}

/* =========================================================================
 * fs2_left — guards (delegated to fs2_substr / fs2_cant_mutate)
 * ========================================================================= */

Test(fs2, left_null_aborts, .signal = SIGABRT) {
    fs2_left(NULL, 3);
}

Test(fs2, left_static_nonempty_aborts, .signal = SIGABRT) {
    fs2 s = FS2LITERAL("hello");
    fs2_left(&s, 3);   /* cant_mutate → invfatal */
}

/* Empty variants — silent skip (no abort) */
Test(fs2, left_fs2empty_noop) {
    fs2 s = FS2EMPTY;
    fs2_left(&s, 5);
    CHECK_TRUE("eq_empty", fs2_eq_empty(&s));
    CHECK_FS2VALID(1, &s);
}

Test(fs2, left_canonical_noop) {
    __fs2_checkreset();
    fs2 s = FS2();
    fs2_left(&s, 5);
    CHECK_EQ("len", s.len, 0);
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_left_new — stack constructor
 * ========================================================================= */

Test(fs2, left_new_basic) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("abcdef");

    fs2 r = fs2_left_new(&src, 3);

    CHECK_STR_EQ("abc", r.v, "abc");
    CHECK_EQ  ("len", r.len, 3);
    CHECK_TRUE("ALLOC", fs2_is_alloc(&r));
    CHECK_FALSE("not BODYALLOC", fs2_is_bodyalloc(&r));
    CHECK_FS2VALID(1, &r);
    CHECK_FS2VALID(2, &src);   /* src untouched */

    fs2_free(&src);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2, left_new_single_char) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello");

    fs2 r = fs2_left_new(&src, 1);

    CHECK_STR_EQ("h", r.v, "h");
    CHECK_EQ  ("len", r.len, 1);
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2, left_new_len_zero_returns_fs2empty) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello");

    fs2 r = fs2_left_new(&src, 0);

    CHECK_TRUE("eq_empty shape", fs2_eq_empty(&r));
    CHECK_FALSE("not canon", fs2_is_canon(&r));
    CHECK_NOT_NULL("v readable", r.v);
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    assert_balanced();
}

Test(fs2, left_new_len_beyond_clamps) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("abc");

    fs2 r = fs2_left_new(&src, 100);

    CHECK_STR_EQ("whole (clamped)", r.v, "abc");
    CHECK_EQ  ("len", r.len, 3);
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2, left_new_from_static_src) {
    __fs2_checkreset();
    fs2 src = FS2LITERAL("hello");

    fs2 r = fs2_left_new(&src, 3);

    CHECK_STR_EQ("hel", r.v, "hel");
    CHECK_TRUE("ALLOC copy (not aliased)", fs2_is_alloc(&r));
    CHECK_FALSE("not STATIC", fs2_is_static(&r));
    CHECK_FS2VALID(1, &r);

    fs2_free(&r);
}

Test(fs2, left_new_empty_src_returns_fs2empty) {
    __fs2_checkreset();
    fs2 src = FS2EMPTY;

    fs2 r = fs2_left_new(&src, 5);

    CHECK_TRUE("eq_empty shape", fs2_eq_empty(&r));
    CHECK_FS2VALID(1, &r);
}

Test(fs2, left_new_src_untouched) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("original");
    void *orig_v = src.v;

    fs2 r = fs2_left_new(&src, 4);

    CHECK_PTR_EQ("src v unchanged", src.v, orig_v);
    CHECK_STR_EQ("src content unchanged", src.v, "original");
    CHECK_STR_EQ("result is copy", r.v, "orig");
    CHECK_FS2VALID(1, &r);
    CHECK_FS2VALID(2, &src);

    fs2_free(&src);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2, left_new_null_aborts, .signal = SIGABRT) {
    fs2_left_new(NULL, 3);
}

/* =========================================================================
 * fs2_heap_left — heap constructor
 * ========================================================================= */

Test(fs2, heap_left_basic) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("abcdef");

    fs2 *h = fs2_heap_left(&src, 3);

    CHECK_STR_EQ("abc", h->v, "abc");
    CHECK_EQ  ("len", h->len, 3);
    CHECK_TRUE("ALLOC",     fs2_is_alloc(h));
    CHECK_TRUE("BODYALLOC", fs2_is_bodyalloc(h));
    CHECK_FS2VALID(1, h);
    CHECK_FS2VALID(2, &src);

    fs2_free(&src);
    fs2_free(h);
    assert_balanced();
}

Test(fs2, heap_left_single_char) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello");

    fs2 *h = fs2_heap_left(&src, 1);

    CHECK_STR_EQ("h", h->v, "h");
    CHECK_EQ  ("len", h->len, 1);
    CHECK_TRUE("BODYALLOC", fs2_is_bodyalloc(h));
    CHECK_FS2VALID(1, h);

    fs2_free(&src);
    fs2_free(h);
    assert_balanced();
}

/* ⚠ Empty result: FS2EMPTY-shape + BODYALLOC = STATIC|BODYALLOC */
Test(fs2, heap_left_len_zero_returns_static_bodyalloc) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello");

    fs2 *h = fs2_heap_left(&src, 0);

    CHECK_EQ  ("len", h->len, 0);
    CHECK_EQ  ("sz",  h->sz, 1);
    CHECK_TRUE("STATIC axis", fs2_is_static(h));
    CHECK_TRUE("BODYALLOC",   fs2_is_bodyalloc(h));
    CHECK_FALSE("not ALLOC-string", fs2_is_alloc(h));
    CHECK_STR_EQ("v empty", h->v, "");
    CHECK_FS2VALID(1, h);

    fs2_free(h);   /* frees only struct */
    fs2_free(&src);
    assert_balanced();
}

Test(fs2, heap_left_len_beyond_clamps) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("abc");

    fs2 *h = fs2_heap_left(&src, 100);

    CHECK_STR_EQ("whole (clamped)", h->v, "abc");
    CHECK_TRUE("ALLOC", fs2_is_alloc(h));
    CHECK_FS2VALID(1, h);

    fs2_free(&src);
    fs2_free(h);
    assert_balanced();
}

Test(fs2, heap_left_from_static_src) {
    __fs2_checkreset();
    fs2 src = FS2LITERAL("hello");

    fs2 *h = fs2_heap_left(&src, 3);

    CHECK_STR_EQ("hel", h->v, "hel");
    CHECK_TRUE("ALLOC (heap copy)", fs2_is_alloc(h));
    CHECK_TRUE("BODYALLOC", fs2_is_bodyalloc(h));
    CHECK_FS2VALID(1, h);

    fs2_free(h);
}

Test(fs2, heap_left_src_untouched) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("original");
    void *orig_v = src.v;

    fs2 *h = fs2_heap_left(&src, 4);

    CHECK_PTR_EQ("src v unchanged", src.v, orig_v);
    CHECK_STR_EQ("src content unchanged", src.v, "original");
    CHECK_STR_EQ("result is copy", h->v, "orig");
    CHECK_FS2VALID(1, h);
    CHECK_FS2VALID(2, &src);

    fs2_free(&src);
    fs2_free(h);
    assert_balanced();
}

Test(fs2, heap_left_null_aborts, .signal = SIGABRT) {
    fs2_heap_left(NULL, 3);
}

/* =========================================================================
 * Consistency: left_new == *heap_left (content-wise)
 * ========================================================================= */

Test(fs2, left_new_and_heap_left_match) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("abcdef");
    const size_t cases[] = { 0, 1, 3, 5, 6, 100 };
    size_t n = sizeof(cases) / sizeof(cases[0]);

    for (size_t i = 0; i < n; i++) {
        fs2  stack_v = fs2_left_new(&src, cases[i]);
        fs2 *heap_v  = fs2_heap_left(&src, cases[i]);

        /* Same content, len, sz */
        CHECK_EQ  ("len",     stack_v.len, heap_v->len);
        CHECK_EQ  ("sz",      stack_v.sz,  heap_v->sz);
        CHECK_STR_EQ("content", stack_v.v, heap_v->v);

        /* Same string-axis flags (ignore BODYALLOC) */
        CHECK_EQ  ("string-axis flags match",
                   stack_v.flags & ~FS2_BODYALLOC,
                   heap_v->flags & ~FS2_BODYALLOC);

        CHECK_TRUE ("heap has BODYALLOC",  fs2_is_bodyalloc(heap_v));
        CHECK_FALSE("stack no BODYALLOC", fs2_is_bodyalloc(&stack_v));
        CHECK_FS2VALID((int)(i * 2),     &stack_v);
        CHECK_FS2VALID((int)(i * 2 + 1), heap_v);

        fs2_free(&stack_v);
        fs2_free(heap_v);
    }

    fs2_free(&src);
    assert_balanced();
}

/* =========================================================================
 * fs2_right — nominal
 * ========================================================================= */

Test(fs2, right_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abcdef");

    fs2 *ret = fs2_right(&s, 3);

    CHECK_PTR_EQ("returns s", ret, &s);
    CHECK_STR_EQ("def", s.v, "def");
    CHECK_EQ  ("len", s.len, 3);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, right_single_char) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    fs2_right(&s, 1);

    CHECK_STR_EQ("o", s.v, "o");
    CHECK_EQ  ("len", s.len, 1);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, right_len_zero_noop) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");
    /* len=0 → from=5, substr(5, 0) → empty */
    fs2_right(&s, 0);
    CHECK_STR_EQ("empty", s.v, "");
    CHECK_EQ  ("len", s.len, 0);
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

/* ⚠ Underflow-safe: len > s->len → from=0, substr clamps len → whole string */
Test(fs2, right_len_beyond_clamps_no_underflow) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");

    fs2_right(&s, 100);

    CHECK_STR_EQ("whole (clamped)", s.v, "abc");
    CHECK_EQ  ("len", s.len, 3);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, right_huge_len_size_max) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    fs2_right(&s, SIZE_MAX);   /* from=0, substr clamps SIZE_MAX → len */

    CHECK_STR_EQ("whole", s.v, "hello");
    CHECK_EQ  ("len", s.len, 5);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, right_local) {
    __fs2_checkreset();
    FS2LOCAL(s, 16);
    memcpy(s.v, "abcdef", 7);
    fs2_setlen(&s, 6);

    fs2_right(&s, 2);

    CHECK_STR_EQ("ef", s.v, "ef");
    CHECK_TRUE("still LOCAL", fs2_is_local(&s));
    CHECK_FS2VALID(1, &s);
}

/* =========================================================================
 * fs2_right — guards (consistent с fs2_left: abort via cant_mutate)
 * ========================================================================= */

Test(fs2, right_null_aborts, .signal = SIGABRT) {
    /* ← было silent return NULL, стало SIGABRT */
    fs2_right(NULL, 3);
}

Test(fs2, right_static_nonempty_aborts, .signal = SIGABRT) {
    fs2 s = FS2LITERAL("hello");
    fs2_right(&s, 3);   /* ← было silent return s (для len>s->len), теперь abort */
}

/* ⚠ Главное: даже clamp-кейс (len>=s->len) abort'ит на non-empty STATIC —
 * консистентно с fs2_left, который тоже abort'ит через cant_mutate. */
Test(fs2, right_static_nonempty_clamp_aborts, .signal = SIGABRT) {
    fs2 s = FS2LITERAL("hi");
    fs2_right(&s, 100);   /* было silent return s, теперь abort */
}

Test(fs2, right_fs2empty_noop) {
    fs2 s = FS2EMPTY;
    fs2_right(&s, 5);
    CHECK_TRUE("eq_empty", fs2_eq_empty(&s));
    CHECK_FS2VALID(1, &s);
}

Test(fs2, right_canonical_noop) {
    __fs2_checkreset();
    fs2 s = FS2();
    fs2_right(&s, 5);
    CHECK_EQ("len", s.len, 0);
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2, right_len_eq_slen_noop) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");
    void *orig_v = s.v;

    fs2_right(&s, 5);

    CHECK_PTR_EQ("v unchanged", s.v, orig_v);
    CHECK_STR_EQ("unchanged", s.v, "hello");
    CHECK_EQ  ("len", s.len, 5);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_right_new — nominal
 * ========================================================================= */

Test(fs2, right_new_basic) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("abcdef");

    fs2 r = fs2_right_new(&src, 3);

    CHECK_STR_EQ("def", r.v, "def");
    CHECK_EQ  ("len", r.len, 3);
    CHECK_TRUE("ALLOC", fs2_is_alloc(&r));
    CHECK_FS2VALID(1, &r);
    CHECK_FS2VALID(2, &src);   /* src untouched */

    fs2_free(&src);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2, right_new_single_char) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello");

    fs2 r = fs2_right_new(&src, 1);

    CHECK_STR_EQ("o", r.v, "o");
    CHECK_EQ  ("len", r.len, 1);
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    fs2_free(&r);
    assert_balanced();
}

/* len == s->len → clone (whole string) */
Test(fs2, right_new_len_eq_slen_clones_whole) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello");

    fs2 r = fs2_right_new(&src, 5);

    CHECK_STR_EQ("whole", r.v, "hello");
    CHECK_EQ  ("len", r.len, 5);
    CHECK_TRUE("ALLOC", fs2_is_alloc(&r));
    CHECK_TRUE("independent buffer", r.v != src.v);
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2, right_new_len_beyond_clamps_to_whole) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("abc");

    fs2 r = fs2_right_new(&src, 100);

    CHECK_STR_EQ("whole (clamped)", r.v, "abc");
    CHECK_EQ  ("len", r.len, 3);
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    fs2_free(&r);
    assert_balanced();
}

/* ⚠ len == 0: substring empty → FS2EMPTY */
Test(fs2, right_new_len_zero_returns_fs2empty) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello");

    fs2 r = fs2_right_new(&src, 0);

    CHECK_TRUE("eq_empty shape", fs2_eq_empty(&r));
    CHECK_FALSE("not canon", fs2_is_canon(&r));
    CHECK_NOT_NULL("v readable", r.v);
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    assert_balanced();
}

Test(fs2, right_new_from_static_src) {
    __fs2_checkreset();
    fs2 src = FS2LITERAL("hello");

    fs2 r = fs2_right_new(&src, 3);

    CHECK_STR_EQ("llo", r.v, "llo");
    CHECK_TRUE("ALLOC copy (not aliased)", fs2_is_alloc(&r));
    CHECK_FALSE("not STATIC", fs2_is_static(&r));
    CHECK_FS2VALID(1, &r);

    fs2_free(&r);
}

Test(fs2, right_new_from_static_src_clamp) {
    __fs2_checkreset();
    fs2 src = FS2LITERAL("hi");

    fs2 r = fs2_right_new(&src, 100);

    /* clone path — ALLOC copy of literal */
    CHECK_STR_EQ("whole", r.v, "hi");
    CHECK_TRUE("ALLOC", fs2_is_alloc(&r));
    CHECK_FS2VALID(1, &r);

    fs2_free(&r);
}

/* =========================================================================
 * fs2_right_new — empty src
 * ========================================================================= */

Test(fs2, right_new_fs2empty_returns_fs2empty) {
    fs2 src = FS2EMPTY;

    fs2 r = fs2_right_new(&src, 5);

    CHECK_TRUE("eq_empty shape", fs2_eq_empty(&r));
    CHECK_FS2VALID(1, &r);
}

Test(fs2, right_new_canonical_returns_fs2empty) {
    __fs2_checkreset();
    fs2 src = FS2();

    fs2 r = fs2_right_new(&src, 5);

    CHECK_TRUE("eq_empty shape", fs2_eq_empty(&r));
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    assert_balanced();
}

/* =========================================================================
 * fs2_right_new — guards
 * ========================================================================= */

Test(fs2, right_new_null_aborts, .signal = SIGABRT) {
    /* ← было silent FS2(), стало SIGABRT */
    fs2_right_new(NULL, 5);
}

/* =========================================================================
 * src immutability
 * ========================================================================= */

Test(fs2, right_new_src_untouched) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("original");
    void *orig_v = src.v;

    fs2 r = fs2_right_new(&src, 4);

    CHECK_PTR_EQ("src v unchanged", src.v, orig_v);
    CHECK_STR_EQ("src content unchanged", src.v, "original");
    CHECK_STR_EQ("result is copy", r.v, "inal");
    CHECK_FS2VALID(1, &r);
    CHECK_FS2VALID(2, &src);

    fs2_free(&src);
    fs2_free(&r);
    assert_balanced();
}

/* =========================================================================
 * Consistency: right_new == left_new(s, len) на reversed
 * ========================================================================= */

Test(fs2, right_new_len_eq_zero_matches_empty) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("hello");
    fs2 b = fs2_fromstr("world");

    fs2 r1 = fs2_right_new(&a, 0);
    fs2 r2 = fs2_right_new(&b, 0);

    /* Both empty, same shape */
    CHECK_TRUE("r1 empty", fs2_is_empty(&r1));
    CHECK_TRUE("r2 empty", fs2_is_empty(&r2));
    CHECK_EQ  ("same flags", r1.flags, r2.flags);
    CHECK_EQ  ("same sz",    r1.sz,    r2.sz);
    CHECK_FS2VALID(1, &r1);
    CHECK_FS2VALID(2, &r2);

    fs2_free(&a);
    fs2_free(&b);
    assert_balanced();
}

/* =========================================================================
 * fs2_heap_right — nominal (substring path: len < s->len)
 * ========================================================================= */

Test(fs2_api, heap_right_basic) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("abcdef");

    fs2 *r = fs2_heap_right(&src, 3);

    CHECK_STR_EQ("def", r->v, "def");
    CHECK_EQ  ("len", r->len, 3);
    CHECK_TRUE("bodyalloc", fs2_is_bodyalloc(r));
    CHECK_TRUE("ALLOC",     fs2_is_alloc(r));
    CHECK_FS2VALID(1, r);
    CHECK_FS2VALID(2, &src);

    fs2_free(&src);
    fs2_free(r);
    assert_balanced();
}

Test(fs2_api, heap_right_single_char) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello");

    fs2 *r = fs2_heap_right(&src, 1);

    CHECK_STR_EQ("o", r->v, "o");
    CHECK_EQ  ("len", r->len, 1);
    CHECK_TRUE("bodyalloc", fs2_is_bodyalloc(r));
    CHECK_FS2VALID(1, r);

    fs2_free(&src);
    fs2_free(r);
    assert_balanced();
}

Test(fs2_api, heap_right_len_one_less_than_slen) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("abc");

    fs2 *r = fs2_heap_right(&src, 2);

    CHECK_STR_EQ("bc", r->v, "bc");
    CHECK_EQ  ("len", r->len, 2);
    CHECK_FS2VALID(1, r);

    fs2_free(&src);
    fs2_free(r);
    assert_balanced();
}

/* =========================================================================
 * fs2_heap_right — clone path (len >= s->len)
 * ========================================================================= */

Test(fs2_api, heap_right_len_eq_slen_clones_whole) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello");

    fs2 *r = fs2_heap_right(&src, 5);

    CHECK_STR_EQ("whole", r->v, "hello");
    CHECK_EQ  ("len", r->len, 5);
    CHECK_TRUE("bodyalloc", fs2_is_bodyalloc(r));
    CHECK_TRUE("ALLOC",     fs2_is_alloc(r));
    CHECK_TRUE("independent buffer", r->v != src.v);
    CHECK_FS2VALID(1, r);

    fs2_free(&src);
    fs2_free(r);
    assert_balanced();
}

Test(fs2_api, heap_right_len_beyond_clamps_to_whole) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("abc");

    fs2 *r = fs2_heap_right(&src, 100);

    CHECK_STR_EQ("whole (clamped)", r->v, "abc");
    CHECK_EQ  ("len", r->len, 3);
    CHECK_TRUE("bodyalloc", fs2_is_bodyalloc(r));
    CHECK_FS2VALID(1, r);

    fs2_free(&src);
    fs2_free(r);
    assert_balanced();
}

/* =========================================================================
 * fs2_heap_right — len == 0 при непустом src (substring path → пусто)
 *   fs2_heap_substr(s, s->len, 0) → stack FS2EMPTY → stacktoheap
 *   → BODYALLOC | STATIC, eq_empty=true, canon=false, v!=""
 * ========================================================================= */

Test(fs2_api, heap_right_len_zero_is_fs2empty_shape) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello");

    fs2 *r = fs2_heap_right(&src, 0);

    CHECK_TRUE ("eq_empty shape", fs2_eq_empty(r));
    CHECK_FALSE("not canon",      fs2_is_canon(r));
    CHECK_NOT_NULL("v readable",  r->v);
    CHECK_TRUE ("STATIC shape",   fs2_is_static(r));
    CHECK_FALSE("not ALLOC",      fs2_is_alloc(r));
    CHECK_TRUE ("bodyalloc",      fs2_is_bodyalloc(r));
    CHECK_FS2VALID(1, r);

    fs2_free(&src);
    fs2_free(r);
    assert_balanced();
}

/* =========================================================================
 * fs2_heap_right — пустой src (clone path → нормализация в FS2EMPTY)
 * ========================================================================= */

Test(fs2_api, heap_right_fs2empty_returns_empty) {
    __fs2_checkreset();
    fs2 src = FS2EMPTY;

    fs2 *r = fs2_heap_right(&src, 5);

    CHECK_TRUE ("eq_empty shape", fs2_eq_empty(r));
    CHECK_TRUE ("bodyalloc",      fs2_is_bodyalloc(r));
    CHECK_FS2VALID(1, r);

    fs2_free(r);
    /* src STATIC — нечего освобождать */
    assert_balanced();
}

Test(fs2_api, heap_right_canonical_returns_fs2empty) {
    __fs2_checkreset();
    fs2 src = FS2();

    fs2 *r = fs2_heap_right(&src, 5);

    /* clone нормализует пустое (в т.ч. каноническое) → FS2EMPTY */
    CHECK_TRUE ("eq_empty shape", fs2_eq_empty(r));
    CHECK_FALSE("not canon",      fs2_is_canon(r));
    CHECK_TRUE ("bodyalloc",      fs2_is_bodyalloc(r));
    CHECK_FS2VALID(1, r);

    fs2_free(&src);
    fs2_free(r);
    assert_balanced();
}

/* =========================================================================
 * fs2_heap_right — STATIC src
 * ========================================================================= */

Test(fs2_api, heap_right_from_static_src) {
    __fs2_checkreset();
    fs2 src = FS2LITERAL("hello");

    fs2 *r = fs2_heap_right(&src, 3);

    CHECK_STR_EQ("llo", r->v, "llo");
    CHECK_TRUE ("ALLOC copy (not aliased)", fs2_is_alloc(r));
    CHECK_TRUE ("bodyalloc",                fs2_is_bodyalloc(r));
    CHECK_FALSE("not STATIC",               fs2_is_static(r));
    CHECK_TRUE ("independent buffer",       r->v != src.v);
    CHECK_FS2VALID(1, r);

    fs2_free(r);
    assert_balanced();
}

Test(fs2_api, heap_right_from_static_src_clamp) {
    __fs2_checkreset();
    fs2 src = FS2LITERAL("hi");

    fs2 *r = fs2_heap_right(&src, 100);

    /* clone path — heap copy of literal */
    CHECK_STR_EQ("whole", r->v, "hi");
    CHECK_TRUE ("ALLOC",     fs2_is_alloc(r));
    CHECK_TRUE ("bodyalloc", fs2_is_bodyalloc(r));
    CHECK_FS2VALID(1, r);

    fs2_free(r);
    assert_balanced();
}

/* =========================================================================
 * fs2_heap_right — guards (invfatal)
 * ========================================================================= */

Test(fs2_api, heap_right_null_aborts, .signal = SIGABRT) {
    fs2_heap_right(NULL, 5);
}

/* =========================================================================
 * fs2_heap_right — src immutability / independence
 * ========================================================================= */

Test(fs2_api, heap_right_src_untouched) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("original");
    void *orig_v = src.v;

    fs2 *r = fs2_heap_right(&src, 4);

    CHECK_PTR_EQ("src v unchanged",       src.v, orig_v);
    CHECK_STR_EQ("src content unchanged", src.v, "original");
    CHECK_STR_EQ("result is copy",        r->v,  "inal");
    CHECK_FS2VALID(1, r);
    CHECK_FS2VALID(2, &src);

    fs2_free(&src);
    fs2_free(r);
    assert_balanced();
}

Test(fs2_api, heap_right_independent_buffers) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("abcdef");

    fs2 *r = fs2_heap_right(&src, 3);

    /* мутация результата не должна задевать src */
    r->v[0] = 'X';

    CHECK_STR_EQ("result mutated",     r->v,  "Xef");
    CHECK_STR_EQ("src still original", src.v, "abcdef");
    CHECK_FS2VALID(1, r);
    CHECK_FS2VALID(2, &src);

    fs2_free(&src);
    fs2_free(r);
    assert_balanced();
}

/* =========================================================================
 * Consistency: heap_right(s, n) == right_new(s, n) по содержимому
 * (один и тот же алгоритм: clone / substr_new)
 * ========================================================================= */

Test(fs2_api, heap_right_matches_right_new_content) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("abcdef");

    fs2  stackr = fs2_right_new(&src, 4);
    fs2 *heapr  = fs2_heap_right(&src, 4);

    CHECK_STR_EQ("stack content", stackr.v,   "cdef");
    CHECK_STR_EQ("heap  content", heapr->v,   "cdef");
    CHECK_EQ  ("same len",        stackr.len, heapr->len);
    CHECK_FS2VALID(1, &stackr);
    CHECK_FS2VALID(2, heapr);

    fs2_free(&stackr);
    fs2_free(&src);
    fs2_free(heapr);
    assert_balanced();
}

/* =========================================================================
 * Многократные вызовы — баланс аллокаций на каждой итерации
 * ========================================================================= */

Test(fs2_api, heap_right_multiple_calls_balance) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("0123456789");

    for (size_t k = 1; k <= 5; k++) {
        fs2 *r = fs2_heap_right(&src, k);
        CHECK_FS2VALID(1, r);
        CHECK_EQ("len", r->len, k);
        CHECK_STR_EQ("tail", r->v, &"0123456789"[10 - k]);
        fs2_free(r);
    }

    fs2_free(&src);
    assert_balanced();
}

/* =========================================================================
 * fs2_toupper — nominal
 * ========================================================================= */

Test(fs2_api, toupper_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");
    CHECK_FS2VALID(1, &s);

    fs2 *r = fs2_toupper(&s);

    CHECK_STR_EQ("content", s.v, "HELLO");
    CHECK_EQ  ("len",     s.len, 5);
    CHECK_PTR_EQ("chaining: returns same", r, &s);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, toupper_mixed_content) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("Hello World 123!xyz");

    fs2_toupper(&s);

    CHECK_STR_EQ("only letters upper", s.v, "HELLO WORLD 123!XYZ");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, toupper_already_upper_idempotent) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("HELLO123");

    fs2_toupper(&s);

    CHECK_STR_EQ("unchanged", s.v, "HELLO123");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, toupper_in_place_no_realloc) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");
    void *before = s.v;

    fs2_toupper(&s);

    CHECK_PTR_EQ("buffer not reallocated", s.v, before);
    CHECK_STR_EQ("mutated in place",       s.v, "HELLO");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_toupper — LOCAL (writable, но не ALLOC)
 * ========================================================================= */

Test(fs2_api, toupper_local_works) {
    __fs2_checkreset();
    FS2LOCAL(buf, 16);
    fs2_memcpy_str(&buf, 0, "hello", 5);
    fs2_setlen(&buf, 5);
    CHECK_FS2VALID(1, &buf);

    fs2 *r = fs2_toupper(&buf);

    CHECK_STR_EQ("content", buf.v, "HELLO");
    CHECK_PTR_EQ("chaining", r, &buf);
    CHECK_TRUE ("still LOCAL", fs2_is_local(&buf));
    CHECK_FS2VALID(1, &buf);

    /* LOCAL не аллоцируется, ничего free-ить не нужно */
    assert_balanced();
}

/* =========================================================================
 * fs2_toupper — пустые входы: ВАЛИДНЫ, no-op, форма сохраняется
 * ========================================================================= */

/* Ключевой тест: FS2() остаётся каноническим — toupper НЕ promote'ит
 * FS2() → FS2EMPTY (не вызывает reserve, не аллоцирует буфер). */
Test(fs2_api, toupper_fs2_canon_unchanged) {
    __fs2_checkreset();
    fs2 s = FS2();
    CHECK_TRUE("starts canon", fs2_is_canon(&s));

    fs2 *r = fs2_toupper(&s);

    CHECK_PTR_EQ("returns same",       r, &s);
    CHECK_TRUE ("still canon",         fs2_is_canon(&s));
    CHECK_TRUE ("still empty",         fs2_is_empty(&s));
    CHECK_PTR_EQ("v still NULL",       s.v, NULL);
    CHECK_EQ   ("sz still 0",          s.sz, 0);
    CHECK_EQ   ("len still 0",         s.len, 0);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* FS2EMPTY (STATIC, v="") — no-op, не пытается писать в .rodata */
Test(fs2_api, toupper_fs2empty_unchanged) {
    __fs2_checkreset();
    fs2 s = FS2EMPTY;
    CHECK_TRUE("starts eq_empty", fs2_eq_empty(&s));

    fs2 *r = fs2_toupper(&s);

    CHECK_PTR_EQ("returns same",         r, &s);
    CHECK_TRUE ("still eq_empty",        fs2_eq_empty(&s));
    CHECK_TRUE ("still STATIC",          fs2_is_static(&s));
    CHECK_FALSE("no ALLOC flag added",   fs2_is_alloc(&s));
    CHECK_EQ   ("v still points to \"\"", s.v[0], '\0');
    CHECK_FS2VALID(1, &s);

    /* src STATIC — нечего free-ить */
    assert_balanced();
}

/* =========================================================================
 * fs2_toupper — guards
 * ========================================================================= */

/* Непустой STATIC — попытка записи в .rodata запрещена */
Test(fs2_api, toupper_static_nonempty_aborts, .signal = SIGABRT) {
    fs2 s = FS2LITERAL("hello");
    fs2_toupper(&s);
}

Test(fs2_api, toupper_null_aborts, .signal = SIGABRT) {
    fs2_toupper(NULL);
}

/* =========================================================================
 * fs2_toupper — (unsigned char) cast: байт с установленным старшим битом
 *   toupper(int c) требует EOF или unsigned char.
 *   Без cast на платформах с signed char получаем UB.
 *   Здесь проверяем, что не падает и не портит данные (locale-agnostic).
 * ========================================================================= */

Test(fs2_api, toupper_high_byte_no_ub) {
    __fs2_checkreset();

    /* "A\xE9" — 'A' + байт 0xE9 (é в Latin-1) */
    char *raw = strdup("A\xE9");
    cr_assert(raw != NULL);
    fs2 s = fs2_adopt(&raw);
    cr_assert(raw == NULL);
    CHECK_EQ("precondition len", s.len, 2);

    fs2_toupper(&s);   /* must NOT crash */

    CHECK_EQ("'A' stays 'A'",      s.v[0], 'A');
    CHECK_EQ("len preserved",       s.len, 2);
    CHECK_EQ("high byte non-zero", (unsigned char)s.v[1] != 0, true);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, mutators_empty_inputs_are_noops) {
    __fs2_checkreset();

    /* обе пустые формы — валидные входы для всех in-place мутаторов */
    for (int form = 0; form < 2; form++) {
        fs2 s = (form == 0) ? FS2() : FS2EMPTY;

        fs2_toupper(&s);
        fs2_tolower(&s);
        /* fs2_flip, fs2_trim_str(&s, " "), fs2_strip_prefix_str(&s, "x") — тоже сюда */

        if (form == 0) {
            CHECK_TRUE("FS2() remains canon", fs2_is_canon(&s));
        } else {
            CHECK_TRUE("FS2EMPTY remains empty", fs2_eq_empty(&s));
        }
        CHECK_FS2VALID(1, &s);
        fs2_free(&s);
    }
    assert_balanced();
}

/* =========================================================================
 * fs2_tolower — nominal
 * ========================================================================= */

Test(fs2_api, tolower_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("HELLO");
    CHECK_FS2VALID(1, &s);

    fs2 *r = fs2_tolower(&s);

    CHECK_STR_EQ("content", s.v, "hello");
    CHECK_EQ  ("len",     s.len, 5);
    CHECK_PTR_EQ("chaining: returns same", r, &s);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, tolower_mixed_content) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("HeLLo WoRLD 123!XYZ");

    fs2_tolower(&s);

    CHECK_STR_EQ("only letters lower", s.v, "hello world 123!xyz");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, tolower_already_lower_idempotent) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello123");

    fs2_tolower(&s);

    CHECK_STR_EQ("unchanged", s.v, "hello123");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, tolower_in_place_no_realloc) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("HELLO");
    void *before = s.v;

    fs2_tolower(&s);

    CHECK_PTR_EQ("buffer not reallocated", s.v, before);
    CHECK_STR_EQ("mutated in place",       s.v, "hello");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* round-trip: toupper ∘ tolower == identity на ASCII-буквах */
Test(fs2_api, tolower_toupper_roundtrip) {
    __fs2_checkreset();
    fs2 orig = fs2_fromstr("MiXeD CaSe 42");
    fs2 s    = fs2_clone(&orig);

    fs2_toupper(&s);
    fs2_tolower(&s);

    CHECK_STR_EQ("round-trip on letters", s.v, "mixed case 42");
    CHECK_FS2VALID(1, &s);

    fs2_free(&orig);
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_tolower — LOCAL
 * ========================================================================= */

Test(fs2_api, tolower_local_works) {
    __fs2_checkreset();
    FS2LOCAL(buf, 16);
    fs2_memcpy_str(&buf, 0, "HELLO", 5);
    fs2_setlen(&buf, 5);
    CHECK_FS2VALID(1, &buf);

    fs2 *r = fs2_tolower(&buf);

    CHECK_STR_EQ("content", buf.v, "hello");
    CHECK_PTR_EQ("chaining", r, &buf);
    CHECK_TRUE ("still LOCAL", fs2_is_local(&buf));
    CHECK_FS2VALID(1, &buf);

    assert_balanced();
}

/* =========================================================================
 * fs2_tolower — пустые входы: ВАЛИДНЫ, no-op, форма сохраняется
 * ========================================================================= */

Test(fs2_api, tolower_fs2_canon_unchanged) {
    __fs2_checkreset();
    fs2 s = FS2();
    CHECK_TRUE("starts canon", fs2_is_canon(&s));

    fs2 *r = fs2_tolower(&s);

    CHECK_PTR_EQ("returns same", r, &s);
    CHECK_TRUE ("still canon",   fs2_is_canon(&s));
    CHECK_TRUE ("still empty",   fs2_is_empty(&s));
    CHECK_PTR_EQ("v still NULL", s.v, NULL);
    CHECK_EQ   ("sz still 0",    s.sz, 0);
    CHECK_EQ   ("len still 0",   s.len, 0);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, tolower_fs2empty_unchanged) {
    __fs2_checkreset();
    fs2 s = FS2EMPTY;
    CHECK_TRUE("starts eq_empty", fs2_eq_empty(&s));

    fs2 *r = fs2_tolower(&s);

    CHECK_PTR_EQ("returns same",          r, &s);
    CHECK_TRUE ("still eq_empty",         fs2_eq_empty(&s));
    CHECK_TRUE ("still STATIC",           fs2_is_static(&s));
    CHECK_FALSE("no ALLOC flag added",    fs2_is_alloc(&s));
    CHECK_EQ   ("v still points to \"\"", s.v[0], '\0');
    CHECK_FS2VALID(1, &s);

    assert_balanced();
}

/* =========================================================================
 * fs2_tolower — guards
 * ========================================================================= */

Test(fs2_api, tolower_static_nonempty_aborts, .signal = SIGABRT) {
    fs2 s = FS2LITERAL("HELLO");
    fs2_tolower(&s);
}

Test(fs2_api, tolower_null_aborts, .signal = SIGABRT) {
    fs2_tolower(NULL);
}

/* =========================================================================
 * fs2_tolower — (unsigned char) cast: не падает на байте с MSB
 * ========================================================================= */

Test(fs2_api, tolower_high_byte_no_ub) {
    __fs2_checkreset();

    /* "A\xE9" — 'A' + байт 0xE9 (é в Latin-1) */
    char *raw = strdup("A\xE9");
    cr_assert(raw != NULL);
    fs2 s = fs2_adopt(&raw);
    cr_assert(raw == NULL);
    CHECK_EQ("precondition len", s.len, 2);

    fs2_tolower(&s);   /* must NOT crash */

    CHECK_EQ("'A' → 'a'",          s.v[0], 'a');
    CHECK_EQ("len preserved",      s.len, 2);
    CHECK_EQ("high byte non-zero", (unsigned char)s.v[1] != 0, true);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_rpad — nominal
 * ========================================================================= */

Test(fs2_api, rpad_basic) {
    __fs2_checkreset();
    fs2 str = fs2_fromstr("ab");
    fs2 pad = fs2_fromstr("xy");

    fs2 *r = fs2_rpad(&str, 8, &pad);

    CHECK_STR_EQ("abxyxyxy", str.v, "abxyxyxy");
    CHECK_EQ  ("len", str.len, 8);
    CHECK_PTR_EQ("chaining: same ptr", r, &str);
    CHECK_FS2VALID(1, &str);
    CHECK_FS2VALID(2, &pad);

    fs2_free(&str);
    fs2_free(&pad);
    assert_balanced();
}

Test(fs2_api, rpad_single_char_pad) {
    __fs2_checkreset();
    fs2 str = fs2_fromstr("ab");
    fs2 pad = fs2_fromstr("x");

    fs2_rpad(&str, 5, &pad);

    CHECK_STR_EQ("abxxx", str.v, "abxxx");
    CHECK_FS2VALID(1, &str);

    fs2_free(&str);
    fs2_free(&pad);
    assert_balanced();
}

Test(fs2_api, rpad_pad_longer_than_gap) {
    __fs2_checkreset();
    fs2 str = fs2_fromstr("ab");
    fs2 pad = fs2_fromstr("xyz");

    fs2_rpad(&str, 4, &pad);    /* gap = 2, pad "xyz" → "xy" */

    CHECK_STR_EQ("abxy", str.v, "abxy");
    CHECK_FS2VALID(1, &str);

    fs2_free(&str);
    fs2_free(&pad);
    assert_balanced();
}

/* =========================================================================
 * fs2_rpad — truncate / no-op
 * ========================================================================= */

Test(fs2_api, rpad_truncate_when_len_smaller) {
    __fs2_checkreset();
    fs2 str = fs2_fromstr("hello");
    fs2 pad = fs2_fromstr("xy");

    fs2_rpad(&str, 3, &pad);

    CHECK_STR_EQ("hel", str.v, "hel");
    CHECK_EQ  ("len", str.len, 3);
    CHECK_FS2VALID(1, &str);

    fs2_free(&str);
    fs2_free(&pad);
    assert_balanced();
}

Test(fs2_api, rpad_same_len_noop) {
    __fs2_checkreset();
    fs2 str = fs2_fromstr("hello");
    fs2 pad = fs2_fromstr("xy");
    void *before = str.v;

    fs2_rpad(&str, 5, &pad);

    CHECK_PTR_EQ("buffer unchanged", str.v, before);
    CHECK_STR_EQ("hello",            str.v, "hello");
    CHECK_EQ  ("len",                str.len, 5);
    CHECK_FS2VALID(1, &str);

    fs2_free(&str);
    fs2_free(&pad);
    assert_balanced();
}

/* =========================================================================
 * fs2_rpad — empty pad: no-op (твоя семантика)
 * ========================================================================= */

Test(fs2_api, rpad_empty_canon_pad_noop) {
    __fs2_checkreset();
    fs2 str = fs2_fromstr("hello");
    fs2 pad = FS2();

    fs2 *r = fs2_rpad(&str, 10, &pad);

    CHECK_PTR_EQ("chaining", r, &str);
    CHECK_STR_EQ("unchanged", str.v, "hello");
    CHECK_EQ  ("len unchanged", str.len, 5);
    CHECK_FS2VALID(1, &str);
    CHECK_FS2VALID(2, &pad);

    fs2_free(&str);
    fs2_free(&pad);
    assert_balanced();
}

Test(fs2_api, rpad_fs2empty_pad_noop) {
    __fs2_checkreset();
    fs2 str = fs2_fromstr("hello");
    fs2 pad = FS2EMPTY;

    fs2_rpad(&str, 10, &pad);

    CHECK_STR_EQ("unchanged", str.v, "hello");
    CHECK_FS2VALID(1, &str);

    fs2_free(&str);
    assert_balanced();
}

Test(fs2_api, rpad_empty_str_empty_pad_stays_empty) {
    __fs2_checkreset();
    fs2 str = FS2();
    fs2 pad = FS2EMPTY;

    fs2_rpad(&str, 10, &pad);

    CHECK_TRUE("still canon", fs2_is_canon(&str));
    CHECK_FS2VALID(1, &str);

    fs2_free(&str);
    assert_balanced();
}

/* =========================================================================
 * fs2_rpad — guards
 * ========================================================================= */

Test(fs2_api, rpad_null_str_aborts, .signal = SIGABRT) {
    fs2 pad = fs2_fromstr("xy");
    fs2_rpad(NULL, 10, &pad);
    fs2_free(&pad);
}

Test(fs2_api, rpad_null_pad_aborts, .signal = SIGABRT) {
    fs2 str = fs2_fromstr("ab");
    fs2_rpad(&str, 10, NULL);
    fs2_free(&str);
}

Test(fs2_api, rpad_static_nonempty_str_aborts, .signal = SIGABRT) {
    fs2 str = FS2LITERAL("hello");
    fs2 pad = fs2_fromstr("xy");
    fs2_rpad(&str, 10, &pad);
    fs2_free(&pad);
}

/* =========================================================================
 * fs2_rpad — LOCAL & pad immutability
 * ========================================================================= */

Test(fs2_api, rpad_local_str_works) {
    __fs2_checkreset();
    FS2LOCAL(buf, 16);
    fs2_memcpy_str(&buf, 0, "ab", 2);
    fs2_setlen(&buf, 2);
    fs2 pad = fs2_fromstr("xy");

    fs2_rpad(&buf, 8, &pad);

    CHECK_STR_EQ("abxyxyxy", buf.v, "abxyxyxy");
    CHECK_TRUE("still LOCAL", fs2_is_local(&buf));
    CHECK_FS2VALID(1, &buf);

    fs2_free(&pad);
    assert_balanced();
}

Test(fs2_api, rpad_pad_untouched) {
    __fs2_checkreset();
    fs2 str = fs2_fromstr("ab");
    fs2 pad = fs2_fromstr("xy");
    void *pad_v = pad.v;

    fs2_rpad(&str, 8, &pad);

    CHECK_PTR_EQ("pad v unchanged", pad.v, pad_v);
    CHECK_STR_EQ("pad content unchanged", pad.v, "xy");
    CHECK_EQ  ("pad len unchanged", pad.len, 2);
    CHECK_FS2VALID(1, &pad);

    fs2_free(&str);
    fs2_free(&pad);
    assert_balanced();
}

/* =========================================================================
 * fs2_rpad — LOCAL с недостаточной ёмкостью → abort в reserve
 *   (LOCAL проходит fs2_cant_realloc, но fs2_prepare_growth в reserve
 *    внутри падает, если sz < need)
 * ========================================================================= */

Test(fs2_api, rpad_local_insufficient_capacity_aborts, .signal = SIGABRT) {
    __fs2_checkreset();
    FS2LOCAL(buf, 4);                    /* sz = 4, вмещает только "abc\0" */
    fs2_memcpy_str(&buf, 0, "abc", 3);
    fs2_setlen(&buf, 3);
    fs2 pad = fs2_fromstr("x");

    fs2_rpad(&buf, 10, &pad);            /* need = 11 > sz = 4 → abort */
    fs2_free(&pad);
}

/* LOCAL с достаточной ёмкостью — работает, остаётся LOCAL */
Test(fs2_api, rpad_local_sufficient_capacity_works) {
    __fs2_checkreset();
    FS2LOCAL(buf, 16);
    fs2_memcpy_str(&buf, 0, "ab", 2);
    fs2_setlen(&buf, 2);
    fs2 pad = fs2_fromstr("xy");

    fs2_rpad(&buf, 8, &pad);

    CHECK_STR_EQ("abxyxyxy", buf.v, "abxyxyxy");
    CHECK_TRUE("still LOCAL", fs2_is_local(&buf));
    CHECK_TRUE("not ALLOC",  !fs2_is_alloc(&buf));
    CHECK_FS2VALID(1, &buf);

    fs2_free(&pad);
    assert_balanced();
}

/* =========================================================================
 * fs2_rpad — promotion empty → ALLOC (семантика теперь верная)
 * ========================================================================= */

Test(fs2_api, rpad_empty_canon_str_with_nonempty_pad) {
    __fs2_checkreset();
    fs2 str = FS2();               /* canonical empty */
    fs2 pad = fs2_fromstr("ab");

    fs2_rpad(&str, 5, &pad);

    CHECK_STR_EQ("ababa", str.v, "ababa");
    CHECK_EQ  ("len", str.len, 5);
    CHECK_FALSE("no longer canon",  fs2_is_canon(&str));
    CHECK_TRUE ("now ALLOC",        fs2_is_alloc(&str));
    CHECK_FS2VALID(1, &str);
    CHECK_FS2VALID(2, &pad);

    fs2_free(&str);
    fs2_free(&pad);
    assert_balanced();
}

Test(fs2_api, rpad_fs2empty_str_with_nonempty_pad) {
    __fs2_checkreset();
    fs2 str = FS2EMPTY;
    fs2 pad = fs2_fromstr("ab");

    fs2_rpad(&str, 5, &pad);

    CHECK_STR_EQ("ababa", str.v, "ababa");
    CHECK_FALSE("not STATIC anymore", fs2_is_static(&str));
    CHECK_TRUE ("now ALLOC",          fs2_is_alloc(&str));
    CHECK_FS2VALID(1, &str);

    fs2_free(&str);
    fs2_free(&pad);
    assert_balanced();
}

/* =========================================================================
 * Регрессия: fs2_repeat_new через rpad_new теперь работает
 * ========================================================================= */

Test(fs2_api, repeat_new_via_rpad_works) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("ab");

    fs2 r = fs2_repeat_new(&s, 4);

    CHECK_STR_EQ("abababab", r.v, "abababab");
    CHECK_EQ  ("len", r.len, 8);
    CHECK_FS2VALID(1, &r);

    fs2_free(&s);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api, repeat_new_works_via_rpad) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("ab");

    fs2 r = fs2_repeat_new(&s, 4);

    CHECK_STR_EQ("abababab", r.v, "abababab");
    CHECK_EQ  ("len", r.len, 8);
    CHECK_FS2VALID(1, &r);

    fs2_free(&s);
    fs2_free(&r);
    assert_balanced();
}

/* =========================================================================
 * fs2_rpadstr — nominal
 * ========================================================================= */

Test(fs2_api, rpadstr_basic) {
    __fs2_checkreset();
    fs2 str = fs2_fromstr("ab");

    fs2 *r = fs2_rpadstr(&str, 6, "xy");

    CHECK_STR_EQ("abxyxy", str.v, "abxyxy");
    CHECK_EQ  ("len",      str.len, 6);
    CHECK_PTR_EQ("chaining", r, &str);
    CHECK_FS2VALID(1, &str);

    fs2_free(&str);
    assert_balanced();
}

Test(fs2_api, rpadstr_single_char_pad) {
    __fs2_checkreset();
    fs2 str = fs2_fromstr("ab");

    fs2_rpadstr(&str, 5, "x");

    CHECK_STR_EQ("abxxx", str.v, "abxxx");
    CHECK_FS2VALID(1, &str);

    fs2_free(&str);
    assert_balanced();
}

Test(fs2_api, rpadstr_truncate_when_len_smaller) {
    __fs2_checkreset();
    fs2 str = fs2_fromstr("hello");

    fs2_rpadstr(&str, 3, "xy");

    CHECK_STR_EQ("hel", str.v, "hel");
    CHECK_FS2VALID(1, &str);

    fs2_free(&str);
    assert_balanced();
}

Test(fs2_api, rpadstr_same_len_noop) {
    __fs2_checkreset();
    fs2 str = fs2_fromstr("hello");
    void *before = str.v;

    fs2_rpadstr(&str, 5, "xy");

    CHECK_PTR_EQ("buffer unchanged", str.v, before);
    CHECK_STR_EQ("hello",            str.v, "hello");
    CHECK_FS2VALID(1, &str);

    fs2_free(&str);
    assert_balanced();
}

Test(fs2_api, rpadstr_empty_string_noop) {
    __fs2_checkreset();
    fs2 str = fs2_fromstr("ab");
    void *before = str.v;

    fs2_rpadstr(&str, 10, "");

    CHECK_PTR_EQ("buffer unchanged", str.v, before);
    CHECK_STR_EQ("ab",               str.v, "ab");
    CHECK_EQ  ("len",                str.len, 2);
    CHECK_FS2VALID(1, &str);

    fs2_free(&str);
    assert_balanced();
}

Test(fs2_api, rpadstr_empty_str_promotes) {
    __fs2_checkreset();
    fs2 str = FS2();

    fs2_rpadstr(&str, 5, "ab");

    CHECK_STR_EQ("ababa",         str.v, "ababa");
    CHECK_FALSE("no longer canon", fs2_is_canon(&str));
    CHECK_TRUE ("now ALLOC",       fs2_is_alloc(&str));
    CHECK_FS2VALID(1, &str);

    fs2_free(&str);
    assert_balanced();
}

Test(fs2_api, rpadstr_fs2empty_str_promotes) {
    __fs2_checkreset();
    fs2 str = FS2EMPTY;

    fs2_rpadstr(&str, 5, "ab");

    CHECK_STR_EQ("ababa",            str.v, "ababa");
    CHECK_FALSE("not STATIC anymore", fs2_is_static(&str));
    CHECK_FS2VALID(1, &str);

    fs2_free(&str);
    assert_balanced();
}

/* guards */
Test(fs2_api, rpadstr_null_str_aborts, .signal = SIGABRT) {
    fs2_rpadstr(NULL, 10, "xy");
}

Test(fs2_api, rpadstr_null_padstr_aborts, .signal = SIGABRT) {
    fs2 str = fs2_fromstr("ab");
    fs2_rpadstr(&str, 10, NULL);
    fs2_free(&str);
}

Test(fs2_api, rpadstr_static_nonempty_str_aborts, .signal = SIGABRT) {
    fs2 str = FS2LITERAL("hello");
    fs2_rpadstr(&str, 10, "xy");
}

/* LOCAL */
Test(fs2_api, rpadstr_local_sufficient_capacity_works) {
    __fs2_checkreset();
    FS2LOCAL(buf, 16);
    fs2_memcpy_str(&buf, 0, "ab", 2);
    fs2_setlen(&buf, 2);

    fs2_rpadstr(&buf, 8, "xy");

    CHECK_STR_EQ("abxyxyxy",  buf.v, "abxyxyxy");
    CHECK_TRUE ("still LOCAL", fs2_is_local(&buf));
    CHECK_FS2VALID(1, &buf);

    assert_balanced();
}

Test(fs2_api, rpadstr_local_insufficient_capacity_aborts, .signal = SIGABRT) {
    FS2LOCAL(buf, 4);
    fs2_memcpy_str(&buf, 0, "abc", 3);
    fs2_setlen(&buf, 3);
    fs2_rpadstr(&buf, 10, "x");
}

/* =========================================================================
 * fs2_rpad_new — nominal (padding path)
 * ========================================================================= */

Test(fs2_api, rpad_new_basic) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("ab");
    fs2 pad = fs2_fromstr("xy");

    fs2 r = fs2_rpad_new(&src, 8, &pad);

    CHECK_STR_EQ("abxyxyxy", r.v, "abxyxyxy");
    CHECK_EQ  ("len",        r.len, 8);
    CHECK_TRUE("ALLOC",           fs2_is_alloc(&r));
    CHECK_FALSE("not bodyalloc",  fs2_is_bodyalloc(&r));
    CHECK_FS2VALID(1, &r);
    CHECK_FS2VALID(2, &src);
    CHECK_FS2VALID(3, &pad);

    fs2_free(&src);
    fs2_free(&pad);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api, rpad_new_single_char_pad) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("ab");
    fs2 pad = fs2_fromstr("x");

    fs2 r = fs2_rpad_new(&src, 5, &pad);

    CHECK_STR_EQ("abxxx", r.v, "abxxx");
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    fs2_free(&pad);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api, rpad_new_gap_not_multiple_of_pad) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("a");
    fs2 pad = fs2_fromstr("xyz");

    fs2 r = fs2_rpad_new(&src, 8, &pad);   /* gap = 7 → "xyzxyzx" */

    CHECK_STR_EQ("axyzxyzx", r.v, "axyzxyzx");
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    fs2_free(&pad);
    fs2_free(&r);
    assert_balanced();
}

/* =========================================================================
 * fs2_rpad_new — truncate / len == s->len
 * ========================================================================= */

Test(fs2_api, rpad_new_truncate_when_len_smaller) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello");
    fs2 pad = fs2_fromstr("xy");

    fs2 r = fs2_rpad_new(&src, 3, &pad);

    CHECK_STR_EQ("hel", r.v, "hel");
    CHECK_EQ  ("len",   r.len, 3);
    CHECK_FS2VALID(1, &r);
    CHECK_FS2VALID(2, &src);

    fs2_free(&src);
    fs2_free(&pad);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api, rpad_new_len_eq_slen_clone_whole) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello");
    fs2 pad = fs2_fromstr("xy");

    fs2 r = fs2_rpad_new(&src, 5, &pad);

    CHECK_STR_EQ("hello",     r.v, "hello");
    CHECK_EQ  ("len",         r.len, 5);
    CHECK_TRUE("independent", r.v != src.v);
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    fs2_free(&pad);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api, rpad_new_len_greater_than_slen) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("abc");
    fs2 pad = fs2_fromstr("x");

    fs2 r = fs2_rpad_new(&src, 10, &pad);

    CHECK_STR_EQ("abcxxxxxxx", r.v, "abcxxxxxxx");
    CHECK_EQ  ("len",          r.len, 10);
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    fs2_free(&pad);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api, rpad_new_len_zero_truncates_to_empty) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello");
    fs2 pad = fs2_fromstr("xy");

    fs2 r = fs2_rpad_new(&src, 0, &pad);

    CHECK_EQ  ("len",          r.len, 0);
    CHECK_TRUE("empty",        fs2_is_empty(&r));
    CHECK_TRUE("ALLOC shape",  fs2_is_alloc(&r));
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    fs2_free(&pad);
    fs2_free(&r);
    assert_balanced();
}

/* =========================================================================
 * fs2_rpad_new — empty source (clone → FS2EMPTY → promote via rpad)
 * ========================================================================= */

Test(fs2_api, rpad_new_canon_src_with_nonempty_pad) {
    __fs2_checkreset();
    fs2 src = FS2();
    fs2 pad = fs2_fromstr("ab");

    fs2 r = fs2_rpad_new(&src, 5, &pad);

    CHECK_STR_EQ("ababa", r.v, "ababa");
    CHECK_EQ  ("len",     r.len, 5);
    CHECK_TRUE("ALLOC",   fs2_is_alloc(&r));
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    fs2_free(&pad);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api, rpad_new_fs2empty_src_with_nonempty_pad) {
    __fs2_checkreset();
    fs2 src = FS2EMPTY;
    fs2 pad = fs2_fromstr("ab");

    fs2 r = fs2_rpad_new(&src, 5, &pad);

    CHECK_STR_EQ("ababa", r.v, "ababa");
    CHECK_FS2VALID(1, &r);

    fs2_free(&pad);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api, rpad_new_empty_src_empty_pad_stays_empty) {
    __fs2_checkreset();
    fs2 src = FS2();
    fs2 pad = FS2EMPTY;

    fs2 r = fs2_rpad_new(&src, 5, &pad);

    CHECK_TRUE("eq_empty", fs2_eq_empty(&r));
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api, rpad_new_empty_src_len_zero) {
    __fs2_checkreset();
    fs2 src = FS2EMPTY;
    fs2 pad = fs2_fromstr("xy");

    fs2 r = fs2_rpad_new(&src, 0, &pad);

    CHECK_TRUE("eq_empty", fs2_eq_empty(&r));
    CHECK_FS2VALID(1, &r);

    fs2_free(&pad);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api, rpad_new_nonempty_src_empty_pad_clone_unchanged) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello");
    fs2 pad = FS2();

    fs2 r = fs2_rpad_new(&src, 10, &pad);

    CHECK_STR_EQ("hello", r.v, "hello");
    CHECK_EQ  ("len",     r.len, 5);
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    fs2_free(&pad);
    fs2_free(&r);
    assert_balanced();
}

/* =========================================================================
 * fs2_rpad_new — STATIC / LOCAL source
 * ========================================================================= */

Test(fs2_api, rpad_new_static_src) {
    __fs2_checkreset();
    fs2 src = FS2LITERAL("hi");
    fs2 pad = fs2_fromstr("xy");

    fs2 r = fs2_rpad_new(&src, 6, &pad);

    CHECK_STR_EQ("hixyxy",    r.v, "hixyxy");
    CHECK_TRUE ("ALLOC",      fs2_is_alloc(&r));
    CHECK_TRUE ("independent", r.v != src.v);
    CHECK_FS2VALID(1, &r);

    fs2_free(&pad);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api, rpad_new_static_src_empty_pad) {
    __fs2_checkreset();
    fs2 src = FS2LITERAL("hi");
    fs2 pad = FS2EMPTY;

    fs2 r = fs2_rpad_new(&src, 10, &pad);

    CHECK_STR_EQ("hi",    r.v, "hi");
    CHECK_TRUE ("ALLOC",  fs2_is_alloc(&r));
    CHECK_FS2VALID(1, &r);

    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api, rpad_new_local_src) {
    __fs2_checkreset();
    FS2LOCAL(buf, 8);
    fs2_memcpy_str(&buf, 0, "ab", 2);
    fs2_setlen(&buf, 2);
    fs2 pad = fs2_fromstr("x");

    fs2 r = fs2_rpad_new(&buf, 5, &pad);

    CHECK_STR_EQ("abxxx",           r.v,   "abxxx");
    CHECK_TRUE ("result ALLOC",     fs2_is_alloc(&r));
    CHECK_TRUE ("result not LOCAL", !fs2_is_local(&r));
    CHECK_STR_EQ("buf untouched",   buf.v, "ab");
    CHECK_FS2VALID(1, &r);
    CHECK_FS2VALID(2, &buf);

    fs2_free(&pad);
    fs2_free(&r);
    assert_balanced();
}

/* =========================================================================
 * fs2_rpad_new — guards
 * ========================================================================= */

Test(fs2_api, rpad_new_null_str_aborts, .signal = SIGABRT) {
    fs2 pad = fs2_fromstr("xy");
    fs2_rpad_new(NULL, 5, &pad);
    fs2_free(&pad);
}

Test(fs2_api, rpad_new_null_pad_aborts, .signal = SIGABRT) {
    fs2 src = fs2_fromstr("ab");
    fs2_rpad_new(&src, 5, NULL);
    fs2_free(&src);
}

/* =========================================================================
 * fs2_rpad_new — immutability / independence
 * ========================================================================= */

Test(fs2_api, rpad_new_src_untouched) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("original");
    fs2 pad = fs2_fromstr("_");
    void *orig_v = src.v;
    void *pad_v  = pad.v;

    fs2 r = fs2_rpad_new(&src, 12, &pad);

    CHECK_PTR_EQ("src v unchanged", src.v, orig_v);
    CHECK_STR_EQ("src content",     src.v, "original");
    CHECK_PTR_EQ("pad v unchanged", pad.v, pad_v);
    CHECK_STR_EQ("pad content",     pad.v, "_");
    CHECK_STR_EQ("result",          r.v,   "original____");
    CHECK_FS2VALID(1, &r);
    CHECK_FS2VALID(2, &src);
    CHECK_FS2VALID(3, &pad);

    fs2_free(&src);
    fs2_free(&pad);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api, rpad_new_independent_buffer) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("ab");
    fs2 pad = fs2_fromstr("xy");

    fs2 r = fs2_rpad_new(&src, 6, &pad);

    r.v[0] = 'X';

    CHECK_STR_EQ("result mutated", r.v,   "Xbxyxy");
    CHECK_STR_EQ("src original",   src.v, "ab");
    CHECK_FS2VALID(1, &r);
    CHECK_FS2VALID(2, &src);

    fs2_free(&src);
    fs2_free(&pad);
    fs2_free(&r);
    assert_balanced();
}

/* =========================================================================
 * Consistency: rpad_new(s, n, p) == clone(s) + rpad(clone, n, p)
 * ========================================================================= */

Test(fs2_api, rpad_new_matches_clone_then_rpad) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("ab");
    fs2 pad = fs2_fromstr("xy");

    fs2 manual = fs2_clone(&src);
    fs2_rpad(&manual, 8, &pad);

    fs2 via = fs2_rpad_new(&src, 8, &pad);

    CHECK_STR_EQ("same content", manual.v,   via.v);
    CHECK_EQ  ("same len",       manual.len, via.len);
    CHECK_FS2VALID(1, &manual);
    CHECK_FS2VALID(2, &via);

    fs2_free(&src);
    fs2_free(&pad);
    fs2_free(&manual);
    fs2_free(&via);
    assert_balanced();
}

/* =========================================================================
 * fs2_rpadstr_new — nominal (padding path)
 * ========================================================================= */

Test(fs2_api, rpadstr_new_basic) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("ab");

    fs2 r = fs2_rpadstr_new(&src, 8, "xy");

    CHECK_STR_EQ("abxyxyxy", r.v, "abxyxyxy");
    CHECK_EQ  ("len",        r.len, 8);
    CHECK_TRUE("ALLOC",           fs2_is_alloc(&r));
    CHECK_FALSE("not bodyalloc",  fs2_is_bodyalloc(&r));
    CHECK_FS2VALID(1, &r);
    CHECK_FS2VALID(2, &src);

    fs2_free(&src);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api, rpadstr_new_single_char_pad) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("ab");

    fs2 r = fs2_rpadstr_new(&src, 5, "x");

    CHECK_STR_EQ("abxxx", r.v, "abxxx");
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api, rpadstr_new_gap_not_multiple_of_pad) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("a");

    fs2 r = fs2_rpadstr_new(&src, 8, "xyz");   /* gap = 7 → "xyzxyzx" */

    CHECK_STR_EQ("axyzxyzx", r.v, "axyzxyzx");
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api, rpadstr_new_pad_longer_than_gap) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("ab");

    fs2 r = fs2_rpadstr_new(&src, 4, "xyz");   /* gap = 2 → "xy" */

    CHECK_STR_EQ("abxy", r.v, "abxy");
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    fs2_free(&r);
    assert_balanced();
}

/* =========================================================================
 * fs2_rpadstr_new — truncate / len == s->len / len == 0
 * ========================================================================= */

Test(fs2_api, rpadstr_new_truncate_when_len_smaller) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello");

    fs2 r = fs2_rpadstr_new(&src, 3, "xy");

    CHECK_STR_EQ("hel", r.v, "hel");
    CHECK_EQ  ("len",   r.len, 3);
    CHECK_FS2VALID(1, &r);
    CHECK_FS2VALID(2, &src);

    fs2_free(&src);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api, rpadstr_new_len_eq_slen_clones_whole) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello");

    fs2 r = fs2_rpadstr_new(&src, 5, "xy");

    CHECK_STR_EQ("hello",     r.v, "hello");
    CHECK_EQ  ("len",         r.len, 5);
    CHECK_TRUE("independent", r.v != src.v);
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api, rpadstr_new_len_zero_empty) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello");

    fs2 r = fs2_rpadstr_new(&src, 0, "xy");

    CHECK_EQ  ("len",   r.len, 0);
    CHECK_TRUE("empty", fs2_is_empty(&r));
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    fs2_free(&r);
    assert_balanced();
}

/* =========================================================================
 * fs2_rpadstr_new — empty padstr ("") → no-op (clone of src)
 * ========================================================================= */

Test(fs2_api, rpadstr_new_empty_padstr_noop) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello");

    fs2 r = fs2_rpadstr_new(&src, 10, "");

    CHECK_STR_EQ("hello",   r.v, "hello");
    CHECK_EQ  ("len",       r.len, 5);
    CHECK_TRUE("independent", r.v != src.v);
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    fs2_free(&r);
    assert_balanced();
}

/* =========================================================================
 * fs2_rpadstr_new — empty src (clone → FS2EMPTY → promote via rpad)
 * ========================================================================= */

Test(fs2_api, rpadstr_new_canon_src_with_nonempty_pad) {
    __fs2_checkreset();
    fs2 src = FS2();

    fs2 r = fs2_rpadstr_new(&src, 5, "ab");

    CHECK_STR_EQ("ababa", r.v, "ababa");
    CHECK_EQ  ("len",     r.len, 5);
    CHECK_TRUE("ALLOC",   fs2_is_alloc(&r));
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api, rpadstr_new_fs2empty_src_with_nonempty_pad) {
    __fs2_checkreset();
    fs2 src = FS2EMPTY;

    fs2 r = fs2_rpadstr_new(&src, 5, "ab");

    CHECK_STR_EQ("ababa", r.v, "ababa");
    CHECK_FS2VALID(1, &r);

    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api, rpadstr_new_empty_src_empty_pad_stays_empty) {
    __fs2_checkreset();
    fs2 src = FS2();

    fs2 r = fs2_rpadstr_new(&src, 5, "");

    CHECK_TRUE("eq_empty", fs2_eq_empty(&r));
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    fs2_free(&r);
    assert_balanced();
}

/* =========================================================================
 * fs2_rpadstr_new — STATIC / LOCAL source
 * ========================================================================= */

Test(fs2_api, rpadstr_new_static_src) {
    __fs2_checkreset();
    fs2 src = FS2LITERAL("hi");

    fs2 r = fs2_rpadstr_new(&src, 6, "xy");

    CHECK_STR_EQ("hixyxy",    r.v, "hixyxy");
    CHECK_TRUE ("ALLOC",      fs2_is_alloc(&r));
    CHECK_TRUE ("independent", r.v != src.v);
    CHECK_FS2VALID(1, &r);

    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api, rpadstr_new_local_src) {
    __fs2_checkreset();
    FS2LOCAL(buf, 8);
    fs2_memcpy_str(&buf, 0, "ab", 2);
    fs2_setlen(&buf, 2);

    fs2 r = fs2_rpadstr_new(&buf, 5, "x");

    CHECK_STR_EQ("abxxx",           r.v,   "abxxx");
    CHECK_TRUE ("result ALLOC",     fs2_is_alloc(&r));
    CHECK_TRUE ("result not LOCAL", !fs2_is_local(&r));
    CHECK_STR_EQ("buf untouched",   buf.v, "ab");
    CHECK_FS2VALID(1, &r);
    CHECK_FS2VALID(2, &buf);

    fs2_free(&r);
    assert_balanced();
}

/* =========================================================================
 * fs2_rpadstr_new — guards (SIGABRT)
 * ========================================================================= */

Test(fs2_api, rpadstr_new_null_str_aborts, .signal = SIGABRT) {
    fs2_rpadstr_new(NULL, 5, "xy");
}

Test(fs2_api, rpadstr_new_null_padstr_aborts, .signal = SIGABRT) {
    fs2 src = fs2_fromstr("ab");
    fs2_rpadstr_new(&src, 5, NULL);
    fs2_free(&src);
}

/* =========================================================================
 * fs2_rpadstr_new — immutability / independence
 * ========================================================================= */

Test(fs2_api, rpadstr_new_src_untouched) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("original");
    void *orig_v = src.v;

    fs2 r = fs2_rpadstr_new(&src, 12, "_");

    CHECK_PTR_EQ("src v unchanged", src.v, orig_v);
    CHECK_STR_EQ("src content",     src.v, "original");
    CHECK_STR_EQ("result",          r.v,   "original____");
    CHECK_FS2VALID(1, &r);
    CHECK_FS2VALID(2, &src);

    fs2_free(&src);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api, rpadstr_new_independent_buffer) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("ab");

    fs2 r = fs2_rpadstr_new(&src, 6, "xy");

    r.v[0] = 'X';

    CHECK_STR_EQ("result mutated", r.v,   "Xbxyxy");
    CHECK_STR_EQ("src original",   src.v, "ab");
    CHECK_FS2VALID(1, &r);
    CHECK_FS2VALID(2, &src);

    fs2_free(&src);
    fs2_free(&r);
    assert_balanced();
}

/* =========================================================================
 * Consistency: rpadstr_new(s, n, p) == rpadstr(clone(s), n, p)
 * ========================================================================= */

Test(fs2_api, rpadstr_new_matches_clone_then_rpadstr) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("ab");

    fs2 manual = fs2_clone(&src);
    fs2_rpadstr(&manual, 8, "xy");

    fs2 via = fs2_rpadstr_new(&src, 8, "xy");

    CHECK_STR_EQ("same content", manual.v,   via.v);
    CHECK_EQ  ("same len",       manual.len, via.len);
    CHECK_FS2VALID(1, &manual);
    CHECK_FS2VALID(2, &via);

    fs2_free(&src);
    fs2_free(&manual);
    fs2_free(&via);
    assert_balanced();
}

/* =========================================================================
 * fs2_lpad — nominal (padding path)
 * ========================================================================= */

Test(fs2_api, lpad_basic) {
    __fs2_checkreset();
    fs2 str = fs2_fromstr("ab");
    fs2 pad = fs2_fromstr("xy");

    fs2 *r = fs2_lpad(&str, 8, &pad);

    CHECK_STR_EQ("xyxyxyab", str.v, "xyxyxyab");
    CHECK_EQ  ("len",        str.len, 8);
    CHECK_PTR_EQ("chaining", r, &str);
    CHECK_FS2VALID(1, &str);
    CHECK_FS2VALID(2, &pad);

    fs2_free(&str);
    fs2_free(&pad);
    assert_balanced();
}

Test(fs2_api, lpad_single_char_pad) {
    __fs2_checkreset();
    fs2 str = fs2_fromstr("ab");
    fs2 pad = fs2_fromstr("x");

    fs2_lpad(&str, 5, &pad);

    CHECK_STR_EQ("xxxab", str.v, "xxxab");
    CHECK_FS2VALID(1, &str);

    fs2_free(&str);
    fs2_free(&pad);
    assert_balanced();
}

Test(fs2_api, lpad_gap_not_multiple_of_pad) {
    __fs2_checkreset();
    fs2 str = fs2_fromstr("a");
    fs2 pad = fs2_fromstr("xyz");

    fs2_lpad(&str, 8, &pad);   /* gap = 7 → "xyzxyzx" */

    CHECK_STR_EQ("xyzxyzxa", str.v, "xyzxyzxa");
    CHECK_FS2VALID(1, &str);

    fs2_free(&str);
    fs2_free(&pad);
    assert_balanced();
}

Test(fs2_api, lpad_pad_longer_than_gap) {
    __fs2_checkreset();
    fs2 str = fs2_fromstr("ab");
    fs2 pad = fs2_fromstr("xyz");

    fs2_lpad(&str, 4, &pad);   /* gap = 2 → "xy" + "ab" */

    CHECK_STR_EQ("xyab", str.v, "xyab");
    CHECK_FS2VALID(1, &str);

    fs2_free(&str);
    fs2_free(&pad);
    assert_balanced();
}

/* Ключевой: содержимое действительно сдвинуто, а не затёрто паддингом */
Test(fs2_api, lpad_content_shifted_not_overwritten) {
    __fs2_checkreset();
    fs2 str = fs2_fromstr("abc");
    fs2 pad = fs2_fromstr("x");

    fs2_lpad(&str, 6, &pad);

    CHECK_STR_EQ("xxxabc", str.v, "xxxabc");
    CHECK_FS2VALID(1, &str);

    fs2_free(&str);
    fs2_free(&pad);
    assert_balanced();
}

/* =========================================================================
 * fs2_lpad — truncate / len == s->len / len == 0
 * ========================================================================= */

Test(fs2_api, lpad_truncate_when_len_smaller) {
    __fs2_checkreset();
    fs2 str = fs2_fromstr("hello");
    fs2 pad = fs2_fromstr("xy");

    fs2_lpad(&str, 3, &pad);

    CHECK_STR_EQ("hel", str.v, "hel");
    CHECK_EQ  ("len",   str.len, 3);
    CHECK_FS2VALID(1, &str);

    fs2_free(&str);
    fs2_free(&pad);
    assert_balanced();
}

Test(fs2_api, lpad_same_len_noop) {
    __fs2_checkreset();
    fs2 str = fs2_fromstr("hello");
    fs2 pad = fs2_fromstr("xy");
    void *before = str.v;

    fs2_lpad(&str, 5, &pad);

    CHECK_PTR_EQ("buffer unchanged", str.v, before);
    CHECK_STR_EQ("hello",            str.v, "hello");
    CHECK_FS2VALID(1, &str);

    fs2_free(&str);
    fs2_free(&pad);
    assert_balanced();
}

Test(fs2_api, lpad_len_zero_truncates_to_empty) {
    __fs2_checkreset();
    fs2 str = fs2_fromstr("hello");
    fs2 pad = fs2_fromstr("xy");

    fs2_lpad(&str, 0, &pad);

    CHECK_EQ  ("len",    str.len, 0);
    CHECK_TRUE("empty",  fs2_is_empty(&str));
    CHECK_FS2VALID(1, &str);

    fs2_free(&str);
    fs2_free(&pad);
    assert_balanced();
}

/* =========================================================================
 * fs2_lpad — empty pad: no-op
 * ========================================================================= */

Test(fs2_api, lpad_canon_pad_noop) {
    __fs2_checkreset();
    fs2 str = fs2_fromstr("hello");
    fs2 pad = FS2();

    fs2_lpad(&str, 10, &pad);

    CHECK_STR_EQ("hello", str.v, "hello");
    CHECK_FS2VALID(1, &str);

    fs2_free(&str);
    fs2_free(&pad);
    assert_balanced();
}

Test(fs2_api, lpad_fs2empty_pad_noop) {
    __fs2_checkreset();
    fs2 str = fs2_fromstr("hello");
    fs2 pad = FS2EMPTY;

    fs2_lpad(&str, 10, &pad);

    CHECK_STR_EQ("hello", str.v, "hello");
    CHECK_FS2VALID(1, &str);

    fs2_free(&str);
    assert_balanced();
}

/* =========================================================================
 * fs2_lpad — empty str → promote (САМАЯ ВАЖНАЯ ГРУППА, ловит SEGV)
 * ========================================================================= */

Test(fs2_api, lpad_canon_str_promotes) {
    __fs2_checkreset();
    fs2 str = FS2();
    fs2 pad = fs2_fromstr("ab");

    fs2 *r = fs2_lpad(&str, 5, &pad);

    CHECK_STR_EQ("ababa",           str.v, "ababa");
    CHECK_EQ  ("len",               str.len, 5);
    CHECK_FALSE("no longer canon",  fs2_is_canon(&str));
    CHECK_TRUE ("now ALLOC",        fs2_is_alloc(&str));
    CHECK_PTR_EQ("chaining",        r, &str);
    CHECK_FS2VALID(1, &str);

    fs2_free(&str);
    fs2_free(&pad);
    assert_balanced();
}

Test(fs2_api, lpad_fs2empty_str_promotes) {
    __fs2_checkreset();
    fs2 str = FS2EMPTY;
    fs2 pad = fs2_fromstr("ab");

    fs2_lpad(&str, 5, &pad);

    CHECK_STR_EQ("ababa",              str.v, "ababa");
    CHECK_FALSE("not STATIC anymore",   fs2_is_static(&str));
    CHECK_TRUE ("now ALLOC",            fs2_is_alloc(&str));
    CHECK_FS2VALID(1, &str);

    fs2_free(&str);
    fs2_free(&pad);
    assert_balanced();
}

Test(fs2_api, lpad_empty_str_single_char_pad) {
    __fs2_checkreset();
    fs2 str = FS2();
    fs2 pad = fs2_fromstr("x");

    fs2_lpad(&str, 4, &pad);

    CHECK_STR_EQ("xxxx", str.v, "xxxx");
    CHECK_FS2VALID(1, &str);

    fs2_free(&str);
    fs2_free(&pad);
    assert_balanced();
}

Test(fs2_api, lpad_empty_str_empty_pad_stays_empty) {
    __fs2_checkreset();
    fs2 str = FS2();
    fs2 pad = FS2EMPTY;

    fs2_lpad(&str, 5, &pad);

    CHECK_TRUE("still canon", fs2_is_canon(&str));
    CHECK_FS2VALID(1, &str);

    fs2_free(&str);
    assert_balanced();
}

/* =========================================================================
 * fs2_lpad — guards
 * ========================================================================= */

Test(fs2_api, lpad_null_str_aborts, .signal = SIGABRT) {
    fs2 pad = fs2_fromstr("xy");
    fs2_lpad(NULL, 10, &pad);
    fs2_free(&pad);
}

Test(fs2_api, lpad_null_pad_aborts, .signal = SIGABRT) {
    fs2 str = fs2_fromstr("ab");
    fs2_lpad(&str, 10, NULL);
    fs2_free(&str);
}

Test(fs2_api, lpad_static_nonempty_str_aborts, .signal = SIGABRT) {
    fs2 str = FS2LITERAL("hello");
    fs2 pad = fs2_fromstr("xy");
    fs2_lpad(&str, 10, &pad);
    fs2_free(&pad);
}

/* =========================================================================
 * fs2_lpad — LOCAL
 * ========================================================================= */

Test(fs2_api, lpad_local_sufficient_capacity_works) {
    __fs2_checkreset();
    FS2LOCAL(buf, 16);
    fs2_memcpy_str(&buf, 0, "ab", 2);
    fs2_setlen(&buf, 2);
    fs2 pad = fs2_fromstr("xy");

    fs2_lpad(&buf, 8, &pad);

    CHECK_STR_EQ("xyxyxyab",  buf.v, "xyxyxyab");
    CHECK_TRUE ("still LOCAL", fs2_is_local(&buf));
    CHECK_TRUE ("not ALLOC",  !fs2_is_alloc(&buf));
    CHECK_FS2VALID(1, &buf);

    fs2_free(&pad);
    assert_balanced();
}

Test(fs2_api, lpad_local_insufficient_capacity_aborts, .signal = SIGABRT) {
    FS2LOCAL(buf, 4);
    fs2_memcpy_str(&buf, 0, "abc", 3);
    fs2_setlen(&buf, 3);
    fs2 pad = fs2_fromstr("x");

    fs2_lpad(&buf, 10, &pad);   /* need=11 > sz=4 → abort в reserve */
    fs2_free(&pad);
}

/* =========================================================================
 * fs2_lpad — immutability pad / src
 * ========================================================================= */

Test(fs2_api, lpad_pad_untouched) {
    __fs2_checkreset();
    fs2 str = fs2_fromstr("ab");
    fs2 pad = fs2_fromstr("xy");
    void *pad_v = pad.v;

    fs2_lpad(&str, 8, &pad);

    CHECK_PTR_EQ("pad v unchanged",       pad.v, pad_v);
    CHECK_STR_EQ("pad content unchanged", pad.v, "xy");
    CHECK_EQ  ("pad len unchanged",       pad.len, 2);
    CHECK_FS2VALID(1, &pad);

    fs2_free(&str);
    fs2_free(&pad);
    assert_balanced();
}

/* =========================================================================
 * Consistency: lpad(s, n, pad) == reverse(rpad(reverse(s), n, pad))
 * ========================================================================= */

Test(fs2_api, lpad_equals_reverse_rpad_of_reverse) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("abc");
    fs2 b = fs2_fromstr("abc");
    fs2 pad = fs2_fromstr("xy");

    fs2_lpad(&a, 8, &pad);

    /* b: flip → rpad → flip */
    fs2_flip(&b);            /* "cba" */
    fs2_rpad(&b, 8, &pad);   /* "cbaxyxyx" */
    fs2_flip(&b);            /* "xyxyxabc" */

    CHECK_STR_EQ("lpad result",          a.v, "xyxyxabc");
    CHECK_STR_EQ("flip-rpad-flip result", b.v, "xyxyxabc");
    CHECK_EQ  ("same len",               a.len, b.len);
    CHECK_FS2VALID(1, &a);
    CHECK_FS2VALID(2, &b);

    fs2_free(&a);
    fs2_free(&b);
    fs2_free(&pad);
    assert_balanced();
}

/* =========================================================================
 * fs2_lpadstr — nominal
 * ========================================================================= */

Test(fs2_api, lpadstr_basic) {
    __fs2_checkreset();
    fs2 str = fs2_fromstr("ab");

    fs2 *r = fs2_lpadstr(&str, 8, "xy");

    CHECK_STR_EQ("xyxyxyab", str.v, "xyxyxyab");
    CHECK_EQ  ("len",        str.len, 8);
    CHECK_PTR_EQ("chaining", r, &str);
    CHECK_FS2VALID(1, &str);

    fs2_free(&str);
    assert_balanced();
}

Test(fs2_api, lpadstr_single_char_pad) {
    __fs2_checkreset();
    fs2 str = fs2_fromstr("ab");

    fs2_lpadstr(&str, 5, "x");

    CHECK_STR_EQ("xxxab", str.v, "xxxab");
    CHECK_FS2VALID(1, &str);

    fs2_free(&str);
    assert_balanced();
}

Test(fs2_api, lpadstr_gap_not_multiple_of_pad) {
    __fs2_checkreset();
    fs2 str = fs2_fromstr("a");

    fs2_lpadstr(&str, 8, "xyz");   /* gap = 7 → "xyzxyzx" */

    CHECK_STR_EQ("xyzxyzxa", str.v, "xyzxyzxa");
    CHECK_FS2VALID(1, &str);

    fs2_free(&str);
    assert_balanced();
}

Test(fs2_api, lpadstr_pad_longer_than_gap) {
    __fs2_checkreset();
    fs2 str = fs2_fromstr("ab");

    fs2_lpadstr(&str, 4, "xyz");   /* gap = 2 → "xy" */

    CHECK_STR_EQ("xyab", str.v, "xyab");
    CHECK_FS2VALID(1, &str);

    fs2_free(&str);
    assert_balanced();
}

/* =========================================================================
 * fs2_lpadstr — truncate / no-op / len == 0
 * ========================================================================= */

Test(fs2_api, lpadstr_truncate_when_len_smaller) {
    __fs2_checkreset();
    fs2 str = fs2_fromstr("hello");

    fs2_lpadstr(&str, 3, "xy");

    CHECK_STR_EQ("hel", str.v, "hel");
    CHECK_EQ  ("len",   str.len, 3);
    CHECK_FS2VALID(1, &str);

    fs2_free(&str);
    assert_balanced();
}

Test(fs2_api, lpadstr_same_len_noop) {
    __fs2_checkreset();
    fs2 str = fs2_fromstr("hello");
    void *before = str.v;

    fs2_lpadstr(&str, 5, "xy");

    CHECK_PTR_EQ("buffer unchanged", str.v, before);
    CHECK_STR_EQ("hello",            str.v, "hello");
    CHECK_FS2VALID(1, &str);

    fs2_free(&str);
    assert_balanced();
}

Test(fs2_api, lpadstr_len_zero_truncates_to_empty) {
    __fs2_checkreset();
    fs2 str = fs2_fromstr("hello");

    fs2_lpadstr(&str, 0, "xy");

    CHECK_EQ  ("len",   str.len, 0);
    CHECK_TRUE("empty", fs2_is_empty(&str));
    CHECK_FS2VALID(1, &str);

    fs2_free(&str);
    assert_balanced();
}

/* =========================================================================
 * fs2_lpadstr — empty padstr (""): no-op (ключевое отличие от NULL)
 * ========================================================================= */

Test(fs2_api, lpadstr_empty_string_noop) {
    __fs2_checkreset();
    fs2 str = fs2_fromstr("ab");
    void *before = str.v;

    fs2 *r = fs2_lpadstr(&str, 10, "");

    CHECK_PTR_EQ("chaining",         r, &str);
    CHECK_PTR_EQ("buffer unchanged", str.v, before);
    CHECK_STR_EQ("ab",               str.v, "ab");
    CHECK_EQ  ("len",                str.len, 2);
    CHECK_FS2VALID(1, &str);

    fs2_free(&str);
    assert_balanced();
}

/* =========================================================================
 * fs2_lpadstr — empty str → promote
 *   (зависит от фикса fs2_lpad: fs2_reserve_span до selfmove;
 *    без фикса — SEGV)
 * ========================================================================= */

Test(fs2_api, lpadstr_canon_str_promotes) {
    __fs2_checkreset();
    fs2 str = FS2();

    fs2_lpadstr(&str, 5, "ab");

    CHECK_STR_EQ("ababa",           str.v, "ababa");
    CHECK_EQ  ("len",               str.len, 5);
    CHECK_FALSE("no longer canon",  fs2_is_canon(&str));
    CHECK_TRUE ("now ALLOC",        fs2_is_alloc(&str));
    CHECK_FS2VALID(1, &str);

    fs2_free(&str);
    assert_balanced();
}

Test(fs2_api, lpadstr_fs2empty_str_promotes) {
    __fs2_checkreset();
    fs2 str = FS2EMPTY;

    fs2_lpadstr(&str, 5, "ab");

    CHECK_STR_EQ("ababa",              str.v, "ababa");
    CHECK_FALSE("not STATIC anymore",   fs2_is_static(&str));
    CHECK_TRUE ("now ALLOC",            fs2_is_alloc(&str));
    CHECK_FS2VALID(1, &str);

    fs2_free(&str);
    assert_balanced();
}

Test(fs2_api, lpadstr_empty_str_empty_padstr_stays_empty) {
    __fs2_checkreset();
    fs2 str = FS2();

    fs2_lpadstr(&str, 5, "");

    CHECK_TRUE("still canon", fs2_is_canon(&str));
    CHECK_FS2VALID(1, &str);

    fs2_free(&str);
    assert_balanced();
}

/* =========================================================================
 * fs2_lpadstr — guards
 * ========================================================================= */

Test(fs2_api, lpadstr_null_str_aborts, .signal = SIGABRT) {
    fs2_lpadstr(NULL, 10, "xy");
}

Test(fs2_api, lpadstr_null_padstr_aborts, .signal = SIGABRT) {
    fs2 str = fs2_fromstr("ab");
    fs2_lpadstr(&str, 10, NULL);
    fs2_free(&str);
}

Test(fs2_api, lpadstr_static_nonempty_str_aborts, .signal = SIGABRT) {
    fs2 str = FS2LITERAL("hello");
    fs2_lpadstr(&str, 10, "xy");
}

/* =========================================================================
 * fs2_lpadstr — LOCAL
 * ========================================================================= */

Test(fs2_api, lpadstr_local_sufficient_capacity_works) {
    __fs2_checkreset();
    FS2LOCAL(buf, 16);
    fs2_memcpy_str(&buf, 0, "ab", 2);
    fs2_setlen(&buf, 2);

    fs2_lpadstr(&buf, 8, "xy");

    CHECK_STR_EQ("xyxyxyab",  buf.v, "xyxyxyab");
    CHECK_TRUE ("still LOCAL", fs2_is_local(&buf));
    CHECK_TRUE ("not ALLOC",  !fs2_is_alloc(&buf));
    CHECK_FS2VALID(1, &buf);

    assert_balanced();
}

Test(fs2_api, lpadstr_local_insufficient_capacity_aborts, .signal = SIGABRT) {
    FS2LOCAL(buf, 4);
    fs2_memcpy_str(&buf, 0, "abc", 3);
    fs2_setlen(&buf, 3);
    fs2_lpadstr(&buf, 10, "x");
}

/* =========================================================================
 * Consistency: lpadstr(s, n, p) == lpad(s, n, wrap(p))
 * ========================================================================= */

Test(fs2_api, lpad_and_lpadstr_agree) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("ab");
    fs2 b = fs2_fromstr("ab");
    fs2 pad = fs2_fromstr("xy");

    fs2_lpad(&a, 6, &pad);
    fs2_lpadstr(&b, 6, "xy");

    CHECK_STR_EQ("same content", a.v, b.v);
    CHECK_EQ  ("same len",       a.len, b.len);
    CHECK_FS2VALID(1, &a);
    CHECK_FS2VALID(2, &b);

    fs2_free(&a);
    fs2_free(&b);
    fs2_free(&pad);
    assert_balanced();
}

/* =========================================================================
 * Симметрия: rpadstr / lpadstr на разных сторонах дают ожидаемое
 * ========================================================================= */

Test(fs2_api, rpadstr_lpadstr_combined) {
    __fs2_checkreset();
    fs2 str = fs2_fromstr("X");

    fs2_rpadstr(&str, 3, "R");   /* "XRR" */
    fs2_lpadstr(&str, 5, "L");   /* "LLXRR" */

    CHECK_STR_EQ("LLXRR", str.v, "LLXRR");
    CHECK_EQ  ("len",     str.len, 5);
    CHECK_FS2VALID(1, &str);

    fs2_free(&str);
    assert_balanced();
}

/* =========================================================================
 * fs2_lpad_new — nominal (padding path)
 * ========================================================================= */

Test(fs2_api, lpad_new_basic) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("ab");
    fs2 pad = fs2_fromstr("xy");

    fs2 r = fs2_lpad_new(&src, 8, &pad);

    CHECK_STR_EQ("xyxyxyab", r.v, "xyxyxyab");
    CHECK_EQ  ("len",        r.len, 8);
    CHECK_TRUE ("ALLOC",          fs2_is_alloc(&r));
    CHECK_FALSE("not bodyalloc",  fs2_is_bodyalloc(&r));
    CHECK_FS2VALID(1, &r);
    CHECK_FS2VALID(2, &src);
    CHECK_FS2VALID(3, &pad);

    fs2_free(&src);
    fs2_free(&pad);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api, lpad_new_single_char_pad) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("ab");
    fs2 pad = fs2_fromstr("x");

    fs2 r = fs2_lpad_new(&src, 5, &pad);

    CHECK_STR_EQ("xxxab", r.v, "xxxab");
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    fs2_free(&pad);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api, lpad_new_gap_not_multiple_of_pad) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("a");
    fs2 pad = fs2_fromstr("xyz");

    fs2 r = fs2_lpad_new(&src, 8, &pad);   /* gap = 7 → "xyzxyzx" */

    CHECK_STR_EQ("xyzxyzxa", r.v, "xyzxyzxa");
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    fs2_free(&pad);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api, lpad_new_pad_longer_than_gap) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("ab");
    fs2 pad = fs2_fromstr("xyz");

    fs2 r = fs2_lpad_new(&src, 4, &pad);   /* gap = 2 → "xy" */

    CHECK_STR_EQ("xyab", r.v, "xyab");
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    fs2_free(&pad);
    fs2_free(&r);
    assert_balanced();
}

/* =========================================================================
 * fs2_lpad_new — truncate / len == s->len / len == 0
 * ========================================================================= */

Test(fs2_api, lpad_new_truncate_when_len_smaller) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello");
    fs2 pad = fs2_fromstr("xy");

    fs2 r = fs2_lpad_new(&src, 3, &pad);

    CHECK_STR_EQ("hel", r.v, "hel");
    CHECK_EQ  ("len",   r.len, 3);
    CHECK_FS2VALID(1, &r);
    CHECK_FS2VALID(2, &src);

    fs2_free(&src);
    fs2_free(&pad);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api, lpad_new_len_eq_slen_clone_whole) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello");
    fs2 pad = fs2_fromstr("xy");

    fs2 r = fs2_lpad_new(&src, 5, &pad);

    CHECK_STR_EQ("hello",     r.v, "hello");
    CHECK_EQ  ("len",         r.len, 5);
    CHECK_TRUE("independent", r.v != src.v);
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    fs2_free(&pad);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api, lpad_new_len_zero_truncates_to_empty) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello");
    fs2 pad = fs2_fromstr("xy");

    fs2 r = fs2_lpad_new(&src, 0, &pad);

    CHECK_EQ  ("len",   r.len, 0);
    CHECK_TRUE("empty", fs2_is_empty(&r));
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    fs2_free(&pad);
    fs2_free(&r);
    assert_balanced();
}

/* =========================================================================
 * fs2_lpad_new — empty pad (no-op, clone as-is)
 * ========================================================================= */

Test(fs2_api, lpad_new_canon_pad_clone_unchanged) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello");
    fs2 pad = FS2();

    fs2 r = fs2_lpad_new(&src, 10, &pad);

    CHECK_STR_EQ("hello",   r.v, "hello");
    CHECK_EQ  ("len",       r.len, 5);
    CHECK_TRUE("ALLOC",     fs2_is_alloc(&r));
    CHECK_TRUE("independent", r.v != src.v);
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    fs2_free(&pad);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api, lpad_new_fs2empty_pad_clone_unchanged) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello");
    fs2 pad = FS2EMPTY;

    fs2 r = fs2_lpad_new(&src, 10, &pad);

    CHECK_STR_EQ("hello", r.v, "hello");
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    fs2_free(&r);
    assert_balanced();
}

/* =========================================================================
 * fs2_lpad_new — empty src (clone → FS2EMPTY → promote via lpad)
 * ========================================================================= */

Test(fs2_api, lpad_new_canon_src_with_nonempty_pad) {
    __fs2_checkreset();
    fs2 src = FS2();
    fs2 pad = fs2_fromstr("ab");

    fs2 r = fs2_lpad_new(&src, 5, &pad);

    CHECK_STR_EQ("ababa", r.v, "ababa");
    CHECK_EQ  ("len",     r.len, 5);
    CHECK_TRUE("ALLOC",   fs2_is_alloc(&r));
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    fs2_free(&pad);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api, lpad_new_fs2empty_src_with_nonempty_pad) {
    __fs2_checkreset();
    fs2 src = FS2EMPTY;
    fs2 pad = fs2_fromstr("ab");

    fs2 r = fs2_lpad_new(&src, 5, &pad);

    CHECK_STR_EQ("ababa", r.v, "ababa");
    CHECK_FS2VALID(1, &r);

    fs2_free(&pad);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api, lpad_new_empty_src_single_char_pad) {
    __fs2_checkreset();
    fs2 src = FS2();
    fs2 pad = fs2_fromstr("x");

    fs2 r = fs2_lpad_new(&src, 4, &pad);

    CHECK_STR_EQ("xxxx", r.v, "xxxx");
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    fs2_free(&pad);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api, lpad_new_empty_src_empty_pad_stays_empty) {
    __fs2_checkreset();
    fs2 src = FS2();
    fs2 pad = FS2EMPTY;

    fs2 r = fs2_lpad_new(&src, 5, &pad);

    CHECK_TRUE("eq_empty", fs2_eq_empty(&r));
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api, lpad_new_empty_src_len_zero_stays_empty) {
    __fs2_checkreset();
    fs2 src = FS2EMPTY;
    fs2 pad = fs2_fromstr("xy");

    fs2 r = fs2_lpad_new(&src, 0, &pad);

    CHECK_TRUE("eq_empty", fs2_eq_empty(&r));
    CHECK_FS2VALID(1, &r);

    fs2_free(&pad);
    fs2_free(&r);
    assert_balanced();
}

/* =========================================================================
 * fs2_lpad_new — STATIC / LOCAL source (клон → ALLOC)
 * ========================================================================= */

Test(fs2_api, lpad_new_static_src) {
    __fs2_checkreset();
    fs2 src = FS2LITERAL("hi");
    fs2 pad = fs2_fromstr("xy");

    fs2 r = fs2_lpad_new(&src, 6, &pad);

    CHECK_STR_EQ("xyxyhi",    r.v, "xyxyhi");
    CHECK_TRUE ("ALLOC",      fs2_is_alloc(&r));
    CHECK_TRUE ("independent", r.v != src.v);
    CHECK_FS2VALID(1, &r);

    fs2_free(&pad);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api, lpad_new_local_src) {
    __fs2_checkreset();
    FS2LOCAL(buf, 8);
    fs2_memcpy_str(&buf, 0, "ab", 2);
    fs2_setlen(&buf, 2);
    fs2 pad = fs2_fromstr("x");

    fs2 r = fs2_lpad_new(&buf, 5, &pad);

    CHECK_STR_EQ("xxxab",            r.v,   "xxxab");
    CHECK_TRUE ("result ALLOC",      fs2_is_alloc(&r));
    CHECK_TRUE ("result not LOCAL",  !fs2_is_local(&r));
    CHECK_STR_EQ("buf untouched",    buf.v, "ab");
    CHECK_FS2VALID(1, &r);
    CHECK_FS2VALID(2, &buf);

    fs2_free(&pad);
    fs2_free(&r);
    assert_balanced();
}

/* =========================================================================
 * fs2_lpad_new — guards (SIGABRT)
 * ========================================================================= */

Test(fs2_api, lpad_new_null_str_aborts, .signal = SIGABRT) {
    fs2 pad = fs2_fromstr("xy");
    fs2_lpad_new(NULL, 5, &pad);
    fs2_free(&pad);
}

Test(fs2_api, lpad_new_null_pad_aborts, .signal = SIGABRT) {
    fs2 src = fs2_fromstr("ab");
    fs2_lpad_new(&src, 5, NULL);
    fs2_free(&src);
}

/* =========================================================================
 * fs2_lpad_new — immutability / independence
 * ========================================================================= */

Test(fs2_api, lpad_new_src_untouched) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("original");
    fs2 pad = fs2_fromstr("_");
    void *orig_v = src.v;
    void *pad_v  = pad.v;

    fs2 r = fs2_lpad_new(&src, 12, &pad);

    CHECK_PTR_EQ("src v unchanged", src.v, orig_v);
    CHECK_STR_EQ("src content",     src.v, "original");
    CHECK_PTR_EQ("pad v unchanged", pad.v, pad_v);
    CHECK_STR_EQ("pad content",     pad.v, "_");
    CHECK_STR_EQ("result",          r.v,   "____original");
    CHECK_FS2VALID(1, &r);
    CHECK_FS2VALID(2, &src);
    CHECK_FS2VALID(3, &pad);

    fs2_free(&src);
    fs2_free(&pad);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api, lpad_new_independent_buffer) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("ab");
    fs2 pad = fs2_fromstr("xy");

    fs2 r = fs2_lpad_new(&src, 6, &pad);

    r.v[0] = 'X';

    CHECK_STR_EQ("result mutated", r.v,   "Xyxyab");
    CHECK_STR_EQ("src original",   src.v, "ab");
    CHECK_FS2VALID(1, &r);
    CHECK_FS2VALID(2, &src);

    fs2_free(&src);
    fs2_free(&pad);
    fs2_free(&r);
    assert_balanced();
}

/* =========================================================================
 * Consistency: lpad_new(s, n, p) == clone(s) + lpad(clone, n, p)
 * ========================================================================= */

Test(fs2_api, lpad_new_matches_clone_then_lpad) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("ab");
    fs2 pad = fs2_fromstr("xy");

    fs2 manual = fs2_clone(&src);
    fs2_lpad(&manual, 8, &pad);

    fs2 via = fs2_lpad_new(&src, 8, &pad);

    CHECK_STR_EQ("same content", manual.v,   via.v);
    CHECK_EQ  ("same len",       manual.len, via.len);
    CHECK_FS2VALID(1, &manual);
    CHECK_FS2VALID(2, &via);

    fs2_free(&src);
    fs2_free(&pad);
    fs2_free(&manual);
    fs2_free(&via);
    assert_balanced();
}

/* =========================================================================
 * Симметрия: lpad_new / rpad_new на одном src
 * ========================================================================= */

Test(fs2_api, lpad_new_rpad_new_symmetric) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("X");
    fs2 pad = fs2_fromstr("-");

    fs2 left  = fs2_lpad_new(&src, 5, &pad);
    fs2 right = fs2_rpad_new(&src, 5, &pad);

    CHECK_STR_EQ("----X", left.v,  "----X");
    CHECK_STR_EQ("X----", right.v, "X----");
    CHECK_EQ  ("same len", left.len, right.len);
    CHECK_FS2VALID(1, &left);
    CHECK_FS2VALID(2, &right);

    fs2_free(&src);
    fs2_free(&pad);
    fs2_free(&left);
    fs2_free(&right);
    assert_balanced();
}

/* =========================================================================
 * fs2_lpadstr_new — только padstr-специфика
 * (str/len/promote/truncate/LOCAL/immutability — покрыты в lpad_new-тестах)
 * ========================================================================= */

Test(fs2_api, lpadstr_new_basic) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("ab");

    fs2 r = fs2_lpadstr_new(&src, 6, "xy");

    CHECK_STR_EQ("xyxyab", r.v, "xyxyab");
    CHECK_EQ  ("len",      r.len, 6);
    CHECK_FS2VALID(1, &r);
    CHECK_FS2VALID(2, &src);

    fs2_free(&src);
    fs2_free(&r);
    assert_balanced();
}

/* "" padstr — валидный вход, no-op (clone of src) */
Test(fs2_api, lpadstr_new_empty_padstr_noop) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello");

    fs2 r = fs2_lpadstr_new(&src, 10, "");

    CHECK_STR_EQ("hello",   r.v, "hello");
    CHECK_EQ  ("len",       r.len, 5);
    CHECK_TRUE("independent", r.v != src.v);
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    fs2_free(&r);
    assert_balanced();
}

/* NULL padstr — SIGABRT через _fs2_wrap_notnull */
Test(fs2_api, lpadstr_new_null_padstr_aborts, .signal = SIGABRT) {
    fs2 src = fs2_fromstr("ab");
    fs2_lpadstr_new(&src, 5, NULL);
    fs2_free(&src);
}

/* Согласованность с lpad_new через wrap */
Test(fs2_api, lpadstr_new_matches_lpad_new_with_wrapped_pad) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("ab");
    fs2 pad = fs2_fromstr("xy");

    fs2 via_pad  = fs2_lpad_new(&src, 8, &pad);
    fs2 via_str  = fs2_lpadstr_new(&src, 8, "xy");

    CHECK_STR_EQ("same content", via_pad.v,   via_str.v);
    CHECK_EQ  ("same len",       via_pad.len, via_str.len);
    CHECK_FS2VALID(1, &via_pad);
    CHECK_FS2VALID(2, &via_str);

    fs2_free(&src);
    fs2_free(&pad);
    fs2_free(&via_pad);
    fs2_free(&via_str);
    assert_balanced();
}

/* =========================================================================
 * fs2_genrnd — nominal: каждый тип → свой charset
 * ========================================================================= */

/* Хелпер: проверить, что все символы в диапазоне [lo, hi] */
static bool
all_in_range(const fs2 *s, char lo, char hi) {
    for (size_t i = 0; i < s->len; i++)
        if (s->v[i] < lo || s->v[i] > hi)
            return false;
    return true;
}

Test(fs2_api, genrnd_lowercase) {
    __fs2_checkreset();
    fs2 s = FS2();

    fs2 *r = fs2_genrnd(&s, 20, 'a');

    CHECK_PTR_EQ("chaining", r, &s);
    CHECK_EQ  ("len",        s.len, 20);
    CHECK_TRUE("all a-z",    all_in_range(&s, 'a', 'z'));
    CHECK_EQ  ("NUL at len", s.v[s.len], '\0');
    CHECK_TRUE("ALLOC",      fs2_is_alloc(&s));
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, genrnd_uppercase) {
    __fs2_checkreset();
    fs2 s = FS2();

    fs2_genrnd(&s, 20, 'A');

    CHECK_EQ  ("len",    s.len, 20);
    CHECK_TRUE("all A-Z", all_in_range(&s, 'A', 'Z'));
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, genrnd_digits) {
    __fs2_checkreset();
    fs2 s = FS2();

    fs2_genrnd(&s, 20, '0');

    CHECK_EQ  ("len",    s.len, 20);
    CHECK_TRUE("all 0-9", all_in_range(&s, '0', '9'));
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, genrnd_unknown_type_fills_question) {
    __fs2_checkreset();
    fs2 s = FS2();

    fs2_genrnd(&s, 10, 'x');   /* любой не-селектор */

    CHECK_EQ  ("len",    s.len, 10);
    for (size_t i = 0; i < s.len; i++)
        CHECK_EQ("char", s.v[i], '?');
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_genrnd — статистика: покрытие алфавита
 *   Ловит баг "rndint(26 - 1) даёт [0,24], 'z' недостижим"
 * ========================================================================= */

Test(fs2_api, genrnd_lowercase_covers_full_alphabet) {
    __fs2_checkreset();
    fs2 s = FS2();
    fs2_genrnd(&s, 1000, 'a');

    bool saw[26] = {false};
    for (size_t i = 0; i < s.len; i++) {
        if (s.v[i] >= 'a' && s.v[i] <= 'z')
            saw[s.v[i] - 'a'] = true;
    }
    for (int i = 0; i < 26; i++)
        CHECK_TRUE("letter present", saw[i]);   /* все 26 хотя бы раз */

    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, genrnd_digits_covers_full_alphabet) {
    __fs2_checkreset();
    fs2 s = FS2();
    fs2_genrnd(&s, 500, '0');

    bool saw[10] = {false};
    for (size_t i = 0; i < s.len; i++)
        saw[s.v[i] - '0'] = true;
    for (int i = 0; i < 10; i++)
        CHECK_TRUE("digit present", saw[i]);

    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_genrnd — overwrite (не append!) и len == 0
 * ========================================================================= */

Test(fs2_api, genrnd_overwrites_existing_content) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");
    cr_assert(s.len == 5);

    fs2_genrnd(&s, 3, 'a');

    CHECK_EQ  ("len truncated to 3", s.len, 3);
    CHECK_EQ  ("old content gone",   s.v[3], '\0');
    /* старые байты 'h','e','l' заменены случайными — хотя бы один
     * отличается с очень высокой вероятностью, но не проверяем
     * конкретные значения. Главное — len=3, а не 5. */
    CHECK_TRUE("all a-z", all_in_range(&s, 'a', 'z'));
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, genrnd_len_zero_truncates_to_empty) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    fs2_genrnd(&s, 0, 'a');

    CHECK_EQ  ("len",    s.len, 0);
    CHECK_TRUE("empty",  fs2_is_empty(&s));
    CHECK_EQ  ("v[0]",   s.v[0], '\0');
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, genrnd_canon_str_promotes) {
    __fs2_checkreset();
    fs2 s = FS2();

    fs2_genrnd(&s, 5, 'a');

    CHECK_EQ  ("len",             s.len, 5);
    CHECK_FALSE("no longer canon", fs2_is_canon(&s));
    CHECK_TRUE ("now ALLOC",       fs2_is_alloc(&s));
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, genrnd_fs2empty_str_promotes) {
    __fs2_checkreset();
    fs2 s = FS2EMPTY;

    fs2_genrnd(&s, 5, 'a');

    CHECK_EQ  ("len",                 s.len, 5);
    CHECK_FALSE("not STATIC anymore",  fs2_is_static(&s));
    CHECK_TRUE ("now ALLOC",           fs2_is_alloc(&s));
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_genrnd — guards
 * ========================================================================= */

Test(fs2_api, genrnd_null_aborts, .signal = SIGABRT) {
    fs2_genrnd(NULL, 5, 'a');
}

Test(fs2_api, genrnd_static_nonempty_aborts, .signal = SIGABRT) {
    fs2 s = FS2LITERAL("hello");
    fs2_genrnd(&s, 5, 'a');
}

/* =========================================================================
 * fs2_genrnd — LOCAL
 * ========================================================================= */

Test(fs2_api, genrnd_local_sufficient_capacity_works) {
    __fs2_checkreset();
    FS2LOCAL(buf, 16);

    fs2_genrnd(&buf, 10, 'a');

    CHECK_EQ  ("len",          buf.len, 10);
    CHECK_TRUE("still LOCAL",  fs2_is_local(&buf));
    CHECK_TRUE("all a-z",      all_in_range(&buf, 'a', 'z'));
    CHECK_FS2VALID(1, &buf);

    assert_balanced();
}

Test(fs2_api, genrnd_local_insufficient_capacity_aborts, .signal = SIGABRT) {
    FS2LOCAL(buf, 4);
    fs2_genrnd(&buf, 10, 'a');   /* need=11 > sz=4 */
}

/* =========================================================================
 * fs2_genrnd_new — только обёртка-специфика
 * (charset-семантика покрыта в genrnd-тестах)
 * ========================================================================= */

Test(fs2_api, genrnd_new_basic) {
    __fs2_checkreset();
    fs2 s = fs2_genrnd_new(10, 'a');

    CHECK_EQ  ("len",             s.len, 10);
    CHECK_TRUE("ALLOC",           fs2_is_alloc(&s));
    CHECK_FALSE("not bodyalloc",  fs2_is_bodyalloc(&s));
    CHECK_FALSE("not STATIC",     fs2_is_static(&s));
    CHECK_EQ  ("NUL at len",      s.v[s.len], '\0');
    CHECK_FS2VALID(1, &s);

    for (size_t i = 0; i < s.len; i++)
        CHECK_TRUE("a-z", s.v[i] >= 'a' && s.v[i] <= 'z');

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, genrnd_new_unknown_type) {
    __fs2_checkreset();
    fs2 s = fs2_genrnd_new(5, 'x');

    CHECK_EQ("len", 5, s.len);
    for (size_t i = 0; i < s.len; i++)
        CHECK_EQ("'?'", s.v[i], '?');
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, genrnd_new_len_zero) {
    __fs2_checkreset();
    fs2 s = fs2_genrnd_new(0, 'a');

    /* fs2_init(1) → ALLOC, v≠NULL, len=0 — не FS2EMPTY и не canon */
    CHECK_EQ  ("len",          s.len, 0);
    CHECK_TRUE("empty",        fs2_is_empty(&s));
    CHECK_TRUE("ALLOC",        fs2_is_alloc(&s));
    CHECK_NOT_NULL("v allocated", s.v);
    CHECK_EQ  ("v[0] == NUL",  s.v[0], '\0');
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* Согласованность: genrnd_new(l, t) == genrnd(FS2(), l, t) */
Test(fs2_api, genrnd_new_matches_manual_init) {
    __fs2_checkreset();
    fs2 manual = FS2();
    fs2_genrnd(&manual, 20, '0');

    fs2 via = fs2_genrnd_new(20, '0');

    CHECK_EQ("same len",   manual.len, via.len);
    /* содержимое случайное — сравниваем только форму/валидность */
    CHECK_TRUE("manual 0-9", manual.v[0] >= '0' && manual.v[0] <= '9');
    CHECK_TRUE("via 0-9",    via.v[0]    >= '0' && via.v[0]    <= '9');
    CHECK_FS2VALID(1, &manual);
    CHECK_FS2VALID(2, &via);

    fs2_free(&manual);
    fs2_free(&via);
    assert_balanced();
}
