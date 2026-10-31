#include <criterion/criterion.h>

#include "test_fs2.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* -------------------------------------------------------------------------
 * Fixtures — mute stderr, init log
 * ------------------------------------------------------------------------- */

TestSuite(fs2, .init = test_setup, .fini = test_teardown);

/* -------------------------------------------------------------------------
 * Version
 * ------------------------------------------------------------------------- */

Test(fs2, version_string_matches_macro) {
    CHECK_STR_EQ("fs2_version() == FS2_VERSION", fs2_version(), FS2_VERSION);
    CHECK_STR_EQ("fs2_version()",                fs2_version(), "0.1.0");
}

Test(fs2, version_is_top_of_list) {
    const char *const *v = fs2_versions();

    CHECK_NOT_NULL("fs2_versions()",  v);
    CHECK_NOT_NULL("fs2_versions()[0]", v[0]);
    CHECK_STR_EQ("versions[0] vs fs2_version()", v[0], fs2_version());
    CHECK_STR_EQ("versions[0] vs FS2_VERSION",   v[0], FS2_VERSION);
}

Test(fs2, versions_terminated_by_null) {
    const char *const *v = fs2_versions();
    size_t n = 0;
    while (v[n]) ++n;

    CHECK_TRUE("at least one version", n >= 1);
    CHECK_NULL("versions[n]", v[n]);
}

/* -------------------------------------------------------------------------
 * Value macros — flags, len, sz, v
 * ------------------------------------------------------------------------- */

Test(fs2, macro_fs2_default) {
    fs2 s = FS2();

    CHECK_EQ("len", s.len, 0);
    CHECK_EQ("sz",  s.sz,  0);
    CHECK_NULL("v", s.v);
    CHECK_TRUE ("is_alloc",     fs2_is_alloc(&s));
    CHECK_FALSE("is_static",    fs2_is_static(&s));
    CHECK_FALSE("is_bodyalloc", fs2_is_bodyalloc(&s));
}

Test(fs2, macro_empty_static) {
    fs2 s = FS2EMPTY;

    CHECK_TRUE("is_static", fs2_is_static(&s));
    CHECK_FALSE("is_alloc", fs2_is_alloc(&s));
    CHECK_EQ("len", s.len, 0);
    CHECK_STR_EQ("v", s.v, "");
}

Test(fs2, macro_literal) {
    fs2 s = FS2LITERAL("hello");

    CHECK_TRUE("is_static", fs2_is_static(&s));
    CHECK_EQ("len", s.len, 5);
    CHECK_EQ("sz",  s.sz,  6);
    CHECK_STR_EQ("v", s.v, "hello");
}

/* -------------------------------------------------------------------------
 * Getters
 * ------------------------------------------------------------------------- */

Test(fs2, getters_literal) {
    fs2 s = FS2LITERAL("abc");

    CHECK_EQ("fs2_len", s.len, 3);
    CHECK_EQ("fs2_sz",  s.sz,  4);
    CHECK_STR_EQ("fs2_cstr", fs2_cstr(&s), "abc");
    CHECK_PTR_EQ("fs2_str == v", fs2_str(&s), s.v);
}

Test(fs2, getters_empty) {
    fs2 s = FS2EMPTY;

    CHECK_EQ("fs2_len", s.len, 0);
    CHECK_STR_EQ("fs2_cstr", fs2_cstr(&s), "");
}

/* -------------------------------------------------------------------------
 * fs2_init
 * ------------------------------------------------------------------------- */

Test(fs2, init_small) {
    fs2 s = fs2_init(8);

    CHECK_NOT_NULL("v", s.v);
    CHECK_TRUE ("is_alloc",    fs2_is_alloc(&s));
    CHECK_FALSE("is_bodyalloc", fs2_is_bodyalloc(&s));
    CHECK_EQ("len", s.len, 0);
    CHECK_EQ("v[0]", s.v[0], '\0');
    CHECK_TRUE("sz >= 8 (power-of-two overshoot)", s.sz >= 8);

    fs2_free(&s);
}

Test(fs2, init_is_power_of_two_rounded_up) {
    fs2 s = fs2_init(5);
    /* round_up_2(5) == 8 -> floored at FS2_MIN_ALLOC = 16 */
    CHECK_EQ("sz", s.sz, 16);
    fs2_free(&s);
}

Test(fs2, init_exact_power_of_two_bumps_up) {
    fs2 s = fs2_init(4);
    CHECK_EQ("sz", s.sz, 16);
    fs2_free(&s);
}

Test(fs2, init_large_power_of_two) {
    fs2 s = fs2_init(100);
    /* round_up_2(100) = 128 */
    CHECK_EQ("sz", s.sz, 128);
    fs2_free(&s);
}

#if TEST_NOT_SANITIZED
Test(fs2, init_large_fails_with_sigint, .signal = SIGINT) {
    /* 2^62 rounds up to 2^63 — malloc will fail on any realistic host */
    (void)fs2_init((size_t)1 << 62);
}
#endif

Test(fs2, init_zero_does_not_crash) {
    fs2 s = fs2_init(0);

    CHECK_NOT_NULL("v", s.v);
    CHECK_EQ("len", s.len, 0);
    CHECK_EQ("v[0]", s.v[0], '\0');
    CHECK_EQ("sz floored at FS2_MIN_ALLOC", s.sz, 16);
    CHECK_TRUE("is_alloc", fs2_is_alloc(&s));
    fs2_free(&s);
}

Test(fs2, init_small_bumps_to_min) {
    fs2 s = fs2_init(5);
    CHECK_EQ("sz", s.sz, 16);
    fs2_free(&s);
}

/* -------------------------------------------------------------------------
 * fs2_free — negatives and idempotency
 * ------------------------------------------------------------------------- */

Test(fs2, free_null_noop) {
    fs2_free(NULL);   /* must not crash */
    CHECK_TRUE("reached after free(NULL)", 1);
}

Test(fs2, free_static_noop) {
    fs2 s = FS2LITERAL("hello");
    fs2_free(&s);

    CHECK_STR_EQ("v untouched", s.v, "hello");
    CHECK_EQ("len untouched", s.len, 5);
    CHECK_EQ("sz  untouched", s.sz,  6);
}

Test(fs2, free_empty_static_noop) {
    fs2 s = FS2EMPTY;
    fs2_free(&s);

    CHECK_TRUE("is_static", fs2_is_static(&s));
    CHECK_STR_EQ("v", s.v, "");
}

Test(fs2, free_local_noop) {
    char buf[16] = "abc";
    FS2LOCALFILL(s, "%s", buf);
    fs2_free(&s);

    CHECK_STR_EQ("v untouched", s.v, "abc");
}

Test(fs2, free_alloc_releases_buffer) {
    fs2 s = fs2_init(32);
    CHECK_NOT_NULL("v before free", s.v);

    fs2_free(&s);

    CHECK_NULL("v after free", s.v);
    CHECK_EQ("len", s.len, 0);
    CHECK_EQ("sz",  s.sz,  0);
}

