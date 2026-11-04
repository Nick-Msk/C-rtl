#include <criterion/criterion.h>

#include "test_fs2.h"
#include "fs2_api.h"
#include <locale.h>

TestSuite(fs2_api_modificators, .init = test_setup, .fini = test_teardown);

/* =========================================================================
 * fs2_replace — nominal (ALLOC source)
 * ========================================================================= */

Test(fs2_api_modificators, replace_single) {
    __fs2_checkreset();
    fs2 s    = fs2_fromstr("hello world");
    fs2 pat  = fs2_fromstr("world");
    fs2 repl = fs2_fromstr("C");

    fs2 *r = fs2_replace(&s, &pat, &repl);

    CHECK_STR_EQ("hello C", s.v, "hello C");
    CHECK_PTR_EQ("returns s", r, &s);
    CHECK_FS2VALID(1, &s);
    CHECK_FS2VALID(2, &pat);
    CHECK_FS2VALID(3, &repl);

    fs2_free(&s);
    fs2_free(&pat);
    fs2_free(&repl);
    assert_balanced();
}

Test(fs2_api_modificators, replace_multiple) {
    __fs2_checkreset();
    fs2 s    = fs2_fromstr("one apple, two apples, three apple");
    fs2 pat  = fs2_fromstr("apple");
    fs2 repl = fs2_fromstr("banana");

    fs2_replace(&s, &pat, &repl);

    CHECK_STR_EQ("one banana, two bananas, three banana",
                 s.v, "one banana, two bananas, three banana");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    fs2_free(&pat);
    fs2_free(&repl);
    assert_balanced();
}

Test(fs2_api_modificators, replace_at_start) {
    __fs2_checkreset();
    fs2 s    = fs2_fromstr("xxxabc");
    fs2 pat  = fs2_fromstr("xxx");
    fs2 repl = fs2_fromstr("y");

    fs2_replace(&s, &pat, &repl);
    CHECK_STR_EQ("yabc", s.v, "yabc");

    fs2_free(&s);
    fs2_free(&pat);
    fs2_free(&repl);
    assert_balanced();
}

Test(fs2_api_modificators, replace_at_end) {
    __fs2_checkreset();
    fs2 s    = fs2_fromstr("abcxxx");
    fs2 pat  = fs2_fromstr("xxx");
    fs2 repl = fs2_fromstr("y");

    fs2_replace(&s, &pat, &repl);
    CHECK_STR_EQ("abcy", s.v, "abcy");

    fs2_free(&s);
    fs2_free(&pat);
    fs2_free(&repl);
    assert_balanced();
}

/* =========================================================================
 * fs2_replace — Python semantics for overlapping
 * ========================================================================= */

Test(fs2_api_modificators, replace_overlapping_aaa_to_ba) {
    __fs2_checkreset();
    fs2 s    = fs2_fromstr("aaa");
    fs2 pat  = fs2_fromstr("aa");
    fs2 repl = fs2_fromstr("b");

    fs2_replace(&s, &pat, &repl);

    /* Python: "aaa".replace("aa", "b") == "ba" (one match at 0, then pos=1) */
    CHECK_STR_EQ("ba", s.v, "ba");
    CHECK_EQ  ("len",   s.len, 2);

    fs2_free(&s);
    fs2_free(&pat);
    fs2_free(&repl);
    assert_balanced();
}

Test(fs2_api_modificators, replace_overlapping_aaaa_to_bb) {
    __fs2_checkreset();
    fs2 s    = fs2_fromstr("aaaa");
    fs2 pat  = fs2_fromstr("aa");
    fs2 repl = fs2_fromstr("X");

    fs2_replace(&s, &pat, &repl);

    /* Python: "aaaa".replace("aa", "X") == "XX" */
    CHECK_STR_EQ("XX", s.v, "XX");
    CHECK_EQ  ("len",   s.len, 2);

    fs2_free(&s);
    fs2_free(&pat);
    fs2_free(&repl);
    assert_balanced();
}

Test(fs2_api_modificators, replace_sequential_multi) {
    __fs2_checkreset();
    fs2 s    = fs2_fromstr("abcabcabc");
    fs2 pat  = fs2_fromstr("abc");
    fs2 repl = fs2_fromstr("X");

    fs2_replace(&s, &pat, &repl);

    CHECK_STR_EQ("XXX", s.v, "XXX");
    fs2_free(&s);
    fs2_free(&pat);
    fs2_free(&repl);
    assert_balanced();
}

/* =========================================================================
 * fs2_replace — grow / shrink / identity
 * ========================================================================= */

Test(fs2_api_modificators, replace_grow) {
    __fs2_checkreset();
    fs2 s    = fs2_fromstr("aXbXc");
    fs2 pat  = fs2_fromstr("X");
    fs2 repl = fs2_fromstr("YZW");

    fs2_replace(&s, &pat, &repl);

    CHECK_STR_EQ("aYZWbYZWc", s.v, "aYZWbYZWc");
    CHECK_EQ  ("len",         s.len, 9);

    fs2_free(&s);
    fs2_free(&pat);
    fs2_free(&repl);
    assert_balanced();
}

Test(fs2_api_modificators, replace_shrink) {
    __fs2_checkreset();
    fs2 s    = fs2_fromstr("aXYZbXYZc");
    fs2 pat  = fs2_fromstr("XYZ");
    fs2 repl = fs2_fromstr("Q");

    fs2_replace(&s, &pat, &repl);

    CHECK_STR_EQ("aQbQc", s.v, "aQbQc");
    CHECK_EQ  ("len",     s.len, 5);

    fs2_free(&s);
    fs2_free(&pat);
    fs2_free(&repl);
    assert_balanced();
}

Test(fs2_api_modificators, replace_delete) {
    __fs2_checkreset();
    fs2 s    = fs2_fromstr("aXbXcXd");
    fs2 pat  = fs2_fromstr("X");
    fs2 repl = FS2EMPTY;                /* empty = delete */

    fs2_replace(&s, &pat, &repl);

    CHECK_STR_EQ("abcd", s.v, "abcd");
    CHECK_EQ  ("len",    s.len, 4);

    fs2_free(&s);
    fs2_free(&pat);
    assert_balanced();
}

Test(fs2_api_modificators, replace_identity_no_change) {
    __fs2_checkreset();
    fs2 s    = fs2_fromstr("abcabc");
    fs2 pat  = fs2_fromstr("a");
    fs2 repl = fs2_fromstr("a");        /* same content — no visible change */

    fs2_replace(&s, &pat, &repl);

    CHECK_STR_EQ("abcabc", s.v, "abcabc");
    CHECK_EQ  ("len",      s.len, 6);

    fs2_free(&s);
    fs2_free(&pat);
    fs2_free(&repl);
    assert_balanced();
}

/* =========================================================================
 * fs2_replace — no-op cases
 * ========================================================================= */

Test(fs2_api_modificators, replace_not_found) {
    __fs2_checkreset();
    fs2 s    = fs2_fromstr("hello");
    fs2 pat  = fs2_fromstr("zzz");
    fs2 repl = fs2_fromstr("X");
    void *orig = s.v;

    fs2_replace(&s, &pat, &repl);

    CHECK_PTR_EQ("buffer unchanged", s.v, orig);
    CHECK_STR_EQ("hello",            s.v, "hello");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    fs2_free(&pat);
    fs2_free(&repl);
    assert_balanced();
}

Test(fs2_api_modificators, replace_pat_longer_than_src) {
    __fs2_checkreset();
    fs2 s    = fs2_fromstr("ab");
    fs2 pat  = fs2_fromstr("abc");
    fs2 repl = fs2_fromstr("X");

    fs2_replace(&s, &pat, &repl);
    CHECK_STR_EQ("ab", s.v, "ab");

    fs2_free(&s);
    fs2_free(&pat);
    fs2_free(&repl);
    assert_balanced();
}

Test(fs2_api_modificators, replace_empty_pat_noop) {
    __fs2_checkreset();
    fs2 s    = fs2_fromstr("hello");
    fs2 pat  = FS2EMPTY;
    fs2 repl = fs2_fromstr("X");

    fs2_replace(&s, &pat, &repl);
    CHECK_STR_EQ("hello", s.v, "hello");

    fs2_free(&s);
    fs2_free(&repl);
    assert_balanced();
}

Test(fs2_api_modificators, replace_empty_src_fs2empty) {
    __fs2_checkreset();
    fs2 s    = FS2EMPTY;
    fs2 pat  = fs2_fromstr("a");
    fs2 repl = fs2_fromstr("b");

    fs2_replace(&s, &pat, &repl);
    CHECK_TRUE("still empty", fs2_eq_empty(&s));

    fs2_free(&pat);
    fs2_free(&repl);
    assert_balanced();
}

Test(fs2_api_modificators, replace_empty_src_canon) {
    __fs2_checkreset();
    fs2 s    = FS2();
    fs2 pat  = fs2_fromstr("a");
    fs2 repl = fs2_fromstr("b");

    fs2_replace(&s, &pat, &repl);
    CHECK_TRUE("still canon", fs2_is_canon(&s));

    fs2_free(&s);
    fs2_free(&pat);
    fs2_free(&repl);
    assert_balanced();
}

/* =========================================================================
 * fs2_replace — STATIC source
 *   Empty STATIC: early return (pat->len > 0 = s->len)
 *   Non-empty STATIC: mutation attempt -> SIGABRT via fs2_selfmove
 * ========================================================================= */

Test(fs2_api_modificators, replace_static_empty_src_noop) {
    __fs2_checkreset();
    fs2 s    = FS2EMPTY;
    fs2 pat  = fs2_fromstr("a");
    fs2 repl = fs2_fromstr("b");

    fs2_replace(&s, &pat, &repl);

    CHECK_TRUE("unchanged", fs2_eq_empty(&s));
    CHECK_FS2VALID(1, &s);

    fs2_free(&pat);
    fs2_free(&repl);
    assert_balanced();
}

Test(fs2_api_modificators, replace_static_nonempty_src_aborts, .signal = SIGABRT) {
    fs2 s    = FS2LITERAL("abc");
    fs2 pat  = fs2_fromstr("b");
    fs2 repl = fs2_fromstr("X");
    fs2_replace(&s, &pat, &repl);
    fs2_free(&pat);
    fs2_free(&repl);
}

/* =========================================================================
 * fs2_replace — LOCAL source
 *   Shrink / equal length: works (no realloc needed)
 *   Grow within capacity:  works
 *   Grow beyond capacity:  SIGABRT (LOCAL cannot grow)
 *
 * (Strictly, the docstring says @pre fs2_is_alloc(s); these tests pin down
 *  the actual behaviour so a regression is visible.)
 * ========================================================================= */

Test(fs2_api_modificators, replace_local_shrink_works) {
    __fs2_checkreset();
    FS2LOCAL(buf, 16);
    fs2_memcpy_str(&buf, 0, "aXbXc", 5);
    fs2_setlen(&buf, 5);

    fs2 pat  = fs2_fromstr("X");
    fs2 repl = FS2EMPTY;

    fs2_replace(&buf, &pat, &repl);

    CHECK_STR_EQ("abc",        buf.v, "abc");
    CHECK_TRUE ("still LOCAL", fs2_is_local(&buf));
    CHECK_FS2VALID(1, &buf);

    fs2_free(&pat);
    assert_balanced();
}

