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

/* -------------------------------------------------------------------------
 * fs2_elem — auto-allocation, NO len change
 * ------------------------------------------------------------------------- */

Test(fs2_api, elem_existing_pos) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");
    CHECK_FS2VALID(1, &s);

    size_t sz_before = s.sz;
    char *p = fs2_elem(&s, 1);
    *p = 'X'; // overwrite existing character

    CHECK_PTR_EQ("same pointer", p, s.v + 1);
    CHECK_EQ   ("sz unchanged", s.sz, sz_before);
    CHECK_EQ   ("len unchanged", s.len, 3);
    CHECK_STR_EQ("string mutated", s.v, "aXc");
    CHECK_FS2VALID(2, &s); // Valid because len was not changed

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, elem_expands_buffer_no_len_change) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc"); /* len=3, sz=16 */
    CHECK_FS2VALID(1, &s);

    // Request for position beyond current sz
    char *p = fs2_elem(&s, 20); 

    CHECK_PTR_EQ("pointer to pos 20", p, s.v + 20);
    CHECK_TRUE("sz grown", s.sz > 20);
    CHECK_EQ  ("len NOT changed", s.len, 3); // fs2_elem does not touch len
    
    // The string is still valid: len=3 and NUL at pos 3 is intact
    CHECK_FS2VALID(2, &s); 

    fs2_free(&s);
    assert_balanced();
}

/* -------------------------------------------------------------------------
 * fs2_elem0 — auto-allocation + len update (Sequential Build)
 * ------------------------------------------------------------------------- */

Test(fs2_api, elem0_sequential_build) {
    __fs2_checkreset();
    fs2 s = FS2(); // empty
    CHECK_FS2VALID(1, &s);

    // Build "hello" character by character
    const char *word = "hello";
    for (size_t i = 0; i < 5; i++) {
        char *p = fs2_elem0(&s, i); // pos == s->len
        *p = word[i];
        CHECK_EQ("len updated", s.len, i + 1);
        CHECK_FS2VALID(2, &s);
    }

    CHECK_STR_EQ("built string", s.v, "hello");

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, elem0_overwrite_existing) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");
    CHECK_FS2VALID(1, &s);

    // Overwrite existing character (pos < len)
    char *p = fs2_elem0(&s, 1);
    *p = 'X';

    CHECK_EQ("len unchanged", s.len, 3);
    CHECK_STR_EQ("string mutated", s.v, "aXc");
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
    assert_balanced();
}

/* -------------------------------------------------------------------------
 * fs2_get — direct read-only access
 * ------------------------------------------------------------------------- */

Test(fs2_api, get_valid_indices) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello"); /* len=5, sz=16 */
    CHECK_FS2VALID(1, &s);

    // Read all characters
    CHECK_EQ("char at 0", *fs2_get(&s, 0), 'h');
    CHECK_EQ("char at 4", *fs2_get(&s, 4), 'o');
    
    // Read NUL terminator (pos == len)
    CHECK_EQ("char at len", *fs2_get(&s, 5), '\0');

    // Verify pointer arithmetic
    CHECK_PTR_EQ("pointer math", fs2_get(&s, 2), s.v + 2);

    fs2_free(&s);
    assert_balanced();
}

/* -------------------------------------------------------------------------
 * fs2_elem — streaming write scenario (the main use case)
 * ------------------------------------------------------------------------- */

Test(fs2_api, elem_streaming_write) {
    __fs2_checkreset();
    fs2 s = FS2(); /* empty ALLOC */
    CHECK_FS2VALID(1, &s);

    /* Write "streaming" character by character.
     * fs2_elem expands the buffer but does not touch len. */
    const char *data = "streaming";
    size_t len = strlen(data);
    
    for (size_t i = 0; i < len; i++) {
        char *p = fs2_elem(&s, i);
        *p = data[i];
    }

    /* len is still 0, string is formally empty even though bytes are written */
    CHECK_EQ("len unchanged during stream", s.len, 0);
    
    /* Update len once and set NUL */
    fs2_setlen(&s, len);

    /* Now it is valid and readable */
    CHECK_FS2VALID(2, &s);
    CHECK_EQ   ("len updated", s.len, 9);
    CHECK_STR_EQ("streamed string", s.v, "streaming");

    fs2_free(&s);
    assert_balanced();
}

/* -------------------------------------------------------------------------
 * fs2_reverse — in-place string reversal
 * ------------------------------------------------------------------------- */

Test(fs2_api, reverse_even_length) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abcd");
    CHECK_FS2VALID(1, &s);

    fs2 *ret = fs2_reverse(&s);
    
    CHECK_PTR_EQ("returns same pointer", ret, &s);
    CHECK_STR_EQ("reversed", s.v, "dcba");
    CHECK_EQ("len unchanged", s.len, 4);
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, reverse_odd_length) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abcde");
    CHECK_FS2VALID(1, &s);

    fs2_reverse(&s);
    
    CHECK_STR_EQ("reversed", s.v, "edcba");
    CHECK_EQ("len unchanged", s.len, 5);
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, reverse_single_char) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("a");
    CHECK_FS2VALID(1, &s);

    fs2_reverse(&s);
    
    CHECK_STR_EQ("reversed", s.v, "a");
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, reverse_empty) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("");
    CHECK_FS2VALID(1, &s);

    fs2_reverse(&s);
    
    CHECK_STR_EQ("reversed", s.v, "");
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, reverse_local) {
    __fs2_checkreset();
    FS2LOCAL(s, 16);
    memcpy(s.v, "hello", 6);
    s.len = 5;
    CHECK_FS2VALID(1, &s);

    fs2_reverse(&s);
    
    CHECK_STR_EQ("reversed", s.v, "olleh");
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
}

/* ── negatives ─────────────────────────────────────────────────────── */

Test(fs2_api, reverse_null_aborts, .signal = SIGABRT) {
    fs2_reverse(NULL);
}

Test(fs2_api, reverse_static_rejected, .signal = SIGINT) {
    fs2 s = FS2LITERAL("static");
    fs2_reverse(&s); /* userraise → SIGINT */
}


Test(fs2_api, reverse_double_invariant) {
    __fs2_checkreset();
    /* Take a string with odd length for completeness */
    fs2 s = fs2_fromstr("racecar");
    CHECK_FS2VALID(1, &s);

    /* Save original for comparison */
    fs2 original = fs2_clone(&s);
    CHECK_FS2VALID(2, &original);

    /* Reverse 1st time */
    fs2_reverse(&s);
    CHECK_STR_EQ("first reverse", s.v, "racecar"); /* palindrome unchanged */
    
    /* Take a non-palindrome for a real check */
    fs2_free(&s);
    s = fs2_fromstr("algorithms");
    fs2_free(&original);
    original = fs2_clone(&s);

    /* Reverse 1st time */
    fs2_reverse(&s);
    CHECK_STR_EQ("first reverse", s.v, "smhtirogla");
    CHECK_FS2VALID(3, &s);

    /* Reverse 2nd time (should return to original) */
    fs2_reverse(&s);
    
    /* Verify string is byte-for-byte identical to original */
    CHECK_STR_EQ("double reverse matches original", s.v, original.v);
    CHECK_EQ("len unchanged", s.len, original.len);
    CHECK_FS2VALID(4, &s);

    fs2_free(&s);
    fs2_free(&original);
    assert_balanced();
}

Test(fs2_api, reverse_roundtrip_empty_and_single) {
    __fs2_checkreset();
    
    /* Empty string */
    fs2 empty = fs2_fromstr("");
    fs2_reverse(&empty);
    fs2_reverse(&empty);
    CHECK_STR_EQ("empty stays empty", empty.v, "");
    CHECK_EQ("empty len", empty.len, 0);

    /* Single character */
    fs2 single = fs2_fromstr("X");
    fs2_reverse(&single);
    fs2_reverse(&single);
    CHECK_STR_EQ("single stays single", single.v, "X");
    CHECK_EQ("single len", single.len, 1);

    fs2_free(&empty);
    fs2_free(&single);
    assert_balanced();
}

/* -------------------------------------------------------------------------
 * fs2_sort — in-place character sort
 * ------------------------------------------------------------------------- */

Test(fs2_api, sort_ascending) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("banana");
    CHECK_FS2VALID(1, &s);

    fs2_sort(&s, true);

    CHECK_STR_EQ("sorted asc", s.v, "aaabnn");
    CHECK_EQ("len unchanged", s.len, 6);
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, sort_descending) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("banana");
    CHECK_FS2VALID(1, &s);

    fs2_sort(&s, false);

    CHECK_STR_EQ("sorted desc", s.v, "nnbaaa");
    CHECK_EQ("len unchanged", s.len, 6);
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, sort_already_sorted) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");
    fs2_sort(&s, true);
    CHECK_STR_EQ("sorted", s.v, "abc");
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, sort_with_duplicates) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("cbaabc");
    fs2_sort(&s, true);
    CHECK_STR_EQ("sorted", s.v, "aabbcc");
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, sort_empty) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("");
    CHECK_FS2VALID(1, &s);

    fs2_sort(&s, true); /* qsort with 0 elements is safe */

    CHECK_STR_EQ("still empty", s.v, "");
    CHECK_FS2VALID(2, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, sort_single_char) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("x");
    fs2_sort(&s, true);
    CHECK_STR_EQ("still x", s.v, "x");
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, sort_local) {
    __fs2_checkreset();
    FS2LOCAL(s, 16);
    memcpy(s.v, "dcba", 5);
    s.len = 4;
    CHECK_FS2VALID(1, &s);

    fs2_sort(&s, true);
    CHECK_STR_EQ("local sorted", s.v, "abcd");
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
}

/* ── negatives ─────────────────────────────────────────────────────── */

Test(fs2_api, sort_null_aborts, .signal = SIGABRT) {
    fs2_sort(NULL, true);
}

/* -------------------------------------------------------------------------
 * fs2_partialsort — in-place character sort on a sub-range [from, to)
 * ------------------------------------------------------------------------- */

Test(fs2_api, partialsort_middle) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("XdcbaY");
    CHECK_FS2VALID(1, &s);

    /* Sort only "dcba" (indices 1,2,3,4), leaving X and Y in place */
    fs2_partialsort(&s, 1, 5, true);
    CHECK_STR_EQ("sorted middle", s.v, "XabcdY");
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, partialsort_desc) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("XabcdY");

    fs2_partialsort(&s, 1, 5, false);
    CHECK_STR_EQ("sorted middle desc", s.v, "XdcbaY");
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, partialsort_full_range) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("cba");
    
    fs2_partialsort(&s, 0, 3, true);
    CHECK_STR_EQ("full range sorted", s.v, "abc");
    
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, partialsort_empty_range_valid) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("cba");
    
    /* from == to, qsort with 0 elements. This is a valid call for qsort */
    fs2_partialsort(&s, 1, 1, true);
    CHECK_STR_EQ("unchanged", s.v, "cba");
    
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, partialsort_to_clamped_to_len) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("XdcbaY");

    /* to=100 -> clamp to len=6; from=1 < 6 -> sorts "dcbaY" */
    fs2_partialsort(&s, 1, 100, true);
    CHECK_STR_EQ("clamped to end", s.v, "XYabcd");
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, partialsort_from_gt_to_noop) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");

    /* from > to -> no-op (would be SIZE_MAX in qsort without if-guard) */
    fs2_partialsort(&s, 2, 1, true);
    CHECK_STR_EQ("unchanged", s.v, "abc");
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, partialsort_from_beyond_len_noop) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");

    /* from=10, to=100 → to=3, from >= to → no-op */
    fs2_partialsort(&s, 10, 100, true);
    CHECK_STR_EQ("unchanged", s.v, "abc");

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, partialsort_canonical_empty) {
    __fs2_checkreset();
    fs2 s = FS2();   /* v=NULL, len=0, sz=0 */
    CHECK_FS2VALID(1, &s);

    /* to=100 -> clamp to 0; from=0, from < to -> false; v+from is not computed */
    fs2_partialsort(&s, 0, 100, true);
    CHECK_EQ("still empty", s.len, 0);
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
    assert_balanced();
}

/* ── negatives ─────────────────────────────────────────────────────── */

Test(fs2_api, partialsort_null_aborts, .signal = SIGABRT) {
    fs2_partialsort(NULL, 0, 1, true);
}

/* -------------------------------------------------------------------------
 * fs2_chr — first occurrence, unlimited, case-sensitive (strchr proxy)
 * ------------------------------------------------------------------------- */

Test(fs2_api, chr_found_at_start) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");
    CHECK_FS2VALID(1, &s);

    CHECK_EQ("h at 0", fs2_chr(&s, 'h'), 0);
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, chr_first_occurrence_only) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("banana");

    CHECK_EQ("b at 0",  fs2_chr(&s, 'b'), 0);
    CHECK_EQ("a at 1",  fs2_chr(&s, 'a'), 1);   /* not 3 or 5 */
    CHECK_EQ("n at 2",  fs2_chr(&s, 'n'), 2);   /* not 4 */

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, chr_found_at_end) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");
    CHECK_EQ("o at 4", fs2_chr(&s, 'o'), 4);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, chr_not_found) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    CHECK_EQ("z missing",   fs2_chr(&s, 'z'), -1);
    CHECK_EQ("1 missing",   fs2_chr(&s, '1'), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, chr_nul_never_matches) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");
    CHECK_FS2VALID(1, &s);

    /* memchr over [0, len): NUL at len is out of window, does not match */
    CHECK_EQ("NUL rejected", fs2_chr(&s, '\0'), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, chr_empty_string) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("");
    CHECK_FS2VALID(1, &s);

    CHECK_EQ("nothing in empty", fs2_chr(&s, 'a'), -1);
    CHECK_EQ("NUL not matched",  fs2_chr(&s, '\0'), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, chr_c_masked_to_unsigned_char) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("A");   /* 0x41 */

    /* strchr semantics: (unsigned char)0x141 == 'A' */
    CHECK_EQ("0x141 → 'A'", fs2_chr(&s, 0x141), 0);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, chr_negative_c) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("\xFF" "X");   /* 0xFF 'X', len=2 */
    CHECK_FS2VALID(1, &s);

    /* strchr semantics: (unsigned char)-1 == 0xFF */
    CHECK_EQ("-1 → 0xFF", fs2_chr(&s, -1), 0);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, chr_byte_ge_128) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("\xE4" "X");   /* 0xE4 'X' */
    CHECK_FS2VALID(1, &s);

    CHECK_EQ("0xE4 found", fs2_chr(&s, 0xE4), 0);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, chr_local) {
    __fs2_checkreset();
    FS2LOCALFILL(s, "%s", "world");
    CHECK_FS2VALID(1, &s);

    CHECK_EQ("w at 0", fs2_chr(&s, 'w'), 0);
    CHECK_EQ("d at 4", fs2_chr(&s, 'd'), 4);
    CHECK_EQ("x missing", fs2_chr(&s, 'x'), -1);

    fs2_free(&s);
}

/* ── negatives ─────────────────────────────────────────────────────── */

Test(fs2_api, chr_null_aborts, .signal = SIGABRT) {
    fs2_chr(NULL, 'a');
}

/* -------------------------------------------------------------------------
 * fs2_nchr — first occurrence within the first `lim` bytes (case-sensitive)
 * ------------------------------------------------------------------------- */

Test(fs2_api, nchr_within_limit) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");
    CHECK_FS2VALID(1, &s);

    CHECK_EQ("l at 2, lim=3", fs2_nchr(&s, 'l', 3), 2);
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, nchr_at_limit_boundary) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    CHECK_EQ("o at 4, lim=5", fs2_nchr(&s, 'o', 5),  4);
    CHECK_EQ("o missed, lim=4", fs2_nchr(&s, 'o', 4), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, nchr_lim_zero) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    CHECK_EQ("lim=0", fs2_nchr(&s, 'h', 0), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, nchr_lim_gt_len) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hi");

    /* loop stops at NUL, NUL never matched (unlike fs2_chr) */
    CHECK_EQ("i at 1",   fs2_nchr(&s, 'i', 100), 1);
    CHECK_EQ("NUL not matched", fs2_nchr(&s, '\0', 100), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, nchr_nul_never_matches) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    CHECK_EQ("NUL lim=5", fs2_nchr(&s, '\0', 5), -1);
    CHECK_EQ("NUL lim=100", fs2_nchr(&s, '\0', 100), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, nchr_not_found) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    CHECK_EQ("z missing", fs2_nchr(&s, 'z', 5), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, nchr_empty_string) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("");
    CHECK_FS2VALID(1, &s);

    CHECK_EQ("empty", fs2_nchr(&s, 'a', 5), -1);

    fs2_free(&s);
    assert_balanced();
}

/* ⚠ fails on current code: *p == c without masking */
Test(fs2_api, nchr_c_masked_to_unsigned_char) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("A");   /* 0x41 */

    /* (unsigned char)0x141 == 'A' — like strchr */
    CHECK_EQ("0x141 → 'A'", fs2_nchr(&s, 0x141, 1), 0);

    fs2_free(&s);
    assert_balanced();
}

/* ⚠ fails on current code: *p == -1 is never true */
Test(fs2_api, nchr_negative_c) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("\xFF" "X");
    CHECK_FS2VALID(1, &s);

    /* (unsigned char)-1 == 0xFF */
    CHECK_EQ("-1 → 0xFF", fs2_nchr(&s, -1, 2), 0);

    fs2_free(&s);
    assert_balanced();
}

/* this test passes on current code — keep as regression guard */
Test(fs2_api, nchr_byte_ge_128) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("\xE4" "X");
    CHECK_FS2VALID(1, &s);

    CHECK_EQ("0xE4 found", fs2_nchr(&s, 0xE4, 2), 0);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, nchr_local) {
    __fs2_checkreset();
    FS2LOCALFILL(s, "%s", "abcdef");
    CHECK_FS2VALID(1, &s);

    CHECK_EQ("c at 2",        fs2_nchr(&s, 'c', 6), 2);
    CHECK_EQ("c beyond lim=2", fs2_nchr(&s, 'c', 2), -1);

    fs2_free(&s);
}

/* ── negatives ─────────────────────────────────────────────────────── */

Test(fs2_api, nchr_null_aborts, .signal = SIGABRT) {
    fs2_nchr(NULL, 'a', 5);
}

/* -------------------------------------------------------------------------
 * fs2_nchr_ci — first occurrence within first `lim` bytes, case-insensitive
 * ------------------------------------------------------------------------- */

/* ── lowercase haystack: works on current code ──────────────────── */

Test(fs2_api, nichr_lowercase_haystack) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");
    CHECK_FS2VALID(1, &s);

    CHECK_EQ("h at 0", fs2_nchr_ci(&s, 'h', 5), 0);
    CHECK_EQ("H at 0", fs2_nchr_ci(&s, 'H', 5), 0);
    CHECK_EQ("L at 2", fs2_nchr_ci(&s, 'L', 5), 2);
    CHECK_EQ("O at 4", fs2_nchr_ci(&s, 'O', 5), 4);
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, nichr_first_occurrence_only) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("banana");

    CHECK_EQ("B at 0", fs2_nchr_ci(&s, 'B', 6), 0);
    CHECK_EQ("A at 1", fs2_nchr_ci(&s, 'A', 6), 1);
    CHECK_EQ("N at 2", fs2_nchr_ci(&s, 'N', 6), 2);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, nichr_limit_respected) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    CHECK_EQ("O at lim=5",  fs2_nchr_ci(&s, 'O', 5),  4);
    CHECK_EQ("O missed lim=4", fs2_nchr_ci(&s, 'O', 4), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, nichr_lim_zero) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    CHECK_EQ("lim=0", fs2_nchr_ci(&s, 'H', 0), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, nichr_nul_never_matches) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    CHECK_EQ("NUL lim=5",   fs2_nchr_ci(&s, '\0', 5), -1);
    CHECK_EQ("NUL lim=100", fs2_nchr_ci(&s, '\0', 100), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, nichr_not_found) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    CHECK_EQ("Z missing", fs2_nchr_ci(&s, 'Z', 5), -1);
    CHECK_EQ("z missing", fs2_nchr_ci(&s, 'z', 5), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, nichr_empty_string) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("");
    CHECK_FS2VALID(1, &s);

    CHECK_EQ("nothing in empty", fs2_nchr_ci(&s, 'A', 5), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, nichr_local) {
    __fs2_checkreset();
    FS2LOCALFILL(s, "%s", "abcdef");
    CHECK_FS2VALID(1, &s);

    CHECK_EQ("C at 2", fs2_nchr_ci(&s, 'C', 6), 2);
    CHECK_EQ("C beyond lim=2", fs2_nchr_ci(&s, 'C', 2), -1);

    fs2_free(&s);
}

/* ── uppercase haystack: ⚠ FAILS on current code ───────────────────── */

Test(fs2_api, nichr_uppercase_haystack) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("HELLO");
    CHECK_FS2VALID(1, &s);

    /* Symmetry: lowercase needle must find uppercase bytes */
    CHECK_EQ("h at 0", fs2_nchr_ci(&s, 'h', 5), 0);   /* FAIL: *p=='H' != lc=='h' */
    CHECK_EQ("e at 1", fs2_nchr_ci(&s, 'e', 5), 1);   /* FAIL */
    CHECK_EQ("l at 2", fs2_nchr_ci(&s, 'l', 5), 2);   /* FAIL */
    CHECK_EQ("o at 4", fs2_nchr_ci(&s, 'o', 5), 4);   /* FAIL */

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, nichr_mixed_case) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("HeLLo");
    CHECK_FS2VALID(1, &s);

    /* 'l' must match 'L' at position 2 */
    CHECK_EQ("l at 2", fs2_nchr_ci(&s, 'l', 5), 2);   /* FAIL: returns -1 */
    CHECK_EQ("L at 2", fs2_nchr_ci(&s, 'L', 5), 2);   /* FAIL: returns -1 */

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, nichr_masked_c_uppercase) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("A");   /* 0x41 */

    /* (unsigned char)0x141 == 'A' -> tolower -> 'a'; haystack 'A' must match */
    CHECK_EQ("0x141 → 'A'", fs2_nchr_ci(&s, 0x141, 1), 0);   /* FAIL */

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, nichr_byte_ge_128_masked) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("\xFF" "X");   /* 0xFF 'X' */
    CHECK_FS2VALID(1, &s);

    /* (unsigned char)-1 == 0xFF; in C locale tolower(0xFF)==0xFF — unchanged,
     * so it must match the 0xFF byte in haystack */
    CHECK_EQ("-1 → 0xFF", fs2_nchr_ci(&s, -1, 2), 0);

    fs2_free(&s);
    assert_balanced();
}

/* ── negatives ─────────────────────────────────────────────────────── */

Test(fs2_api, nichr_null_aborts, .signal = SIGABRT) {
    fs2_nchr_ci(NULL, 'a', 5);
}
/* -------------------------------------------------------------------------
 * fs2_chr_ci — first occurrence, unlimited, case-insensitive
 * ------------------------------------------------------------------------- */

Test(fs2_api, ichr_lowercase_haystack) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");
    CHECK_FS2VALID(1, &s);

    CHECK_EQ("H at 0", fs2_chr_ci(&s, 'H'), 0);
    CHECK_EQ("E at 1", fs2_chr_ci(&s, 'E'), 1);
    CHECK_EQ("O at 4", fs2_chr_ci(&s, 'O'), 4);
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
    assert_balanced();
}

/* ⚠ FAILS on current code — inherits nichr bug */
Test(fs2_api, ichr_uppercase_haystack) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("HELLO");
    CHECK_FS2VALID(1, &s);

    CHECK_EQ("h at 0", fs2_chr_ci(&s, 'h'), 0);   /* FAIL */
    CHECK_EQ("l at 2", fs2_chr_ci(&s, 'l'), 2);   /* FAIL */

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, ichr_first_occurrence_only) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("banana");

    CHECK_EQ("B at 0", fs2_chr_ci(&s, 'B'), 0);
    CHECK_EQ("A at 1", fs2_chr_ci(&s, 'A'), 1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, ichr_not_found) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    CHECK_EQ("Z missing", fs2_chr_ci(&s, 'Z'), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, ichr_empty_string) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("");
    CHECK_FS2VALID(1, &s);

    CHECK_EQ("nothing in empty", fs2_chr_ci(&s, 'a'), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, ichr_local) {
    __fs2_checkreset();
    FS2LOCALFILL(s, "%s", "hello");
    CHECK_FS2VALID(1, &s);

    CHECK_EQ("H at 0", fs2_chr_ci(&s, 'H'), 0);
    CHECK_EQ("X missing", fs2_chr_ci(&s, 'X'), -1);

    fs2_free(&s);
}

/* ── negatives ─────────────────────────────────────────────────────── */

Test(fs2_api, ichr_null_aborts, .signal = SIGABRT) {
    fs2_chr_ci(NULL, 'a');
}

/* -------------------------------------------------------------------------
 * fs2_rchr — last occurrence, unlimited, case-sensitive
 * ------------------------------------------------------------------------- */

Test(fs2_api, rchr_found_at_start) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");
    CHECK_FS2VALID(1, &s);

    CHECK_EQ("h at 0", fs2_rchr(&s, 'h'), 0);
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, rchr_found_at_end) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    CHECK_EQ("o at 4", fs2_rchr(&s, 'o'), 4);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, rchr_found_in_middle) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");
    CHECK_FS2VALID(1, &s);

    CHECK_EQ("last l at 3", fs2_rchr(&s, 'l'), 3);
    CHECK_EQ("e at 1",      fs2_rchr(&s, 'e'), 1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, rchr_returns_last_not_first) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("banana");
    CHECK_FS2VALID(1, &s);

    /* Contrast with fs2_chr: same needle, different ends */
    CHECK_EQ("chr  'a' first at 1", fs2_chr (&s, 'a'), 1);
    CHECK_EQ("rchr 'a' last  at 5", fs2_rchr(&s, 'a'), 5);
    CHECK_EQ("chr  'n' first at 2", fs2_chr (&s, 'n'), 2);
    CHECK_EQ("rchr 'n' last  at 4", fs2_rchr(&s, 'n'), 4);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, rchr_single_char_match) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("x");
    CHECK_FS2VALID(1, &s);

    CHECK_EQ("x at 0",    fs2_rchr(&s, 'x'), 0);
    CHECK_EQ("y missing", fs2_rchr(&s, 'y'), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, rchr_nul_never_matches) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");
    CHECK_FS2VALID(1, &s);

    /* NUL is outside [0, len); neither forward nor reverse search sees it */
    CHECK_EQ("chr  NUL missing", fs2_chr (&s, '\0'), -1);
    CHECK_EQ("rchr NUL missing", fs2_rchr(&s, '\0'), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, rchr_not_found) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    CHECK_EQ("z missing", fs2_rchr(&s, 'z'), -1);
    CHECK_EQ("1 missing", fs2_rchr(&s, '1'), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, rchr_empty_string) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("");
    CHECK_FS2VALID(1, &s);

    CHECK_EQ("nothing in empty", fs2_rchr(&s, 'a'), -1);
    CHECK_EQ("NUL not matched",  fs2_rchr(&s, '\0'), -1);

    fs2_free(&s);
    assert_balanced();
}

/* ⚠ FAILS on current code: comparing with c, not uc */
Test(fs2_api, rchr_c_masked_to_unsigned_char) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("A");   /* 0x41 */

    /* (unsigned char)0x141 == 'A' */
    CHECK_EQ("0x141 → 'A'", fs2_rchr(&s, 0x141), 0);   /* FAIL */

    fs2_free(&s);
    assert_balanced();
}

/* ⚠ FAILS on current code: p[pos] == 0xFF != -1 */
Test(fs2_api, rchr_negative_c) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("\xFF" "X");
    CHECK_FS2VALID(1, &s);

    CHECK_EQ("-1 → 0xFF", fs2_rchr(&s, -1), 0);   /* FAIL */

    fs2_free(&s);
    assert_balanced();
}

/* passes now — keep as regression guard */
Test(fs2_api, rchr_byte_ge_128) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("\xE4" "X");
    CHECK_FS2VALID(1, &s);

    CHECK_EQ("0xE4 found", fs2_rchr(&s, 0xE4), 0);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, rchr_local) {
    __fs2_checkreset();
    FS2LOCALFILL(s, "%s", "hello");
    CHECK_FS2VALID(1, &s);

    CHECK_EQ("last l at 3", fs2_rchr(&s, 'l'), 3);
    CHECK_EQ("h at 0",       fs2_rchr(&s, 'h'), 0);
    CHECK_EQ("x missing",    fs2_rchr(&s, 'x'), -1);

    fs2_free(&s);
}

/* ── negatives ─────────────────────────────────────────────────────── */

Test(fs2_api, rchr_null_aborts, .signal = SIGABRT) {
    fs2_rchr(NULL, 'a');
}

/* -------------------------------------------------------------------------
 * fs2_rchr_ci — last occurrence, unlimited, case-insensitive
 *
 * Logic is already correct (tolower on both sides), no failing tests.
 * Below — regression suite to ensure the `p` -> `p[pos]` fix does not
 * break behaviour and to pin down symmetry between irchr vs nichr.
 * ------------------------------------------------------------------------- */

Test(fs2_api, irchr_lowercase_haystack) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");
    CHECK_FS2VALID(1, &s);

    CHECK_EQ("H at 0", fs2_rchr_ci(&s, 'H'), 0);
    CHECK_EQ("E at 1", fs2_rchr_ci(&s, 'E'), 1);
    CHECK_EQ("L at 3", fs2_rchr_ci(&s, 'L'), 3);   /* last L */
    CHECK_EQ("O at 4", fs2_rchr_ci(&s, 'O'), 4);
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, irchr_uppercase_haystack) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("HELLO");
    CHECK_FS2VALID(1, &s);

    /* Symmetry — unlike nichr/ichr (pre-fix) this works */
    CHECK_EQ("h at 0", fs2_rchr_ci(&s, 'h'), 0);
    CHECK_EQ("e at 1", fs2_rchr_ci(&s, 'e'), 1);
    CHECK_EQ("l at 3", fs2_rchr_ci(&s, 'l'), 3);
    CHECK_EQ("o at 4", fs2_rchr_ci(&s, 'o'), 4);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, irchr_mixed_case) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("HeLLo");

    CHECK_EQ("L at 3", fs2_rchr_ci(&s, 'L'), 3);
    CHECK_EQ("l at 3", fs2_rchr_ci(&s, 'l'), 3);
    CHECK_EQ("O at 4", fs2_rchr_ci(&s, 'o'), 4);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, irchr_returns_last_not_first) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("BaNaNa");

    CHECK_EQ("A last at 5", fs2_rchr_ci(&s, 'a'), 5);
    CHECK_EQ("N last at 4", fs2_rchr_ci(&s, 'n'), 4);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, irchr_nul_never_matches) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    CHECK_EQ("NUL missing", fs2_rchr_ci(&s, '\0'), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, irchr_not_found) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    CHECK_EQ("Z missing", fs2_rchr_ci(&s, 'Z'), -1);
    CHECK_EQ("z missing", fs2_rchr_ci(&s, 'z'), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, irchr_empty_string) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("");
    CHECK_FS2VALID(1, &s);

    CHECK_EQ("nothing in empty", fs2_rchr_ci(&s, 'a'), -1);

    fs2_free(&s);
    assert_balanced();
}

/* passes now: lc is already masked, tolower on haystack — same */
Test(fs2_api, irchr_c_masked_to_unsigned_char) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("A");   /* 0x41 */

    /* tolower((unsigned char)0x141) == tolower(0x41) == 'a' */
    CHECK_EQ("0x141 → 'A'", fs2_rchr_ci(&s, 0x141), 0);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, irchr_negative_c) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("\xFF" "X");
    CHECK_FS2VALID(1, &s);

    /* (unsigned char)-1 == 0xFF; in C locale tolower(0xFF)==0xFF */
    CHECK_EQ("-1 → 0xFF", fs2_rchr_ci(&s, -1), 0);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, irchr_local) {
    __fs2_checkreset();
    FS2LOCALFILL(s, "%s", "hello");
    CHECK_FS2VALID(1, &s);

    CHECK_EQ("L at 3", fs2_rchr_ci(&s, 'L'), 3);
    CHECK_EQ("X missing", fs2_rchr_ci(&s, 'X'), -1);

    fs2_free(&s);
}

Test(fs2_api, irchr_local_uppercase) {
    __fs2_checkreset();
    FS2LOCALFILL(s, "%s", "HELLO");
    CHECK_FS2VALID(1, &s);

    CHECK_EQ("l at 3", fs2_rchr_ci(&s, 'l'), 3);
    CHECK_EQ("h at 0", fs2_rchr_ci(&s, 'h'), 0);

    fs2_free(&s);
}

/* ── negatives ─────────────────────────────────────────────────────── */

Test(fs2_api, irchr_null_aborts, .signal = SIGABRT) {
    fs2_rchr_ci(NULL, 'a');
}