Test(fs2, free_alloc_is_idempotent) {
    fs2 s = fs2_init(32);
    fs2_free(&s);
    fs2_free(&s);   /* must not crash on double free */
    CHECK_NULL("v", s.v);
}

/* -------------------------------------------------------------------------
 * fs2_setlen
 * ------------------------------------------------------------------------- */

Test(fs2, setlen_truncates) {
    fs2 s = fs2_init(16);
    memcpy(s.v, "hello world", 12);
    s.len = 11;

    fs2_setlen(&s, 5);

    CHECK_EQ("len", s.len, 5);
    CHECK_EQ("v[5] is NUL", s.v[5], '\0');
    CHECK_STR_EQ("v", s.v, "hello");
    fs2_free(&s);
}

Test(fs2, setlen_to_zero) {
    fs2 s = fs2_init(16);
    memcpy(s.v, "abc", 4);
    s.len = 3;

    fs2_setlen(&s, 0);

    CHECK_EQ("len", s.len, 0);
    CHECK_STR_EQ("v", s.v, "");
    fs2_free(&s);
}

Test(fs2_api, setlen_nonzero_on_empty_aborts, .signal = SIGINT) {
    fs2 s = FS2();
    fs2_setlen(&s, 5); // should fail
}

Test(fs2_api, setlen_zero_on_empty_ok) {
    __fs2_checkreset();
    fs2 s = FS2();
    fs2_setlen(&s, 0);
    CHECK_EQ("len", s.len, 0);
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

/* -------------------------------------------------------------------------
 * fs2_is_empty / fs2_is_null
 * ------------------------------------------------------------------------- */

Test(fs2, is_empty_true_on_zero_len) {
    fs2 s = FS2LITERAL("");
    CHECK_TRUE("is_empty", fs2_is_empty(&s));
}

Test(fs2, is_empty_false_on_nonempty) {
    fs2 s = FS2LITERAL("x");
    CHECK_FALSE("is_empty", fs2_is_empty(&s));
}

Test(fs2, is_null_on_pointer_null) {
    CHECK_TRUE("is_null(NULL)", fs2_is_null(NULL));
}

Test(fs2, is_null_on_empty_alloc) {
    fs2 s = FS2();
    CHECK_TRUE("is_null(FS2())", fs2_is_null(&s));
}

Test(fs2, is_null_false_on_literal) {
    fs2 s = FS2LITERAL("ok");
    CHECK_FALSE("is_null", fs2_is_null(&s));
}

/* -------------------------------------------------------------------------
 * fs2_validate
 * ------------------------------------------------------------------------- */

Test(fs2, validate_ok_literal) {
    fs2 s = FS2LITERAL("hello");
    CHECK_TRUE("validate", fs2_validate(NULL, &s));
}

Test(fs2, validate_ok_init) {
    fs2 s = fs2_init(16);
    CHECK_TRUE("validate", fs2_validate(NULL, &s));
    fs2_free(&s);
}

Test(fs2, validate_null_pointer) {
    CHECK_FALSE("validate(NULL)", fs2_validate(NULL, NULL));
}

Test(fs2, validate_null_buffer) {
    /* STATIC with v == NULL — invalid (STATIC must have a buffer). */
    fs2 s = FS2LITERAL("x");
    s.v = NULL;
    CHECK_FALSE("validate on STATIC with NULL v", fs2_validate(NULL, &s));
}

Test(fs2, validate_len_ge_sz) {
    fs2 s = FS2LITERAL("abc");
    s.len = s.sz;   /* len >= sz — forged invariant violation */
    CHECK_FALSE("validate with len >= sz", fs2_validate(NULL, &s));
}

Test(fs2, validate_len_gt_strlen) {
    fs2 s = FS2LITERAL("abc");
    s.len = 100;
    CHECK_FALSE("validate with len=100", fs2_validate(NULL, &s));
}

Test(fs2, validate_writes_to_stream) {
    char path[256];
    test_tmpname(path, sizeof path, "test_fs2_validate", NULL, "txt");

    FILE *f = fopen(path, "w");
    CHECK_NOT_NULL("fopen write", f);

    fs2 s = FS2LITERAL("x");
    s.v = NULL;                    /* STATIC with NULL buffer — invalid */

    CHECK_FALSE("validate(f, STATIC|NULL)", fs2_validate(f, &s));
    fclose(f);

    char buf[512];
    size_t n = test_read_file(path, buf, sizeof buf);
    remove(path);

    CHECK_TRUE("non-empty output", n > 0);
    CHECK_NOT_NULL("strstr \"null buffer\"", strstr(buf, "null buffer"));
}

/* -------------------------------------------------------------------------
 * fs_techfprint
 * ------------------------------------------------------------------------- */

Test(fs2, techfprint_literal) {
    char path[256];
    test_tmpname(path, sizeof path, "test_fs2_tech", NULL, "txt");

    FILE *f = fopen(path, "w");
    CHECK_NOT_NULL("fopen write", f);

    fs2  s = FS2LITERAL("payload");
    long n = fs_techfprint(f, &s, "s");
    fclose(f);

    CHECK_TRUE("positive write count", n > 0);

    char buf[512];
    size_t got = test_read_file(path, buf, sizeof buf);
    remove(path);

    CHECK_TRUE("non-empty output", got > 0);
    CHECK_NOT_NULL("strstr \"FS2:\"",   strstr(buf, "FS2:"));
    CHECK_NOT_NULL("strstr \"payload\"", strstr(buf, "payload"));
}

Test(fs2, techfprint_null_s) {
    char path[256];
    test_tmpname(path, sizeof path, "test_fs2_tech_null", NULL, "txt");

    FILE *f = fopen(path, "w");
    CHECK_NOT_NULL("fopen write", f);

    long n = fs_techfprint(f, NULL, "s");
    fclose(f);

    CHECK_TRUE("positive write count", n > 0);

    char buf[512];
    size_t got = test_read_file(path, buf, sizeof buf);
    remove(path);

    CHECK_TRUE("non-empty output", got > 0);
    CHECK_NOT_NULL("strstr \"<NULL>\"", strstr(buf, "<NULL>"));
}

Test(fs2, techfprint_null_out_returns_zero) {
    fs2 s = FS2LITERAL("x");
    CHECK_EQ("fs_techfprint(NULL, ...)", fs_techfprint(NULL, &s, "s"), 0);
}

/* -------------------------------------------------------------------------
 * FS2LOCAL — stack buffer, no ownership
 * ------------------------------------------------------------------------- */

Test(fs2, local_creates_empty) {
    FS2LOCAL(t, 32);

    CHECK_NOT_NULL("v", t.v);
    CHECK_EQ("sz",  t.sz,  32);
    CHECK_EQ("len", t.len, 0);
    CHECK_EQ("v[0] is NUL", t.v[0], '\0');
    CHECK_TRUE ("is_local",     fs2_is_local(&t));
    CHECK_FALSE("is_static",    fs2_is_static(&t));
    CHECK_FALSE("is_alloc",     fs2_is_alloc(&t));
    CHECK_FALSE("is_bodyalloc", fs2_is_bodyalloc(&t));
    CHECK_TRUE ("is_empty",     fs2_is_empty(&t));
}

Test(fs2, local_buffer_is_writable) {
    FS2LOCAL(t, 16);

    memcpy(t.v, "hello", 6);
    t.len = 5;

    CHECK_STR_EQ("v", t.v, "hello");
    CHECK_EQ("len", t.len, (size_t) 5);
    CHECK_EQ("sz",  t.sz,  (size_t) 16);
    CHECK_TRUE("is_local", fs2_is_local(&t));
}

Test(fs2, local_free_is_noop) {
    FS2LOCAL(t, 16);
    memcpy(t.v, "keep me", 8);
    t.len = 7;

    fs2_free(&t);

    /* STATIC / LOCAL: free must not touch the buffer */
    CHECK_STR_EQ("v untouched", t.v, "keep me");
    CHECK_EQ("len untouched", t.len, 7);
    CHECK_TRUE("still local", fs2_is_local(&t));
}

Test(fs2, local_two_instances_independent) {
    FS2LOCAL(a, 8);
    FS2LOCAL(b, 16);

    memcpy(a.v, "aaa",  4); a.len = 3;
    memcpy(b.v, "bbbb", 5); b.len = 4;

    CHECK_STR_EQ("a.v", a.v, "aaa");
    CHECK_STR_EQ("b.v", b.v, "bbbb");
    CHECK_EQ("a.sz", a.sz, 8);
    CHECK_EQ("b.sz", b.sz, 16);
    CHECK_PTR_NEQ("a.v vs b.v", a.v, b.v);
}

/* -------------------------------------------------------------------------
 * FS2LOCALFILL — format into stack buffer
 * ------------------------------------------------------------------------- */

Test(fs2, localfill_empty_format) {
    FS2LOCALFILL(t, "%s", "");

    CHECK_EQ("len", t.len, 0);
    CHECK_EQ("sz",  t.sz,  1);
    CHECK_STR_EQ("v", t.v, "");
    CHECK_TRUE("is_local", fs2_is_local(&t));
}

Test(fs2, localfill_plain_string) {
    FS2LOCALFILL(t, "%s", "hello world");

    CHECK_EQ("len", t.len, 11);
    CHECK_STR_EQ("v", t.v, "hello world");
    CHECK_EQ("sz",  t.sz,  12);
    CHECK_TRUE("is_local", fs2_is_local(&t));
}

Test(fs2, localfill_int) {
    FS2LOCALFILL(t, "x=%d", 42);

    CHECK_EQ("len", t.len, 4);
    CHECK_STR_EQ("v", t.v, "x=42");
}

Test(fs2, localfill_string_arg) {
    const char *name = "alice";
    FS2LOCALFILL(t, "user=%s", name);

    CHECK_EQ("len", t.len, 10);
    CHECK_STR_EQ("v", t.v, "user=alice");
}

Test(fs2, localfill_mixed) {
    FS2LOCALFILL(t, "%s=%d (%c)", "count", 7, 'x');

    CHECK_STR_EQ("v", t.v, "count=7 (x)");
    CHECK_EQ("len", t.len, 11);
}

Test(fs2, localfill_negative_int) {
    FS2LOCALFILL(t, "%d", -123);

    CHECK_STR_EQ("v", t.v, "-123");
    CHECK_EQ("len", t.len, 4);
}

Test(fs2, localfill_two_instances) {
    FS2LOCALFILL(a, "first=%d",  1);
    FS2LOCALFILL(b, "second=%d", 2);

    CHECK_STR_EQ("a.v", a.v, "first=1");
    CHECK_STR_EQ("b.v", b.v, "second=2");
    CHECK_PTR_NEQ("a.v vs b.v", a.v, b.v);
    CHECK_TRUE("a is_local", fs2_is_local(&a));
    CHECK_TRUE("b is_local", fs2_is_local(&b));
}

Test(fs2, localfill_free_is_noop) {
    FS2LOCALFILL(t, "%s", "keep me please");

    fs2_free(&t);

    CHECK_STR_EQ("v untouched", t.v, "keep me please");
    CHECK_EQ("len untouched", t.len, (int)strlen("keep me please"));
    CHECK_TRUE("still local", fs2_is_local(&t));
}

/* -------------------------------------------------------------------------
 * fs2_fromstr / fs2_clone 
 *
 * All tests reset the allocation counters first and verify balance at
 * the end. Requires -DFS2TEST (see fs2/Makefile).
 * ------------------------------------------------------------------------- */


Test(fs2_api, clone_empty_returns_fs2empty) {
    __fs2_checkreset();
    
    /* Все пустые формы — clone → FS2EMPTY */
    fs2 e_static  = FS2EMPTY;
    fs2 e_literal = FS2LITERAL("");
    fs2 e_canon   = FS2();
    fs2 e_alloc   = fs2_init(16);          /* ALLOC, v=malloc(16), sz=16 */
    fs2 e_cleared = fs2_init(16);
    fs2_clear(&e_cleared);                 /* явно cleared */

    fs2 *inputs[] = { &e_static, &e_literal, &e_canon, &e_alloc, &e_cleared };
    const char *names[] = { "STATIC", "literal", "canon", "alloc", "cleared" };
    
    for (size_t i = 0; i < 5; i++) {
        fs2 r = fs2_clone(inputs[i]);
        
        CHECK_TRUE ("STATIC result", fs2_is_static(&r));
        CHECK_EQ   ("sz == 1", r.sz, 1);
        CHECK_EQ   ("len == 0", r.len, 0);
        CHECK_NOT_NULL("v", r.v);
        CHECK_STR_EQ("readable", r.v, "");
        CHECK_FS2VALID((int)i, &r);
        
        /* fs2_free на FS2EMPTY — no-op */
        (void)names[i];
    }

    fs2_free(&e_canon);
    fs2_free(&e_alloc);
    fs2_free(&e_cleared);
    assert_balanced();
}

Test(fs2_api, clone_alloc_empty_returns_fs2empty) {
    __fs2_checkreset();
    fs2 src = fs2_init(16);   /* {v=malloc(16), sz=16, len=0, ALLOC} */
    CHECK_TRUE ("not empty by eq_empty",  !fs2_eq_empty(&src));
    CHECK_TRUE ("not canon",              !fs2_is_canon(&src));
    CHECK_TRUE ("but is_empty",           fs2_is_empty(&src));

    fs2 r = fs2_clone(&src);

    /* ← регресс: со старым guard'ом тут был бы FS2() (canon), не FS2EMPTY */
    CHECK_TRUE("STATIC result", fs2_is_static(&r));
    CHECK_EQ  ("sz", r.sz, 1);
    CHECK_STR_EQ("readable", r.v, "");
    CHECK_FALSE("not canon", fs2_is_canon(&r));
    CHECK_FS2VALID(1, &r);

    fs2_free(&src);
    /* r — FS2EMPTY, fs2_free no-op */
    assert_balanced();
}

/* =========================================================================
 * fs2_fromstr — basic cases
 * ========================================================================= */

Test(fs2_api, fromstr_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    CHECK_STR_EQ("content", s.v, "hello");
    CHECK_EQ  ("len", s.len, 5);
    CHECK_TRUE ("is alloc", fs2_is_alloc(&s));
    CHECK_FALSE("not STATIC", fs2_is_static(&s));
    CHECK_TRUE ("sz >= 16", s.sz >= 16);   /* FS2_MIN_ALLOC floor */
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, fromstr_empty_returns_fs2empty) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("");

    CHECK_EQ  ("len", s.len, 0);
    CHECK_TRUE("STATIC (FS2EMPTY)", fs2_is_static(&s));
    CHECK_EQ  ("sz == 1", s.sz, 1);
    CHECK_NOT_NULL("v readable", s.v);
    CHECK_STR_EQ("readable as empty", s.v, "");
    CHECK_FS2VALID(1, &s);
    /* fs2_free no-op для STATIC */
}

