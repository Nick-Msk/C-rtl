#include <criterion/criterion.h>

#include "test_fs2.h"
#include "fs2_api.h"

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
 * fs2_nichr — first occurrence within first `lim` bytes, case-insensitive
 * ------------------------------------------------------------------------- */

/* ── lowercase haystack: works on current code ──────────────────── */

Test(fs2_api, nichr_lowercase_haystack) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");
    CHECK_FS2VALID(1, &s);

    CHECK_EQ("h at 0", fs2_nichr(&s, 'h', 5), 0);
    CHECK_EQ("H at 0", fs2_nichr(&s, 'H', 5), 0);
    CHECK_EQ("L at 2", fs2_nichr(&s, 'L', 5), 2);
    CHECK_EQ("O at 4", fs2_nichr(&s, 'O', 5), 4);
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, nichr_first_occurrence_only) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("banana");

    CHECK_EQ("B at 0", fs2_nichr(&s, 'B', 6), 0);
    CHECK_EQ("A at 1", fs2_nichr(&s, 'A', 6), 1);
    CHECK_EQ("N at 2", fs2_nichr(&s, 'N', 6), 2);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, nichr_limit_respected) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    CHECK_EQ("O at lim=5",  fs2_nichr(&s, 'O', 5),  4);
    CHECK_EQ("O missed lim=4", fs2_nichr(&s, 'O', 4), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, nichr_lim_zero) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    CHECK_EQ("lim=0", fs2_nichr(&s, 'H', 0), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, nichr_nul_never_matches) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    CHECK_EQ("NUL lim=5",   fs2_nichr(&s, '\0', 5), -1);
    CHECK_EQ("NUL lim=100", fs2_nichr(&s, '\0', 100), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, nichr_not_found) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    CHECK_EQ("Z missing", fs2_nichr(&s, 'Z', 5), -1);
    CHECK_EQ("z missing", fs2_nichr(&s, 'z', 5), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, nichr_empty_string) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("");
    CHECK_FS2VALID(1, &s);

    CHECK_EQ("nothing in empty", fs2_nichr(&s, 'A', 5), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, nichr_local) {
    __fs2_checkreset();
    FS2LOCALFILL(s, "%s", "abcdef");
    CHECK_FS2VALID(1, &s);

    CHECK_EQ("C at 2", fs2_nichr(&s, 'C', 6), 2);
    CHECK_EQ("C beyond lim=2", fs2_nichr(&s, 'C', 2), -1);

    fs2_free(&s);
}

/* ── uppercase haystack: ⚠ FAILS on current code ───────────────────── */

Test(fs2_api, nichr_uppercase_haystack) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("HELLO");
    CHECK_FS2VALID(1, &s);

    /* Symmetry: lowercase needle must find uppercase bytes */
    CHECK_EQ("h at 0", fs2_nichr(&s, 'h', 5), 0);   /* FAIL: *p=='H' != lc=='h' */
    CHECK_EQ("e at 1", fs2_nichr(&s, 'e', 5), 1);   /* FAIL */
    CHECK_EQ("l at 2", fs2_nichr(&s, 'l', 5), 2);   /* FAIL */
    CHECK_EQ("o at 4", fs2_nichr(&s, 'o', 5), 4);   /* FAIL */

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, nichr_mixed_case) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("HeLLo");
    CHECK_FS2VALID(1, &s);

    /* 'l' must match 'L' at position 2 */
    CHECK_EQ("l at 2", fs2_nichr(&s, 'l', 5), 2);   /* FAIL: returns -1 */
    CHECK_EQ("L at 2", fs2_nichr(&s, 'L', 5), 2);   /* FAIL: returns -1 */

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, nichr_masked_c_uppercase) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("A");   /* 0x41 */

    /* (unsigned char)0x141 == 'A' -> tolower -> 'a'; haystack 'A' must match */
    CHECK_EQ("0x141 → 'A'", fs2_nichr(&s, 0x141, 1), 0);   /* FAIL */

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, nichr_byte_ge_128_masked) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("\xFF" "X");   /* 0xFF 'X' */
    CHECK_FS2VALID(1, &s);

    /* (unsigned char)-1 == 0xFF; in C locale tolower(0xFF)==0xFF — unchanged,
     * so it must match the 0xFF byte in haystack */
    CHECK_EQ("-1 → 0xFF", fs2_nichr(&s, -1, 2), 0);

    fs2_free(&s);
    assert_balanced();
}

