#include <criterion/criterion.h>

#include "test_fs2.h"
#include "fs2_api.h"
#include <locale.h>

TestSuite(fs2_api_instr, .init = test_setup, .fini = test_teardown);

/* =========================================================================
 * Empty pattern — Python semantics matrix
 *
 * | str      | pattern  | pos | nth | expected |
 * |----------|----------|-----|-----|----------|
 * | FS2EMPTY | FS2EMPTY | 0   | 1   | 0        |
 * | FS2()    | FS2EMPTY | 0   | 1   | 0 (after consistency fix) |
 * | "abc"    | FS2EMPTY | 0   | 1   | 0        |
 * | "abc"    | FS2EMPTY | 0   | 4   | 3        |
 * | "abc"    | FS2EMPTY | 0   | 5   | -1       |
 * | "abc"    | FS2EMPTY | 4   | 1   | -1       |
 * ========================================================================= */

Test(fs2_api_instr, empty_pattern_fs2empty_str) {
    __fs2_checkreset();
    fs2 str = FS2EMPTY;
    fs2 pat = FS2EMPTY;

    CHECK_EQ("pos=0 nth=1", fs2_instr_pos_nth(&str, &pat, 0, 1), 0);
    CHECK_EQ("pos=0 nth=2", fs2_instr_pos_nth(&str, &pat, 0, 2), -1);

    assert_balanced();
}

// Documented contract: FS2() has no buffer -> 0.
Test(fs2_api_instr, empty_pattern_fs2_str) {
    __fs2_checkreset();
    fs2 str = FS2();          /* v == NULL */
    fs2 pat = FS2EMPTY;

    CHECK_EQ("pos=0 nth=1", fs2_instr_pos_nth(&str, &pat, 0, 1), 0);

    fs2_free(&str);
    assert_balanced();
}

Test(fs2_api_instr, empty_pattern_matrix) {
    __fs2_checkreset();
    fs2 str = fs2_fromstr("abc");   /* len=3 */
    fs2 pat = FS2EMPTY;

    CHECK_EQ("pos=0 nth=1", fs2_instr_pos_nth(&str, &pat, 0, 1), 0);
    CHECK_EQ("pos=0 nth=2", fs2_instr_pos_nth(&str, &pat, 0, 2), 1);
    CHECK_EQ("pos=0 nth=3", fs2_instr_pos_nth(&str, &pat, 0, 3), 2);
    CHECK_EQ("pos=0 nth=4", fs2_instr_pos_nth(&str, &pat, 0, 4), 3);
    CHECK_EQ("pos=0 nth=5", fs2_instr_pos_nth(&str, &pat, 0, 5), -1);

    CHECK_EQ("pos=3 nth=1", fs2_instr_pos_nth(&str, &pat, 3, 1), 3);
    CHECK_EQ("pos=3 nth=2", fs2_instr_pos_nth(&str, &pat, 3, 2), -1);
    CHECK_EQ("pos=4 nth=1", fs2_instr_pos_nth(&str, &pat, 4, 1), -1);

    fs2_free(&str);
    assert_balanced();
}

/* =========================================================================
 * Overlapping matches — "aaa" / "aa" -> 0, 1 (not 0, -1 for non-overlapping)
 * ========================================================================= */

Test(fs2_api_instr, overlapping_matches) {
    __fs2_checkreset();
    fs2 str = fs2_fromstr("aaa");
    fs2 pat = fs2_fromstr("aa");

    CHECK_EQ("nth=1", fs2_instr_pos_nth(&str, &pat, 0, 1), 0);
    CHECK_EQ("nth=2", fs2_instr_pos_nth(&str, &pat, 0, 2), 1);
    CHECK_EQ("nth=3", fs2_instr_pos_nth(&str, &pat, 0, 3), -1);

    fs2_free(&str);
    fs2_free(&pat);
    assert_balanced();
}

/* =========================================================================
 * Boundary: pos near end, pattern longer than remaining
 * ========================================================================= */

