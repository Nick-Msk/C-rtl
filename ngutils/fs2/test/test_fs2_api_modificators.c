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

/* =========================================================================
 * fs2_strip_prefix — nominal
 * ========================================================================= */

Test(fs2_api_modificators, strip_prefix_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello world");
    fs2 p = fs2_fromstr("hello ");

    fs2 *r = fs2_strip_prefix(&s, &p);

    CHECK_STR_EQ("world",   s.v, "world");
    CHECK_EQ  ("len",       s.len, 5);
    CHECK_PTR_EQ("chaining", r, &s);
    CHECK_FS2VALID(1, &s);
    CHECK_FS2VALID(2, &p);

    fs2_free(&s);
    fs2_free(&p);
    assert_balanced();
}

Test(fs2_api_modificators, strip_prefix_whole_string) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");
    fs2 p = fs2_fromstr("abc");

    fs2_strip_prefix(&s, &p);

    CHECK_EQ  ("len",    s.len, 0);
    CHECK_TRUE("empty",  fs2_is_empty(&s));
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    fs2_free(&p);
    assert_balanced();
}

Test(fs2_api_modificators, strip_prefix_no_match) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");
    fs2 p = fs2_fromstr("world");
    void *orig = s.v;

    fs2_strip_prefix(&s, &p);

    CHECK_PTR_EQ("buffer untouched", s.v, orig);
    CHECK_STR_EQ("hello",            s.v, "hello");

    fs2_free(&s);
    fs2_free(&p);
    assert_balanced();
}

Test(fs2_api_modificators, strip_prefix_empty_prefix_noop) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");
    fs2 e = FS2EMPTY;
    fs2 c = FS2();
    void *orig = s.v;

    fs2_strip_prefix(&s, &e);
    CHECK_PTR_EQ("FS2EMPTY noop", s.v, orig);
    fs2_strip_prefix(&s, &c);
    CHECK_PTR_EQ("FS2() noop",    s.v, orig);

    fs2_free(&s);
    fs2_free(&c);
    assert_balanced();
}

Test(fs2_api_modificators, strip_prefix_prefix_longer_than_s) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("ab");
    fs2 p = fs2_fromstr("abc");

    fs2_strip_prefix(&s, &p);

    CHECK_STR_EQ("ab", s.v, "ab");

    fs2_free(&s);
    fs2_free(&p);
    assert_balanced();
}

/* =========================================================================
 * fs2_strip_suffix — nominal
 * ========================================================================= */

Test(fs2_api_modificators, strip_suffix_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello world");
    fs2 p = fs2_fromstr(" world");

    fs2 *r = fs2_strip_suffix(&s, &p);

    CHECK_STR_EQ("hello",   s.v, "hello");
    CHECK_EQ  ("len",       s.len, 5);
    CHECK_PTR_EQ("chaining", r, &s);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    fs2_free(&p);
    assert_balanced();
}

Test(fs2_api_modificators, strip_suffix_whole_string) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");
    fs2 p = fs2_fromstr("abc");

    fs2_strip_suffix(&s, &p);

    CHECK_EQ  ("len",   s.len, 0);
    CHECK_TRUE("empty", fs2_is_empty(&s));

    fs2_free(&s);
    fs2_free(&p);
    assert_balanced();
}

Test(fs2_api_modificators, strip_suffix_no_match) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");
    fs2 p = fs2_fromstr("world");
    void *orig = s.v;

    fs2_strip_suffix(&s, &p);

    CHECK_PTR_EQ("buffer untouched", s.v, orig);
    CHECK_STR_EQ("hello",            s.v, "hello");

    fs2_free(&s);
    fs2_free(&p);
    assert_balanced();
}

Test(fs2_api_modificators, strip_suffix_empty_suffix_noop) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");
    fs2 e = FS2EMPTY;
    fs2 c = FS2();

    fs2_strip_suffix(&s, &e);
    fs2_strip_suffix(&s, &c);

    CHECK_STR_EQ("hello", s.v, "hello");

    fs2_free(&s);
    fs2_free(&c);
    assert_balanced();
}

Test(fs2_api_modificators, strip_suffix_suffix_longer_than_s) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("ab");
    fs2 p = fs2_fromstr("cab");

    fs2_strip_suffix(&s, &p);
    CHECK_STR_EQ("ab", s.v, "ab");

    fs2_free(&s);
    fs2_free(&p);
    assert_balanced();
}

/* =========================================================================
 * Overlap: "aaa" strip "aa" -> "a" (only ONE strip, not recursive)
 * ========================================================================= */

Test(fs2_api_modificators, strip_prefix_only_one_occurrence) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("aaa");
    fs2 p = fs2_fromstr("aa");

    fs2_strip_prefix(&s, &p);

    CHECK_STR_EQ("a", s.v, "a");   /* not "" */

    fs2_free(&s);
    fs2_free(&p);
    assert_balanced();
}

Test(fs2_api_modificators, strip_suffix_only_one_occurrence) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("aaa");
    fs2 p = fs2_fromstr("aa");

    fs2_strip_suffix(&s, &p);

    CHECK_STR_EQ("a", s.v, "a");   /* not "" */

    fs2_free(&s);
    fs2_free(&p);
    assert_balanced();
}

/* =========================================================================
 * Empty src (FS2() / FS2EMPTY) — no-op, no crash
 * ========================================================================= */

Test(fs2_api_modificators, strip_empty_src_noop) {
    __fs2_checkreset();
    fs2 p = fs2_fromstr("a");

    fs2 canon = FS2();
    fs2 empty = FS2EMPTY;

    fs2_strip_prefix(&canon, &p);
    fs2_strip_prefix(&empty, &p);
    CHECK_TRUE("canon empty", fs2_is_canon(&canon));
    CHECK_TRUE("empty empty", fs2_eq_empty(&empty));

    fs2_strip_suffix(&canon, &p);
    fs2_strip_suffix(&empty, &p);
    CHECK_TRUE("canon empty 2", fs2_is_canon(&canon));
    CHECK_TRUE("empty empty 2", fs2_eq_empty(&empty));

    fs2_free(&canon);
    fs2_free(&p);
    assert_balanced();
}

/* =========================================================================
 * STATIC non-empty: lazy behaviour (no-match -> no-op, match -> abort)
 *
 *   If you add `fs2_cant_mutate(s)` upfront (eager, like fs2_toupper),
 *   flip both `_no_match_noop` tests to `.signal = SIGABRT`.
 * ========================================================================= */

Test(fs2_api_modificators, strip_static_prefix_no_match_abort, .signal = SIGABRT) {
    __fs2_checkreset();
    fs2 s = FS2LITERAL("hello");
    fs2 p = fs2_fromstr("xyz");

    fs2_strip_prefix(&s, &p);   /* starts_with false -> no-op */

    CHECK_STR_EQ("hello", s.v, "hello");
    CHECK_TRUE("still STATIC", fs2_is_static(&s));
    CHECK_FS2VALID(1, &s);

    fs2_free(&p);
    assert_balanced();
}

Test(fs2_api_modificators, strip_static_prefix_match_aborts, .signal = SIGABRT) {
    fs2 s = FS2LITERAL("hello");
    fs2 p = fs2_fromstr("hel");
    fs2_strip_prefix(&s, &p);   /* selfmove on STATIC -> abort */
    fs2_free(&p);
}

Test(fs2_api_modificators, strip_static_suffix_no_match_abort, .signal = SIGABRT) {
    __fs2_checkreset();
    fs2 s = FS2LITERAL("hello");
    fs2 p = fs2_fromstr("xyz");

    fs2_strip_suffix(&s, &p);   /* ends_with false -> no-op */

    CHECK_STR_EQ("hello", s.v, "hello");
    CHECK_TRUE("still STATIC", fs2_is_static(&s));
    CHECK_FS2VALID(1, &s);

    fs2_free(&p);
    assert_balanced();
}

Test(fs2_api_modificators, strip_static_suffix_match_aborts, .signal = SIGABRT) {
    fs2 s = FS2LITERAL("hello");
    fs2 p = fs2_fromstr("llo");
    fs2_strip_suffix(&s, &p);   /* setlen on STATIC with pos < len -> abort */
    fs2_free(&p);
}

/* =========================================================================
 * LOCAL src — shrinks only, always fits
 * ========================================================================= */

Test(fs2_api_modificators, strip_prefix_local) {
    __fs2_checkreset();
    FS2LOCAL(buf, 16);
    fs2_memcpy_str(&buf, 0, "hello world", 11);
    fs2_setlen(&buf, 11);
    fs2 p = fs2_fromstr("hello ");

    fs2_strip_prefix(&buf, &p);

    CHECK_STR_EQ("world",      buf.v, "world");
    CHECK_TRUE ("still LOCAL", fs2_is_local(&buf));
    CHECK_FS2VALID(1, &buf);

    fs2_free(&p);
    assert_balanced();
}

Test(fs2_api_modificators, strip_suffix_local) {
    __fs2_checkreset();
    FS2LOCAL(buf, 16);
    fs2_memcpy_str(&buf, 0, "hello world", 11);
    fs2_setlen(&buf, 11);
    fs2 p = fs2_fromstr(" world");

    fs2_strip_suffix(&buf, &p);

    CHECK_STR_EQ("hello",      buf.v, "hello");
    CHECK_TRUE ("still LOCAL", fs2_is_local(&buf));
    CHECK_FS2VALID(1, &buf);

    fs2_free(&p);
    assert_balanced();
}

/* =========================================================================
 * Guards
 * ========================================================================= */

Test(fs2_api_modificators, strip_prefix_null_s_aborts, .signal = SIGABRT) {
    fs2 p = fs2_fromstr("a");
    fs2_strip_prefix(NULL, &p);
    fs2_free(&p);
}

Test(fs2_api_modificators, strip_prefix_null_prefix_aborts, .signal = SIGABRT) {
    fs2 s = fs2_fromstr("abc");
    fs2_strip_prefix(&s, NULL);
    fs2_free(&s);
}

Test(fs2_api_modificators, strip_suffix_null_s_aborts, .signal = SIGABRT) {
    fs2 p = fs2_fromstr("a");
    fs2_strip_suffix(NULL, &p);
    fs2_free(&p);
}

Test(fs2_api_modificators, strip_suffix_null_suffix_aborts, .signal = SIGABRT) {
    fs2 s = fs2_fromstr("abc");
    fs2_strip_suffix(&s, NULL);
    fs2_free(&s);
}

/* =========================================================================
 * Pattern not touched, buffer not realloc when result fits
 * ========================================================================= */

Test(fs2_api_modificators, strip_prefix_pattern_untouched) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");
    fs2 p = fs2_fromstr("he");
    void *p_v = p.v;

    fs2_strip_prefix(&s, &p);

    CHECK_PTR_EQ("p.v unchanged", p.v, p_v);
    CHECK_STR_EQ("p content",     p.v, "he");
    CHECK_FS2VALID(1, &p);

    fs2_free(&s);
    fs2_free(&p);
    assert_balanced();
}

Test(fs2_api_modificators, strip_suffix_pattern_untouched) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");
    fs2 p = fs2_fromstr("lo");
    void *p_v = p.v;

    fs2_strip_suffix(&s, &p);

    CHECK_PTR_EQ("p.v unchanged", p.v, p_v);
    CHECK_STR_EQ("p content",     p.v, "lo");
    CHECK_FS2VALID(1, &p);

    fs2_free(&s);
    fs2_free(&p);
    assert_balanced();
}

/* =========================================================================
 * strip_prefix_str / strip_suffix_str — nominal
 * ========================================================================= */

Test(fs2_api_modificators, strip_prefix_str_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello world");

    fs2 *r = fs2_strip_prefix_str(&s, "hello ");

    CHECK_STR_EQ("world",   s.v, "world");
    CHECK_EQ  ("len",       s.len, 5);
    CHECK_PTR_EQ("chaining", r, &s);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_modificators, strip_suffix_str_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello world");

    fs2 *r = fs2_strip_suffix_str(&s, " world");

    CHECK_STR_EQ("hello",   s.v, "hello");
    CHECK_EQ  ("len",       s.len, 5);
    CHECK_PTR_EQ("chaining", r, &s);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_modificators, strip_prefix_str_no_match_noop) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");
    void *orig = s.v;

    fs2_strip_prefix_str(&s, "xyz");

    CHECK_PTR_EQ("buffer untouched", s.v, orig);
    CHECK_STR_EQ("hello",            s.v, "hello");

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_modificators, strip_suffix_str_no_match_noop) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");
    void *orig = s.v;

    fs2_strip_suffix_str(&s, "xyz");

    CHECK_PTR_EQ("buffer untouched", s.v, orig);
    CHECK_STR_EQ("hello",            s.v, "hello");

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * NULL affix — LENIENT via _fs2_wrap(NULL) -> FS2EMPTY -> no-op
 *   (contrast: fs2_strip_prefix(&s, NULL) aborts)
 * ========================================================================= */

Test(fs2_api_modificators, strip_prefix_str_null_affix_noop) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");
    void *orig = s.v;

    fs2_strip_prefix_str(&s, NULL);

    CHECK_PTR_EQ("buffer untouched", s.v, orig);
    CHECK_STR_EQ("hello",            s.v, "hello");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_modificators, strip_suffix_str_null_affix_noop) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");
    void *orig = s.v;

    fs2_strip_suffix_str(&s, NULL);

    CHECK_PTR_EQ("buffer untouched", s.v, orig);
    CHECK_STR_EQ("hello",            s.v, "hello");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* "" c-string behaves identically to NULL */
Test(fs2_api_modificators, strip_str_empty_affix_noop) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    fs2_strip_prefix_str(&s, "");
    CHECK_STR_EQ("prefix", s.v, "hello");

    fs2_strip_suffix_str(&s, "");
    CHECK_STR_EQ("suffix", s.v, "hello");

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * Contrast: fs2-version aborts on NULL affix, _str-version does not
 * ========================================================================= */

Test(fs2_api_modificators, strip_prefix_null_affix_aborts, .signal = SIGABRT) {
    fs2 s = fs2_fromstr("abc");
    fs2_strip_prefix(&s, NULL);
    fs2_free(&s);
}

Test(fs2_api_modificators, strip_suffix_null_affix_aborts, .signal = SIGABRT) {
    fs2 s = fs2_fromstr("abc");
    fs2_strip_suffix(&s, NULL);
    fs2_free(&s);
}

/* =========================================================================
 * Equivalence: _str == fs2 with explicit _fs2_wrap(affix)
 * ========================================================================= */

Test(fs2_api_modificators, strip_prefix_str_matches_fs2_version) {
    __fs2_checkreset();
    fs2 p = fs2_fromstr("he");

    fs2 a = fs2_fromstr("hello");
    fs2 b = fs2_fromstr("hello");
    fs2_strip_prefix_str(&a, "he");
    fs2_strip_prefix(&b, &p);
    CHECK_STR_EQ("strip_prefix_str == strip_prefix", a.v, b.v);
    CHECK_EQ  ("same len", a.len, b.len);
    fs2_free(&a);
    fs2_free(&b);

    fs2_free(&p);
    assert_balanced();
}