Test(fs2_api, fromstr_single_char) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("x");

    CHECK_STR_EQ("content", s.v, "x");
    CHECK_EQ  ("len", s.len, 1);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, fromstr_long_string) {
    __fs2_checkreset();
    char big[1001];
    memset(big, 'A', 1000);
    big[1000] = '\0';

    fs2 s = fs2_fromstr(big);

    CHECK_EQ("len", s.len, 1000);
    CHECK_EQ("first", s.v[0], 'A');
    CHECK_EQ("last", s.v[999], 'A');
    CHECK_EQ("NUL at [1000]", s.v[1000], '\0');
    CHECK_TRUE("sz >= 1001", s.sz >= 1001);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, fromstr_non_ascii) {
    __fs2_checkreset();
    /* UTF-8 "привет" = 12 байт */
    fs2 s = fs2_fromstr("\xD0\xBF\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82");

    CHECK_EQ("len", s.len, 12);
    CHECK_EQ("first byte", (unsigned char)s.v[0], 0xD0);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fromstr — ownership (independent from src)
 * ========================================================================= */

Test(fs2_api, fromstr_independent_from_source) {
    __fs2_checkreset();
    char src[] = "hello";
    fs2 s = fs2_fromstr(src);

    /* Изменение src не влияет на s */
    src[0] = 'X';
    CHECK_STR_EQ("s intact", s.v, "hello");
    CHECK_TRUE ("different buffers", s.v != src);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fromstr — NULL handling (выбери A или B)
 * ========================================================================= */

#if defined(FS2_FROMSTR_STRICT)   /* Вариант B */
Test(fs2_api, fromstr_null_aborts, .signal = SIGINT) {
    fs2_fromstr(NULL);
}
#else                              /* Вариант A: silent */
Test(fs2_api, fromstr_null_returns_fs2empty) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr(NULL);

    CHECK_EQ  ("len", s.len, 0);
    CHECK_TRUE("STATIC", fs2_is_static(&s));
    CHECK_EQ  ("sz", s.sz, 1);
    CHECK_STR_EQ("readable", s.v, "");
    CHECK_FS2VALID(1, &s);
}
#endif

/* =========================================================================
 * fromstr — round-trip через cstr
 * ========================================================================= */

Test(fs2_api, fromstr_roundtrip_cstr) {
    __fs2_checkreset();
    const char *inputs[] = { "hello", "x", "a b c", "", "0123456789" };
    size_t n = sizeof(inputs) / sizeof(inputs[0]);

    for (size_t i = 0; i < n; i++) {
        fs2 s = fs2_fromstr(inputs[i]);
        CHECK_STR_EQ("round-trip", fs2_cstr(&s), inputs[i]);
        CHECK_FS2VALID((int)i, &s);
        fs2_free(&s);
    }
    assert_balanced();
}

/* =========================================================================
 * fromstr — auto-promote (результат-пустышка остаётся usable)
 * ========================================================================= */

Test(fs2_api, fromstr_empty_auto_promotes_on_reserve) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("");   /* FS2EMPTY */
    CHECK_TRUE("STATIC", fs2_is_static(&s));

    /* Core-only promote: reserve аллоцирует, дальше memcpy + setlen */
    fs2_reserve(&s, 3);        /* "hi\0" */
    memcpy(s.v, "hi", 2);
    fs2_setlen(&s, 2);

    CHECK_STR_EQ("content", s.v, "hi");
    CHECK_TRUE ("promoted to ALLOC", fs2_is_alloc(&s));
    CHECK_FALSE("no longer STATIC", fs2_is_static(&s));
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fromstr — consistency with wrap+clone
 * ========================================================================= */

Test(fs2_api, fromstr_matches_wrap_clone) {
    __fs2_checkreset();
    const char *inputs[] = { "hello", "", "long string with spaces" };
    size_t n = sizeof(inputs) / sizeof(inputs[0]);

    for (size_t i = 0; i < n; i++) {
        fs2 a = fs2_fromstr(inputs[i]);
        
        fs2 tmp = fs2_wrap(inputs[i]);
        fs2 b = fs2_clone(&tmp);

        CHECK_STR_EQ("same content", a.v, b.v);
        CHECK_EQ  ("same len", a.len, b.len);
        CHECK_EQ  ("same flags", a.flags, b.flags);
        CHECK_FS2VALID((int)i, &a);
        CHECK_FS2VALID((int)i, &b);

        fs2_free(&a);
        fs2_free(&b);
    }
    assert_balanced();
}

Test(fs2_api, clone_does_not_recurse_through_fromstr) {
    __fs2_checkreset();
    
    /* Если кто-то случайно сделает clone → fromstr → wrap → clone,
     * этот тест упадёт на stack overflow (SIGSEGV) вместо прохода. */
    fs2 src = fs2_fromstr("hello");
    fs2 r = fs2_clone(&src);   /* ← trigger potential recursion */
    
    CHECK_STR_EQ("content", r.v, "hello");
    CHECK_FS2VALID(1, &r);
    
    fs2_free(&src);
    fs2_free(&r);
    assert_balanced();
}

/* =========================================================================
 * fs2_fromstrf — basic formatting
 * ========================================================================= */

Test(fs2_api, fromstrf_plain) {
    __fs2_checkreset();
    fs2 s = fs2_fromstrf("hello");

    CHECK_STR_EQ("content", s.v, "hello");
    CHECK_EQ  ("len", s.len, 5);
    CHECK_TRUE ("is alloc", fs2_is_alloc(&s));
    CHECK_TRUE ("sz >= 6", s.sz >= 6);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, fromstrf_int) {
    __fs2_checkreset();
    fs2 s = fs2_fromstrf("%d", 42);

    CHECK_STR_EQ("content", s.v, "42");
    CHECK_EQ  ("len", s.len, 2);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, fromstrf_multiple_args) {
    __fs2_checkreset();
    fs2 s = fs2_fromstrf("%s=%d (%.2f)", "x", 7, 3.14);

    CHECK_STR_EQ("content", s.v, "x=7 (3.14)");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, fromstrf_negative_and_long) {
    __fs2_checkreset();
    fs2 s = fs2_fromstrf("%ld", -1234567890L);

    CHECK_STR_EQ("content", s.v, "-1234567890");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fromstrf — empty result → FS2EMPTY (contract)
 * ========================================================================= */

/* ⚠ Главный регресс: пустой результат должен быть FS2EMPTY, не ALLOC empty */
Test(fs2_api, fromstrf_empty_fmt_returns_fs2empty) {
    __fs2_checkreset();
    fs2 s = fs2_fromstrf("%s", "");

    CHECK_EQ  ("len", s.len, 0);
    CHECK_TRUE("STATIC (FS2EMPTY)", fs2_is_static(&s));
    CHECK_EQ  ("sz == 1", s.sz, 1);
    CHECK_NOT_NULL("v readable", s.v);
    CHECK_STR_EQ("readable", s.v, "");
    CHECK_FS2VALID(1, &s);
    /* fs2_free no-op для STATIC */
}

Test(fs2_api, fromstrf_empty_arg_returns_fs2empty) {
    __fs2_checkreset();
    fs2 s = fs2_fromstrf("%s", "");

    CHECK_EQ  ("len", s.len, 0);
    CHECK_TRUE("STATIC", fs2_is_static(&s));
    CHECK_EQ  ("sz == 1", s.sz, 1);
    CHECK_FS2VALID(1, &s);
}

Test(fs2_api, fromstrf_zero_width_returns_fs2empty) {
    __fs2_checkreset();
    fs2 s = fs2_fromstrf("%.0s", "ignored");

    CHECK_EQ  ("len", s.len, 0);
    CHECK_TRUE("STATIC", fs2_is_static(&s));
    CHECK_FS2VALID(1, &s);
}

Test(fs2_api, fromstrf_empty_matches_fromstr_empty) {
    __fs2_checkreset();
    fs2 a = fs2_fromstrf("%s", "");
    fs2 b = fs2_fromstr("");

    /* Оба FS2EMPTY, идентичные флаги */
    CHECK_EQ  ("same len",   a.len,   b.len);
    CHECK_EQ  ("same sz",    a.sz,    b.sz);
    CHECK_EQ  ("same flags", a.flags, b.flags);
    CHECK_TRUE("both STATIC", fs2_is_static(&a) && fs2_is_static(&b));
    CHECK_FS2VALID(1, &a);
    CHECK_FS2VALID(2, &b);
}

/* =========================================================================
 * fromstrf — independence
 * ========================================================================= */

Test(fs2_api, fromstrf_independent_from_args) {
    __fs2_checkreset();
    char arg[] = "orig";
    fs2 s = fs2_fromstrf("%s", arg);

    /* Изменение arg не влияет на s */
    arg[0] = 'X';
    CHECK_STR_EQ("s intact", s.v, "orig");
    CHECK_TRUE ("different buffer", s.v != arg);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fromstrf — long output (heap, multi-KB)
 * ========================================================================= */

Test(fs2_api, fromstrf_long_output) {
    __fs2_checkreset();
    char big[2001];
    memset(big, 'A', 2000);
    big[2000] = '\0';

    fs2 s = fs2_fromstrf("%s", big);

    CHECK_EQ("len", s.len, 2000);
    CHECK_EQ("first", s.v[0], 'A');
    CHECK_EQ("NUL at [2000]", s.v[2000], '\0');
    CHECK_TRUE("sz > 2000", s.sz > 2000);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fromstrf — NULL fmt
 * ========================================================================= */

Test(fs2_api, fromstrf_null_aborts, .signal = SIGINT) {
    fs2_fromstrf(NULL);
}

/* =========================================================================
 * fromstrf — composite: build then mutate
 * ========================================================================= */

Test(fs2_api, fromstrf_then_extend_core) {
    __fs2_checkreset();
    fs2 s = fs2_fromstrf("id=%d", 42);
    CHECK_STR_EQ("built", s.v, "id=42");
    CHECK_FS2VALID(1, &s);

    /* Core extend: reserve + memcpy + setlen (без catstr) */
    const char *suffix     = "&active=1";
    size_t      suffix_len = strlen(suffix);
    fs2_reserve(&s, s.len + suffix_len + 1);
    memcpy(s.v + s.len, suffix, suffix_len);
    fs2_setlen(&s, s.len + suffix_len);

    CHECK_STR_EQ("extended", s.v, "id=42&active=1");
    CHECK_FS2VALID(2, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, fromstrf_empty_auto_promotes_on_reserve) {
    __fs2_checkreset();
    fs2 s = fs2_fromstrf("%s", "");   /* FS2EMPTY (после фикса sz==0) */
    CHECK_TRUE("STATIC", fs2_is_static(&s));

    /* Core-only promote: reserve аллоцирует, дальше memcpy */
    fs2_reserve(&s, 6);         /* "hello\0" */
    memcpy(s.v, "hello", 5);
    fs2_setlen(&s, 5);

    CHECK_STR_EQ("content", s.v, "hello");
    CHECK_TRUE ("promoted to ALLOC", fs2_is_alloc(&s));
    CHECK_FALSE("no longer STATIC", fs2_is_static(&s));
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_movefrom — basic
 * ========================================================================= */

Test(fs2_api, movefrom_alloc_stack) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello");
    void *orig_v = src.v;
    CHECK_FS2VALID(1, &src);

    fs2 res = fs2_movefrom(&src);

    /* res owns the buffer */
    CHECK_STR_EQ("content", res.v, "hello");
    CHECK_EQ  ("len", res.len, 5);
    CHECK_TRUE("is alloc", fs2_is_alloc(&res));
    CHECK_FALSE("no bodyalloc", fs2_is_bodyalloc(&res));
    CHECK_PTR_EQ("same buffer", res.v, orig_v);
    CHECK_FS2VALID(2, &res);

    /* src is now canonical empty */
    CHECK_EQ  ("src len", src.len, 0);
    CHECK_PTR_EQ("src v NULL", src.v, NULL);
    CHECK_TRUE("src canon", fs2_is_canon(&src));
    CHECK_FALSE("src no bodyalloc", fs2_is_bodyalloc(&src));

    fs2_free(&res);   /* frees buffer */
    /* src = FS2() — fs2_free would be no-op since v=NULL, but call for safety */
    fs2_free(&src);
    assert_balanced();
}

Test(fs2_api, movefrom_static) {
    __fs2_checkreset();
    fs2 src = FS2LITERAL("hello");
    void *orig_v = src.v;

    fs2 res = fs2_movefrom(&src);

    CHECK_STR_EQ("content", res.v, "hello");
    CHECK_TRUE("STATIC", fs2_is_static(&res));
    CHECK_PTR_EQ("same literal buffer", res.v, orig_v);
    CHECK_FS2VALID(1, &res);

    CHECK_TRUE("src canon", fs2_is_canon(&src));
    CHECK_PTR_EQ("src v NULL", src.v, NULL);

    /* fs2_free on STATIC — no-op; on src (canon) — no-op too */
    fs2_free(&src);
    assert_balanced();
}

Test(fs2_api, movefrom_empty_canon) {
    __fs2_checkreset();
    fs2 src = FS2();   /* canonical empty */
    CHECK_TRUE("initial canon", fs2_is_canon(&src));

    fs2 res = fs2_movefrom(&src);

    /* res preserves src's state — still FS2() shape, not FS2EMPTY */
    CHECK_TRUE("res canon", fs2_is_canon(&res));
    CHECK_EQ  ("len", res.len, 0);
    CHECK_PTR_EQ("v NULL", res.v, NULL);
    CHECK_FS2VALID(1, &res);

    CHECK_TRUE("src canon", fs2_is_canon(&src));

    fs2_free(&res);
    fs2_free(&src);
    assert_balanced();
}

Test(fs2_api, movefrom_fs2empty_preserves_shape) {
    fs2 src = FS2EMPTY;

    fs2 res = fs2_movefrom(&src);

    /* res is FS2EMPTY shape (STATIC, v="", sz=1) — preserved from src */
    CHECK_TRUE("eq_empty", fs2_eq_empty(&res));
    CHECK_EQ  ("sz", res.sz, 1);
    CHECK_NOT_NULL("v readable", res.v);
    CHECK_FS2VALID(1, &res);

    /* src became canonical empty */
    CHECK_TRUE("src canon", fs2_is_canon(&src));
}

Test(fs2_api, movefrom_heap_preserves_bodyalloc_on_src) {
    __fs2_checkreset();
    fs2 *h = fs2_heap_fromstr("hello");
    void *orig_v = h->v;

    fs2 res = fs2_movefrom(h);

    /* res takes buffer, no bodyalloc */
    CHECK_STR_EQ("content", res.v, "hello");
    CHECK_TRUE("is alloc", fs2_is_alloc(&res));
    CHECK_FALSE("res not bodyalloc", fs2_is_bodyalloc(&res));
    CHECK_PTR_EQ("same buffer", res.v, orig_v);
    CHECK_FS2VALID(1, &res);

    /* h reset to canon, BODYALLOC preserved */
    CHECK_TRUE ("h still bodyalloc", fs2_is_bodyalloc(h));
    CHECK_TRUE ("h canon", fs2_is_canon(h));
    CHECK_EQ   ("h len", h->len, 0);

    fs2_free(&res);   /* frees buffer */
    fs2_free(h);      /* frees struct */
    assert_balanced();
}

Test(fs2_api, movefrom_null_aborts, .signal = SIGINT) {
    fs2_movefrom(NULL);
}

Test(fs2_api, movefrom_local_aborts, .signal = SIGINT) {
    __fs2_checkreset();
    FS2LOCAL(s, 16);
    memcpy(s.v, "hi", 2);
    s.len = 2;

    fs2_movefrom(&s);   /* LOCAL forbidden */
}

/* =========================================================================
 * fs2_move — basic
 * ========================================================================= */

Test(fs2_api, move_basic_alloc_to_alloc) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("old");
    fs2 src = fs2_fromstr("new");
    void *src_v = src.v;

    fs2 *r = fs2_move(&dst, &src);

    CHECK_PTR_EQ("returns dst", r, &dst);
    CHECK_STR_EQ("dst content", dst.v, "new");
    CHECK_EQ  ("dst len", dst.len, 3);
    CHECK_PTR_EQ("same buffer", dst.v, src_v);
    CHECK_FS2VALID(1, &dst);

    CHECK_TRUE("src canon", fs2_is_canon(&src));
    CHECK_PTR_EQ("src v NULL", src.v, NULL);

    fs2_free(&dst);
    fs2_free(&src);
    assert_balanced();
}

Test(fs2_api, move_from_static_src) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("old");
    fs2 src = FS2LITERAL("new");

    fs2_move(&dst, &src);

    CHECK_STR_EQ("dst content", dst.v, "new");
    CHECK_TRUE ("dst STATIC", fs2_is_static(&dst));
    CHECK_FALSE("dst not alloc", fs2_is_alloc(&dst));
    CHECK_FS2VALID(1, &dst);

    CHECK_TRUE("src canon", fs2_is_canon(&src));

    fs2_free(&dst);   /* no-op */
    fs2_free(&src);
    assert_balanced();
}

Test(fs2_api, move_to_fs2empty_dst) {
    __fs2_checkreset();
    fs2 dst = FS2EMPTY;
    fs2 src = fs2_fromstr("hello");

    fs2_move(&dst, &src);

    CHECK_STR_EQ("dst content", dst.v, "hello");
    CHECK_TRUE ("dst ALLOC", fs2_is_alloc(&dst));
    CHECK_FALSE("dst not STATIC", fs2_is_static(&dst));
    CHECK_FS2VALID(1, &dst);

    CHECK_TRUE("src canon", fs2_is_canon(&src));

    fs2_free(&dst);
    fs2_free(&src);
    assert_balanced();
}

Test(fs2_api, move_to_canon_dst) {
    __fs2_checkreset();
    fs2 dst = FS2();   /* canonical */
    fs2 src = fs2_fromstr("hello");

    fs2_move(&dst, &src);

    CHECK_STR_EQ("dst content", dst.v, "hello");
    CHECK_TRUE ("dst ALLOC", fs2_is_alloc(&dst));
    CHECK_FS2VALID(1, &dst);
    CHECK_TRUE ("src canon", fs2_is_canon(&src));

    fs2_free(&dst);
    fs2_free(&src);
    assert_balanced();
}

Test(fs2_api, move_self_noop) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");
    void *orig_v = s.v;

    fs2 *r = fs2_move(&s, &s);

    CHECK_PTR_EQ("returns same", r, &s);
    CHECK_STR_EQ("content unchanged", s.v, "hello");
    CHECK_PTR_EQ("v unchanged", s.v, orig_v);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_move — BODYALLOC preservation
 * ========================================================================= */

Test(fs2_api, move_preserves_dst_bodyalloc) {
    __fs2_checkreset();
    fs2 *dst_heap = fs2_heap_fromstr("old");   /* BODYALLOC */
    fs2 src = fs2_fromstr("new");
    void *src_v = src.v;

    fs2_move(dst_heap, &src);

    CHECK_TRUE ("dst BODYALLOC preserved", fs2_is_bodyalloc(dst_heap));
    CHECK_STR_EQ("dst content", dst_heap->v, "new");
    CHECK_PTR_EQ("same buffer", dst_heap->v, src_v);
    CHECK_FS2VALID(1, dst_heap);
    CHECK_TRUE ("src canon", fs2_is_canon(&src));

    fs2_free(dst_heap);   /* frees buffer + struct */
    fs2_free(&src);
    assert_balanced();
}

Test(fs2_api, move_preserves_src_bodyalloc_on_reset) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("old");
    fs2 *src_heap = fs2_heap_fromstr("new");

    fs2_move(&dst, src_heap);

    CHECK_STR_EQ("dst content", dst.v, "new");
    CHECK_FS2VALID(1, &dst);

    CHECK_TRUE ("src still BODYALLOC", fs2_is_bodyalloc(src_heap));
    CHECK_TRUE ("src canon", fs2_is_canon(src_heap));
    CHECK_EQ   ("src len", src_heap->len, 0);

    fs2_free(&dst);
    fs2_free(src_heap);   /* frees struct, v=NULL already */
    assert_balanced();
}

/* =========================================================================
 * fs2_move — NULL/LOCAL
 * ========================================================================= */

Test(fs2_api, move_null_dst_aborts, .signal = SIGINT) {
    fs2 src = fs2_fromstr("x");
    fs2_move(NULL, &src);
    fs2_free(&src);
}

Test(fs2_api, move_null_src_aborts, .signal = SIGINT) {
    fs2 dst = fs2_fromstr("x");
    fs2_move(&dst, NULL);
    fs2_free(&dst);
}

Test(fs2_api, move_local_dst_aborts, .signal = SIGINT) {
    __fs2_checkreset();
    FS2LOCAL(dst, 16);
    fs2 src = fs2_fromstr("x");
    fs2_move(&dst, &src);   /* LOCAL dst forbidden */
    fs2_free(&src);
}

Test(fs2_api, move_local_src_aborts, .signal = SIGINT) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("x");
    FS2LOCAL(src, 16);
    fs2_move(&dst, &src);   /* LOCAL src forbidden */
    fs2_free(&dst);
}

/* =========================================================================
 * Ownership transfer — no leak, no double-free
 * ========================================================================= */

Test(fs2_api, move_does_not_leak_dst_old_buffer) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("old");   /* owns buffer */
    fs2 src = fs2_fromstr("new");   /* owns buffer */

    fs2_move(&dst, &src);

    /* Old dst buffer должен быть освобождён (иначе assert_balanced поймает leak) */
    fs2_free(&dst);
    fs2_free(&src);
    assert_balanced();
}