Test(fs2_api_instr, pattern_longer_than_remaining) {
    __fs2_checkreset();
    fs2 str = fs2_fromstr("abc");
    fs2 pat = fs2_fromstr("abcd");

    CHECK_EQ("pattern too long", fs2_instr_pos_nth(&str, &pat, 0, 1), -1);

    fs2_free(&str);
    fs2_free(&pat);
    assert_balanced();
}

Test(fs2_api_instr, pattern_fits_exactly_at_tail) {
    __fs2_checkreset();
    fs2 str = fs2_fromstr("abc");
    fs2 pat = fs2_fromstr("bc");     /* len=2, fits only at pos=1 */

    CHECK_EQ("pos=0 nth=1", fs2_instr_pos_nth(&str, &pat, 0, 1), 1);
    CHECK_EQ("pos=1 nth=1", fs2_instr_pos_nth(&str, &pat, 1, 1), 1);
    CHECK_EQ("pos=2 nth=1", fs2_instr_pos_nth(&str, &pat, 2, 1), -1);   /* need 2 bytes, only 1 left */

    fs2_free(&str);
    fs2_free(&pat);
    assert_balanced();
}

/* =========================================================================
 * FS2EMPTY as pattern with non-empty str, ci variant too
 * ========================================================================= */

Test(fs2_api_instr, empty_pattern_ci) {
    __fs2_checkreset();
    fs2 str = fs2_fromstr("AbC");
    fs2 pat = FS2EMPTY;

    CHECK_EQ("pos=0 nth=1", fs2_instr_pos_ci_nth(&str, &pat, 0, 1), 0);
    CHECK_EQ("pos=0 nth=4", fs2_instr_pos_ci_nth(&str, &pat, 0, 4), 3);
    CHECK_EQ("pos=0 nth=5", fs2_instr_pos_ci_nth(&str, &pat, 0, 5), -1);

    fs2_free(&str);
    assert_balanced();
}

/* =========================================================================
 * fs2_instr_pos_nth — core, case-sensitive
 * ========================================================================= */

Test(fs2_api_instr, instr_pos_nth_basic) {
    __fs2_checkreset();
    fs2 h = fs2_fromstr("hello world");
    fs2 p = fs2_fromstr("o");

    CHECK_EQ("nth=1", fs2_instr_pos_nth(&h, &p, 0, 1), 4);
    CHECK_EQ("nth=2", fs2_instr_pos_nth(&h, &p, 0, 2), 7);
    CHECK_EQ("nth=3", fs2_instr_pos_nth(&h, &p, 0, 3), -1);

    CHECK_FS2VALID(1, &h);
    CHECK_FS2VALID(2, &p);
    fs2_free(&h);
    fs2_free(&p);
    assert_balanced();
}

Test(fs2_api_instr, instr_pos_nth_zero) {
    __fs2_checkreset();
    fs2 h = fs2_fromstr("hello");
    fs2 p = fs2_fromstr("l");
    CHECK_EQ("nth=0 → -1", fs2_instr_pos_nth(&h, &p, 0, 0), -1);
    fs2_free(&h);
    fs2_free(&p);
    assert_balanced();
}

Test(fs2_api_instr, instr_pos_nth_pos_skips_earlier) {
    __fs2_checkreset();
    fs2 h = fs2_fromstr("banana");   /* "an" at 1, 3 */
    fs2 p = fs2_fromstr("an");

    CHECK_EQ("pos=0 nth=1", fs2_instr_pos_nth(&h, &p, 0, 1), 1);
    CHECK_EQ("pos=0 nth=2", fs2_instr_pos_nth(&h, &p, 0, 2), 3);

    /* pos=2 → "an" at 1 is excluded */
    CHECK_EQ("pos=2 nth=1", fs2_instr_pos_nth(&h, &p, 2, 1), 3);
    CHECK_EQ("pos=2 nth=2", fs2_instr_pos_nth(&h, &p, 2, 2), -1);

    fs2_free(&h);
    fs2_free(&p);
    assert_balanced();
}