Test(fs2_api_modificators, strip_suffix_str_matches_fs2_version) {
    __fs2_checkreset();
    fs2 p = fs2_fromstr("lo");

    fs2 a = fs2_fromstr("hello");
    fs2 b = fs2_fromstr("hello");
    fs2_strip_suffix_str(&a, "lo");
    fs2_strip_suffix(&b, &p);
    CHECK_STR_EQ("strip_suffix_str == strip_suffix", a.v, b.v);
    CHECK_EQ  ("same len", a.len, b.len);
    fs2_free(&a);
    fs2_free(&b);

    fs2_free(&p);
    assert_balanced();
}

/* =========================================================================
 * Whole string, empty src (FS2() / FS2EMPTY) — no deref, no crash
 * ========================================================================= */

Test(fs2_api_modificators, strip_prefix_str_whole_string) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");

    fs2_strip_prefix_str(&s, "abc");

    CHECK_EQ  ("len",   s.len, 0);
    CHECK_TRUE("empty", fs2_is_empty(&s));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_modificators, strip_suffix_str_whole_string) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");

    fs2_strip_suffix_str(&s, "abc");

    CHECK_EQ  ("len",   s.len, 0);
    CHECK_TRUE("empty", fs2_is_empty(&s));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_modificators, strip_str_empty_src_null_affix) {
    __fs2_checkreset();
    fs2 canon = FS2();
    fs2 empty = FS2EMPTY;

    fs2_strip_prefix_str(&canon, NULL);
    fs2_strip_suffix_str(&canon, NULL);
    CHECK_TRUE("canon empty", fs2_is_canon(&canon));

    fs2_strip_prefix_str(&empty, NULL);
    fs2_strip_suffix_str(&empty, NULL);
    CHECK_TRUE("empty empty", fs2_eq_empty(&empty));

    fs2_free(&canon);
    assert_balanced();
}

/* =========================================================================
 * NULL s still aborts (inherited)
 * ========================================================================= */

Test(fs2_api_modificators, strip_prefix_str_null_s_aborts, .signal = SIGABRT) {
    fs2_strip_prefix_str(NULL, "a");
}

Test(fs2_api_modificators, strip_suffix_str_null_s_aborts, .signal = SIGABRT) {
    fs2_strip_suffix_str(NULL, "a");
}

/* =========================================================================
 * fs2_strip_prefix_new — nominal (match path: substr_new, exact-fit)
 * ========================================================================= */

Test(fs2_api_modificators, strip_prefix_new_basic) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello world");
    fs2 p   = fs2_fromstr("hello ");

    fs2 r = fs2_strip_prefix_new(&src, &p);

    CHECK_STR_EQ("world", r.v, "world");
    CHECK_EQ  ("len",     r.len, 5);
    CHECK_TRUE ("ALLOC",           fs2_is_alloc(&r));
    CHECK_FALSE("not bodyalloc",   fs2_is_bodyalloc(&r));
    CHECK_TRUE ("independent",     r.v != src.v);
    CHECK_FS2VALID(1, &r);
    CHECK_FS2VALID(2, &src);
    CHECK_FS2VALID(3, &p);

    fs2_free(&src);
    fs2_free(&p);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api_modificators, strip_suffix_new_basic) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello world");
    fs2 p   = fs2_fromstr(" world");

    fs2 r = fs2_strip_suffix_new(&src, &p);

    CHECK_STR_EQ("hello", r.v, "hello");
    CHECK_EQ  ("len",     r.len, 5);
    CHECK_TRUE ("ALLOC",           fs2_is_alloc(&r));
    CHECK_FALSE("not bodyalloc",   fs2_is_bodyalloc(&r));
    CHECK_FS2VALID(1, &r);
    CHECK_FS2VALID(2, &src);

    fs2_free(&src);
    fs2_free(&p);
    fs2_free(&r);
    assert_balanced();
}

/* Whole string stripped -> empty result */
Test(fs2_api_modificators, strip_prefix_new_whole_string) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("abc");
    fs2 p   = fs2_fromstr("abc");

    fs2 r = fs2_strip_prefix_new(&src, &p);

    CHECK_EQ  ("len",   r.len, 0);
    CHECK_TRUE("empty", fs2_is_empty(&r));
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    fs2_free(&p);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api_modificators, strip_suffix_new_whole_string) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("abc");
    fs2 p   = fs2_fromstr("abc");

    fs2 r = fs2_strip_suffix_new(&src, &p);

    CHECK_EQ  ("len",   r.len, 0);
    CHECK_TRUE("empty", fs2_is_empty(&r));
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    fs2_free(&p);
    fs2_free(&r);
    assert_balanced();
}

/* Only ONE strip, not recursive: "aaa" - "aa" -> "a" */
Test(fs2_api_modificators, strip_prefix_new_only_one_occurrence) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("aaa");
    fs2 p   = fs2_fromstr("aa");

    fs2 r = fs2_strip_prefix_new(&src, &p);

    CHECK_STR_EQ("a", r.v, "a");   /* not "" */
    fs2_free(&src);
    fs2_free(&p);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api_modificators, strip_suffix_new_only_one_occurrence) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("aaa");
    fs2 p   = fs2_fromstr("aa");

    fs2 r = fs2_strip_suffix_new(&src, &p);

    CHECK_STR_EQ("a", r.v, "a");
    fs2_free(&src);
    fs2_free(&p);
    fs2_free(&r);
    assert_balanced();
}

/* =========================================================================
 * no-op paths (clone of src)
 * ========================================================================= */

Test(fs2_api_modificators, strip_prefix_new_no_match_clones) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello");
    fs2 p   = fs2_fromstr("xyz");

    fs2 r = fs2_strip_prefix_new(&src, &p);

    CHECK_STR_EQ("hello",     r.v, "hello");
    CHECK_TRUE ("ALLOC",      fs2_is_alloc(&r));
    CHECK_TRUE ("independent", r.v != src.v);
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    fs2_free(&p);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api_modificators, strip_prefix_new_empty_affix_clones) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello");
    fs2 e   = FS2EMPTY;
    fs2 c   = FS2();

    fs2 r1 = fs2_strip_prefix_new(&src, &e);
    fs2 r2 = fs2_strip_prefix_new(&src, &c);

    CHECK_STR_EQ("FS2EMPTY affix", r1.v, "hello");
    CHECK_STR_EQ("FS2() affix",    r2.v, "hello");
    CHECK_FS2VALID(1, &r1);
    CHECK_FS2VALID(2, &r2);

    fs2_free(&src);
    fs2_free(&c);
    fs2_free(&r1);
    fs2_free(&r2);
    assert_balanced();
}

Test(fs2_api_modificators, strip_suffix_new_empty_affix_clones) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello");
    fs2 e   = FS2EMPTY;

    fs2 r = fs2_strip_suffix_new(&src, &e);

    CHECK_STR_EQ("hello", r.v, "hello");
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    fs2_free(&r);
    assert_balanced();
}

/* affix longer than src -> no match -> clone (no underflow in src->len - prefix->len) */
Test(fs2_api_modificators, strip_prefix_new_affix_longer_clones) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("ab");
    fs2 p   = fs2_fromstr("abc");

    fs2 r = fs2_strip_prefix_new(&src, &p);

    CHECK_STR_EQ("ab", r.v, "ab");
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    fs2_free(&p);
    fs2_free(&r);
    assert_balanced();
}

/* =========================================================================
 * empty src -> clone -> FS2EMPTY (normalization)
 * ========================================================================= */

Test(fs2_api_modificators, strip_prefix_new_empty_src_returns_empty) {
    __fs2_checkreset();
    fs2 p = fs2_fromstr("a");

    fs2 c = FS2();
    fs2 e = FS2EMPTY;

    fs2 rc = fs2_strip_prefix_new(&c, &p);
    fs2 re = fs2_strip_prefix_new(&e, &p);

    CHECK_TRUE("FS2() src -> FS2EMPTY", fs2_eq_empty(&rc));
    CHECK_TRUE("FS2EMPTY src -> FS2EMPTY", fs2_eq_empty(&re));
    CHECK_FS2VALID(1, &rc);
    CHECK_FS2VALID(2, &re);

    fs2_free(&c);
    fs2_free(&rc);
    fs2_free(&re);
    fs2_free(&p);
    assert_balanced();
}

/* =========================================================================
 * STATIC / LOCAL src — works (this is a constructor, not mutator)
 * ========================================================================= */

Test(fs2_api_modificators, strip_prefix_new_static_src) {
    __fs2_checkreset();
    fs2 src = FS2LITERAL("hello world");
    fs2 p   = fs2_fromstr("hello ");

    fs2 r = fs2_strip_prefix_new(&src, &p);

    CHECK_STR_EQ("world",     r.v, "world");
    CHECK_TRUE ("ALLOC",      fs2_is_alloc(&r));
    CHECK_TRUE ("independent", r.v != src.v);
    CHECK_TRUE ("src still STATIC", fs2_is_static(&src));
    CHECK_FS2VALID(1, &r);

    fs2_free(&p);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api_modificators, strip_suffix_new_static_src) {
    __fs2_checkreset();
    fs2 src = FS2LITERAL("hello world");
    fs2 p   = fs2_fromstr(" world");

    fs2 r = fs2_strip_suffix_new(&src, &p);

    CHECK_STR_EQ("hello", r.v, "hello");
    CHECK_FS2VALID(1, &r);

    fs2_free(&p);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api_modificators, strip_prefix_new_local_src) {
    __fs2_checkreset();
    FS2LOCAL(buf, 16);
    fs2_memcpy_str(&buf, 0, "hello world", 11);
    fs2_setlen(&buf, 11);
    fs2 p = fs2_fromstr("hello ");

    fs2 r = fs2_strip_prefix_new(&buf, &p);

    CHECK_STR_EQ("world",         r.v,   "world");
    CHECK_TRUE ("result ALLOC",   fs2_is_alloc(&r));
    CHECK_TRUE ("result not LOCAL", !fs2_is_local(&r));
    CHECK_STR_EQ("buf untouched", buf.v, "hello world");
    CHECK_FS2VALID(1, &r);
    CHECK_FS2VALID(2, &buf);

    fs2_free(&p);
    fs2_free(&r);
    assert_balanced();
}

/* =========================================================================
 * guards
 * ========================================================================= */

Test(fs2_api_modificators, strip_prefix_new_null_src_aborts, .signal = SIGABRT) {
    fs2 p = fs2_fromstr("a");
    fs2_strip_prefix_new(NULL, &p);
    fs2_free(&p);
}

Test(fs2_api_modificators, strip_prefix_new_null_prefix_aborts, .signal = SIGABRT) {
    fs2 src = fs2_fromstr("abc");
    fs2_strip_prefix_new(&src, NULL);
    fs2_free(&src);
}

Test(fs2_api_modificators, strip_suffix_new_null_src_aborts, .signal = SIGABRT) {
    fs2 p = fs2_fromstr("a");
    fs2_strip_suffix_new(NULL, &p);
    fs2_free(&p);
}

Test(fs2_api_modificators, strip_suffix_new_null_suffix_aborts, .signal = SIGABRT) {
    fs2 src = fs2_fromstr("abc");
    fs2_strip_suffix_new(&src, NULL);
    fs2_free(&src);
}

/* =========================================================================
 * src untouched / independent buffer
 * ========================================================================= */

Test(fs2_api_modificators, strip_new_src_untouched_and_independent) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello world");
    fs2 p   = fs2_fromstr("hello ");
    void *orig = src.v;

    fs2 r = fs2_strip_prefix_new(&src, &p);

    CHECK_PTR_EQ("src v unchanged", src.v, orig);
    CHECK_STR_EQ("src content",     src.v, "hello world");
    CHECK_STR_EQ("result",          r.v,   "world");
    CHECK_TRUE ("independent",      r.v != src.v);

    r.v[0] = 'X';
    CHECK_STR_EQ("result mutated", r.v,   "Xorld");
    CHECK_STR_EQ("src original",   src.v, "hello world");

    CHECK_FS2VALID(1, &r);
    CHECK_FS2VALID(2, &src);
    CHECK_FS2VALID(3, &p);

    fs2_free(&src);
    fs2_free(&p);
    fs2_free(&r);
    assert_balanced();
}

/* =========================================================================
 * Consistency: strip_*_new(src, pat) ≡ strip_*(clone(src), pat)
 * ========================================================================= */

Test(fs2_api_modificators, strip_prefix_new_matches_inplace_on_clone) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello world");
    fs2 p   = fs2_fromstr("hello ");

    fs2 manual = fs2_clone(&src);
    fs2_strip_prefix(&manual, &p);

    fs2 via = fs2_strip_prefix_new(&src, &p);

    CHECK_STR_EQ("same content", manual.v,   via.v);
    CHECK_EQ  ("same len",       manual.len, via.len);
    CHECK_FS2VALID(1, &manual);
    CHECK_FS2VALID(2, &via);

    fs2_free(&src);
    fs2_free(&p);
    fs2_free(&manual);
    fs2_free(&via);
    assert_balanced();
}

Test(fs2_api_modificators, strip_suffix_new_matches_inplace_on_clone) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello world");
    fs2 p   = fs2_fromstr(" world");

    fs2 manual = fs2_clone(&src);
    fs2_strip_suffix(&manual, &p);

    fs2 via = fs2_strip_suffix_new(&src, &p);

    CHECK_STR_EQ("same content", manual.v,   via.v);
    CHECK_EQ  ("same len",       manual.len, via.len);
    CHECK_FS2VALID(1, &manual);
    CHECK_FS2VALID(2, &via);

    fs2_free(&src);
    fs2_free(&p);
    fs2_free(&manual);
    fs2_free(&via);
    assert_balanced();
}

/* =========================================================================
 * strip_*_str_new — minimal: only the _fs2_wrap(NULL) behaviour differs
 *   (NULL affix -> FS2EMPTY -> clone of src; core fs2-version aborts)
 * ========================================================================= */

Test(fs2_api_modificators, strip_prefix_str_new_null_affix_clones) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello");

    fs2 r = fs2_strip_prefix_str_new(&src, NULL);

    CHECK_STR_EQ("hello",     r.v, "hello");
    CHECK_TRUE ("ALLOC",      fs2_is_alloc(&r));
    CHECK_TRUE ("independent", r.v != src.v);
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api_modificators, strip_suffix_str_new_null_affix_clones) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello");

    fs2 r = fs2_strip_suffix_str_new(&src, NULL);

    CHECK_STR_EQ("hello", r.v, "hello");
    CHECK_TRUE ("independent", r.v != src.v);
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    fs2_free(&r);
    assert_balanced();
}

/* "" behaves the same as NULL */
Test(fs2_api_modificators, strip_str_new_empty_affix_clones) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello");

    fs2 r1 = fs2_strip_prefix_str_new(&src, "");
    fs2 r2 = fs2_strip_suffix_str_new(&src, "");

    CHECK_STR_EQ("prefix \"\"", r1.v, "hello");
    CHECK_STR_EQ("suffix \"\"", r2.v, "hello");

    fs2_free(&src);
    fs2_free(&r1);
    fs2_free(&r2);
    assert_balanced();
}

/* =========================================================================
 * Contrast: core fs2-version aborts on NULL affix
 * ========================================================================= */