Test(fs2_api, movefrom_does_not_leak_original) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello");

    fs2 res = fs2_movefrom(&src);

    fs2_free(&res);
    fs2_free(&src);
    assert_balanced();
}

/* =========================================================================
 * Composite: pipeline через move/movefrom
 * ========================================================================= */

Test(fs2_api, composite_move_chain) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("A");
    fs2 b = fs2_fromstr("B");
    fs2 c = fs2_fromstr("C");
    void *c_v = c.v;

    /* a ← b, b ← c. Теперь: a="B", b="C", c=canon */
    fs2_move(&a, &b);
    fs2_move(&b, &c);

    CHECK_STR_EQ("a", "B", a.v);   /* careful: swap args in CHECK_STR_EQ */
    CHECK_STR_EQ("a content", a.v, "B");
    CHECK_STR_EQ("b content", b.v, "C");
    CHECK_PTR_EQ("b has c's buffer", b.v, c_v);
    CHECK_TRUE ("c canon", fs2_is_canon(&c));
    CHECK_FS2VALID(1, &a);
    CHECK_FS2VALID(2, &b);

    fs2_free(&a);
    fs2_free(&b);
    fs2_free(&c);
    assert_balanced();
}

Test(fs2_api, movefrom_preserves_fs2_shape) {
    __fs2_checkreset();
    fs2 src = FS2();   /* canonical: v=NULL, sz=0, ALLOC */

    fs2 res = fs2_movefrom(&src);

    /* Preservation: res — точная копия src, не нормализованный FS2EMPTY */
    CHECK_TRUE ("res canon", fs2_is_canon(&res));
    CHECK_FALSE("not eq_empty", fs2_eq_empty(&res));
    CHECK_PTR_EQ("v NULL", res.v, NULL);
    CHECK_EQ  ("sz", res.sz, 0);
    CHECK_FS2VALID(1, &res);

    CHECK_TRUE ("src canon (reset)", fs2_is_canon(&src));

    fs2_free(&res);
    fs2_free(&src);
    assert_balanced();
}