/* Overlapping occurrences are counted separately: "aaa"/"aa" → 0 and 1 */
Test(fs2_api_instr, instr_pos_nth_overlapping) {
    __fs2_checkreset();
    fs2 h = fs2_fromstr("aaa");
    fs2 p = fs2_fromstr("aa");

    CHECK_EQ("nth=1", fs2_instr_pos_nth(&h, &p, 0, 1), 0);
    CHECK_EQ("nth=2", fs2_instr_pos_nth(&h, &p, 0, 2), 1);
    CHECK_EQ("nth=3", fs2_instr_pos_nth(&h, &p, 0, 3), -1);

    fs2_free(&h);
    fs2_free(&p);
    assert_balanced();
}

Test(fs2_api_instr, instr_pos_nth_not_found_and_too_long) {
    __fs2_checkreset();
    fs2 h = fs2_fromstr("abc");
    fs2 p1 = fs2_fromstr("xyz");
    fs2 p2 = fs2_fromstr("abcd");

    CHECK_EQ("not found",   fs2_instr_pos_nth(&h, &p1, 0, 1), -1);
    CHECK_EQ("too long",    fs2_instr_pos_nth(&h, &p2, 0, 1), -1);

    fs2_free(&h);
    fs2_free(&p1);
    fs2_free(&p2);
    assert_balanced();
}

Test(fs2_api_instr, instr_pos_nth_pos_at_or_beyond_len) {
    __fs2_checkreset();
    fs2 h = fs2_fromstr("abc");
    fs2 p = fs2_fromstr("a");

    CHECK_EQ("pos=len",   fs2_instr_pos_nth(&h, &p, 3, 1), -1);
    CHECK_EQ("pos>len",   fs2_instr_pos_nth(&h, &p, 100, 1), -1);

    fs2_free(&h);
    fs2_free(&p);
    assert_balanced();
}

/* =========================================================================
 * fs2_instr_pos_nth — empty pattern (Python semantics)
 * ========================================================================= */

Test(fs2_api_instr, instr_pos_nth_empty_pattern_from_zero) {
    __fs2_checkreset();
    fs2 h = fs2_fromstr("abc");
    fs2 p = FS2EMPTY;

    CHECK_EQ("nth=1", fs2_instr_pos_nth(&h, &p, 0, 1), 0);
    CHECK_EQ("nth=2", fs2_instr_pos_nth(&h, &p, 0, 2), 1);
    CHECK_EQ("nth=3", fs2_instr_pos_nth(&h, &p, 0, 3), 2);
    CHECK_EQ("nth=4", fs2_instr_pos_nth(&h, &p, 0, 4), 3);
    CHECK_EQ("nth=5", fs2_instr_pos_nth(&h, &p, 0, 5), -1);

    fs2_free(&h);
    assert_balanced();
}

Test(fs2_api_instr, instr_pos_nth_empty_pattern_with_pos) {
    __fs2_checkreset();
    fs2 h = fs2_fromstr("abc");
    fs2 p = FS2EMPTY;

    CHECK_EQ("pos=1 nth=1", fs2_instr_pos_nth(&h, &p, 1, 1), 1);
    CHECK_EQ("pos=1 nth=2", fs2_instr_pos_nth(&h, &p, 1, 2), 2);
    CHECK_EQ("pos=1 nth=3", fs2_instr_pos_nth(&h, &p, 1, 3), 3);
    CHECK_EQ("pos=1 nth=4", fs2_instr_pos_nth(&h, &p, 1, 4), -1);

    CHECK_EQ("pos=3 nth=1", fs2_instr_pos_nth(&h, &p, 3, 1), 3);
    CHECK_EQ("pos=3 nth=2", fs2_instr_pos_nth(&h, &p, 3, 2), -1);

    CHECK_EQ("pos=4 nth=1", fs2_instr_pos_nth(&h, &p, 4, 1), -1);

    fs2_free(&h);
    assert_balanced();
}