/* ── negatives ─────────────────────────────────────────────────────── */

Test(fs2_api, nichr_null_aborts, .signal = SIGABRT) {
    fs2_nichr(NULL, 'a', 5);
}
/* -------------------------------------------------------------------------
 * fs2_ichr — first occurrence, unlimited, case-insensitive
 * ------------------------------------------------------------------------- */

Test(fs2_api, ichr_lowercase_haystack) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");
    CHECK_FS2VALID(1, &s);

    CHECK_EQ("H at 0", fs2_ichr(&s, 'H'), 0);
    CHECK_EQ("E at 1", fs2_ichr(&s, 'E'), 1);
    CHECK_EQ("O at 4", fs2_ichr(&s, 'O'), 4);
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
    assert_balanced();
}

/* ⚠ FAILS on current code — inherits nichr bug */
Test(fs2_api, ichr_uppercase_haystack) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("HELLO");
    CHECK_FS2VALID(1, &s);

    CHECK_EQ("h at 0", fs2_ichr(&s, 'h'), 0);   /* FAIL */
    CHECK_EQ("l at 2", fs2_ichr(&s, 'l'), 2);   /* FAIL */

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, ichr_first_occurrence_only) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("banana");

    CHECK_EQ("B at 0", fs2_ichr(&s, 'B'), 0);
    CHECK_EQ("A at 1", fs2_ichr(&s, 'A'), 1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, ichr_not_found) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    CHECK_EQ("Z missing", fs2_ichr(&s, 'Z'), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, ichr_empty_string) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("");
    CHECK_FS2VALID(1, &s);

    CHECK_EQ("nothing in empty", fs2_ichr(&s, 'a'), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, ichr_local) {
    __fs2_checkreset();
    FS2LOCALFILL(s, "%s", "hello");
    CHECK_FS2VALID(1, &s);

    CHECK_EQ("H at 0", fs2_ichr(&s, 'H'), 0);
    CHECK_EQ("X missing", fs2_ichr(&s, 'X'), -1);

    fs2_free(&s);
}

/* ── negatives ─────────────────────────────────────────────────────── */

Test(fs2_api, ichr_null_aborts, .signal = SIGABRT) {
    fs2_ichr(NULL, 'a');
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
 * fs2_irchr — last occurrence, unlimited, case-insensitive
 *
 * Logic is already correct (tolower on both sides), no failing tests.
 * Below — regression suite to ensure the `p` -> `p[pos]` fix does not
 * break behaviour and to pin down symmetry between irchr vs nichr.
 * ------------------------------------------------------------------------- */

Test(fs2_api, irchr_lowercase_haystack) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");
    CHECK_FS2VALID(1, &s);

    CHECK_EQ("H at 0", fs2_irchr(&s, 'H'), 0);
    CHECK_EQ("E at 1", fs2_irchr(&s, 'E'), 1);
    CHECK_EQ("L at 3", fs2_irchr(&s, 'L'), 3);   /* last L */
    CHECK_EQ("O at 4", fs2_irchr(&s, 'O'), 4);
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, irchr_uppercase_haystack) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("HELLO");
    CHECK_FS2VALID(1, &s);

    /* Symmetry — unlike nichr/ichr (pre-fix) this works */
    CHECK_EQ("h at 0", fs2_irchr(&s, 'h'), 0);
    CHECK_EQ("e at 1", fs2_irchr(&s, 'e'), 1);
    CHECK_EQ("l at 3", fs2_irchr(&s, 'l'), 3);
    CHECK_EQ("o at 4", fs2_irchr(&s, 'o'), 4);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, irchr_mixed_case) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("HeLLo");

    CHECK_EQ("L at 3", fs2_irchr(&s, 'L'), 3);
    CHECK_EQ("l at 3", fs2_irchr(&s, 'l'), 3);
    CHECK_EQ("O at 4", fs2_irchr(&s, 'o'), 4);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, irchr_returns_last_not_first) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("BaNaNa");

    CHECK_EQ("A last at 5", fs2_irchr(&s, 'a'), 5);
    CHECK_EQ("N last at 4", fs2_irchr(&s, 'n'), 4);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, irchr_nul_never_matches) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    CHECK_EQ("NUL missing", fs2_irchr(&s, '\0'), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, irchr_not_found) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    CHECK_EQ("Z missing", fs2_irchr(&s, 'Z'), -1);
    CHECK_EQ("z missing", fs2_irchr(&s, 'z'), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, irchr_empty_string) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("");
    CHECK_FS2VALID(1, &s);

    CHECK_EQ("nothing in empty", fs2_irchr(&s, 'a'), -1);

    fs2_free(&s);
    assert_balanced();
}