Test(fs2_api_modificators, replace_local_grow_within_capacity_works) {
    __fs2_checkreset();
    FS2LOCAL(buf, 16);
    fs2_memcpy_str(&buf, 0, "abc", 3);
    fs2_setlen(&buf, 3);

    fs2 pat  = fs2_fromstr("b");
    fs2 repl = fs2_fromstr("XYZ");

    fs2_replace(&buf, &pat, &repl);

    CHECK_STR_EQ("aXYZc",      buf.v, "aXYZc");
    CHECK_TRUE ("still LOCAL", fs2_is_local(&buf));
    CHECK_FS2VALID(1, &buf);

    fs2_free(&pat);
    fs2_free(&repl);
    assert_balanced();
}

Test(fs2_api_modificators, replace_local_grow_beyond_capacity_aborts, .signal = SIGABRT) {
    FS2LOCAL(buf, 8);
    fs2_memcpy_str(&buf, 0, "abcdefg", 7);   /* len=7, sz=8 */
    fs2_setlen(&buf, 7);

    fs2 pat  = fs2_fromstr("a");
    fs2 repl = fs2_fromstr("abcdefghij");    /* need far more than 8 */

    fs2_replace(&buf, &pat, &repl);
    fs2_free(&pat);
    fs2_free(&repl);
}

/* =========================================================================
 * fs2_replace — guards (invraise -> SIGABRT in debug)
 * ========================================================================= */

Test(fs2_api_modificators, replace_null_s_aborts, .signal = SIGABRT) {
    fs2 pat  = fs2_fromstr("a");
    fs2 repl = fs2_fromstr("b");
    fs2_replace(NULL, &pat, &repl);
    fs2_free(&pat);
    fs2_free(&repl);
}

Test(fs2_api_modificators, replace_null_pat_aborts, .signal = SIGABRT) {
    fs2 s    = fs2_fromstr("abc");
    fs2 repl = fs2_fromstr("b");
    fs2_replace(&s, NULL, &repl);
    fs2_free(&s);
    fs2_free(&repl);
}

Test(fs2_api_modificators, replace_null_repl_aborts, .signal = SIGABRT) {
    fs2 s   = fs2_fromstr("abc");
    fs2 pat = fs2_fromstr("b");
    fs2_replace(&s, &pat, NULL);
    fs2_free(&s);
    fs2_free(&pat);
}

/* =========================================================================
 * fs2_replace — pattern/repl untouched, s buffer grown as needed
 * ========================================================================= */

Test(fs2_api_modificators, replace_pat_and_repl_untouched) {
    __fs2_checkreset();
    fs2 s    = fs2_fromstr("aXbXc");
    fs2 pat  = fs2_fromstr("X");
    fs2 repl = fs2_fromstr("QQ");
    void *pat_v  = pat.v;
    void *repl_v = repl.v;

    fs2_replace(&s, &pat, &repl);

    CHECK_PTR_EQ("pat.v unchanged",  pat.v,  pat_v);
    CHECK_PTR_EQ("repl.v unchanged", repl.v, repl_v);
    CHECK_STR_EQ("pat content",      pat.v,  "X");
    CHECK_STR_EQ("repl content",     repl.v, "QQ");
    CHECK_FS2VALID(1, &pat);
    CHECK_FS2VALID(2, &repl);

    fs2_free(&s);
    fs2_free(&pat);
    fs2_free(&repl);
    assert_balanced();
}

Test(fs2_api_modificators, replace_fs2empty_repl_deletes) {
    __fs2_checkreset();
    fs2 s    = fs2_fromstr("aXXbXXc");
    fs2 pat  = fs2_fromstr("XX");
    fs2 repl = FS2EMPTY;                     /* v="" (STATIC) */

    fs2_replace(&s, &pat, &repl);
    CHECK_STR_EQ("abc", s.v, "abc");

    fs2_free(&s);
    fs2_free(&pat);
    assert_balanced();
}

Test(fs2_api_modificators, replace_canon_repl_deletes) {
    __fs2_checkreset();
    fs2 s    = fs2_fromstr("aXXbXXc");
    fs2 pat  = fs2_fromstr("XX");
    fs2 repl = FS2();                        /* v=NULL, len=0 — skip memcpy */

    fs2_replace(&s, &pat, &repl);
    CHECK_STR_EQ("abc", s.v, "abc");

    fs2_free(&s);
    fs2_free(&pat);
    fs2_free(&repl);
    assert_balanced();
}

/* =========================================================================
 * fs2_replace — combo matrix: {ALLOC, STATIC, FS2EMPTY, FS2()} for pat/repl
 *   s = ALLOC "aXbXc"; pat represents "X"; repl represents "QQ"
 *   Expected: "aQQbQQc" for every non-empty flavor; no-op for empty flavors
 * ========================================================================= */

Test(fs2_api_modificators, replace_pat_repl_flavor_matrix) {
    __fs2_checkreset();

    /* Build every flavor of "X" once, reuse across iterations. */
    fs2 pat_alloc = fs2_fromstr("X");
    fs2 pat_static = FS2LITERAL("X");
    fs2 pat_local;
    FS2LOCAL(pat_local_buf, 8);
    fs2_memcpy_str(&pat_local_buf, 0, "X", 1);
    fs2_setlen(&pat_local_buf, 1);
    pat_local = pat_local_buf;

    fs2 repl_alloc  = fs2_fromstr("QQ");
    fs2 repl_static = FS2LITERAL("QQ");
    FS2LOCAL(repl_local_buf, 8);
    fs2_memcpy_str(&repl_local_buf, 0, "QQ", 2);
    fs2_setlen(&repl_local_buf, 2);

    struct { const fs2 *pat; const fs2 *repl; const char *label; } cases[] = {
        { &pat_alloc,  &repl_alloc,  "alloc × alloc"  },
        { &pat_alloc,  &repl_static, "alloc × static" },
        { &pat_static, &repl_alloc,  "static × alloc" },
        { &pat_static, &repl_static, "static × static"},
        { &pat_local,  &repl_alloc,  "local × alloc"  },
        { &pat_alloc,  &repl_local_buf, "alloc × local" },
    };

    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        fs2 s = fs2_fromstr("aXbXc");
        fs2_replace(&s, cases[i].pat, cases[i].repl);
        CHECK_STR_EQ(cases[i].label, s.v, "aQQbQQc");
        CHECK_FS2VALID(1, &s);
        fs2_free(&s);
    }

    fs2_free(&pat_alloc);
    fs2_free(&repl_alloc);
    assert_balanced();
}

/* =========================================================================
 * pat is one of the empty flavors -> no-op (early return)
 * ========================================================================= */

Test(fs2_api_modificators, replace_empty_pat_flavors_noop) {
    __fs2_checkreset();

    fs2 pat_canon  = FS2();     /* v = NULL, len = 0 */
    fs2 pat_empty  = FS2EMPTY;  /* v = "",   len = 0 */
    fs2 repl       = fs2_fromstr("QQ");

    const fs2 *pats[] = { &pat_canon, &pat_empty };

    for (size_t i = 0; i < 2; i++) {
        fs2 s = fs2_fromstr("aXbXc");
        void *orig = s.v;
        fs2_replace(&s, pats[i], &repl);
        CHECK_PTR_EQ("buffer untouched", s.v, orig);
        CHECK_STR_EQ("no-op",            s.v, "aXbXc");
        fs2_free(&s);
    }

    fs2_free(&pat_canon);
    fs2_free(&repl);
    assert_balanced();
}

/* =========================================================================
 * s is one of the empty flavors -> always no-op, form preserved
 * ========================================================================= */

Test(fs2_api_modificators, replace_empty_src_flavors_stay_empty) {
    __fs2_checkreset();

    fs2 pat  = fs2_fromstr("X");
    fs2 repl = fs2_fromstr("QQ");

    /* FS2() (v == NULL): pat->len > s->len=0 -> early return; remains canonical */
    fs2 s_canon = FS2();
    fs2_replace(&s_canon, &pat, &repl);
    CHECK_TRUE ("still canon", fs2_is_canon(&s_canon));
    CHECK_TRUE ("still empty", fs2_is_empty(&s_canon));
    fs2_free(&s_canon);

    /* FS2EMPTY (v == ""): same, remains FS2EMPTY */
    fs2 s_empty = FS2EMPTY;
    fs2_replace(&s_empty, &pat, &repl);
    CHECK_TRUE("still eq_empty", fs2_eq_empty(&s_empty));
    CHECK_TRUE("still STATIC",   fs2_is_static(&s_empty));
    CHECK_FS2VALID(1, &s_empty);

    fs2_free(&pat);
    fs2_free(&repl);
    assert_balanced();
}

/* Empty-src × empty-pat also no-op (two empties, still empty) */
Test(fs2_api_modificators, replace_empty_src_empty_pat_noop) {
    __fs2_checkreset();

    fs2 pat_canon = FS2();
    fs2 pat_empty = FS2EMPTY;
    fs2 repl      = fs2_fromstr("QQ");

    const fs2 *pats[] = { &pat_canon, &pat_empty };

    for (size_t i = 0; i < 2; i++) {
        fs2 s_canon = FS2();
        fs2_replace(&s_canon, pats[i], &repl);
        CHECK_TRUE("canon stays canon", fs2_is_canon(&s_canon));
        fs2_free(&s_canon);

        fs2 s_empty = FS2EMPTY;
        fs2_replace(&s_empty, pats[i], &repl);
        CHECK_TRUE("empty stays empty", fs2_eq_empty(&s_empty));
    }

    fs2_free(&pat_canon);
    fs2_free(&repl);
    assert_balanced();
}

/* =========================================================================
 * repl is one of the empty flavors -> delete (skip memcpy, incl. v==NULL)
 * ========================================================================= */

Test(fs2_api_modificators, replace_empty_repl_flavors_delete) {
    __fs2_checkreset();

    fs2 pat      = fs2_fromstr("XX");
    fs2 repl_canon = FS2();     /* v == NULL -> memcpy must be skipped */
    fs2 repl_empty = FS2EMPTY;  /* v == ""   */

    const fs2 *repls[] = { &repl_canon, &repl_empty };

    for (size_t i = 0; i < 2; i++) {
        fs2 s = fs2_fromstr("aXXbXXc");
        fs2_replace(&s, &pat, repls[i]);
        CHECK_STR_EQ("deleted", s.v, "abc");
        CHECK_FS2VALID(1, &s);
        fs2_free(&s);
    }

    fs2_free(&pat);
    fs2_free(&repl_canon);
    assert_balanced();
}

/* =========================================================================
 * pat and repl are the same byte sequence in different flavors
 *   (self-replace in spirit, but distinct objects — no restrict violation)
 * ========================================================================= */

Test(fs2_api_modificators, replace_pat_repl_equal_different_objects) {
    __fs2_checkreset();

    fs2 s         = fs2_fromstr("abcabc");
    fs2 pat_alloc = fs2_fromstr("abc");
    fs2 repl_lit  = FS2LITERAL("abc");

    fs2_replace(&s, &pat_alloc, &repl_lit);
    CHECK_STR_EQ("abcabc", s.v, "abcabc");   /* byte-identical, no visible change */

    fs2_free(&s);
    fs2_free(&pat_alloc);
    assert_balanced();
}