/* =========================================================================
 * fs2_instr_pos_ci_nth — core, case-insensitive
 * ========================================================================= */

Test(fs2_api_instr, instr_pos_ci_nth_basic) {
    __fs2_checkreset();
    fs2 h = fs2_fromstr("Hello World");
    fs2 p = fs2_fromstr("O");

    CHECK_EQ("nth=1", fs2_instr_pos_ci_nth(&h, &p, 0, 1), 4);
    CHECK_EQ("nth=2", fs2_instr_pos_ci_nth(&h, &p, 0, 2), 7);
    CHECK_EQ("nth=3", fs2_instr_pos_ci_nth(&h, &p, 0, 3), -1);

    fs2_free(&h);
    fs2_free(&p);
    assert_balanced();
}

Test(fs2_api_instr, instr_pos_ci_nth_mixed_case) {
    __fs2_checkreset();
    fs2 h = fs2_fromstr("HeLLo");

    fs2 p_lower = fs2_fromstr("ll");
    fs2 p_upper = fs2_fromstr("LL");
    fs2 p_mixed = fs2_fromstr("Ll");

    CHECK_EQ("lower", fs2_instr_pos_ci_nth(&h, &p_lower, 0, 1), 2);
    CHECK_EQ("upper", fs2_instr_pos_ci_nth(&h, &p_upper, 0, 1), 2);
    CHECK_EQ("mixed", fs2_instr_pos_ci_nth(&h, &p_mixed, 0, 1), 2);

    fs2_free(&h);
    fs2_free(&p_lower);
    fs2_free(&p_upper);
    fs2_free(&p_mixed);
    assert_balanced();
}

Test(fs2_api_instr, instr_pos_ci_nth_overlapping) {
    __fs2_checkreset();
    fs2 h = fs2_fromstr("aAa");
    fs2 p = fs2_fromstr("AA");

    CHECK_EQ("nth=1", fs2_instr_pos_ci_nth(&h, &p, 0, 1), 0);
    CHECK_EQ("nth=2", fs2_instr_pos_ci_nth(&h, &p, 0, 2), 1);
    CHECK_EQ("nth=3", fs2_instr_pos_ci_nth(&h, &p, 0, 3), -1);

    fs2_free(&h);
    fs2_free(&p);
    assert_balanced();
}

Test(fs2_api_instr, instr_pos_ci_nth_nth_zero) {
    __fs2_checkreset();
    fs2 h = fs2_fromstr("HeLLo");
    fs2 p = fs2_fromstr("l");
    CHECK_EQ("nth=0", fs2_instr_pos_ci_nth(&h, &p, 0, 0), -1);
    fs2_free(&h);
    fs2_free(&p);
    assert_balanced();
}

/* Empty pattern in ci — same Python semantics, case doesn't matter */
Test(fs2_api_instr, instr_pos_ci_nth_empty_pattern) {
    __fs2_checkreset();
    fs2 h = fs2_fromstr("aBc");
    fs2 p = FS2EMPTY;

    CHECK_EQ("nth=1", fs2_instr_pos_ci_nth(&h, &p, 0, 1), 0);
    CHECK_EQ("nth=4", fs2_instr_pos_ci_nth(&h, &p, 0, 4), 3);
    CHECK_EQ("nth=5", fs2_instr_pos_ci_nth(&h, &p, 0, 5), -1);

    fs2_free(&h);
    assert_balanced();
}

/* =========================================================================
 * Wrappers fs2_instr / _from / _nth — case-sensitive
 * ========================================================================= */

Test(fs2_api_instr, instr_first) {
    __fs2_checkreset();
    fs2 h = fs2_fromstr("hello world");
    fs2 p = fs2_fromstr("o");

    /* fs2_instr == fs2_instr_pos_nth(..., 0, 1) */
    CHECK_EQ("first 'o'", fs2_instr(&h, &p), 4);
    fs2_free(&h);
    fs2_free(&p);
    assert_balanced();
}