Test(fs2_api_modificators, strip_prefix_new_null_affix_aborts, .signal = SIGABRT) {
    fs2 src = fs2_fromstr("abc");
    fs2_strip_prefix_new(&src, NULL);
    fs2_free(&src);
}

Test(fs2_api_modificators, strip_suffix_new_null_affix_aborts, .signal = SIGABRT) {
    fs2 src = fs2_fromstr("abc");
    fs2_strip_suffix_new(&src, NULL);
    fs2_free(&src);
}

/* =========================================================================
 * Equivalence: _str ≡ fs2-version on non-NULL affix
 * ========================================================================= */

Test(fs2_api_modificators, strip_str_new_matches_fs2_version) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello world");
    fs2 pfx = fs2_fromstr("hello ");
    fs2 sfx = fs2_fromstr(" world");

    fs2 a = fs2_strip_prefix_str_new(&src, "hello ");
    fs2 b = fs2_strip_prefix_new(&src, &pfx);
    CHECK_STR_EQ("prefix match", a.v, b.v);
    CHECK_EQ  ("prefix len",     a.len, b.len);
    fs2_free(&a);
    fs2_free(&b);

    fs2 c = fs2_strip_suffix_str_new(&src, " world");
    fs2 d = fs2_strip_suffix_new(&src, &sfx);
    CHECK_STR_EQ("suffix match", c.v, d.v);
    CHECK_EQ  ("suffix len",     c.len, d.len);
    fs2_free(&c);
    fs2_free(&d);

    fs2_free(&src);
    fs2_free(&pfx);
    fs2_free(&sfx);
    assert_balanced();
}

/* =========================================================================
 * fs2_initcap — nominal (running in C locale; see test_setup)
 * ========================================================================= */

Test(fs2_api_modificators, initcap_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello world");

    fs2 *r = fs2_initcap(&s);

    CHECK_STR_EQ("Hello World", s.v, "Hello World");
    CHECK_PTR_EQ("chaining", r, &s);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_modificators, initcap_all_upper) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("HELLO WORLD");
    fs2_initcap(&s);
    CHECK_STR_EQ("Hello World", s.v, "Hello World");
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_modificators, initcap_mixed_case) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hELLo WoRLD");
    fs2_initcap(&s);
    CHECK_STR_EQ("Hello World", s.v, "Hello World");
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_modificators, initcap_single_word) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");
    fs2_initcap(&s);
    CHECK_STR_EQ("Hello", s.v, "Hello");
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_modificators, initcap_single_char) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("h");
    fs2 b = fs2_fromstr("H");
    fs2 c = fs2_fromstr("1");
    fs2 d = fs2_fromstr("!");

    fs2_initcap(&a); fs2_initcap(&b); fs2_initcap(&c); fs2_initcap(&d);

    CHECK_STR_EQ("h", a.v, "H");
    CHECK_STR_EQ("H", b.v, "H");
    CHECK_STR_EQ("1", c.v, "1");
    CHECK_STR_EQ("!", d.v, "!");

    fs2_free(&a); fs2_free(&b); fs2_free(&c); fs2_free(&d);
    assert_balanced();
}

/* =========================================================================
 * Digits are alnum (PostgreSQL semantics)
 * ========================================================================= */

Test(fs2_api_modificators, initcap_digits_are_part_of_word) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc123def");
    fs2_initcap(&s);
    CHECK_STR_EQ("Abc123def", s.v, "Abc123def");
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_modificators, initcap_leading_digit_no_change) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("123abc");
    fs2_initcap(&s);
    /* '1' is alnum -> toupper('1') == '1'; rest lowercased (already) */
    CHECK_STR_EQ("123abc", s.v, "123abc");
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * Separators: space, punctuation, apostrophe
 * ========================================================================= */

Test(fs2_api_modificators, initcap_multiple_spaces) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("a  b   c");
    fs2_initcap(&s);
    CHECK_STR_EQ("A  B   C", s.v, "A  B   C");
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_modificators, initcap_leading_trailing_spaces) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("  hello  ");
    fs2_initcap(&s);
    CHECK_STR_EQ("  Hello  ", s.v, "  Hello  ");
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_modificators, initcap_punctuation_separators) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello-world!foo,bar");
    fs2_initcap(&s);
    CHECK_STR_EQ("Hello-World!Foo,Bar", s.v, "Hello-World!Foo,Bar");
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_modificators, initcap_apostrophe_is_separator) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("can't");
    fs2_initcap(&s);
    /* PostgreSQL: non-alnum = separator -> 't' starts a new word */
    CHECK_STR_EQ("Can'T", s.v, "Can'T");
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_modificators, initcap_underscore_is_separator) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("_abc_");
    fs2_initcap(&s);
    CHECK_STR_EQ("_Abc_", s.v, "_Abc_");
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * PostgreSQL docstring example
 * ========================================================================= */

Test(fs2_api_modificators, initcap_pg_docstring_example) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hi THOMAS");
    fs2_initcap(&s);
    CHECK_STR_EQ("Hi Thomas", s.v, "Hi Thomas");
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * Empty s (FS2 / FS2EMPTY / FS2LITERAL("")) — no-op
 * ========================================================================= */

Test(fs2_api_modificators, initcap_fs2empty_noop) {
    __fs2_checkreset();
    fs2 s = FS2EMPTY;
    fs2_initcap(&s);
    CHECK_TRUE("still eq_empty", fs2_eq_empty(&s));
    CHECK_FS2VALID(1, &s);
    assert_balanced();
}

Test(fs2_api_modificators, initcap_canon_noop) {
    __fs2_checkreset();
    fs2 s = FS2();
    fs2_initcap(&s);
    CHECK_TRUE("still canon", fs2_is_canon(&s));
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_modificators, initcap_static_empty_noop) {
    __fs2_checkreset();
    fs2 s = FS2LITERAL("");
    fs2_initcap(&s);
    CHECK_EQ  ("len",    s.len, 0);
    CHECK_TRUE("STATIC", fs2_is_static(&s));
    CHECK_FS2VALID(1, &s);
    assert_balanced();
}

/* =========================================================================
 * STATIC non-empty -> eager abort (fs2_cant_mutate)
 * ========================================================================= */

Test(fs2_api_modificators, initcap_static_nonempty_aborts, .signal = SIGABRT) {
    fs2 s = FS2LITERAL("hello");
    fs2_initcap(&s);
}

/* =========================================================================
 * LOCAL — works (same-size transform, no realloc)
 * ========================================================================= */

Test(fs2_api_modificators, initcap_local) {
    __fs2_checkreset();
    FS2LOCAL(buf, 16);
    fs2_memcpy_str(&buf, 0, "hello world", 11);
    fs2_setlen(&buf, 11);

    fs2_initcap(&buf);

    CHECK_STR_EQ("Hello World", buf.v, "Hello World");
    CHECK_TRUE ("still LOCAL",  fs2_is_local(&buf));
    CHECK_FS2VALID(1, &buf);

    assert_balanced();
}

/* =========================================================================
 * Guards
 * ========================================================================= */

Test(fs2_api_modificators, initcap_null_aborts, .signal = SIGABRT) {
    fs2_initcap(NULL);
}

/* =========================================================================
 * Idempotency: initcap(initcap(s)) == initcap(s)
 * ========================================================================= */

Test(fs2_api_modificators, initcap_idempotent) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hELLo WoRLD");
    fs2_initcap(&s);
    char *first = strdup(s.v);
    cr_assert_not_null(first);
    fs2_initcap(&s);
    CHECK_STR_EQ("idempotent", s.v, first);
    free(first);
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * High byte in C locale: unchanged, treated as separator
 *   (in a non-C locale this may differ — see docstring note)
 * ========================================================================= */

Test(fs2_api_modificators, initcap_high_byte_c_locale_unchanged) {
    __fs2_checkreset();
    char *raw = strdup("ab\xE9" "cd");
    cr_assert(raw != NULL);
    fs2 s = fs2_adopt(&raw);
    cr_assert(raw == NULL);

    fs2_initcap(&s);

    /* 'A', 'b', 0xE9 (separator in C locale, unchanged),
     * 'C' (new word start after 0xE9), 'd' */
    CHECK_EQ("'A'",            s.v[0], 'A');
    CHECK_EQ("'b'",            s.v[1], 'b');
    CHECK_EQ("0xE9 unchanged", (unsigned char)s.v[2], 0xE9u);
    CHECK_EQ("'C'",            s.v[3], 'C');
    CHECK_EQ("'d'",            s.v[4], 'd');
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_initcap_new — nominal
 * ========================================================================= */

Test(fs2_api_modificators, initcap_new_basic) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello world");

    fs2 r = fs2_initcap_new(&src);

    CHECK_STR_EQ("Hello World", r.v, "Hello World");
    CHECK_EQ  ("len",           r.len, 11);
    CHECK_TRUE ("ALLOC",             fs2_is_alloc(&r));
    CHECK_FALSE("not BODYALLOC",     fs2_is_bodyalloc(&r));
    CHECK_TRUE ("independent",       r.v != src.v);
    CHECK_FS2VALID(1, &r);
    CHECK_FS2VALID(2, &src);

    fs2_free(&src);
    fs2_free(&r);
    assert_balanced();
}

/* =========================================================================
 * src untouched — the core difference vs in-place fs2_initcap
 * ========================================================================= */

Test(fs2_api_modificators, initcap_new_src_untouched) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hELLo WoRLD");
    void *orig = src.v;

    fs2 r = fs2_initcap_new(&src);

    CHECK_PTR_EQ("src v unchanged", src.v, orig);
    CHECK_STR_EQ("src content",     src.v, "hELLo WoRLD");   /* NOT capitalized */
    CHECK_STR_EQ("result",          r.v,   "Hello World");
    CHECK_TRUE ("independent",      r.v != src.v);

    /* Mutating result must not affect src */
    r.v[0] = 'X';
    CHECK_STR_EQ("result mutated", r.v,   "Xello World");
    CHECK_STR_EQ("src original",   src.v, "hELLo WoRLD");

    CHECK_FS2VALID(1, &r);
    CHECK_FS2VALID(2, &src);

    fs2_free(&src);
    fs2_free(&r);
    assert_balanced();
}

/* =========================================================================
 * STATIC / LOCAL src — clone removes the mutate barrier
 * ========================================================================= */

Test(fs2_api_modificators, initcap_new_static_src) {
    __fs2_checkreset();
    fs2 src = FS2LITERAL("hello world");

    fs2 r = fs2_initcap_new(&src);

    CHECK_STR_EQ("Hello World", r.v, "Hello World");
    CHECK_TRUE ("ALLOC",         fs2_is_alloc(&r));
    CHECK_TRUE ("independent",   r.v != src.v);
    CHECK_TRUE ("src still STATIC", fs2_is_static(&src));
    CHECK_FS2VALID(1, &r);
    CHECK_FS2VALID(2, &src);

    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api_modificators, initcap_new_local_src) {
    __fs2_checkreset();
    FS2LOCAL(buf, 16);
    fs2_memcpy_str(&buf, 0, "hello world", 11);
    fs2_setlen(&buf, 11);

    fs2 r = fs2_initcap_new(&buf);

    CHECK_STR_EQ("Hello World",     r.v,   "Hello World");
    CHECK_TRUE ("result ALLOC",     fs2_is_alloc(&r));
    CHECK_TRUE ("result not LOCAL", !fs2_is_local(&r));
    CHECK_STR_EQ("buf untouched",   buf.v, "hello world");
    CHECK_FS2VALID(1, &r);
    CHECK_FS2VALID(2, &buf);

    fs2_free(&r);
    assert_balanced();
}

/* =========================================================================
 * empty src -> clone -> FS2EMPTY (normalization)
 * ========================================================================= */

Test(fs2_api_modificators, initcap_new_canon_src_returns_empty) {
    __fs2_checkreset();
    fs2 src = FS2();

    fs2 r = fs2_initcap_new(&src);

    CHECK_TRUE("eq_empty", fs2_eq_empty(&r));
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    assert_balanced();
}

Test(fs2_api_modificators, initcap_new_fs2empty_src_returns_empty) {
    __fs2_checkreset();
    fs2 src = FS2EMPTY;

    fs2 r = fs2_initcap_new(&src);

    CHECK_TRUE("eq_empty", fs2_eq_empty(&r));
    CHECK_FS2VALID(1, &r);

    assert_balanced();
}

Test(fs2_api_modificators, initcap_new_static_empty_literal_returns_empty) {
    __fs2_checkreset();
    fs2 src = FS2LITERAL("");

    fs2 r = fs2_initcap_new(&src);

    CHECK_TRUE("eq_empty", fs2_eq_empty(&r));
    CHECK_FS2VALID(1, &r);

    assert_balanced();
}

/* =========================================================================
 * guards
 * ========================================================================= */

Test(fs2_api_modificators, initcap_new_null_src_aborts, .signal = SIGABRT) {
    fs2_initcap_new(NULL);
}

/* =========================================================================
 * Consistency: _new ≡ clone + in-place initcap
 * ========================================================================= */

Test(fs2_api_modificators, initcap_new_matches_clone_then_initcap) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hi THOMAS");

    fs2 manual = fs2_clone(&src);
    fs2_initcap(&manual);

    fs2 via = fs2_initcap_new(&src);

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
 * Chaining with other _new constructors
 * ========================================================================= */

Test(fs2_api_modificators, initcap_new_of_strip_prefix_new) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("prefix:hello world");
    fs2 pfx = fs2_fromstr("prefix:");

    fs2 stripped = fs2_strip_prefix_new(&src, &pfx);
    fs2 result   = fs2_initcap_new(&stripped);

    CHECK_STR_EQ("Hello World", result.v, "Hello World");
    CHECK_FS2VALID(1, &result);
    CHECK_FS2VALID(2, &stripped);

    fs2_free(&src);
    fs2_free(&pfx);
    fs2_free(&stripped);
    fs2_free(&result);
    assert_balanced();
}


/* =========================================================================
 * fs2_removeall — nominal (single char set)
 * ========================================================================= */

Test(fs2_api_modificators, removeall_single_char) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("axbxc");
    fs2 c = fs2_fromstr("x");

    fs2 *r = fs2_removeall(&s, &c);

    CHECK_STR_EQ("abc",    s.v, "abc");
    CHECK_EQ  ("len",      s.len, 3);
    CHECK_PTR_EQ("chain",  r, &s);
    CHECK_FS2VALID(1, &s);
    CHECK_FS2VALID(2, &c);

    fs2_free(&s);
    fs2_free(&c);
    assert_balanced();
}

Test(fs2_api_modificators, removeall_multichar_set) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello world");
    fs2 c = fs2_fromstr("aeiou");

    fs2_removeall(&s, &c);

    CHECK_STR_EQ("hll wrld", s.v, "hll wrld");

    fs2_free(&s);
    fs2_free(&c);
    assert_balanced();
}

Test(fs2_api_modificators, removeall_no_match) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");
    fs2 c = fs2_fromstr("xyz");
    void *orig = s.v;

    fs2_removeall(&s, &c);

    CHECK_PTR_EQ("buffer untouched", s.v, orig);
    CHECK_STR_EQ("abc",              s.v, "abc");

    fs2_free(&s);
    fs2_free(&c);
    assert_balanced();
}