/* =========================================================================
 * LOCAL dst (for completeness) — shrink only, grow-within-cap
 * ========================================================================= */

Test(fs2_api_modificators, replace_local_dst_matrix) {
    __fs2_checkreset();

    fs2 pat  = fs2_fromstr("X");
    fs2 repl = fs2_fromstr("Q");

    /* equal-length replace: LOCAL untouched in size */
    FS2LOCAL(b1, 16);
    fs2_memcpy_str(&b1, 0, "aXbXc", 5);
    fs2_setlen(&b1, 5);
    fs2_replace(&b1, &pat, &repl);
    CHECK_STR_EQ("aQbQc", b1.v, "aQbQc");
    CHECK_TRUE ("still LOCAL", fs2_is_local(&b1));
    CHECK_FS2VALID(1, &b1);

    /* shrink: delete X */
    FS2LOCAL(b2, 16);
    fs2_memcpy_str(&b2, 0, "aXbXc", 5);
    fs2_setlen(&b2, 5);
    fs2 repl_empty = FS2EMPTY;
    fs2_replace(&b2, &pat, &repl_empty);
    CHECK_STR_EQ("abc", b2.v, "abc");
    CHECK_TRUE ("still LOCAL", fs2_is_local(&b2));
    CHECK_FS2VALID(1, &b2);

    fs2_free(&pat);
    fs2_free(&repl);
    assert_balanced();
}

/* =========================================================================
 * fs2_replace_fromstr — NULL repl -> _fs2_wrap(NULL) -> FS2EMPTY -> delete
 *   (unlike fs2_replace(&s, pat, NULL) which aborts)
 * ========================================================================= */

Test(fs2_api_modificators, replace_fromstr_null_repl_deletes) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("aXbXc");
    fs2 pat = fs2_fromstr("X");

    /* NULL repl -> empty -> delete */
    fs2_replace_fromstr(&s, &pat, NULL);

    CHECK_STR_EQ("abc", s.v, "abc");
    CHECK_EQ  ("len",   s.len, 3);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    fs2_free(&pat);
    assert_balanced();
}

Test(fs2_api_modificators, replace_fromstr_empty_str_repl_deletes) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("aXbXc");
    fs2 pat = fs2_fromstr("X");

    fs2_replace_fromstr(&s, &pat, "");

    CHECK_STR_EQ("abc", s.v, "abc");

    fs2_free(&s);
    fs2_free(&pat);
    assert_balanced();
}

/* Both NULL and "" go through _fs2_wrap -> FS2EMPTY -> same result */
Test(fs2_api_modificators, replace_fromstr_null_and_empty_repl_same) {
    __fs2_checkreset();
    fs2 pat = fs2_fromstr("X");

    fs2 a = fs2_fromstr("aXbXc");
    fs2 b = fs2_fromstr("aXbXc");

    fs2_replace_fromstr(&a, &pat, NULL);
    fs2_replace_fromstr(&b, &pat, "");

    CHECK_STR_EQ("null repl",  a.v, "abc");
    CHECK_STR_EQ("empty repl", b.v, "abc");
    CHECK_EQ  ("same len",     a.len, b.len);

    fs2_free(&a);
    fs2_free(&b);
    fs2_free(&pat);
    assert_balanced();
}

/* Non-NULL repl — normal replace */
Test(fs2_api_modificators, replace_fromstr_normal) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("aXbXc");
    fs2 pat = fs2_fromstr("X");

    fs2_replace_fromstr(&s, &pat, "QQ");

    CHECK_STR_EQ("aQQbQQc", s.v, "aQQbQQc");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    fs2_free(&pat);
    assert_balanced();
}

/* =========================================================================
 * fs2_replace_fromstr — CONTRAST: NULL pat still aborts (pat non-lenient)
 * ========================================================================= */

Test(fs2_api_modificators, replace_fromstr_null_pat_aborts, .signal = SIGABRT) {
    fs2 s = fs2_fromstr("abc");
    fs2_replace_fromstr(&s, NULL, "X");
    fs2_free(&s);
}

Test(fs2_api_modificators, replace_fromstr_null_s_aborts, .signal = SIGABRT) {
    fs2 pat = fs2_fromstr("a");
    fs2_replace_fromstr(NULL, &pat, "X");
    fs2_free(&pat);
}

/* =========================================================================
 * fs2_replace_cstr — BOTH args lenient (pat AND repl via _fs2_wrap)
 * ========================================================================= */

Test(fs2_api_modificators, replace_cstr_normal) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("one apple, two apple");

    fs2_replace_cstr(&s, "apple", "banana");

    CHECK_STR_EQ("one banana, two banana", s.v, "one banana, two banana");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* NULL pat -> _fs2_wrap(NULL) -> FS2EMPTY -> fs2_replace early-returns (no-op) */
Test(fs2_api_modificators, replace_cstr_null_pat_noop) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    fs2_replace_cstr(&s, NULL, "X");

    CHECK_STR_EQ("hello", s.v, "hello");   /* pat empty -> no-op */

    fs2_free(&s);
    assert_balanced();
}

/* NULL repl -> _fs2_wrap(NULL) -> FS2EMPTY -> delete */
Test(fs2_api_modificators, replace_cstr_null_repl_deletes) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("aXbXc");

    fs2_replace_cstr(&s, "X", NULL);

    CHECK_STR_EQ("abc", s.v, "abc");

    fs2_free(&s);
    assert_balanced();
}

/* Both NULL -> both wrapped as FS2EMPTY -> pat empty -> no-op */
Test(fs2_api_modificators, replace_cstr_both_null_noop) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    fs2_replace_cstr(&s, NULL, NULL);

    CHECK_STR_EQ("hello", s.v, "hello");

    fs2_free(&s);
    assert_balanced();
}

/* Both "" — pat empty -> no-op */
Test(fs2_api_modificators, replace_cstr_both_empty_noop) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    fs2_replace_cstr(&s, "", "");

    CHECK_STR_EQ("hello", s.v, "hello");

    fs2_free(&s);
    assert_balanced();
}

/* Only dst NULL aborts (invraise in fs2_replace) */
Test(fs2_api_modificators, replace_cstr_null_s_aborts, .signal = SIGABRT) {
    fs2_replace_cstr(NULL, "a", "b");
}

/* =========================================================================
 * Consistency: fromstr ≡ replace with explicit FS2EMPTY for NULL repl
 * ========================================================================= */

Test(fs2_api_modificators, replace_fromstr_null_equiv_to_explicit_empty) {
    __fs2_checkreset();
    fs2 pat  = fs2_fromstr("X");
    fs2 empty = FS2EMPTY;

    fs2 a = fs2_fromstr("aXbXc");
    fs2 b = fs2_fromstr("aXbXc");

    fs2_replace_fromstr(&a, &pat, NULL);
    fs2_replace(&b, &pat, &empty);

    CHECK_STR_EQ("same result",  a.v, b.v);
    CHECK_EQ  ("same len",       a.len, b.len);
    CHECK_FS2VALID(1, &a);
    CHECK_FS2VALID(2, &b);

    fs2_free(&a);
    fs2_free(&b);
    fs2_free(&pat);
    assert_balanced();
}

/* =========================================================================
 * fs2_trim / fs2_ltrim / fs2_rtrim — nominal
 * ========================================================================= */

Test(fs2_api_modificators, trim_basic_both) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("  hello  ");
    fs2 pat = fs2_fromstr(" ");

    fs2 *r = fs2_trim(&s, &pat);

    CHECK_STR_EQ("hello", s.v, "hello");
    CHECK_EQ  ("len",     s.len, 5);
    CHECK_PTR_EQ("chaining", r, &s);
    CHECK_FS2VALID(1, &s);
    CHECK_FS2VALID(2, &pat);

    fs2_free(&s);
    fs2_free(&pat);
    assert_balanced();
}

Test(fs2_api_modificators, trim_left_only) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("  hello  ");
    fs2 pat = fs2_fromstr(" ");

    fs2_ltrim(&s, &pat);

    CHECK_STR_EQ("hello  ", s.v, "hello  ");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    fs2_free(&pat);
    assert_balanced();
}

Test(fs2_api_modificators, trim_right_only) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("  hello  ");
    fs2 pat = fs2_fromstr(" ");

    fs2_rtrim(&s, &pat);

    CHECK_STR_EQ("  hello", s.v, "  hello");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    fs2_free(&pat);
    assert_balanced();
}

/* =========================================================================
 * Multichar set pattern — set semantics, not substring
 * ========================================================================= */

Test(fs2_api_modificators, trim_multichar_set) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("xyxhellozyx");   /* only x/y/z get trimmed */
    fs2 pat = fs2_fromstr("xyz");

    fs2_trim(&s, &pat);

    CHECK_STR_EQ("hello", s.v, "hello");

    fs2_free(&s);
    fs2_free(&pat);
    assert_balanced();
}

Test(fs2_api_modificators, trim_pattern_with_duplicates) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("xxhelloxx");
    fs2 pat = fs2_fromstr("xx");            /* still set {x} */

    fs2_trim(&s, &pat);

    CHECK_STR_EQ("hello", s.v, "hello");

    fs2_free(&s);
    fs2_free(&pat);
    assert_balanced();
}

/* Mixed content: pattern chars interleaved with kept content */
Test(fs2_api_modificators, trim_mixed) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("xax");           /* only edge x's trimmed */
    fs2 pat = fs2_fromstr("x");

    fs2_trim(&s, &pat);

    CHECK_STR_EQ("a", s.v, "a");

    fs2_free(&s);
    fs2_free(&pat);
    assert_balanced();
}

/* =========================================================================
 * Full-trim-to-empty (three different code paths)
 * ========================================================================= */

Test(fs2_api_modificators, trim_all_left_path) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("xxxx");
    fs2 pat = fs2_fromstr("x");

    /* left path: start == end -> setlen(0) early */
    fs2_ltrim(&s, &pat);

    CHECK_EQ  ("len",  s.len, 0);
    CHECK_TRUE("empty", fs2_is_empty(&s));

    fs2_free(&s);
    fs2_free(&pat);
    assert_balanced();
}

Test(fs2_api_modificators, trim_all_right_path) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("xxxx");
    fs2 pat = fs2_fromstr("x");

    /* right path: end decreases to 0, no selfmove */
    fs2_rtrim(&s, &pat);

    CHECK_EQ  ("len",  s.len, 0);
    CHECK_TRUE("empty", fs2_is_empty(&s));

    fs2_free(&s);
    fs2_free(&pat);
    assert_balanced();
}

Test(fs2_api_modificators, trim_all_both_path) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("xxxx");
    fs2 pat = fs2_fromstr("x");

    fs2_trim(&s, &pat);

    CHECK_EQ  ("len",  s.len, 0);
    CHECK_TRUE("empty", fs2_is_empty(&s));

    fs2_free(&s);
    fs2_free(&pat);
    assert_balanced();
}

/* =========================================================================
 * No-op cases (buffer must not be touched — pointer stability)
 * ========================================================================= */