Test(fs2_api, chr_vs_rchr_hello_l) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");   /* idx: h0 e1 l2 l3 o4 */
    CHECK_FS2VALID(1, &s);

    /* 'l' occurs twice: at 2 and 3.
     * Forward search — first occurrence, reverse — last. */
    CHECK_EQ("chr  'l' → 2", fs2_chr (&s, 'l'), 2);
    CHECK_EQ("rchr 'l' → 3", fs2_rchr(&s, 'l'), 3);

    /* This is the difference between forward and reverse search —
     * on the same haystack, same needle. */
    CHECK_TRUE("positions differ",
               fs2_chr(&s, 'l') != fs2_rchr(&s, 'l'));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, chr_eq_rchr_single_occurrence) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    /* 'h', 'e', 'o' — unique. chr and rchr must agree. */
    CHECK_EQ("h  chr",  fs2_chr (&s, 'h'), 0);
    CHECK_EQ("h rchr",  fs2_rchr(&s, 'h'), 0);

    CHECK_EQ("e  chr",  fs2_chr (&s, 'e'), 1);
    CHECK_EQ("e rchr",  fs2_rchr(&s, 'e'), 1);

    CHECK_EQ("o  chr",  fs2_chr (&s, 'o'), 4);
    CHECK_EQ("o rchr",  fs2_rchr(&s, 'o'), 4);

    fs2_free(&s);
    assert_balanced();
}

// inv test
Test(fs2_api, nul_rejected_by_all_finders) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");
    CHECK_FS2VALID(1, &s);

    CHECK_EQ("chr",    fs2_chr   (&s, '\0'), -1);
    CHECK_EQ("nchr",   fs2_nchr  (&s, '\0', 5), -1);
    CHECK_EQ("ichr",   fs2_chr_ci  (&s, '\0'), -1);
    CHECK_EQ("nichr",  fs2_nchr_ci (&s, '\0', 5), -1);
    CHECK_EQ("rchr",   fs2_rchr  (&s, '\0'), -1);
    CHECK_EQ("nrchr",  fs2_nrchr (&s, '\0', 5), -1);
    CHECK_EQ("irchr",  fs2_rchr_ci (&s, '\0'), -1);
    CHECK_EQ("nirchr", fs2_nrchr_ci(&s, '\0', 5), -1);

    fs2_free(&s);
    assert_balanced();
}

/* -------------------------------------------------------------------------
 * fs2_chr N-th occurence series
 * ------------------------------------------------------------------------- */

Test(fs2_api, chrn_return_type_is_ptrdiff_t) {
    /* Compile-time: return type mismatch — constraint violation
     * (warning/error) even if sizeof matches. */
    ptrdiff_t (*f_chr_nth )(const fs2 *, int, size_t)        = fs2_chr_nth;
    ptrdiff_t (*f_nchr_nth)(const fs2 *, int, size_t, size_t) = fs2_nchr_nth;
    (void)f_chr_nth;
    (void)f_nchr_nth;

    /* Runtime: -1 is signed, not SIZE_MAX */
    __fs2_checkreset();
    fs2 s = fs2_fromstr("x");
    ptrdiff_t r = fs2_chr_nth(&s, 'z', 1);
    cr_assert(r < 0, "not-found must be negative, got %td", r);
    cr_assert(r == -1, "not-found must be exactly -1, got %td", r);
    fs2_free(&s);
    assert_balanced();
}

_Static_assert(
        _Generic(&fs2_chr_nth,
                 ptrdiff_t (*)(const fs2 *, int, size_t): 1,
                 default: 0) == 1,
        "fs2_chr_nth must return ptrdiff_t");

Test(fs2_api, chrn_k1_equals_chr) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("banana");   /* b0 a1 n2 a3 n4 a5 */
    CHECK_FS2VALID(1, &s);

    CHECK_EQ("chr_nth  k=1 == chr",
             fs2_chr_nth (&s, 'a', 1),    fs2_chr (&s, 'a'));
    CHECK_EQ("nchr_nth k=1 lim=6 == nchr",
             fs2_nchr_nth(&s, 'a', 1, 6), fs2_nchr(&s, 'a', 6));
    CHECK_EQ("nchr_nth k=1 lim=3 == nchr lim=3",
             fs2_nchr_nth(&s, 'a', 1, 3), fs2_nchr(&s, 'a', 3));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, chrn_last_k_equals_rchr) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("banana");   /* three 'a's: 1, 3, 5 */

    /* Last (3rd) occurrence in forward == first in reverse */
    CHECK_EQ("k=3 fwd == 1st rev",
             fs2_chr_nth(&s, 'a', 3), fs2_rchr(&s, 'a'));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, chrn_forward_sequence) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("banana");   /* b0 a1 n2 a3 n4 a5 */
    CHECK_FS2VALID(1, &s);

    CHECK_EQ("k=1 → 1", fs2_chr_nth(&s, 'a', 1), 1);
    CHECK_EQ("k=2 → 3", fs2_chr_nth(&s, 'a', 2), 3);
    CHECK_EQ("k=3 → 5", fs2_chr_nth(&s, 'a', 3), 5);
    CHECK_EQ("k=4 → -1", fs2_chr_nth(&s, 'a', 4), -1);

    /* Single occurrences: k=2 is already -1 */
    CHECK_EQ("'n' k=1 → 2", fs2_chr_nth(&s, 'n', 1), 2);
    CHECK_EQ("'n' k=2 → 4", fs2_chr_nth(&s, 'n', 2), 4);
    CHECK_EQ("'n' k=3 → -1", fs2_chr_nth(&s, 'n', 3), -1);
    CHECK_EQ("'b' k=1 → 0", fs2_chr_nth(&s, 'b', 1), 0);
    CHECK_EQ("'b' k=2 → -1", fs2_chr_nth(&s, 'b', 2), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, chrn_k_zero_rejected) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    /* k is 1-based: zero is invalid, not "first occurrence" */
    CHECK_EQ("k=0 → -1", fs2_chr_nth(&s, 'l', 0), -1);
    CHECK_EQ("nchr k=0 → -1", fs2_nchr_nth(&s, 'l', 0, 5), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, chrn_k_exceeds_matches) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");   /* one 'b' */

    CHECK_EQ("k=2 → -1", fs2_chr_nth(&s, 'b', 2), -1);
    CHECK_EQ("k=100 → -1", fs2_chr_nth(&s, 'b', 100), -1);

    /* k = SIZE_MAX: finishes quickly, does not hang */
    CHECK_EQ("k=SIZE_MAX → -1", fs2_chr_nth(&s, 'b', SIZE_MAX), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, chrn_empty_string) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("");
    CHECK_FS2VALID(1, &s);

    CHECK_EQ("k=1 → -1", fs2_chr_nth(&s, 'a', 1), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, nchrn_lim_zero) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    /* empty window — no occurrence */
    CHECK_EQ("lim=0", fs2_nchr_nth(&s, 'h', 1, 0), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, nchrn_lim_lt_len) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("banana");   /* 'a' at 1, 3, 5 */

    /* lim=4 -> window [0,4) = "bana"; third 'a' is absent */
    CHECK_EQ("k=1 lim=4", fs2_nchr_nth(&s, 'a', 1, 4), 1);
    CHECK_EQ("k=2 lim=4", fs2_nchr_nth(&s, 'a', 2, 4), 3);
    CHECK_EQ("k=3 lim=4", fs2_nchr_nth(&s, 'a', 3, 4), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, nchrn_lim_ge_len) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("banana");

    /* lim >= len -> behaves like chr_nth */
    CHECK_EQ("lim=len",    fs2_nchr_nth(&s, 'a', 3, 6),
                           fs2_chr_nth(&s, 'a', 3));
    CHECK_EQ("lim>len",    fs2_nchr_nth(&s, 'a', 3, 100),
                           fs2_chr_nth(&s, 'a', 3));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, chrn_match_at_last_position) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abcabc");   /* c at 2 and 5 */
    CHECK_FS2VALID(1, &s);

    CHECK_EQ("k=1 'c' → 2", fs2_chr_nth(&s, 'c', 1), 2);
    CHECK_EQ("k=2 'c' → 5", fs2_chr_nth(&s, 'c', 2), 5);   /* last byte */

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, nchrn_match_at_window_end) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("XabcX");   /* 'c' at 3 */

    /* lim=4 -> window [0,3] = "Xabc"; 'c' is the last byte of window */
    CHECK_EQ("k=1 lim=4", fs2_nchr_nth(&s, 'c', 1, 4), 3);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, chrn_k1_of_k_on_last) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("aaa");   /* three 'a's: 0, 1, 2 */

    CHECK_EQ("k=3 → 2 (last byte)", fs2_chr_nth(&s, 'a', 3), 2);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, chrn_large_offsets) {
    __fs2_checkreset();

    const size_t n = 100 * 1024;          /* 100 KB */
    fs2 s = fs2_init(n + 2);
    memset(s.v, 'a', n);
    s.v[n]     = 'b';                      /* last significant byte */
    s.v[n + 1] = '\0';
    s.len      = n + 1;
    CHECK_FS2VALID(1, &s);

    /* Position ~100000 — definitely does not fit in int16/int8 */
    CHECK_EQ("'b' at end", fs2_chr_nth(&s, 'b', 1), (ptrdiff_t)n);
    CHECK_EQ("1st 'a'",    fs2_chr_nth(&s, 'a', 1), 0);
    CHECK_EQ("n-th 'a'",   fs2_chr_nth(&s, 'a', (size_t)n), (ptrdiff_t)(n - 1));
    CHECK_EQ("(n+1)-th 'a'", fs2_chr_nth(&s, 'a', (size_t)n + 1), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, chrn_nul_rejected) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");
    CHECK_FS2VALID(1, &s);

    CHECK_EQ("chr_nth  k=1", fs2_chr_nth (&s, '\0', 1), -1);
    CHECK_EQ("chr_nth  k=5", fs2_chr_nth (&s, '\0', 5), -1);
    CHECK_EQ("nchr_nth lim=100", fs2_nchr_nth(&s, '\0', 1, 100), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, nrchrn_k1_equals_rchr) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("banana");

    CHECK_EQ("k=1 == rchr",
             fs2_rchr_nth (&s, 'a', 1),    fs2_rchr (&s, 'a'));
    CHECK_EQ("k=1 lim=4 == nrchr",
             fs2_nrchr_nth(&s, 'a', 1, 4), fs2_nrchr(&s, 'a', 4));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, nrchrn_reverse_sequence) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("banana");   /* 'a' at 1, 3, 5 */

    CHECK_EQ("k=1 (last)  → 5", fs2_rchr_nth(&s, 'a', 1), 5);
    CHECK_EQ("k=2          → 3", fs2_rchr_nth(&s, 'a', 2), 3);
    CHECK_EQ("k=3 (first)  → 1", fs2_rchr_nth(&s, 'a', 3), 1);
    CHECK_EQ("k=4          → -1", fs2_rchr_nth(&s, 'a', 4), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, nrchrn_forward_reverse_symmetry) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("banana");   /* three 'a's */

    /* i-th forward + (count-i+1)-th reverse point to the same position */
    for (size_t i = 1; i <= 3; i++) {
        cr_assert_eq(fs2_chr_nth(&s, 'a', i),
                     fs2_rchr_nth(&s, 'a', 4 - i),
                     "fwd(%zu) != rev(%zu)", i, 4 - i);
    }

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, chrn_null_aborts, .signal = SIGABRT) {
    fs2_chr_nth(NULL, 'a', 1);
}

Test(fs2_api, nchrn_null_aborts, .signal = SIGABRT) {
    fs2_nchr_nth(NULL, 'a', 1, 5);
}

Test(fs2_api, chrn_symmetry_sensitive) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("banana");   /* 'a' at 1, 3, 5 — total=3 */
    CHECK_FS2VALID(1, &s);

    for (size_t k = 1; k <= 3; k++) {
        CHECK_EQ("chr_nth(k) == rchr_nth(total+1-k)",
                 fs2_chr_nth (&s, 'a', k),
                 fs2_rchr_nth(&s, 'a', 4 - k));
        CHECK_EQ("nchr_nth(k) == nrchr_nth(total+1-k)",
                 fs2_nchr_nth(&s, 'a', k, 100),
                 fs2_nrchr_nth(&s, 'a', 4 - k, 100));
    }

    fs2_free(&s);
    assert_balanced();
}
// --------------------- invariants checkings ------------------
// forward <-> reverse symmetry
Test(fs2_api, chrn_symmetry_insensitive) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("BaNaNa");   /* mixed case, same positions */
    CHECK_FS2VALID(1, &s);

    for (size_t k = 1; k <= 3; k++) {
        CHECK_EQ("ichr_nth(k) == irchr_nth(total+1-k)",
                 fs2_chr_ci_nth (&s, 'A', k),
                 fs2_rchr_ci_nth(&s, 'A', 4 - k));
        CHECK_EQ("nichr_nth(k) == nirchr_nth(total+1-k)",
                 fs2_nchr_ci_nth(&s, 'A', k, 100),
                 fs2_nrchr_ci_nth(&s, 'A', 4 - k, 100));
    }

    fs2_free(&s);
    assert_balanced();
}
// upper  case-sensitivity on uppercase
Test(fs2_api, chrn_insensitive_uppercase_haystack) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("BANANA");
    CHECK_FS2VALID(1, &s);

    /* Forward + insensitive: finds lowercase needle in uppercase haystack */
    CHECK_EQ("ichr_nth   k=1 → 1", fs2_chr_ci_nth  (&s, 'a', 1), 1);
    CHECK_EQ("ichr_nth   k=2 → 3", fs2_chr_ci_nth  (&s, 'a', 2), 3);
    CHECK_EQ("ichr_nth   k=3 → 5", fs2_chr_ci_nth  (&s, 'a', 3), 5);
    CHECK_EQ("nichr_nth  k=2 → 3", fs2_nchr_ci_nth (&s, 'a', 2, 100), 3);

    /* Reverse + insensitive: mirror */
    CHECK_EQ("irchr_nth  k=1 → 5", fs2_rchr_ci_nth (&s, 'a', 1), 5);
    CHECK_EQ("irchr_nth  k=3 → 1", fs2_rchr_ci_nth (&s, 'a', 3), 1);
    CHECK_EQ("nirchr_nth k=2 → 3", fs2_nrchr_ci_nth(&s, 'a', 2, 100), 3);

    fs2_free(&s);
    assert_balanced();
}
// consecutive matches — specific to reverse
Test(fs2_api, rchr_nth_consecutive) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("aa");   /* 'a' at 0 and 1 — adjacent */
    CHECK_FS2VALID(1, &s);

    /* If someone wrote `end = hit - 1` instead of `end = hit`,
     * k=2 would return -1 instead of 0 — the second 'a' is lost. */
    CHECK_EQ("k=1 → 1", fs2_rchr_nth (&s, 'a', 1), 1);
    CHECK_EQ("k=2 → 0", fs2_rchr_nth (&s, 'a', 2), 0);
    CHECK_EQ("k=3 → -1", fs2_rchr_nth(&s, 'a', 3), -1);

    CHECK_EQ("irchr k=2 → 0", fs2_rchr_ci_nth(&s, 'A', 2), 0);
    CHECK_EQ("nrchr k=2 → 0", fs2_nrchr_nth(&s, 'a', 2, 100), 0);

    fs2_free(&s);
    assert_balanced();
}
// lim + reverse — off + hit arithmetic
Test(fs2_api, nrchr_nth_lim_offset) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("banana");   /* len=6, 'a' at 1, 3, 5 */
    CHECK_FS2VALID(1, &s);

    /* lim=4 -> window [2, 6) = "nana"; 'a' at 3 and 5 (in orig coords) */
    /* If someone forgot `+ off`, we would get 1 and 3 instead of 5 and 3. */
    CHECK_EQ("k=1 lim=4 → 5", fs2_nrchr_nth(&s, 'a', 1, 4), 5);
    CHECK_EQ("k=2 lim=4 → 3", fs2_nrchr_nth(&s, 'a', 2, 4), 3);
    CHECK_EQ("k=3 lim=4 → -1", fs2_nrchr_nth(&s, 'a', 3, 4), -1);

    /* lim=1 -> window [5, 6) = "a"; only position 5 */
    CHECK_EQ("k=1 lim=1 → 5", fs2_nrchr_ci_nth(&s, 'A', 1, 1), 5);
    CHECK_EQ("k=2 lim=1 → -1", fs2_nrchr_ci_nth(&s, 'A', 2, 1), -1);

    fs2_free(&s);
    assert_balanced();
}
// all 8
Test(fs2_api, chrn_all_variants_banana_k2) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("banana");   /* 'a' at 1, 3, 5 — k=2 -> middle at 3 */
    CHECK_FS2VALID(1, &s);

    /* k=2 — the only occurrence where forward and reverse agree */
    CHECK_EQ("chr_nth   k=2", fs2_chr_nth   (&s, 'a', 2),     3);
    CHECK_EQ("nchr_nth  k=2", fs2_nchr_nth  (&s, 'a', 2, 10), 3);
    CHECK_EQ("ichr_nth  k=2", fs2_chr_ci_nth  (&s, 'a', 2),     3);
    CHECK_EQ("nichr_nth k=2", fs2_nchr_ci_nth (&s, 'a', 2, 10), 3);
    CHECK_EQ("rchr_nth  k=2", fs2_rchr_nth  (&s, 'a', 2),     3);
    CHECK_EQ("nrchr_nth k=2", fs2_nrchr_nth (&s, 'a', 2, 10), 3);
    CHECK_EQ("irchr_nth k=2", fs2_rchr_ci_nth (&s, 'a', 2),     3);
    CHECK_EQ("nirchr_nth k=2", fs2_nrchr_ci_nth(&s, 'a', 2, 10), 3);
    CHECK_EQ("irchr_nth k=0", fs2_rchr_ci_nth(&s, 'a', 0), -1);
    CHECK_EQ("nirchr_nth k=0", fs2_nrchr_ci_nth(&s, 'a', 0, 10), -1);

    fs2_free(&s);
    assert_balanced();
}
// null
Test(fs2_api, ichr_nth_null,   .signal = SIGABRT) { fs2_chr_ci_nth  (NULL, 'a', 1);    }
Test(fs2_api, nichr_nth_null,  .signal = SIGABRT) { fs2_nchr_ci_nth (NULL, 'a', 1, 10);}
Test(fs2_api, rchr_nth_null,   .signal = SIGABRT) { fs2_rchr_nth  (NULL, 'a', 1);    }
Test(fs2_api, nrchr_nth_null,  .signal = SIGABRT) { fs2_nrchr_nth (NULL, 'a', 1, 10);}
Test(fs2_api, irchr_nth_null,  .signal = SIGABRT) { fs2_rchr_ci_nth (NULL, 'a', 1);    }
Test(fs2_api, nirchr_nth_null, .signal = SIGABRT) { fs2_nrchr_ci_nth(NULL, 'a', 1, 10);}

/* ─── fs2_wrap ────────────────────────────────────────────────────── */

Test(fs2_api, wrap_normal) {
    fs2 w = fs2_wrap("hello");
    CHECK_EQ("len", w.len, 5);
    CHECK_EQ("sz",  w.sz, 6);
    CHECK_TRUE("STATIC", fs2_is_static(&w));
    CHECK_STR_EQ("v", w.v, "hello");
    CHECK_FS2VALID(1, &w);
}

Test(fs2_api, wrap_empty_string) {
    fs2 w = fs2_wrap("");
    CHECK_EQ("len", w.len, 0);
    CHECK_EQ("sz",  w.sz, 1);
    CHECK_TRUE("STATIC", fs2_is_static(&w));
    CHECK_NOT_NULL("v", w.v);
    CHECK_FS2VALID(1, &w);
}

Test(fs2_api, wrap_null_returns_empty_view) {
    fs2 w = fs2_wrap(NULL);
    CHECK_EQ("len", w.len, 0);
    CHECK_EQ("sz",  w.sz, 1);          /* FS2_EMPTY, not {0,0} */
    CHECK_TRUE("STATIC", fs2_is_static(&w));
    CHECK_NOT_NULL("v non-NULL", w.v);
    CHECK_STR_EQ("v empty", w.v, "");
    CHECK_FS2VALID(1, &w);             /* ← passes */
}

/* ─── fs2_cat ──────────────────────────────────────────────────── */

Test(fs2_api, cat_basic) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("hello");
    fs2 src = fs2_fromstr(" world");
    CHECK_FS2VALID(1, &dst);
    CHECK_FS2VALID(2, &src);

    fs2 *ret = fs2_cat(&dst, &src);

    CHECK_PTR_EQ("returns dst", ret, &dst);
    CHECK_STR_EQ("content", dst.v, "hello world");
    CHECK_EQ("len", dst.len, 11);
    CHECK_FS2VALID(3, &dst);
    CHECK_FS2VALID(4, &src);   /* src untouched */

    fs2_free(&dst);
    fs2_free(&src);
    assert_balanced();
}

Test(fs2_api, cat_static_src) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("hello");
    fs2 src = FS2LITERAL(" world");

    fs2_cat(&dst, &src);
    CHECK_STR_EQ("appended", dst.v, "hello world");
    CHECK_FS2VALID(2, &dst);
    CHECK_FS2VALID(3, &src);   /* STATIC does not change */

    fs2_free(&dst);            /* src — STATIC, free no-op */
    assert_balanced();
}

Test(fs2_api, cat_empty_src) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("hello");
    fs2 src = fs2_fromstr("");

    fs2_cat(&dst, &src);
    CHECK_STR_EQ("unchanged", dst.v, "hello");
    CHECK_EQ("len", dst.len, 5);
    CHECK_FS2VALID(2, &dst);

    fs2_free(&dst);
    fs2_free(&src);
    assert_balanced();
}

Test(fs2_api, cat_empty_dst) {
    __fs2_checkreset();
    fs2 dst = FS2();                    /* canonical empty ALLOC */
    CHECK_FS2VALID(1, &dst);

    fs2 src = fs2_fromstr("hello");
    fs2_cat(&dst, &src);

    CHECK_STR_EQ("appended to empty", dst.v, "hello");
    CHECK_EQ("len", dst.len, 5);
    CHECK_FS2VALID(2, &dst);

    fs2_free(&dst);
    fs2_free(&src);
    assert_balanced();
}


/* ─── fs2_catstr ──────────────────────────────────────────────────── */

Test(fs2_api, catstr_null_src_is_noop) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");
    CHECK_FS2VALID(1, &s);

    fs2_catstr(&s, NULL);

    CHECK_STR_EQ("unchanged", s.v, "hello");
    CHECK_EQ("len", s.len, 5);
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, catstr_via_wrap_no_copy) {
    __fs2_checkreset();
    fs2 s = FS2();
    fs2_catstr(&s, "hello");
    /* Verify the intermediate fs2 did not allocate:
     * one alloc/free on dst, zero on wrap. */
    CHECK_STR_EQ("appended", s.v, "hello");
    CHECK_FS2VALID(2, &s);
    fs2_free(&s);
    assert_balanced();   /* ← would catch a leak from the fs2_fromstr variant */
}

Test(fs2_api, catstr_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");
    CHECK_FS2VALID(1, &s);

    fs2 *ret = fs2_catstr(&s, " world");

    CHECK_PTR_EQ("returns dst", ret, &s);
    CHECK_STR_EQ("content", s.v, "hello world");
    CHECK_EQ("len", s.len, 11);
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, catstr_empty_src) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    /* FS2WRAP("") yields valid {len=0, sz=1, STATIC, v=""} */
    fs2_catstr(&s, "");
    CHECK_STR_EQ("unchanged", s.v, "hello");
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, catstr_empty_dst) {
    __fs2_checkreset();
    fs2 s = FS2();
    CHECK_FS2VALID(1, &s);

    fs2_catstr(&s, "hello");
    CHECK_STR_EQ("appended", s.v, "hello");
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, catstr_chained) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("a");

    fs2_catstr(fs2_catstr(fs2_catstr(&s, "b"), "c"), "d");

    CHECK_STR_EQ("chained", s.v, "abcd");
    CHECK_EQ("len", s.len, 4);
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, catstr_triggers_growth) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("x");
    size_t orig_sz = s.sz;

    char big[1024];
    memset(big, 'A', sizeof(big) - 1);
    big[sizeof(big) - 1] = '\0';

    fs2_catstr(&s, big);

    CHECK_TRUE("buffer grew", s.sz > orig_sz);
    CHECK_EQ("len", s.len, 1 + (sizeof(big) - 1));
    CHECK_EQ("first char", s.v[0], 'x');
    for (size_t i = 1; i < s.len; i++)
        CHECK_EQ("expected 'A'", s.v[i], 'A');
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, catstr_multiple_append) {
    __fs2_checkreset();
    fs2 s = FS2();

    fs2_catstr(&s, "one");
    fs2_catstr(&s, " ");
    fs2_catstr(&s, "two");
    fs2_catstr(&s, " ");
    fs2_catstr(&s, "three");

    CHECK_STR_EQ("all appended", s.v, "one two three");
    CHECK_EQ("len", s.len, 13);
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
    assert_balanced();
}

/* -------------------------------------------------------------------------
 * fs2_rev_catstr — prepend string (reverse concatenation)
 * ------------------------------------------------------------------------- */

Test(fs2_api, rev_catstr_basic) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("world");
    CHECK_FS2VALID(1, &dst);

    fs2 *ret = fs2_rev_catstr(&dst, "hello ");

    CHECK_PTR_EQ("returns dst", ret, &dst);
    CHECK_STR_EQ("content", dst.v, "hello world");
    CHECK_EQ("len", dst.len, 11);
    CHECK_FS2VALID(2, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2_api, rev_catstr_empty_dst) {
    __fs2_checkreset();
    fs2 dst = FS2();   /* canonical empty */
    CHECK_FS2VALID(1, &dst);

    fs2_rev_catstr(&dst, "hello");

    CHECK_STR_EQ("prepended to empty", dst.v, "hello");
    CHECK_EQ("len", dst.len, 5);
    CHECK_FS2VALID(2, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2_api, rev_catstr_empty_src) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("world");
    size_t orig_sz = dst.sz;

    fs2_rev_catstr(&dst, "");

    CHECK_STR_EQ("unchanged", dst.v, "world");
    CHECK_EQ("len unchanged", dst.len, 5);
    CHECK_EQ("sz unchanged (early return)", dst.sz, orig_sz);
    CHECK_FS2VALID(2, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2_api, rev_catstr_null_src_is_noop) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("world");

    fs2_rev_catstr(&dst, NULL);

    CHECK_STR_EQ("unchanged", dst.v, "world");
    CHECK_EQ("len unchanged", dst.len, 5);
    CHECK_FS2VALID(2, &dst);

    fs2_free(&dst);
    assert_balanced();
}

/* Regression for memcpy vs memmove bug:
 * if memcpy is used instead of memmove, the old string gets overwritten. */
Test(fs2_api, rev_catstr_shifts_correctly) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("world");

    fs2_rev_catstr(&dst, "hello ");

    /* If memcpy(dst->v, "hello ", 6) was called, "world" gets clobbered,
     * and dst.v[6] will be 'd', but the rest is garbage, strlen < len. */
    CHECK_STR_EQ("suffix intact", dst.v + 6, "world");
    CHECK_FS2VALID(2, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2_api, rev_catstr_triggers_growth) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("world");
    size_t orig_sz = dst.sz;
    
    // 100 A characters
    char big[128];
    memset(big, 'A', 100);
    big[100] = '\0';

    fs2_rev_catstr(&dst, big);

    CHECK_TRUE("buffer grew", dst.sz > orig_sz);
    CHECK_EQ("len", dst.len, 105);
    for (size_t i = 0; i < 100; i++)
        CHECK_EQ("prefix 'A'", dst.v[i], 'A');
    CHECK_STR_EQ("suffix intact", dst.v + 100, "world");
    CHECK_FS2VALID(2, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2_api, rev_catstr_local) {
    __fs2_checkreset();
    FS2LOCAL(dst, 32);
    memcpy(dst.v, "world", 6);
    dst.len = 5;
    CHECK_FS2VALID(1, &dst);

    fs2_rev_catstr(&dst, "hello ");

    CHECK_STR_EQ("prepended", dst.v, "hello world");
    CHECK_EQ("len", dst.len, 11);
    CHECK_TRUE("still LOCAL", fs2_is_local(&dst));
    CHECK_FS2VALID(2, &dst);

    fs2_free(&dst);
}

Test(fs2_api, rev_catstr_null_dst_aborts, .signal = SIGABRT) {
    fs2_rev_catstr(NULL, "abc");
}

Test(fs2_api, composite_pipeline_basic) {
    __fs2_checkreset();

    /* 1. Create "base" */
    fs2 s = fs2_fromstr("base");
    CHECK_FS2VALID(1, &s);

    /* 2. Clone: deep copy, but independent buffers */
    fs2 c = fs2_clone(&s);
    CHECK_FS2VALID(2, &c);

    /* 3. cat: "base" + "_tail" = "base_tail" */
    fs2 tail = FS2LITERAL("_tail");
    fs2_cat(&c, &tail);
    CHECK_STR_EQ("after cat", c.v, "base_tail");
    CHECK_FS2VALID(3, &c);

    /* 4. rev_catstr: "head_" + "base_tail" = "head_base_tail" */
    fs2_rev_catstr(&c, "head_");
    CHECK_STR_EQ("after rev_catstr", c.v, "head_base_tail");
    CHECK_FS2VALID(4, &c);

    /* 5. catstr: "head_base_tail" + "!" = "head_base_tail!" */
    fs2_catstr(&c, "!");
    CHECK_STR_EQ("after catstr", c.v, "head_base_tail!");
    CHECK_EQ("final len", c.len, 15);
    CHECK_FS2VALID(5, &c);

    /* Original untouched */
    CHECK_STR_EQ("orig intact", s.v, "base");
    CHECK_FS2VALID(6, &s);

    fs2_free(&s);
    fs2_free(&c);
    assert_balanced();
}

Test(fs2_api, composite_order_matters) {
    __fs2_checkreset();
    
    fs2 s = fs2_fromstr("X");
    
    fs2_catstr(&s, "Y");        // "XY"
    fs2_rev_catstr(&s, "Z");    // "ZXY"
    fs2_catstr(&s, "W");        // "ZXYW"
    fs2_rev_catstr(&s, "A");    // "AZXYW"
    
    CHECK_STR_EQ("pipeline result", s.v, "AZXYW");
    CHECK_EQ("len", s.len, 5);
    CHECK_FS2VALID(2, &s);
    
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, composite_stress_reallocs) {
    __fs2_checkreset();
    fs2 s = FS2();  /* start from zero */
    
    /* 100 times prepend and append to trigger many reallocs */
    for (int i = 0; i < 100; i++) {
        fs2_catstr(&s, "R");
        fs2_rev_catstr(&s, "L");
        CHECK_FS2VALID(i, &s);
    }
    
    CHECK_EQ("len", s.len, 200);
    
    /* Verify "L" and "R" are arranged correctly:
     * "LLL...RRR..." — all L at the front, all R at the end */
    for (size_t i = 0; i < 100; i++) {
        CHECK_EQ("front is L", s.v[i], 'L');
        CHECK_EQ("back is R",  s.v[100 + i], 'R');
    }
    
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, composite_local_buffer) {
    __fs2_checkreset();
    FS2LOCAL(s, 64);
    s.len = 0;
    s.v[0] = '\0';

    fs2_catstr(&s, "middle");
    fs2_rev_catstr(&s, "start_");
    fs2_catstr(&s, "_end");

    CHECK_STR_EQ("local pipeline", s.v, "start_middle_end");
    CHECK_EQ("len", s.len, 16);
    CHECK_TRUE("still LOCAL", fs2_is_local(&s));
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
}

Test(fs2_api, composite_clone_independence) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("shared");
    
    fs2 b = fs2_clone(&a);

    CHECK_FS2VALID(1, &a);
    CHECK_FS2VALID(2, &b);

    /* Mutate b in various ways */
    fs2_catstr(&b, "_b");
    fs2_rev_catstr(&b, "b_");
    CHECK_STR_EQ("b modified", b.v, "b_shared_b");
    CHECK_FS2VALID(3, &b);

    /* a must not change */
    CHECK_STR_EQ("a intact", a.v, "shared");
    CHECK_EQ("a len intact", a.len, 6);
    CHECK_FS2VALID(4, &a);

    fs2_free(&a);
    fs2_free(&b);
    assert_balanced();
}

/* -------------------------------------------------------------------------
 * fs2_cpy — overwrite dst with src
 * ------------------------------------------------------------------------- */

Test(fs2_api, cpy_basic) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("hello");
    fs2 src = fs2_fromstr("world");
    CHECK_FS2VALID(1, &dst);

    fs2 *ret = fs2_cpy(&dst, &src);

    CHECK_PTR_EQ("returns dst", ret, &dst);
    CHECK_STR_EQ("copied", dst.v, "world");
    CHECK_EQ("len", dst.len, 5);
    CHECK_FS2VALID(2, &dst);
    CHECK_FS2VALID(3, &src); // src untouched

    fs2_free(&dst);
    fs2_free(&src);
    assert_balanced();
}

Test(fs2_api, cpy_shrink) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("longer string");
    fs2 src = fs2_fromstr("short");
    
    /* dst was 13, now 5. Buffer must not shrink (sz stays the same) */
    size_t orig_sz = dst.sz;
    
    fs2_cpy(&dst, &src);

    CHECK_STR_EQ("copied shorter", dst.v, "short");
    CHECK_EQ("len", dst.len, 5);
    CHECK_EQ("sz unchanged", dst.sz, orig_sz); // reserve does not shrink
    CHECK_FS2VALID(2, &dst);

    fs2_free(&dst);
    fs2_free(&src);
    assert_balanced();
}