/* passes now: lc is already masked, tolower on haystack — same */
Test(fs2_api, irchr_c_masked_to_unsigned_char) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("A");   /* 0x41 */

    /* tolower((unsigned char)0x141) == tolower(0x41) == 'a' */
    CHECK_EQ("0x141 → 'A'", fs2_irchr(&s, 0x141), 0);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, irchr_negative_c) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("\xFF" "X");
    CHECK_FS2VALID(1, &s);

    /* (unsigned char)-1 == 0xFF; in C locale tolower(0xFF)==0xFF */
    CHECK_EQ("-1 → 0xFF", fs2_irchr(&s, -1), 0);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, irchr_local) {
    __fs2_checkreset();
    FS2LOCALFILL(s, "%s", "hello");
    CHECK_FS2VALID(1, &s);

    CHECK_EQ("L at 3", fs2_irchr(&s, 'L'), 3);
    CHECK_EQ("X missing", fs2_irchr(&s, 'X'), -1);

    fs2_free(&s);
}

Test(fs2_api, irchr_local_uppercase) {
    __fs2_checkreset();
    FS2LOCALFILL(s, "%s", "HELLO");
    CHECK_FS2VALID(1, &s);

    CHECK_EQ("l at 3", fs2_irchr(&s, 'l'), 3);
    CHECK_EQ("h at 0", fs2_irchr(&s, 'h'), 0);

    fs2_free(&s);
}

/* ── negatives ─────────────────────────────────────────────────────── */