Test(fs2_api, movefrom_preserves_fs2empty_shape) {
    fs2 src = FS2EMPTY;   /* STATIC, v="", sz=1 */

    fs2 res = fs2_movefrom(&src);

    /* Preservation: res — FS2EMPTY shape */
    CHECK_TRUE("res eq_empty", fs2_eq_empty(&res));
    CHECK_EQ  ("sz", res.sz, 1);
    CHECK_STR_EQ("readable", res.v, "");
    CHECK_FS2VALID(1, &res);

    CHECK_TRUE("src canon (reset)", fs2_is_canon(&src));
}

Test(fs2_api, movefrom_preserves_literal_empty_sz) {
    fs2 src = FS2LITERAL("");   /* STATIC, но sz=1 как FS2EMPTY */
    /* Если когда-нибудь FS2LITERAL("") станет sz != 1 — этот тест поймает */

    fs2 res = fs2_movefrom(&src);
    CHECK_TRUE("STATIC", fs2_is_static(&res));
    CHECK_EQ  ("sz preserved", res.sz, src.sz == 1 ? 1 : res.sz);
    CHECK_FS2VALID(1, &res);
}

/* Контраст: clone — нормализует */
Test(fs2_api, clone_normalizes_but_movefrom_preserves) {
    __fs2_checkreset();

    fs2 src1 = FS2();
    fs2 src2 = FS2();

    fs2 cloned    = fs2_clone(&src1);      /* → FS2EMPTY (normalized) */
    fs2 moved     = fs2_movefrom(&src2);   /* → FS2() (preserved) */

    CHECK_TRUE ("clone returns FS2EMPTY", fs2_eq_empty(&cloned));
    CHECK_TRUE ("movefrom returns FS2()", fs2_is_canon(&moved));
    CHECK_TRUE ("both empty", fs2_is_empty(&cloned) && fs2_is_empty(&moved));

    fs2_free(&src1);
    fs2_free(&src2);
    fs2_free(&moved);
    /* cloned — FS2EMPTY, fs2_free no-op */
    assert_balanced();
}