Test(fs2_api, cpy_grow) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("short");
    fs2 src = fs2_fromstr("much much longer string");
    size_t orig_sz = dst.sz;

    fs2_cpy(&dst, &src);

    CHECK_STR_EQ("copied longer", dst.v, "much much longer string");
    CHECK_EQ("len", dst.len, 23);
    CHECK_TRUE("sz grew", dst.sz > orig_sz);
    CHECK_FS2VALID(2, &dst);

    fs2_free(&dst);
    fs2_free(&src);
    assert_balanced();
}

Test(fs2_api, cpy_empty_src) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("hello");
    fs2 src = fs2_fromstr("");

    fs2_cpy(&dst, &src);

    CHECK_STR_EQ("cleared", dst.v, "");
    CHECK_EQ("len", dst.len, 0);
    CHECK_FS2VALID(2, &dst);

    fs2_free(&dst);
    fs2_free(&src);
    assert_balanced();
}

Test(fs2_api, cpy_local_to_alloc) {
    __fs2_checkreset();
    FS2LOCAL(dst, 16);
    memcpy(dst.v, "local", 6);
    dst.len = 5;
    
    fs2 src = fs2_fromstr("allocated");

    fs2_cpy(&dst, &src);

    CHECK_STR_EQ("copied", dst.v, "allocated");
    CHECK_EQ("len", dst.len, 9);
    CHECK_FS2VALID(2, &dst);

    fs2_free(&dst); // no-op
    fs2_free(&src);
    assert_balanced();
}

/* -------------------------------------------------------------------------
 * fs2_cpystr — overwrite dst with C-string
 * ------------------------------------------------------------------------- */

Test(fs2_api, cpystr_basic) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("hello");

    fs2 *ret = fs2_cpystr(&dst, "world");

    CHECK_PTR_EQ("returns dst", ret, &dst);
    CHECK_STR_EQ("copied", dst.v, "world");
    CHECK_EQ("len", dst.len, 5);
    CHECK_FS2VALID(2, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2_api, cpystr_null_clears) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("hello");

    /* fs2_wrap(NULL) -> FS2EMPTY -> fs2_cpy -> clears dst */
    fs2_cpystr(&dst, NULL);

    CHECK_STR_EQ("cleared", dst.v, "");
    CHECK_EQ("len", dst.len, 0);
    CHECK_FS2VALID(2, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2_api, cpystr_empty_string_clears) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("hello");

    fs2_cpystr(&dst, "");

    CHECK_STR_EQ("cleared", dst.v, "");
    CHECK_EQ("len", dst.len, 0);
    CHECK_FS2VALID(2, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2_api, composite_cpy_chain) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("initial");
    
    fs2_cpystr(&s, "one");        // "one"
    fs2_catstr(&s, " two");       // "one two"
    fs2_rev_catstr(&s, "zero ");  // "zero one two"
    fs2_cpy(&s, &FS2LITERAL("reset")); // "reset"
    fs2_catstr(&s, "!");          // "reset!"
    
    CHECK_STR_EQ("final", s.v, "reset!");
    CHECK_EQ("len", s.len, 6);
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, cpystr_null_no_alloc) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("hello");

    /* Verify allocation counters did not change */
    int alloc_before, freed_before, ba_before, bf_before;
    fs2_checkleak(&alloc_before, &freed_before, &ba_before, &bf_before);

    fs2_cpystr(&dst, NULL);

    int alloc_after, freed_after, ba_after, bf_after;
    fs2_checkleak(&alloc_after, &freed_after, &ba_after, &bf_after);

    CHECK_EQ("no new allocations", alloc_after, alloc_before);
    CHECK_EQ("no new frees",       freed_after, freed_before);
    
    CHECK_STR_EQ("cleared", dst.v, "");
    CHECK_EQ("len", dst.len, 0);
    CHECK_FS2VALID(2, &dst);

    fs2_free(&dst);
    assert_balanced();
}

/* -------------------------------------------------------------------------
 * fs2_sprintf — formatted write from position 0, updates len
 * ------------------------------------------------------------------------- */

Test(fs2_api, sprintf_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("initial");
    CHECK_FS2VALID(1, &s);

    long res = fs2_sprintf(&s, "Hello %s %d", "world", 42);

    CHECK_EQ("return value", res, 14); // strlen("Hello world 42")
    CHECK_STR_EQ("content", s.v, "Hello world 42");
    CHECK_EQ("len updated", s.len, 14);
    CHECK_FS2VALID(2, &s); // Invariant strlen == len holds

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, sprintf_overwrite_shorter) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("long string here");
    size_t orig_sz = s.sz;

    /* Write a shorter string. Buffer must not shrink. */
    long res = fs2_sprintf(&s, "short");

    CHECK_EQ("return value", res, 5);
    CHECK_STR_EQ("content", s.v, "short");
    CHECK_EQ("len updated", s.len, 5);
    CHECK_EQ("sz unchanged", s.sz, orig_sz);
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, sprintf_triggers_growth) {
    __fs2_checkreset();
    fs2 s = FS2(); // canonical empty
    size_t orig_sz = s.sz;

    /* 100 'A' characters via format */
    long res = fs2_sprintf(&s, "%0100d", 0); 
    // %0100d yields 100 characters (zeros + '0')

    CHECK_EQ("return value", res, 100);
    CHECK_EQ("len updated", s.len, 100);
    CHECK_TRUE("sz grew", s.sz > orig_sz);
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, sprintf_exact_fit_no_realloc) {
    __fs2_checkreset();
    /* Create buffer for exactly 5 chars + NUL = 6 */
    fs2 s = fs2_init(6); 
    
    /* Write exactly 5 characters */
    long res = fs2_sprintf(&s, "12345");

    CHECK_EQ("return value", res, 5);
    CHECK_STR_EQ("content", s.v, "12345");
    CHECK_EQ("len updated", s.len, 5);
    CHECK_GE("sz not changed (fits exactly)", s.sz, 6);
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, sprintf_regression_last_char_not_eaten) {
    __fs2_checkreset();
    fs2 s = FS2();
    
    /* Regression for a bug: vsnprintf(s->v, needed, ...) ate the last character.
     * Here needed = 5. If size=5 is passed instead of 6, we get "1234". */
    fs2_sprintf(&s, "12345");

    CHECK_STR_EQ("last char kept", s.v, "12345");
    CHECK_EQ("len", s.len, 5);
    
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, sprintf_empty_format) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    long res = fs2_sprintf(&s, "%s", "");

    CHECK_EQ("return value", res, 0);
    CHECK_STR_EQ("content", s.v, "");
    CHECK_EQ("len updated", s.len, 0);
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, sprintf_null_aborts, .signal = SIGABRT) {
    fs2_sprintf(NULL, "abc");
}

Test(fs2_api, sprintf_null_fmt_aborts, .signal = SIGABRT) {
    fs2 s = FS2();
    fs2_sprintf(&s, NULL);
}

/* =========================================================================
 * fs2_sprintf_concat — append formatted text
 * ========================================================================= */

Test(fs2_api, sprintf_concat_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("Hello");
    CHECK_FS2VALID(1, &s);

    long res = fs2_sprintf_concat(&s, ", %s!", "world");

    CHECK_EQ("return", res, 8);            /* ", world!" = 8 chars */
    CHECK_STR_EQ("content", s.v, "Hello, world!");
    CHECK_EQ("len", s.len, 13);
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, sprintf_concat_empty_dst) {
    __fs2_checkreset();
    fs2 s = FS2();                          /* canonical empty, v == NULL */
    CHECK_FS2VALID(1, &s);

    long res = fs2_sprintf_concat(&s, "%d", 42);

    CHECK_EQ("return", res, 2);
    CHECK_STR_EQ("content", s.v, "42");
    CHECK_EQ("len", s.len, 2);
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, sprintf_concat_empty_fmt) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("data");

    long res = fs2_sprintf_concat(&s,  "%.0s", "x");

    CHECK_EQ("return", res, 0);
    CHECK_STR_EQ("content unchanged", s.v, "data");
    CHECK_EQ("len unchanged", s.len, 4);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, sprintf_concat_chain) {
    __fs2_checkreset();
    fs2 s = FS2();

    fs2_sprintf_concat(&s, "%d", 1);
    fs2_sprintf_concat(&s, ",%d", 2);
    fs2_sprintf_concat(&s, ",%d", 3);

    CHECK_STR_EQ("content", s.v, "1,2,3");
    CHECK_EQ("len", s.len, 5);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, sprintf_concat_triggers_growth) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("x");
    size_t orig_sz = s.sz;

    char big[128];
    memset(big, 'A', sizeof(big) - 1);
    big[sizeof(big) - 1] = '\0';

    fs2_sprintf_concat(&s, "%s", big);

    CHECK_TRUE("buffer grew", s.sz > orig_sz);
    CHECK_EQ("len", s.len, 1 + 127);
    CHECK_EQ("first char", s.v[0], 'x');
    for (size_t i = 1; i < 128; i++)
        CHECK_EQ("expected 'A'", s.v[i], 'A');
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, sprintf_concat_return_equals_len_delta) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("prefix:");

    size_t before = s.len;
    long res = fs2_sprintf_concat(&s, "%s", "abc");
    CHECK_EQ("return == delta", res, (long)(s.len - before));

    before = s.len;
    res = fs2_sprintf_concat(&s, "%d", 12345);
    CHECK_EQ("return == delta int", res, (long)(s.len - before));

    before = s.len;
    res = fs2_sprintf_concat(&s, "%c%c", 'X', 'Y');
    CHECK_EQ("return == delta chars", res, (long)(s.len - before));

    CHECK_STR_EQ("final", s.v, "prefix:abc12345XY");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, sprintf_concat_local) {
    __fs2_checkreset();
    FS2LOCAL(s, 64);
    memcpy(s.v, "start:", 6);
    s.len = 6;
    CHECK_FS2VALID(1, &s);

    fs2_sprintf_concat(&s, "%s=%d", "key", 42);

    CHECK_STR_EQ("content", s.v, "start:key=42");
    CHECK_EQ("len", s.len, 12);
    CHECK_TRUE("still LOCAL", fs2_is_local(&s));
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);   /* no-op */
}

Test(fs2_api, sprintf_concat_null_aborts, .signal = SIGABRT) {
    fs2_sprintf_concat(NULL, "x");
}

Test(fs2_api, sprintf_concat_null_fmt_aborts, .signal = SIGABRT) {
    fs2 s = FS2();
    fs2_sprintf_concat(&s, NULL);
    fs2_free(&s);
}

/* =========================================================================
 * Composite
 * ========================================================================= */

Test(fs2_api, composite_sprintf_concat_build_csv) {
    __fs2_checkreset();
    fs2 row = FS2();

    fs2_sprintf_concat(&row, "%d",       42);
    fs2_sprintf_concat(&row, ",%s",      "alice");
    fs2_sprintf_concat(&row, ",%.2f",    3.14);

    CHECK_STR_EQ("row", row.v, "42,alice,3.14");
    CHECK_EQ("len", row.len, 13);
    CHECK_FS2VALID(1, &row);

    fs2_free(&row);
    assert_balanced();
}

Test(fs2_api, composite_sprintf_concat_build_message) {
    __fs2_checkreset();
    fs2 msg = FS2();

    int id = 7;
    const char *name = "widget";
    double price = 19.99;

    fs2_sprintf_concat(&msg, "item[%d] %s = $%.2f", id, name, price);

    CHECK_STR_EQ("msg", msg.v, "item[7] widget = $19.99");
    CHECK_FS2VALID(1, &msg);

    fs2_free(&msg);
    assert_balanced();
}

Test(fs2_api, composite_sprintf_concat_with_generic_serialize) {
    __fs2_checkreset();
    fs2 s = fs2_from_new(42);           /* "42" */

    fs2_sprintf_concat(&s, "|%s|", "name");
    fs2_sprintf_concat(&s, "%.2f", 3.14);

    CHECK_STR_EQ("content", s.v, "42|name|3.14");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, composite_sprintf_concat_100_appends) {
    __fs2_checkreset();
    fs2 s = FS2();

    /* 100 small appends — guaranteed 3-4 reallocs */
    for (int i = 0; i < 100; i++) {
        fs2_sprintf_concat(&s, "%d", i);
        if (i < 99) fs2_sprintf_concat(&s, ",");
    }

    /* "0,1,2,...,9,10,11,...,99" — 289 chars */
    CHECK_EQ("len", s.len, 289);
    CHECK_EQ("first chars", strncmp(s.v, "0,1,2,", 6), 0);
    CHECK_EQ("last chars",  strcmp(s.v + s.len - 2, "99"), 0);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, sprintf_bad_format_returns_negative) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("initial");
    
    /* %d without an argument is UB in C, better not to test it.
     * An invalid specifier (e.g. %q) will return -1 though. */

    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wformat"
    long res = fs2_sprintf(&s, "%q");
    #pragma GCC diagnostic pop

    CHECK_TRUE("return value < 0", res <= 0);
    /* len does not have to change, so we just check we are alive */
    
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_substr (in-place)
 * ========================================================================= */

Test(fs2_api, substr_middle) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abcdef");
    CHECK_FS2VALID(1, &s);

    fs2 *ret = fs2_substr(&s, 2, 3); // "cde"

    CHECK_PTR_EQ("returns s", ret, &s);
    CHECK_STR_EQ("content", s.v, "cde");
    CHECK_EQ("len", s.len, 3);
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, substr_clamp_len) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");

    fs2_substr(&s, 1, 100); // "bc", len clamp to 2

    CHECK_STR_EQ("clamped len", s.v, "bc");
    CHECK_EQ("len", s.len, 2);
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, substr_clamp_from) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");

    fs2_substr(&s, 100, 2); // from > len, becomes empty

    CHECK_STR_EQ("empty", s.v, "");
    CHECK_EQ("len", s.len, 0);
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, substr_empty) {
    __fs2_checkreset();
    fs2 s = FS2();

    fs2_substr(&s, 0, 5);
    CHECK_EQ("still empty", s.len, 0);
    CHECK_FS2VALID(1, &s);
    
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_substr_new (value constructor)
 * ========================================================================= */

Test(fs2_api, substr_new_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abcdef");

    fs2 res = fs2_substr_new(&s, 2, 3); // "cde"

    CHECK_STR_EQ("content", res.v, "cde");
    CHECK_EQ("len", res.len, 3);
    CHECK_TRUE("is alloc", fs2_is_alloc(&res));
    CHECK_FS2VALID(1, &res);
    CHECK_FS2VALID(2, &s); // original untouched
    CHECK_STR_EQ("original intact", s.v, "abcdef");

    fs2_free(&s);
    fs2_free(&res);
    assert_balanced();
}

Test(fs2_api, substr_new_static_src) {
    __fs2_checkreset();
    fs2 src = FS2LITERAL("hello");

    fs2 res = fs2_substr_new(&src, 1, 3); // "ell"

    CHECK_STR_EQ("content", res.v, "ell");
    CHECK_FS2VALID(1, &res);
    CHECK_FS2VALID(2, &src);

    fs2_free(&res); // src - STATIC, free no-op
    assert_balanced();
}

Test(fs2_api, substr_new_clamp) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");

    fs2 res = fs2_substr_new(&s, 1, 100); // "bc"

    CHECK_STR_EQ("clamped", res.v, "bc");
    CHECK_EQ("len", res.len, 2);
    CHECK_FS2VALID(1, &res);

    fs2_free(&s);
    fs2_free(&res);
    assert_balanced();
}

Test(fs2_api, substr_new_oob) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");

    /* from > len, must return empty fs2 */
    fs2 res = fs2_substr_new(&s, 10, 5); 

    CHECK_EQ("len", res.len, 0);
    CHECK_TRUE("is empty", fs2_is_empty(&res));
    CHECK_FS2VALID(1, &res);

    fs2_free(&s);
    fs2_free(&res);
    assert_balanced();
}

Test(fs2_api, substr_new_empty_src) {
    __fs2_checkreset();
    fs2 s = FS2();

    fs2 res = fs2_substr_new(&s, 0, 5);

    CHECK_EQ("len", res.len, 0);
    CHECK_TRUE("is empty", fs2_is_empty(&res));
    CHECK_FS2VALID(1, &res);

    fs2_free(&s);
    fs2_free(&res);
    assert_balanced();
}

/* =========================================================================
 * fs2_heap_substr (heap constructor)
 * ========================================================================= */

Test(fs2_api, heap_substr_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abcdef");

    fs2 *res = fs2_heap_substr(&s, 2, 3); // "cde"

    CHECK_STR_EQ("content", res->v, "cde");
    CHECK_EQ("len", res->len, 3);
    CHECK_TRUE("BODYALLOC set", fs2_is_bodyalloc(res));
    CHECK_FS2VALID(1, res);
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
    fs2_free(res); // frees both the struct and the buffer
    assert_balanced();
}

Test(fs2_api, heap_substr_clamp) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");

    fs2 *res = fs2_heap_substr(&s, 1, 100); // "bc"

    CHECK_STR_EQ("clamped", res->v, "bc");
    CHECK_EQ("len", res->len, 2);
    CHECK_FS2VALID(1, res);

    fs2_free(&s);
    fs2_free(res);
    assert_balanced();
}

Test(fs2_api, heap_substr_empty) {
    __fs2_checkreset();
    fs2 s = FS2();

    fs2 *res = fs2_heap_substr(&s, 10, 5);

    CHECK_EQ("len", res->len, 0);
    CHECK_TRUE("is empty", fs2_is_empty(res));
    CHECK_FS2VALID(1, res);

    fs2_free(&s);
    fs2_free(res);
    assert_balanced();
}
// Cumulative
Test(fs2_api, composite_substr_pipeline) {
    __fs2_checkreset();

    /* 1. Original string */
    fs2 src = fs2_fromstr("Hello, World! This is fs2.");
    CHECK_FS2VALID(1, &src);

    /* 2. Extract "Hello" to heap */
    fs2 *hello = fs2_heap_substr(&src, 0, 5); // "Hello"
    CHECK_STR_EQ("extracted hello", hello->v, "Hello");
    CHECK_TRUE("is bodyalloc", fs2_is_bodyalloc(hello));
    CHECK_FS2VALID(2, hello);

    /* 3. Extract "World!" to stack */
    fs2 world = fs2_substr_new(&src, 7, 6); // "World!"
    CHECK_STR_EQ("extracted world", world.v, "World!");
    CHECK_FS2VALID(3, &world);

    /* 4. Concatenate: "Hello" + "World!" = "HelloWorld!" */
    fs2_cat(hello, &world);
    CHECK_STR_EQ("concatenated", hello->v, "HelloWorld!");
    CHECK_EQ("len", hello->len, 11);
    CHECK_FS2VALID(4, hello);

    /* 5. Append a string via catstr */
    fs2_catstr(hello, " And fs2");
    CHECK_STR_EQ("appended", hello->v, "HelloWorld! And fs2");
    CHECK_EQ("len", hello->len, 19);
    CHECK_FS2VALID(5, hello);

    /* 6. Trim in-place to "HelloWorld" */
    fs2_substr(hello, 0, 10);
    CHECK_STR_EQ("truncated in-place", hello->v, "HelloWorld");
    CHECK_EQ("len", hello->len, 10);
    CHECK_FS2VALID(6, hello);

    /* 7. Create an empty string and copy the result into it (cpy) */
    fs2 final_copy = FS2();
    fs2_cpy(&final_copy, hello);
    CHECK_STR_EQ("copied", final_copy.v, "HelloWorld");
    CHECK_FS2VALID(7, &final_copy);

    /* 8. Verify the original is untouched by all these operations */
    CHECK_STR_EQ("src intact", src.v, "Hello, World! This is fs2.");
    CHECK_FS2VALID(8, &src);

    /* 9. Clean up */
    fs2_free(hello);     // frees both the struct and the buffer
    fs2_free(&world);
    fs2_free(&final_copy);
    fs2_free(&src);

    assert_balanced();
}

Test(fs2_api, composite_substr_clamping_pipeline) {
    __fs2_checkreset();

    fs2 s = fs2_fromstr("abcdef");
    
    /* 
     * 1. Take len=100 (should be clamped to 3). 
     * We get "def".
     */
    fs2 *part1 = fs2_heap_substr(&s, 3, 100);
    CHECK_STR_EQ("part1 clamped", part1->v, "def");
    CHECK_EQ("part1 len", part1->len, 3);
    CHECK_FS2VALID(1, part1);

    /* 
     * 2. Take from=10 (out of bounds). Should be an empty string.
     */
    fs2 part2 = fs2_substr_new(&s, 10, 5);
    CHECK_EQ("part2 is empty", part2.len, 0);
    CHECK_TRUE("part2 is empty", fs2_is_empty(&part2));
    CHECK_FS2VALID(2, &part2);

    /* 
     * 3. Glue emptiness to part1. part1 must not change.
     */
    fs2_cat(part1, &part2);
    CHECK_STR_EQ("cat empty", part1->v, "def");
    CHECK_EQ("len unchanged", part1->len, 3);
    CHECK_FS2VALID(3, part1);

    /* 
     * 4. Do in-place substr with from = 1 and len = 100. 
     * 2 characters remain ("ef").
     */
    fs2_substr(part1, 1, 100);
    CHECK_STR_EQ("in-place clamped", part1->v, "ef");
    CHECK_EQ("len clamped", part1->len, 2);
    CHECK_FS2VALID(4, part1);

    /* 5. Try to shift beyond bounds (from=10). Should remain empty. */
    fs2_substr(part1, 10, 10);
    CHECK_EQ("in-place empty", part1->len, 0);
    CHECK_FS2VALID(5, part1);

    fs2_free(part1);
    fs2_free(&part2);
    fs2_free(&s);

    assert_balanced();
}

/* =========================================================================
 * fs2_left (in-place, value, heap)
 * ========================================================================= */

Test(fs2_api, left_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abcdef");
    
    fs2_left(&s, 3);
    CHECK_STR_EQ("left", s.v, "abc");
    CHECK_EQ("len", s.len, 3);
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, left_clamp) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");
    
    /* Request more than available — should get the entire string */
    fs2_left(&s, 100);
    CHECK_STR_EQ("whole string", s.v, "abc");
    CHECK_EQ("len", s.len, 3);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, left_new_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abcdef");
    
    fs2 res = fs2_left_new(&s, 3);
    CHECK_STR_EQ("left", res.v, "abc");
    CHECK_FS2VALID(1, &res);
    
    fs2_free(&s);
    fs2_free(&res);
    assert_balanced();
}

Test(fs2_api, heap_left_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abcdef");
    
    fs2 *res = fs2_heap_left(&s, 3);
    CHECK_STR_EQ("left", res->v, "abc");
    CHECK_FS2VALID(1, res);
    
    fs2_free(&s);
    fs2_free(res);
    assert_balanced();
}

/* =========================================================================
 * fs2_right (in-place, value, heap) — regression on underflow!
 * ========================================================================= */

Test(fs2_api, right_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abcdef");
    
    fs2_right(&s, 3);
    CHECK_STR_EQ("right", s.v, "def");
    CHECK_EQ("len", s.len, 3);
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

/* ⚠ REGRESSION: this test would have crashed on the old code due to underflow! */
Test(fs2_api, right_len_ge_slen) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");
    
    fs2_right(&s, 100);
    CHECK_STR_EQ("whole string", s.v, "abc");
    CHECK_EQ("len unchanged", s.len, 3);
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, right_new_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abcdef");
    
    fs2 res = fs2_right_new(&s, 3);
    CHECK_STR_EQ("right", res.v, "def");
    CHECK_FS2VALID(1, &res);
    
    fs2_free(&s);
    fs2_free(&res);
    assert_balanced();
}

Test(fs2_api, right_new_len_ge_slen) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");
    
    fs2 res = fs2_right_new(&s, 100);
    CHECK_STR_EQ("whole string", res.v, "abc");
    CHECK_FS2VALID(1, &res);
    
    fs2_free(&s);
    fs2_free(&res);
    assert_balanced();
}

Test(fs2_api, heap_right_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abcdef");
    
    fs2 *res = fs2_heap_right(&s, 3);
    CHECK_STR_EQ("right", res->v, "def");
    CHECK_FS2VALID(1, res);
    
    fs2_free(&s);
    fs2_free(res);
    assert_balanced();
}

Test(fs2_api, heap_right_len_ge_slen) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");
    
    fs2 *res = fs2_heap_right(&s, 100);
    CHECK_STR_EQ("whole string", res->v, "abc");
    CHECK_FS2VALID(1, res);
    
    fs2_free(&s);
    fs2_free(res);
    assert_balanced();
}

/* =========================================================================
 * fs2_toupper — in-place uppercase
 * ========================================================================= */

Test(fs2_api, toupper_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("Hello World 123!");
    
    fs2 *ret = fs2_toupper(&s);
    
    CHECK_PTR_EQ("returns str", ret, &s);
    CHECK_STR_EQ("uppercased", s.v, "HELLO WORLD 123!");
    CHECK_EQ("len unchanged", s.len, 16);
    CHECK_FS2VALID(1, &s);
    
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, toupper_local) {
    __fs2_checkreset();
    FS2LOCALFILL(s, "%s", "abc");
    
    fs2_toupper(&s);
    CHECK_STR_EQ("local uppercased", s.v, "ABC");
    CHECK_FS2VALID(1, &s);
    
    fs2_free(&s); // no-op
}

Test(fs2_api, toupper_empty) {
    __fs2_checkreset();
    fs2 s = FS2(); // v=NULL
    
    fs2_toupper(&s); // Should not crash
    CHECK_EQ("len", s.len, 0);
    CHECK_FS2VALID(1, &s);
    
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_toupper_new — value constructor
 * ========================================================================= */

Test(fs2_api, toupper_new_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("Hello");
    
    fs2 res = fs2_toupper_new(&s);
    
    CHECK_STR_EQ("new uppercased", res.v, "HELLO");
    CHECK_FS2VALID(1, &res);
    
    CHECK_STR_EQ("original intact", s.v, "Hello");
    CHECK_FS2VALID(2, &s);
    
    fs2_free(&s);
    fs2_free(&res);
    assert_balanced();
}

Test(fs2_api, toupper_new_static_src) {
    __fs2_checkreset();
    fs2 src = FS2LITERAL("hello");
    
    /* Clones STATIC to ALLOC, then does upper. Original is untouched. */
    fs2 res = fs2_toupper_new(&src);
    
    CHECK_STR_EQ("new uppercased from static", res.v, "HELLO");
    CHECK_FS2VALID(1, &res);
    CHECK_FS2VALID(2, &src);
    CHECK_STR_EQ("static intact", src.v, "hello");
    
    fs2_free(&res); // src - STATIC, free no-op
    assert_balanced();
}

/* =========================================================================
 * fs2_heap_toupper — heap constructor
 * ========================================================================= */

Test(fs2_api, heap_toupper_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("Hello");
    
    fs2 *res = fs2_heap_toupper(&s);
    
    CHECK_STR_EQ("heap uppercased", res->v, "HELLO");
    CHECK_TRUE("bodyalloc", fs2_is_bodyalloc(res));
    CHECK_FS2VALID(1, res);
    
    CHECK_STR_EQ("original intact", s.v, "Hello");
    CHECK_FS2VALID(2, &s);
    
    fs2_free(&s);
    fs2_free(res);
    assert_balanced();
}

/* =========================================================================
 * fs2_tolower — in-place lowercase
 * ========================================================================= */

Test(fs2_api, tolower_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("Hello World 123!");
    
    fs2 *ret = fs2_tolower(&s);
    
    CHECK_PTR_EQ("returns str", ret, &s);
    CHECK_STR_EQ("lowercased", s.v, "hello world 123!");
    CHECK_EQ("len unchanged", s.len, 16);
    CHECK_FS2VALID(1, &s);
    
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, tolower_new_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("Hello");
    
    fs2 res = fs2_tolower_new(&s);
    
    CHECK_STR_EQ("new lowercased", res.v, "hello");
    CHECK_FS2VALID(1, &res);
    
    CHECK_STR_EQ("original intact", s.v, "Hello");
    
    fs2_free(&s);
    fs2_free(&res);
    assert_balanced();
}

Test(fs2_api, heap_tolower_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("Hello");
    
    fs2 *res = fs2_heap_tolower(&s);
    
    CHECK_STR_EQ("heap lowercased", res->v, "hello");
    CHECK_FS2VALID(1, res);
    
    fs2_free(&s);
    fs2_free(res);
    assert_balanced();
}

/* =========================================================================
 * fs2_toupper / fs2_tolower (regression for UB)
 * ========================================================================= */

Test(fs2_api, toupper_non_ascii_no_crash) {
    __fs2_checkreset();
    
    /* String with bytes >= 128 (e.g. UTF-8 Cyrillic) */
    fs2 s = fs2_fromstr("\xD0\xB4" "\xD0\xB0"); // "da" (Cyrillic)
    size_t orig_len = s.len;

    /* This used to crash with a Segfault due to signed char.
     * It should now simply return the string (in C locale they are unchanged). */
    fs2_toupper(&s);
    
    CHECK_EQ("len unchanged", s.len, orig_len);
    CHECK_FS2VALID(1, &s);
    
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, tolower_non_ascii_no_crash) {
    __fs2_checkreset();
    
    fs2 s = fs2_fromstr("\xC4\xE4"); // 'Ä' 'ä' in Latin-1
    size_t orig_len = s.len;

    fs2_tolower(&s);
    
    CHECK_EQ("len unchanged", s.len, orig_len);
    CHECK_FS2VALID(1, &s);
    
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * Check on mixed characters
 * ========================================================================= */

Test(fs2_api, toupper_mixed) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("aBcD 123_!");
    
    fs2_toupper(&s);
    CHECK_STR_EQ("uppercased", s.v, "ABCD 123_!");
    CHECK_FS2VALID(1, &s);
    
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, tolower_mixed) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("AbCd 123_!");
    
    fs2_tolower(&s);
    CHECK_STR_EQ("lowercased", s.v, "abcd 123_!");
    CHECK_FS2VALID(1, &s);
    
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_rpad — right padding (in-place)
 * ========================================================================= */

Test(fs2_api, rpad_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");
    fs2 pad = fs2_fromstr("12");

    fs2 *ret = fs2_rpad(&s, 7, &pad);

    CHECK_PTR_EQ("returns str", ret, &s);
    CHECK_STR_EQ("padded", s.v, "abc1212");
    CHECK_EQ("len", s.len, 7);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    fs2_free(&pad);
    assert_balanced();
}

Test(fs2_api, rpad_pad_longer_than_needed) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("x");
    fs2 pad = fs2_fromstr("abcdef");

    /* Pad longer than needed to fill. A prefix of the pad is taken. */
    fs2_rpad(&s, 4, &pad);

    CHECK_STR_EQ("padded", s.v, "xabc");
    CHECK_EQ("len", s.len, 4);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    fs2_free(&pad);
    assert_balanced();
}

/* ⚠ REGRESSION: using fs2_right instead of fs2_left would crash this test */
Test(fs2_api, rpad_truncation) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abcdef");
    fs2 pad = fs2_fromstr(".");

    /* If len < str->len, rpad must trim to the left len characters */
    fs2_rpad(&s, 3, &pad);

    CHECK_STR_EQ("truncated", s.v, "abc"); // not "def"!
    CHECK_EQ("len", s.len, 3);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    fs2_free(&pad);
    assert_balanced();
}

Test(fs2_api, rpad_empty_pad) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");
    fs2 pad = fs2_fromstr("");

    /* Empty pad — string is unchanged (if no trimming needed) */
    fs2_rpad(&s, 10, &pad);

    CHECK_STR_EQ("unchanged", s.v, "abc");
    CHECK_EQ("len", s.len, 3);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    fs2_free(&pad);
    assert_balanced();
}

Test(fs2_api, rpad_same_len) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");
    fs2 pad = fs2_fromstr(".");

    fs2_rpad(&s, 3, &pad);

    CHECK_STR_EQ("unchanged", s.v, "abc");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    fs2_free(&pad);
    assert_balanced();
}

/* =========================================================================
 * fs2_rpadstr — sugar for C-strings
 * ========================================================================= */

Test(fs2_api, rpadstr_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("1");

    fs2_rpadstr(&s, 5, "0");

    CHECK_STR_EQ("padded", s.v, "10000");
    CHECK_EQ("len", s.len, 5);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, rpadstr_null_pad) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");

    /* fs2_wrap(NULL) -> FS2EMPTY (len=0), so no padding will occur */
    fs2_rpadstr(&s, 10, NULL);

    CHECK_STR_EQ("unchanged", s.v, "abc");
    CHECK_EQ("len", s.len, 3);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_rpad_new — stack constructor
 * ========================================================================= */