Test(fs2_api_modificators, removeall_all_chars) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");
    fs2 c = fs2_fromstr("abc");

    fs2_removeall(&s, &c);

    CHECK_EQ  ("len",    s.len, 0);
    CHECK_TRUE("empty",  fs2_is_empty(&s));
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    fs2_free(&c);
    assert_balanced();
}

Test(fs2_api_modificators, removeall_duplicates_in_chars) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("axbxc");
    fs2 c = fs2_fromstr("xxxx");   /* set {x}, four copies */

    fs2_removeall(&s, &c);

    CHECK_STR_EQ("abc", s.v, "abc");

    fs2_free(&s);
    fs2_free(&c);
    assert_balanced();
}

/* All 256 byte values in a charset -> result empty, no UB */
Test(fs2_api_modificators, removeall_all_256_bytes) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");

    /* Every byte 1..255 in a C-string; NUL-terminated at [n].
     * No embedded NUL because we start at 1 — so fs2_fromstr
     * (which uses strlen) works correctly. */
    char raw[256];
    size_t n = 0;
    for (int b = 1; b < 256; b++) raw[n++] = (char)b;
    raw[n] = '\0';

    fs2 c = fs2_fromstr(raw);

    fs2_removeall(&s, &c);

    CHECK_EQ  ("len",   s.len, 0);
    CHECK_TRUE("empty", fs2_is_empty(&s));

    fs2_free(&s);
    fs2_free(&c);
    assert_balanced();
}

/* =========================================================================
 * Only edge chars removed — leading, trailing, first, last
 * ========================================================================= */

Test(fs2_api_modificators, removeall_leading) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("xxxabc");
    fs2 c = fs2_fromstr("x");

    fs2_removeall(&s, &c);
    CHECK_STR_EQ("abc", s.v, "abc");

    fs2_free(&s);
    fs2_free(&c);
    assert_balanced();
}

Test(fs2_api_modificators, removeall_trailing) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abcxxx");
    fs2 c = fs2_fromstr("x");

    fs2_removeall(&s, &c);
    CHECK_STR_EQ("abc", s.v, "abc");

    fs2_free(&s);
    fs2_free(&c);
    assert_balanced();
}

/* =========================================================================
 * chars == s content — all bytes in s are in chars -> result empty
 *   (DISTINCT objects; passing same pointer is UB by restrict)
 * ========================================================================= */

Test(fs2_api_modificators, removeall_chars_equals_content) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abcabc");
    fs2 c = fs2_fromstr("abcabc");     /* same content, distinct object */

    fs2_removeall(&s, &c);

    CHECK_EQ  ("len",   s.len, 0);
    CHECK_TRUE("empty", fs2_is_empty(&s));

    fs2_free(&s);
    fs2_free(&c);
    assert_balanced();
}

/* =========================================================================
 * Empty chars / empty s — early no-op
 * ========================================================================= */

Test(fs2_api_modificators, removeall_empty_chars_noop) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");
    fs2 e = FS2EMPTY;
    fs2 c = FS2();
    void *orig = s.v;

    fs2_removeall(&s, &e);
    CHECK_PTR_EQ("FS2EMPTY noop", s.v, orig);
    fs2_removeall(&s, &c);
    CHECK_PTR_EQ("FS2() noop",    s.v, orig);

    CHECK_STR_EQ("hello", s.v, "hello");
    fs2_free(&s);
    fs2_free(&c);
    assert_balanced();
}

Test(fs2_api_modificators, removeall_empty_src_noop) {
    __fs2_checkreset();
    fs2 c = fs2_fromstr("x");

    fs2 canon = FS2();
    fs2 empty = FS2EMPTY;

    fs2_removeall(&canon, &c);
    fs2_removeall(&empty, &c);

    CHECK_TRUE("canon still canon", fs2_is_canon(&canon));
    CHECK_TRUE("empty still empty", fs2_eq_empty(&empty));

    fs2_free(&canon);
    fs2_free(&c);
    assert_balanced();
}

/* =========================================================================
 * STATIC non-empty: eager abort (fs2_cant_mutate)
 * ========================================================================= */

Test(fs2_api_modificators, removeall_static_no_match_aborts, .signal = SIGABRT) {
    fs2 s = FS2LITERAL("hello");
    fs2 c = fs2_fromstr("xyz");
    fs2_removeall(&s, &c);      /* eager: aborts even though no match */
    fs2_free(&c);
}

Test(fs2_api_modificators, removeall_static_match_aborts, .signal = SIGABRT) {
    fs2 s = FS2LITERAL("hello");
    fs2 c = fs2_fromstr("l");
    fs2_removeall(&s, &c);
    fs2_free(&c);
}

Test(fs2_api_modificators, removeall_static_empty_literal_noop) {
    __fs2_checkreset();
    fs2 s = FS2LITERAL("");
    fs2 c = fs2_fromstr("x");

    fs2_removeall(&s, &c);

    CHECK_EQ  ("len",    s.len, 0);
    CHECK_TRUE("STATIC", fs2_is_static(&s));
    CHECK_FS2VALID(1, &s);

    fs2_free(&c);
    assert_balanced();
}

/* =========================================================================
 * LOCAL src — only shrinks, works
 * ========================================================================= */

Test(fs2_api_modificators, removeall_local) {
    __fs2_checkreset();
    FS2LOCAL(buf, 16);
    fs2_memcpy_str(&buf, 0, "hello world", 11);
    fs2_setlen(&buf, 11);
    fs2 c = fs2_fromstr("aeiou");

    fs2_removeall(&buf, &c);

    CHECK_STR_EQ("hll wrld",   buf.v, "hll wrld");
    CHECK_TRUE ("still LOCAL", fs2_is_local(&buf));
    CHECK_FS2VALID(1, &buf);

    fs2_free(&c);
    assert_balanced();
}

/* =========================================================================
 * Guards
 * ========================================================================= */

Test(fs2_api_modificators, removeall_null_s_aborts, .signal = SIGABRT) {
    fs2 c = fs2_fromstr("x");
    fs2_removeall(NULL, &c);
    fs2_free(&c);
}

Test(fs2_api_modificators, removeall_null_chars_aborts, .signal = SIGABRT) {
    fs2 s = fs2_fromstr("abc");
    fs2_removeall(&s, NULL);
    fs2_free(&s);
}

/* =========================================================================
 * chars untouched, s buffer not realloc
 * ========================================================================= */

Test(fs2_api_modificators, removeall_chars_untouched) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("axbxc");
    fs2 c = fs2_fromstr("x");
    void *c_v = c.v;

    fs2_removeall(&s, &c);

    CHECK_PTR_EQ("c.v unchanged", c.v, c_v);
    CHECK_STR_EQ("c content",     c.v, "x");
    CHECK_FS2VALID(1, &c);

    fs2_free(&s);
    fs2_free(&c);
    assert_balanced();
}

/* =========================================================================
 * fs2_removeall_new — nominal
 * ========================================================================= */

Test(fs2_api_modificators, removeall_new_nothing_to_remove_clones) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello");
    fs2 c   = fs2_fromstr("xyz");

    fs2 r = fs2_removeall_new(&src, &c);

    CHECK_STR_EQ("hello",     r.v, "hello");
    CHECK_EQ  ("len",         r.len, 5);
    CHECK_TRUE("ALLOC",       fs2_is_alloc(&r));
    CHECK_TRUE("independent", r.v != src.v);
    CHECK_FS2VALID(1, &r);
    CHECK_FS2VALID(2, &src);

    fs2_free(&src);
    fs2_free(&c);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api_modificators, removeall_new_vowels) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello world");
    fs2 c   = fs2_fromstr("aeiou");

    fs2 r = fs2_removeall_new(&src, &c);

    CHECK_STR_EQ("hll wrld", r.v, "hll wrld");
    CHECK_EQ  ("len",        r.len, 8);
    CHECK_TRUE("ALLOC",      fs2_is_alloc(&r));   /* promoted by elem() */
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    fs2_free(&c);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api_modificators, removeall_new_leading_trailing) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("xxxabc");
    fs2 b = fs2_fromstr("abcxxx");
    fs2 c = fs2_fromstr("x");

    fs2 ra = fs2_removeall_new(&a, &c);
    fs2 rb = fs2_removeall_new(&b, &c);

    CHECK_STR_EQ("leading",  ra.v, "abc");
    CHECK_STR_EQ("trailing", rb.v, "abc");

    fs2_free(&a); fs2_free(&b); fs2_free(&c);
    fs2_free(&ra); fs2_free(&rb);
    assert_balanced();
}

/* =========================================================================
 * All removed -> FS2EMPTY (not FS2(); res never promoted by elem)
 * ========================================================================= */

Test(fs2_api_modificators, removeall_new_all_removed_is_fs2empty) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("abc");
    fs2 c   = fs2_fromstr("abc");

    fs2 r = fs2_removeall_new(&src, &c);

    CHECK_EQ  ("len",              r.len, 0);
    CHECK_TRUE("eq_empty",         fs2_eq_empty(&r));    /* v="" STATIC sz=1 */
    CHECK_TRUE("v readable",       r.v != NULL);
    CHECK_TRUE("empty",            fs2_is_empty(&r));
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    fs2_free(&c);
    assert_balanced();
}

/* =========================================================================
 * Empty chars — early clone of src (guard against v==NULL in fill_table)
 * ========================================================================= */

Test(fs2_api_modificators, removeall_new_empty_chars_fs2empty) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello");
    fs2 e   = FS2EMPTY;

    fs2 r = fs2_removeall_new(&src, &e);

    CHECK_STR_EQ("hello",     r.v, "hello");
    CHECK_TRUE("independent", r.v != src.v);
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    fs2_free(&r);
    assert_balanced();
}

/* chars = FS2() (v==NULL) — must not crash in fs2_fill_table */
Test(fs2_api_modificators, removeall_new_canon_chars_clones) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello");
    fs2 c   = FS2();

    fs2 r = fs2_removeall_new(&src, &c);

    CHECK_STR_EQ("hello", r.v, "hello");   /* no-op, clone of src */
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    fs2_free(&c);
    fs2_free(&r);
    assert_balanced();
}

/* =========================================================================
 * Empty src -> FS2EMPTY (all flavors normalize)
 * ========================================================================= */

Test(fs2_api_modificators, removeall_new_empty_src_all_flavors) {
    __fs2_checkreset();
    fs2 c = fs2_fromstr("x");

    fs2 canon = FS2();
    fs2 empty = FS2EMPTY;
    fs2 lit   = FS2LITERAL("");

    fs2 rc = fs2_removeall_new(&canon, &c);
    fs2 re = fs2_removeall_new(&empty, &c);
    fs2 rl = fs2_removeall_new(&lit,   &c);

    CHECK_TRUE("canon src",   fs2_eq_empty(&rc));
    CHECK_TRUE("empty src",   fs2_eq_empty(&re));
    CHECK_TRUE("literal src", fs2_eq_empty(&rl));
    CHECK_FS2VALID(1, &rc);
    CHECK_FS2VALID(2, &re);
    CHECK_FS2VALID(3, &rl);

    fs2_free(&canon);
    fs2_free(&c);
    fs2_free(&rc);
    fs2_free(&re);
    fs2_free(&rl);
    assert_balanced();
}

/* =========================================================================
 * STATIC / LOCAL src — _new constructor works, src untouched
 * ========================================================================= */

Test(fs2_api_modificators, removeall_new_static_src) {
    __fs2_checkreset();
    fs2 src = FS2LITERAL("hello world");
    fs2 c   = fs2_fromstr("aeiou");

    fs2 r = fs2_removeall_new(&src, &c);

    CHECK_STR_EQ("hll wrld",        r.v, "hll wrld");
    CHECK_TRUE ("ALLOC",            fs2_is_alloc(&r));
    CHECK_TRUE ("independent",      r.v != src.v);
    CHECK_TRUE ("src still STATIC", fs2_is_static(&src));
    CHECK_FS2VALID(1, &r);
    CHECK_FS2VALID(2, &src);

    fs2_free(&c);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api_modificators, removeall_new_local_src) {
    __fs2_checkreset();
    FS2LOCAL(buf, 16);
    fs2_memcpy_str(&buf, 0, "hello world", 11);
    fs2_setlen(&buf, 11);
    fs2 c = fs2_fromstr("aeiou");

    fs2 r = fs2_removeall_new(&buf, &c);

    CHECK_STR_EQ("hll wrld",        r.v,   "hll wrld");
    CHECK_TRUE ("result ALLOC",     fs2_is_alloc(&r));
    CHECK_TRUE ("result not LOCAL", !fs2_is_local(&r));
    CHECK_STR_EQ("buf untouched",   buf.v, "hello world");
    CHECK_FS2VALID(1, &r);
    CHECK_FS2VALID(2, &buf);

    fs2_free(&c);
    fs2_free(&r);
    assert_balanced();
}

/* =========================================================================
 * src untouched + independence
 * ========================================================================= */

Test(fs2_api_modificators, removeall_new_src_untouched_and_independent) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello world");
    fs2 c   = fs2_fromstr("aeiou");
    void *orig = src.v;

    fs2 r = fs2_removeall_new(&src, &c);

    CHECK_PTR_EQ("src v unchanged", src.v, orig);
    CHECK_STR_EQ("src content",     src.v, "hello world");
    CHECK_STR_EQ("result",          r.v,   "hll wrld");
    CHECK_TRUE ("independent",      r.v != src.v);

    r.v[0] = 'X';
    CHECK_STR_EQ("result mutated", r.v,   "Xll wrld");
    CHECK_STR_EQ("src original",   src.v, "hello world");

    CHECK_FS2VALID(1, &r);
    CHECK_FS2VALID(2, &src);

    fs2_free(&src);
    fs2_free(&c);
    fs2_free(&r);
    assert_balanced();
}

/* =========================================================================
 * Guards
 * ========================================================================= */

Test(fs2_api_modificators, removeall_new_null_s_aborts, .signal = SIGABRT) {
    fs2 c = fs2_fromstr("x");
    fs2_removeall_new(NULL, &c);
    fs2_free(&c);
}

Test(fs2_api_modificators, removeall_new_null_chars_aborts, .signal = SIGABRT) {
    fs2 src = fs2_fromstr("abc");
    fs2_removeall_new(&src, NULL);
    fs2_free(&src);
}

/* =========================================================================
 * Consistency: string content matches clone + in-place
 *   (flavors differ: in-place keeps ALLOC "", _new gives FS2EMPTY on empty)
 * ========================================================================= */

Test(fs2_api_modificators, removeall_new_matches_inplace_content) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello world");
    fs2 c   = fs2_fromstr("aeiou");

    fs2 manual = fs2_clone(&src);
    fs2_removeall(&manual, &c);

    fs2 via = fs2_removeall_new(&src, &c);

    CHECK_STR_EQ("same content", manual.v,   via.v);
    CHECK_EQ  ("same len",       manual.len, via.len);
    CHECK_EQ  ("strcmp == 0",    fs2_cmp(&manual, &via), 0);
    CHECK_FS2VALID(1, &manual);
    CHECK_FS2VALID(2, &via);

    fs2_free(&src);
    fs2_free(&c);
    fs2_free(&manual);
    fs2_free(&via);
    assert_balanced();
}