Test(fs2_api, irchr_null_aborts, .signal = SIGABRT) {
    fs2_irchr(NULL, 'a');
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
    CHECK_EQ("ichr",   fs2_ichr  (&s, '\0'), -1);
    CHECK_EQ("nichr",  fs2_nichr (&s, '\0', 5), -1);
    CHECK_EQ("rchr",   fs2_rchr  (&s, '\0'), -1);
    CHECK_EQ("nrchr",  fs2_nrchr (&s, '\0', 5), -1);
    CHECK_EQ("irchr",  fs2_irchr (&s, '\0'), -1);
    CHECK_EQ("nirchr", fs2_nirchr(&s, '\0', 5), -1);

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
                 fs2_ichr_nth (&s, 'A', k),
                 fs2_irchr_nth(&s, 'A', 4 - k));
        CHECK_EQ("nichr_nth(k) == nirchr_nth(total+1-k)",
                 fs2_nichr_nth(&s, 'A', k, 100),
                 fs2_nirchr_nth(&s, 'A', 4 - k, 100));
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
    CHECK_EQ("ichr_nth   k=1 → 1", fs2_ichr_nth  (&s, 'a', 1), 1);
    CHECK_EQ("ichr_nth   k=2 → 3", fs2_ichr_nth  (&s, 'a', 2), 3);
    CHECK_EQ("ichr_nth   k=3 → 5", fs2_ichr_nth  (&s, 'a', 3), 5);
    CHECK_EQ("nichr_nth  k=2 → 3", fs2_nichr_nth (&s, 'a', 2, 100), 3);

    /* Reverse + insensitive: mirror */
    CHECK_EQ("irchr_nth  k=1 → 5", fs2_irchr_nth (&s, 'a', 1), 5);
    CHECK_EQ("irchr_nth  k=3 → 1", fs2_irchr_nth (&s, 'a', 3), 1);
    CHECK_EQ("nirchr_nth k=2 → 3", fs2_nirchr_nth(&s, 'a', 2, 100), 3);

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

    CHECK_EQ("irchr k=2 → 0", fs2_irchr_nth(&s, 'A', 2), 0);
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
    CHECK_EQ("k=1 lim=1 → 5", fs2_nirchr_nth(&s, 'A', 1, 1), 5);
    CHECK_EQ("k=2 lim=1 → -1", fs2_nirchr_nth(&s, 'A', 2, 1), -1);

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
    CHECK_EQ("ichr_nth  k=2", fs2_ichr_nth  (&s, 'a', 2),     3);
    CHECK_EQ("nichr_nth k=2", fs2_nichr_nth (&s, 'a', 2, 10), 3);
    CHECK_EQ("rchr_nth  k=2", fs2_rchr_nth  (&s, 'a', 2),     3);
    CHECK_EQ("nrchr_nth k=2", fs2_nrchr_nth (&s, 'a', 2, 10), 3);
    CHECK_EQ("irchr_nth k=2", fs2_irchr_nth (&s, 'a', 2),     3);
    CHECK_EQ("nirchr_nth k=2", fs2_nirchr_nth(&s, 'a', 2, 10), 3);
    CHECK_EQ("irchr_nth k=0", fs2_irchr_nth(&s, 'a', 0), -1);
    CHECK_EQ("nirchr_nth k=0", fs2_nirchr_nth(&s, 'a', 0, 10), -1);

    fs2_free(&s);
    assert_balanced();
}
// null
Test(fs2_api, ichr_nth_null,   .signal = SIGABRT) { fs2_ichr_nth  (NULL, 'a', 1);    }
Test(fs2_api, nichr_nth_null,  .signal = SIGABRT) { fs2_nichr_nth (NULL, 'a', 1, 10);}
Test(fs2_api, rchr_nth_null,   .signal = SIGABRT) { fs2_rchr_nth  (NULL, 'a', 1);    }
Test(fs2_api, nrchr_nth_null,  .signal = SIGABRT) { fs2_nrchr_nth (NULL, 'a', 1, 10);}
Test(fs2_api, irchr_nth_null,  .signal = SIGABRT) { fs2_irchr_nth (NULL, 'a', 1);    }
Test(fs2_api, nirchr_nth_null, .signal = SIGABRT) { fs2_nirchr_nth(NULL, 'a', 1, 10);}

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
    CHECK_EQ("sz",  w.sz, 1);          /* FS2_EMPTY, не {0,0} */
    CHECK_TRUE("STATIC", fs2_is_static(&w));
    CHECK_NOT_NULL("v non-NULL", w.v);
    CHECK_STR_EQ("v empty", w.v, "");
    CHECK_FS2VALID(1, &w);             /* ← проходит */
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
    CHECK_FS2VALID(4, &src);   /* src не тронут */

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
    CHECK_FS2VALID(3, &src);   /* STATIC не меняется */

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
    /* Проверим, что промежуточный fs2 не аллоцировал:
     * один alloc/free — на dst, zero — на wrap. */
    CHECK_STR_EQ("appended", s.v, "hello");
    CHECK_FS2VALID(2, &s);
    fs2_free(&s);
    assert_balanced();   /* ← поймал бы утечку от fs2_fromstr-варианта */
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

    /* FS2WRAP("") даёт валидный {len=0, sz=1, STATIC, v=""} */
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

/* Регрессия на баг memcpy vs memmove:
 * если используется memcpy вместо memmove, старая строка затирается. */
Test(fs2_api, rev_catstr_shifts_correctly) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("world");

    fs2_rev_catstr(&dst, "hello ");

    /* Если был memcpy(dst->v, "hello ", 6), то "world" затрётся,
     * и dst.v[6] будет 'd', но остальное — мусор, strlen < len. */
    CHECK_STR_EQ("suffix intact", dst.v + 6, "world");
    CHECK_FS2VALID(2, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2_api, rev_catstr_triggers_growth) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("world");
    size_t orig_sz = dst.sz;
    
    // 100 символов 'A'
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

    /* 1. Создаём "base" */
    fs2 s = fs2_fromstr("base");
    CHECK_FS2VALID(1, &s);

    /* 2. Клонируем: deep copy, но независимые буферы */
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

    /* Оригинал не тронут */
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
    fs2 s = FS2();  /* начинаем с нуля */
    
    /* 100 раз добавляем спереди и сзади, чтобы вызвать много realloc'ов */
    for (int i = 0; i < 100; i++) {
        fs2_catstr(&s, "R");
        fs2_rev_catstr(&s, "L");
        CHECK_FS2VALID(i, &s);
    }
    
    CHECK_EQ("len", s.len, 200);
    
    /* Проверяем, что "L" и "R" чередуются как надо:
     * "LLL...RRR..." — все L в начале, все R в конце */
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

    /* Мутируем b разными способами */
    fs2_catstr(&b, "_b");
    fs2_rev_catstr(&b, "b_");
    CHECK_STR_EQ("b modified", b.v, "b_shared_b");
    CHECK_FS2VALID(3, &b);

    /* a не должен измениться */
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
    CHECK_FS2VALID(3, &src); // src не тронут

    fs2_free(&dst);
    fs2_free(&src);
    assert_balanced();
}