Test(fs2_api_instr, instr_from_skips_prefix) {
    __fs2_checkreset();
    fs2 h = fs2_fromstr("banana");
    fs2 p = fs2_fromstr("an");

    CHECK_EQ("from 0", fs2_instr_from(&h, &p, 0), 1);
    CHECK_EQ("from 2", fs2_instr_from(&h, &p, 2), 3);
    CHECK_EQ("from 4", fs2_instr_from(&h, &p, 4), -1);

    fs2_free(&h);
    fs2_free(&p);
    assert_balanced();
}

Test(fs2_api_instr, instr_nth_from_zero) {
    __fs2_checkreset();
    fs2 h = fs2_fromstr("hello world");
    fs2 p = fs2_fromstr("o");

    CHECK_EQ("nth=1", fs2_instr_nth(&h, &p, 1), 4);
    CHECK_EQ("nth=2", fs2_instr_nth(&h, &p, 2), 7);
    CHECK_EQ("nth=0", fs2_instr_nth(&h, &p, 0), -1);

    fs2_free(&h);
    fs2_free(&p);
    assert_balanced();
}

/* Wrappers are equivalent to core calls with proper arguments */
Test(fs2_api_instr, instr_wrappers_match_core) {
    __fs2_checkreset();
    fs2 h = fs2_fromstr("mississippi");
    fs2 p = fs2_fromstr("ss");

    CHECK_EQ("fs2_instr",       fs2_instr(&h, &p),       fs2_instr_pos_nth(&h, &p, 0, 1));
    CHECK_EQ("fs2_instr_from",  fs2_instr_from(&h, &p, 3), fs2_instr_pos_nth(&h, &p, 3, 1));
    CHECK_EQ("fs2_instr_nth",   fs2_instr_nth(&h, &p, 2),  fs2_instr_pos_nth(&h, &p, 0, 2));

    fs2_free(&h);
    fs2_free(&p);
    assert_balanced();
}

/* =========================================================================
 * Wrappers fs2_instr_ci / _from_ci / _nth_ci — case-insensitive
 * ========================================================================= */

Test(fs2_api_instr, instr_ci_first) {
    __fs2_checkreset();
    fs2 h = fs2_fromstr("Hello World");
    fs2 p = fs2_fromstr("O");

    CHECK_EQ("first 'O'", fs2_instr_ci(&h, &p), 4);
    fs2_free(&h);
    fs2_free(&p);
    assert_balanced();
}

Test(fs2_api_instr, instr_from_ci) {
    __fs2_checkreset();
    fs2 h = fs2_fromstr("HeLLo");
    fs2 p = fs2_fromstr("l");

    CHECK_EQ("from 0", fs2_instr_from_ci(&h, &p, 0), 2);
    CHECK_EQ("from 3", fs2_instr_from_ci(&h, &p, 3), 3);
    CHECK_EQ("from 4", fs2_instr_from_ci(&h, &p, 4), -1);

    fs2_free(&h);
    fs2_free(&p);
    assert_balanced();
}

Test(fs2_api_instr, instr_nth_ci) {
    __fs2_checkreset();
    fs2 h = fs2_fromstr("aAaA");
    fs2 p = fs2_fromstr("A");

    CHECK_EQ("nth=1", fs2_instr_nth_ci(&h, &p, 1), 0);
    CHECK_EQ("nth=2", fs2_instr_nth_ci(&h, &p, 2), 1);
    CHECK_EQ("nth=3", fs2_instr_nth_ci(&h, &p, 3), 2);
    CHECK_EQ("nth=4", fs2_instr_nth_ci(&h, &p, 4), 3);
    CHECK_EQ("nth=5", fs2_instr_nth_ci(&h, &p, 5), -1);

    fs2_free(&h);
    fs2_free(&p);
    assert_balanced();
}