Test(fs2_api_modificators, trim_empty_pattern_noop) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("hello");
    fs2 pat = FS2EMPTY;
    void *orig = s.v;

    fs2_trim(&s, &pat);

    CHECK_PTR_EQ("buffer untouched", s.v, orig);
    CHECK_STR_EQ("hello",            s.v, "hello");

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_modificators, trim_canon_pattern_noop) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("hello");
    fs2 pat = FS2();

    fs2_trim(&s, &pat);

    CHECK_STR_EQ("hello", s.v, "hello");

    fs2_free(&s);
    fs2_free(&pat);
    assert_balanced();
}

Test(fs2_api_modificators, trim_pattern_no_match_noop) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("hello");
    fs2 pat = fs2_fromstr("xyz");
    void *orig = s.v;

    fs2_trim(&s, &pat);

    CHECK_PTR_EQ("buffer untouched", s.v, orig);
    CHECK_STR_EQ("hello",            s.v, "hello");

    fs2_free(&s);
    fs2_free(&pat);
    assert_balanced();
}

/* =========================================================================
 * Empty s (FS2EMPTY / FS2()) — early return, form preserved
 * ========================================================================= */

Test(fs2_api_modificators, trim_fs2empty_s_stays_empty) {
    __fs2_checkreset();
    fs2 s   = FS2EMPTY;
    fs2 pat = fs2_fromstr("x");

    fs2_trim(&s, &pat);
    CHECK_TRUE("still eq_empty", fs2_eq_empty(&s));
    CHECK_FS2VALID(1, &s);

    fs2_free(&pat);
    assert_balanced();
}

Test(fs2_api_modificators, trim_canon_s_stays_canon) {
    __fs2_checkreset();
    fs2 s   = FS2();
    fs2 pat = fs2_fromstr("x");

    fs2_trim(&s, &pat);
    CHECK_TRUE("still canon", fs2_is_canon(&s));
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    fs2_free(&pat);
    assert_balanced();
}

/* =========================================================================
 * STATIC s — current behaviour matrix (lazy abort)
 *   no-match -> no-op; match -> abort; empty pattern -> no-op
 * ========================================================================= */

Test(fs2_api_modificators, trim_static_no_match_abort, .signal = SIGABRT) {
    __fs2_checkreset();
    fs2 s   = FS2LITERAL("hello");
    fs2 pat = fs2_fromstr("xyz");

    fs2_trim(&s, &pat);   /* strspn=0, no writes, setlen(len) is a no-op */

    CHECK_STR_EQ("hello", s.v, "hello");
    CHECK_TRUE("still STATIC", fs2_is_static(&s));
    CHECK_FS2VALID(1, &s);

    fs2_free(&pat);
    assert_balanced();
}

Test(fs2_api_modificators, trim_static_empty_pattern_abort, .signal = SIGABRT) {
    __fs2_checkreset();
    fs2 s   = FS2LITERAL("hello");
    fs2 pat = FS2EMPTY;

    fs2_trim(&s, &pat);   /* early return */

    CHECK_STR_EQ("hello", s.v, "hello");
    CHECK_FS2VALID(1, &s);

    assert_balanced();
}

Test(fs2_api_modificators, trim_static_full_match_aborts, .signal = SIGABRT) {
    fs2 s   = FS2LITERAL("   ");
    fs2 pat = fs2_fromstr(" ");
    fs2_trim(&s, &pat);   /* start == end -> fs2_setlen(0) -> abort */
    fs2_free(&pat);
}

Test(fs2_api_modificators, trim_static_partial_aborts, .signal = SIGABRT) {
    fs2 s   = FS2LITERAL("  hi  ");
    fs2 pat = fs2_fromstr(" ");
    fs2_trim(&s, &pat);   /* selfmove on STATIC -> abort */
    fs2_free(&pat);
}

/* =========================================================================
 * LOCAL s — only shrinks/keeps size, always fits
 * ========================================================================= */

Test(fs2_api_modificators, trim_local_shrinks) {
    __fs2_checkreset();
    FS2LOCAL(buf, 16);
    fs2_memcpy_str(&buf, 0, "  hello  ", 9);
    fs2_setlen(&buf, 9);
    fs2 pat = fs2_fromstr(" ");

    fs2_trim(&buf, &pat);

    CHECK_STR_EQ("hello",      buf.v, "hello");
    CHECK_TRUE ("still LOCAL", fs2_is_local(&buf));
    CHECK_FS2VALID(1, &buf);

    fs2_free(&pat);
    assert_balanced();
}

Test(fs2_api_modificators, trim_local_all_to_empty) {
    __fs2_checkreset();
    FS2LOCAL(buf, 8);
    fs2_memcpy_str(&buf, 0, "xxxx", 4);
    fs2_setlen(&buf, 4);
    fs2 pat = fs2_fromstr("x");

    fs2_trim(&buf, &pat);

    CHECK_EQ  ("len",  buf.len, 0);
    CHECK_TRUE("LOCAL", fs2_is_local(&buf));
    CHECK_FS2VALID(1, &buf);

    fs2_free(&pat);
    assert_balanced();
}

/* =========================================================================
 * Guards
 * ========================================================================= */

Test(fs2_api_modificators, trim_null_s_aborts, .signal = SIGABRT) {
    fs2 pat = fs2_fromstr("x");
    fs2_trim(NULL, &pat);
    fs2_free(&pat);
}

Test(fs2_api_modificators, trim_null_pat_aborts, .signal = SIGABRT) {
    fs2 s = fs2_fromstr("abc");
    fs2_trim(&s, NULL);
    fs2_free(&s);
}

/* =========================================================================
 * Pattern not touched
 * ========================================================================= */

Test(fs2_api_modificators, trim_pattern_untouched) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("  hi  ");
    fs2 pat = fs2_fromstr(" ");
    void *pat_v = pat.v;

    fs2_trim(&s, &pat);

    CHECK_PTR_EQ("pat.v unchanged", pat.v, pat_v);
    CHECK_STR_EQ("pat content",     pat.v, " ");
    CHECK_FS2VALID(1, &pat);

    fs2_free(&s);
    fs2_free(&pat);
    assert_balanced();
}

/* =========================================================================
 * Direct fs2_trim_common — both=false is a no-op
 * ========================================================================= */

Test(fs2_api_modificators, trim_common_neither_side_noop) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("  hi  ");
    fs2 pat = fs2_fromstr(" ");
    void *orig = s.v;

    fs2_trim_common(&s, &pat, false, false);

    CHECK_PTR_EQ("buffer untouched", s.v, orig);
    CHECK_STR_EQ("  hi  ",           s.v, "  hi  ");

    fs2_free(&s);
    fs2_free(&pat);
    assert_balanced();
}

/* =========================================================================
 * STATIC src — EAGER abort via fs2_cant_mutate
 *   Non-empty STATIC is always a programmer error (trimming a literal),
 *   regardless of whether the pattern matches anything.
 * ========================================================================= */

Test(fs2_api_modificators, trim_static_no_match_aborts, .signal = SIGABRT) {
    fs2 s   = FS2LITERAL("hello");
    fs2 pat = fs2_fromstr("xyz");
    fs2_trim(&s, &pat);       /* no writes would be needed, but still aborts */
    fs2_free(&pat);
}

Test(fs2_api_modificators, trim_static_empty_pattern_aborts, .signal = SIGABRT) {
    fs2 s   = FS2LITERAL("hello");
    fs2 pat = FS2EMPTY;
    fs2_trim(&s, &pat);       /* early return used to happen here — now aborts */
    fs2_free(&pat);
}

/* Also for ltrim / rtrim (same fs2_trim_common path) */
Test(fs2_api_modificators, trim_static_ltrim_aborts, .signal = SIGABRT) {
    fs2 s   = FS2LITERAL("  hi  ");
    fs2 pat = fs2_fromstr(" ");
    fs2_ltrim(&s, &pat);
    fs2_free(&pat);
}

Test(fs2_api_modificators, trim_static_rtrim_aborts, .signal = SIGABRT) {
    fs2 s   = FS2LITERAL("  hi  ");
    fs2 pat = fs2_fromstr(" ");
    fs2_rtrim(&s, &pat);
    fs2_free(&pat);
}

Test(fs2_api_modificators, trim_null_pattern_aborts, .signal = SIGABRT) {
    fs2 s = fs2_fromstr("abc");
    fs2_trim(&s, NULL);                       /* fs2-version: strict */
    fs2_free(&s);
}

/* =========================================================================
 * fs2_trim / fs2_ltrim / fs2_rtrim — flag dispatch
 *   Core logic is covered by fs2_trim_common tests; here we only verify
 *   that each wrapper passes the right (left, right) pair.
 * ========================================================================= */

Test(fs2_api_modificators, trim_wrapper_both_flags) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("  hi  ");
    fs2 pat = fs2_fromstr(" ");

    fs2 *r = fs2_trim(&s, &pat);

    CHECK_STR_EQ("hi",      s.v, "hi");   /* both sides trimmed */
    CHECK_PTR_EQ("chain",   r, &s);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    fs2_free(&pat);
    assert_balanced();
}

Test(fs2_api_modificators, ltrim_wrapper_left_only) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("  hi  ");
    fs2 pat = fs2_fromstr(" ");

    fs2 *r = fs2_ltrim(&s, &pat);

    CHECK_STR_EQ("hi  ",   s.v, "hi  ");   /* right side intact */
    CHECK_PTR_EQ("chain",  r, &s);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    fs2_free(&pat);
    assert_balanced();
}

Test(fs2_api_modificators, rtrim_wrapper_right_only) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("  hi  ");
    fs2 pat = fs2_fromstr(" ");

    fs2 *r = fs2_rtrim(&s, &pat);

    CHECK_STR_EQ("  hi",   s.v, "  hi");   /* left side intact */
    CHECK_PTR_EQ("chain",  r, &s);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    fs2_free(&pat);
    assert_balanced();
}

/* =========================================================================
 * Equivalence: each wrapper ≡ fs2_trim_common with explicit flags
 * ========================================================================= */

Test(fs2_api_modificators, trim_wrappers_match_common) {
    __fs2_checkreset();
    fs2 pat = fs2_fromstr(" ");

    fs2 a = fs2_fromstr("  hi  ");
    fs2 b = fs2_fromstr("  hi  ");
    fs2_trim(&a, &pat);
    fs2_trim_common(&b, &pat, true, true);
    CHECK_STR_EQ("trim == common(t,t)", a.v, b.v);
    fs2_free(&a);
    fs2_free(&b);

    fs2 c = fs2_fromstr("  hi  ");
    fs2 d = fs2_fromstr("  hi  ");
    fs2_ltrim(&c, &pat);
    fs2_trim_common(&d, &pat, true, false);
    CHECK_STR_EQ("ltrim == common(t,f)", c.v, d.v);
    fs2_free(&c);
    fs2_free(&d);

    fs2 e = fs2_fromstr("  hi  ");
    fs2 f = fs2_fromstr("  hi  ");
    fs2_rtrim(&e, &pat);
    fs2_trim_common(&f, &pat, false, true);
    CHECK_STR_EQ("rtrim == common(f,t)", e.v, f.v);
    fs2_free(&e);
    fs2_free(&f);

    fs2_free(&pat);
    assert_balanced();
}