/* All-removed case: content same "", flavors differ (ALLOC vs STATIC) */
Test(fs2_api_modificators, removeall_new_matches_inplace_all_removed) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("abc");
    fs2 c   = fs2_fromstr("abc");

    fs2 manual = fs2_clone(&src);
    fs2_removeall(&manual, &c);       /* ALLOC "" */

    fs2 via = fs2_removeall_new(&src, &c);   /* FS2EMPTY */

    CHECK_EQ("strcmp == 0",     fs2_cmp(&manual, &via), 0);
    CHECK_TRUE("manual is empty", fs2_is_empty(&manual));
    CHECK_TRUE("via is empty",    fs2_is_empty(&via));

    fs2_free(&src);
    fs2_free(&c);
    fs2_free(&manual);
    fs2_free(&via);
    assert_balanced();
}

/* =========================================================================
 * fs2_removeall_new_str — minimal: only the _fs2_wrap(chars) behaviour differs
 *   (core fs2-version aborts on NULL chars; _str is lenient -> no-op clone)
 * ========================================================================= */

Test(fs2_api_modificators, removeall_new_str_basic) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello world");

    fs2 r = fs2_removeall_new_str(&src, "aeiou");

    CHECK_STR_EQ("hll wrld", r.v, "hll wrld");
    CHECK_EQ  ("len",        r.len, 8);
    CHECK_TRUE("ALLOC",      fs2_is_alloc(&r));
    CHECK_FS2VALID(1, &r);
    CHECK_FS2VALID(2, &src);

    fs2_free(&src);
    fs2_free(&r);
    assert_balanced();
}

/* NULL chars -> _fs2_wrap(NULL) -> FS2EMPTY -> clone of src (no-op) */
Test(fs2_api_modificators, removeall_new_str_null_chars_clones) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello");

    fs2 r = fs2_removeall_new_str(&src, NULL);

    CHECK_STR_EQ("hello",     r.v, "hello");
    CHECK_TRUE ("independent", r.v != src.v);
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    fs2_free(&r);
    assert_balanced();
}

/* "" chars behaves identically to NULL */
Test(fs2_api_modificators, removeall_new_str_empty_chars_clones) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello");

    fs2 r = fs2_removeall_new_str(&src, "");

    CHECK_STR_EQ("hello", r.v, "hello");
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    fs2_free(&r);
    assert_balanced();
}

/* =========================================================================
 * Contrast: core fs2-version aborts on NULL chars
 * ========================================================================= */

Test(fs2_api_modificators, removeall_new_str_null_chars_aborts, .signal = SIGABRT) {
    fs2 src = fs2_fromstr("abc");
    fs2_removeall_new(&src, NULL);
    fs2_free(&src);
}

/* =========================================================================
 * Equivalence: _str ≡ fs2-version with explicit _fs2_wrap(chars)
 * ========================================================================= */

Test(fs2_api_modificators, removeall_new_str_matches_fs2_version) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello world");
    fs2 c   = fs2_fromstr("aeiou");

    fs2 via_str = fs2_removeall_new_str(&src, "aeiou");
    fs2 via_fs2 = fs2_removeall_new(&src, &c);

    CHECK_STR_EQ("same content", via_str.v,   via_fs2.v);
    CHECK_EQ  ("same len",       via_str.len, via_fs2.len);
    CHECK_FS2VALID(1, &via_str);
    CHECK_FS2VALID(2, &via_fs2);

    fs2_free(&src);
    fs2_free(&c);
    fs2_free(&via_str);
    fs2_free(&via_fs2);
    assert_balanced();
}

/* NULL s still aborts (inherited from fs2_removeall_new) */
Test(fs2_api_modificators, removeall_new_str_null_s_aborts, .signal = SIGABRT) {
    fs2_removeall_new_str(NULL, "a");
}

/* =========================================================================
 * fs2_concatall — empty dst (Bug #1 + #2)
 *   This is THE primary use case: build a string from scratch.
 * ========================================================================= */

Test(fs2_api_modificators, concatall_empty_dst_builds_string) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("a");
    fs2 b = fs2_fromstr("b");
    fs2 c = fs2_fromstr("c");
    fs2 delim = fs2_fromstr("-");
    const fs2 *arr[] = { &a, &b, &c, NULL };

    fs2 dst = FS2();
    fs2 *r = fs2_concatall(&dst, &delim, arr);

    CHECK_STR_EQ("a-b-c", dst.v, "a-b-c");   /* bug #1: would be "" */
    CHECK_EQ  ("len",     dst.len, 5);       /* bug #2: setlen(total) would corrupt */
    CHECK_PTR_EQ("chain", r, &dst);
    CHECK_FS2VALID(1, &dst);

    fs2_free(&a); fs2_free(&b); fs2_free(&c);
    fs2_free(&delim);
    fs2_free(&dst);
    assert_balanced();
}

Test(fs2_api_modificators, concatall_fs2empty_dst_builds_string) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("X");
    fs2 delim = fs2_fromstr(",");
    const fs2 *arr[] = { &a, NULL };

    fs2 dst = FS2EMPTY;
    fs2_concatall(&dst, &delim, arr);

    CHECK_STR_EQ("X", dst.v, "X");
    CHECK_TRUE ("promoted to ALLOC", fs2_is_alloc(&dst));
    CHECK_FS2VALID(1, &dst);

    fs2_free(&a);
    fs2_free(&delim);
    fs2_free(&dst);
    assert_balanced();
}

/* =========================================================================
 * Between-element delimiter (Bug #3)
 * ========================================================================= */

Test(fs2_api_modificators, concatall_between_delimiter) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("x");
    fs2 b = fs2_fromstr("y");
    fs2 c = fs2_fromstr("z");
    fs2 d = fs2_fromstr(", ");
    const fs2 *arr[] = { &a, &b, &c, NULL };

    fs2 dst = FS2();
    fs2_concatall(&dst, &d, arr);

    CHECK_STR_EQ("x, y, z", dst.v, "x, y, z");   /* NOT ", x, y, z" */
    CHECK_FS2VALID(1, &dst);

    fs2_free(&a); fs2_free(&b); fs2_free(&c);
    fs2_free(&d);
    fs2_free(&dst);
    assert_balanced();
}

/* Single element: no delimiter at all */
Test(fs2_api_modificators, concatall_single_element_no_delimiter) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("solo");
    fs2 d = fs2_fromstr("-");
    const fs2 *arr[] = { &a, NULL };

    fs2 dst = FS2();
    fs2_concatall(&dst, &d, arr);

    CHECK_STR_EQ("solo", dst.v, "solo");   /* no leading "-" */

    fs2_free(&a);
    fs2_free(&d);
    fs2_free(&dst);
    assert_balanced();
}

/* =========================================================================
 * NULL delim — no separator
 * ========================================================================= */

Test(fs2_api_modificators, concatall_null_delim_no_separator) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("foo");
    fs2 b = fs2_fromstr("bar");
    const fs2 *arr[] = { &a, &b, NULL };

    fs2 dst = FS2();
    fs2_concatall(&dst, NULL, arr);

    CHECK_STR_EQ("foobar", dst.v, "foobar");
    CHECK_FS2VALID(1, &dst);

    fs2_free(&a);
    fs2_free(&b);
    fs2_free(&dst);
    assert_balanced();
}

/* Empty delim FS2EMPTY — same as NULL, no separator */
Test(fs2_api_modificators, concatall_empty_delim_no_separator) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("foo");
    fs2 b = fs2_fromstr("bar");
    fs2 d = FS2EMPTY;
    const fs2 *arr[] = { &a, &b, NULL };

    fs2 dst = FS2();
    fs2_concatall(&dst, &d, arr);

    CHECK_STR_EQ("foobar", dst.v, "foobar");

    fs2_free(&a);
    fs2_free(&b);
    fs2_free(&dst);
    assert_balanced();
}

/* =========================================================================
 * Pre-existing dst content preserved (Bug #4)
 * ========================================================================= */

Test(fs2_api_modificators, concatall_appends_to_existing) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("prefix:");
    fs2 a = fs2_fromstr("a");
    fs2 b = fs2_fromstr("b");
    fs2 d = fs2_fromstr("-");
    const fs2 *arr[] = { &a, &b, NULL };

    fs2_concatall(&dst, &d, arr);

    CHECK_STR_EQ("prefix:-a-b", dst.v, "prefix:-a-b");
    CHECK_EQ  ("len",          dst.len, strlen("prefix:-a-b"));
    CHECK_FS2VALID(1, &dst);

    fs2_free(&a);
    fs2_free(&b);
    fs2_free(&d);
    fs2_free(&dst);
    assert_balanced();
}

/* =========================================================================
 * Empty arr / NULL arr / NULL dst guards
 * ========================================================================= */

Test(fs2_api_modificators, concatall_empty_arr_noop) {
    __fs2_checkreset();
    const fs2 *arr[] = { NULL };
    fs2 d = fs2_fromstr("-");

    fs2 dst = fs2_fromstr("hello");
    void *orig = dst.v;

    fs2_concatall(&dst, &d, arr);

    CHECK_PTR_EQ("buffer unchanged", dst.v, orig);
    CHECK_STR_EQ("hello",            dst.v, "hello");
    CHECK_FS2VALID(1, &dst);

    fs2_free(&d);
    fs2_free(&dst);
    assert_balanced();
}

Test(fs2_api_modificators, concatall_null_src_noop) {
    __fs2_checkreset();
    fs2 d = fs2_fromstr("-");
    fs2 dst = fs2_fromstr("hello");

    fs2_concatall(&dst, &d, NULL);

    CHECK_STR_EQ("hello", dst.v, "hello");

    fs2_free(&d);
    fs2_free(&dst);
    assert_balanced();
}

Test(fs2_api_modificators, concatall_null_dst_aborts, .signal = SIGABRT) {
    fs2 a = fs2_fromstr("a");
    fs2 d = fs2_fromstr("-");
    const fs2 *arr[] = { &a, NULL };
    fs2_concatall(NULL, &d, arr);
    fs2_free(&a);
    fs2_free(&d);
}

/* =========================================================================
 * STATIC non-empty dst → abort (inherited from fs2_cant_realloc)
 * ========================================================================= */

Test(fs2_api_modificators, concatall_static_nonempty_aborts, .signal = SIGABRT) {
    fs2 dst = FS2LITERAL("hello");
    fs2 a = fs2_fromstr("a");
    fs2 d = fs2_fromstr("-");
    const fs2 *arr[] = { &a, NULL };
    fs2_concatall(&dst, &d, arr);
    fs2_free(&a);
    fs2_free(&d);
}

/* =========================================================================
 * LOCAL dst — works if capacity fits
 * ========================================================================= */

Test(fs2_api_modificators, concatall_local_dst) {
    __fs2_checkreset();
    FS2LOCAL(buf, 32);
    fs2 a = fs2_fromstr("ab");
    fs2 b = fs2_fromstr("cd");
    fs2 d = fs2_fromstr("-");
    const fs2 *arr[] = { &a, &b, NULL };

    fs2_concatall(&buf, &d, arr);

    CHECK_STR_EQ("ab-cd",     buf.v, "ab-cd");
    CHECK_TRUE ("still LOCAL", fs2_is_local(&buf));
    CHECK_FS2VALID(1, &buf);

    fs2_free(&a);
    fs2_free(&b);
    fs2_free(&d);
    assert_balanced();
}

/* =========================================================================
 * FS2CONCAT macro — NULL terminator (Bug #5)
 * ========================================================================= */

Test(fs2_api_modificators, fconat_macro_two_elements) {
    __fs2_checkreset();
    fs2 dst = FS2();
    fs2 a = fs2_fromstr("x");
    fs2 b = fs2_fromstr("y");
    fs2 d = fs2_fromstr("-");

    FS2CONCAT(&dst, &d, &a, &b);

    CHECK_STR_EQ("x-y", dst.v, "x-y");   /* bug #5: without NULL terminator, OOB */
    CHECK_FS2VALID(1, &dst);

    fs2_free(&a);
    fs2_free(&b);
    fs2_free(&d);
    fs2_free(&dst);
    assert_balanced();
}

Test(fs2_api_modificators, fconat_macro_one_element) {
    __fs2_checkreset();
    fs2 dst = FS2();
    fs2 a = fs2_fromstr("solo");
    fs2 d = fs2_fromstr("-");

    FS2CONCAT(&dst, &d, &a);

    CHECK_STR_EQ("solo", dst.v, "solo");
    CHECK_FS2VALID(1, &dst);

    fs2_free(&a);
    fs2_free(&d);
    fs2_free(&dst);
    assert_balanced();
}

/* =========================================================================
 * src elements untouched
 * ========================================================================= */

Test(fs2_api_modificators, concatall_elements_untouched) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("alpha");
    fs2 b = fs2_fromstr("beta");
    fs2 d = fs2_fromstr("-");
    void *a_v = a.v, *b_v = b.v, *d_v = d.v;
    const fs2 *arr[] = { &a, &b, NULL };

    fs2 dst = FS2();
    fs2_concatall(&dst, &d, arr);

    CHECK_PTR_EQ("a.v", a.v, a_v);
    CHECK_PTR_EQ("b.v", b.v, b_v);
    CHECK_PTR_EQ("d.v", d.v, d_v);
    CHECK_STR_EQ("a", a.v, "alpha");
    CHECK_STR_EQ("b", b.v, "beta");
    CHECK_FS2VALID(1, &a);
    CHECK_FS2VALID(2, &b);
    CHECK_FS2VALID(3, &d);

    fs2_free(&a);
    fs2_free(&b);
    fs2_free(&d);
    fs2_free(&dst);
    assert_balanced();
}

/* Pre-reserve must cover the final size: no extra realloc inside cat.
 * (We can't observe realloc directly, but we can verify sz >= len+1
 *  and that the buffer pointer doesn't change from the reserve forward.) */
Test(fs2_api_modificators, concatall_pre_reserve_sufficient) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("hello");
    fs2 b = fs2_fromstr("world");
    fs2 d = fs2_fromstr("-");
    const fs2 *arr[] = { &a, &b, NULL };

    fs2 dst = FS2();
    fs2_concatall(&dst, &d, arr);

    /* Final "hello-world": len=11, sz must be >= 12 */
    CHECK_EQ  ("len",    dst.len, 11);
    CHECK_TRUE("sz fits", dst.sz >= dst.len + 1);
    CHECK_STR_EQ("hello-world", dst.v, "hello-world");
    CHECK_FS2VALID(1, &dst);

    fs2_free(&a);
    fs2_free(&b);
    fs2_free(&d);
    fs2_free(&dst);
    assert_balanced();
}

/* =========================================================================
 * fs2_concatall_str — nominal
 * ========================================================================= */

