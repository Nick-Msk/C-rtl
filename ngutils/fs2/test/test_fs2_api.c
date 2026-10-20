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
    *p = 'X'; // Меняем существующий символ

    CHECK_PTR_EQ("same pointer", p, s.v + 1);
    CHECK_EQ   ("sz unchanged", s.sz, sz_before);
    CHECK_EQ   ("len unchanged", s.len, 3);
    CHECK_STR_EQ("string mutated", s.v, "aXc");
    CHECK_FS2VALID(2, &s); // Валидно, потому что len не менялся

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, elem_expands_buffer_no_len_change) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc"); /* len=3, sz=16 */
    CHECK_FS2VALID(1, &s);

    // Запрос на позицию за пределами текущего sz
    char *p = fs2_elem(&s, 20); 

    CHECK_PTR_EQ("pointer to pos 20", p, s.v + 20);
    CHECK_TRUE("sz grown", s.sz > 20);
    CHECK_EQ  ("len NOT changed", s.len, 3); // fs2_elem не трогает len
    
    // Сама по себе строка все еще валидна, потому что len=3, а NUL на pos 3 на месте
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

    // Строим строку "hello" посимвольно
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

    // Перезаписываем существующий символ (pos < len)
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

    // Чтение всех символов
    CHECK_EQ("char at 0", *fs2_get(&s, 0), 'h');
    CHECK_EQ("char at 4", *fs2_get(&s, 4), 'o');
    
    // Чтение NUL-терминатора (pos == len)
    CHECK_EQ("char at len", *fs2_get(&s, 5), '\0');

    // Проверка арифметики указателей
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

    /* Пишем строку "streaming" посимвольно.
     * fs2_elem расширяет буфер, но не трогает len. */
    const char *data = "streaming";
    size_t len = strlen(data);
    
    for (size_t i = 0; i < len; i++) {
        char *p = fs2_elem(&s, i);
        *p = data[i];
    }

    /* len все еще 0, строка формально пустая, хотя байты записаны */
    CHECK_EQ("len unchanged during stream", s.len, 0);
    
    /* Один раз обновляем len и ставим NUL */
    fs2_setlen(&s, len);

    /* Теперь всё валидно и читается */
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
    fs2 s = FS2_LITERAL("static");
    fs2_reverse(&s); /* userraise → SIGINT */
}


Test(fs2_api, reverse_double_invariant) {
    __fs2_checkreset();
    /* Берем строку с четной и нечетной длиной для полноты */
    fs2 s = fs2_fromstr("racecar");
    CHECK_FS2VALID(1, &s);

    /* Сохраняем оригинал для сравнения */
    fs2 original = fs2_clone(&s);
    CHECK_FS2VALID(2, &original);

    /* Разворачиваем 1-й раз */
    fs2_reverse(&s);
    CHECK_STR_EQ("first reverse", s.v, "racecar"); /* палиндром не меняется */
    
    /* Берем не-палиндром для честной проверки */
    fs2_free(&s);
    s = fs2_fromstr("algorithms");
    fs2_free(&original);
    original = fs2_clone(&s);

    /* Разворачиваем 1-й раз */
    fs2_reverse(&s);
    CHECK_STR_EQ("first reverse", s.v, "smhtirogla");
    CHECK_FS2VALID(3, &s);

    /* Разворачиваем 2-й раз (должно вернуть к оригиналу) */
    fs2_reverse(&s);
    
    /* Проверяем, что строка стала байт-в-байт как оригинал */
    CHECK_STR_EQ("double reverse matches original", s.v, original.v);
    CHECK_EQ("len unchanged", s.len, original.len);
    CHECK_FS2VALID(4, &s);

    fs2_free(&s);
    fs2_free(&original);
    assert_balanced();
}

Test(fs2_api, reverse_roundtrip_empty_and_single) {
    __fs2_checkreset();
    
    /* Пустая строка */
    fs2 empty = fs2_fromstr("");
    fs2_reverse(&empty);
    fs2_reverse(&empty);
    CHECK_STR_EQ("empty stays empty", empty.v, "");
    CHECK_EQ("empty len", empty.len, 0);

    /* Один символ */
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

    fs2_sort(&s, true); /* qsort с 0 элементов — безопасно */

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

Test(fs2_api, sort_static_rejected, .signal = SIGINT) {
    fs2 s = FS2_LITERAL("static");
    fs2_sort(&s, true); /* userraise → SIGINT */
}

/* -------------------------------------------------------------------------
 * fs2_partialsort — in-place character sort on a sub-range [from, to)
 * ------------------------------------------------------------------------- */

Test(fs2_api, partialsort_middle) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("XdcbaY");
    CHECK_FS2VALID(1, &s);

    /* Сортируем только "dcba" (индексы 1,2,3,4), оставляя X и Y на местах */
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
    
    /* from == to, qsort с 0 элементами. Это валидный вызов для qsort */
    fs2_partialsort(&s, 1, 1, true);
    CHECK_STR_EQ("unchanged", s.v, "cba");
    
    fs2_free(&s);
    assert_balanced();
}

/* ── negatives ─────────────────────────────────────────────────────── */

Test(fs2_api, partialsort_static_rejected, .signal = SIGINT) {
    fs2 s = FS2_LITERAL("static");
    fs2_partialsort(&s, 0, 3, true);
}

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
    CHECK_EQ("a at 1",  fs2_chr(&s, 'a'), 1);   /* не 3 и не 5 */
    CHECK_EQ("n at 2",  fs2_chr(&s, 'n'), 2);   /* не 4 */

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

    /* memchr over [0, len): NUL на len — вне окна, не матчится */
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
    FS2LOCALFILL(s, "world");
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

