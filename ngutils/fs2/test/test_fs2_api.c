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