Test(fs2_api, rpad_new_basic) {
    __fs2_checkreset();
    fs2 str = fs2_fromstr("a");
    fs2 pad = fs2_fromstr("bc");

    fs2 res = fs2_rpad_new(&str, 5, &pad);

    CHECK_STR_EQ("new padded", res.v, "abcbc");
    CHECK_EQ("len", res.len, 5);
    CHECK_FS2VALID(1, &res);
    
    CHECK_STR_EQ("original intact", str.v, "a");
    CHECK_FS2VALID(2, &str);

    fs2_free(&str);
    fs2_free(&pad);
    fs2_free(&res);
    assert_balanced();
}

Test(fs2_api, rpad_new_null_pad) {
    __fs2_checkreset();
    fs2 str = fs2_fromstr("abc");

    /* fs2_rpad_new has if (!pad) return res; */
    fs2 res = fs2_rpad_new(&str, 10, NULL);

    CHECK_STR_EQ("cloned but not padded", res.v, "abc");
    CHECK_EQ("len", res.len, 3);
    CHECK_FS2VALID(1, &res);

    fs2_free(&str);
    fs2_free(&res);
    assert_balanced();
}

/* =========================================================================
 * fs2_rpadstr_new — stack constructor from C-string
 * ========================================================================= */

Test(fs2_api, rpadstr_new_basic) {
    __fs2_checkreset();
    fs2 str = fs2_fromstr("1");

    fs2 res = fs2_rpadstr_new(&str, 4, "0");

    CHECK_STR_EQ("new padded", res.v, "1000");
    CHECK_EQ("len", res.len, 4);
    CHECK_FS2VALID(1, &res);

    fs2_free(&str);
    fs2_free(&res);
    assert_balanced();
}

Test(fs2_api, rpad_postgres_example) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hi");
    fs2 pad = fs2_fromstr("xy");

    /* From the Postgres docs: rpad('hi', 5, 'xy') → hixyx */
    fs2_rpad(&s, 5, &pad);

    CHECK_STR_EQ("postgres example", s.v, "hixyx");
    CHECK_EQ("len", s.len, 5);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    fs2_free(&pad);
    assert_balanced();
}

Test(fs2_api, rpad_truncates_from_right) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");
    fs2 pad = fs2_fromstr("x");

    /* Example: rpad('hello', 2) → 'he' (trims the tail, keeps the left part) */
    fs2_rpad(&s, 2, &pad);

    CHECK_STR_EQ("truncated left part", s.v, "he");
    CHECK_EQ("len", s.len, 2);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    fs2_free(&pad);
    assert_balanced();
}

Test(fs2_api, rpad_empty_pad_does_not_extend) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");
    fs2 pad = fs2_fromstr("");

    /* If pad is empty but len is larger, string is unchanged (cannot pad with nothing) */
    fs2_rpad(&s, 10, &pad);

    CHECK_STR_EQ("unchanged", s.v, "abc");
    CHECK_EQ("len", s.len, 3);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    fs2_free(&pad);
    assert_balanced();
}

Test(fs2_api, rpad_len_zero) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");
    fs2 pad = fs2_fromstr("x");

    /* If len = 0, nothing remains of the string */
    fs2_rpad(&s, 0, &pad);

    CHECK_STR_EQ("empty", s.v, "");
    CHECK_EQ("len", s.len, 0);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    fs2_free(&pad);
    assert_balanced();
}

/* =========================================================================
 * fs2_lpad — left padding (in-place)
 * ========================================================================= */

Test(fs2_api, lpad_postgres_example) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hi");
    fs2 pad = fs2_fromstr("xy");

    /* From the Postgres docs: lpad('hi', 5, 'xy') → xyxhi */
    fs2 *ret = fs2_lpad(&s, 5, &pad);

    CHECK_PTR_EQ("returns str", ret, &s);
    CHECK_STR_EQ("padded", s.v, "xyxhi");
    CHECK_EQ("len", s.len, 5);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    fs2_free(&pad);
    assert_balanced();
}

Test(fs2_api, lpad_pad_longer_than_needed) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("x");
    fs2 pad = fs2_fromstr("abcdef");

    /* Pad longer than needed (3 chars needed). The prefix "abc" is taken */
    fs2_lpad(&s, 4, &pad);

    CHECK_STR_EQ("padded", s.v, "abcx");
    CHECK_EQ("len", s.len, 4);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    fs2_free(&pad);
    assert_balanced();
}

Test(fs2_api, lpad_truncates_from_right) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");
    fs2 pad = fs2_fromstr("x");

    /* If len < str->len, lpad trims to the LEFT part */
    fs2_lpad(&s, 3, &pad);

    CHECK_STR_EQ("truncated", s.v, "hel"); // not "llo"
    CHECK_EQ("len", s.len, 3);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    fs2_free(&pad);
    assert_balanced();
}

Test(fs2_api, lpad_empty_pad) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");
    fs2 pad = fs2_fromstr("");

    /* Empty pad — string is unchanged */
    fs2_lpad(&s, 10, &pad);

    CHECK_STR_EQ("unchanged", s.v, "abc");
    CHECK_EQ("len", s.len, 3);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    fs2_free(&pad);
    assert_balanced();
}

Test(fs2_api, lpad_same_len) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");
    fs2 pad = fs2_fromstr(".");

    fs2_lpad(&s, 3, &pad);

    CHECK_STR_EQ("unchanged", s.v, "abc");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    fs2_free(&pad);
    assert_balanced();
}

Test(fs2_api, lpad_len_zero) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");
    fs2 pad = fs2_fromstr("x");

    fs2_lpad(&s, 0, &pad);

    CHECK_STR_EQ("empty", s.v, "");
    CHECK_EQ("len", s.len, 0);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    fs2_free(&pad);
    assert_balanced();
}

/* =========================================================================
 * fs2_lpadstr — sugar for C-strings
 * ========================================================================= */

Test(fs2_api, lpadstr_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("1");

    fs2_lpadstr(&s, 5, "0");

    CHECK_STR_EQ("padded", s.v, "00001");
    CHECK_EQ("len", s.len, 5);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, lpadstr_null_pad) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");

    /* fs2_wrap(NULL) -> FS2EMPTY (len=0), no padding will occur */
    fs2_lpadstr(&s, 10, NULL);

    CHECK_STR_EQ("unchanged", s.v, "abc");
    CHECK_EQ("len", s.len, 3);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_lpad_new — stack constructor
 * ========================================================================= */

Test(fs2_api, lpad_new_basic) {
    __fs2_checkreset();
    fs2 str = fs2_fromstr("a");
    fs2 pad = fs2_fromstr("bc");

    fs2 res = fs2_lpad_new(&str, 5, &pad);

    CHECK_STR_EQ("new padded", res.v, "bcbca");
    CHECK_EQ("len", res.len, 5);
    CHECK_FS2VALID(1, &res);
    
    CHECK_STR_EQ("original intact", str.v, "a");
    CHECK_FS2VALID(2, &str);

    fs2_free(&str);
    fs2_free(&pad);
    fs2_free(&res);
    assert_balanced();
}

Test(fs2_api, lpad_new_null_pad) {
    __fs2_checkreset();
    fs2 str = fs2_fromstr("abc");

    /* fs2_lpad_new has if (!pad) return res; */
    fs2 res = fs2_lpad_new(&str, 10, NULL);

    CHECK_STR_EQ("cloned but not padded", res.v, "abc");
    CHECK_EQ("len", res.len, 3);
    CHECK_FS2VALID(1, &res);

    fs2_free(&str);
    fs2_free(&res);
    assert_balanced();
}

/* =========================================================================
 * fs2_lpadstr_new — stack constructor from C-string
 * ========================================================================= */

Test(fs2_api, lpadstr_new_basic) {
    __fs2_checkreset();
    fs2 str = fs2_fromstr("1");

    fs2 res = fs2_lpadstr_new(&str, 4, "0");

    CHECK_STR_EQ("new padded", res.v, "0001");
    CHECK_EQ("len", res.len, 4);
    CHECK_FS2VALID(1, &res);

    fs2_free(&str);
    fs2_free(&res);
    assert_balanced();
}

/* =========================================================================
 * fs2_genrnd — random string generator
 * ========================================================================= */

Test(fs2_api, genrnd_lowercase) {
    __fs2_checkreset();
    fs2 s = FS2();

    fs2_genrnd(&s, 50, 'a');

    CHECK_EQ("len", s.len, 50);
    CHECK_FS2VALID(1, &s);

    for (size_t i = 0; i < s.len; i++) {
        CHECK_TRUE("is lowercase", s.v[i] >= 'a' && s.v[i] <= 'z');
    }

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, genrnd_uppercase) {
    __fs2_checkreset();
    fs2 s = FS2();

    fs2_genrnd(&s, 50, 'A');

    CHECK_EQ("len", s.len, 50);
    CHECK_FS2VALID(1, &s);

    for (size_t i = 0; i < s.len; i++) {
        CHECK_TRUE("is uppercase", s.v[i] >= 'A' && s.v[i] <= 'Z');
    }

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, genrnd_digits) {
    __fs2_checkreset();
    fs2 s = FS2();

    fs2_genrnd(&s, 50, '0');

    CHECK_EQ("len", s.len, 50);
    CHECK_FS2VALID(1, &s);

    for (size_t i = 0; i < s.len; i++) {
        CHECK_TRUE("is digit", s.v[i] >= '0' && s.v[i] <= '9');
    }

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, genrnd_unknown_type_fallback) {
    __fs2_checkreset();
    fs2 s = FS2();

    /* Pass an unknown type, expect fallback '?' */
    fs2_genrnd(&s, 10, 'X');

    CHECK_EQ("len", s.len, 10);
    CHECK_FS2VALID(1, &s);

    for (size_t i = 0; i < s.len; i++) {
        CHECK_EQ("fallback char", s.v[i], '?');
    }

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, genrnd_zero_len) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("initial");

    fs2_genrnd(&s, 0, 'a');

    CHECK_EQ("len", s.len, 0);
    CHECK_STR_EQ("empty", s.v, "");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, genrnd_overwrite_existing) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("old_string");

    /* Generate 5 characters over the old string */
    fs2_genrnd(&s, 5, 'a');

    CHECK_EQ("len", s.len, 5);
    CHECK_FS2VALID(1, &s);

    for (size_t i = 0; i < s.len; i++) {
        CHECK_TRUE("is lowercase", s.v[i] >= 'a' && s.v[i] <= 'z');
    }

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_genrnd_new — value constructor
 * ========================================================================= */

Test(fs2_api, genrnd_new_basic) {
    __fs2_checkreset();

    fs2 res = fs2_genrnd_new(100, 'a');

    CHECK_EQ("len", res.len, 100);
    CHECK_FS2VALID(1, &res);

    for (size_t i = 0; i < res.len; i++) {
        CHECK_TRUE("is lowercase", res.v[i] >= 'a' && res.v[i] <= 'z');
    }

    fs2_free(&res);
    assert_balanced();
}

/* =========================================================================
 * fs2_heap_genrnd — heap constructor
 * ========================================================================= */

Test(fs2_api, heap_genrnd_basic) {
    __fs2_checkreset();

    fs2 *res = fs2_heap_genrnd(100, '0');

    CHECK_EQ("len", res->len, 100);
    CHECK_TRUE("is bodyalloc", fs2_is_bodyalloc(res));
    CHECK_FS2VALID(1, res);

    for (size_t i = 0; i < res->len; i++) {
        CHECK_TRUE("is digit", res->v[i] >= '0' && res->v[i] <= '9');
    }

    fs2_free(res);
    assert_balanced();
}

Test(fs2_api, genrnd_bounds_regression) {
    __fs2_checkreset();
    fs2 s = FS2();

    /* 
     * Generate many characters. If rndint(26) returns 26,
     * we get '{', and the test will fail.
     */
    fs2_genrnd(&s, 1000, 'a');

    CHECK_EQ("len", s.len, 1000);
    for (size_t i = 0; i < s.len; i++) {
        CHECK_TRUE("lowercase strictly a-z", s.v[i] >= 'a' && s.v[i] <= 'z');
    }

    fs2_genrnd(&s, 1000, '0');
    for (size_t i = 0; i < s.len; i++) {
        CHECK_TRUE("digits strictly 0-9", s.v[i] >= '0' && s.v[i] <= '9');
    }

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_in / fs2_in_ci (case-sensitive / insensitive match)
 * ========================================================================= */

Test(fs2_api, in_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("apple");
    const char *list[] = { "banana", "apple", "cherry", NULL };

    CHECK_TRUE("found in list", fs2_in(&s, list));
    CHECK_FALSE("not found", fs2_not_in(&s, list));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, in_not_found) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("grape");
    const char *list[] = { "banana", "apple", "cherry", NULL };

    CHECK_FALSE("not in list", fs2_in(&s, list));
    CHECK_TRUE("notin true", fs2_not_in(&s, list));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, iin_case_insensitive) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("APPLE");
    const char *list[] = { "banana", "apple", "cherry", NULL };

    /* Plain in will not find (APPLE != apple) */
    CHECK_FALSE("case-sensitive miss", fs2_in(&s, list));
    /* iin must find it */
    CHECK_TRUE("case-insensitive hit", fs2_in_ci(&s, list));
    CHECK_FALSE("case-insensitive notin", fs2_not_in_ci(&s, list));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, in_canonical_empty) {
    __fs2_checkreset();
    fs2 s = FS2(); // v = NULL
    const char *list[] = { "", "a", NULL };

    /* Must not crash with a Segfault! */
    CHECK_TRUE("empty matches empty", fs2_in(&s, list));
    CHECK_FALSE("empty notin", fs2_not_in(&s, list));

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_anyof — index-returning base
 * ========================================================================= */

Test(fs2_api, anyof_returns_index) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("banana");
    const char *list[] = { "apple", "banana", "cherry", NULL };

    CHECK_EQ("found at index 1", fs2_anyof(&s, list, strcmp), 1);
    CHECK_EQ("case-insensitive at index 1", fs2_anyof(&s, list, strcasecmp), 1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, anyof_not_found) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("grape");
    const char *list[] = { "apple", "banana", "cherry", NULL };

    CHECK_EQ("missing returns -1", fs2_anyof(&s, list, strcmp), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, anyof_empty_list) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("anything");
    const char *list[] = { NULL };

    CHECK_EQ("empty list returns -1", fs2_anyof(&s, list, strcmp), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, anyof_canonical_empty) {
    __fs2_checkreset();
    fs2 s = FS2(); // v = NULL
    const char *list[] = { "a", "", "b", NULL };

    /* Empty string is at index 1 */
    CHECK_EQ("empty matches at index 1", fs2_anyof(&s, list, strcmp), 1);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * Boolean wrappers (regression check)
 * ========================================================================= */

Test(fs2_api, in_wrappers) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("APPLE");
    const char *list[] = { "apple", "banana", NULL };

    /* case-sensitive: does not find APPLE */
    CHECK_FALSE("in miss", fs2_in(&s, list));
    CHECK_TRUE("notin hit", fs2_not_in(&s, list));

    /* case-insensitive: finds it */
    CHECK_TRUE ("iin hit", fs2_in_ci(&s, list));
    CHECK_FALSE("not_iin miss", fs2_not_in_ci(&s, list));

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * Macros (variadic arguments)
 * ========================================================================= */

Test(fs2_api, in_macro_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("banana");

    CHECK_TRUE("FS2_IN", FS2_IN(&s, "apple", "banana", "cherry"));
    CHECK_FALSE("FS2_IN miss", FS2_IN(&s, "apple", "grape"));
    
    CHECK_TRUE("FS2_NOT_IN", FS2_NOT_IN(&s, "apple", "grape"));
    CHECK_FALSE("FS2_NOT_IN miss", FS2_NOT_IN(&s, "apple", "banana"));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, iin_macro_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("Banana");

    CHECK_TRUE("fs2_in_ci", FS2_IN_CI(&s, "apple", "banana", "cherry"));
    CHECK_TRUE("FS2_NOT_IN_CI", FS2_NOT_IN_CI(&s, "apple", "grape"));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, in_macro_empty_list) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("anything");

    /* Pass only NULL (empty list) */
    CHECK_FALSE("empty list -> false", FS2_IN(&s, NULL));
    CHECK_TRUE("empty list -> notin true", FS2_NOT_IN(&s, NULL));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, in_canonical_empty_matches_empty_string) {
    __fs2_checkreset();
    fs2 s = FS2(); // v = NULL, len = 0

    /* If the list does not have "", then FS2() is not there */
    CHECK_FALSE("empty not in [a, b]", FS2_IN(&s, "a", "b"));
    CHECK_TRUE ("empty notin [a, b]", FS2_NOT_IN(&s, "a", "b"));

    /* If the list has "", then FS2() is there */
    CHECK_TRUE ("empty in [a, \"\"]", FS2_IN(&s, "a", ""));
    CHECK_FALSE("empty notin [a, \"\"]", FS2_NOT_IN(&s, "a", ""));

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * Binary Search Finders (index-returning)
 * ========================================================================= */

Test(fs2_api, in_bsearch_fs2_array) {
    __fs2_checkreset();
    
    fs2 arr[4] = {
        fs2_fromstr("apple"),
        fs2_fromstr("banana"),
        fs2_fromstr("cherry"),
        fs2_fromstr("date")
    };

    fs2 k1 = fs2_fromstr("banana");
    fs2 k2 = fs2_fromstr("cherry");
    fs2 k3 = fs2_fromstr("zucchini"); // Largest of all
    fs2 k4 = fs2_fromstr("a");        // Smallest of all

    CHECK_EQ("banana index", fs2_bsearch(&k1, arr, 4), 1);
    CHECK_EQ("cherry index", fs2_bsearch(&k2, arr, 4), 2);
    CHECK_EQ("missing large", fs2_bsearch(&k3, arr, 4), -1);
    CHECK_EQ("missing small", fs2_bsearch(&k4, arr, 4), -1);

    for (int i = 0; i < 4; i++) fs2_free(&arr[i]);
    fs2_free(&k1); fs2_free(&k2); fs2_free(&k3); fs2_free(&k4);
    assert_balanced();
}

Test(fs2_api, in_bsearch_str_array) {
    __fs2_checkreset();
    
    const char *arr[] = { "apple", "banana", "cherry", "date", "elderberry" };
    size_t n = sizeof(arr) / sizeof(arr[0]);

    fs2 k1 = fs2_fromstr("apple");      // Left edge (index 0)
    fs2 k2 = fs2_fromstr("elderberry"); // Right edge (index 4)
    fs2 k3 = fs2_fromstr("avocado");    // Miss

    CHECK_EQ("left boundary", fs2_bsearch_str(&k1, arr, n), 0);
    CHECK_EQ("right boundary", fs2_bsearch_str(&k2, arr, n), 4);
    CHECK_EQ("in-between miss", fs2_bsearch_str(&k3, arr, n), -1);

    fs2_free(&k1); fs2_free(&k2); fs2_free(&k3);
    assert_balanced();
}

Test(fs2_api, in_bsearch_canonical_empty) {
    __fs2_checkreset();
    
    /* Array contains an empty string (FS2EMPTY) at position 0 */
    fs2 arr[3] = { FS2EMPTY, fs2_fromstr("a"), fs2_fromstr("b") };
    fs2 key_empty = FS2EMPTY;
    fs2 key_a = fs2_fromstr("a");

    CHECK_EQ("empty matches empty at 0", fs2_bsearch(&key_empty, arr, 3), 0);
    CHECK_EQ("a at 1", fs2_bsearch(&key_a, arr, 3), 1);

    fs2_free(&arr[1]);
    fs2_free(&arr[2]);
    fs2_free(&key_empty);
    fs2_free(&key_a);
    assert_balanced();
}

Test(fs2_api, iin_bsearch_case_insensitive) {
    __fs2_checkreset();
    
    /* Sorted case-insensitively for iin_bsearch */
    fs2 arr[3] = {
        fs2_fromstr("Apple"),
        fs2_fromstr("banana"),
        fs2_fromstr("Cherry")
    };

    fs2 k1 = fs2_fromstr("cherry"); // Lowercase
    fs2 k2 = fs2_fromstr("BANANA"); // Uppercase

    CHECK_EQ("miss case-sensitive", fs2_bsearch(&k1, arr, 3), -1);
    CHECK_EQ("hit case-insens cherry", fs2_bsearch_ci(&k1, arr, 3), 2);
    CHECK_EQ("hit case-insens banana", fs2_bsearch_ci(&k2, arr, 3), 1);

    for (int i = 0; i < 3; i++) fs2_free(&arr[i]);
    fs2_free(&k1); fs2_free(&k2);
    assert_balanced();
}

Test(fs2_api, in_bsearch_empty_array) {
    __fs2_checkreset();
    fs2 key = fs2_fromstr("anything");
    
    CHECK_EQ("n=0", fs2_bsearch(&key, NULL, 0), -1);
    CHECK_EQ("n=0 str", fs2_bsearch_str(&key, NULL, 0), -1);

    fs2_free(&key);
    assert_balanced();
}


/* =========================================================================
 * fs2_pos_parse_uint / ulong / ullong
 * ========================================================================= */

Test(fs2_api, pos_parse_uint_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42abc");
    unsigned out = 0;
    size_t newpos = 0;

    CHECK_TRUE("parse", fs2_pos_parse_uint(&s, 0, &newpos, &out));
    CHECK_EQ("val", out, 42u);
    CHECK_EQ("newpos", newpos, 2);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_uint_max) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("4294967295");   /* UINT_MAX */
    unsigned out = 0;

    CHECK_TRUE("UINT_MAX ok", fs2_pos_parse_uint(&s, 0, NULL, &out));
    CHECK_EQ("val", out, UINT_MAX);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_uint_overflow) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("4294967296");   /* UINT_MAX + 1 */
    unsigned out = 0;

    CHECK_FALSE("overflow", fs2_pos_parse_uint(&s, 0, NULL, &out));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_uint_rejects_negative) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("-1");
    unsigned out = 0;

    /* Key check: strtoull("-1") would return ULLONG_MAX,
     * but we explicitly reject the minus */
    CHECK_FALSE("negative rejected", fs2_pos_parse_uint(&s, 0, NULL, &out));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_uint_accepts_plus) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("+42");
    unsigned out = 0;

    CHECK_TRUE("plus ok", fs2_pos_parse_uint(&s, 0, NULL, &out));
    CHECK_EQ("val", out, 42u);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ulong_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("12345678");
    unsigned long out = 0;

    CHECK_TRUE("parse", fs2_pos_parse_ulong(&s, 0, NULL, &out));
    CHECK_EQ("val", out, 12345678ul);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ulong_max) {
    __fs2_checkreset();
    
    char buf[32];
    snprintf(buf, sizeof(buf), "%lu", ULONG_MAX);
    fs2 s = fs2_fromstr(buf);
    unsigned long out = 0;

    CHECK_TRUE("ULONG_MAX ok", fs2_pos_parse_ulong(&s, 0, NULL, &out));
    CHECK_EQ("val", out, ULONG_MAX);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ulong_rejects_beyond_uint) {
    __fs2_checkreset();
    /* On LP64: a value that fits in ulong but not in uint */
    fs2 s = fs2_fromstr("5000000000");   /* > UINT_MAX */
    unsigned out = 0;
    unsigned long ul_out = 0;

    CHECK_FALSE("doesn't fit uint",  fs2_pos_parse_uint (&s, 0, NULL, &out));
    CHECK_TRUE ("fits ulong on LP64", fs2_pos_parse_ulong(&s, 0, NULL, &ul_out));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ullong_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("18446744073709551615");   /* ULLONG_MAX */
    unsigned long long out = 0;

    CHECK_TRUE("parse", fs2_pos_parse_ullong(&s, 0, NULL, &out));
    CHECK_EQ("val", out, ULLONG_MAX);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ullong_overflow) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("18446744073709551616");   /* ULLONG_MAX + 1 */
    unsigned long long out = 0;

    CHECK_FALSE("overflow", fs2_pos_parse_ullong(&s, 0, NULL, &out));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ullong_out_null) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("999abc");
    size_t newpos = 0;

    /* out == NULL — validation only, value not needed */
    CHECK_TRUE("valid prefix", fs2_pos_parse_ullong(&s, 0, &newpos, NULL));
    CHECK_EQ("newpos", newpos, 3);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ullong_skips_spaces_and_tabs) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("  \t42");
    unsigned long long out = 0;
    size_t newpos = 0;

    CHECK_TRUE("parse", fs2_pos_parse_ullong(&s, 0, &newpos, &out));
    CHECK_EQ("val", out, 42ull);
    CHECK_EQ("newpos", newpos, 5);   /* 2 sp + 1 tab + 2 digits */

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_ullong_rejects_newline) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("\n42");
    unsigned long long out = 0;

    /* strtoull would skip \n on its own — we reject before the call */
    CHECK_FALSE("newline rejected", fs2_pos_parse_ullong(&s, 0, NULL, &out));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_uint_chain_range) {
    __fs2_checkreset();
    /* Each function rejects values outside its range */
    fs2 s_u  = fs2_fromstr("4294967296");          /* UINT_MAX + 1 */
    fs2 s_ul = fs2_fromstr("18446744073709551615"); /* ULONG_MAX on LP64 */
    fs2 s_ull = fs2_fromstr("18446744073709551615"); /* ULLONG_MAX */

    unsigned           u  = 0;
    unsigned long      ul = 0;
    unsigned long long ull = 0;

    CHECK_FALSE("uint rejects",  fs2_pos_parse_uint  (&s_u,  0, NULL, &u));
    CHECK_TRUE ("ulong fits on LP64", fs2_pos_parse_ulong (&s_ul, 0, NULL, &ul));
    CHECK_TRUE ("ullong fits",   fs2_pos_parse_ullong(&s_ull, 0, NULL, &ull));

    fs2_free(&s_u);
    fs2_free(&s_ul);
    fs2_free(&s_ull);
    assert_balanced();
}

Test(fs2_api, tryparse_ullong_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("12345");
    unsigned long long out = 0;

    CHECK_TRUE("parse", fs2_tryparse_ullong(&s, &out, true));
    CHECK_EQ("val", out, 12345ULL);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, tryparse_ullong_max) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("18446744073709551615");   /* ULLONG_MAX */
    unsigned long long out = 0;

    CHECK_TRUE("ULLONG_MAX", fs2_tryparse_ullong(&s, &out, true));
    CHECK_EQ("val", out, ULLONG_MAX);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, tryparse_ullong_rejects_negative) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("-1");
    unsigned long long out = 0;

    /* Key: strtoull("-1") = ULLONG_MAX, but we reject the minus 
     * at the pos_parse_ullong level */
    CHECK_FALSE("negative rejected", fs2_tryparse_ullong(&s, &out, true));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, tryparse_ullong_strict_vs_lenient) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42xyz");
    unsigned long long out = 0;

    CHECK_TRUE ("lenient allows garbage", fs2_tryparse_ullong(&s, &out, false));
    CHECK_EQ   ("val", out, 42ULL);
    CHECK_FALSE("strict rejects",         fs2_tryparse_ullong(&s, &out, true));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, tryparse_ullong_overflow) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("18446744073709551616");   /* ULLONG_MAX + 1 */
    unsigned long long out = 0;

    CHECK_FALSE("overflow", fs2_tryparse_ullong(&s, &out, true));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, parse_ullong_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("18446744073709551615");

    CHECK_EQ("parsed", fs2_parse_ullong(&s), ULLONG_MAX);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, parse_ullong_fails_userraise, .signal = SIGINT) {
    fs2 s = fs2_fromstr("abc");
    fs2_parse_ullong(&s);
    fs2_free(&s);
}

/* =========================================================================
 * fs2_tryparse_uint / fs2_parse_uint
 * ========================================================================= */

Test(fs2_api, tryparse_uint_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("12345");
    unsigned out = 0;

    CHECK_TRUE("parse", fs2_tryparse_uint(&s, &out, true));
    CHECK_EQ("val", out, 12345u);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, tryparse_uint_max) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("4294967295");   /* UINT_MAX */
    unsigned out = 0;

    CHECK_TRUE("UINT_MAX", fs2_tryparse_uint(&s, &out, true));
    CHECK_EQ("val", out, UINT_MAX);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, tryparse_uint_overflow) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("4294967296");   /* UINT_MAX + 1 */
    unsigned out = 0;

    CHECK_FALSE("overflow", fs2_tryparse_uint(&s, &out, true));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, tryparse_uint_rejects_negative) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("-1");
    unsigned out = 0;

    /* Chain: uint → ulong → ullong, minus is rejected at the lowest level */
    CHECK_FALSE("negative rejected", fs2_tryparse_uint(&s, &out, true));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, tryparse_uint_accepts_plus) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("+42");
    unsigned out = 0;

    CHECK_TRUE("plus ok", fs2_tryparse_uint(&s, &out, true));
    CHECK_EQ("val", out, 42u);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, tryparse_uint_strict_vs_lenient) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42xyz");
    unsigned out = 0;

    CHECK_TRUE ("lenient", fs2_tryparse_uint(&s, &out, false));
    CHECK_EQ   ("val", out, 42u);
    CHECK_FALSE("strict rejects", fs2_tryparse_uint(&s, &out, true));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, parse_uint_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("2024");

    CHECK_EQ("parsed", fs2_parse_uint(&s), 2024u);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, parse_uint_max) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("4294967295");

    CHECK_EQ("UINT_MAX", fs2_parse_uint(&s), UINT_MAX);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, parse_uint_fails_userraise, .signal = SIGINT) {
    fs2 s = fs2_fromstr("abc");
    fs2_parse_uint(&s);
    fs2_free(&s);
}

Test(fs2_api, parse_uint_overflow_aborts, .signal = SIGINT) {
    fs2 s = fs2_fromstr("4294967296");   /* UINT_MAX + 1 */
    fs2_parse_uint(&s);
    fs2_free(&s);
}

/* =========================================================================
 * Chain uint → ulong → ullong: one value, three levels
 * ========================================================================= */

Test(fs2_api, uint_chain_rejects_above_uint) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("5000000000");   /* > UINT_MAX, < ULONG_MAX on LP64 */

    unsigned           u   = 0;
    unsigned long      ul  = 0;
    unsigned long long ull = 0;

    CHECK_FALSE("uint rejects",   fs2_tryparse_uint  (&s, &u,   true));
    CHECK_TRUE ("ulong accepts",  fs2_tryparse_ulong (&s, &ul,  true));
    CHECK_EQ   ("ulong val",      ul, 5000000000ul);
    CHECK_TRUE ("ullong accepts", fs2_tryparse_ullong(&s, &ull, true));
    CHECK_EQ   ("ullong val",     ull, 5000000000ull);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, uint_chain_all_reject_beyond_ullong) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("99999999999999999999");   /* > ULLONG_MAX */

    unsigned           u   = 0;
    unsigned long      ul  = 0;
    unsigned long long ull = 0;

    CHECK_FALSE("uint rejects",   fs2_tryparse_uint  (&s, &u,   true));
    CHECK_FALSE("ulong rejects",  fs2_tryparse_ulong (&s, &ul,  true));
    CHECK_FALSE("ullong rejects", fs2_tryparse_ullong(&s, &ull, true));

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * Signed vs unsigned: the same input
 * ========================================================================= */

Test(fs2_api, parse_signed_vs_unsigned_int) {
    __fs2_checkreset();

    fs2 s_pos = fs2_fromstr("100");
    fs2 s_neg = fs2_fromstr("-100");

    /* Positive: both handle it */
    CHECK_EQ("signed int 100",   fs2_parse_int (&s_pos), 100);
    CHECK_EQ("unsigned int 100", fs2_parse_uint(&s_pos), 100u);

    /* Negative: int ok, uint — no */
    CHECK_EQ("signed int -100", fs2_parse_int(&s_neg), -100);

    unsigned u_out = 0;
    CHECK_FALSE("uint rejects -100", fs2_tryparse_uint(&s_neg, &u_out, true));

    fs2_free(&s_pos);
    fs2_free(&s_neg);
    assert_balanced();
}

/* =========================================================================
 * Composite: uint in real code
 * ========================================================================= */

Test(fs2_api, composite_parse_uint_port) {
    __fs2_checkreset();

    /* Scenario: "port=8080" from a config */
    fs2 line = fs2_fromstr("port=8080");

    ptrdiff_t eq = fs2_chr(&line, '=');
    CHECK_TRUE("eq found", eq >= 0);

    size_t   newpos = 0;
    unsigned port   = 0;
    CHECK_TRUE("parse port", 
               fs2_pos_parse_uint(&line, (size_t)eq + 1, &newpos, &port));
    CHECK_EQ("port", port, 8080u);
    CHECK_EQ("consumed all", newpos, line.len);
    CHECK_TRUE("fits uint", port <= 65535u);

    fs2_free(&line);
    assert_balanced();
}

