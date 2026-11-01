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
    CHECK_EQ(*fs2_get(&s, s.len), '\0');
    CHECK_PTR_EQ(fs2_get(&s, s.len), s.v + s.len);

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
    fs2 s = FS2EMPTY;   /* sz=1, v="", STATIC */

    char *p = fs2_elem(&s, 0);
    *p = 'x';

    CHECK_TRUE ("now ALLOC",  fs2_is_alloc(&s));
    CHECK_FALSE("no STATIC",  fs2_is_static(&s));
    CHECK_TRUE ("grew",       s.sz >= 2);   /* 'x' + NUL */
    CHECK_EQ   ("v[0]",       s.v[0], 'x');
    CHECK_EQ   ("v[1] NUL",   s.v[1], '\0');   /* set by fs2_clear in reserve */
    CHECK_EQ   ("len untouched (raw writer)", s.len, 0);

    fs2_setlen(&s, 1);
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
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* Демонстрация: если бы был +1, этот вызов был бы no-op → setlen aborts */
Test(fs2, elem_penultimate_byte_no_grow) {
    __fs2_checkreset();
    fs2 s = fs2_init(16);
    size_t sz_before = s.sz;

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