/* =========================================================================
 * C-str wrappers — smoke test: equivalence with fs2 version
 * ========================================================================= */

Test(fs2_api_instr, instr_str_matches_fs2_version) {
    __fs2_checkreset();
    fs2 h  = fs2_fromstr("mississippi");
    fs2 p  = fs2_fromstr("ss");

    CHECK_EQ("fs2_instr_str",       fs2_instr_str(&h, "ss"),       fs2_instr(&h, &p));
    CHECK_EQ("fs2_instr_from_str",  fs2_instr_from_str(&h, "ss", 3), fs2_instr_from(&h, &p, 3));
    CHECK_EQ("fs2_instr_nth_str",   fs2_instr_nth_str(&h, "ss", 2), fs2_instr_nth(&h, &p, 2));

    CHECK_EQ("fs2_instr_str_ci",      fs2_instr_str_ci(&h, "SS"),       fs2_instr_ci(&h, &p));
    CHECK_EQ("fs2_instr_from_str_ci", fs2_instr_from_str_ci(&h, "SS", 3), fs2_instr_from_ci(&h, &p, 3));
    CHECK_EQ("fs2_instr_nth_str_ci",  fs2_instr_nth_str_ci(&h, "SS", 2),  fs2_instr_nth_ci(&h, &p, 2));

    fs2_free(&h);
    fs2_free(&p);
    assert_balanced();
}

/* =========================================================================
 * Edge cases: empty haystack / STATIC
 * ========================================================================= */

Test(fs2_api_instr, instr_empty_haystack) {
    __fs2_checkreset();
    fs2 h = FS2EMPTY;
    fs2 p = fs2_fromstr("a");

    CHECK_EQ("nonempty pattern", fs2_instr_pos_nth(&h, &p, 0, 1), -1);

    /* empty pattern on empty haystack: the only position is 0 */
    fs2 e = FS2EMPTY;
    CHECK_EQ("nth=1", fs2_instr_pos_nth(&h, &e, 0, 1), 0);
    CHECK_EQ("nth=2", fs2_instr_pos_nth(&h, &e, 0, 2), -1);

    fs2_free(&p);
    assert_balanced();
}

Test(fs2_api_instr, instr_static_haystack) {
    __fs2_checkreset();
    fs2 h = FS2LITERAL("hello world");
    fs2 p = fs2_fromstr("o");

    CHECK_EQ("first 'o'", fs2_instr(&h, &p), 4);
    CHECK_EQ("last 'o'",  fs2_instr_nth(&h, &p, 2), 7);

    fs2_free(&p);
    assert_balanced();
}

/* =========================================================================
 * NULL-guards (debug: SIGABRT via invraise)
 * ========================================================================= */

Test(fs2_api_instr, instr_null_str_aborts, .signal = SIGABRT) {
    fs2 p = fs2_fromstr("x");
    fs2_instr_pos_nth(NULL, &p, 0, 1);
    fs2_free(&p);
}

Test(fs2_api_instr, instr_null_pattern_aborts, .signal = SIGABRT) {
    fs2 h = fs2_fromstr("hello");
    fs2_instr_pos_nth(&h, NULL, 0, 1);
    fs2_free(&h);
}

Test(fs2_api_instr, instr_ci_null_str_aborts, .signal = SIGABRT) {
    fs2 p = fs2_fromstr("x");
    fs2_instr_pos_ci_nth(NULL, &p, 0, 1);
    fs2_free(&p);
}

Test(fs2_api_instr, instr_ci_null_pattern_aborts, .signal = SIGABRT) {
    fs2 h = fs2_fromstr("hello");
    fs2_instr_pos_ci_nth(&h, NULL, 0, 1);
    fs2_free(&h);
}