Test(fs2_api, composite_parse_uint_with_validation) {
    __fs2_checkreset();

    /* Parse and validate range — a typical scenario */
    fs2 s = fs2_fromstr("512");
    unsigned val = 0;

    CHECK_TRUE("parsed", fs2_tryparse_uint(&s, &val, true));
    CHECK_TRUE("in range [1, 1024]", val >= 1u && val <= 1024u);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * Symmetry: signed vs unsigned on one input
 * ========================================================================= */

Test(fs2_api, parse_signed_vs_unsigned) {
    __fs2_checkreset();
    
    fs2 s_pos = fs2_fromstr("100");
    fs2 s_neg = fs2_fromstr("-100");

    /* Positive — both handle it */
    CHECK_EQ("signed 100",   fs2_parse_llong (&s_pos), 100LL);
    CHECK_EQ("unsigned 100", fs2_parse_ullong(&s_pos), 100ULL);

    /* Negative — signed ok, unsigned fails */
    CHECK_EQ("signed -100", fs2_parse_llong(&s_neg), -100LL);
    /* fs2_parse_ullong(&s_neg) → SIGINT; check via tryparse */
    unsigned long long u_out = 0;
    CHECK_FALSE("unsigned rejects -100", fs2_tryparse_ullong(&s_neg, &u_out, true));

    fs2_free(&s_pos);
    fs2_free(&s_neg);
    assert_balanced();
}

/* =========================================================================
 * fs2_tryparse_ulong / fs2_parse_ulong
 * ========================================================================= */

Test(fs2_api, tryparse_ulong_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("12345");
    unsigned long out = 0;

    CHECK_TRUE("parse", fs2_tryparse_ulong(&s, &out, true));
    CHECK_EQ("val", out, 12345ul);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, tryparse_ulong_max) {
    __fs2_checkreset();

    char buf[32];
    snprintf(buf, sizeof(buf), "%lu", ULONG_MAX);
    fs2 s = fs2_fromstr(buf);
    unsigned long out = 0;

    CHECK_TRUE("ULONG_MAX", fs2_tryparse_ulong(&s, &out, true));
    CHECK_EQ("val", out, ULONG_MAX);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, tryparse_ulong_rejects_negative) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("-1");
    unsigned long out = 0;

    /* strtoull("-1") = ULLONG_MAX, but pos_parse_ulong rejects the minus */
    CHECK_FALSE("negative rejected", fs2_tryparse_ulong(&s, &out, true));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, tryparse_ulong_strict_vs_lenient) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42xyz");
    unsigned long out = 0;

    CHECK_TRUE ("lenient", fs2_tryparse_ulong(&s, &out, false));
    CHECK_EQ   ("val", out, 42ul);
    CHECK_FALSE("strict rejects", fs2_tryparse_ulong(&s, &out, true));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, tryparse_ulong_overflow) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("99999999999999999999999");   /* >> ULONG_MAX */
    unsigned long out = 0;

    CHECK_FALSE("overflow", fs2_tryparse_ulong(&s, &out, true));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, tryparse_ulong_beyond_uint) {
    __fs2_checkreset();
    /* 5e9 — fits in ulong (LP64) but not in uint.
     * Check it is not truncated at the ulong level. */
    fs2 s = fs2_fromstr("5000000000");

    unsigned           u_out = 0;
    unsigned long     ul_out = 0;

    CHECK_FALSE("uint rejects",  fs2_tryparse_uint (&s, &u_out,  true));
    CHECK_TRUE ("ulong accepts", fs2_tryparse_ulong(&s, &ul_out, true));
    CHECK_EQ   ("val", ul_out, 5000000000ul);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, parse_ulong_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("12345678");

    CHECK_EQ("parsed", fs2_parse_ulong(&s), 12345678ul);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, parse_ulong_max) {
    __fs2_checkreset();

    char buf[32];
    snprintf(buf, sizeof(buf), "%lu", ULONG_MAX);
    fs2 s = fs2_fromstr(buf);

    CHECK_EQ("ULONG_MAX", fs2_parse_ulong(&s), ULONG_MAX);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, parse_ulong_fails_userraise, .signal = SIGINT) {
    fs2 s = fs2_fromstr("abc");
    fs2_parse_ulong(&s);
    fs2_free(&s);
}

Test(fs2_api, parse_ulong_negative_aborts, .signal = SIGINT) {
    fs2 s = fs2_fromstr("-5");
    fs2_parse_ulong(&s);
    fs2_free(&s);
}

/* =========================================================================
 * Symmetry: ulong vs ullong on different values
 * ========================================================================= */

Test(fs2_api, parse_ulong_vs_ullong_symmetry) {
    __fs2_checkreset();

    /* Ordinary positive — both handle it */
    fs2 s_small = fs2_fromstr("42");
    CHECK_EQ("ulong 42",  fs2_parse_ulong (&s_small), 42ul);
    CHECK_EQ("ullong 42", fs2_parse_ullong(&s_small), 42ull);

    /* ULLONG_MAX — equals ULONG_MAX on LP64,
     * but ulong is smaller on LLP64. Check round-trip. */
    char buf[32];
    snprintf(buf, sizeof(buf), "%llu", ULLONG_MAX);
    fs2 s_max = fs2_fromstr(buf);

    CHECK_EQ("ullong ULLONG_MAX", fs2_parse_ullong(&s_max), ULLONG_MAX);

    /* ulong accepts ULLONG_MAX only on LP64 (where they are the same size) */
    #if ULONG_MAX == ULLONG_MAX
        CHECK_EQ("ulong ULLONG_MAX", fs2_parse_ulong(&s_max), ULONG_MAX);
    #else
        unsigned long ul = 0;
        CHECK_FALSE("ulong rejects ULLONG_MAX on LLP64",
                    fs2_tryparse_ulong(&s_max, &ul, true));
    #endif

    fs2_free(&s_small);
    fs2_free(&s_max);
    assert_balanced();
}

/* =========================================================================
 * Composite: parsing a number from real text
 * ========================================================================= */

Test(fs2_api, composite_parse_ulong_from_config) {
    __fs2_checkreset();

    /* Typical scenario: "timeout=30" — parse the number after '=' */
    fs2 line = fs2_fromstr("timeout=30");

    /* Look for '=' */
    ptrdiff_t eq = fs2_chr(&line, '=');
    CHECK_TRUE("eq found", eq >= 0);

    /* Parse from position eq+1 */
    size_t newpos = 0;
    unsigned long timeout = 0;
    CHECK_TRUE("parse", fs2_pos_parse_ulong(&line, (size_t)eq + 1, &newpos, &timeout));
    CHECK_EQ("timeout", timeout, 30ul);
    CHECK_EQ("newpos", newpos, line.len);   /* consumed to the end */

    fs2_free(&line);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_llong (base)
 * ========================================================================= */

Test(fs2_api, pos_parse_llong_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42abc");
    long long out = 0;
    size_t newpos = 0;

    CHECK_TRUE("parse", fs2_pos_parse_llong(&s, 0, &newpos, &out));
    CHECK_EQ("val", out, 42LL);
    CHECK_EQ("newpos", newpos, 2);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_llong_boundaries) {
    __fs2_checkreset();

    char buf[32];
    snprintf(buf, sizeof(buf), "%lld", LLONG_MAX);
    fs2 s_max = fs2_fromstr(buf);

    snprintf(buf, sizeof(buf), "%lld", LLONG_MIN);
    fs2 s_min = fs2_fromstr(buf);

    long long out = 0;

    CHECK_TRUE("LLONG_MAX", fs2_pos_parse_llong(&s_max, 0, NULL, &out));
    CHECK_EQ("max val", out, LLONG_MAX);

    CHECK_TRUE("LLONG_MIN", fs2_pos_parse_llong(&s_min, 0, NULL, &out));
    CHECK_EQ("min val", out, LLONG_MIN);

    fs2_free(&s_max);
    fs2_free(&s_min);
    assert_balanced();
}

Test(fs2_api, pos_parse_llong_overflow) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("99999999999999999999999");   /* >> LLONG_MAX */
    long long out = 0;

    CHECK_FALSE("overflow", fs2_pos_parse_llong(&s, 0, NULL, &out));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_llong_rejects_newline) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("\n42");
    long long out = 0;

    CHECK_FALSE("newline rejected", fs2_pos_parse_llong(&s, 0, NULL, &out));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, tryparse_llong_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("12345");
    long long out = 0;

    CHECK_TRUE("parse", fs2_tryparse_llong(&s, &out, true));
    CHECK_EQ("val", out, 12345LL);

    fs2_free(&s);
    assert_balanced();
}

/* ⚠ Key regression: value out of long range — if inside _long, will crash here */
Test(fs2_api, tryparse_llong_beyond_long_on_llp64) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("9223372036854775807");   /* LLONG_MAX */
    long long out = 0;

    CHECK_TRUE("LLONG_MAX parses", fs2_tryparse_llong(&s, &out, true));
    CHECK_EQ("val", out, LLONG_MAX);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, tryparse_llong_strict_trailing_reject) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42abc");
    long long out = 0;

    CHECK_TRUE ("non-strict ok", fs2_tryparse_llong(&s, &out, false));
    CHECK_FALSE("strict rejects", fs2_tryparse_llong(&s, &out, true));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, parse_llong_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("5000000000");   /* > INT_MAX */

    CHECK_EQ("parsed", fs2_parse_llong(&s), 5000000000LL);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, parse_llong_fails_userraise, .signal = SIGINT) {
    fs2 s = fs2_fromstr("abc");
    fs2_parse_llong(&s);
    fs2_free(&s);
}

/* =========================================================================
 * fs2_pos_parse_long / fs2_pos_parse_int
 * ========================================================================= */

Test(fs2_api, pos_parse_long_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("1234567890123 456"); // big long
    long out = 0;
    size_t newpos = 0;

    CHECK_TRUE("parse long", fs2_pos_parse_long(&s, 0, &newpos, &out));
    CHECK_EQ("val", out, 1234567890123L);
    CHECK_EQ("newpos", newpos, 13);

    /* Parse the remainder */
    CHECK_TRUE("parse next", fs2_pos_parse_long(&s, 14, &newpos, &out));
    CHECK_EQ("val", out, 456);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_long_out_null) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42abc");
    size_t newpos = 0;

    /* Validate the prefix without storing the value */
    CHECK_TRUE("valid prefix", fs2_pos_parse_long(&s, 0, &newpos, NULL));
    CHECK_EQ("newpos", newpos, 2);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_int_overflow) {
    __fs2_checkreset();
    fs2 s1 = fs2_fromstr("2147483648");  // INT_MAX + 1
    fs2 s2 = fs2_fromstr("-2147483649"); // INT_MIN - 1
    int out = 0;

    CHECK_FALSE("int overflow +", fs2_pos_parse_int(&s1, 0, NULL, &out));
    CHECK_FALSE("int overflow -", fs2_pos_parse_int(&s2, 0, NULL, &out));

    fs2_free(&s1); fs2_free(&s2);
    assert_balanced();
}

/* =========================================================================
 * Regression: each level's error propagates
 * ========================================================================= */

Test(fs2_api, pos_parse_chain_range_check) {
    __fs2_checkreset();
    
    /* Same value — different results at different levels.
     * 3000000000 fits in long (LP64) but not in int. */
    fs2 s = fs2_fromstr("3000000000");

    int  i_val = 0;
    long l_val = 0;
    long long ll_val = 0;

    CHECK_FALSE("int rejects",   fs2_pos_parse_int  (&s, 0, NULL, &i_val));
    CHECK_TRUE ("long accepts",  fs2_pos_parse_long (&s, 0, NULL, &l_val));
    CHECK_EQ   ("long val",      l_val, 3000000000L);
    CHECK_TRUE ("llong accepts", fs2_pos_parse_llong(&s, 0, NULL, &ll_val));
    CHECK_EQ   ("llong val",     ll_val, 3000000000LL);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_llong_rejects_beyond_llong) {
    __fs2_checkreset();
    
    /* "99999999999999999999" — 20 nines, > LLONG_MAX (~9.2e18) */
    fs2 s = fs2_fromstr("99999999999999999999");

    int       i_val  = 0;
    long      l_val  = 0;
    long long ll_val = 0;

    /* All three must reject — error at the lowest level */
    CHECK_FALSE("int rejects",   fs2_pos_parse_int  (&s, 0, NULL, &i_val));
    CHECK_FALSE("long rejects",  fs2_pos_parse_long (&s, 0, NULL, &l_val));
    CHECK_FALSE("llong rejects", fs2_pos_parse_llong(&s, 0, NULL, &ll_val));

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_tryparse_long / fs2_tryparse_int (strict mode)
 * ========================================================================= */

Test(fs2_api, tryparse_strict_exact) {
    __fs2_checkreset();
    fs2 s1 = fs2_fromstr("42");
    fs2 s2 = fs2_fromstr("-42");
    int out = 0;

    CHECK_TRUE("clean int", fs2_tryparse_int(&s1, &out, true));
    CHECK_EQ("val", out, 42);

    CHECK_TRUE("clean negative", fs2_tryparse_int(&s2, &out, true));
    CHECK_EQ("val", out, -42);

    fs2_free(&s1); fs2_free(&s2);
    assert_balanced();
}

Test(fs2_api, tryparse_strict_rejects_trailing) {
    __fs2_checkreset();
    fs2 s1 = fs2_fromstr("42abc");
    fs2 s2 = fs2_fromstr("42   "); // Even spaces are forbidden in strict mode!
    int out = 0;

    CHECK_FALSE("trailing garbage", fs2_tryparse_int(&s1, &out, true));
    CHECK_FALSE("trailing spaces", fs2_tryparse_int(&s2, &out, true));

    fs2_free(&s1); fs2_free(&s2);
    assert_balanced();
}

Test(fs2_api, tryparse_non_strict_prefix) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42abc");
    int out = 0;

    CHECK_TRUE("non-strict allows garbage", fs2_tryparse_int(&s, &out, false));
    CHECK_EQ("val", out, 42);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_parse_int / fs2_parse_long (raises on failure)
 * ========================================================================= */

Test(fs2_api, parse_long_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("1234567890123");
    
    CHECK_EQ("parsed", fs2_parse_long(&s), 1234567890123L);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, parse_int_fails_userraise, .signal = SIGINT) {
    /* "abc42" — digits start only at the 4th character.
     * Lenient mode also will not find a number at the start, userraise -> SIGINT. */
    fs2 s = fs2_fromstr("abc42");
    fs2_parse_int(&s);
    fs2_free(&s);
}

/* =========================================================================
 * Whitespace handling
 * ========================================================================= */

Test(fs2_api, pos_parse_long_leading_spaces) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("  \t42abc");
    long out = 0;
    size_t newpos = 0;

    CHECK_TRUE("skip spaces/tabs", fs2_pos_parse_long(&s, 0, &newpos, &out));
    CHECK_EQ("val", out, 42);
    /* skipped = 3 (' ',' ','\t'), num_len = 2 ('4','2'), newpos = 0 + 3 + 2 = 5 */
    CHECK_EQ("newpos after number", newpos, 5);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_long_rejects_newlines) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("\n42");  // \n is NOT skipped by skip_spaces
    long out = 0;

    /* Should fail because \n is not a space/tab */
    CHECK_FALSE("reject newline", fs2_pos_parse_long(&s, 0, NULL, &out));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, tryparse_strict_with_leading_spaces) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("  42"); // Leading spaces OK
    int out = 0;

    CHECK_TRUE("strict allows leading spaces", fs2_tryparse_int(&s, &out, true));
    CHECK_EQ("val", out, 42);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, tryparse_strict_rejects_trailing_spaces) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42  "); // Trailing spaces NOT OK
    int out = 0;

    CHECK_FALSE("strict rejects trailing spaces", fs2_tryparse_int(&s, &out, true));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_long_skips_only_spaces_and_tabs) {
    __fs2_checkreset();
    fs2 s1 = fs2_fromstr(" \t 123");
    fs2 s2 = fs2_fromstr(" \n 123"); // \n must not be skipped
    long out = 0;
    size_t newpos = 0;

    /* Spaces and tabs are skipped */
    CHECK_TRUE("spaces and tabs", fs2_pos_parse_long(&s1, 0, &newpos, &out));
    CHECK_EQ("val", out, 123);
    CHECK_EQ("newpos", newpos, 6); // 3 spaces/tabs + 3 digits

    /* \n is not skipped, so strtol will fail (endptr == start) */
    CHECK_FALSE("newline not skipped", fs2_pos_parse_long(&s2, 0, NULL, &out));

    fs2_free(&s1);
    fs2_free(&s2);
    assert_balanced();
}

/* =========================================================================
 * fs2_parse_int / fs2_parse_long (lenient mode)
 * ========================================================================= */

Test(fs2_api, parse_int_lenient_success) {
    __fs2_checkreset();
    
    fs2 s1 = fs2_fromstr("42");
    fs2 s2 = fs2_fromstr("  \t 42");
    fs2 s3 = fs2_fromstr("200 OK");      // HTTP-style
    fs2 s4 = fs2_fromstr("42abc");
    
    CHECK_EQ("clean", fs2_parse_int(&s1), 42);
    CHECK_EQ("leading spaces", fs2_parse_int(&s2), 42);
    CHECK_EQ("HTTP status", fs2_parse_int(&s3), 200);
    CHECK_EQ("trailing garbage", fs2_parse_int(&s4), 42);

    fs2_free(&s1); fs2_free(&s2); fs2_free(&s3); fs2_free(&s4);
    assert_balanced();
}

Test(fs2_api, parse_int_lenient_negative) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr(" -42 items");
    
    CHECK_EQ("negative with suffix", fs2_parse_int(&s), -42);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, parse_int_lenient_overflow_still_fails, .signal = SIGINT) {
    /* 
     * In lenient mode we still check if the number fits in int.
     * "999999999999" will not fit, so userraise is expected.
     */
    fs2 s = fs2_fromstr("999999999999");
    fs2_parse_int(&s);
    fs2_free(&s);
}

Test(fs2_api, parse_int_lenient_no_digits_fails, .signal = SIGINT) {
    /* Letters at the start — nothing to parse */
    fs2 s = fs2_fromstr("abc42");
    fs2_parse_int(&s);
    fs2_free(&s);
}

Test(fs2_api, parse_int_lenient_empty_fails, .signal = SIGINT) {
    fs2 s = fs2_fromstr("   ");
    fs2_parse_int(&s);
    fs2_free(&s);
}

Test(fs2_api, parse_long_lenient_success) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("1234567890123 bytes");
    
    CHECK_EQ("long prefix", fs2_parse_long(&s), 1234567890123L);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, parse_int_strict_rejects_trailing) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42   ");
    int out = 0;
    CHECK_FALSE("strict rejects trailing spaces", fs2_tryparse_int(&s, &out, true));
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, parse_long_strict_rejects_trailing) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("4255   ");
    long out = 0;
    CHECK_FALSE("strict rejects trailing spaces", fs2_tryparse_long(&s, &out, true));
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_serialize_int — format int into existing fs2
 * ========================================================================= */

 /* =========================================================================
 * Generic serialization macros
 * ========================================================================= */

Test(fs2_api, from_new_int) {
    __fs2_checkreset();
    fs2 s = fs2_from_new(42);
    CHECK_STR_EQ("int", s.v, "42");
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, from_new_long_and_llong) {
    __fs2_checkreset();
    fs2 s1 = fs2_from_new(5000000000L);
    fs2 s2 = fs2_from_new(LLONG_MIN);
    
    CHECK_STR_EQ("long", s1.v, "5000000000");
    CHECK_STR_EQ("llong", s2.v, "-9223372036854775808");
    CHECK_FS2VALID(1, &s1);
    CHECK_FS2VALID(2, &s2);
    
    fs2_free(&s1);
    fs2_free(&s2);
    assert_balanced();
}

Test(fs2_api, from_new_unsigned) {
    __fs2_checkreset();
    fs2 s = fs2_from_new(4'294'967'295U);
    CHECK_STR_EQ("unsigned", s.v, "4294967295");
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, from_new_float_double) {
    __fs2_checkreset();
    fs2 s1 = fs2_from_new(3.14f);  // float (will be promoted to double for printf)
    fs2 s2 = fs2_from_new(2.71828); // double
    
    CHECK_STR_EQ("float", s1.v, "3.140000");
    CHECK_STR_EQ("double", s2.v, "2.718280");
    CHECK_FS2VALID(1, &s1);
    CHECK_FS2VALID(2, &s2);
    
    fs2_free(&s1);
    fs2_free(&s2);
    assert_balanced();
}

Test(fs2_api, from_new_pointer) {
    __fs2_checkreset();
    int x = 42;
    void *ptr = &x;
    fs2 s = fs2_from_new(ptr);
    
    /* Verify the string starts with "0x" */
    CHECK_TRUE("starts with 0x", s.len > 2 && s.v[0] == '0' && s.v[1] == 'x');
    CHECK_FS2VALID(1, &s);
    
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, serialize_into_existing) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("overwrite_me");
    
    CHECK_TRUE("serialize int", fs2_serialize(&dst, 99) > 0);
    CHECK_STR_EQ("int content", dst.v, "99");
    
    CHECK_TRUE("serialize double", fs2_serialize(&dst, 1.5) > 0);
    CHECK_STR_EQ("double content", dst.v, "1.500000");
    
    CHECK_FS2VALID(1, &dst);
    fs2_free(&dst);
    assert_balanced();
}

Test(fs2_api, heap_from_via_generic) {
    __fs2_checkreset();
    
    /* Test that the macro returns fs2* and correctly uses stacktoheap */
    fs2 *h1 = fs2_heap_from(12'345L);
    fs2 *h2 = fs2_heap_from(3.14f);
    
    CHECK_STR_EQ("long heap", h1->v, "12345");
    CHECK_STR_EQ("float heap", h2->v, "3.140000");
    
    CHECK_TRUE("h1 bodyalloc", fs2_is_bodyalloc(h1));
    CHECK_TRUE("h2 bodyalloc", fs2_is_bodyalloc(h2));
    
    CHECK_FS2VALID(1, h1);
    CHECK_FS2VALID(2, h2);
    
    fs2_free(h1);
    fs2_free(h2);
    assert_balanced();  /* If stacktoheap inside the macro leaks, assert_balanced will catch it */
}

/* =========================================================================
 * fs2_serialize — write any primitive into existing fs2
 * ========================================================================= */

Test(fs2_api, serialize_int_basic) {
    __fs2_checkreset();
    fs2 dst = FS2();

    CHECK_EQ("42 → 2 chars", fs2_serialize(&dst, 42), 2);
    CHECK_STR_EQ("content", dst.v, "42");
    CHECK_EQ("len", dst.len, 2);
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2_api, serialize_int_negative) {
    __fs2_checkreset();
    fs2 dst = FS2();

    CHECK_EQ("-12345 → 6 chars", fs2_serialize(&dst, -12345), 6);
    CHECK_STR_EQ("content", dst.v, "-12345");
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2_api, serialize_int_boundaries) {
    __fs2_checkreset();

    fs2 s_max = FS2();
    fs2 s_min = FS2();

    fs2_serialize(&s_max, INT_MAX);
    fs2_serialize(&s_min, INT_MIN);

    CHECK_STR_EQ("INT_MAX", s_max.v, "2147483647");
    CHECK_STR_EQ("INT_MIN", s_min.v, "-2147483648");
    CHECK_FS2VALID(1, &s_max);
    CHECK_FS2VALID(2, &s_min);

    fs2_free(&s_max);
    fs2_free(&s_min);
    assert_balanced();
}

Test(fs2_api, serialize_long_beyond_int_range) {
    __fs2_checkreset();
    fs2 dst = FS2();

    /* Regression: if _Generic picked %d instead of %ld — truncation */
    fs2_serialize(&dst, 5'000'000'000L);

    CHECK_STR_EQ("content", dst.v, "5000000000");
    CHECK_EQ("len", dst.len, 10);
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2_api, serialize_llong_boundaries) {
    __fs2_checkreset();

    fs2 s_max = FS2();
    fs2 s_min = FS2();

    fs2_serialize(&s_max, LLONG_MAX);
    fs2_serialize(&s_min, LLONG_MIN);

    CHECK_STR_EQ("LLONG_MAX", s_max.v, "9223372036854775807");
    CHECK_STR_EQ("LLONG_MIN", s_min.v, "-9223372036854775808");
    CHECK_EQ("max len", s_max.len, 19);
    CHECK_EQ("min len", s_min.len, 20);
    CHECK_FS2VALID(1, &s_max);
    CHECK_FS2VALID(2, &s_min);

    fs2_free(&s_max);
    fs2_free(&s_min);
    assert_balanced();
}

Test(fs2_api, serialize_unsigned) {
    __fs2_checkreset();

    fs2 s_u   = FS2();
    fs2 s_ul  = FS2();
    fs2 s_ull = FS2();

    fs2_serialize(&s_u,   UINT_MAX);
    fs2_serialize(&s_ul,  ULONG_MAX);
    fs2_serialize(&s_ull, ULLONG_MAX);

    CHECK_STR_EQ("UINT_MAX", s_u.v, "4294967295");
    CHECK_STR_EQ("ULONG_MAX", s_ul.v, "18446744073709551615");
    CHECK_STR_EQ("ULLONG_MAX", s_ull.v, "18446744073709551615");
    CHECK_FS2VALID(1, &s_u);
    CHECK_FS2VALID(2, &s_ul);
    CHECK_FS2VALID(3, &s_ull);

    fs2_free(&s_u);
    fs2_free(&s_ul);
    fs2_free(&s_ull);
    assert_balanced();
}

Test(fs2_api, serialize_float_double) {
    __fs2_checkreset();

    fs2 s_f = FS2();
    fs2 s_d = FS2();

    fs2_serialize(&s_f, 3.14f);
    fs2_serialize(&s_d, 2.71828);

    /* %f gives 6 digits after the decimal point by default */
    CHECK_STR_EQ("float", s_f.v, "3.140000");
    CHECK_STR_EQ("double", s_d.v, "2.718280");
    CHECK_FS2VALID(1, &s_f);
    CHECK_FS2VALID(2, &s_d);

    fs2_free(&s_f);
    fs2_free(&s_d);
    assert_balanced();
}

Test(fs2_api, serialize_pointer) {
    __fs2_checkreset();
    int x = 42;
    void *ptr = &x;

    fs2 s = FS2();
    fs2_serialize(&s, ptr);

    /* %p prints a hex address "0x..." */
    CHECK_TRUE("starts with 0x", s.len > 2 && s.v[0] == '0' && s.v[1] == 'x');
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, serialize_overwrites_existing) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("long existing content");

    fs2_serialize(&dst, 7);

    CHECK_STR_EQ("content", dst.v, "7");
    CHECK_EQ("len", dst.len, 1);
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2_api, serialize_return_equals_len) {
    __fs2_checkreset();

    /* Return must always match len after writing */
    int values[] = { 0, 1, -1, 42, INT_MAX, INT_MIN };
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
        fs2 dst = FS2();
        long written = fs2_serialize(&dst, values[i]);

        CHECK_EQ("return == len", written, (long)dst.len);
        CHECK_EQ("strlen == len", strlen(dst.v), dst.len);

        fs2_free(&dst);
    }
    assert_balanced();
}

Test(fs2_api, serialize_null_aborts, .signal = SIGABRT) {
    fs2_serialize(NULL, 42);
}

/* =========================================================================
 * fs2_from_new — stack constructor
 * ========================================================================= */

Test(fs2_api, from_new_int2) {
    __fs2_checkreset();
    fs2 s = fs2_from_new(12345);

    CHECK_STR_EQ("content", s.v, "12345");
    CHECK_EQ("len", s.len, 5);
    CHECK_TRUE("is alloc", fs2_is_alloc(&s));
    CHECK_FALSE("not bodyalloc", fs2_is_bodyalloc(&s));
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, from_new_long_beyond_int_range) {
    __fs2_checkreset();
    /* Regression: _Generic must pick %ld, not %d */
    fs2 s = fs2_from_new(5000000000L);

    CHECK_STR_EQ("content", s.v, "5000000000");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, from_new_llong_boundaries) {
    __fs2_checkreset();

    fs2 s_max = fs2_from_new(LLONG_MAX);
    fs2 s_min = fs2_from_new(LLONG_MIN);

    CHECK_STR_EQ("max", s_max.v, "9223372036854775807");
    CHECK_STR_EQ("min", s_min.v, "-9223372036854775808");
    CHECK_FS2VALID(1, &s_max);
    CHECK_FS2VALID(2, &s_min);

    fs2_free(&s_max);
    fs2_free(&s_min);
    assert_balanced();
}

Test(fs2_api, from_new_unsigned2) {
    __fs2_checkreset();
    fs2 s = fs2_from_new(4294967295U);

    CHECK_STR_EQ("content", s.v, "4294967295");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, from_new_double) {
    __fs2_checkreset();
    fs2 s = fs2_from_new(3.14);

    CHECK_STR_EQ("content", s.v, "3.140000");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, from_new_independent) {
    __fs2_checkreset();

    fs2 a = fs2_from_new(100);
    fs2 b = fs2_from_new(200);

    CHECK_STR_EQ("a", a.v, "100");
    CHECK_STR_EQ("b", b.v, "200");
    CHECK_TRUE("different buffers", a.v != b.v);

    fs2_catstr(&a, "0");
    CHECK_STR_EQ("a mutated", a.v, "1000");
    CHECK_STR_EQ("b intact", b.v, "200");

    fs2_free(&a);
    fs2_free(&b);
    assert_balanced();
}

/* =========================================================================
 * fs2_heap_from — heap constructor
 * ========================================================================= */

Test(fs2_api, heap_from_int) {
    __fs2_checkreset();
    fs2 *h = fs2_heap_from(12345);

    CHECK_STR_EQ("content", h->v, "12345");
    CHECK_EQ("len", h->len, 5);
    CHECK_TRUE("is alloc", fs2_is_alloc(h));
    CHECK_TRUE("is bodyalloc", fs2_is_bodyalloc(h));
    CHECK_FS2VALID(1, h);

    fs2_free(h);
    assert_balanced();
}

Test(fs2_api, heap_from_long_and_llong) {
    __fs2_checkreset();

    fs2 *h_long  = fs2_heap_from(5000000000L);
    fs2 *h_llong = fs2_heap_from(LLONG_MIN);

    CHECK_STR_EQ("long", h_long->v, "5000000000");
    CHECK_STR_EQ("llong", h_llong->v, "-9223372036854775808");
    CHECK_FS2VALID(1, h_long);
    CHECK_FS2VALID(2, h_llong);

    fs2_free(h_long);
    fs2_free(h_llong);
    assert_balanced();
}

Test(fs2_api, heap_from_double) {
    __fs2_checkreset();
    fs2 *h = fs2_heap_from(3.14);

    CHECK_STR_EQ("content", h->v, "3.140000");
    CHECK_FS2VALID(1, h);

    fs2_free(h);
    assert_balanced();
}

Test(fs2_api, heap_from_no_leak) {
    __fs2_checkreset();

    /* Loop — if the macro leaks via stacktoheap, assert_balanced will catch it */
    for (int i = 0; i < 100; i++) {
        fs2 *h = fs2_heap_from(i);
        fs2_free(h);
    }
    assert_balanced();
}

/* =========================================================================
 * Composite — roundtrip through a string
 * ========================================================================= */

Test(fs2_api, composite_int_roundtrip) {
    __fs2_checkreset();

    int values[] = { 0, 1, -1, 42, INT_MAX, INT_MIN };
    size_t n = sizeof(values) / sizeof(values[0]);

    for (size_t i = 0; i < n; i++) {
        fs2 s = fs2_from_new(values[i]);
        CHECK_FS2VALID((int)i, &s);
        CHECK_EQ("roundtrip", fs2_parse_int(&s), values[i]);
        fs2_free(&s);
    }
    assert_balanced();
}

Test(fs2_api, composite_long_roundtrip) {
    __fs2_checkreset();

    long values[] = { 0L, 1L, -1L, 42L, 5000000000L, LONG_MAX, LONG_MIN };
    size_t n = sizeof(values) / sizeof(values[0]);

    for (size_t i = 0; i < n; i++) {
        fs2 s = fs2_from_new(values[i]);
        CHECK_FS2VALID((int)i, &s);
        CHECK_EQ("roundtrip", fs2_parse_long(&s), values[i]);
        fs2_free(&s);
    }
    assert_balanced();
}

Test(fs2_api, composite_llong_roundtrip) {
    __fs2_checkreset();

    long long values[] = { 0LL, 1LL, -1LL, 5000000000LL,
                           LLONG_MAX, LLONG_MIN };
    size_t n = sizeof(values) / sizeof(values[0]);

    for (size_t i = 0; i < n; i++) {
        fs2 s = fs2_from_new(values[i]);
        CHECK_FS2VALID((int)i, &s);
        /* parse_long works on LP64 (long == long long in size) */
        CHECK_EQ("roundtrip", fs2_parse_long(&s), (long)values[i]);
        fs2_free(&s);
    }
    assert_balanced();
}

Test(fs2_api, composite_int_to_string_pipeline) {
    __fs2_checkreset();

    /* Build "Result: 42 (forty-two)" */
    fs2 msg = fs2_from_new(42);
    CHECK_FS2VALID(1, &msg);

    fs2_rev_catstr(&msg, "Result: ");
    CHECK_STR_EQ("after prepend", msg.v, "Result: 42");
    CHECK_FS2VALID(2, &msg);

    fs2_catstr(&msg, " (forty-two)");
    CHECK_STR_EQ("after append", msg.v, "Result: 42 (forty-two)");
    CHECK_EQ("len", msg.len, 22);
    CHECK_FS2VALID(3, &msg);

    fs2_free(&msg);
    assert_balanced();
}