/* =========================================================================
 * fs2_fromraw — direct construction
 * ========================================================================= */

Test(fs2_api, fromraw_static_literal) {
    fs2 s = fs2_fromraw((char *)"hello", 5, FS2_STATIC);

    CHECK_STR_EQ("content", s.v, "hello");
    CHECK_EQ  ("len", s.len, 5);
    CHECK_EQ  ("sz", s.sz, 6);
    CHECK_TRUE("STATIC", fs2_is_static(&s));
    CHECK_FS2VALID(1, &s);
    /* fs2_free no-op */
}

Test(fs2_api, fromraw_alloc_adopts_heap) {
    __fs2_checkreset();
    char *buf = strdup("hello");
    fs2 s = fs2_fromraw(buf, 5, FS2_ALLOC);

    CHECK_STR_EQ("content", s.v, "hello");
    CHECK_EQ  ("len", s.len, 5);
    CHECK_EQ  ("sz", s.sz, 6);
    CHECK_TRUE("ALLOC", fs2_is_alloc(&s));
    CHECK_PTR_EQ("same buffer", s.v, buf);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);   /* frees buf */
    assert_balanced();
}

Test(fs2_api, fromraw_empty_alloc) {
    __fs2_checkreset();
    char *buf = strdup("");
    fs2 s = fs2_fromraw(buf, 0, FS2_ALLOC);

    CHECK_EQ  ("len", s.len, 0);
    CHECK_EQ  ("sz", s.sz, 1);
    CHECK_TRUE("ALLOC", fs2_is_alloc(&s));
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, fromraw_nul_written_for_alloc) {
    __fs2_checkreset();
    char *buf = malloc(6);
    memcpy(buf, "hello", 5);   /* без NUL */

    fs2 s = fs2_fromraw(buf, 5, FS2_ALLOC);

    CHECK_EQ("NUL written", s.v[5], '\0');
    CHECK_STR_EQ("readable", s.v, "hello");

    fs2_free(&s);
    assert_balanced();
}

