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
 * fs_reverse — in-place string reversal
 * ------------------------------------------------------------------------- */

Test(fs2_api, reverse_even_length) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abcd");
    CHECK_FS2VALID(1, &s);

    fs2 *ret = fs_reverse(&s);
    
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

    fs_reverse(&s);
    
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

    fs_reverse(&s);
    
    CHECK_STR_EQ("reversed", s.v, "a");
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, reverse_empty) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("");
    CHECK_FS2VALID(1, &s);

    fs_reverse(&s);
    
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

    fs_reverse(&s);
    
    CHECK_STR_EQ("reversed", s.v, "olleh");
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
}

/* ── negatives ─────────────────────────────────────────────────────── */

Test(fs2_api, reverse_null_aborts, .signal = SIGABRT) {
    fs_reverse(NULL);
}

Test(fs2_api, reverse_static_rejected, .signal = SIGINT) {
    fs2 s = FS2_LITERAL("static");
    fs_reverse(&s); /* userraise → SIGINT */
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
    fs_reverse(&s);
    CHECK_STR_EQ("first reverse", s.v, "racecar"); /* палиндром не меняется */
    
    /* Берем не-палиндром для честной проверки */
    fs2_free(&s);
    s = fs2_fromstr("algorithms");
    fs2_free(&original);
    original = fs2_clone(&s);

    /* Разворачиваем 1-й раз */
    fs_reverse(&s);
    CHECK_STR_EQ("first reverse", s.v, "smhtirogla");
    CHECK_FS2VALID(3, &s);

    /* Разворачиваем 2-й раз (должно вернуть к оригиналу) */
    fs_reverse(&s);
    
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
    fs_reverse(&empty);
    fs_reverse(&empty);
    CHECK_STR_EQ("empty stays empty", empty.v, "");
    CHECK_EQ("empty len", empty.len, 0);

    /* Один символ */
    fs2 single = fs2_fromstr("X");
    fs_reverse(&single);
    fs_reverse(&single);
    CHECK_STR_EQ("single stays single", single.v, "X");
    CHECK_EQ("single len", single.len, 1);

    fs2_free(&empty);
    fs2_free(&single);
    assert_balanced();
}

/* -------------------------------------------------------------------------
 * fs_sort — in-place character sort
 * ------------------------------------------------------------------------- */

Test(fs2_api, sort_ascending) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("banana");
    CHECK_FS2VALID(1, &s);

    fs_sort(&s, true);

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

    fs_sort(&s, false);

    CHECK_STR_EQ("sorted desc", s.v, "nnbaaa");
    CHECK_EQ("len unchanged", s.len, 6);
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, sort_already_sorted) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("abc");
    fs_sort(&s, true);
    CHECK_STR_EQ("sorted", s.v, "abc");
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, sort_with_duplicates) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("cbaabc");
    fs_sort(&s, true);
    CHECK_STR_EQ("sorted", s.v, "aabbcc");
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, sort_empty) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("");
    CHECK_FS2VALID(1, &s);

    fs_sort(&s, true); /* qsort с 0 элементов — безопасно */

    CHECK_STR_EQ("still empty", s.v, "");
    CHECK_FS2VALID(2, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, sort_single_char) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("x");
    fs_sort(&s, true);
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

    fs_sort(&s, true);
    CHECK_STR_EQ("local sorted", s.v, "abcd");
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
}

/* ── negatives ─────────────────────────────────────────────────────── */

Test(fs2_api, sort_null_aborts, .signal = SIGABRT) {
    fs_sort(NULL, true);
}

Test(fs2_api, sort_static_rejected, .signal = SIGINT) {
    fs2 s = FS2_LITERAL("static");
    fs_sort(&s, true); /* userraise → SIGINT */
}