/* =========================================================================
 * fs2_instr_posn_str / _ci — NULL pattern → _fs2_wrap(NULL) → FS2EMPTY
 *   (unlike fs2_instr_pos_nth which aborts on NULL pattern)
 * ========================================================================= */

Test(fs2_api_instr, instr_posn_str_null_pattern_is_empty_semantics) {
    __fs2_checkreset();
    fs2 str = fs2_fromstr("abc");

    /* NULL pattern → wrapped as FS2EMPTY → Python empty-pattern */
    CHECK_EQ("pos=0 nth=1", fs2_instr_posn_str(&str, NULL, 0, 1), 0);
    CHECK_EQ("pos=0 nth=4", fs2_instr_posn_str(&str, NULL, 0, 4), 3);
    CHECK_EQ("pos=0 nth=5", fs2_instr_posn_str(&str, NULL, 0, 5), -1);
    CHECK_EQ("pos=3 nth=1", fs2_instr_posn_str(&str, NULL, 3, 1), 3);

    CHECK_FS2VALID(1, &str);
    fs2_free(&str);
    assert_balanced();
}

Test(fs2_api_instr, instr_posn_str_ci_null_pattern_is_empty_semantics) {
    __fs2_checkreset();
    fs2 str = fs2_fromstr("AbC");

    CHECK_EQ("pos=0 nth=1", fs2_instr_posn_str_ci(&str, NULL, 0, 1), 0);
    CHECK_EQ("pos=0 nth=4", fs2_instr_posn_str_ci(&str, NULL, 0, 4), 3);
    CHECK_EQ("pos=0 nth=5", fs2_instr_posn_str_ci(&str, NULL, 0, 5), -1);

    fs2_free(&str);
    assert_balanced();
}

/* "for the record": core version aborts on NULL pattern, wrapper does not.
 * This is the whole reason _fs2_wrap (lenient) is used here. */
Test(fs2_api_instr, instr_posn_null_pattern_aborts, .signal = SIGABRT) {
    fs2 str = fs2_fromstr("abc");
    fs2_instr_pos_nth(&str, NULL, 0, 1);
    fs2_free(&str);
}

/* =========================================================================
 * fs2_instr_posn_str — delegation (non-NULL pattern)
 * ========================================================================= */

Test(fs2_api_instr, instr_posn_str_delegates) {
    __fs2_checkreset();
    fs2       str = fs2_fromstr("mississippi");
    fs2       pat = fs2_fromstr("ss");

    CHECK_EQ("pos=0 nth=1", fs2_instr_posn_str(&str, "ss", 0, 1),
                            fs2_instr_pos_nth(&str, &pat, 0, 1));
    CHECK_EQ("pos=0 nth=2", fs2_instr_posn_str(&str, "ss", 0, 2),
                            fs2_instr_pos_nth(&str, &pat, 0, 2));
    CHECK_EQ("pos=4 nth=1", fs2_instr_posn_str(&str, "ss", 4, 1),
                            fs2_instr_pos_nth(&str, &pat, 4, 1));
    CHECK_EQ("nth=0",       fs2_instr_posn_str(&str, "ss", 0, 0),
                            fs2_instr_pos_nth(&str, &pat, 0, 0));

    fs2_free(&str);
    fs2_free(&pat);
    assert_balanced();
}

Test(fs2_api_instr, instr_posn_str_ci_delegates) {
    __fs2_checkreset();
    fs2       str = fs2_fromstr("MiSsIsSiPpI");
    fs2       pat = fs2_fromstr("SS");

    CHECK_EQ("pos=0 nth=1", fs2_instr_posn_str_ci(&str, "ss", 0, 1),
                            fs2_instr_pos_ci_nth(&str, &pat, 0, 1));
    CHECK_EQ("pos=0 nth=2", fs2_instr_posn_str_ci(&str, "ss", 0, 2),
                            fs2_instr_pos_ci_nth(&str, &pat, 0, 2));
    CHECK_EQ("pos=4 nth=1", fs2_instr_posn_str_ci(&str, "ss", 4, 1),
                            fs2_instr_pos_ci_nth(&str, &pat, 4, 1));

    fs2_free(&str);
    fs2_free(&pat);
    assert_balanced();
}