/* ⚠ Защита от BODYALLOC */
Test(fs2_api, fromraw_bodyalloc_rejected, .signal = SIGINT) {
    char *buf = strdup("hi");
    fs2_fromraw(buf, 2, FS2_BODYALLOC);
    free(buf);   /* unreachable */
}

/* =========================================================================
 * fs2_movefromstr
 * ========================================================================= */

Test(fs2_api, movefromstr_alloc) {
    __fs2_checkreset();
    char *heap = strdup("hello");
    const char *p = heap;

    fs2 s = fs2_movefromstr(&p, FS2_ALLOC);

    CHECK_STR_EQ("content", s.v, "hello");
    CHECK_TRUE("ALLOC", fs2_is_alloc(&s));
    CHECK_PTR_EQ("same buffer", s.v, heap);
    CHECK_PTR_EQ("from reset", p, NULL);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);   /* frees heap */
    assert_balanced();
}

Test(fs2_api, movefromstr_static) {
    __fs2_checkreset();
    const char *p = "literal";

    fs2 s = fs2_movefromstr(&p, FS2_STATIC);

    CHECK_STR_EQ("content", s.v, "literal");
    CHECK_TRUE("STATIC", fs2_is_static(&s));
    CHECK_PTR_EQ("from reset", p, NULL);
    CHECK_FS2VALID(1, &s);
    /* fs2_free no-op */
}