/* =========================================================================
 * Guards inherited from fs2_trim_common
 * ========================================================================= */

Test(fs2_api_modificators, ltrim_null_aborts, .signal = SIGABRT) {
    fs2_ltrim(NULL, NULL);
}

Test(fs2_api_modificators, rtrim_null_aborts, .signal = SIGABRT) {
    fs2_rtrim(NULL, NULL);
}

/* =========================================================================
 * trim_str / ltrim_str / rtrim_str — nominal
 * ========================================================================= */

Test(fs2_api_modificators, trim_str_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("  hi  ");

    fs2 *r = fs2_trim_str(&s, " ");

    CHECK_STR_EQ("hi",    s.v, "hi");
    CHECK_PTR_EQ("chain", r, &s);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_modificators, ltrim_str_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("  hi  ");

    fs2_ltrim_str(&s, " ");

    CHECK_STR_EQ("hi  ", s.v, "hi  ");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_modificators, rtrim_str_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("  hi  ");

    fs2_rtrim_str(&s, " ");

    CHECK_STR_EQ("  hi", s.v, "  hi");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * NULL pattern — LENIENT via _fs2_wrap(NULL) -> FS2EMPTY -> no-op
 *   (contrast: fs2_trim(&s, NULL) aborts)
 * ========================================================================= */

Test(fs2_api_modificators, trim_str_null_pattern_noop) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("  hi  ");
    void *orig = s.v;

    fs2_trim_str(&s, NULL);

    CHECK_PTR_EQ("buffer untouched", s.v, orig);
    CHECK_STR_EQ("  hi  ",           s.v, "  hi  ");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_modificators, trim_str_null_pattern_noop2) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("  hi  ");
    fs2_trim_str(&s, NULL);
    CHECK_STR_EQ("  hi  ", s.v, "  hi  ");   /* NULL → FS2EMPTY → no-op */
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_modificators, ltrim_str_null_pattern_noop) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("  hi  ");

    fs2_ltrim_str(&s, NULL);

    CHECK_STR_EQ("  hi  ", s.v, "  hi  ");

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_modificators, rtrim_str_null_pattern_noop) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("  hi  ");

    fs2_rtrim_str(&s, NULL);

    CHECK_STR_EQ("  hi  ", s.v, "  hi  ");

    fs2_free(&s);
    assert_balanced();
}

/* "" pattern — same as NULL: _fs2_wrap("") -> FS2EMPTY -> no-op */
Test(fs2_api_modificators, trim_str_empty_pattern_noop) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("  hi  ");

    fs2_trim_str(&s, "");

    CHECK_STR_EQ("  hi  ", s.v, "  hi  ");

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * Contrast: fs2-version aborts on NULL pattern, _str-version does not
 * ========================================================================= */

Test(fs2_api_modificators, trim_null_str_pattern_aborts, .signal = SIGABRT) {
    fs2 s = fs2_fromstr("abc");
    fs2_trim(&s, NULL);                    /* fs2-version: strict */
    fs2_free(&s);
}

/* =========================================================================
 * Equivalence: _str == fs2 with explicit _fs2_wrap(pattern)
 * ========================================================================= */

Test(fs2_api_modificators, trim_str_matches_fs2_version) {
    __fs2_checkreset();
    fs2 pat = fs2_fromstr(" ");

    fs2 a = fs2_fromstr("  hi  ");
    fs2 b = fs2_fromstr("  hi  ");
    fs2_trim_str(&a, " ");
    fs2_trim(&b, &pat);
    CHECK_STR_EQ("trim_str == trim", a.v, b.v);
    fs2_free(&a);
    fs2_free(&b);

    fs2 c = fs2_fromstr("  hi  ");
    fs2 d = fs2_fromstr("  hi  ");
    fs2_ltrim_str(&c, " ");
    fs2_ltrim(&d, &pat);
    CHECK_STR_EQ("ltrim_str == ltrim", c.v, d.v);
    fs2_free(&c);
    fs2_free(&d);

    fs2 e = fs2_fromstr("  hi  ");
    fs2 f = fs2_fromstr("  hi  ");
    fs2_rtrim_str(&e, " ");
    fs2_rtrim(&f, &pat);
    CHECK_STR_EQ("rtrim_str == rtrim", e.v, f.v);
    fs2_free(&e);
    fs2_free(&f);

    fs2_free(&pat);
    assert_balanced();
}

/* =========================================================================
 * Multichar pattern
 * ========================================================================= */

Test(fs2_api_modificators, trim_str_multichar_set) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("xyzhelloxyz");

    fs2_trim_str(&s, "xyz");

    CHECK_STR_EQ("hello", s.v, "hello");

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * NULL s still aborts (inherited from fs2_trim_common via invfatal)
 * ========================================================================= */

Test(fs2_api_modificators, trim_str_null_s_aborts, .signal = SIGABRT) {
    fs2_trim_str(NULL, " ");
}

Test(fs2_api_modificators, ltrim_str_null_s_aborts, .signal = SIGABRT) {
    fs2_ltrim_str(NULL, " ");
}

Test(fs2_api_modificators, rtrim_str_null_s_aborts, .signal = SIGABRT) {
    fs2_rtrim_str(NULL, " ");
}

/* =========================================================================
 * fs2_starts_with — nominal
 * ========================================================================= */

Test(fs2_api_modificators, starts_with_match) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("hello world");
    fs2 p1  = fs2_fromstr("hello");
    fs2 p2  = fs2_fromstr("hello world");   /* full string */

    CHECK_TRUE("prefix at start", fs2_starts_with(&s, &p1));
    CHECK_TRUE("whole string",    fs2_starts_with(&s, &p2));

    fs2_free(&s);
    fs2_free(&p1);
    fs2_free(&p2);
    assert_balanced();
}

Test(fs2_api_modificators, starts_with_no_match) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("hello");
    fs2 p1  = fs2_fromstr("world");      /* different content */
    fs2 p2  = fs2_fromstr("Hello");      /* case differs */
    fs2 p3  = fs2_fromstr("ell");        /* matches inside, not at start */

    CHECK_FALSE("wrong content",  fs2_starts_with(&s, &p1));
    CHECK_FALSE("case mismatch",  fs2_starts_with(&s, &p2));
    CHECK_FALSE("not at start",   fs2_starts_with(&s, &p3));

    fs2_free(&s);
    fs2_free(&p1);
    fs2_free(&p2);
    fs2_free(&p3);
    assert_balanced();
}

Test(fs2_api_modificators, starts_with_prefix_longer) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("ab");
    fs2 p   = fs2_fromstr("abc");        /* len 3 > s->len 2 */

    CHECK_FALSE("prefix longer", fs2_starts_with(&s, &p));

    fs2_free(&s);
    fs2_free(&p);
    assert_balanced();
}

/* =========================================================================
 * fs2_ends_with — nominal
 * ========================================================================= */

Test(fs2_api_modificators, ends_with_match) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("hello world");
    fs2 s1  = fs2_fromstr("world");
    fs2 s2  = fs2_fromstr("hello world");   /* full string */

    CHECK_TRUE("suffix at end", fs2_ends_with(&s, &s1));
    CHECK_TRUE("whole string",  fs2_ends_with(&s, &s2));

    fs2_free(&s);
    fs2_free(&s1);
    fs2_free(&s2);
    assert_balanced();
}

Test(fs2_api_modificators, ends_with_no_match) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("hello");
    fs2 s1  = fs2_fromstr("world");      /* different */
    fs2 s2  = fs2_fromstr("Hello");      /* case differs */
    fs2 s3  = fs2_fromstr("ell");        /* matches inside, not at end */

    CHECK_FALSE("wrong content", fs2_ends_with(&s, &s1));
    CHECK_FALSE("case mismatch", fs2_ends_with(&s, &s2));
    CHECK_FALSE("not at end",    fs2_ends_with(&s, &s3));

    fs2_free(&s);
    fs2_free(&s1);
    fs2_free(&s2);
    fs2_free(&s3);
    assert_balanced();
}

Test(fs2_api_modificators, ends_with_suffix_longer) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("ab");
    fs2 suf = fs2_fromstr("cab");

    CHECK_FALSE("suffix longer", fs2_ends_with(&s, &suf));

    fs2_free(&s);
    fs2_free(&suf);
    assert_balanced();
}

/* Repeated pattern: memcmp slice must match at the very end */
Test(fs2_api_modificators, ends_with_slice_offset_correct) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("abcabc");
    fs2 suf = fs2_fromstr("abc");

    CHECK_TRUE("last 3 bytes", fs2_ends_with(&s, &suf));

    /* Wrong if the implementation compared v[0..3) instead of v[3..6) */
    fs2 notsuf = fs2_fromstr("cba");
    CHECK_FALSE("slice offset", fs2_ends_with(&s, &notsuf));

    fs2_free(&s);
    fs2_free(&suf);
    fs2_free(&notsuf);
    assert_balanced();
}

/* =========================================================================
 * Empty prefix / suffix — always true (all flavors)
 * ========================================================================= */

Test(fs2_api_modificators, starts_with_empty_prefix_all_flavors) {
    __fs2_checkreset();
    fs2 s      = fs2_fromstr("abc");
    fs2 empty  = FS2EMPTY;                /* v="", len=0 */
    fs2 canon  = FS2();                   /* v=NULL, len=0 */
    fs2 lit    = FS2LITERAL("");          /* v="", len=0, STATIC */

    CHECK_TRUE("FS2EMPTY", fs2_starts_with(&s, &empty));
    CHECK_TRUE("FS2()",    fs2_starts_with(&s, &canon));
    CHECK_TRUE("FS2LITERAL(\"\")", fs2_starts_with(&s, &lit));

    fs2_free(&s);
    fs2_free(&canon);
    assert_balanced();
}

Test(fs2_api_modificators, ends_with_empty_suffix_all_flavors) {
    __fs2_checkreset();
    fs2 s      = fs2_fromstr("abc");
    fs2 empty  = FS2EMPTY;
    fs2 canon  = FS2();
    fs2 lit    = FS2LITERAL("");

    CHECK_TRUE("FS2EMPTY", fs2_ends_with(&s, &empty));
    CHECK_TRUE("FS2()",    fs2_ends_with(&s, &canon));
    CHECK_TRUE("FS2LITERAL(\"\")", fs2_ends_with(&s, &lit));

    fs2_free(&s);
    fs2_free(&canon);
    assert_balanced();
}

/* =========================================================================
 * Empty s + non-empty prefix/suffix — always false (no deref)
 * ========================================================================= */

Test(fs2_api_modificators, starts_with_empty_src_false) {
    __fs2_checkreset();
    fs2 p = fs2_fromstr("a");

    fs2 canon = FS2();
    fs2 empty = FS2EMPTY;

    CHECK_FALSE("FS2() + \"a\"",    fs2_starts_with(&canon, &p));
    CHECK_FALSE("FS2EMPTY + \"a\"", fs2_starts_with(&empty, &p));

    fs2_free(&canon);
    fs2_free(&p);
    assert_balanced();
}

