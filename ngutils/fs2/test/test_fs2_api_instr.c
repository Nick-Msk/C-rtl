#include <criterion/criterion.h>

#include "test_fs2.h"
#include "fs2_api.h"
#include <locale.h>

TestSuite(test_fs2_api_instr, .init = test_setup, .fini = test_teardown);

/* =========================================================================
 * fs2_instr_pos_nth — ядро, case-sensitive
 * ========================================================================= */

Test(fs2_api, instr_pos_nth_basic) {
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

Test(fs2_api, instr_pos_nth_zero) {
    __fs2_checkreset();
    fs2 h = fs2_fromstr("hello");
    fs2 p = fs2_fromstr("l");
    CHECK_EQ("nth=0 → -1", fs2_instr_pos_nth(&h, &p, 0, 0), -1);
    fs2_free(&h);
    fs2_free(&p);
    assert_balanced();
}

Test(fs2_api, instr_pos_nth_pos_skips_earlier) {
    __fs2_checkreset();
    fs2 h = fs2_fromstr("banana");   /* "an" на 1, 3 */
    fs2 p = fs2_fromstr("an");

    CHECK_EQ("pos=0 nth=1", fs2_instr_pos_nth(&h, &p, 0, 1), 1);
    CHECK_EQ("pos=0 nth=2", fs2_instr_pos_nth(&h, &p, 0, 2), 3);

    /* pos=2 → "an" на 1 отсекается */
    CHECK_EQ("pos=2 nth=1", fs2_instr_pos_nth(&h, &p, 2, 1), 3);
    CHECK_EQ("pos=2 nth=2", fs2_instr_pos_nth(&h, &p, 2, 2), -1);

    fs2_free(&h);
    fs2_free(&p);
    assert_balanced();
}

/* Перекрывающиеся вхождения считаются раздельно: "aaa"/"aa" → 0 и 1 */
Test(fs2_api, instr_pos_nth_overlapping) {
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

Test(fs2_api, instr_pos_nth_not_found_and_too_long) {
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

Test(fs2_api, instr_pos_nth_pos_at_or_beyond_len) {
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
 * fs2_instr_pos_nth — empty pattern (Python-семантика)
 * ========================================================================= */

Test(fs2_api, instr_pos_nth_empty_pattern_from_zero) {
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

Test(fs2_api, instr_pos_nth_empty_pattern_with_pos) {
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
 * fs2_instr_pos_ci_nth — ядро, case-insensitive
 * ========================================================================= */

Test(fs2_api, instr_pos_ci_nth_basic) {
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

Test(fs2_api, instr_pos_ci_nth_mixed_case) {
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

Test(fs2_api, instr_pos_ci_nth_overlapping) {
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

Test(fs2_api, instr_pos_ci_nth_nth_zero) {
    __fs2_checkreset();
    fs2 h = fs2_fromstr("HeLLo");
    fs2 p = fs2_fromstr("l");
    CHECK_EQ("nth=0", fs2_instr_pos_ci_nth(&h, &p, 0, 0), -1);
    fs2_free(&h);
    fs2_free(&p);
    assert_balanced();
}

/* Empty pattern в ci — та же Python-семантика, регистр не важен */
Test(fs2_api, instr_pos_ci_nth_empty_pattern) {
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
 * Врапперы fs2_instr / _from / _nth — case-sensitive
 * ========================================================================= */

Test(fs2_api, instr_first) {
    __fs2_checkreset();
    fs2 h = fs2_fromstr("hello world");
    fs2 p = fs2_fromstr("o");

    /* fs2_instr == fs2_instr_pos_nth(..., 0, 1) */
    CHECK_EQ("first 'o'", fs2_instr(&h, &p), 4);
    fs2_free(&h);
    fs2_free(&p);
    assert_balanced();
}

Test(fs2_api, instr_from_skips_prefix) {
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

Test(fs2_api, instr_nth_from_zero) {
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

/* Врапперы эквивалентны вызовам ядра с правильными аргументами */
Test(fs2_api, instr_wrappers_match_core) {
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
 * Врапперы fs2_instr_ci / _from_ci / _nth_ci — case-insensitive
 * ========================================================================= */

Test(fs2_api, instr_ci_first) {
    __fs2_checkreset();
    fs2 h = fs2_fromstr("Hello World");
    fs2 p = fs2_fromstr("O");

    CHECK_EQ("first 'O'", fs2_instr_ci(&h, &p), 4);
    fs2_free(&h);
    fs2_free(&p);
    assert_balanced();
}

Test(fs2_api, instr_from_ci) {
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

Test(fs2_api, instr_nth_ci) {
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
 * C-str врапперы — smoke test: эквивалентность fs2-версии
 * ========================================================================= */

Test(fs2_api, instr_str_matches_fs2_version) {
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
 * Граничные: пустой haystack / STATIC
 * ========================================================================= */

Test(fs2_api, instr_empty_haystack) {
    __fs2_checkreset();
    fs2 h = FS2EMPTY;
    fs2 p = fs2_fromstr("a");

    CHECK_EQ("nonempty pattern", fs2_instr_pos_nth(&h, &p, 0, 1), -1);

    /* empty pattern на пустом haystack: единственная позиция — 0 */
    fs2 e = FS2EMPTY;
    CHECK_EQ("nth=1", fs2_instr_pos_nth(&h, &e, 0, 1), 0);
    CHECK_EQ("nth=2", fs2_instr_pos_nth(&h, &e, 0, 2), -1);

    fs2_free(&p);
    assert_balanced();
}

Test(fs2_api, instr_static_haystack) {
    __fs2_checkreset();
    fs2 h = FS2LITERAL("hello world");
    fs2 p = fs2_fromstr("o");

    CHECK_EQ("first 'o'", fs2_instr(&h, &p), 4);
    CHECK_EQ("last 'o'",  fs2_instr_nth(&h, &p, 2), 7);

    fs2_free(&p);
    assert_balanced();
}

/* =========================================================================
 * NULL-гарды (debug: SIGABRT через invraise)
 * ========================================================================= */

Test(fs2_api, instr_null_str_aborts, .signal = SIGABRT) {
    fs2 p = fs2_fromstr("x");
    fs2_instr_pos_nth(NULL, &p, 0, 1);
    fs2_free(&p);
}

Test(fs2_api, instr_null_pattern_aborts, .signal = SIGABRT) {
    fs2 h = fs2_fromstr("hello");
    fs2_instr_pos_nth(&h, NULL, 0, 1);
    fs2_free(&h);
}

Test(fs2_api, instr_ci_null_str_aborts, .signal = SIGABRT) {
    fs2 p = fs2_fromstr("x");
    fs2_instr_pos_ci_nth(NULL, &p, 0, 1);
    fs2_free(&p);
}

Test(fs2_api, instr_ci_null_pattern_aborts, .signal = SIGABRT) {
    fs2 h = fs2_fromstr("hello");
    fs2_instr_pos_ci_nth(&h, NULL, 0, 1);
    fs2_free(&h);
}