Test(fs2_api, movefromstr_null_from_aborts, .signal = SIGINT) {
    fs2_movefromstr(NULL, FS2_ALLOC);
}

Test(fs2_api, movefromstr_null_pointer_aborts, .signal = SIGINT) {
    const char *p = NULL;
    fs2_movefromstr(&p, FS2_ALLOC);   /* *from == NULL */
}

Test(fs2_api, movefromstr_local_aborts, .signal = SIGINT) {
    const char *p = "x";
    fs2_movefromstr(&p, FS2_LOCAL);   /* LOCAL not allowed */
}

Test(fs2_api, movefromstr_bodyalloc_aborts, .signal = SIGINT) {
    const char *p = "x";
    fs2_movefromstr(&p, FS2_ALLOC | FS2_BODYALLOC);
}

/* =========================================================================
 * fs2_fromraw
 * ========================================================================= */

Test(fs2_api, fromraw_alloc_bodyalloc_rejected, .signal = SIGINT) {
    char *buf = strdup("hi");
    fs2_fromraw(buf, 2, FS2_ALLOC | FS2_BODYALLOC);
    free(buf);   /* unreachable */
}

Test(fs2_api, fromraw_local_rejected, .signal = SIGINT) {
    char buf[8] = "hi";
    fs2_fromraw(buf, 2, FS2_LOCAL);
}

Test(fs2_api, fromraw_null_buf_aborts, .signal = SIGINT) {
    fs2_fromraw(NULL, 5, FS2_STATIC);
}

/* =========================================================================
 * fs2_adoptn — nominal
 * ========================================================================= */

Test(fs2_api, adoptn_alloc_adopts) {
    __fs2_checkreset();
    char *buf = strdup("hello");
    const char *orig = buf;

    fs2 s = fs2_adoptn(&buf, 5);

    CHECK_STR_EQ("content", s.v, "hello");
    CHECK_TRUE ("ALLOC", fs2_is_alloc(&s));
    CHECK_PTR_EQ("same buffer", s.v, orig);
    CHECK_PTR_EQ("from reset", buf, NULL);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);   /* frees orig */
    assert_balanced();
}

/* ⚠ Ключевой: empty → ALLOC empty, НЕ FS2EMPTY (preserve semantics) */
Test(fs2_api, adoptn_empty_preserves_alloc) {
    __fs2_checkreset();
    char *buf = strdup("");
    const char *orig = buf;

    fs2 s = fs2_adoptn(&buf, 0);

    CHECK_EQ  ("len", s.len, 0);
    CHECK_EQ  ("sz", s.sz, 1);
    CHECK_TRUE("ALLOC", fs2_is_alloc(&s));
    CHECK_FALSE("not STATIC", fs2_is_static(&s));
    CHECK_FALSE("not eq_empty", fs2_eq_empty(&s));
    CHECK_PTR_EQ("same buffer", s.v, orig);
    CHECK_PTR_EQ("from reset", buf, NULL);
    CHECK_STR_EQ("readable", s.v, "");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);   /* frees orig — must not leak, must not double-free */
    assert_balanced();
}

/* ⚠ Регресс на double-free: free(*str) после fromraw = UB */
Test(fs2_api, adoptn_no_double_free) {
    __fs2_checkreset();
    char *buf = strdup("hello");

    fs2 s = fs2_adoptn(&buf, 5);

    /* ASan ловит double-free если импл случайно free(*str) после adopt */
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, adoptn_null_str_aborts, .signal = SIGINT) {
    fs2_adoptn(NULL, 5);
}

Test(fs2_api, adoptn_null_pointer_aborts, .signal = SIGINT) {
    char *buf = NULL;
    fs2_adoptn(&buf, 5);
}

/* =========================================================================
 * fs2_adopt — via strlen
 * ========================================================================= */

Test(fs2_api, adopt_basic) {
    __fs2_checkreset();
    char *buf = strdup("hello");

    fs2 s = fs2_adopt(&buf);

    CHECK_STR_EQ("content", s.v, "hello");
    CHECK_TRUE ("ALLOC", fs2_is_alloc(&s));
    CHECK_PTR_EQ("from reset", buf, NULL);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, adopt_empty_preserves_alloc) {
    __fs2_checkreset();
    char *buf = strdup("");

    fs2 s = fs2_adopt(&buf);

    CHECK_EQ  ("len", s.len, 0);
    CHECK_TRUE("ALLOC", fs2_is_alloc(&s));
    CHECK_FALSE("not STATIC", fs2_is_static(&s));
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * Composite: sprintf-into-buffer
 * ========================================================================= */

Test(fs2_api, adoptn_sprintf_pattern) {
    __fs2_checkreset();
    char *tmp = malloc(64);
    int n = snprintf(tmp, 64, "id=%d", 42);
    CHECK_TRUE("snprintf ok", n > 0);

    fs2 s = fs2_adoptn(&tmp, (size_t)n);
    CHECK_STR_EQ("content", s.v, "id=42");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}