Test(fs2_api_modificators, ends_with_empty_src_false) {
    __fs2_checkreset();
    fs2 suf = fs2_fromstr("a");

    fs2 canon = FS2();
    fs2 empty = FS2EMPTY;

    CHECK_FALSE("FS2() + \"a\"",    fs2_ends_with(&canon, &suf));
    CHECK_FALSE("FS2EMPTY + \"a\"", fs2_ends_with(&empty, &suf));

    fs2_free(&canon);
    fs2_free(&suf);
    assert_balanced();
}

/* =========================================================================
 * BOTH sides empty, all 4 flavor combinations — must be true, must not crash
 * ========================================================================= */

Test(fs2_api_modificators, starts_with_both_empty_all_flavors) {
    __fs2_checkreset();

    fs2 fl[] = { FS2(), FS2EMPTY, FS2LITERAL("") };

    for (size_t i = 0; i < 3; i++) {
        for (size_t j = 0; j < 3; j++) {
            CHECK_TRUE("all empty flavors",
                       fs2_starts_with(&fl[i], &fl[j]));
        }
    }

    fs2_free(&fl[0]);
    assert_balanced();
}

Test(fs2_api_modificators, ends_with_both_empty_all_flavors) {
    __fs2_checkreset();

    fs2 fl[] = { FS2(), FS2EMPTY, FS2LITERAL("") };

    for (size_t i = 0; i < 3; i++) {
        for (size_t j = 0; j < 3; j++) {
            CHECK_TRUE("all empty flavors",
                       fs2_ends_with(&fl[i], &fl[j]));
        }
    }

    fs2_free(&fl[0]);
    assert_balanced();
}

/* =========================================================================
 * STATIC src
 * ========================================================================= */

Test(fs2_api_modificators, starts_with_static_src) {
    __fs2_checkreset();
    fs2 s = FS2LITERAL("hello world");
    fs2 p = fs2_fromstr("hello");

    CHECK_TRUE("match", fs2_starts_with(&s, &p));
    CHECK_FS2VALID(1, &s);

    fs2_free(&p);
    assert_balanced();
}

Test(fs2_api_modificators, ends_with_static_src) {
    __fs2_checkreset();
    fs2 s = FS2LITERAL("hello world");
    fs2 p = fs2_fromstr("world");

    CHECK_TRUE("match", fs2_ends_with(&s, &p));
    CHECK_FS2VALID(1, &s);

    fs2_free(&p);
    assert_balanced();
}

/* =========================================================================
 * Guards (invfatal -> SIGABRT in debug)
 * ========================================================================= */

Test(fs2_api_modificators, starts_with_null_s_aborts, .signal = SIGABRT) {
    fs2 p = fs2_fromstr("a");
    fs2_starts_with(NULL, &p);
    fs2_free(&p);
}

Test(fs2_api_modificators, starts_with_null_prefix_aborts, .signal = SIGABRT) {
    fs2 s = fs2_fromstr("abc");
    fs2_starts_with(&s, NULL);
    fs2_free(&s);
}

Test(fs2_api_modificators, ends_with_null_s_aborts, .signal = SIGABRT) {
    fs2 p = fs2_fromstr("a");
    fs2_ends_with(NULL, &p);
    fs2_free(&p);
}

Test(fs2_api_modificators, ends_with_null_suffix_aborts, .signal = SIGABRT) {
    fs2 s = fs2_fromstr("abc");
    fs2_ends_with(&s, NULL);
    fs2_free(&s);
}

/* =========================================================================
 * starts_with_str / ends_with_str — nominal
 * ========================================================================= */

Test(fs2_api_modificators, starts_with_str_match_and_miss) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello world");

    CHECK_TRUE ("match",       fs2_starts_with_str(&s, "hello"));
    CHECK_TRUE ("whole",       fs2_starts_with_str(&s, "hello world"));
    CHECK_FALSE("wrong",       fs2_starts_with_str(&s, "world"));
    CHECK_FALSE("case",        fs2_starts_with_str(&s, "Hello"));
    CHECK_FALSE("longer",      fs2_starts_with_str(&s, "hello world!!"));

    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_modificators, ends_with_str_match_and_miss) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello world");

    CHECK_TRUE ("match",   fs2_ends_with_str(&s, "world"));
    CHECK_TRUE ("whole",   fs2_ends_with_str(&s, "hello world"));
    CHECK_FALSE("wrong",   fs2_ends_with_str(&s, "hello"));
    CHECK_FALSE("case",    fs2_ends_with_str(&s, "World"));
    CHECK_FALSE("longer",  fs2_ends_with_str(&s, "!!hello world"));

    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

/* Slice offset check: "abcabc" ends_with "abc" — must compare the LAST 3 bytes,
 * not the first 3. */
Test(fs2_api_modificators, ends_with_str_slice_offset) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abcabc");

    CHECK_TRUE ("last 3",  fs2_ends_with_str(&s, "abc"));
    CHECK_FALSE("cba",     fs2_ends_with_str(&s, "cba"));   /* first 3 reversed */

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * NULL pattern — LENIENT via _fs2_wrap(NULL) -> FS2EMPTY -> true
 *   (contrast: fs2_starts_with(&s, NULL) aborts)
 * ========================================================================= */

Test(fs2_api_modificators, starts_with_str_null_prefix_true) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");

    CHECK_TRUE("NULL prefix == empty == always true",
               fs2_starts_with_str(&s, NULL));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_modificators, ends_with_str_null_suffix_true) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");

    CHECK_TRUE("NULL suffix == empty == always true",
               fs2_ends_with_str(&s, NULL));

    fs2_free(&s);
    assert_balanced();
}

/* "" c-string behaves identically to NULL */
Test(fs2_api_modificators, starts_with_str_empty_string_true) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");

    CHECK_TRUE("empty prefix", fs2_starts_with_str(&s, ""));
    CHECK_TRUE("NULL prefix",  fs2_starts_with_str(&s, NULL));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_modificators, ends_with_str_empty_string_true) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");

    CHECK_TRUE("empty suffix", fs2_ends_with_str(&s, ""));
    CHECK_TRUE("NULL suffix",  fs2_ends_with_str(&s, NULL));

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * Equivalence: _str == fs2 with explicit _fs2_wrap(pattern)
 * ========================================================================= */

Test(fs2_api_modificators, starts_with_str_matches_fs2_version) {
    __fs2_checkreset();
    fs2 s    = fs2_fromstr("hello world");
    fs2 p_ok = fs2_fromstr("hello");
    fs2 p_no = fs2_fromstr("world");

    CHECK_EQ("match", fs2_starts_with_str(&s, "hello"),
                      fs2_starts_with(&s, &p_ok));
    CHECK_EQ("miss",  fs2_starts_with_str(&s, "world"),
                      fs2_starts_with(&s, &p_no));

    fs2_free(&s);
    fs2_free(&p_ok);
    fs2_free(&p_no);
    assert_balanced();
}

Test(fs2_api_modificators, ends_with_str_matches_fs2_version) {
    __fs2_checkreset();
    fs2 s    = fs2_fromstr("hello world");
    fs2 p_ok = fs2_fromstr("world");
    fs2 p_no = fs2_fromstr("hello");

    CHECK_EQ("match", fs2_ends_with_str(&s, "world"),
                      fs2_ends_with(&s, &p_ok));
    CHECK_EQ("miss",  fs2_ends_with_str(&s, "hello"),
                      fs2_ends_with(&s, &p_no));

    fs2_free(&s);
    fs2_free(&p_ok);
    fs2_free(&p_no);
    assert_balanced();
}

/* =========================================================================
 * Empty s (FS2 / FS2EMPTY) + NULL/"" pattern — no deref, returns true
 * ========================================================================= */

Test(fs2_api_modificators, starts_with_str_empty_src_null_prefix) {
    __fs2_checkreset();
    fs2 canon = FS2();
    fs2 empty = FS2EMPTY;

    CHECK_TRUE("FS2() + NULL",    fs2_starts_with_str(&canon, NULL));
    CHECK_TRUE("FS2() + \"\"",    fs2_starts_with_str(&canon, ""));
    CHECK_TRUE("FS2EMPTY + NULL", fs2_starts_with_str(&empty, NULL));
    CHECK_TRUE("FS2EMPTY + \"\"", fs2_starts_with_str(&empty, ""));

    /* Non-empty pattern against empty s -> false, no deref */
    CHECK_FALSE("FS2() + \"a\"",    fs2_starts_with_str(&canon, "a"));
    CHECK_FALSE("FS2EMPTY + \"a\"", fs2_starts_with_str(&empty, "a"));

    fs2_free(&canon);
    assert_balanced();
}

Test(fs2_api_modificators, ends_with_str_empty_src_null_suffix) {
    __fs2_checkreset();
    fs2 canon = FS2();
    fs2 empty = FS2EMPTY;

    CHECK_TRUE("FS2() + NULL",    fs2_ends_with_str(&canon, NULL));
    CHECK_TRUE("FS2() + \"\"",    fs2_ends_with_str(&canon, ""));
    CHECK_TRUE("FS2EMPTY + NULL", fs2_ends_with_str(&empty, NULL));
    CHECK_TRUE("FS2EMPTY + \"\"", fs2_ends_with_str(&empty, ""));

    CHECK_FALSE("FS2() + \"a\"",    fs2_ends_with_str(&canon, "a"));
    CHECK_FALSE("FS2EMPTY + \"a\"", fs2_ends_with_str(&empty, "a"));

    fs2_free(&canon);
    assert_balanced();
}

/* =========================================================================
 * NULL s still aborts (inherited from fs2_starts_with / fs2_ends_with)
 * ========================================================================= */

Test(fs2_api_modificators, starts_with_str_null_s_aborts, .signal = SIGABRT) {
    fs2_starts_with_str(NULL, "a");
}

Test(fs2_api_modificators, ends_with_str_null_s_aborts, .signal = SIGABRT) {
    fs2_ends_with_str(NULL, "a");
}

/* =========================================================================
 * fs2_contains — nominal
 * ========================================================================= */

Test(fs2_api_modificators, contains_match_and_miss) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("hello world");
    fs2 y1  = fs2_fromstr("hello");      /* prefix */
    fs2 y2  = fs2_fromstr("world");      /* suffix */
    fs2 y3  = fs2_fromstr("o w");        /* middle */
    fs2 y4  = fs2_fromstr("world!!");    /* not in */
    fs2 y5  = fs2_fromstr("Hello");      /* case-sensitive miss */

    CHECK_TRUE ("prefix",  fs2_contains(&s, &y1));
    CHECK_TRUE ("suffix",  fs2_contains(&s, &y2));
    CHECK_TRUE ("middle",  fs2_contains(&s, &y3));
    CHECK_FALSE("longer",  fs2_contains(&s, &y4));
    CHECK_FALSE("case",    fs2_contains(&s, &y5));

    fs2_free(&s);
    fs2_free(&y1); fs2_free(&y2); fs2_free(&y3);
    fs2_free(&y4); fs2_free(&y5);
    assert_balanced();
}