Test(fs2_api, composite_build_csv_row) {
    __fs2_checkreset();

    /* Build CSV: id,name,score */
    fs2 row = fs2_from_new(42);              // id
    fs2_catstr(&row, ",");
    fs2_catstr(&row, "alice");
    fs2_catstr(&row, ",");
    fs2_serialize(&row, 3.14);               // score (append after "42,alice,")

    /* Note: fs2_serialize writes FROM THE BEGINNING (pos=0), not at the end!
     * For CSV at the end you need fs2_catstr with manual formatting,
     * or fs2_sprintf_position. Checking real behavior. */
    CHECK_STR_EQ("row", row.v, "3.140000");
    CHECK_FS2VALID(1, &row);

    fs2_free(&row);
    assert_balanced();
}

Test(fs2_api, composite_heap_to_stack) {
    __fs2_checkreset();

    fs2 *h = fs2_heap_from(7);
    CHECK_FS2VALID(1, h);

    fs2 s = fs2_heaptostack(&h);

    CHECK_PTR_EQ("h reset", h, NULL);
    CHECK_STR_EQ("content survived", s.v, "7");
    CHECK_FALSE("no longer bodyalloc", fs2_is_bodyalloc(&s));
    CHECK_TRUE("still alloc", fs2_is_alloc(&s));
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, composite_array_of_values) {
    __fs2_checkreset();

    /* Array of 10 serialized ints */
    fs2 arr[10];
    for (int i = 0; i < 10; i++) {
        arr[i] = fs2_from_new(i);
        CHECK_FS2VALID(i, &arr[i]);
    }

    CHECK_STR_EQ("arr[0]", arr[0].v, "0");
    CHECK_STR_EQ("arr[5]", arr[5].v, "5");
    CHECK_STR_EQ("arr[9]", arr[9].v, "9");

    for (int i = 0; i < 10; i++)
        fs2_free(&arr[i]);
    assert_balanced();
}

Test(fs2_api, composite_mixed_types_in_array) {
    __fs2_checkreset();

    /* 
     * Tricky scenario: serialize different types, concatenate.
     * Demonstrates the macro works with any type in one loop
     * (here — manually, in real code — via separate calls).
     */
    fs2 a = fs2_from_new(42);
    fs2 b = fs2_from_new(5000000000L);
    fs2 c = fs2_from_new(3.14);

    fs2 joined = fs2_clone(&a);
    fs2_catstr(&joined, "|");
    fs2_cat(&joined, &b);
    fs2_catstr(&joined, "|");
    fs2_cat(&joined, &c);

    CHECK_STR_EQ("joined", joined.v, "42|5000000000|3.140000");
    CHECK_FS2VALID(1, &joined);

    fs2_free(&a);
    fs2_free(&b);
    fs2_free(&c);
    fs2_free(&joined);
    assert_balanced();
}

/* =========================================================================
 * fs2_serialize_concat — append serialized primitive
 * ========================================================================= */

Test(fs2_api, serialize_concat_int_appends) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("id: ");

    long res = fs2_serialize_concat(&s, 42);

    CHECK_EQ("return", res, 2);
    CHECK_STR_EQ("appended", s.v, "id: 42");
    CHECK_EQ("len", s.len, strlen("id: 42"));
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, serialize_concat_empty_dst) {
    __fs2_checkreset();
    fs2 s = FS2();                          /* canonical empty */

    fs2_serialize_concat(&s, 42);

    CHECK_STR_EQ("content", s.v, "42");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, serialize_concat_all_types) {
    __fs2_checkreset();
    fs2 s = FS2();

    fs2_serialize_concat(&s, 1);                    /* int */
    fs2_catstr(&s, "|");
    fs2_serialize_concat(&s, 2L);                   /* long */
    fs2_catstr(&s, "|");
    fs2_serialize_concat(&s, 3LL);                  /* long long */
    fs2_catstr(&s, "|");
    fs2_serialize_concat(&s, 4U);                   /* unsigned */
    fs2_catstr(&s, "|");
    fs2_serialize_concat(&s, 5UL);                  /* unsigned long */
    fs2_catstr(&s, "|");
    fs2_serialize_concat(&s, 6ULL);                 /* unsigned long long */

    CHECK_STR_EQ("content", s.v, "1|2|3|4|5|6");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, serialize_concat_long_no_truncation) {
    __fs2_checkreset();
    fs2 s = FS2();

    /* Regression: if _Generic accidentally picked %d, we get garbage */
    fs2_serialize_concat(&s, 5000000000L);

    CHECK_STR_EQ("content", s.v, "5000000000");
    CHECK_EQ("len", s.len, 10);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, serialize_concat_llong_boundaries) {
    __fs2_checkreset();
    fs2 s = FS2();

    fs2_serialize_concat(&s, LLONG_MAX);
    fs2_catstr(&s, "|");
    fs2_serialize_concat(&s, LLONG_MIN);

    CHECK_STR_EQ("content", s.v,
                 "9223372036854775807|-9223372036854775808");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, serialize_concat_float_double) {
    __fs2_checkreset();
    fs2 s = FS2();

    fs2_serialize_concat(&s, 3.14f);
    fs2_catstr(&s, "|");
    fs2_serialize_concat(&s, 2.71828);

    CHECK_STR_EQ("content", s.v, "3.140000|2.718280");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, serialize_concat_pointer) {
    __fs2_checkreset();
    int x = 42;
    void *ptr = &x;
    fs2 s = FS2();

    fs2_serialize_concat(&s, ptr);

    /* %p prints "0x..." */
    CHECK_TRUE("starts with 0x", s.len > 2 && s.v[0] == '0' && s.v[1] == 'x');
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, serialize_concat_return_equals_delta) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("p:");

    size_t before = s.len;
    long res = fs2_serialize_concat(&s, 12345);
    CHECK_EQ("int delta", res, (long)(s.len - before));

    before = s.len;
    res = fs2_serialize_concat(&s, 5000000000L);
    CHECK_EQ("long delta", res, (long)(s.len - before));

    before = s.len;
    res = fs2_serialize_concat(&s, -1);
    CHECK_EQ("negative delta", res, (long)(s.len - before));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, serialize_concat_chain_ints) {
    __fs2_checkreset();
    fs2 s = FS2();

    /* Build "0,1,2,...,99" via 100 int-appends */
    for (int i = 0; i < 100; i++) {
        fs2_serialize_concat(&s, i);
        if (i < 99) fs2_catstr(&s, ",");
    }

    CHECK_EQ("len", s.len, 289);            /* known from the previous test */
    CHECK_EQ("starts", strncmp(s.v, "0,1,2,", 6), 0);
    CHECK_EQ("ends", strcmp(s.v + s.len - 2, "99"), 0);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, serialize_concat_many_reallocs) {
    __fs2_checkreset();
    fs2 s = FS2();
    size_t initial_sz = s.sz;               /* 0 */

    /* Huge number of appends — guarantees 4-5 reallocs */
    for (int i = 0; i < 500; i++) {
        fs2_serialize_concat(&s, i);
    }

    CHECK_TRUE("buffer grew massively", s.sz > initial_sz);
    CHECK_TRUE("sz >= len+1", s.sz >= s.len + 1);
    CHECK_FS2VALID(1, &s);

    /* Verify no content was lost at realloc boundaries */
    CHECK_EQ("starts with 012", strncmp(s.v, "012", 3), 0);
    CHECK_EQ("ends with 499", strcmp(s.v + s.len - 3, "499"), 0);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, serialize_concat_local) {
    __fs2_checkreset();
    FS2LOCAL(s, 64);
    memcpy(s.v, "id=", 4);
    s.len = 3;

    fs2_serialize_concat(&s, 42);
    fs2_catstr(&s, ",ok");

    CHECK_STR_EQ("content", s.v, "id=42,ok");
    CHECK_TRUE("still LOCAL", fs2_is_local(&s));
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);   /* no-op */
}

/* =========================================================================
 * Composite — real-world scenarios
 * ========================================================================= */

Test(fs2_api, composite_serialize_concat_csv) {
    __fs2_checkreset();
    fs2 row = FS2();

    fs2_serialize_concat(&row, 42);
    fs2_catstr(&row, ",");
    fs2_catstr(&row, "alice");
    fs2_catstr(&row, ",");
    fs2_serialize_concat(&row, 3.14);

    CHECK_STR_EQ("csv row", row.v, "42,alice,3.140000");
    CHECK_FS2VALID(1, &row);

    fs2_free(&row);
    assert_balanced();
}

Test(fs2_api, composite_serialize_concat_json_ish) {
    __fs2_checkreset();
    fs2 j = FS2();

    fs2_catstr(&j, "{\"id\":");
    fs2_serialize_concat(&j, 7);
    fs2_catstr(&j, ",\"score\":");
    fs2_serialize_concat(&j, 9.5);
    fs2_catstr(&j, "}");

    CHECK_STR_EQ("json", j.v, "{\"id\":7,\"score\":9.500000}");
    CHECK_FS2VALID(1, &j);

    fs2_free(&j);
    assert_balanced();
}

Test(fs2_api, composite_serialize_concat_mixed_widths) {
    __fs2_checkreset();
    fs2 s = FS2();

    /* Mix all integer widths — if _Generic confuses them,
     * one of the values will be truncated to garbage */
    fs2_serialize_concat(&s, (int)INT_MIN);
    fs2_catstr(&s, ",");
    fs2_serialize_concat(&s, (long)LONG_MIN);
    fs2_catstr(&s, ",");
    fs2_serialize_concat(&s, (long long)LLONG_MIN);

    /* Verify each value fit without distortion */
    CHECK_EQ("starts with INT_MIN",
             strncmp(s.v, "-2147483648,", 12), 0);
    CHECK_TRUE("contains LONG_MIN",
               strstr(s.v, "-9223372036854775808") != NULL);
    CHECK_TRUE("ends with LLONG_MIN",
               strcmp(s.v + s.len - 20, "-9223372036854775808") == 0);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, composite_serialize_concat_with_from_new) {
    __fs2_checkreset();

    /* Combine: from_new creates, then append via concat */
    fs2 s = fs2_from_new(100);
    fs2_serialize_concat(&s, 200);
    fs2_serialize_concat(&s, 300);

    CHECK_STR_EQ("content", s.v, "100200300");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_double_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("3.14abc");
    double out = 0.0;
    size_t newpos = 0;

    CHECK_TRUE("parse", fs2_pos_parse_double(&s, 0, &newpos, &out));
    CHECK_TRUE("val", fabs(out - 3.14) < 1e-9);
    CHECK_EQ("newpos", newpos, 4);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_double_negative) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("-2.5");
    double out = 0.0;

    CHECK_TRUE("parse", fs2_pos_parse_double(&s, 0, NULL, &out));
    CHECK_TRUE("val", fabs(out - (-2.5)) < 1e-9);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_double_scientific) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("1.5e10");
    double out = 0.0;

    CHECK_TRUE("parse", fs2_pos_parse_double(&s, 0, NULL, &out));
    CHECK_TRUE("val", fabs(out - 1.5e10) < 1e-3);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_double_plus_sign) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("+3.14");
    double out = 0.0;

    CHECK_TRUE("parse", fs2_pos_parse_double(&s, 0, NULL, &out));
    CHECK_TRUE("val", fabs(out - 3.14) < 1e-9);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, tryparse_double_strict_vs_lenient) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("3.14xyz");
    double out = 0.0;

    CHECK_TRUE ("lenient", fs2_tryparse_double(&s, &out, false));
    CHECK_TRUE ("val", fabs(out - 3.14) < 1e-9);
    CHECK_FALSE("strict rejects trailing", fs2_tryparse_double(&s, &out, true));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, tryparse_double_accepts_leading_spaces) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("  \t3.14");
    double out = 0.0;

    CHECK_TRUE("parse", fs2_tryparse_double(&s, &out, true));
    CHECK_TRUE("val", fabs(out - 3.14) < 1e-9);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, tryparse_double_rejects_newline) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("\n3.14");
    double out = 0.0;

    /* strtod would eat the \n on its own — we reject before the call */
    CHECK_FALSE("newline rejected", fs2_tryparse_double(&s, &out, true));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, parse_double_uses_current_locale) {
    __fs2_checkreset();
    
    /* Documenting: the parser uses the current locale.
     * In C locale (default for tests) the decimal separator is '.'.
     * If CI ever runs the tests in a non-C locale — this test
     * will reveal the real behavior. */
    fs2 s = fs2_fromstr("3.14");
    double out = 0.0;

    CHECK_TRUE("parses with dot", fs2_tryparse_double(&s, &out, true));
    CHECK_TRUE("val ~3.14", fabs(out - 3.14) < 1e-9);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, tryparse_double_overflow) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("1e400");   /* >> DBL_MAX (~1.8e308) */
    double out = 0.0;

    CHECK_FALSE("overflow", fs2_tryparse_double(&s, &out, true));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, tryparse_double_underflow) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("1e-400");   /* << DBL_MIN (~2.2e-308) */
    double out = 0.0;

    /* strtod returns 0.0 + ERANGE. We reject it. */
    CHECK_FALSE("underflow", fs2_tryparse_double(&s, &out, true));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, tryparse_double_nan) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("nan");
    double out = 0.0;

    CHECK_TRUE("parses", fs2_tryparse_double(&s, &out, true));
    CHECK_TRUE("is nan", isnan(out));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, tryparse_double_inf) {
    __fs2_checkreset();
    fs2 s_pos = fs2_fromstr("inf");
    fs2 s_neg = fs2_fromstr("-infinity");
    double out = 0.0;

    CHECK_TRUE("inf", fs2_tryparse_double(&s_pos, &out, true));
    CHECK_TRUE("is +inf", isinf(out) && out > 0);

    CHECK_TRUE("-infinity", fs2_tryparse_double(&s_neg, &out, true));
    CHECK_TRUE("is -inf", isinf(out) && out < 0);

    fs2_free(&s_pos);
    fs2_free(&s_neg);
    assert_balanced();
}

Test(fs2_api, parse_double_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("3.14159");

    CHECK_TRUE("parsed", fabs(fs2_parse_double(&s) - 3.14159) < 1e-9);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, parse_double_fails_userraise, .signal = SIGINT) {
    fs2 s = fs2_fromstr("abc");
    fs2_parse_double(&s);
    fs2_free(&s);
}

Test(fs2_api, composite_parse_double_from_json) {
    __fs2_checkreset();

    /* {"score":9.5} — parse the value after ":" */
    fs2 json = fs2_fromstr("{\"score\":9.5}");

    ptrdiff_t colon = fs2_chr(&json, ':');
    CHECK_TRUE("colon found", colon >= 0);

    size_t newpos = 0;
    double score  = 0.0;
    CHECK_TRUE("parse", 
               fs2_pos_parse_double(&json, (size_t)colon + 1, &newpos, &score));
    CHECK_TRUE("val", fabs(score - 9.5) < 1e-9);
    CHECK_EQ("stopped at '}'", json.v[newpos], '}');

    fs2_free(&json);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_char
 * ========================================================================= */

Test(fs2_api, pos_parse_char_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");
    char out = 0;
    size_t newpos = 0;

    CHECK_TRUE("parse", fs2_pos_parse_char(&s, 0, &newpos, &out));
    CHECK_EQ("val", out, 'a');
    CHECK_EQ("newpos", newpos, 1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_char_sequential) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("xyz");
    char out = 0;
    size_t pos = 0;

    /* Parse character by character — a typical tokenization scenario */
    CHECK_TRUE ("x", fs2_pos_parse_char(&s, pos, &pos, &out));
    CHECK_EQ   ("val", out, 'x');
    CHECK_TRUE ("y", fs2_pos_parse_char(&s, pos, &pos, &out));
    CHECK_EQ   ("val", out, 'y');
    CHECK_TRUE ("z", fs2_pos_parse_char(&s, pos, &pos, &out));
    CHECK_EQ   ("val", out, 'z');
    CHECK_FALSE("eof", fs2_pos_parse_char(&s, pos, &pos, &out));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_char_out_of_range) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("ab");
    char out = 0;

    CHECK_FALSE("pos == len",   fs2_pos_parse_char(&s, 2, NULL, &out));
    CHECK_FALSE("pos > len",    fs2_pos_parse_char(&s, 5, NULL, &out));

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_tryparse_char — strict/lenient
 * ========================================================================= */

Test(fs2_api, tryparse_char_lenient_accepts_space) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr(" abc");
    char out = 0;

    CHECK_TRUE("lenient space", fs2_tryparse_char(&s, &out, false));
    CHECK_EQ("val is space", out, ' ');

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, tryparse_char_lenient_accepts_tab) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("\tabc");
    char out = 0;

    CHECK_TRUE("lenient tab", fs2_tryparse_char(&s, &out, false));
    CHECK_EQ("val is tab", out, '\t');

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, tryparse_char_lenient_accepts_newline) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("\nabc");
    char out = 0;

    CHECK_TRUE("lenient newline", fs2_tryparse_char(&s, &out, false));
    CHECK_EQ("val is newline", out, '\n');

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, tryparse_char_strict_rejects_space) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr(" X");
    char out = 0;

    CHECK_FALSE("strict rejects space", fs2_tryparse_char(&s, &out, true));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, tryparse_char_strict_rejects_tab) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("\tX");
    char out = 0;

    CHECK_FALSE("strict rejects tab", fs2_tryparse_char(&s, &out, true));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, tryparse_char_strict_rejects_newline) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("\nX");
    char out = 0;

    CHECK_FALSE("strict rejects newline", fs2_tryparse_char(&s, &out, true));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, tryparse_char_strict_rejects_all_isspace) {
    __fs2_checkreset();

    /* All six isspace characters: ' ', \t, \n, \v, \f, \r */
    const char ws[] = { ' ', '\t', '\n', '\v', '\f', '\r' };
    for (size_t i = 0; i < sizeof(ws); i++) {
        char buf[2] = { ws[i], '\0' };
        fs2 s = fs2_fromstr(buf);
        char out = 0;

        CHECK_FALSE("strict rejects isspace", 
                    fs2_tryparse_char(&s, &out, true));

        fs2_free(&s);
    }
    assert_balanced();
}

Test(fs2_api, tryparse_char_strict_accepts_nonspace) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("X");
    char out = 0;

    CHECK_TRUE("strict accepts 'X'", fs2_tryparse_char(&s, &out, true));
    CHECK_EQ("val", out, 'X');

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, tryparse_char_strict_accepts_digit) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("5");
    char out = 0;

    CHECK_TRUE("strict accepts digit", fs2_tryparse_char(&s, &out, true));
    CHECK_EQ("val", out, '5');

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, tryparse_char_strict_accepts_sign) {
    __fs2_checkreset();
    fs2 s_plus  = fs2_fromstr("+");
    fs2 s_minus = fs2_fromstr("-");
    char out = 0;

    /* '+' and '-' are not whitespace — pass strict */
    CHECK_TRUE("strict '+'", fs2_tryparse_char(&s_plus, &out, true));
    CHECK_EQ("val", out, '+');

    CHECK_TRUE("strict '-'", fs2_tryparse_char(&s_minus, &out, true));
    CHECK_EQ("val", out, '-');

    fs2_free(&s_plus);
    fs2_free(&s_minus);
    assert_balanced();
}

Test(fs2_api, tryparse_char_strict_accepts_non_ascii) {
    __fs2_checkreset();
    /* UTF-8 lead byte 0xC3 — not isspace in C locale */
    fs2 s = fs2_fromstr("\xC3\xA4");
    char out = 0;

    CHECK_TRUE("strict accepts non-ascii", fs2_tryparse_char(&s, &out, true));
    CHECK_EQ("val is 0xC3", out, (char)0xC3);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, tryparse_char_empty_string) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("");
    char out = 0;

    CHECK_FALSE("empty lenient", fs2_tryparse_char(&s, &out, false));
    CHECK_FALSE("empty strict",  fs2_tryparse_char(&s, &out, true));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, tryparse_char_canonical_empty) {
    __fs2_checkreset();
    fs2 s = FS2();
    char out = 0;

    CHECK_FALSE("canonical empty", fs2_tryparse_char(&s, &out, false));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, tryparse_char_out_null) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("x");

    CHECK_TRUE ("lenient with NULL out", fs2_tryparse_char(&s, NULL, false));

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_parse_char — raises on empty
 * ========================================================================= */

Test(fs2_api, parse_char_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("Z");

    CHECK_EQ("parsed", fs2_parse_char(&s), 'Z');

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, parse_char_space_is_valid_lenient) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr(" x");

    /* fs2_parse_char — lenient, space is valid */
    CHECK_EQ("parsed", fs2_parse_char(&s), ' ');

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, parse_char_empty_aborts, .signal = SIGINT) {
    fs2 s = fs2_fromstr("");
    fs2_parse_char(&s);
    fs2_free(&s);
}

Test(fs2_api, parse_char_canonical_empty_aborts, .signal = SIGINT) {
    fs2 s = FS2();
    fs2_parse_char(&s);
    fs2_free(&s);
}

/* =========================================================================
 * Signature compatibility for container dispatcher
 * ========================================================================= */

Test(fs2_api, tryparse_char_signature_matches) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("X");
    char out = 0;

    /* Verify the signature (const fs2*, T*, bool) → bool is compatible */
    bool (*fn)(const fs2 *, char *, bool) = fs2_tryparse_char;
    CHECK_TRUE("callable via fn ptr", fn(&s, &out, true));
    CHECK_EQ("val", out, 'X');

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * char in FS2_FMT / FS2_SER_ERRMSG
 * ========================================================================= */

Test(fs2_api, serialize_char_basic) {
    __fs2_checkreset();
    fs2 dst = FS2();
    char c = 'A';                          /* ← specifically char, not a literal */

    long res = fs2_serialize(&dst, c);

    CHECK_EQ("return 1 char", res, 1);
    CHECK_STR_EQ("content", dst.v, "A");
    CHECK_EQ("len", dst.len, 1);
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2_api, serialize_char_is_not_int) {
    __fs2_checkreset();
    fs2 dst = FS2();
    char c = 'A';

    fs2_serialize(&dst, c);

    /* If _Generic picked %d instead of %c, we get "65" instead of "A".
     * This test is the main regression for the correct branch. */
    CHECK_STR_EQ("is char not int", dst.v, "A");
    CHECK_EQ("len is 1 not 2", dst.len, 1);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2_api, serialize_char_space) {
    __fs2_checkreset();
    fs2 dst = FS2();
    char c = ' ';

    fs2_serialize(&dst, c);

    CHECK_STR_EQ("content", dst.v, " ");
    CHECK_EQ("len", dst.len, 1);
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2_api, serialize_char_tab_newline) {
    __fs2_checkreset();

    fs2 s_tab = FS2();
    fs2 s_nl  = FS2();
    char tab = '\t';
    char nl  = '\n';

    fs2_serialize(&s_tab, tab);
    fs2_serialize(&s_nl,  nl);

    CHECK_STR_EQ("tab",     s_tab.v, "\t");
    CHECK_EQ   ("tab len",  s_tab.len, 1);
    CHECK_STR_EQ("newline", s_nl.v,  "\n");
    CHECK_EQ   ("nl len",   s_nl.len, 1);

    fs2_free(&s_tab);
    fs2_free(&s_nl);
    assert_balanced();
}

Test(fs2_api, serialize_char_non_ascii) {
    __fs2_checkreset();
    fs2 dst = FS2();
    char c = (char)0xE4;                   /* Latin-1 'ä' */

    fs2_serialize(&dst, c);

    /* %c writes a raw byte — 0xE4 */
    CHECK_EQ("len", dst.len, 1);
    CHECK_EQ("byte", (unsigned char)dst.v[0], 0xE4u);
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

/* =========================================================================
 * from_new / serialize_concat — char variants
 * ========================================================================= */

Test(fs2_api, from_new_char) {
    __fs2_checkreset();
    char c = 'Z';

    fs2 s = fs2_from_new(c);

    CHECK_STR_EQ("content", s.v, "Z");
    CHECK_EQ("len", s.len, 1);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, heap_from_char) {
    __fs2_checkreset();
    char c = '!';

    fs2 *h = fs2_heap_from(c);

    CHECK_STR_EQ("content", h->v, "!");
    CHECK_TRUE("is bodyalloc", fs2_is_bodyalloc(h));
    CHECK_FS2VALID(1, h);

    fs2_free(h);
    assert_balanced();
}

Test(fs2_api, serialize_concat_char) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("cmd:");
    char c = 'X';

    long res = fs2_serialize_concat(&s, c);

    CHECK_EQ("return 1 char", res, 1);
    CHECK_STR_EQ("appended", s.v, "cmd:X");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * Composite: building a string character by character
 * ========================================================================= */

Test(fs2_api, composite_char_by_char_build) {
    __fs2_checkreset();
    fs2 s = FS2();

    /* Build "hello" via serialize_concat one character at a time */
    const char *word = "hello";
    for (size_t i = 0; i < strlen(word); i++) {
        char c = word[i];
        fs2_serialize_concat(&s, c);
    }

    CHECK_STR_EQ("built", s.v, "hello");
    CHECK_EQ("len", s.len, 5);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, composite_mixed_char_and_int) {
    __fs2_checkreset();
    fs2 s = FS2();

    char   c  = '=';
    int    n  = 42;

    /* Typical scenario "key=42" — character + number */
    fs2_serialize_concat(&s, c);
    fs2_serialize_concat(&s, n);

    CHECK_STR_EQ("content", s.v, "=42");
    CHECK_EQ("len", s.len, 3);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, composite_roundtrip_char) {
    __fs2_checkreset();

    /* char → fs2 → char, round-trip */
    char original = 'Q';
    fs2 s = fs2_from_new(original);
    CHECK_EQ("len", s.len, 1);

    char parsed = 0;
    CHECK_TRUE("parse back", fs2_tryparse_char(&s, &parsed, false));
    CHECK_EQ("round-trip", parsed, original);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * Regression: character literal 'x' goes through int branch, not char
 * ========================================================================= */

Test(fs2_api, char_literal_goes_through_int_branch) {
    __fs2_checkreset();
    fs2 s = FS2();

    /* In C, 'x' has type int. _Generic picks int → %d → "120".
     * This is NOT a bug, it is the C standard. We fix the behavior. */
    fs2_serialize(&s, 'x');            /* int literal */
    CHECK_STR_EQ("literal as int → decimal", s.v, "120");

    fs2_free(&s);

    /* For char — a variable or an explicit cast */
    fs2 s2 = FS2();
    fs2_serialize(&s2, (char)'x');
    CHECK_STR_EQ("cast to char → char", s2.v, "x");

    fs2_free(&s2);
    assert_balanced();
}

Test(fs2_api, serialize_signed_char_as_number) {
    __fs2_checkreset();
    fs2 dst = FS2();
    signed char c = -100;

    fs2_serialize(&dst, c);

    CHECK_STR_EQ("number", dst.v, "-100");
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2_api, serialize_unsigned_char_as_number) {
    __fs2_checkreset();
    fs2 dst = FS2();
    unsigned char c = 200;

    fs2_serialize(&dst, c);

    /* If someone confused %hhu and %c — they get byte 0xC8 instead of "200" */
    CHECK_STR_EQ("number", dst.v, "200");
    CHECK_EQ("len", dst.len, 3);
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2_api, serialize_signed_char_negative) {
    __fs2_checkreset();
    fs2 dst = FS2();
    signed char c = -128;                  /* INT8_MIN */

    fs2_serialize(&dst, c);

    CHECK_STR_EQ("min int8", dst.v, "-128");
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2_api, serialize_unsigned_char_max) {
    __fs2_checkreset();
    fs2 dst = FS2();
    unsigned char c = 255;                 /* UINT8_MAX */

    fs2_serialize(&dst, c);

    CHECK_STR_EQ("max uint8", dst.v, "255");
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2_api, serialize_char_vs_unsigned_char) {
    __fs2_checkreset();
    
    /* Same byte 65 ('A' = UINT8 65) — but different types produce different output */
    char          c_char = 'A';            /* char   → "%c"  → "A" */
    unsigned char c_uchar = 65;            /* uchar  → "%hhu" → "65" */
    
    fs2 s_char  = FS2();
    fs2 s_uchar = FS2();
    
    fs2_serialize(&s_char,  c_char);
    fs2_serialize(&s_uchar, c_uchar);
    
    CHECK_STR_EQ("char → 'A'",       s_char.v,  "A");
    CHECK_STR_EQ("uchar → '65'",     s_uchar.v, "65");
    CHECK_EQ   ("char len 1",        s_char.len,  1);
    CHECK_EQ   ("uchar len 2",       s_uchar.len, 2);
    
    fs2_free(&s_char);
    fs2_free(&s_uchar);
    assert_balanced();
}

Test(fs2_api, serialize_uint8_via_uchar) {
    __fs2_checkreset();
    
    /* Typical use: uint8_t as a counter */
    unsigned char flags = 42;
    fs2 s = fs2_from_new(flags);
    
    CHECK_STR_EQ("counter as number", s.v, "42");
    CHECK_FS2VALID(1, &s);
    
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_pos_parse_short — boundary checks
 * ========================================================================= */

Test(fs2_api, pos_parse_short_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("12345");
    short out = 0;
    size_t newpos = 0;

    CHECK_TRUE("parse", fs2_pos_parse_short(&s, 0, &newpos, &out));
    CHECK_EQ("val", out, 12345);
    CHECK_EQ("newpos", newpos, 5);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_short_max_exact) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("32767");   /* SHRT_MAX */
    short out = 0;

    CHECK_TRUE("SHRT_MAX ok", fs2_pos_parse_short(&s, 0, NULL, &out));
    CHECK_EQ("val", out, SHRT_MAX);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_short_min_exact) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("-32768");   /* SHRT_MIN */
    short out = 0;

    CHECK_TRUE("SHRT_MIN ok", fs2_pos_parse_short(&s, 0, NULL, &out));
    CHECK_EQ("val", out, SHRT_MIN);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_short_overflow_plus_one) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("32768");   /* SHRT_MAX + 1 */
    short out = 0;

    CHECK_FALSE("SHRT_MAX + 1 rejected", fs2_pos_parse_short(&s, 0, NULL, &out));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_short_underflow_minus_one) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("-32769");   /* SHRT_MIN - 1 */
    short out = 0;

    CHECK_FALSE("SHRT_MIN - 1 rejected", fs2_pos_parse_short(&s, 0, NULL, &out));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, pos_parse_short_far_beyond) {
    __fs2_checkreset();
    fs2 s_big  = fs2_fromstr("1000000");
    fs2 s_neg  = fs2_fromstr("-1000000");
    short out = 0;

    /* Fits in long but not in short */
    CHECK_FALSE("1M rejected",   fs2_pos_parse_short(&s_big, 0, NULL, &out));
    CHECK_FALSE("-1M rejected",  fs2_pos_parse_short(&s_neg, 0, NULL, &out));

    fs2_free(&s_big);
    fs2_free(&s_neg);
    assert_balanced();
}

Test(fs2_api, pos_parse_short_just_inside_bounds) {
    __fs2_checkreset();
    
    /* Bounds: SHRT_MAX-1 and SHRT_MIN+1 — must pass */
    fs2 s_hi = fs2_fromstr("32766");
    fs2 s_lo = fs2_fromstr("-32767");
    short out = 0;

    CHECK_TRUE("32766 ok",  fs2_pos_parse_short(&s_hi, 0, NULL, &out));
    CHECK_EQ   ("val", out, (short)32766);
    CHECK_TRUE("-32767 ok", fs2_pos_parse_short(&s_lo, 0, NULL, &out));
    CHECK_EQ   ("val", out, (short)-32767);

    fs2_free(&s_hi);
    fs2_free(&s_lo);
    assert_balanced();
}

Test(fs2_api, pos_parse_short_zero) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("0");
    short out = 99;

    CHECK_TRUE("zero ok", fs2_pos_parse_short(&s, 0, NULL, &out));
    CHECK_EQ("val", out, 0);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_tryparse_short / fs2_parse_short
 * ========================================================================= */

Test(fs2_api, tryparse_short_strict_vs_lenient) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42abc");
    short out = 0;

    CHECK_TRUE ("lenient",       fs2_tryparse_short(&s, &out, false));
    CHECK_EQ   ("val", out, 42);
    CHECK_FALSE("strict rejects", fs2_tryparse_short(&s, &out, true));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, tryparse_short_overflow) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("32768");
    short out = 0;

    CHECK_FALSE("strict overflow", fs2_tryparse_short(&s, &out, true));
    CHECK_FALSE("lenient overflow", fs2_tryparse_short(&s, &out, false));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, parse_short_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("-1234");

    CHECK_EQ("parsed", fs2_parse_short(&s), (short)-1234);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, parse_short_fails_userraise, .signal = SIGINT) {
    fs2 s = fs2_fromstr("abc");
    fs2_parse_short(&s);
    fs2_free(&s);
}

Test(fs2_api, parse_short_overflow_aborts, .signal = SIGINT) {
    fs2 s = fs2_fromstr("40000");   /* > SHRT_MAX */
    fs2_parse_short(&s);
    fs2_free(&s);
}

/* =========================================================================
 * Chain int → short: one value, different results
 * ========================================================================= */

