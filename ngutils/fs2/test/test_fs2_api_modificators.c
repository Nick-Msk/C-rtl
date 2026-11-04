#include <criterion/criterion.h>

#include "test_fs2.h"
#include "fs2_api.h"
#include <locale.h>

TestSuite(fs2_api_modificators, .init = test_setup, .fini = test_teardown);

/* =========================================================================
 * fs2_replace — nominal (ALLOC source)
 * ========================================================================= */

Test(fs2_api, replace_single) {
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

Test(fs2_api, replace_multiple) {
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

Test(fs2_api, replace_at_start) {
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

Test(fs2_api, replace_at_end) {
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

Test(fs2_api, replace_overlapping_aaa_to_ba) {
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

Test(fs2_api, replace_overlapping_aaaa_to_bb) {
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

Test(fs2_api, replace_sequential_multi) {
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

Test(fs2_api, replace_grow) {
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

Test(fs2_api, replace_shrink) {
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

Test(fs2_api, replace_delete) {
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

Test(fs2_api, replace_identity_no_change) {
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

Test(fs2_api, replace_not_found) {
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

Test(fs2_api, replace_pat_longer_than_src) {
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

Test(fs2_api, replace_empty_pat_noop) {
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

Test(fs2_api, replace_empty_src_fs2empty) {
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

Test(fs2_api, replace_empty_src_canon) {
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

Test(fs2_api, replace_static_empty_src_noop) {
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

Test(fs2_api, replace_static_nonempty_src_aborts, .signal = SIGABRT) {
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

Test(fs2_api, replace_local_shrink_works) {
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

Test(fs2_api, replace_local_grow_within_capacity_works) {
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

Test(fs2_api, replace_local_grow_beyond_capacity_aborts, .signal = SIGABRT) {
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

Test(fs2_api, replace_null_s_aborts, .signal = SIGABRT) {
    fs2 pat  = fs2_fromstr("a");
    fs2 repl = fs2_fromstr("b");
    fs2_replace(NULL, &pat, &repl);
    fs2_free(&pat);
    fs2_free(&repl);
}

Test(fs2_api, replace_null_pat_aborts, .signal = SIGABRT) {
    fs2 s    = fs2_fromstr("abc");
    fs2 repl = fs2_fromstr("b");
    fs2_replace(&s, NULL, &repl);
    fs2_free(&s);
    fs2_free(&repl);
}

Test(fs2_api, replace_null_repl_aborts, .signal = SIGABRT) {
    fs2 s   = fs2_fromstr("abc");
    fs2 pat = fs2_fromstr("b");
    fs2_replace(&s, &pat, NULL);
    fs2_free(&s);
    fs2_free(&pat);
}

/* =========================================================================
 * fs2_replace — pattern/repl untouched, s buffer grown as needed
 * ========================================================================= */

Test(fs2_api, replace_pat_and_repl_untouched) {
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

Test(fs2_api, replace_fs2empty_repl_deletes) {
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

Test(fs2_api, replace_canon_repl_deletes) {
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

Test(fs2_api, replace_pat_repl_flavor_matrix) {
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

Test(fs2_api, replace_empty_pat_flavors_noop) {
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

Test(fs2_api, replace_empty_src_flavors_stay_empty) {
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
Test(fs2_api, replace_empty_src_empty_pat_noop) {
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

Test(fs2_api, replace_empty_repl_flavors_delete) {
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

Test(fs2_api, replace_pat_repl_equal_different_objects) {
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

Test(fs2_api, replace_local_dst_matrix) {
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

Test(fs2_api_modif, replace_fromstr_null_repl_deletes) {
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

Test(fs2_api_modif, replace_fromstr_empty_str_repl_deletes) {
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
Test(fs2_api_modif, replace_fromstr_null_and_empty_repl_same) {
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
Test(fs2_api_modif, replace_fromstr_normal) {
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

Test(fs2_api_modif, replace_fromstr_null_pat_aborts, .signal = SIGABRT) {
    fs2 s = fs2_fromstr("abc");
    fs2_replace_fromstr(&s, NULL, "X");
    fs2_free(&s);
}

Test(fs2_api_modif, replace_fromstr_null_s_aborts, .signal = SIGABRT) {
    fs2 pat = fs2_fromstr("a");
    fs2_replace_fromstr(NULL, &pat, "X");
    fs2_free(&pat);
}

/* =========================================================================
 * fs2_replace_cstr — BOTH args lenient (pat AND repl via _fs2_wrap)
 * ========================================================================= */

Test(fs2_api_modif, replace_cstr_normal) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("one apple, two apple");

    fs2_replace_cstr(&s, "apple", "banana");

    CHECK_STR_EQ("one banana, two banana", s.v, "one banana, two banana");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* NULL pat -> _fs2_wrap(NULL) -> FS2EMPTY -> fs2_replace early-returns (no-op) */
Test(fs2_api_modif, replace_cstr_null_pat_noop) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    fs2_replace_cstr(&s, NULL, "X");

    CHECK_STR_EQ("hello", s.v, "hello");   /* pat empty -> no-op */

    fs2_free(&s);
    assert_balanced();
}

/* NULL repl -> _fs2_wrap(NULL) -> FS2EMPTY -> delete */
Test(fs2_api_modif, replace_cstr_null_repl_deletes) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("aXbXc");

    fs2_replace_cstr(&s, "X", NULL);

    CHECK_STR_EQ("abc", s.v, "abc");

    fs2_free(&s);
    assert_balanced();
}

/* Both NULL -> both wrapped as FS2EMPTY -> pat empty -> no-op */
Test(fs2_api_modif, replace_cstr_both_null_noop) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    fs2_replace_cstr(&s, NULL, NULL);

    CHECK_STR_EQ("hello", s.v, "hello");

    fs2_free(&s);
    assert_balanced();
}

/* Both "" — pat empty -> no-op */
Test(fs2_api_modif, replace_cstr_both_empty_noop) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    fs2_replace_cstr(&s, "", "");

    CHECK_STR_EQ("hello", s.v, "hello");

    fs2_free(&s);
    assert_balanced();
}

/* Only dst NULL aborts (invraise in fs2_replace) */
Test(fs2_api_modif, replace_cstr_null_s_aborts, .signal = SIGABRT) {
    fs2_replace_cstr(NULL, "a", "b");
}

/* =========================================================================
 * Consistency: fromstr ≡ replace with explicit FS2EMPTY for NULL repl
 * ========================================================================= */

Test(fs2_api_modif, replace_fromstr_null_equiv_to_explicit_empty) {
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