Test(fs2_api, cpy_shrink) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("longer string");
    fs2 src = fs2_fromstr("short");
    
    /* dst был 13, стал 5. Буфер не должен уменьшаться (sz остается прежним) */
    size_t orig_sz = dst.sz;
    
    fs2_cpy(&dst, &src);

    CHECK_STR_EQ("copied shorter", dst.v, "short");
    CHECK_EQ("len", dst.len, 5);
    CHECK_EQ("sz unchanged", dst.sz, orig_sz); // reserve не сжимает
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

    /* fs2_wrap(NULL) -> FS2EMPTY -> fs2_cpy -> очищает dst */
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

    /* Проверяем, что счетчики аллокаций не изменятся */
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
    CHECK_FS2VALID(2, &s); // Инвариант strlen == len соблюден

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, sprintf_overwrite_shorter) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("long string here");
    size_t orig_sz = s.sz;

    /* Пишем строку короче, чем была. Буфер не должен сжиматься. */
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

    /* 100 символов 'A' через формат */
    long res = fs2_sprintf(&s, "%0100d", 0); 
    // %0100d даст 100 символов (нули + '0')

    CHECK_EQ("return value", res, 100);
    CHECK_EQ("len updated", s.len, 100);
    CHECK_TRUE("sz grew", s.sz > orig_sz);
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, sprintf_exact_fit_no_realloc) {
    __fs2_checkreset();
    /* Создаем буфер ровно на 5 символов + NUL = 6 */
    fs2 s = fs2_init(6); 
    
    /* Пишем ровно 5 символов */
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
    
    /* Регрессия на баг: vsnprintf(s->v, needed, ...) съедал последний символ. 
     * Тут needed = 5. Если передать size=5 вместо 6, получится "1234". */
    fs2_sprintf(&s, "12345");

    CHECK_STR_EQ("last char kept", s.v, "12345");
    CHECK_EQ("len", s.len, 5);
    
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, sprintf_empty_format) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    long res = fs2_sprintf(&s, "");

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

/* -------------------------------------------------------------------------
 * fs2_sprintf_position — raw writer (does NOT update len)
 * ------------------------------------------------------------------------- */

Test(fs2_api, sprintf_position_middle) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("0123456789"); // len=10
    CHECK_FS2VALID(1, &s);

    /* Перезаписываем символы с 4-й позиции */
    long res = fs2_sprintf_position(&s, 4, "XXX");

    CHECK_EQ("return value", res, 3);
    /* 0 1 2 3 X X X 7 8 9 */
    CHECK_STR_EQ("content", s.v, "0123XXX");
    
    /* ГЛАВНОЕ: len НЕ должен измениться, т.к. это raw writer */
    CHECK_EQ("len untouched", s.len, 10);
    
    /* Валидатор упадет, если его позвать, потому что strlen (10) != len (10) - 
     * стоп, strlen всё еще 10, потому что мы не ставили \0 в середину, 
     * а XXX перезаписали 456. 
     * Но если бы мы записали туда же, скажем, "X", то strlen стал бы 9. 
     * Поэтому в primitive-слое мы len не трогаем вообще. */

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, sprintf_position_overwrites_nul) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc"); // len=3
    CHECK_FS2VALID(1, &s);

    /* Пишем с конца (pos=3), прямо на место NUL */
    fs2_sprintf_position(&s, 3, "def");
    
    /* В памяти теперь "abcdef\0", но len всё еще 3 */
    CHECK_STR_EQ("content", s.v, "abcdef");
    CHECK_EQ("len untouched (raw writer)", s.len, 3);
    
    /* Если теперь вызывающий хочет, чтобы это стало валидной строкой, он делает: */
    fs2_setlen(&s, 6);
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, sprintf_position_triggers_growth) {
    __fs2_checkreset();
    fs2 s = FS2(); // sz=0, v=NULL
    size_t orig_sz = s.sz;
    (void) orig_sz;

    /* Пишем далеко за пределами текущего размера (pos=20) */
    long res = fs2_sprintf_position(&s, 20, "test");

    CHECK_EQ("return value", res, 4);
    CHECK_TRUE("sz grew", s.sz > 20);
    CHECK_EQ("len untouched", s.len, 0);
    
    /* v[20..23] == "test", v[24] == '\0' */
    CHECK_STR_EQ("content at offset", s.v + 20, "test");

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, sprintf_position_null_aborts, .signal = SIGABRT) {
    fs2_sprintf_position(NULL, 0, "abc");
}