Test(fs2_api, short_vs_int_chain) {
    __fs2_checkreset();
    
    fs2 s_big = fs2_fromstr("100000");   /* > SHRT_MAX, < INT_MAX */
    
    short s_out = 0;
    int   i_out = 0;
    
    CHECK_FALSE("short rejects", fs2_tryparse_short(&s_big, &s_out, true));
    CHECK_TRUE ("int accepts",   fs2_tryparse_int  (&s_big, &i_out, true));
    CHECK_EQ   ("int val",       i_out, 100000);
    
    fs2_free(&s_big);
    assert_balanced();
}

Test(fs2_api, short_vs_long_chain) {
    __fs2_checkreset();
    
    fs2 s = fs2_fromstr("32768");   /* SHRT_MAX + 1, but fits in long */
    
    short s_out = 0;
    long  l_out = 0;
    
    CHECK_FALSE("short rejects", fs2_tryparse_short(&s, &s_out, true));
    CHECK_TRUE ("long accepts",  fs2_tryparse_long (&s, &l_out, true));
    CHECK_EQ   ("long val",      l_out, 32768L);
    
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * Composite: short in real code
 * ========================================================================= */

Test(fs2_api, composite_parse_short_range_validation) {
    __fs2_checkreset();
    
    /* Scenario: parse "temperature=25" and validate range [-40, 80] */
    fs2 line = fs2_fromstr("temperature=25");
    
    ptrdiff_t eq = fs2_chr(&line, '=');
    CHECK_TRUE("eq found", eq >= 0);
    
    size_t newpos = 0;
    short  temp   = 0;
    CHECK_TRUE("parse", 
               fs2_pos_parse_short(&line, (size_t)eq + 1, &newpos, &temp));
    CHECK_EQ("temp", temp, 25);
    CHECK_TRUE("in [-40, 80]", temp >= -40 && temp <= 80);
    
    fs2_free(&line);
    assert_balanced();
}

/* =========================================================================
 * short / unsigned short in FS2_FMT / FS2_SER_ERRMSG
 * ========================================================================= */

Test(fs2_api, serialize_short_basic) {
    __fs2_checkreset();
    fs2 dst = FS2();
    short s_val = 42;

    long res = fs2_serialize(&dst, s_val);

    CHECK_EQ("return", res, 2);
    CHECK_STR_EQ("content", dst.v, "42");
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2_api, serialize_short_negative) {
    __fs2_checkreset();
    fs2 dst = FS2();
    short s_val = -1234;

    fs2_serialize(&dst, s_val);

    CHECK_STR_EQ("content", dst.v, "-1234");
    CHECK_EQ("len", dst.len, 5);
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

/* ⚠ Main test: catches %hhd (signed char) instead of %hd (short).
 * For SHRT_MAX=32767: %hd gives "32767", %hhd gives "-1". */
Test(fs2_api, serialize_short_boundaries) {
    __fs2_checkreset();

    fs2 s_max = FS2();
    fs2 s_min = FS2();
    short v_max = SHRT_MAX;   /* 32767 */
    short v_min = SHRT_MIN;   /* -32768 */

    fs2_serialize(&s_max, v_max);
    fs2_serialize(&s_min, v_min);

    CHECK_STR_EQ("SHRT_MAX", s_max.v, "32767");
    CHECK_STR_EQ("SHRT_MIN", s_min.v, "-32768");
    CHECK_EQ("max len", s_max.len, 5);
    CHECK_EQ("min len", s_min.len, 6);
    CHECK_FS2VALID(1, &s_max);
    CHECK_FS2VALID(2, &s_min);

    fs2_free(&s_max);
    fs2_free(&s_min);
    assert_balanced();
}

Test(fs2_api, serialize_ushort_basic) {
    __fs2_checkreset();
    fs2 dst = FS2();
    unsigned short us_val = 42;

    fs2_serialize(&dst, us_val);

    CHECK_STR_EQ("content", dst.v, "42");
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

/* ⚠ Main test: catches %hhu (unsigned char) instead of %hu (unsigned short).
 * For USHRT_MAX=65535: %hu gives "65535", %hhu gives "255". */
Test(fs2_api, serialize_ushort_max) {
    __fs2_checkreset();
    fs2 dst = FS2();
    unsigned short us_val = USHRT_MAX;   /* 65535 */

    fs2_serialize(&dst, us_val);

    CHECK_STR_EQ("USHRT_MAX", dst.v, "65535");
    CHECK_EQ("len", dst.len, 5);
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

/* Sanity: short vs unsigned short with the same numeric value
 * in the overlap zone — output is the same (both "42"), but these are different branches. */
Test(fs2_api, serialize_short_vs_ushort_same_value) {
    __fs2_checkreset();

    fs2 s_signed   = FS2();
    fs2 s_unsigned = FS2();
    short          v_s = 42;
    unsigned short v_u = 42;

    fs2_serialize(&s_signed,   v_s);
    fs2_serialize(&s_unsigned, v_u);

    CHECK_STR_EQ("signed",   s_signed.v,   "42");
    CHECK_STR_EQ("unsigned", s_unsigned.v, "42");

    fs2_free(&s_signed);
    fs2_free(&s_unsigned);
    assert_balanced();
}

/* =========================================================================
 * fs2_tryparse_ushort / fs2_parse_ushort — minimal coverage
 * ========================================================================= */

Test(fs2_api, tryparse_ushort_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42");
    unsigned short out = 0;

    CHECK_TRUE("parse", fs2_tryparse_ushort(&s, &out, true));
    CHECK_EQ("val", out, 42);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, tryparse_ushort_max_exact) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("65535");   /* USHRT_MAX */
    unsigned short out = 0;

    CHECK_TRUE("USHRT_MAX ok", fs2_tryparse_ushort(&s, &out, true));
    CHECK_EQ("val", out, USHRT_MAX);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, tryparse_ushort_overflow_plus_one) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("65536");   /* USHRT_MAX + 1 */
    unsigned short out = 0;

    CHECK_FALSE("USHRT_MAX+1 rejected", fs2_tryparse_ushort(&s, &out, true));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, tryparse_ushort_rejects_negative) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("-1");
    unsigned short out = 0;

    /* strtoull("-1") = ULLONG_MAX, but pos_parse_ullong rejects the minus */
    CHECK_FALSE("negative rejected", fs2_tryparse_ushort(&s, &out, true));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, tryparse_ushort_strict_vs_lenient) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("42xyz");
    unsigned short out = 0;

    CHECK_TRUE ("lenient", fs2_tryparse_ushort(&s, &out, false));
    CHECK_EQ   ("val", out, 42);
    CHECK_FALSE("strict rejects trailing", fs2_tryparse_ushort(&s, &out, true));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, tryparse_ushort_zero) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("0");
    unsigned short out = 99;

    CHECK_TRUE("zero ok", fs2_tryparse_ushort(&s, &out, true));
    CHECK_EQ("val", out, 0);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, parse_ushort_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("1234");

    CHECK_EQ("parsed", fs2_parse_ushort(&s), 1234);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, parse_ushort_overflow_aborts, .signal = SIGINT) {
    fs2 s = fs2_fromstr("70000");   /* > USHRT_MAX */
    fs2_parse_ushort(&s);
    fs2_free(&s);
}

/* =========================================================================
 * fs2_spn / fs2_cspn
 * ========================================================================= */

Test(fs2_api, spn_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("aaabbb");

    CHECK_EQ("3 a's",     fs2_spn(&s, 0, "a"), 3);
    CHECK_EQ("3 b's",     fs2_spn(&s, 3, "b"), 3);
    CHECK_EQ("no match",  fs2_spn(&s, 0, "xyz"), 0);
    CHECK_EQ("mixed set", fs2_spn(&s, 0, "ab"), 6);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, spn_eof) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");
    fs2 empty = FS2();

    CHECK_EQ("at end",     fs2_spn(&s, 3, "a"), 0);
    CHECK_EQ("beyond end", fs2_spn(&s, 10, "a"), 0);
    CHECK_EQ("canonical empty", fs2_spn(&empty, 0, "a"), 0);

    fs2_free(&s);
    fs2_free(&empty);
    assert_balanced();
}

Test(fs2_api, cspn_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello=world");

    CHECK_EQ("until '='",      fs2_cspn(&s, 0, "="), 5);
    CHECK_EQ("from 'world'",   fs2_cspn(&s, 6, "="), 5);
    CHECK_EQ("at '='",         fs2_cspn(&s, 5, "="), 0);
    CHECK_EQ("not found",      fs2_cspn(&s, 0, "z"), 11);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, nonspace_span_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello world\nfoo");

    CHECK_EQ("hello",  fs2_nonspace_span(&s, 0), 5);
    CHECK_EQ("space",  fs2_nonspace_span(&s, 5), 0);
    CHECK_EQ("world",  fs2_nonspace_span(&s, 6), 5);
    CHECK_EQ("at \\n", fs2_nonspace_span(&s, 11), 0);
    CHECK_EQ("foo",    fs2_nonspace_span(&s, 12), 3);

    fs2_free(&s);
    assert_balanced();
}

/* Composite: tokenizer in 5 lines */
Test(fs2_api, composite_tokenize) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("the quick brown fox");
    size_t pos = 0;
    int    count = 0;

    while (pos < s.len) {
        size_t len = fs2_nonspace_span(&s, pos);
        if (len > 0) count++;
        pos += len > 0 ? len : 1;   /* skip whitespace */
    }

    CHECK_EQ("token count", count, 4);

    fs2_free(&s);
    assert_balanced();
}

/* Composite: key=value */
Test(fs2_api, composite_key_value) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("timeout=30");

    /* 1. Find '=' via cspn */
    size_t key_len = fs2_cspn(&s, 0, "=");
    CHECK_EQ("key len", key_len, 7);
    CHECK_EQ("eq found", s.v[key_len], '=');

    /* 2. Parse the number AFTER '=' via pos_parse_int (not tryparse!) */
    size_t val_end = 0;
    int    value   = 0;
    CHECK_TRUE("parse value after '='", 
               fs2_pos_parse_int(&s, key_len + 1, &val_end, &value));
    CHECK_EQ("value", value, 30);
    CHECK_EQ("consumed all", val_end, s.len);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_instr_posn — base (pattern is fs2)
 * ========================================================================= */

Test(fs2_api, instr_posn_basic) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("hello world");
    fs2 pat = FS2LITERAL("world");

    CHECK_EQ("world at 6", fs2_instr_posn(&s, &pat, 0, 1), 6);

    fs2 pat2 = FS2LITERAL("hello");
    CHECK_EQ("hello at 0", fs2_instr_posn(&s, &pat2, 0, 1), 0);

    fs2 pat3 = FS2LITERAL("l");
    CHECK_EQ("l at 2", fs2_instr_posn(&s, &pat3, 0, 1), 2);

    fs2_free(&s);   /* pattern — STATIC, fs2_free no-op */
    assert_balanced();
}

Test(fs2_api, instr_posn_nth) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("abcabcabc");
    fs2 pat = FS2LITERAL("abc");

    CHECK_EQ("1st", fs2_instr_posn(&s, &pat, 0, 1), 0);
    CHECK_EQ("2nd", fs2_instr_posn(&s, &pat, 0, 2), 3);
    CHECK_EQ("3rd", fs2_instr_posn(&s, &pat, 0, 3), 6);
    CHECK_EQ("4th", fs2_instr_posn(&s, &pat, 0, 4), -1);
    CHECK_EQ("nth=0", fs2_instr_posn(&s, &pat, 0, 0), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, instr_posn_from_pos) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("abcabcabc");
    fs2 pat = FS2LITERAL("abc");

    CHECK_EQ("from 1, 1st", fs2_instr_posn(&s, &pat, 1, 1), 3);
    CHECK_EQ("from 1, 2nd", fs2_instr_posn(&s, &pat, 1, 2), 6);
    CHECK_EQ("from 1, 3rd", fs2_instr_posn(&s, &pat, 1, 3), -1);
    CHECK_EQ("from 4, 1st", fs2_instr_posn(&s, &pat, 4, 1), 6);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, instr_posn_not_found) {
    __fs2_checkreset();
    fs2 s      = fs2_fromstr("hello world");
    fs2 pat    = FS2LITERAL("xyz");
    fs2 pat_ci = FS2LITERAL("WORLD");

    CHECK_EQ("missing",    fs2_instr_posn(&s, &pat,    0, 1), -1);
    CHECK_EQ("case miss",  fs2_instr_posn(&s, &pat_ci, 0, 1), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, instr_posn_pattern_too_long) {
    __fs2_checkreset();
    fs2 s         = fs2_fromstr("hi");
    fs2 pat_long  = FS2LITERAL("hello");
    fs2 pat_exact = FS2LITERAL("hi");
    fs2 pat_by1   = FS2LITERAL("hii");

    CHECK_EQ("too long", fs2_instr_posn(&s, &pat_long,  0, 1), -1);
    CHECK_EQ("exact len", fs2_instr_posn(&s, &pat_exact, 0, 1), 0);
    CHECK_EQ("by 1",     fs2_instr_posn(&s, &pat_by1,   0, 1), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, instr_posn_at_end) {
    __fs2_checkreset();
    fs2 s    = fs2_fromstr("hello");
    fs2 pat1 = FS2LITERAL("llo");
    fs2 pat2 = FS2LITERAL("o");

    CHECK_EQ("llo at 2", fs2_instr_posn(&s, &pat1, 0, 1), 2);
    CHECK_EQ("o at 4",   fs2_instr_posn(&s, &pat2, 0, 1), 4);

    fs2_free(&s);
    assert_balanced();
}

/* ⚠ Key: fs2_is_empty covers both cases — "" and FS2() */

Test(fs2_api, instr_posn_empty_pattern_literal) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("hello");
    fs2 pat = FS2EMPTY;                 /* STATIC v="", len=0 */

    CHECK_EQ("empty pattern", fs2_instr_posn(&s, &pat, 0, 1), 0);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, instr_posn_canonical_empty_pattern) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("hello");
    fs2 pat = FS2();                    /* canonical empty: v=NULL, len=0 */

    /* fs2_is_empty() catches this BEFORE strstr(NULL, ...) — otherwise Segfault */
    CHECK_EQ("canonical empty pattern", fs2_instr_posn(&s, &pat, 0, 1), 0);

    fs2_free(&s);
    fs2_free(&pat);
    assert_balanced();
}

Test(fs2_api, instr_posn_empty_haystack) {
    __fs2_checkreset();
    fs2 s_empty  = fs2_fromstr("");
    fs2 s_canon  = FS2();
    fs2 pat      = FS2LITERAL("x");

    CHECK_EQ("empty haystack",     fs2_instr_posn(&s_empty, &pat, 0, 1), -1);
    CHECK_EQ("canonical haystack", fs2_instr_posn(&s_canon, &pat, 0, 1), -1);

    fs2_free(&s_empty);
    fs2_free(&s_canon);
    assert_balanced();
}

Test(fs2_api, instr_posn_overlap) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("aaa");
    fs2 pat = FS2LITERAL("aa");

    CHECK_EQ("1st overlapping", fs2_instr_posn(&s, &pat, 0, 1), 0);
    CHECK_EQ("2nd overlapping", fs2_instr_posn(&s, &pat, 0, 2), 1);
    CHECK_EQ("3rd",             fs2_instr_posn(&s, &pat, 0, 3), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, instr_posn_null_str_aborts, .signal = SIGABRT) {
    fs2 pat = FS2LITERAL("x");
    fs2_instr_posn(NULL, &pat, 0, 1);
}

Test(fs2_api, instr_posn_null_pattern_aborts, .signal = SIGABRT) {
    fs2 s = fs2_fromstr("abc");
    fs2_instr_posn(&s, NULL, 0, 1);
    fs2_free(&s);
}

/* =========================================================================
 * Sugar (fs2 *pattern)
 * ========================================================================= */

Test(fs2_api, instr_from_basic) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("abcabcabc");
    fs2 pat = FS2LITERAL("abc");

    CHECK_EQ("from 0", fs2_instr_from(&s, &pat, 0), 0);
    CHECK_EQ("from 1", fs2_instr_from(&s, &pat, 1), 3);
    CHECK_EQ("from 4", fs2_instr_from(&s, &pat, 4), 6);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, instr_nth_basic) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("abcabcabc");
    fs2 pat = FS2LITERAL("abc");

    CHECK_EQ("nth 1", fs2_instr_nth(&s, &pat, 1), 0);
    CHECK_EQ("nth 2", fs2_instr_nth(&s, &pat, 2), 3);
    CHECK_EQ("nth 3", fs2_instr_nth(&s, &pat, 3), 6);
    CHECK_EQ("nth 4", fs2_instr_nth(&s, &pat, 4), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, instr_basic) {
    __fs2_checkreset();
    fs2 s      = fs2_fromstr("the quick brown fox");
    fs2 pat_q  = FS2LITERAL("quick");
    fs2 pat_f  = FS2LITERAL("fox");
    fs2 pat_c  = FS2LITERAL("cat");

    CHECK_EQ("quick", fs2_instr(&s, &pat_q), 4);
    CHECK_EQ("fox",   fs2_instr(&s, &pat_f), 16);
    CHECK_EQ("cat",   fs2_instr(&s, &pat_c), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, instr_with_alloc_pattern) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("needle in haystack");
    fs2 pat = fs2_fromstr("needle");   /* ALLOC, not STATIC */

    CHECK_EQ("alloc pattern", fs2_instr(&s, &pat), 0);

    fs2_free(&s);
    fs2_free(&pat);
    assert_balanced();
}

/* =========================================================================
 * Composite
 * ========================================================================= */

Test(fs2_api, composite_find_all_occurrences) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("a,b,c,d,e");
    fs2 pat = FS2LITERAL(",");

    size_t pos    = 0;
    int    commas = 0;

    ptrdiff_t hit;
    while ((hit = fs2_instr_from(&s, &pat, pos)) >= 0) {
        commas++;
        pos = (size_t)hit + 1;
    }

    CHECK_EQ("comma count", commas, 4);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, composite_split_by_occurrences) {
    __fs2_checkreset();
    fs2 s    = fs2_fromstr("key=value=with=equals");
    fs2 eq   = FS2LITERAL("=");

    ptrdiff_t eq1 = fs2_instr(&s, &eq);
    CHECK_EQ("first eq", eq1, 3);

    ptrdiff_t eq2 = fs2_instr_from(&s, &eq, (size_t)eq1 + 1);
    CHECK_EQ("second eq", eq2, 9);

    fs2 key   = fs2_substr_new(&s, 0, (size_t)eq1);
    fs2 value = fs2_substr_new(&s, (size_t)eq1 + 1, (size_t)eq2 - (size_t)eq1 - 1);

    CHECK_STR_EQ("key",   key.v,   "key");
    CHECK_STR_EQ("value", value.v, "value");

    fs2_free(&key);
    fs2_free(&value);
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_instr_posn_ci
 * ========================================================================= */

Test(fs2_api, instr_posn_ci_basic) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("Hello World");
    fs2 pat = FS2LITERAL("world");

    CHECK_EQ("lower needle, upper haystack", 
             fs2_instr_posn_ci(&s, &pat, 0, 1), 6);

    fs2 pat_up = FS2LITERAL("WORLD");
    CHECK_EQ("upper needle, mixed haystack",
             fs2_instr_posn_ci(&s, &pat_up, 0, 1), 6);

    fs2_free(&s);
    assert_balanced();
}

/* ⚠ Regression: sensitive must miss, CI must hit */
Test(fs2_api, instr_posn_ci_vs_sensitive) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("Hello World");
    fs2 pat = FS2LITERAL("world");

    CHECK_EQ("sensitive miss", fs2_instr_posn(&s, &pat, 0, 1), -1);
    CHECK_EQ("ci hit",         fs2_instr_posn_ci(&s, &pat, 0, 1), 6);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, instr_posn_ci_nth) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("AbCabcABC");   /* 3 case variants */
    fs2 pat = FS2LITERAL("abc");

    CHECK_EQ("1st (AbC)", fs2_instr_posn_ci(&s, &pat, 0, 1), 0);
    CHECK_EQ("2nd (abc)", fs2_instr_posn_ci(&s, &pat, 0, 2), 3);
    CHECK_EQ("3rd (ABC)", fs2_instr_posn_ci(&s, &pat, 0, 3), 6);
    CHECK_EQ("4th",       fs2_instr_posn_ci(&s, &pat, 0, 4), -1);
    CHECK_EQ("nth=0",     fs2_instr_posn_ci(&s, &pat, 0, 0), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, instr_posn_ci_from) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("abcABCabc");
    fs2 pat = FS2LITERAL("ABC");

    CHECK_EQ("from 0", fs2_instr_posn_ci(&s, &pat, 0, 1), 0);
    CHECK_EQ("from 1", fs2_instr_posn_ci(&s, &pat, 1, 1), 3);
    CHECK_EQ("from 4", fs2_instr_posn_ci(&s, &pat, 4, 1), 6);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, instr_posn_ci_overlap) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("AAA");
    fs2 pat = FS2LITERAL("aa");

    CHECK_EQ("1st overlapping", fs2_instr_posn_ci(&s, &pat, 0, 1), 0);
    CHECK_EQ("2nd overlapping", fs2_instr_posn_ci(&s, &pat, 0, 2), 1);
    CHECK_EQ("3rd",             fs2_instr_posn_ci(&s, &pat, 0, 3), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, instr_posn_ci_pattern_too_long) {
    __fs2_checkreset();
    fs2 s     = fs2_fromstr("Hi");
    fs2 p_long = FS2LITERAL("hello");
    fs2 p_exact = FS2LITERAL("hi");

    CHECK_EQ("too long",  fs2_instr_posn_ci(&s, &p_long,  0, 1), -1);
    CHECK_EQ("exact len", fs2_instr_posn_ci(&s, &p_exact, 0, 1), 0);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, instr_posn_ci_empty_pattern) {
    __fs2_checkreset();
    fs2 s        = fs2_fromstr("hello");
    fs2 p_empty  = FS2EMPTY;
    fs2 p_canon  = FS2();   /* canonical empty */

    CHECK_EQ("STATIC empty",    fs2_instr_posn_ci(&s, &p_empty, 0, 1), 0);
    CHECK_EQ("canonical empty", fs2_instr_posn_ci(&s, &p_canon, 0, 1), 0);

    fs2_free(&s);
    fs2_free(&p_canon);
    assert_balanced();
}

/* =========================================================================
 * Sugar — _ci variants
 * ========================================================================= */

Test(fs2_api, instr_ci_basic) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("the QUICK brown FOX");
    fs2 pq  = FS2LITERAL("quick");
    fs2 pf  = FS2LITERAL("fox");
    fs2 pn  = FS2LITERAL("cat");

    CHECK_EQ("quick", fs2_instr_ci(&s, &pq), 4);
    CHECK_EQ("fox",   fs2_instr_ci(&s, &pf), 16);
    CHECK_EQ("cat",   fs2_instr_ci(&s, &pn), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, instr_from_ci) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("abcabcabc");
    fs2 pat = FS2LITERAL("ABC");

    CHECK_EQ("from 0", fs2_instr_from_ci(&s, &pat, 0), 0);
    CHECK_EQ("from 1", fs2_instr_from_ci(&s, &pat, 1), 3);
    CHECK_EQ("from 4", fs2_instr_from_ci(&s, &pat, 4), 6);
    CHECK_EQ("from 7", fs2_instr_from_ci(&s, &pat, 7), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, instr_nth_ci) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("abcABCabc");
    fs2 pat = FS2LITERAL("AbC");

    CHECK_EQ("nth 1", fs2_instr_nth_ci(&s, &pat, 1), 0);
    CHECK_EQ("nth 2", fs2_instr_nth_ci(&s, &pat, 2), 3);
    CHECK_EQ("nth 3", fs2_instr_nth_ci(&s, &pat, 3), 6);
    CHECK_EQ("nth 4", fs2_instr_nth_ci(&s, &pat, 4), -1);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * Composite: case-insensitive split
 * ========================================================================= */

Test(fs2_api, composite_split_ci) {
    __fs2_checkreset();
    
    /* Parse "KEY=Value=With=Equals" — insensitive on '=' (no difference)
     * But for the real case: key-value with any case of the key */
    fs2 line   = fs2_fromstr("Content-Type: text/html");
    fs2 sep    = FS2LITERAL(":");
    
    ptrdiff_t colon = fs2_instr(&line, &sep);
    CHECK_EQ("colon at", colon, 12);
    
    fs2 key = fs2_substr_new(&line, 0, 12);
    fs2_substr(&key, 0, 12);
    /* Verify insensitive comparison: "content-type" == "Content-Type" */
    fs2 expected_key = FS2LITERAL("content-type");
    CHECK_EQ("key ci match", fs2_cmp_ci(&key, &expected_key), 0);
    
    fs2_free(&key);
    fs2_free(&line);
    assert_balanced();
}

Test(fs2_api, composite_find_all_ci) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("aXbxcXC");     /* we want to find all 'x' in any case */
    fs2 pat = FS2LITERAL("x");

    size_t pos   = 0;
    int    count = 0;
    ptrdiff_t hit;

    while ((hit = fs2_instr_from_ci(&s, &pat, pos)) >= 0) {
        count++;
        pos = (size_t)hit + 1;
    }

    CHECK_EQ("found 3 'x' variants", count, 3);

    fs2_free(&s);
    assert_balanced();
}
/* =========================================================================
 * Wrappers — instr_str/_ci variants
 * ========================================================================= */
Test(fs2_api, instr_str_null_pattern) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    /* fs2_wrap(NULL) → FS2EMPTY → fs2_is_empty → -1.
     * Никакого краша, никакого invraise. */
    CHECK_EQ("null pattern →  found",    fs2_instr_str(&s, NULL)    , 0);
    CHECK_EQ("null pattern ci",          fs2_instr_str_ci(&s, NULL) , 0);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, instr_str_empty_pattern) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    CHECK_EQ("empty pattern", fs2_instr_str(&s, ""), 0);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_instr_posn_str / _ci — base _str functions
 * ========================================================================= */

Test(fs2_api, instr_posn_str_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello world");

    CHECK_EQ("world at 6", fs2_instr_posn_str(&s, "world", 0, 1), 6);
    CHECK_EQ("l 1st at 2", fs2_instr_posn_str(&s, "l", 0, 1), 2);
    CHECK_EQ("l 3rd at 3", fs2_instr_posn_str(&s, "l", 0, 3), 9);
    CHECK_EQ("l 4th",      fs2_instr_posn_str(&s, "l", 0, 4), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, instr_posn_str_null_pattern) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    /* fs2_wrap(NULL) → FS2EMPTY → fs2_is_empty → -1. Никакого краша. */
    CHECK_EQ("null pattern", fs2_instr_posn_str(&s, NULL, 0, 1), 0);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, instr_posn_str_empty_pattern) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    CHECK_EQ("empty pattern", fs2_instr_posn_str(&s, "", 0, 1), 0);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, instr_posn_str_empty_haystack) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("");

    CHECK_EQ("empty haystack", fs2_instr_posn_str(&s, "x", 0, 1), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, instr_posn_str_canonical_empty_haystack) {
    __fs2_checkreset();
    fs2 s = FS2();   /* v == NULL */

    CHECK_EQ("canonical empty", fs2_instr_posn_str(&s, "x", 0, 1), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, instr_posn_str_null_aborts, .signal = SIGABRT) {
    fs2_instr_posn_str(NULL, "x", 0, 1);
}

Test(fs2_api, instr_posn_str_nth_zero) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abcabc");

    CHECK_EQ("nth=0", fs2_instr_posn_str(&s, "abc", 0, 0), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, instr_posn_str_pos_beyond) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");

    CHECK_EQ("pos=len",   fs2_instr_posn_str(&s, "a", 3, 1), -1);
    CHECK_EQ("pos>len",   fs2_instr_posn_str(&s, "a", 10, 1), -1);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_instr_posn_str_ci
 * ========================================================================= */

Test(fs2_api, instr_posn_str_ci_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("Hello World");

    CHECK_EQ("world ci at 6", fs2_instr_posn_str_ci(&s, "world", 0, 1), 6);
    CHECK_EQ("WORLD ci at 6", fs2_instr_posn_str_ci(&s, "WORLD", 0, 1), 6);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, instr_posn_str_ci_vs_sensitive) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("Hello World");

    /* Регресс: sensitive miss, ci hit */
    CHECK_EQ("sensitive miss", fs2_instr_posn_str(&s,    "world", 0, 1), -1);
    CHECK_EQ("ci hit",         fs2_instr_posn_str_ci(&s, "world", 0, 1), 6);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, instr_posn_str_ci_null_pattern) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    CHECK_EQ("null pattern ci", fs2_instr_posn_str_ci(&s, NULL, 0, 1), 0);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, instr_posn_str_ci_empty_pattern) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    CHECK_EQ("empty pattern ci", fs2_instr_posn_str_ci(&s, "", 0, 1), 0);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, instr_posn_str_ci_null_aborts, .signal = SIGABRT) {
    fs2_instr_posn_str_ci(NULL, "x", 0, 1);
}

/* =========================================================================
 * Sugar: fs2_instr_str / _from_str / _nth_str (sensitive)
 * ========================================================================= */

Test(fs2_api, instr_str_delegates) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abcabcabc");

    /* fs2_instr_str = posn_str(s, pat, 0, 1) */
    CHECK_EQ("instr_str == posn_str",
             fs2_instr_str(&s, "abc"),
             fs2_instr_posn_str(&s, "abc", 0, 1));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, instr_from_str_delegates) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abcabcabc");

    CHECK_EQ("from_str == posn_str",
             fs2_instr_from_str(&s, "abc", 4),
             fs2_instr_posn_str(&s, "abc", 4, 1));
    CHECK_EQ("value", fs2_instr_from_str(&s, "abc", 4), 6);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, instr_nth_str_delegates) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abcabcabc");

    CHECK_EQ("nth_str == posn_str",
             fs2_instr_nth_str(&s, "abc", 3),
             fs2_instr_posn_str(&s, "abc", 0, 3));
    CHECK_EQ("value", fs2_instr_nth_str(&s, "abc", 3), 6);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, instr_str_sugar_null_pattern) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    CHECK_EQ("instr_str",       fs2_instr_str(&s, NULL)         , 0);
    CHECK_EQ("instr_from_str",  fs2_instr_from_str(&s, NULL, 0) , 0);
    CHECK_EQ("instr_nth_str",   fs2_instr_nth_str(&s, NULL, 1)  , 0);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, instr_str_sugar_empty_pattern) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    CHECK_EQ("instr_str",       fs2_instr_str(&s, "")           , 0);
    CHECK_EQ("instr_from_str",  fs2_instr_from_str(&s, "", 0)   , 0);
    CHECK_EQ("instr_nth_str",   fs2_instr_nth_str(&s, "", 1)    , 0);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * Sugar: fs2_instr_str_ci / _from_str_ci / _nth_str_ci (insensitive)
 * ========================================================================= */

Test(fs2_api, instr_str_ci_delegates) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("Hello World");

    CHECK_EQ("instr_str_ci == posn_str_ci",
             fs2_instr_str_ci(&s, "world"),
             fs2_instr_posn_str_ci(&s, "world", 0, 1));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, instr_from_str_ci_delegates) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abcABCabc");

    CHECK_EQ("from_str_ci == posn_str_ci",
             fs2_instr_from_str_ci(&s, "abc", 1),
             fs2_instr_posn_str_ci(&s, "abc", 1, 1));
    CHECK_EQ("value", fs2_instr_from_str_ci(&s, "abc", 1), 3);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, instr_nth_str_ci_delegates) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abcABCabc");

    CHECK_EQ("nth_str_ci == posn_str_ci",
             fs2_instr_nth_str_ci(&s, "ABC", 2),
             fs2_instr_posn_str_ci(&s, "ABC", 0, 2));
    CHECK_EQ("value", fs2_instr_nth_str_ci(&s, "ABC", 2), 3);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, instr_str_ci_sugar_null_pattern) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    CHECK_EQ("instr_str_ci",      fs2_instr_str_ci(&s, NULL)        , 0);
    CHECK_EQ("instr_from_str_ci", fs2_instr_from_str_ci(&s, NULL, 0), 0);
    CHECK_EQ("instr_nth_str_ci",  fs2_instr_nth_str_ci(&s, NULL, 1) , 0);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, instr_str_ci_sugar_empty_pattern) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    CHECK_EQ("instr_str_ci",      fs2_instr_str_ci(&s, "")          , 0);
    CHECK_EQ("instr_from_str_ci", fs2_instr_from_str_ci(&s, "", 0)  , 0);
    CHECK_EQ("instr_nth_str_ci",  fs2_instr_nth_str_ci(&s, "", 1)   , 0);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * Full matrix: все 8 _str функций на NULL-паттерне — без крашей
 * ========================================================================= */