Test(fs2_api_modificators, concatall_str_basic) {
    __fs2_checkreset();
    const char *arr[] = { "a", "b", "c", NULL };

    fs2 dst = FS2();
    fs2 *r = fs2_concatall_str(&dst, "-", arr);

    CHECK_STR_EQ("a-b-c", dst.v, "a-b-c");
    CHECK_EQ  ("len",     dst.len, 5);
    CHECK_PTR_EQ("chain", r, &dst);
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2_api_modificators, concatall_str_empty_dst_builds_string) {
    __fs2_checkreset();
    const char *arr[] = { "hello", "world", NULL };

    fs2 dst = FS2EMPTY;
    fs2_concatall_str(&dst, ", ", arr);

    CHECK_STR_EQ("hello, world",   dst.v, "hello, world");
    CHECK_TRUE ("promoted to ALLOC", fs2_is_alloc(&dst));
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2_api_modificators, concatall_str_single_element_no_delimiter) {
    __fs2_checkreset();
    const char *arr[] = { "solo", NULL };

    fs2 dst = FS2();
    fs2_concatall_str(&dst, "-", arr);

    CHECK_STR_EQ("solo", dst.v, "solo");   /* no leading "-" */

    fs2_free(&dst);
    assert_balanced();
}

/* =========================================================================
 * NULL / "" delim — no separator
 * ========================================================================= */

Test(fs2_api_modificators, concatall_str_null_delim) {
    __fs2_checkreset();
    const char *arr[] = { "foo", "bar", "baz", NULL };

    fs2 dst = FS2();
    fs2_concatall_str(&dst, NULL, arr);

    CHECK_STR_EQ("foobarbaz", dst.v, "foobarbaz");
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2_api_modificators, concatall_str_empty_delim) {
    __fs2_checkreset();
    const char *arr[] = { "foo", "bar", NULL };

    fs2 dst = FS2();
    fs2_concatall_str(&dst, "", arr);

    CHECK_STR_EQ("foobar", dst.v, "foobar");

    fs2_free(&dst);
    assert_balanced();
}

/* =========================================================================
 * Empty elem "" — no-op for that element, delim still between non-empty
 * ========================================================================= */

Test(fs2_api_modificators, concatall_str_empty_elements) {
    __fs2_checkreset();
    const char *arr[] = { "a", "", "b", NULL };

    fs2 dst = FS2();
    fs2_concatall_str(&dst, "-", arr);

    /* "a" + "-" + "" + "-" + "b" = "a--b" (empty elem still gets delims) */
    CHECK_STR_EQ("a--b", dst.v, "a--b");
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

/* =========================================================================
 * Pre-existing dst content preserved
 * ========================================================================= */

Test(fs2_api_modificators, concatall_str_appends_to_existing) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("prefix:");
    const char *arr[] = { "a", "b", NULL };

    fs2_concatall_str(&dst, "-", arr);

    CHECK_STR_EQ("prefix:-a-b", dst.v, "prefix:-a-b");
    CHECK_EQ  ("len",          dst.len, strlen("prefix:-a-b"));
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

/* =========================================================================
 * Guard paths: NULL src / empty arr / NULL dst / STATIC dst
 * ========================================================================= */

Test(fs2_api_modificators, concatall_str_null_src_noop) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("hello");
    void *orig = dst.v;

    fs2_concatall_str(&dst, "-", NULL);

    CHECK_PTR_EQ("buffer unchanged", dst.v, orig);
    CHECK_STR_EQ("hello",            dst.v, "hello");
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2_api_modificators, concatall_str_empty_arr_noop) {
    __fs2_checkreset();
    const char *arr[] = { NULL };

    fs2 dst = fs2_fromstr("hello");
    void *orig = dst.v;

    fs2_concatall_str(&dst, "-", arr);

    CHECK_PTR_EQ("buffer unchanged", dst.v, orig);
    CHECK_STR_EQ("hello",            dst.v, "hello");

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2_api_modificators, concatall_str_null_dst_aborts, .signal = SIGABRT) {
    const char *arr[] = { "a", NULL };
    fs2_concatall_str(NULL, "-", arr);
}

Test(fs2_api_modificators, concatall_str_static_nonempty_aborts, .signal = SIGABRT) {
    fs2 dst = FS2LITERAL("hello");
    const char *arr[] = { "a", NULL };
    fs2_concatall_str(&dst, "-", arr);
}

/* =========================================================================
 * LOCAL dst — works if capacity fits
 * ========================================================================= */

Test(fs2_api_modificators, concatall_str_local_dst) {
    __fs2_checkreset();
    FS2LOCAL(buf, 32);
    const char *arr[] = { "ab", "cd", NULL };

    fs2_concatall_str(&buf, "-", arr);

    CHECK_STR_EQ("ab-cd",      buf.v, "ab-cd");
    CHECK_TRUE ("still LOCAL", fs2_is_local(&buf));
    CHECK_FS2VALID(1, &buf);

    assert_balanced();
}

/* =========================================================================
 * FS2CONCATSTR macro — NULL terminator
 * ========================================================================= */

Test(fs2_api_modificators, fconcatstr_macro_two_elements) {
    __fs2_checkreset();
    fs2 dst = FS2();

    FS2CONCATSTR(&dst, "-", "x", "y");

    CHECK_STR_EQ("x-y", dst.v, "x-y");
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2_api_modificators, fconcatstr_macro_one_element) {
    __fs2_checkreset();
    fs2 dst = FS2();

    FS2CONCATSTR(&dst, "-", "solo");

    CHECK_STR_EQ("solo", dst.v, "solo");

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2_api_modificators, fconcatstr_macro_three_elements) {
    __fs2_checkreset();
    fs2 dst = FS2();

    FS2CONCATSTR(&dst, ", ", "red", "green", "blue");

    CHECK_STR_EQ("red, green, blue", dst.v, "red, green, blue");
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

/* =========================================================================
 * Consistency: concatall_str(dst, d, arr) ≡ concatall(dst, d_fs2, wrapped)
 * ========================================================================= */

Test(fs2_api_modificators, concatall_str_matches_concatall_wrapped) {
    __fs2_checkreset();
    const char *arr[] = { "a", "b", "c", NULL };

    /* via _str */
    fs2 dst_str = FS2();
    fs2_concatall_str(&dst_str, "-", arr);

    /* via concatall with wrapped views */
    fs2 a = fs2_fromstr("a");
    fs2 b = fs2_fromstr("b");
    fs2 c = fs2_fromstr("c");
    fs2 d = fs2_fromstr("-");
    const fs2 *arr_fs2[] = { &a, &b, &c, NULL };

    fs2 dst_fs2 = FS2();
    fs2_concatall(&dst_fs2, &d, arr_fs2);

    CHECK_STR_EQ("same content", dst_str.v,   dst_fs2.v);
    CHECK_EQ  ("same len",       dst_str.len, dst_fs2.len);
    CHECK_FS2VALID(1, &dst_str);
    CHECK_FS2VALID(2, &dst_fs2);

    fs2_free(&a); fs2_free(&b); fs2_free(&c); fs2_free(&d);
    fs2_free(&dst_str);
    fs2_free(&dst_fs2);
    assert_balanced();
}

/* =========================================================================
 * Elements untouched (c-strings are const, but verify they survive)
 * ========================================================================= */

Test(fs2_api_modificators, concatall_str_elements_untouched) {
    __fs2_checkreset();
    const char *e1 = "alpha";
    const char *e2 = "beta";
    const char *d  = "-";
    const char *arr[] = { e1, e2, NULL };

    fs2 dst = FS2();
    fs2_concatall_str(&dst, d, arr);

    /* c-strings themselves are immutable by contract; we verify pointers
     * in arr[] are still the same (the function does not modify the array) */
    CHECK_PTR_EQ("arr[0]", arr[0], e1);
    CHECK_PTR_EQ("arr[1]", arr[1], e2);
    CHECK_STR_EQ("e1", e1, "alpha");
    CHECK_STR_EQ("e2", e2, "beta");
    CHECK_STR_EQ("d",  d,  "-");

    fs2_free(&dst);
    assert_balanced();
}

/* =========================================================================
 * fs2_concatall_new — nominal
 * ========================================================================= */

Test(fs2_api_modificators, concatall_new_basic) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("a");
    fs2 b = fs2_fromstr("b");
    fs2 d = fs2_fromstr("-");
    fs2 dst = fs2_fromstr("prefix:");
    const fs2 *arr[] = { &a, &b, NULL };

    fs2 r = fs2_concatall_new(&dst, &d, arr);

    CHECK_STR_EQ("prefix:-a-b", r.v, "prefix:-a-b");
    CHECK_EQ  ("len",          r.len, strlen("prefix:-a-b"));
    CHECK_TRUE("ALLOC",           fs2_is_alloc(&r));
    CHECK_FALSE("not BODYALLOC",  fs2_is_bodyalloc(&r));
    CHECK_TRUE("independent",     r.v != dst.v);
    CHECK_FS2VALID(1, &r);
    CHECK_FS2VALID(2, &dst);

    fs2_free(&a);
    fs2_free(&b);
    fs2_free(&d);
    fs2_free(&dst);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api_modificators, concatall_new_empty_dst_canon) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("hello");
    fs2 b = fs2_fromstr("world");
    fs2 d = fs2_fromstr(", ");
    fs2 dst = FS2();
    const fs2 *arr[] = { &a, &b, NULL };

    fs2 r = fs2_concatall_new(&dst, &d, arr);

    CHECK_STR_EQ("hello, world", r.v, "hello, world");
    CHECK_EQ  ("len",            r.len, 12);
    CHECK_TRUE("ALLOC",          fs2_is_alloc(&r));
    CHECK_FS2VALID(1, &r);

    fs2_free(&a);
    fs2_free(&b);
    fs2_free(&d);
    fs2_free(&dst);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api_modificators, concatall_new_empty_dst_fs2empty) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("x");
    fs2 d = fs2_fromstr("-");
    fs2 dst = FS2EMPTY;
    const fs2 *arr[] = { &a, NULL };

    fs2 r = fs2_concatall_new(&dst, &d, arr);

    CHECK_STR_EQ("x", r.v, "x");
    CHECK_FS2VALID(1, &r);

    fs2_free(&a);
    fs2_free(&d);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api_modificators, concatall_new_single_element_no_delimiter) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("solo");
    fs2 d = fs2_fromstr("-");
    fs2 dst = FS2();
    const fs2 *arr[] = { &a, NULL };

    fs2 r = fs2_concatall_new(&dst, &d, arr);

    CHECK_STR_EQ("solo", r.v, "solo");   /* no leading "-" */
    fs2_free(&a);
    fs2_free(&d);
    fs2_free(&r);
    assert_balanced();
}

/* =========================================================================
 * NULL delim / delim untouched
 * ========================================================================= */

Test(fs2_api_modificators, concatall_new_null_delim) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("foo");
    fs2 b = fs2_fromstr("bar");
    fs2 dst = FS2();
    const fs2 *arr[] = { &a, &b, NULL };

    fs2 r = fs2_concatall_new(&dst, NULL, arr);

    CHECK_STR_EQ("foobar", r.v, "foobar");

    fs2_free(&a);
    fs2_free(&b);
    fs2_free(&r);
    assert_balanced();
}

/* =========================================================================
 * src == NULL / empty arr — returns clone of dst unchanged
 * ========================================================================= */

Test(fs2_api_modificators, concatall_new_null_src_returns_clone) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("hello");

    fs2 r = fs2_concatall_new(&dst, NULL, NULL);

    CHECK_STR_EQ("hello",     r.v, "hello");
    CHECK_TRUE ("independent", r.v != dst.v);
    CHECK_FS2VALID(1, &r);
    CHECK_FS2VALID(2, &dst);

    fs2_free(&dst);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api_modificators, concatall_new_empty_arr_returns_clone) {
    __fs2_checkreset();
    const fs2 *arr[] = { NULL };
    fs2 dst = fs2_fromstr("hello");
    fs2 d   = fs2_fromstr("-");

    fs2 r = fs2_concatall_new(&dst, &d, arr);

    CHECK_STR_EQ("hello", r.v, "hello");
    CHECK_FS2VALID(1, &r);

    fs2_free(&d);
    fs2_free(&dst);
    fs2_free(&r);
    assert_balanced();
}

/* =========================================================================
 * KEY: STATIC non-empty dst works (would abort with in-place concatall)
 * ========================================================================= */

Test(fs2_api_modificators, concatall_new_static_dst_works) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("a");
    fs2 b = fs2_fromstr("b");
    fs2 d = fs2_fromstr("-");
    fs2 dst = FS2LITERAL("prefix:");
    const fs2 *arr[] = { &a, &b, NULL };

    fs2 r = fs2_concatall_new(&dst, &d, arr);

    CHECK_STR_EQ("prefix:-a-b",     r.v, "prefix:-a-b");
    CHECK_TRUE ("result ALLOC",    fs2_is_alloc(&r));
    CHECK_TRUE ("result not STATIC", !fs2_is_static(&r));
    CHECK_TRUE ("dst still STATIC", fs2_is_static(&dst));
    CHECK_STR_EQ("dst unchanged",   dst.v, "prefix:");
    CHECK_FS2VALID(1, &r);
    CHECK_FS2VALID(2, &dst);

    fs2_free(&a);
    fs2_free(&b);
    fs2_free(&d);
    fs2_free(&r);
    assert_balanced();
}

/* Contrast: in-place on STATIC non-empty aborts */
Test(fs2_api_modificators, concatall_static_dst_aborts, .signal = SIGABRT) {
    fs2 a = fs2_fromstr("a");
    fs2 d = fs2_fromstr("-");
    fs2 dst = FS2LITERAL("hello");
    const fs2 *arr[] = { &a, NULL };
    fs2_concatall(&dst, &d, arr);
    fs2_free(&a);
    fs2_free(&d);
}

/* =========================================================================
 * KEY: LOCAL dst with insufficient capacity works (would abort in-place)
 * ========================================================================= */

Test(fs2_api_modificators, concatall_new_local_dst_grows_freely) {
    __fs2_checkreset();
    FS2LOCAL(buf, 4);      /* capacity 4 — too small for "abc-def" */
    fs2_memcpy_str(&buf, 0, "x", 1);
    fs2_setlen(&buf, 1);

    fs2 a = fs2_fromstr("abc");
    fs2 b = fs2_fromstr("def");
    fs2 d = fs2_fromstr("-");
    const fs2 *arr[] = { &a, &b, NULL };

    fs2 r = fs2_concatall_new(&buf, &d, arr);

    CHECK_STR_EQ("x-abc-def",        r.v, "x-abc-def");
    CHECK_TRUE ("result ALLOC",     fs2_is_alloc(&r));
    CHECK_TRUE ("result not LOCAL", !fs2_is_local(&r));
    CHECK_STR_EQ("buf untouched",   buf.v, "x");
    CHECK_FS2VALID(1, &r);
    CHECK_FS2VALID(2, &buf);

    fs2_free(&a);
    fs2_free(&b);
    fs2_free(&d);
    fs2_free(&r);
    assert_balanced();
}

/* =========================================================================
 * dst untouched / independent
 * ========================================================================= */