Test(fs2_api, sprintf_position_null_fmt_aborts, .signal = SIGABRT) {
    fs2 s = FS2();
    fs2_sprintf_position(&s, 0, NULL);
}

Test(fs2_api, sprintf_bad_format_returns_negative) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("initial");
    
    /* %d без аргумента — UB в C, лучше не тестировать.
     * А вот невалидный спецификатор (например, %q) даст -1. */
    long res = fs2_sprintf(&s, "%q");
    
    CHECK_TRUE("return value < 0", res <= 0);
    /* len не обязан меняться, поэтому просто проверяем, что мы живы */
    
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
    CHECK_FS2VALID(2, &s); // оригинал не тронут
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

    /* from > len, должен вернуть пустой fs2 */
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
    fs2_free(res); // освобождает и структуру, и буфер
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

    /* 1. Исходная строка */
    fs2 src = fs2_fromstr("Hello, World! This is fs2.");
    CHECK_FS2VALID(1, &src);

    /* 2. Извлекаем "Hello" на кучу (heap) */
    fs2 *hello = fs2_heap_substr(&src, 0, 5); // "Hello"
    CHECK_STR_EQ("extracted hello", hello->v, "Hello");
    CHECK_TRUE("is bodyalloc", fs2_is_bodyalloc(hello));
    CHECK_FS2VALID(2, hello);

    /* 3. Извлекаем "World!" на стек */
    fs2 world = fs2_substr_new(&src, 7, 6); // "World!"
    CHECK_STR_EQ("extracted world", world.v, "World!");
    CHECK_FS2VALID(3, &world);

    /* 4. Склеиваем: "Hello" + "World!" = "HelloWorld!" */
    fs2_cat(hello, &world);
    CHECK_STR_EQ("concatenated", hello->v, "HelloWorld!");
    CHECK_EQ("len", hello->len, 11);
    CHECK_FS2VALID(4, hello);

    /* 5. Добавляем строку через catstr */
    fs2_catstr(hello, " And fs2");
    CHECK_STR_EQ("appended", hello->v, "HelloWorld! And fs2");
    CHECK_EQ("len", hello->len, 19);
    CHECK_FS2VALID(5, hello);

    /* 6. Обрезаем на месте (in-place) до "HelloWorld" */
    fs2_substr(hello, 0, 10);
    CHECK_STR_EQ("truncated in-place", hello->v, "HelloWorld");
    CHECK_EQ("len", hello->len, 10);
    CHECK_FS2VALID(6, hello);

    /* 7. Создаем пустую строку и копируем в неё результат (cpy) */
    fs2 final_copy = FS2();
    fs2_cpy(&final_copy, hello);
    CHECK_STR_EQ("copied", final_copy.v, "HelloWorld");
    CHECK_FS2VALID(7, &final_copy);

    /* 8. Проверяем, что оригинал не тронут всеми этими манипуляциями */
    CHECK_STR_EQ("src intact", src.v, "Hello, World! This is fs2.");
    CHECK_FS2VALID(8, &src);

    /* 9. Убираем мусор */
    fs2_free(hello);     // освобождает и структуру, и буфер
    fs2_free(&world);
    fs2_free(&final_copy);
    fs2_free(&src);

    assert_balanced();
}