/* Empty pattern: Python semantics — matches anywhere, always true */
Test(fs2_api_modificators, contains_empty_pattern_true) {
    __fs2_checkreset();
    fs2 s      = fs2_fromstr("abc");
    fs2 pat    = FS2EMPTY;
    fs2 canon  = FS2();
    fs2 lit    = FS2LITERAL("");

    CHECK_TRUE("FS2EMPTY pattern",      fs2_contains(&s, &pat));
    CHECK_TRUE("FS2() pattern",         fs2_contains(&s, &canon));
    CHECK_TRUE("FS2LITERAL(\"\") pattern", fs2_contains(&s, &lit));

    fs2_free(&s);
    fs2_free(&canon);
    assert_balanced();
}

/* =========================================================================
 * fs2_contains — empty s (post-fix consistency with FS2EMPTY)
 * ========================================================================= */

Test(fs2_api_modificators, contains_empty_src_nonempty_pattern_false) {
    __fs2_checkreset();
    fs2 pat   = fs2_fromstr("a");
    fs2 canon = FS2();
    fs2 empty = FS2EMPTY;

    CHECK_FALSE("FS2()",    fs2_contains(&canon, &pat));
    CHECK_FALSE("FS2EMPTY", fs2_contains(&empty, &pat));

    fs2_free(&canon);
    fs2_free(&pat);
    assert_balanced();
}

/* After the instr_pos_nth consistency fix, FS2() and FS2EMPTY are
 * equivalent for empty pattern — both should yield true. If this fails,
 * the FS2()/empty-pattern check ordering was reverted. */
Test(fs2_api_modificators, contains_empty_src_empty_pattern_true) {
    __fs2_checkreset();
    fs2 pat   = FS2EMPTY;
    fs2 canon = FS2();
    fs2 empty = FS2EMPTY;

    CHECK_TRUE("FS2()",    fs2_contains(&canon, &pat));
    CHECK_TRUE("FS2EMPTY", fs2_contains(&empty, &pat));

    fs2_free(&canon);
    assert_balanced();
}

/* =========================================================================
 * fs2_contains — guards
 * ========================================================================= */

Test(fs2_api_modificators, contains_null_s_aborts, .signal = SIGABRT) {
    fs2 pat = fs2_fromstr("a");
    fs2_contains(NULL, &pat);
    fs2_free(&pat);
}

Test(fs2_api_modificators, contains_null_pat_aborts, .signal = SIGABRT) {
    fs2 s = fs2_fromstr("abc");
    fs2_contains(&s, NULL);              /* invfatal in instr_pos_nth */
    fs2_free(&s);
}

/* =========================================================================
 * fs2_contains_str — nominal + NULL/"" lenient
 * ========================================================================= */

Test(fs2_api_modificators, contains_str_match_and_miss) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello world");

    CHECK_TRUE ("prefix", fs2_contains_str(&s, "hello"));
    CHECK_TRUE ("suffix", fs2_contains_str(&s, "world"));
    CHECK_TRUE ("middle", fs2_contains_str(&s, "o w"));
    CHECK_FALSE("longer", fs2_contains_str(&s, "world!!"));
    CHECK_FALSE("case",   fs2_contains_str(&s, "Hello"));

    fs2_free(&s);
    assert_balanced();
}