/* ⚠ падает на текущем коде: *p == c без маскирования */
Test(fs2_api, nchr_c_masked_to_unsigned_char) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("A");   /* 0x41 */

    /* (unsigned char)0x141 == 'A' — как strchr */
    CHECK_EQ("0x141 → 'A'", fs2_nchr(&s, 0x141, 1), 0);

    fs2_free(&s);
    assert_balanced();
}

/* ⚠ падает на текущем коде: *p == -1 никогда не true */
Test(fs2_api, nchr_negative_c) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("\xFF" "X");
    CHECK_FS2VALID(1, &s);

    /* (unsigned char)-1 == 0xFF */
    CHECK_EQ("-1 → 0xFF", fs2_nchr(&s, -1, 2), 0);

    fs2_free(&s);
    assert_balanced();
}

/* этот тест проходит и на текущем коде — оставь как защиту от регрессии */
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
    FS2LOCALFILL(s, "abcdef");
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

/* ── lowercase haystack: работает и на текущем коде ──────────────── */

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
    FS2LOCALFILL(s, "abcdef");
    CHECK_FS2VALID(1, &s);

    CHECK_EQ("C at 2", fs2_nichr(&s, 'C', 6), 2);
    CHECK_EQ("C beyond lim=2", fs2_nichr(&s, 'C', 2), -1);

    fs2_free(&s);
}

/* ── uppercase haystack: ⚠ ПАДАЕТ на текущем коде ──────────────────── */

Test(fs2_api, nichr_uppercase_haystack) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("HELLO");
    CHECK_FS2VALID(1, &s);

    /* Симметрия: needle в нижнем регистре должен находить uppercase-байты */
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

    /* 'l' должен попадать и в 'L' на позиции 2 */
    CHECK_EQ("l at 2", fs2_nichr(&s, 'l', 5), 2);   /* FAIL: возвращает -1 */
    CHECK_EQ("L at 2", fs2_nichr(&s, 'L', 5), 2);   /* FAIL: возвращает -1 */

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, nichr_masked_c_uppercase) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("A");   /* 0x41 */

    /* (unsigned char)0x141 == 'A' → tolower → 'a'; haystack 'A' должен совпасть */
    CHECK_EQ("0x141 → 'A'", fs2_nichr(&s, 0x141, 1), 0);   /* FAIL */

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, nichr_byte_ge_128_masked) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("\xFF" "X");   /* 0xFF 'X' */
    CHECK_FS2VALID(1, &s);

    /* (unsigned char)-1 == 0xFF; в C locale tolower(0xFF)==0xFF — без изменений,
     * поэтому должно совпасть с 0xFF-байтом haystack */
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

/* ⚠ ПАДАЕТ на текущем коде — наследует баг nichr */
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
    FS2LOCALFILL(s, "hello");
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

    /* Контраст с fs2_chr: тот же needle, разные концы */
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

    /* NUL — вне [0, len); ни прямой, ни обратный поиск его не видит */
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

/* ⚠ ПАДАЕТ на текущем коде: сравниваем с c, а не с uc */
Test(fs2_api, rchr_c_masked_to_unsigned_char) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("A");   /* 0x41 */

    /* (unsigned char)0x141 == 'A' */
    CHECK_EQ("0x141 → 'A'", fs2_rchr(&s, 0x141), 0);   /* FAIL */

    fs2_free(&s);
    assert_balanced();
}

/* ⚠ ПАДАЕТ на текущем коде: p[pos] == 0xFF != -1 */
Test(fs2_api, rchr_negative_c) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("\xFF" "X");
    CHECK_FS2VALID(1, &s);

    CHECK_EQ("-1 → 0xFF", fs2_rchr(&s, -1), 0);   /* FAIL */

    fs2_free(&s);
    assert_balanced();
}

/* проходит и сейчас — оставь как защиту от регрессии */
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
    FS2LOCALFILL(s, "hello");
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
 * Логика уже корректна (tolower на обе стороны), красных тестов нет.
 * Ниже — регрессионный набор, чтобы фикс `p` → `p[pos]` не сломал
 * поведение и чтобы зафиксировать симметрию с irchr vs nichr.
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

    /* Симметрия — в отличие от nichr/ichr (до фикса) работает */
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

/* проходит и сейчас: lc уже маскирован, tolower на haystack — тоже */
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

    /* (unsigned char)-1 == 0xFF; в C locale tolower(0xFF)==0xFF */
    CHECK_EQ("-1 → 0xFF", fs2_irchr(&s, -1), 0);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, irchr_local) {
    __fs2_checkreset();
    FS2LOCALFILL(s, "hello");
    CHECK_FS2VALID(1, &s);

    CHECK_EQ("L at 3", fs2_irchr(&s, 'L'), 3);
    CHECK_EQ("X missing", fs2_irchr(&s, 'X'), -1);

    fs2_free(&s);
}

Test(fs2_api, irchr_local_uppercase) {
    __fs2_checkreset();
    FS2LOCALFILL(s, "HELLO");
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

    /* 'l' встречается дважды: на 2 и 3.
     * Прямой поиск — первое вхождение, обратный — последнее. */
    CHECK_EQ("chr  'l' → 2", fs2_chr (&s, 'l'), 2);
    CHECK_EQ("rchr 'l' → 3", fs2_rchr(&s, 'l'), 3);

    /* Это и есть разница между прямым и обратным поиском —
     * на одном и том же haystack, одном needle. */
    CHECK_TRUE("positions differ",
               fs2_chr(&s, 'l') != fs2_rchr(&s, 'l'));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, chr_eq_rchr_single_occurrence) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    /* 'h', 'e', 'o' — уникальны. chr и rchr должны совпадать. */
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
