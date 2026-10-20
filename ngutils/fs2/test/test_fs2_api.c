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

Test(fs2_api, partialsort_to_clamped_to_len) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("XdcbaY");

    /* to=100 → clamp to len=6; from=1 < 6 → сортирует "dcbaY" */
    fs2_partialsort(&s, 1, 100, true);
    CHECK_STR_EQ("clamped to end", s.v, "XYabcd");
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, partialsort_from_gt_to_noop) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");

    /* from > to → no-op (было бы SIZE_MAX в qsort без if-guard) */
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

    /* to=100 → clamp to 0; from=0, from < to → false; v+from не вычисляется */
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
    FS2LOCALFILL(s, "%s", "abcdef");
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

/* -------------------------------------------------------------------------
 * fs2_chr N-th occurence series
 * ------------------------------------------------------------------------- */

Test(fs2_api, chrn_return_type_is_ptrdiff_t) {
    /* Compile-time: несоответствие типа возврата — constraint violation
     * (warning/error) даже если sizeof совпадает. */
    ptrdiff_t (*f_chr_nth )(const fs2 *, int, size_t)        = fs2_chr_nth;
    ptrdiff_t (*f_nchr_nth)(const fs2 *, int, size_t, size_t) = fs2_nchr_nth;
    (void)f_chr_nth;
    (void)f_nchr_nth;

    /* Runtime: -1 — знаковое, не SIZE_MAX */
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
    fs2 s = fs2_fromstr("banana");   /* три 'a': 1, 3, 5 */

    /* Последнее (3-е) вхождение при прямом поиске == первое при обратном */
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

    /* Одиночные вхождения: k=2 уже -1 */
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

    /* k 1-based: ноль невалиден, не "первое вхождение" */
    CHECK_EQ("k=0 → -1", fs2_chr_nth(&s, 'l', 0), -1);
    CHECK_EQ("nchr k=0 → -1", fs2_nchr_nth(&s, 'l', 0, 5), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, chrn_k_exceeds_matches) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");   /* один 'b' */

    CHECK_EQ("k=2 → -1", fs2_chr_nth(&s, 'b', 2), -1);
    CHECK_EQ("k=100 → -1", fs2_chr_nth(&s, 'b', 100), -1);

    /* k = SIZE_MAX: завершается быстро, не зависает */
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

    /* пустое окно — ни одного вхождения */
    CHECK_EQ("lim=0", fs2_nchr_nth(&s, 'h', 1, 0), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, nchrn_lim_lt_len) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("banana");   /* 'a' на 1, 3, 5 */

    /* lim=4 → окно [0,4) = "bana"; третьего 'a' нет */
    CHECK_EQ("k=1 lim=4", fs2_nchr_nth(&s, 'a', 1, 4), 1);
    CHECK_EQ("k=2 lim=4", fs2_nchr_nth(&s, 'a', 2, 4), 3);
    CHECK_EQ("k=3 lim=4", fs2_nchr_nth(&s, 'a', 3, 4), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, nchrn_lim_ge_len) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("banana");

    /* lim >= len → ведёт себя как chr_nth */
    CHECK_EQ("lim=len",    fs2_nchr_nth(&s, 'a', 3, 6),
                           fs2_chr_nth(&s, 'a', 3));
    CHECK_EQ("lim>len",    fs2_nchr_nth(&s, 'a', 3, 100),
                           fs2_chr_nth(&s, 'a', 3));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, chrn_match_at_last_position) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abcabc");   /* c на 2 и 5 */
    CHECK_FS2VALID(1, &s);

    CHECK_EQ("k=1 'c' → 2", fs2_chr_nth(&s, 'c', 1), 2);
    CHECK_EQ("k=2 'c' → 5", fs2_chr_nth(&s, 'c', 2), 5);   /* последний байт */

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, nchrn_match_at_window_end) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("XabcX");   /* 'c' на 3 */

    /* lim=4 → окно [0,3] = "Xabc"; 'c' — последний байт окна */
    CHECK_EQ("k=1 lim=4", fs2_nchr_nth(&s, 'c', 1, 4), 3);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, chrn_k1_of_k_on_last) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("aaa");   /* три 'a': 0, 1, 2 */

    CHECK_EQ("k=3 → 2 (last byte)", fs2_chr_nth(&s, 'a', 3), 2);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, chrn_large_offsets) {
    __fs2_checkreset();

    const size_t n = 100 * 1024;          /* 100 KB */
    fs2 s = fs2_init(n + 2);
    memset(s.v, 'a', n);
    s.v[n]     = 'b';                      /* последний значащий */
    s.v[n + 1] = '\0';
    s.len      = n + 1;
    CHECK_FS2VALID(1, &s);

    /* Позиция ~100000 — точно не влезет в int16/int8 */
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
    fs2 s = fs2_fromstr("banana");   /* 'a' на 1, 3, 5 */

    CHECK_EQ("k=1 (last)  → 5", fs2_rchr_nth(&s, 'a', 1), 5);
    CHECK_EQ("k=2          → 3", fs2_rchr_nth(&s, 'a', 2), 3);
    CHECK_EQ("k=3 (first)  → 1", fs2_rchr_nth(&s, 'a', 3), 1);
    CHECK_EQ("k=4          → -1", fs2_rchr_nth(&s, 'a', 4), -1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, nrchrn_forward_reverse_symmetry) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("banana");   /* три 'a' */

    /* i-е прямое + (count-i+1)-е обратное указывают на одну позицию */
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
// forward ↔ reverse симметрия
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
// upper  чувствительность на апперкейсе
Test(fs2_api, chrn_insensitive_uppercase_haystack) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("BANANA");
    CHECK_FS2VALID(1, &s);

    /* Forward + insensitive: находит lowercase needle в uppercase haystack */
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
// consecutive matches — специфичный для reverse
Test(fs2_api, rchr_nth_consecutive) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("aa");   /* 'a' на 0 и 1 — соседние */
    CHECK_FS2VALID(1, &s);

    /* Если кто-то написал `end = hit - 1` вместо `end = hit`,
     * k=2 вернёт -1 вместо 0 — второй 'a' потеряется. */
    CHECK_EQ("k=1 → 1", fs2_rchr_nth (&s, 'a', 1), 1);
    CHECK_EQ("k=2 → 0", fs2_rchr_nth (&s, 'a', 2), 0);
    CHECK_EQ("k=3 → -1", fs2_rchr_nth(&s, 'a', 3), -1);

    CHECK_EQ("irchr k=2 → 0", fs2_irchr_nth(&s, 'A', 2), 0);
    CHECK_EQ("nrchr k=2 → 0", fs2_nrchr_nth(&s, 'a', 2, 100), 0);

    fs2_free(&s);
    assert_balanced();
}
// lim + reverse — арифметика off + hit
Test(fs2_api, nrchr_nth_lim_offset) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("banana");   /* len=6, 'a' на 1, 3, 5 */
    CHECK_FS2VALID(1, &s);

    /* lim=4 → окно [2, 6) = "nana"; 'a' на 3 и 5 (в orig coords) */
    /* Если кто-то забыл `+ off`, получим 1 и 3 вместо 5 и 3. */
    CHECK_EQ("k=1 lim=4 → 5", fs2_nrchr_nth(&s, 'a', 1, 4), 5);
    CHECK_EQ("k=2 lim=4 → 3", fs2_nrchr_nth(&s, 'a', 2, 4), 3);
    CHECK_EQ("k=3 lim=4 → -1", fs2_nrchr_nth(&s, 'a', 3, 4), -1);

    /* lim=1 → окно [5, 6) = "a"; только позиция 5 */
    CHECK_EQ("k=1 lim=1 → 5", fs2_nirchr_nth(&s, 'A', 1, 1), 5);
    CHECK_EQ("k=2 lim=1 → -1", fs2_nirchr_nth(&s, 'A', 2, 1), -1);

    fs2_free(&s);
    assert_balanced();
}
// all 8
Test(fs2_api, chrn_all_variants_banana_k2) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("banana");   /* 'a' на 1, 3, 5 — k=2 → средний на 3 */
    CHECK_FS2VALID(1, &s);

    /* k=2 — единственное вхождение, где forward и reverse совпадают */
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