/* =========================================================================
 * fs2_instr_from_str / _nth_str / _str — CS
 *   One test covering all three via delegation vs their fs2 equivalents
 * ========================================================================= */

Test(fs2_api_instr, instr_from_nth_str_wrappers) {
    __fs2_checkreset();
    fs2  str = fs2_fromstr("banana");
    fs2  pat = fs2_fromstr("an");

    /* fs2_instr_str == posn_str(..., 0, 1) */
    CHECK_EQ("instr_str",       fs2_instr_str(&str, "an"),          1);
    CHECK_EQ("instr_str fs2",   fs2_instr(&str, &pat),              1);

    /* fs2_instr_from_str == posn_str(..., pos, 1) */
    CHECK_EQ("from 0",          fs2_instr_from_str(&str, "an", 0),  1);
    CHECK_EQ("from 2",          fs2_instr_from_str(&str, "an", 2),  3);
    CHECK_EQ("from 4",          fs2_instr_from_str(&str, "an", 4), -1);
    CHECK_EQ("from fs2 eq",     fs2_instr_from(&str, &pat, 2),      3);

    /* fs2_instr_nth_str == posn_str(..., 0, nth) */
    CHECK_EQ("nth=1",           fs2_instr_nth_str(&str, "an", 1),   1);
    CHECK_EQ("nth=2",           fs2_instr_nth_str(&str, "an", 2),   3);
    CHECK_EQ("nth=3",           fs2_instr_nth_str(&str, "an", 3),  -1);
    CHECK_EQ("nth=0",           fs2_instr_nth_str(&str, "an", 0),  -1);

    fs2_free(&str);
    fs2_free(&pat);
    assert_balanced();
}

/* =========================================================================
 * fs2_instr_from_str_ci / _nth_str_ci / _str_ci — CI
 * ========================================================================= */

Test(fs2_api_instr, instr_from_nth_str_ci_wrappers) {
    __fs2_checkreset();
    fs2  str = fs2_fromstr("BaNaNa");
    fs2  pat = fs2_fromstr("AN");

    CHECK_EQ("instr_str_ci",    fs2_instr_str_ci(&str, "an"),         1);
    CHECK_EQ("instr_str_ci fs2",fs2_instr_ci(&str, &pat),             1);

    CHECK_EQ("from_ci 0",       fs2_instr_from_str_ci(&str, "an", 0), 1);
    CHECK_EQ("from_ci 2",       fs2_instr_from_str_ci(&str, "an", 2), 3);
    CHECK_EQ("from_ci 4",       fs2_instr_from_str_ci(&str, "an", 4), -1);

    CHECK_EQ("nth_ci 1",        fs2_instr_nth_str_ci(&str, "an", 1),  1);
    CHECK_EQ("nth_ci 2",        fs2_instr_nth_str_ci(&str, "an", 2),  3);
    CHECK_EQ("nth_ci 3",        fs2_instr_nth_str_ci(&str, "an", 3), -1);

    fs2_free(&str);
    fs2_free(&pat);
    assert_balanced();
}

/* =========================================================================
 * Empty c-string pattern "" — same as NULL for _fs2_wrap? YES (both → FS2EMPTY)
 * ========================================================================= */

Test(fs2_api_instr, instr_str_empty_pattern) {
    __fs2_checkreset();
    fs2 str = fs2_fromstr("abc");

    CHECK_EQ("instr_str \"\"",       fs2_instr_str(&str, ""),       0);
    CHECK_EQ("instr_nth_str \"\" 3", fs2_instr_nth_str(&str, "", 3), 2);
    CHECK_EQ("instr_nth_str \"\" 5", fs2_instr_nth_str(&str, "", 5), -1);

    fs2_free(&str);
    assert_balanced();
}