Test(fs2_api_modificators, concatall_new_dst_untouched_and_independent) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("orig");
    fs2 a = fs2_fromstr("A");
    fs2 d = fs2_fromstr("|");
    void *orig_v = dst.v;
    const fs2 *arr[] = { &a, NULL };

    fs2 r = fs2_concatall_new(&dst, &d, arr);

    CHECK_PTR_EQ("dst v unchanged", dst.v, orig_v);
    CHECK_STR_EQ("dst content",     dst.v, "orig");
    CHECK_STR_EQ("result",          r.v,   "orig|A");
    CHECK_TRUE ("independent",      r.v != dst.v);

    r.v[0] = 'X';
    CHECK_STR_EQ("result mutated", r.v,   "Xrig|A");
    CHECK_STR_EQ("dst original",   dst.v, "orig");

    CHECK_FS2VALID(1, &r);
    CHECK_FS2VALID(2, &dst);

    fs2_free(&a);
    fs2_free(&d);
    fs2_free(&dst);
    fs2_free(&r);
    assert_balanced();
}

/* =========================================================================
 * NULL dst → SIGABRT (via fs2_clone)
 * ========================================================================= */

Test(fs2_api_modificators, concatall_new_null_dst_aborts, .signal = SIGABRT) {
    fs2 a = fs2_fromstr("a");
    fs2 d = fs2_fromstr("-");
    const fs2 *arr[] = { &a, NULL };
    fs2_concatall_new(NULL, &d, arr);
    fs2_free(&a);
    fs2_free(&d);
}

/* =========================================================================
 * FS2CONCAT_NEW macro
 * ========================================================================= */

Test(fs2_api_modificators, fconcat_new_macro_two_elements) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("p:");
    fs2 a = fs2_fromstr("x");
    fs2 b = fs2_fromstr("y");
    fs2 d = fs2_fromstr("-");

    fs2 r = FS2CONCAT_NEW(&dst, &d, &a, &b);

    CHECK_STR_EQ("p:-x-y", r.v, "p:-x-y");
    CHECK_FS2VALID(1, &r);

    fs2_free(&dst);
    fs2_free(&a);
    fs2_free(&b);
    fs2_free(&d);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api_modificators, fconcat_new_macro_three_elements) {
    __fs2_checkreset();
    fs2 dst = FS2();
    fs2 a = fs2_fromstr("red");
    fs2 b = fs2_fromstr("green");
    fs2 c = fs2_fromstr("blue");
    fs2 d = fs2_fromstr(", ");

    fs2 r = FS2CONCAT_NEW(&dst, &d, &a, &b, &c);

    CHECK_STR_EQ("red, green, blue", r.v, "red, green, blue");

    fs2_free(&dst);
    fs2_free(&a); fs2_free(&b); fs2_free(&c);
    fs2_free(&d);
    fs2_free(&r);
    assert_balanced();
}

/* =========================================================================
 * Consistency: _new ≡ clone + in-place
 * ========================================================================= */

Test(fs2_api_modificators, concatall_new_matches_clone_then_inplace) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("prefix:");
    fs2 a = fs2_fromstr("a");
    fs2 b = fs2_fromstr("b");
    fs2 d = fs2_fromstr("-");
    const fs2 *arr[] = { &a, &b, NULL };

    fs2 manual = fs2_clone(&dst);
    fs2_concatall(&manual, &d, arr);

    fs2 via = fs2_concatall_new(&dst, &d, arr);

    CHECK_STR_EQ("same content", manual.v,   via.v);
    CHECK_EQ  ("same len",       manual.len, via.len);
    CHECK_FS2VALID(1, &manual);
    CHECK_FS2VALID(2, &via);

    fs2_free(&dst);
    fs2_free(&a);
    fs2_free(&b);
    fs2_free(&d);
    fs2_free(&manual);
    fs2_free(&via);
    assert_balanced();
}

/* =========================================================================
 * fs2_concatall_str_new — minimal: clone + delegate
 *   (core concatall_str semantics already covered by its own tests)
 * ========================================================================= */

Test(fs2_api_modificators, concatall_str_new_basic) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("p:");
    const char *arr[] = { "a", "b", NULL };

    fs2 r = fs2_concatall_str_new(&dst, "-", arr);

    CHECK_STR_EQ("p:-a-b", r.v, "p:-a-b");   /* new semantics: delim before first when dst non-empty */
    CHECK_TRUE ("ALLOC",           fs2_is_alloc(&r));
    CHECK_FALSE("not BODYALLOC",   fs2_is_bodyalloc(&r));
    CHECK_FS2VALID(1, &r);
    CHECK_FS2VALID(2, &dst);

    fs2_free(&dst);
    fs2_free(&r);
    assert_balanced();
}

/* dst untouched (clone makes an independent copy) */
Test(fs2_api_modificators, concatall_str_new_dst_untouched) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("orig");
    const char *arr[] = { "a", NULL };
    void *orig_v = dst.v;

    fs2 r = fs2_concatall_str_new(&dst, "|", arr);

    CHECK_PTR_EQ("dst v unchanged", dst.v, orig_v);
    CHECK_STR_EQ("dst content",     dst.v, "orig");
    CHECK_STR_EQ("result",          r.v,   "orig|a");
    CHECK_TRUE ("independent",      r.v != dst.v);

    r.v[0] = 'X';
    CHECK_STR_EQ("result mutated", r.v,   "Xrig|a");
    CHECK_STR_EQ("dst original",   dst.v, "orig");

    CHECK_FS2VALID(1, &r);
    CHECK_FS2VALID(2, &dst);

    fs2_free(&dst);
    fs2_free(&r);
    assert_balanced();
}

/* KEY: STATIC non-empty dst works (clone removes the mutate barrier) */
Test(fs2_api_modificators, concatall_str_new_static_dst_works) {
    __fs2_checkreset();
    fs2 dst = FS2LITERAL("prefix:");
    const char *arr[] = { "x", "y", NULL };

    fs2 r = fs2_concatall_str_new(&dst, "-", arr);

    CHECK_STR_EQ("prefix:-x-y",       r.v, "prefix:-x-y");
    CHECK_TRUE ("result ALLOC",       fs2_is_alloc(&r));
    CHECK_TRUE ("result not STATIC",  !fs2_is_static(&r));
    CHECK_TRUE ("dst still STATIC",   fs2_is_static(&dst));
    CHECK_STR_EQ("dst unchanged",     dst.v, "prefix:");
    CHECK_FS2VALID(1, &r);
    CHECK_FS2VALID(2, &dst);

    fs2_free(&r);
    assert_balanced();
}

/* =========================================================================
 * NULL src / empty arr -> clone of dst
 * ========================================================================= */

Test(fs2_api_modificators, concatall_str_new_null_src_returns_clone) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("hello");

    fs2 r = fs2_concatall_str_new(&dst, "-", NULL);

    CHECK_STR_EQ("hello",     r.v, "hello");
    CHECK_TRUE ("independent", r.v != dst.v);
    CHECK_FS2VALID(1, &r);
    CHECK_FS2VALID(2, &dst);

    fs2_free(&dst);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api_modificators, concatall_str_new_empty_arr_returns_clone) {
    __fs2_checkreset();
    const char *arr[] = { NULL };
    fs2 dst = fs2_fromstr("hello");

    fs2 r = fs2_concatall_str_new(&dst, "-", arr);

    CHECK_STR_EQ("hello", r.v, "hello");
    CHECK_FS2VALID(1, &r);

    fs2_free(&dst);
    fs2_free(&r);
    assert_balanced();
}

/* =========================================================================
 * Guard: NULL dst -> SIGABRT (via fs2_clone)
 * ========================================================================= */

Test(fs2_api_modificators, concatall_str_new_null_dst_aborts, .signal = SIGABRT) {
    const char *arr[] = { "a", NULL };
    fs2_concatall_str_new(NULL, "-", arr);
}

/* =========================================================================
 * Consistency: _str_new ≡ clone + _str in-place
 * ========================================================================= */

Test(fs2_api_modificators, concatall_str_new_matches_clone_then_inplace) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("p:");
    const char *arr[] = { "a", "b", NULL };

    fs2 manual = fs2_clone(&dst);
    fs2_concatall_str(&manual, "-", arr);

    fs2 via = fs2_concatall_str_new(&dst, "-", arr);

    CHECK_STR_EQ("same content", manual.v,   via.v);
    CHECK_EQ  ("same len",       manual.len, via.len);
    CHECK_FS2VALID(1, &manual);
    CHECK_FS2VALID(2, &via);

    fs2_free(&dst);
    fs2_free(&manual);
    fs2_free(&via);
    assert_balanced();
}

/* =========================================================================
 * FS2CONCAT_STR_NEW macro
 * ========================================================================= */

Test(fs2_api_modificators, fconcat_str_new_macro_three_elements) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("p:");
    fs2 r = FS2CONCAT_STR_NEW(&dst, ", ", "red", "green", "blue");

    CHECK_STR_EQ("p:, red, green, blue", r.v, "p:, red, green, blue");
    CHECK_FS2VALID(1, &r);

    fs2_free(&dst);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api_modificators, fconcat_str_new_macro_one_element) {
    __fs2_checkreset();
    fs2 dst = FS2();
    fs2 r = FS2CONCAT_STR_NEW(&dst, "-", "solo");

    CHECK_STR_EQ("solo", r.v, "solo");

    fs2_free(&dst);
    fs2_free(&r);
    assert_balanced();
}

/* =========================================================================
 * fs2_capitalize — nominal (documented semantics: skip non-alnum, upper
 * first alnum, lower rest)
 * ========================================================================= */

Test(fs2_api_modificators, capitalize_single_word) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    fs2 *r = fs2_capitalize(&s);

    CHECK_STR_EQ("Hello", s.v, "Hello");
    CHECK_PTR_EQ("chain", r, &s);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_modificators, capitalize_all_upper) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("HELLO");
    fs2_capitalize(&s);
    CHECK_STR_EQ("Hello", s.v, "Hello");
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_modificators, capitalize_mixed_case) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hELLo");
    fs2_capitalize(&s);
    CHECK_STR_EQ("Hello", s.v, "Hello");
    fs2_free(&s);
    assert_balanced();
}

/* Multi-word: ONLY the first word capitalized, rest lowercased.
 * Contrast with fs2_initcap (each word). */
Test(fs2_api_modificators, capitalize_multiword_only_first) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello WORLD");

    fs2_capitalize(&s);

    CHECK_STR_EQ("Hello world", s.v, "Hello world");   /* not "Hello World" */
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

/* Contrast with initcap on the same input */
Test(fs2_api_modificators, capitalize_vs_initcap) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("hello world");
    fs2 b = fs2_fromstr("hello world");

    fs2_capitalize(&a);   /* "Hello world" */
    fs2_initcap   (&b);   /* "Hello World" */

    CHECK_STR_EQ("capitalize", a.v, "Hello world");
    CHECK_STR_EQ("initcap",    b.v, "Hello World");

    fs2_free(&a);
    fs2_free(&b);
    assert_balanced();
}

/* =========================================================================
 * fs2_capitalize — documented "skip non-alnum" behaviour
 * ========================================================================= */

Test(fs2_api_modificators, capitalize_leading_space_skipped) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("  hello");

    fs2_capitalize(&s);

    /* Differs from Python: 'h' is uppercased, leading spaces preserved */
    CHECK_STR_EQ("  Hello", s.v, "  Hello");

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_modificators, capitalize_leading_punct_skipped) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("!hello");

    fs2_capitalize(&s);

    CHECK_STR_EQ("!Hello", s.v, "!Hello");   /* not "!hello" (Python) */

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_modificators, capitalize_leading_digit_is_first_alnum) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("123abc");

    fs2_capitalize(&s);

    /* '1' is alnum -> "first" -> toupper('1') == '1', flag consumed;
     * 'a','b','c' -> tolower (already lower). */
    CHECK_STR_EQ("123abc", s.v, "123abc");

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_modificators, capitalize_leading_underscore_skipped) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("_hello");

    fs2_capitalize(&s);

    CHECK_STR_EQ("_Hello", s.v, "_Hello");   /* '_' is not alnum */

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * All-punctuation / all-digits: nothing to capitalize, no crash
 * ========================================================================= */

Test(fs2_api_modificators, capitalize_all_punctuation_unchanged) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("!@#$%");

    fs2_capitalize(&s);

    CHECK_STR_EQ("!@#$%", s.v, "!@#$%");

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_modificators, capitalize_all_digits_unchanged) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("12345");

    fs2_capitalize(&s);

    CHECK_STR_EQ("12345", s.v, "12345");

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * Single char / empty
 * ========================================================================= */

Test(fs2_api_modificators, capitalize_single_letter) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("a");
    fs2 b = fs2_fromstr("A");
    fs2 c = fs2_fromstr("1");
    fs2 d = fs2_fromstr("!");

    fs2_capitalize(&a);
    fs2_capitalize(&b);
    fs2_capitalize(&c);
    fs2_capitalize(&d);

    CHECK_STR_EQ("'a' -> 'A'", a.v, "A");
    CHECK_STR_EQ("'A' -> 'A'", b.v, "A");
    CHECK_STR_EQ("'1' -> '1'", c.v, "1");
    CHECK_STR_EQ("'!' -> '!'", d.v, "!");

    fs2_free(&a); fs2_free(&b); fs2_free(&c); fs2_free(&d);
    assert_balanced();
}

Test(fs2_api_modificators, capitalize_fs2empty_noop) {
    __fs2_checkreset();
    fs2 s = FS2EMPTY;

    fs2_capitalize(&s);

    CHECK_TRUE("still eq_empty", fs2_eq_empty(&s));
    CHECK_FS2VALID(1, &s);
    assert_balanced();
}

Test(fs2_api_modificators, capitalize_canon_noop) {
    __fs2_checkreset();
    fs2 s = FS2();

    fs2_capitalize(&s);

    CHECK_TRUE("still canon", fs2_is_canon(&s));
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * STATIC non-empty -> eager abort (fs2_cant_mutate)
 * ========================================================================= */

Test(fs2_api_modificators, capitalize_static_nonempty_aborts, .signal = SIGABRT) {
    fs2 s = FS2LITERAL("hello");
    fs2_capitalize(&s);
}

Test(fs2_api_modificators, capitalize_static_empty_literal_noop) {
    __fs2_checkreset();
    fs2 s = FS2LITERAL("");

    fs2_capitalize(&s);

    CHECK_EQ  ("len",    s.len, 0);
    CHECK_TRUE("STATIC", fs2_is_static(&s));
    CHECK_FS2VALID(1, &s);
    assert_balanced();
}

/* =========================================================================
 * LOCAL — works (same-size transform)
 * ========================================================================= */

Test(fs2_api_modificators, capitalize_local) {
    __fs2_checkreset();
    FS2LOCAL(buf, 16);
    fs2_memcpy_str(&buf, 0, "hello world", 11);
    fs2_setlen(&buf, 11);

    fs2_capitalize(&buf);

    CHECK_STR_EQ("Hello world", buf.v, "Hello world");
    CHECK_TRUE ("still LOCAL",  fs2_is_local(&buf));
    CHECK_FS2VALID(1, &buf);
    assert_balanced();
}

/* =========================================================================
 * Guards
 * ========================================================================= */

Test(fs2_api_modificators, capitalize_null_aborts, .signal = SIGABRT) {
    fs2_capitalize(NULL);
}

/* =========================================================================
 * Idempotency: capitalize(capitalize(s)) == capitalize(s)
 * ========================================================================= */

Test(fs2_api_modificators, capitalize_idempotent) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hELLo WoRLD");
    fs2_capitalize(&s);
    char *first = strdup(s.v);
    cr_assert_not_null(first);
    fs2_capitalize(&s);
    CHECK_STR_EQ("idempotent", s.v, first);
    free(first);
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_zfill — nominal (no sign: behaves like lpad with "0")
 * ========================================================================= */

Test(fs2_api_modificators, zfill_positive_number) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42");

    fs2 *r = fs2_zfill(&s, 5);

    CHECK_STR_EQ("00042", s.v, "00042");
    CHECK_EQ  ("len",     s.len, 5);
    CHECK_PTR_EQ("chain", r, &s);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_modificators, zfill_non_numeric) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");

    fs2_zfill(&s, 5);

    CHECK_STR_EQ("00abc", s.v, "00abc");   /* no sign -> plain left-pad */

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_zfill — SIGN handling (the whole point)
 * ========================================================================= */