Test(fs2_api, instr_all_str_variants_null_pattern_no_crash) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("some text");

    /* Все должны вернуть -1, без segfault */
    CHECK_EQ("posn_str",        fs2_instr_posn_str       (&s, NULL, 0, 1), 0);
    CHECK_EQ("posn_str_ci",     fs2_instr_posn_str_ci    (&s, NULL, 0, 1), 0);
    CHECK_EQ("instr_str",       fs2_instr_str            (&s, NULL),       0);
    CHECK_EQ("instr_str_ci",    fs2_instr_str_ci         (&s, NULL),       0);
    CHECK_EQ("instr_from_str",  fs2_instr_from_str       (&s, NULL, 0),    0);
    CHECK_EQ("instr_from_str_ci", fs2_instr_from_str_ci  (&s, NULL, 0),    0);
    CHECK_EQ("instr_nth_str",   fs2_instr_nth_str        (&s, NULL, 1),    0);
    CHECK_EQ("instr_nth_str_ci", fs2_instr_nth_str_ci    (&s, NULL, 1),    0);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, instr_all_str_variants_empty_pattern_no_crash) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("some text");

    CHECK_EQ("posn_str",        fs2_instr_posn_str       (&s, "", 0, 1), 0);
    CHECK_EQ("posn_str_ci",     fs2_instr_posn_str_ci    (&s, "", 0, 1), 0);
    CHECK_EQ("instr_str",       fs2_instr_str            (&s, ""),       0);
    CHECK_EQ("instr_str_ci",    fs2_instr_str_ci         (&s, ""),       0);
    CHECK_EQ("instr_from_str",  fs2_instr_from_str       (&s, "", 0),    0);
    CHECK_EQ("instr_from_str_ci", fs2_instr_from_str_ci  (&s, "", 0),    0);
    CHECK_EQ("instr_nth_str",   fs2_instr_nth_str        (&s, "", 1),    0);
    CHECK_EQ("instr_nth_str_ci", fs2_instr_nth_str_ci    (&s, "", 1),    0);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_replace — same length
 * ========================================================================= */

Test(fs2_api, replace_same_length) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("hello world");
    fs2 pat = FS2LITERAL("o");
    fs2 rep = FS2LITERAL("0");

    fs2_replace(&s, &pat, &rep);

    CHECK_STR_EQ("content", s.v, "hell0 w0rld");
    CHECK_EQ("len unchanged", s.len, 11);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_replace — growing
 * ========================================================================= */

Test(fs2_api, replace_growing) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("a b c");
    fs2 pat = FS2LITERAL(" ");
    fs2 rep = FS2LITERAL("__");

    fs2_replace(&s, &pat, &rep);

    CHECK_STR_EQ("content", s.v, "a__b__c");
    CHECK_EQ("len", s.len, 7);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, replace_massive_growth) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("x x x");
    fs2 pat = FS2LITERAL("x");
    fs2 rep = FS2LITERAL("0123456789");

    fs2_replace(&s, &pat, &rep);

    CHECK_STR_EQ("content", s.v, "0123456789 0123456789 0123456789");
    CHECK_EQ("len", s.len, 32);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_replace — shrinking
 * ========================================================================= */

Test(fs2_api, replace_shrinking) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("aXXbXXc");
    fs2 pat = FS2LITERAL("XX");
    fs2 rep = FS2LITERAL("-");

    fs2_replace(&s, &pat, &rep);

    CHECK_STR_EQ("content", s.v, "a-b-c");
    CHECK_EQ("len", s.len, 5);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, replace_delete) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("hello");
    fs2 pat = FS2LITERAL("l");
    fs2 rep = FS2EMPTY;

    fs2_replace(&s, &pat, &rep);

    CHECK_STR_EQ("content", s.v, "heo");
    CHECK_EQ("len", s.len, 3);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, replace_delete_all) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("aaa");
    fs2 pat = FS2LITERAL("a");
    fs2 rep = FS2EMPTY;

    fs2_replace(&s, &pat, &rep);

    CHECK_STR_EQ("content", s.v, "");
    CHECK_EQ("len", s.len, 0);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_replace — edge cases
 * ========================================================================= */

Test(fs2_api, replace_pattern_not_found) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("hello");
    fs2 pat = FS2LITERAL("xyz");
    fs2 rep = FS2LITERAL("ABC");

    fs2_replace(&s, &pat, &rep);

    CHECK_STR_EQ("content unchanged", s.v, "hello");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, replace_empty_pattern) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("hello");
    fs2 pat = FS2();
    fs2 rep = FS2LITERAL("X");

    fs2_replace(&s, &pat, &rep);

    CHECK_STR_EQ("content unchanged", s.v, "hello");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    fs2_free(&pat);
    assert_balanced();
}

Test(fs2_api, replace_pat_longer_than_string) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("hi");
    fs2 pat = FS2LITERAL("hello");
    fs2 rep = FS2LITERAL("X");

    fs2_replace(&s, &pat, &rep);

    CHECK_STR_EQ("content unchanged", s.v, "hi");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, replace_canonical_empty_s) {
    __fs2_checkreset();
    fs2 s   = FS2();
    fs2 pat = FS2LITERAL("x");
    fs2 rep = FS2LITERAL("y");

    fs2_replace(&s, &pat, &rep);

    CHECK_EQ("still empty", s.len, 0);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_replace — non-overlapping semantics (Python-style)
 * ========================================================================= */

Test(fs2_api, replace_non_overlapping) {
    __fs2_checkreset();
    /* Python: "aaa".replace("aa", "b") = "ba" */
    fs2 s   = fs2_fromstr("aaa");
    fs2 pat = FS2LITERAL("aa");
    fs2 rep = FS2LITERAL("b");

    fs2_replace(&s, &pat, &rep);

    CHECK_STR_EQ("non-overlapping like Python", s.v, "ba");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, replace_no_recursive) {
    __fs2_checkreset();
    /* "x".replace("x", "xx") = "xx" (не "xxxx" — не ресканируем вставленное) */
    fs2 s   = fs2_fromstr("x");
    fs2 pat = FS2LITERAL("x");
    fs2 rep = FS2LITERAL("xx");

    fs2_replace(&s, &pat, &rep);

    CHECK_STR_EQ("no recursive replacement", s.v, "xx");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_replace — NULL arguments
 * ========================================================================= */

Test(fs2_api, replace_null_s_aborts, .signal = SIGABRT) {
    fs2 pat = FS2LITERAL("x");
    fs2 rep = FS2LITERAL("y");
    fs2_replace(NULL, &pat, &rep);
}

Test(fs2_api, replace_null_pat_aborts, .signal = SIGABRT) {
    fs2 s   = fs2_fromstr("abc");
    fs2 rep = FS2LITERAL("y");
    fs2_replace(&s, NULL, &rep);
    fs2_free(&s);
}

Test(fs2_api, replace_null_repl_aborts, .signal = SIGABRT) {
    fs2 s   = fs2_fromstr("abc");
    fs2 pat = FS2LITERAL("x");
    fs2_replace(&s, &pat, NULL);
    fs2_free(&s);
}

/* =========================================================================
 * Composite: HTML escape
 * ========================================================================= */

Test(fs2_api, composite_replace_html_escape) {
    __fs2_checkreset();
    fs2 s    = fs2_fromstr("<b>bold</b>");
    fs2 lt   = FS2LITERAL("<");
    fs2 amp_lt = FS2LITERAL("&lt;");
    fs2 gt   = FS2LITERAL(">");
    fs2 amp_gt = FS2LITERAL("&gt;");

    fs2_replace(&s, &lt, &amp_lt);
    fs2_replace(&s, &gt, &amp_gt);

    CHECK_STR_EQ("escaped", s.v, "&lt;b&gt;bold&lt;/b&gt;");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, replace_with_realloc) {
    __fs2_checkreset();
    /* Маленький буфер, огромное расширение — гарантированный realloc */
    fs2 s   = fs2_fromstr("a|b|c");
    fs2 pat = FS2LITERAL("|");
    
    char big[256];
    memset(big, 'X', sizeof(big) - 1);
    big[sizeof(big) - 1] = '\0';
    fs2 rep = fs2_fromstr(big);
    
    size_t orig_sz = s.sz;
    fs2_replace(&s, &pat, &rep);
    
    CHECK_TRUE("buffer grew massively", s.sz > orig_sz);
    CHECK_EQ  ("len", s.len, 1 + 255 + 1 + 255 + 1);
    CHECK_EQ  ("a intact", s.v[0], 'a');
    CHECK_EQ  ("b after repl", s.v[256], 'b');
    CHECK_FS2VALID(1, &s);
    
    fs2_free(&s);
    fs2_free(&rep);
    assert_balanced();
}
/* =========================================================================
 * fs2_replace c-str wrappers
 * ========================================================================= */

Test(fs2_api, replace_cstr_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello world");

    fs2_replace_cstr(&s, "world", "there");

    CHECK_STR_EQ("content", s.v, "hello there");
    CHECK_EQ("len", s.len, 11);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, replace_cstr_null_repl_deletes) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("aXbXc");

    fs2_replace_cstr(&s, "X", NULL);

    CHECK_STR_EQ("deleted", s.v, "abc");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, replace_cstr_null_pat_noop) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    fs2_replace_cstr(&s, NULL, "X");

    CHECK_STR_EQ("unchanged", s.v, "hello");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, replace_cstr_empty_pat_noop) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    fs2_replace_cstr(&s, "", "X");

    CHECK_STR_EQ("unchanged", s.v, "hello");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, replace_cstr_both_null_noop) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    fs2_replace_cstr(&s, NULL, NULL);

    CHECK_STR_EQ("unchanged", s.v, "hello");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, replace_cstr_equivalent_to_base) {
    __fs2_checkreset();
    fs2 s1 = fs2_fromstr("x.y.z");
    fs2 s2 = fs2_fromstr("x.y.z");

    fs2 pat  = FS2LITERAL(".");
    fs2 repl = FS2LITERAL("::");

    fs2_replace      (&s1, &pat, &repl);
    fs2_replace_cstr (&s2, ".", "::");

    CHECK_STR_EQ("same result", s1.v, s2.v);
    CHECK_FS2VALID(1, &s1);
    CHECK_FS2VALID(2, &s2);

    fs2_free(&s1);
    fs2_free(&s2);
    assert_balanced();
}

Test(fs2_api, composite_replace_cstr_template) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("Hello, {name}! You are {age} years old.");

    fs2_replace_cstr(&s, "{name}", "Alice");
    fs2_replace_cstr(&s, "{age}",  "30");

    CHECK_STR_EQ("rendered", s.v, "Hello, Alice! You are 30 years old.");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_trim — both sides
 * ========================================================================= */

Test(fs2_api, trim_whitespace) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("  \t hello world \n ");
    fs2 pat = FS2LITERAL(" \t\n\r\v\f");

    fs2_trim(&s, &pat);

    CHECK_STR_EQ("trimmed", s.v, "hello world");
    CHECK_EQ("len", s.len, 11);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, trim_left_only) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("  hello  ");
    fs2 pat = FS2LITERAL(" ");

    fs2_ltrim(&s, &pat);

    CHECK_STR_EQ("left trimmed", s.v, "hello  ");
    CHECK_EQ("len", s.len, 7);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, trim_right_only) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("  hello  ");
    fs2 pat = FS2LITERAL(" ");

    fs2_rtrim(&s, &pat);

    CHECK_STR_EQ("right trimmed", s.v, "  hello");
    CHECK_EQ("len", s.len, 7);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, trim_both_sides) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("xxxHELLOxxx");
    fs2 pat = FS2LITERAL("x");

    fs2_trim(&s, &pat);

    CHECK_STR_EQ("trimmed", s.v, "HELLO");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, trim_multichar_pattern) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("xyxabcxyx");
    fs2 pat = FS2LITERAL("xy");

    fs2_trim(&s, &pat);

    /* x, y, x all in pattern → trimmed; abc unchanged */
    CHECK_STR_EQ("trimmed", s.v, "abc");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_trim — edge cases
 * ========================================================================= */

Test(fs2_api, trim_nothing_to_trim) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("hello");
    fs2 pat = FS2LITERAL(" ");

    fs2_trim(&s, &pat);

    CHECK_STR_EQ("unchanged", s.v, "hello");
    CHECK_EQ("len", s.len, 5);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, trim_all_content) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("     ");
    fs2 pat = FS2LITERAL(" ");

    fs2_trim(&s, &pat);

    CHECK_STR_EQ("empty", s.v, "");
    CHECK_EQ("len", s.len, 0);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, trim_empty_string) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("");
    fs2 pat = FS2LITERAL(" ");

    fs2_trim(&s, &pat);

    CHECK_EQ("len", s.len, 0);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, trim_canonical_empty) {
    __fs2_checkreset();
    fs2 s   = FS2();   /* v == NULL */
    fs2 pat = FS2LITERAL(" ");

    fs2_trim(&s, &pat);

    CHECK_EQ("len", s.len, 0);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, trim_empty_pattern_noop) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("  hello  ");
    fs2 pat = FS2();

    fs2_trim(&s, &pat);

    CHECK_STR_EQ("unchanged", s.v, "  hello  ");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    fs2_free(&pat);
    assert_balanced();
}

Test(fs2_api, trim_pattern_covers_all) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("abcabc");
    fs2 pat = FS2LITERAL("abc");

    fs2_trim(&s, &pat);

    CHECK_STR_EQ("all trimmed", s.v, "");
    CHECK_EQ("len", s.len, 0);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * c-string pattern wrappers
 * ========================================================================= */

Test(fs2_api, trim_str_whitespace) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("  hello  ");

    fs2_trim_str(&s, " \t\n\r\v\f");

    CHECK_STR_EQ("trimmed", s.v, "hello");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, trim_str_null_pattern_noop) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("  hello  ");

    /* fs2_wrap(NULL) → FS2EMPTY → no-op */
    fs2_trim_str(&s, NULL);

    CHECK_STR_EQ("unchanged", s.v, "  hello  ");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, trim_left_str_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr(">>>hello");

    fs2_ltrim_str(&s, ">");

    CHECK_STR_EQ("left trimmed", s.v, "hello");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, trim_right_str_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello<<<");

    fs2_rtrim_str(&s, "<");

    CHECK_STR_EQ("right trimmed", s.v, "hello");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * Non-ASCII bytes — regression for (unsigned char) cast
 * ========================================================================= */

Test(fs2_api, trim_non_ascii) {
    __fs2_checkreset();
    /* UTF-8 continuation byte 0x80 at the edges */
    fs2 s   = fs2_fromstr("\x80\x80hello\x80");
    fs2 pat = fs2_fromstr("\x80");

    fs2_trim(&s, &pat);

    CHECK_STR_EQ("trimmed", s.v, "hello");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    fs2_free(&pat);
    assert_balanced();
}

/* =========================================================================
 * Composite: trim then parse
 * ========================================================================= */

Test(fs2_api, composite_trim_then_parse_int) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("  \t42 \n");

    fs2_trim_str(&s, " \t\n\r\v\f");

    int val = 0;
    CHECK_TRUE("parse after trim", fs2_tryparse_int(&s, &val, true));
    CHECK_EQ("val", val, 42);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, composite_trim_csv_field) {
    __fs2_checkreset();
    fs2 line = fs2_fromstr("  alice  ,  bob  ,  charlie  ");
    
    /* Разбиваем по ',' и тримим каждое поле */
    fs2_trim_str(&line, " ");
    CHECK_STR_EQ("trimmed line", line.v, "alice  ,  bob  ,  charlie");

    fs2_free(&line);
    assert_balanced();
}

Test(fs2_api, btrim_equivalent_to_trim) {
    __fs2_checkreset();
    fs2 s1 = fs2_fromstr("  hello  ");
    fs2 s2 = fs2_fromstr("  hello  ");
    fs2 pat = FS2LITERAL(" ");

    fs2_trim (&s1, &pat);
    fs2_trim(&s2, &pat);

    CHECK_STR_EQ("same result", s1.v, s2.v);
    CHECK_STR_EQ("content", s2.v, "hello");
    CHECK_FS2VALID(1, &s1);
    CHECK_FS2VALID(2, &s2);

    fs2_free(&s1);
    fs2_free(&s2);
    assert_balanced();
}

Test(fs2_api, ltrim_rtrim_btrim_matrix) {
    __fs2_checkreset();

    fs2 s_l = fs2_fromstr("  hello  ");
    fs2 s_r = fs2_fromstr("  hello  ");
    fs2 s_b = fs2_fromstr("  hello  ");
    fs2 pat = FS2LITERAL(" ");

    fs2_ltrim(&s_l, &pat);
    fs2_rtrim(&s_r, &pat);
    fs2_trim(&s_b, &pat);

    CHECK_STR_EQ("ltrim keeps right", s_l.v, "hello  ");
    CHECK_STR_EQ("rtrim keeps left",  s_r.v, "  hello");
    CHECK_STR_EQ("btrim both",        s_b.v, "hello");

    fs2_free(&s_l);
    fs2_free(&s_r);
    fs2_free(&s_b);
    assert_balanced();
}

/* =========================================================================
 * fs2_starts_with / fs2_ends_with
 * ========================================================================= */

Test(fs2_api, starts_with_basic) {
    __fs2_checkreset();
    fs2 s  = fs2_fromstr("hello world");
    fs2 he = FS2LITERAL("he");
    fs2 hl = FS2LITERAL("hello");
    fs2 wo = FS2LITERAL("wo");
    fs2 x  = FS2LITERAL("x");

    CHECK_TRUE ("'he' is prefix",    fs2_starts_with(&s, &he));
    CHECK_TRUE ("whole is prefix",   fs2_starts_with(&s, &hl));
    CHECK_FALSE("'wo' not prefix",   fs2_starts_with(&s, &wo));
    CHECK_FALSE("'x' not prefix",    fs2_starts_with(&s, &x));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, ends_with_basic) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("hello world");
    fs2 ld  = FS2LITERAL("ld");
    fs2 wld = FS2LITERAL("world");
    fs2 lo  = FS2LITERAL("lo");
    fs2 x   = FS2LITERAL("x");

    CHECK_TRUE ("'ld' is suffix",    fs2_ends_with(&s, &ld));
    CHECK_TRUE ("'world' is suffix", fs2_ends_with(&s, &wld));
    CHECK_FALSE("'lo' not suffix",   fs2_ends_with(&s, &lo));
    CHECK_FALSE("'x' not suffix",    fs2_ends_with(&s, &x));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, starts_ends_with_equal_string) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    CHECK_TRUE("whole string is prefix", fs2_starts_with(&s, &s));
    CHECK_TRUE("whole string is suffix", fs2_ends_with(&s, &s));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, starts_ends_pattern_longer) {
    __fs2_checkreset();
    fs2 s   = fs2_fromstr("hi");
    fs2 pat = FS2LITERAL("hello");

    CHECK_FALSE("pattern longer, starts", fs2_starts_with(&s, &pat));
    CHECK_FALSE("pattern longer, ends",   fs2_ends_with(&s, &pat));

    fs2_free(&s);
    assert_balanced();
}

/* ⚠ Ключевые: пустой паттерн → true, и NULL v не крашит */

Test(fs2_api, starts_ends_empty_pattern) {
    __fs2_checkreset();
    fs2 s      = fs2_fromstr("hello");
    fs2 empty1 = FS2EMPTY;
    fs2 empty2 = FS2();   /* canonical empty: v=NULL, len=0 */

    /* Пустой префикс/суффикс — всегда true */
    CHECK_TRUE("literal empty prefix", fs2_starts_with(&s, &empty1));
    CHECK_TRUE("literal empty suffix", fs2_ends_with(&s, &empty1));
    CHECK_TRUE("canonical empty prefix", fs2_starts_with(&s, &empty2));
    CHECK_TRUE("canonical empty suffix", fs2_ends_with(&s, &empty2));

    fs2_free(&s);
    fs2_free(&empty2);
    assert_balanced();
}

Test(fs2_api, starts_ends_canonical_empty_s) {
    __fs2_checkreset();
    fs2 s_empty = FS2();            /* v=NULL, len=0 */
    fs2 s_lit   = FS2EMPTY;         /* v="", len=0 */
    fs2 pat     = FS2LITERAL("x");
    fs2 pat_e   = FS2();

    /* Пустая строка + пустой паттерн → true */
    CHECK_TRUE("canonical empty + empty",   fs2_starts_with(&s_empty, &pat_e));
    CHECK_TRUE("literal empty + empty",     fs2_starts_with(&s_lit,   &pat_e));

    /* Пустая строка + непустой паттерн → false (не доходит до memcmp) */
    CHECK_FALSE("canonical empty + 'x'",    fs2_starts_with(&s_empty, &pat));
    CHECK_FALSE("literal empty + 'x'",      fs2_starts_with(&s_lit,   &pat));
    CHECK_FALSE("canonical empty ends 'x'", fs2_ends_with  (&s_empty, &pat));

    fs2_free(&s_empty);
    fs2_free(&pat_e);
    assert_balanced();
}

/* =========================================================================
 * c-string wrappers
 * ========================================================================= */

Test(fs2_api, starts_with_str_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello world");

    CHECK_TRUE ("'he'",  fs2_starts_with_str(&s, "he"));
    CHECK_TRUE ("'hello'", fs2_starts_with_str(&s, "hello"));
    CHECK_FALSE("'wo'",  fs2_starts_with_str(&s, "wo"));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, ends_with_str_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello world");

    CHECK_TRUE ("'ld'",    fs2_ends_with_str(&s, "ld"));
    CHECK_TRUE ("'world'", fs2_ends_with_str(&s, "world"));
    CHECK_FALSE("'lo'",    fs2_ends_with_str(&s, "lo"));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, starts_ends_with_str_null) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    /* fs2_wrap(NULL) → FS2EMPTY → пустой префикс → true */
    CHECK_TRUE("NULL prefix → true", fs2_starts_with_str(&s, NULL));
    CHECK_TRUE("NULL suffix → true", fs2_ends_with_str(&s, NULL));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, starts_ends_with_str_empty) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    CHECK_TRUE("empty prefix", fs2_starts_with_str(&s, ""));
    CHECK_TRUE("empty suffix", fs2_ends_with_str(&s, ""));

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * NULL fs2 arguments abort
 * ========================================================================= */

Test(fs2_api, starts_with_null_s_aborts, .signal = SIGABRT) {
    fs2 pat = FS2LITERAL("x");
    fs2_starts_with(NULL, &pat);
}

Test(fs2_api, starts_with_null_prefix_aborts, .signal = SIGABRT) {
    fs2 s = fs2_fromstr("abc");
    fs2_starts_with(&s, NULL);
    fs2_free(&s);
}

Test(fs2_api, ends_with_null_s_aborts, .signal = SIGABRT) {
    fs2 pat = FS2LITERAL("x");
    fs2_ends_with(NULL, &pat);
}

Test(fs2_api, ends_with_null_suffix_aborts, .signal = SIGABRT) {
    fs2 s = fs2_fromstr("abc");
    fs2_ends_with(&s, NULL);
    fs2_free(&s);
}

/* =========================================================================
 * Non-ASCII — memcmp безопасен для любых байтов
 * ========================================================================= */

Test(fs2_api, starts_with_non_ascii) {
    __fs2_checkreset();
    /* UTF-8: "привет" = D0 BF D1 80 D0 B8 D0 B2 D0 B5 D1 82 */
    fs2 s    = fs2_fromstr("\xD0\xBF\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82");
    fs2 head = fs2_fromstr("\xD0\xBF\xD1\x80");  /* "пр" */

    CHECK_TRUE("UTF-8 prefix", fs2_starts_with(&s, &head));
    CHECK_TRUE("UTF-8 suffix", fs2_ends_with(&s, &(fs2){.v="\xD0\xB5\xD1\x82", .len=4, .sz=5, .flags=FS2_STATIC}));

    fs2_free(&s);
    fs2_free(&head);
    assert_balanced();
}

/* =========================================================================
 * Composite: file extension check
 * ========================================================================= */

Test(fs2_api, composite_file_extension) {
    __fs2_checkreset();
    fs2 path = fs2_fromstr("main.c");

    CHECK_TRUE ("starts with 'm'", fs2_starts_with_str(&path, "m"));
    CHECK_TRUE ("ends with '.c'",   fs2_ends_with_str(&path, ".c"));
    CHECK_FALSE("ends with '.h'",   fs2_ends_with_str(&path, ".h"));

    fs2_free(&path);
    assert_balanced();
}

Test(fs2_api, composite_http_status_check) {
    __fs2_checkreset();
    fs2 status_line = fs2_fromstr("HTTP/1.1 200 OK");

    CHECK_TRUE("starts with HTTP/", fs2_starts_with_str(&status_line, "HTTP/"));
    CHECK_TRUE("ends with OK",      fs2_ends_with_str(&status_line, "OK"));

    fs2_free(&status_line);
    assert_balanced();
}
// composite, fs2_starts_with vs fs2_instr
Test(fs2_api, starts_with_consistent_with_instr) {
    __fs2_checkreset();
    
    fs2 s = fs2_fromstr("hello");
    
    /* Non-empty pattern — согласовано */
    fs2 he = FS2LITERAL("he");
    fs2 x  = FS2LITERAL("x");
    
    CHECK_EQ("he", 
             fs2_starts_with(&s, &he),
             fs2_instr(&s, &he) == 0);
    
    CHECK_EQ("x",
             fs2_starts_with(&s, &x),
             fs2_instr(&s, &x) == 0);
    
    /* ⚠ Пустой паттерн — РАСХОЖДЕНИЕ */
    fs2 empty = FS2EMPTY;
    
    CHECK_TRUE("starts_with(hello, '') == true",  
               fs2_starts_with(&s, &empty));
    
    CHECK_EQ("instr(hello, '') == 0",
             fs2_instr(&s, &empty), 0);
    
    /* Инвариант, который должен держаться, но не держится: */
    CHECK_EQ("EQUIVALENCE BROKEN on empty pattern",
             fs2_starts_with(&s, &empty),
             fs2_instr(&s, &empty) == 0);
    /*   ^ true          ^ (-1 == 0) == false
     *   → ASSERTION FAILS
     */
    
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_contains
 * ========================================================================= */

Test(fs2_api, contains_basic) {
    __fs2_checkreset();
    fs2 s     = fs2_fromstr("hello world");
    fs2 world = FS2LITERAL("world");
    fs2 xyz   = FS2LITERAL("xyz");
    fs2 hl    = FS2LITERAL("hello world");   /* whole string */

    CHECK_TRUE ("'world'",  fs2_contains(&s, &world));
    CHECK_FALSE("'xyz'",    fs2_contains(&s, &xyz));
    CHECK_TRUE ("whole",    fs2_contains(&s, &hl));

    fs2_free(&s);
    assert_balanced();
}

/* ⚠ Ключевой: пустой паттерн — Python/JS semantics → true */
Test(fs2_api, contains_empty_pattern) {
    __fs2_checkreset();
    fs2 s      = fs2_fromstr("hello");
    fs2 empty1 = FS2EMPTY;
    fs2 empty2 = FS2();

    CHECK_TRUE("FS2EMPTY pattern",   fs2_contains(&s, &empty1));
    CHECK_TRUE("canonical empty",    fs2_contains(&s, &empty2));

    fs2_free(&s);
    fs2_free(&empty2);
    assert_balanced();
}

Test(fs2_api, contains_empty_haystack) {
    __fs2_checkreset();
    fs2 s     = fs2_fromstr("");
    fs2 canon = FS2();
    fs2 x     = FS2LITERAL("x");
    fs2 empty = FS2();

    /* Непустой паттерн в пустой строке — false */
    CHECK_FALSE("empty + 'x'",     fs2_contains(&s, &x));
    CHECK_FALSE("canonical + 'x'", fs2_contains(&canon, &x));

    /* Пустой паттерн в пустой строке — true (Python) */
    CHECK_TRUE("empty + empty",     fs2_contains(&s, &empty));
    CHECK_TRUE("canonical + empty", fs2_contains(&canon, &empty));

    fs2_free(&s);
    fs2_free(&canon);
    fs2_free(&empty);
    assert_balanced();
}

Test(fs2_api, contains_str_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello world");

    CHECK_TRUE ("world", fs2_contains_str(&s, "world"));
    CHECK_FALSE("xyz",   fs2_contains_str(&s, "xyz"));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, contains_str_null_pattern) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    /* NULL → FS2EMPTY → empty pattern → true (consistent with "" ) */
    CHECK_TRUE("NULL pattern", fs2_contains_str(&s, NULL));
    CHECK_TRUE("empty pattern", fs2_contains_str(&s, ""));
    CHECK_EQ ("null == empty",
              fs2_contains_str(&s, NULL),
              fs2_contains_str(&s, ""));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, contains_null_aborts, .signal = SIGABRT) {
    fs2 pat = FS2LITERAL("x");
    fs2_contains(NULL, &pat);
}

/* =========================================================================
 * fs2_count — non-overlapping semantics
 * ========================================================================= */

Test(fs2_api, count_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello world");

    CHECK_EQ("'o' twice",      fs2_count_str(&s, "o"), 2);
    CHECK_EQ("'l' three",      fs2_count_str(&s, "l"), 3);
    CHECK_EQ("'world' once",   fs2_count_str(&s, "world"), 1);
    CHECK_EQ("'xyz' zero",     fs2_count_str(&s, "xyz"), 0);

    fs2_free(&s);
    assert_balanced();
}

/* ⚠ Главный тест: non-overlapping, как Python */
Test(fs2_api, count_non_overlapping) {
    __fs2_checkreset();

    fs2 s1 = fs2_fromstr("aaa");
    fs2 p1 = FS2LITERAL("aa");
    /* Python: "aaa".count("aa") == 1 */
    CHECK_EQ("'aaa' / 'aa' = 1", fs2_count(&s1, &p1), 1);

    fs2 s2 = fs2_fromstr("aaaa");
    fs2 p2 = FS2LITERAL("aa");
    /* Python: "aaaa".count("aa") == 2 */
    CHECK_EQ("'aaaa' / 'aa' = 2", fs2_count(&s2, &p2), 2);

    fs2 s3 = fs2_fromstr("aaaaa");
    /* Python: "aaaaa".count("aa") == 2 (позиции 0, 2, остаток "a") */
    CHECK_EQ("'aaaaa' / 'aa' = 2", fs2_count(&s3, &p2), 2);

    fs2_free(&s1);
    fs2_free(&s2);
    fs2_free(&s3);
    assert_balanced();
}

Test(fs2_api, count_empty_pattern_zero) {
    __fs2_checkreset();
    fs2 s      = fs2_fromstr("hello");
    fs2 empty1 = FS2EMPTY;
    fs2 empty2 = FS2();

    /* Расходимся с Python (len+1), но осмысленнее: 0 */
    CHECK_EQ("FS2EMPTY → 0",   fs2_count(&s, &empty1), 0);
    CHECK_EQ("canonical → 0",  fs2_count(&s, &empty2), 0);

    fs2_free(&s);
    fs2_free(&empty2);
    assert_balanced();
}

Test(fs2_api, count_pattern_not_found) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    CHECK_EQ("xyz not found", fs2_count_str(&s, "xyz"), 0);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, count_empty_string) {
    __fs2_checkreset();
    fs2 s_empty = fs2_fromstr("");
    fs2 s_canon = FS2();

    CHECK_EQ("empty → 0",     fs2_count_str(&s_empty, "x"), 0);
    CHECK_EQ("canonical → 0", fs2_count_str(&s_canon, "x"), 0);

    fs2_free(&s_empty);
    fs2_free(&s_canon);
    assert_balanced();
}

Test(fs2_api, count_str_null_pattern) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    /* NULL → FS2EMPTY → count 0 */
    CHECK_EQ("NULL pattern → 0", fs2_count_str(&s, NULL), 0);
    CHECK_EQ("empty pattern → 0", fs2_count_str(&s, ""), 0);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, count_null_aborts, .signal = SIGABRT) {
    fs2 pat = FS2LITERAL("x");
    fs2_count(NULL, &pat);
}

Test(fs2_api, count_null_pattern_aborts, .signal = SIGABRT) {
    fs2 s = fs2_fromstr("abc");
    fs2_count(&s, NULL);
    fs2_free(&s);
}

/* =========================================================================
 * Consistency: contains(s,p) == (count(s,p) > 0) для непустого p
 * ========================================================================= */

Test(fs2_api, contains_consistent_with_count) {
    __fs2_checkreset();

    const char *haystacks[] = { "hello", "aaa", "aaaa", "", "abcabc" };
    const char *patterns[]  = { "a", "aa", "abc", "xyz", "l" };

    for (size_t i = 0; i < sizeof(haystacks) / sizeof(haystacks[0]); i++) {
        fs2 s = fs2_fromstr(haystacks[i]);
        for (size_t j = 0; j < sizeof(patterns) / sizeof(patterns[0]); j++) {
            fs2 p = fs2_wrap(patterns[j]);
            CHECK_EQ("contains == (count > 0)",
                     fs2_contains(&s, &p),
                     fs2_count(&s, &p) > 0);
        }
        fs2_free(&s);
    }
    assert_balanced();
}

/* =========================================================================
 * Composite: word frequency
 * ========================================================================= */

Test(fs2_api, composite_count_keywords) {
    __fs2_checkreset();
    fs2 text = fs2_fromstr("the cat and the dog and the bird");

    CHECK_EQ("'the' 3 times", fs2_count_str(&text, "the"), 3);
    CHECK_EQ("'and' 2 times", fs2_count_str(&text, "and"), 2);
    CHECK_EQ("'cat' 1 time",  fs2_count_str(&text, "cat"), 1);
    CHECK_EQ("'fox' 0 times", fs2_count_str(&text, "fox"), 0);

    fs2_free(&text);
    assert_balanced();
}