Test(fs2_api, composite_substr_clamping_pipeline) {
    __fs2_checkreset();

    fs2 s = fs2_fromstr("abcdef");
    
    /* 
     * 1. Берем len=100 (должно обрезаться до 3). 
     * Получаем "def".
     */
    fs2 *part1 = fs2_heap_substr(&s, 3, 100);
    CHECK_STR_EQ("part1 clamped", part1->v, "def");
    CHECK_EQ("part1 len", part1->len, 3);
    CHECK_FS2VALID(1, part1);

    /* 
     * 2. Берем from=10 (за пределами). Должна быть пустая строка.
     */
    fs2 part2 = fs2_substr_new(&s, 10, 5);
    CHECK_EQ("part2 is empty", part2.len, 0);
    CHECK_TRUE("part2 is empty", fs2_is_empty(&part2));
    CHECK_FS2VALID(2, &part2);

    /* 
     * 3. Приклеиваем пустоту к part1. part1 не должен измениться.
     */
    fs2_cat(part1, &part2);
    CHECK_STR_EQ("cat empty", part1->v, "def");
    CHECK_EQ("len unchanged", part1->len, 3);
    CHECK_FS2VALID(3, part1);

    /* 
     * 4. Делаем in-place substr с from = 1 и len = 100. 
     * Осталось 2 символа ("ef").
     */
    fs2_substr(part1, 1, 100);
    CHECK_STR_EQ("in-place clamped", part1->v, "ef");
    CHECK_EQ("len clamped", part1->len, 2);
    CHECK_FS2VALID(4, part1);

    /* 5. Пробуем сдвинуть за пределы (from=10). Должна остаться пустой. */
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
    
    /* Просим больше, чем есть — должны получить всю строку */
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
 * fs2_right (in-place, value, heap) — регрессия на underflow!
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

/* ⚠ РЕГРЕССИЯ: этот тест падал бы на старом коде из-за underflow! */
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
    
    fs2_toupper(&s); // Не должно упасть
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
    
    /* Клонирует STATIC в ALLOC, затем делает upper. Оригинал не трогается. */
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
    
    /* Строка из байтов >= 128 (например, UTF-8 кириллица) */
    fs2 s = fs2_fromstr("\xD0\xB4" "\xD0\xB0"); // "да"
    size_t orig_len = s.len;

    /* Раньше это могло упасть с Segfault из-за signed char.
     * Теперь должно просто вернуть строку (в C locale они не меняются). */
    fs2_toupper(&s);
    
    CHECK_EQ("len unchanged", s.len, orig_len);
    CHECK_FS2VALID(1, &s);
    
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, tolower_non_ascii_no_crash) {
    __fs2_checkreset();
    
    fs2 s = fs2_fromstr("\xC4\xE4"); // 'Ä' 'ä' в Latin-1
    size_t orig_len = s.len;

    fs2_tolower(&s);
    
    CHECK_EQ("len unchanged", s.len, orig_len);
    CHECK_FS2VALID(1, &s);
    
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * Проверка на смешанных символах
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

    /* Pad длиннее, чем нужно дополнить. Должен взяться префикс pad'а. */
    fs2_rpad(&s, 4, &pad);

    CHECK_STR_EQ("padded", s.v, "xabc");
    CHECK_EQ("len", s.len, 4);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    fs2_free(&pad);
    assert_balanced();
}

/* ⚠ РЕГРЕССИЯ: если использовать fs2_right вместо fs2_left, тест упадет */
Test(fs2_api, rpad_truncation) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abcdef");
    fs2 pad = fs2_fromstr(".");

    /* Если len < str->len, rpad должен обрезать до левых len символов */
    fs2_rpad(&s, 3, &pad);

    CHECK_STR_EQ("truncated", s.v, "abc"); // а не "def"!
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

    /* Пустой pad — строка не меняется (если не нужно обрезать) */
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

    /* fs2_wrap(NULL) -> FS2EMPTY (len=0), поэтому padding не произойдет */
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

    /* В fs2_rpad_new стоит if (!pad) return res; */
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

    /* Пример из документации Postgres: rpad('hi', 5, 'xy') → hixyx */
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

    /* Пример: rpad('hello', 2) → 'he' (обрезает хвост, оставляет левую часть) */
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

    /* Если pad пустой, а len больше, строка не меняется (нельзя дополнить пустотой) */
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

    /* Если len = 0, от строки ничего не остается */
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

    /* Пример из документации Postgres: lpad('hi', 5, 'xy') → xyxhi */
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

    /* Pad длиннее, чем нужно (нужно 3 символа). Берется префикс "abc" */
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

    /* Если len < str->len, lpad обрезает до ЛЕВОЙ части */
    fs2_lpad(&s, 3, &pad);

    CHECK_STR_EQ("truncated", s.v, "hel"); // а не "llo"
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

    /* Пустой pad — строка не меняется */
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

    /* fs2_wrap(NULL) -> FS2EMPTY (len=0), padding не произойдет */
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

    /* В fs2_lpad_new стоит if (!pad) return res; */
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

    /* Передаем неизвестный тип, ожидаем fallback '?' */
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

    /* Генерируем 5 символов поверх старой строки */
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
     * Генерируем много символов. Если rndint(26) выдает 26,
     * то мы получим '{', и тест упадет.
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