Test(fs2_api_modificators, zfill_negative_number) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("-42");

    fs2_zfill(&s, 5);

    CHECK_STR_EQ("-0042", s.v, "-0042");   /* NOT "00-42" */
    CHECK_EQ  ("len",     s.len, 5);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_modificators, zfill_positive_sign) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("+42");

    fs2_zfill(&s, 5);

    CHECK_STR_EQ("+0042", s.v, "+0042");

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_modificators, zfill_sign_only_minus) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("-");

    fs2_zfill(&s, 3);

    CHECK_STR_EQ("-00", s.v, "-00");       /* Python: "-".zfill(3) == "-00" */

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_modificators, zfill_sign_with_non_numeric) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("-abc");

    fs2_zfill(&s, 6);

    CHECK_STR_EQ("-00abc", s.v, "-00abc");  /* literal Python rule */

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * Contrast with fs2_lpad (which does NOT handle sign)
 * ========================================================================= */

Test(fs2_api_modificators, zfill_vs_lpad_on_negative) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("-42");
    fs2 b = fs2_fromstr("-42");
    fs2 zero = FS2LITERAL("0");

    fs2_zfill(&a, 5);                 /* "-0042" */
    fs2_lpad (&b, 5, &zero);          /* "00-42" */

    CHECK_STR_EQ("zfill", a.v, "-0042");
    CHECK_STR_EQ("lpad",  b.v, "00-42");
    CHECK_NEQ ("differ",  strcmp(a.v, b.v), 0);

    fs2_free(&a);
    fs2_free(&b);
    assert_balanced();
}

/* =========================================================================
 * Empty / canonical empty -> promoted, filled with zeros
 * ========================================================================= */

Test(fs2_api_modificators, zfill_fs2empty) {
    __fs2_checkreset();
    fs2 s = FS2EMPTY;

    fs2_zfill(&s, 5);

    CHECK_STR_EQ("00000", s.v, "00000");
    CHECK_TRUE ("ALLOC",  fs2_is_alloc(&s));
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_modificators, zfill_canon_empty) {
    __fs2_checkreset();
    fs2 s = FS2();
    cr_assert_null(s.v);

    fs2_zfill(&s, 5);

    CHECK_STR_EQ("00000", s.v, "00000");
    CHECK_TRUE ("ALLOC",  fs2_is_alloc(&s));
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * No-op: width <= len
 * ========================================================================= */

Test(fs2_api_modificators, zfill_width_eq_len_noop) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("-42");
    void *orig = s.v;

    fs2_zfill(&s, 3);

    CHECK_PTR_EQ("buffer unchanged", s.v, orig);
    CHECK_STR_EQ("-42",              s.v, "-42");

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_modificators, zfill_width_less_than_len_noop) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("12345");

    fs2_zfill(&s, 3);

    CHECK_STR_EQ("12345", s.v, "12345");

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * Guards: STATIC non-empty aborts, NULL aborts, LOCAL with capacity works
 * ========================================================================= */

Test(fs2_api_modificators, zfill_null_aborts, .signal = SIGABRT) {
    fs2_zfill(NULL, 5);
}

Test(fs2_api_modificators, zfill_static_nonempty_aborts, .signal = SIGABRT) {
    fs2 s = FS2LITERAL("42");
    fs2_zfill(&s, 5);
}

Test(fs2_api_modificators, zfill_local_with_capacity) {
    __fs2_checkreset();
    FS2LOCAL(buf, 16);
    fs2_memcpy_str(&buf, 0, "-42", 3);
    fs2_setlen(&buf, 3);

    fs2_zfill(&buf, 5);

    CHECK_STR_EQ("-0042",      buf.v, "-0042");
    CHECK_TRUE ("still LOCAL", fs2_is_local(&buf));
    CHECK_FS2VALID(1, &buf);

    assert_balanced();
}

Test(fs2_api_modificators, zfill_local_insufficient_capacity_aborts, .signal = SIGABRT) {
    FS2LOCAL(buf, 4);
    fs2_memcpy_str(&buf, 0, "-42", 3);
    fs2_setlen(&buf, 3);

    fs2_zfill(&buf, 10);   /* need 11 bytes, sz=4 -> abort in reserve */

    /* unreachable */
}

/* =========================================================================
 * Idempotency: zfill(zfill(s, n), n) == zfill(s, n)
 * ========================================================================= */

Test(fs2_api_modificators, zfill_idempotent) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("-42");

    fs2_zfill(&s, 5);
    char *first = strdup(s.v);
    cr_assert_not_null(first);

    fs2_zfill(&s, 5);

    CHECK_STR_EQ("idempotent", s.v, first);

    free(first);
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_zfill_new — clone + in-place
 * ========================================================================= */

Test(fs2_api_modificators, zfill_new_negative) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("-42");

    fs2 r = fs2_zfill_new(&src, 5);

    CHECK_STR_EQ("-0042",     r.v, "-0042");
    CHECK_TRUE ("ALLOC",      fs2_is_alloc(&r));
    CHECK_TRUE ("independent", r.v != src.v);
    CHECK_STR_EQ("src untouched", src.v, "-42");
    CHECK_FS2VALID(1, &r);
    CHECK_FS2VALID(2, &src);

    fs2_free(&src);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api_modificators, zfill_new_static_src) {
    __fs2_checkreset();
    fs2 src = FS2LITERAL("-7");

    fs2 r = fs2_zfill_new(&src, 4);

    CHECK_STR_EQ("-007",           r.v, "-007");
    CHECK_TRUE ("ALLOC",           fs2_is_alloc(&r));
    CHECK_TRUE ("src still STATIC", fs2_is_static(&src));
    CHECK_FS2VALID(1, &r);

    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api_modificators, zfill_new_width_noop_clone) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("12345");

    fs2 r = fs2_zfill_new(&src, 3);

    CHECK_STR_EQ("12345",     r.v, "12345");
    CHECK_TRUE ("independent", r.v != src.v);   /* clone even on noop */
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    fs2_free(&r);
    assert_balanced();
}

Test(fs2_api_modificators, zfill_new_null_aborts, .signal = SIGABRT) {
    fs2_zfill_new(NULL, 5);
}

#include <criterion/criterion.h>

#include "test_fs2.h"
#include "fs2_api.h"
#include <locale.h>

TestSuite(fs2_api_modificators, .init = test_setup, .fini = test_teardown);

/* =========================================================================
 * fs2_translate — nominal / PG-documented examples
 * ========================================================================= */

Test(fs2_api_modificators, translate_pg_example) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("12345");
    fs2 f = fs2_fromstr("143");
    fs2 t = fs2_fromstr("ax");

    fs2 *r = fs2_translate(&s, &f, &t);

    /* PG: translate('12345', '143', 'ax') = 'a2x5' */
    CHECK_STR_EQ("a2x5", s.v, "a2x5");
    CHECK_PTR_EQ("chain", r, &s);
    CHECK_FS2VALID(1, &s);
    CHECK_FS2VALID(2, &f);
    CHECK_FS2VALID(3, &t);

    fs2_free(&s); fs2_free(&f); fs2_free(&t);
    assert_balanced();
}

Test(fs2_api_modificators, translate_same_length_from_to) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");
    fs2 f = fs2_fromstr("lo");
    fs2 t = fs2_fromstr("01");

    fs2_translate(&s, &f, &t);

    CHECK_STR_EQ("he001", s.v, "he001");

    fs2_free(&s); fs2_free(&f); fs2_free(&t);
    assert_balanced();
}

/* to longer than from: extra ignored, no deletion */
Test(fs2_api_modificators, translate_to_longer_extra_ignored) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("12345");
    fs2 f = fs2_fromstr("123");
    fs2 t = fs2_fromstr("axzq");

    fs2_translate(&s, &f, &t);

    CHECK_STR_EQ("axz45", s.v, "axz45");

    fs2_free(&s); fs2_free(&f); fs2_free(&t);
    assert_balanced();
}

/* =========================================================================
 * delete path: to shorter than from
 * ========================================================================= */

Test(fs2_api_modificators, translate_delete_extra_from) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("aXbXc");
    fs2 f = fs2_fromstr("X");
    fs2 t = FS2EMPTY;

    fs2_translate(&s, &f, &t);

    CHECK_STR_EQ("abc", s.v, "abc");

    fs2_free(&s); fs2_free(&f);
    assert_balanced();
}

Test(fs2_api_modificators, translate_delete_vowels) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello world");
    fs2 f = fs2_fromstr("aeiou");
    fs2 t = FS2EMPTY;

    fs2_translate(&s, &f, &t);

    CHECK_STR_EQ("hll wrld", s.v, "hll wrld");

    fs2_free(&s); fs2_free(&f);
    assert_balanced();
}

/* partial mapping + delete: "143" -> "ax" */
Test(fs2_api_modificators, translate_mixed_map_and_delete) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("41233");
    fs2 f = fs2_fromstr("143");
    fs2 t = fs2_fromstr("ax");

    fs2_translate(&s, &f, &t);

    /* '4'->x, '1'->a, '2' unchanged, '3' deleted */
    CHECK_STR_EQ("xa2", s.v, "xa2");

    fs2_free(&s); fs2_free(&f); fs2_free(&t);
    assert_balanced();
}

/* =========================================================================
 * duplicate in from: FIRST wins
 * ========================================================================= */

Test(fs2_api_modificators, translate_duplicate_in_from_first_wins) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("a");
    fs2 f = fs2_fromstr("aa");
    fs2 t = fs2_fromstr("xy");

    fs2_translate(&s, &f, &t);

    CHECK_STR_EQ("x", s.v, "x");   /* not "y" */

    fs2_free(&s); fs2_free(&f); fs2_free(&t);
    assert_balanced();
}

/* =========================================================================
 * no-op cases
 * ========================================================================= */

Test(fs2_api_modificators, translate_empty_from_noop) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");
    fs2 f = FS2EMPTY;
    fs2 t = fs2_fromstr("XYZ");
    void *orig = s.v;

    fs2_translate(&s, &f, &t);

    CHECK_PTR_EQ("buffer untouched", s.v, orig);
    CHECK_STR_EQ("abc",              s.v, "abc");

    fs2_free(&s); fs2_free(&t);
    assert_balanced();
}

Test(fs2_api_modificators, translate_no_match_noop) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");
    fs2 f = fs2_fromstr("XYZ");
    fs2 t = fs2_fromstr("qrs");
    void *orig = s.v;

    fs2_translate(&s, &f, &t);

    CHECK_PTR_EQ("buffer untouched", s.v, orig);
    CHECK_STR_EQ("abc",              s.v, "abc");

    fs2_free(&s); fs2_free(&f); fs2_free(&t);
    assert_balanced();
}

Test(fs2_api_modificators, translate_empty_src_all_flavors) {
    __fs2_checkreset();
    fs2 f = fs2_fromstr("abc");
    fs2 t = fs2_fromstr("XYZ");

    fs2 canon = FS2();
    fs2 empty = FS2EMPTY;
    fs2 lit   = FS2LITERAL("");

    fs2_translate(&canon, &f, &t);
    fs2_translate(&empty, &f, &t);
    fs2_translate(&lit,   &f, &t);

    CHECK_TRUE("canon canon", fs2_is_canon(&canon));
    CHECK_TRUE("empty empty", fs2_eq_empty(&empty));
    CHECK_EQ  ("lit len",     lit.len, 0);

    fs2_free(&canon);
    fs2_free(&f);
    fs2_free(&t);
    assert_balanced();
}

/* =========================================================================
 * guards (eager, like other mutators)
 * ========================================================================= */

Test(fs2_api_modificators, translate_null_s_aborts, .signal = SIGABRT) {
    fs2 f = fs2_fromstr("a");
    fs2 t = fs2_fromstr("b");
    fs2_translate(NULL, &f, &t);
    fs2_free(&f); fs2_free(&t);
}

Test(fs2_api_modificators, translate_null_from_aborts, .signal = SIGABRT) {
    fs2 s = fs2_fromstr("abc");
    fs2 t = fs2_fromstr("b");
    fs2_translate(&s, NULL, &t);
    fs2_free(&s); fs2_free(&t);
}

Test(fs2_api_modificators, translate_null_to_aborts, .signal = SIGABRT) {
    fs2 s = fs2_fromstr("abc");
    fs2 f = fs2_fromstr("a");
    fs2_translate(&s, &f, NULL);
    fs2_free(&s); fs2_free(&f);
}

Test(fs2_api_modificators, translate_static_nonempty_aborts, .signal = SIGABRT) {
    fs2 s = FS2LITERAL("hello");
    fs2 f = fs2_fromstr("lo");
    fs2 t = fs2_fromstr("01");
    fs2_translate(&s, &f, &t);
    fs2_free(&f); fs2_free(&t);
}

/* =========================================================================
 * LOCAL — never grows, always works
 * ========================================================================= */

Test(fs2_api_modificators, translate_local) {
    __fs2_checkreset();
    FS2LOCAL(buf, 16);
    fs2_memcpy_str(&buf, 0, "hello", 5);
    fs2_setlen(&buf, 5);
    fs2 f = fs2_fromstr("aeiou");
    fs2 t = FS2EMPTY;

    fs2_translate(&buf, &f, &t);

    CHECK_STR_EQ("hll",       buf.v, "hll");
    CHECK_TRUE ("still LOCAL", fs2_is_local(&buf));
    CHECK_FS2VALID(1, &buf);

    fs2_free(&f);
    assert_balanced();
}

/* =========================================================================
 * str wrapper — NULL args lenient via _fs2_wrap
 * ========================================================================= */

Test(fs2_api_modificators, translate_str_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");

    fs2_translate_str(&s, "ac", "XY");

    CHECK_STR_EQ("XbY", s.v, "XbY");

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_modificators, translate_str_null_from_noop) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");

    fs2_translate_str(&s, NULL, "XY");   /* from=NULL → FS2EMPTY → no-op */

    CHECK_STR_EQ("abc", s.v, "abc");

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api_modificators, translate_str_null_to_deletes) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("aXbXc");

    fs2_translate_str(&s, "X", NULL);    /* to=NULL → FS2EMPTY → delete */

    CHECK_STR_EQ("abc", s.v, "abc");

    fs2_free(&s);
    assert_balanced();
}