/* NULL pat -> _fs2_wrap(NULL) -> FS2EMPTY -> empty pattern -> true */
Test(fs2_api_modificators, contains_str_null_pat_true) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");

    CHECK_TRUE("NULL pat == empty pattern", fs2_contains_str(&s, NULL));
    CHECK_TRUE("""  pat == empty pattern", fs2_contains_str(&s, ""));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_modificators, contains_str_empty_src_null_pat_true) {
    __fs2_checkreset();
    fs2 canon = FS2();
    fs2 empty = FS2EMPTY;

    CHECK_TRUE("FS2() + NULL",    fs2_contains_str(&canon, NULL));
    CHECK_TRUE("FS2EMPTY + NULL", fs2_contains_str(&empty, NULL));
    CHECK_TRUE("FS2() + \"\"",    fs2_contains_str(&canon, ""));
    CHECK_TRUE("FS2EMPTY + \"\"", fs2_contains_str(&empty, ""));

    fs2_free(&canon);
    assert_balanced();
}

Test(fs2_api_modificators, contains_str_null_s_aborts, .signal = SIGABRT) {
    fs2_contains_str(NULL, "a");
}

/* =========================================================================
 * Consistency: _str ≡ fs2 with explicit wrapped pattern
 * ========================================================================= */

Test(fs2_api_modificators, contains_str_matches_fs2_version) {
    __fs2_checkreset();
    fs2 s     = fs2_fromstr("hello world");
    fs2 p_ok  = fs2_fromstr("world");
    fs2 p_no  = fs2_fromstr("xyz");

    CHECK_EQ("match", fs2_contains_str(&s, "world"),
                      fs2_contains(&s, &p_ok));
    CHECK_EQ("miss",  fs2_contains_str(&s, "xyz"),
                      fs2_contains(&s, &p_no));

    fs2_free(&s);
    fs2_free(&p_ok);
    fs2_free(&p_no);
    assert_balanced();
}

/* =========================================================================
 * fs2_count — nominal
 * ========================================================================= */

Test(fs2_api_modificators, count_basic) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("banana");
    fs2 pat = fs2_fromstr("an");

    CHECK_EQ("banana/an", fs2_count(&s, &pat), 2);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    fs2_free(&pat);
    assert_balanced();
}

Test(fs2_api_modificators, count_three_occurrences) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("abcabcabc");
    fs2 pat = fs2_fromstr("abc");

    CHECK_EQ("abcabcabc/abc", fs2_count(&s, &pat), 3);

    fs2_free(&s);
    fs2_free(&pat);
    assert_balanced();
}

Test(fs2_api_modificators, count_not_found) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("hello");
    fs2 pat = fs2_fromstr("xyz");

    CHECK_EQ("no match", fs2_count(&s, &pat), 0);

    fs2_free(&s);
    fs2_free(&pat);
    assert_balanced();
}

Test(fs2_api_modificators, count_whole_string_as_pattern) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("abc");
    fs2 pat = fs2_fromstr("abc");

    CHECK_EQ("whole match", fs2_count(&s, &pat), 1);

    fs2_free(&s);
    fs2_free(&pat);
    assert_balanced();
}

Test(fs2_api_modificators, count_pattern_longer_than_src) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("ab");
    fs2 pat = fs2_fromstr("abc");

    CHECK_EQ("too long", fs2_count(&s, &pat), 0);

    fs2_free(&s);
    fs2_free(&pat);
    assert_balanced();
}

/* =========================================================================
 * CRITICAL: non-overlapping semantics
 *   Overlapping (naive "count every starting position") would give:
 *     "aaa"/"aa"  -> 2
 *     "aaaa"/"aa" -> 3
 *   Non-overlapping (this implementation) gives:
 *     "aaa"/"aa"  -> 1
 *     "aaaa"/"aa" -> 2
 * ========================================================================= */

Test(fs2_api_modificators, count_non_overlapping_aaa) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("aaa");
    fs2 pat = fs2_fromstr("aa");

    CHECK_EQ("non-overlapping", fs2_count(&s, &pat), 1);   /* NOT 2 */

    fs2_free(&s);
    fs2_free(&pat);
    assert_balanced();
}

Test(fs2_api_modificators, count_non_overlapping_aaaa) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("aaaa");
    fs2 pat = fs2_fromstr("aa");

    CHECK_EQ("non-overlapping", fs2_count(&s, &pat), 2);   /* NOT 3 */

    fs2_free(&s);
    fs2_free(&pat);
    assert_balanced();
}

Test(fs2_api_modificators, count_non_overlapping_single_char) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("aaaaa");
    fs2 pat = fs2_fromstr("a");

    CHECK_EQ("single-char pattern", fs2_count(&s, &pat), 5);

    fs2_free(&s);
    fs2_free(&pat);
    assert_balanced();
}

Test(fs2_api_modificators, count_mississippi_ss) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("mississippi");
    fs2 pat = fs2_fromstr("ss");

    /* "ss" at 2 and 5 — not overlapping */
    CHECK_EQ("mississippi/ss", fs2_count(&s, &pat), 2);

    fs2_free(&s);
    fs2_free(&pat);
    assert_balanced();
}

/* =========================================================================
 * Empty pattern — explicit 0 (contrasts Python's len+1)
 * ========================================================================= */

Test(fs2_api_modificators, count_empty_pattern_zero_all_flavors) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("abc");
    fs2 e   = FS2EMPTY;
    fs2 c   = FS2();
    fs2 lit = FS2LITERAL("");

    CHECK_EQ("FS2EMPTY",         fs2_count(&s, &e),   0);
    CHECK_EQ("FS2()",            fs2_count(&s, &c),   0);
    CHECK_EQ("FS2LITERAL(\"\")", fs2_count(&s, &lit), 0);

    fs2_free(&s);
    fs2_free(&c);
    assert_balanced();
}

Test(fs2_api_modificators, count_empty_pattern_empty_src_zero) {
    __fs2_checkreset();
    fs2 canon = FS2();
    fs2 empty = FS2EMPTY;
    fs2 pat   = FS2EMPTY;

    CHECK_EQ("FS2() + empty",    fs2_count(&canon, &pat), 0);
    CHECK_EQ("FS2EMPTY + empty", fs2_count(&empty, &pat), 0);

    fs2_free(&canon);
    assert_balanced();
}

/* =========================================================================
 * Empty src + non-empty pattern -> 0 (no deref)
 * ========================================================================= */

Test(fs2_api_modificators, count_empty_src_nonempty_pat_zero) {
    __fs2_checkreset();
    fs2 canon = FS2();
    fs2 empty = FS2EMPTY;
    fs2 pat   = fs2_fromstr("a");

    CHECK_EQ("FS2()",    fs2_count(&canon, &pat), 0);
    CHECK_EQ("FS2EMPTY", fs2_count(&empty, &pat), 0);

    fs2_free(&canon);
    fs2_free(&pat);
    assert_balanced();
}

/* =========================================================================
 * Guards
 * ========================================================================= */

Test(fs2_api_modificators, count_null_s_aborts, .signal = SIGABRT) {
    fs2 pat = fs2_fromstr("a");
    fs2_count(NULL, &pat);
    fs2_free(&pat);
}

Test(fs2_api_modificators, count_null_pat_aborts, .signal = SIGABRT) {
    fs2 s = fs2_fromstr("abc");
    fs2_count(&s, NULL);
    fs2_free(&s);
}

/* =========================================================================
 * fs2_count_str — nominal
 * ========================================================================= */

Test(fs2_api_modificators, count_str_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("banana");

    CHECK_EQ("banana/an", fs2_count_str(&s, "an"), 2);
    CHECK_EQ("no match",  fs2_count_str(&s, "xyz"), 0);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_modificators, count_str_non_overlapping) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("aaaa");

    CHECK_EQ("aaaa/aa", fs2_count_str(&s, "aa"), 2);   /* NOT 3 */

    fs2_free(&s);
    assert_balanced();
}

/* NULL / "" pat -> _fs2_wrap -> FS2EMPTY -> 0 (lenient, no crash) */
Test(fs2_api_modificators, count_str_null_or_empty_pat_zero) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");

    CHECK_EQ("NULL pat", fs2_count_str(&s, NULL), 0);
    CHECK_EQ("\"\" pat",  fs2_count_str(&s, ""),   0);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_modificators, count_str_null_s_aborts, .signal = SIGABRT) {
    fs2_count_str(NULL, "a");
}

/* =========================================================================
 * Consistency: _str ≡ fs2 with wrapped pattern
 * ========================================================================= */

Test(fs2_api_modificators, count_str_matches_fs2_version) {
    __fs2_checkreset();
    fs2 s    = fs2_fromstr("abcabcabc");
    fs2 pat  = fs2_fromstr("abc");

    CHECK_EQ("match",  fs2_count_str(&s, "abc"), fs2_count(&s, &pat));
    CHECK_EQ("miss",   fs2_count_str(&s, "xyz"), 0);

    fs2_free(&s);
    fs2_free(&pat);
    assert_balanced();
}

/* =========================================================================
 * fs2_repeat_new — nominal
 * ========================================================================= */

Test(fs2_api_modificators, repeat_new_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("ab");

    fs2 r = fs2_repeat_new(&s, 4);

    CHECK_STR_EQ("abababab", r.v, "abababab");
    CHECK_EQ  ("len",        r.len, 8);
    CHECK_TRUE("ALLOC",           fs2_is_alloc(&r));
    CHECK_FALSE("not bodyalloc",  fs2_is_bodyalloc(&r));
    CHECK_FS2VALID(1, &r);
    CHECK_FS2VALID(2, &s);        /* src untouched */

    fs2_free(&s);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api_modificators, repeat_new_multi_char) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");

    fs2 r = fs2_repeat_new(&s, 3);
    CHECK_STR_EQ("abcabcabc", r.v, "abcabcabc");
    CHECK_EQ  ("len",         r.len, 9);
    fs2_free(&r);

    /* Single char x5 */
    fs2 c = fs2_fromstr("x");
    fs2 r2 = fs2_repeat_new(&c, 5);
    CHECK_STR_EQ("xxxxx", r2.v, "xxxxx");
    fs2_free(&c);
    fs2_free(&r2);

    fs2_free(&s);
    assert_balanced();
}

/* n == 1 -> equivalent to a clone of s */
Test(fs2_api_modificators, repeat_new_n_one) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    fs2 r = fs2_repeat_new(&s, 1);

    CHECK_STR_EQ("hello", r.v, "hello");
    CHECK_EQ  ("len",     r.len, 5);
    CHECK_TRUE("independent", r.v != s.v);
    CHECK_FS2VALID(1, &r);

    fs2_free(&s);
    fs2_free(&r);
    assert_balanced();
}

/* Source is STATIC / LOCAL — result is always ALLOC (by value) */
Test(fs2_api_modificators, repeat_new_static_src) {
    __fs2_checkreset();
    fs2 s = FS2LITERAL("ab");

    fs2 r = fs2_repeat_new(&s, 3);

    CHECK_STR_EQ("ababab", r.v, "ababab");
    CHECK_TRUE("ALLOC",     fs2_is_alloc(&r));
    CHECK_TRUE("independent", r.v != s.v);
    CHECK_FS2VALID(1, &r);

    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api_modificators, repeat_new_local_src) {
    __fs2_checkreset();
    FS2LOCAL(buf, 8);
    fs2_memcpy_str(&buf, 0, "ab", 2);
    fs2_setlen(&buf, 2);

    fs2 r = fs2_repeat_new(&buf, 4);

    CHECK_STR_EQ("abababab", r.v, "abababab");
    CHECK_TRUE("ALLOC",      fs2_is_alloc(&r));
    CHECK_TRUE("not LOCAL",  !fs2_is_local(&r));
    CHECK_FS2VALID(1, &r);

    fs2_free(&r);
    assert_balanced();
}

/* =========================================================================
 * fs2_repeat_new — early-return cases (empty src / n == 0)
 *
 *   Both return FS2() (canonical empty, ALLOC v=NULL),
 *   NOT a clone of src, NOT FS2EMPTY.
 * ========================================================================= */

Test(fs2_api_modificators, repeat_new_n_zero_returns_canon) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");

    fs2 r = fs2_repeat_new(&s, 0);

    CHECK_TRUE ("canon",  fs2_is_canon(&r));
    CHECK_TRUE ("empty",  fs2_is_empty(&r));
    CHECK_EQ   ("len",    r.len, 0);
    CHECK_PTR_EQ("v",     r.v, NULL);
    CHECK_EQ   ("sz",     r.sz, 0);
    CHECK_FS2VALID(1, &r);

    fs2_free(&s);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api_modificators, repeat_new_fs2empty_src_returns_canon) {
    __fs2_checkreset();
    fs2 s = FS2EMPTY;

    fs2 r = fs2_repeat_new(&s, 100);

    CHECK_TRUE("canon", fs2_is_canon(&r));
    CHECK_FS2VALID(1, &r);

    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api_modificators, repeat_new_canon_src_returns_canon) {
    __fs2_checkreset();
    fs2 s = FS2();

    fs2 r = fs2_repeat_new(&s, 5);

    CHECK_TRUE("canon", fs2_is_canon(&r));
    CHECK_FS2VALID(1, &r);

    fs2_free(&s);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api_modificators, repeat_new_static_empty_literal_returns_canon) {
    __fs2_checkreset();
    fs2 s = FS2LITERAL("");

    fs2 r = fs2_repeat_new(&s, 5);

    CHECK_TRUE("canon", fs2_is_canon(&r));
    CHECK_FS2VALID(1, &r);

    fs2_free(&r);
    assert_balanced();
}

/* =========================================================================
 * fs2_repeat_new — overflow guard (userraise -> SIGINT without TRY)
 * ========================================================================= */

/* s->len == 1, n == SIZE_MAX: n > (SIZE_MAX-1)/1 -> overflow */
Test(fs2_api_modificators, repeat_new_overflow_1_byte_sigint, .signal = SIGINT) {
    fs2 s = fs2_fromstr("a");
    (void) fs2_repeat_new(&s, SIZE_MAX);
    fs2_free(&s);
}

/* s->len == 2, n == SIZE_MAX: n > (SIZE_MAX-1)/2 -> overflow */
Test(fs2_api_modificators, repeat_new_overflow_2_bytes_sigint, .signal = SIGINT) {
    fs2 s = fs2_fromstr("ab");
    (void) fs2_repeat_new(&s, SIZE_MAX);
    fs2_free(&s);
}

/* Inside TRY(): overflow longjmps to else, records ERR_OUT_OF_RANGE */
Test(fs2_api_modificators, repeat_new_overflow_caught_by_try) {
    __fs2_checkreset();
    err_clean(true);
    err_resetenv();

    fs2 s = fs2_fromstr("abc");
    bool caught = false;

    TRY() {
        (void) fs2_repeat_new(&s, SIZE_MAX);
        cr_assert_fail("expected userraise; got normal return");
    } else {
        caught = true;
    }

    cr_assert(caught, "TRY() else-branch should have been reached");
    cr_assert_eq(err_count(), 1, "one error record expected");

    ErrorInfo info = {0};
    cr_assert(err_last(&info));
    cr_assert_eq(info.type, ERR_USER);
    cr_assert_eq(info.code, ERR_OUT_OF_RANGE);

    err_clean(true);
    err_resetenv();
    fs2_free(&s);
    assert_balanced();
}

/* Boundary: n = (SIZE_MAX - 1) / s->len must NOT overflow the check.
 * We don't actually allocate that much — the check passes, then
 * fs2_reserve tries to allocate and userraises (OOM). Either way, not UB.
 * Skip this test (would try to allocate ~SIZE_MAX bytes). */
/* (intentionally not tested) */

/* =========================================================================
 * fs2_repeat_new — guards
 * ========================================================================= */

Test(fs2_api_modificators, repeat_new_null_s_aborts, .signal = SIGABRT) {
    fs2_repeat_new(NULL, 5);
}

/* =========================================================================
 * src immutability / independence
 * ========================================================================= */

Test(fs2_api_modificators, repeat_new_src_untouched_and_independent) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("xy");
    void *orig = s.v;

    fs2 r = fs2_repeat_new(&s, 3);

    CHECK_PTR_EQ("src v unchanged", s.v, orig);
    CHECK_STR_EQ("src content",     s.v, "xy");
    CHECK_STR_EQ("result",          r.v, "xyxyxy");
    CHECK_TRUE ("independent",      r.v != s.v);

    /* Mutating result must not affect src */
    r.v[0] = 'Z';
    CHECK_STR_EQ("result mutated", r.v, "Zyxyxy");
    CHECK_STR_EQ("src original",   s.v, "xy");

    CHECK_FS2VALID(1, &r);
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
    fs2_free(&r);
    assert_balanced();
}

/* =========================================================================
 * fs2_heap_repeat — BODYALLOC wrapper
 * ========================================================================= */

Test(fs2_api_modificators, heap_repeat_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("ab");

    fs2 *r = fs2_heap_repeat(&s, 4);

    cr_assert_not_null(r);
    CHECK_STR_EQ("abababab", r->v, "abababab");
    CHECK_EQ  ("len",        r->len, 8);
    CHECK_TRUE("ALLOC",      fs2_is_alloc(r));
    CHECK_TRUE("BODYALLOC",  fs2_is_bodyalloc(r));
    CHECK_FS2VALID(1, r);
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
    fs2_free(r);
    assert_balanced();
}

Test(fs2_api_modificators, heap_repeat_empty_result) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");

    fs2 *r = fs2_heap_repeat(&s, 0);

    cr_assert_not_null(r);
    CHECK_TRUE ("BODYALLOC",     fs2_is_bodyalloc(r));
    CHECK_TRUE ("empty",         fs2_is_empty(r));
    CHECK_TRUE ("eq_empty shape", fs2_eq_empty(r));   /* v!="", STATIC, sz=1 */
    CHECK_EQ  ("len",       r->len, 0);
    CHECK_FS2VALID(1, r);

    fs2_free(&s);
    fs2_free(r);
    assert_balanced();
}

Test(fs2_api_modificators, heap_repeat_null_s_aborts, .signal = SIGABRT) {
    fs2_heap_repeat(NULL, 5);
}

/* heap_repeat overflow: inner userraise -> SIGINT (no TRY) */
Test(fs2_api_modificators, heap_repeat_overflow_sigint, .signal = SIGINT) {
    fs2 s = fs2_fromstr("abc");
    (void) fs2_heap_repeat(&s, SIZE_MAX);
    fs2_free(&s);
}

/* =========================================================================
 * Consistency: heap_repeat ≡ heaptostack(repeat_new)
 * ========================================================================= */

Test(fs2_api_modificators, heap_repeat_matches_stack_version) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("xy");

    fs2  stack = fs2_repeat_new(&s, 3);
    fs2 *heap  = fs2_heap_repeat(&s, 3);

    cr_assert_not_null(heap);
    CHECK_STR_EQ("same content", stack.v,   heap->v);
    CHECK_EQ  ("same len",       stack.len, heap->len);
    CHECK_FALSE("stack not body", fs2_is_bodyalloc(&stack));
    CHECK_TRUE ("heap body",      fs2_is_bodyalloc(heap));
    CHECK_FS2VALID(1, &stack);
    CHECK_FS2VALID(2, heap);

    fs2_free(&s);
    fs2_free(&stack);
    fs2_free(heap);
    assert_balanced();
}
