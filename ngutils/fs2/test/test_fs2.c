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

static inline void
test_cat(fs2 *dst, const fs2 *src) {
    fs2_reserve(dst, dst->len + src->len + 1);
    memcpy(dst->v + dst->len, src->v, src->len);
    fs2_setlen(dst, dst->len + src->len);
}
/**
 * @brief Test surrogate for fs2_catstr — appends src to dst using core primitives.
 *
 * fs2_catstr lives in fs2_api.h; this helper provides equivalent behavior
 * for core-level tests (test_fs2.c) that only include fs2.h.
 *
 * Uses fs2_reserve (core) which routes through prepare_growth → raises on
 * LOCAL overflow, non-empty STATIC, NULL. Same guards as the real fs2_catstr.
 *
 * Simplified: no overflow guard on dst->len + len + 1 (tests use small values).
 * NULL/empty src → no-op, consistent with fs2_catstr.
 */
static inline void
test_catstr(fs2 *dst, const char *src) {
    if (!src)
        return;
    size_t len = strlen(src);
    if (len == 0) return;
    fs2_reserve(dst, dst->len + len + 1);
    memcpy(dst->v + dst->len, src, len);
    fs2_setlen(dst, dst->len + len);
}
// static inline void
// test_cpystr(fs2 *dst, const char *src) {
//     fs2_setlen(dst, 0);
//     test_catstr(dst, src);
// }

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
    (void) fs2_init((size_t)1 << 62);
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
    fs2_setlen(&s, 11);

    fs2_setlen(&s, 5);

    CHECK_EQ("len", s.len, 5);
    CHECK_EQ("v[5] is NUL", s.v[5], '\0');
    CHECK_STR_EQ("v", s.v, "hello");
    fs2_free(&s);
}

Test(fs2, setlen_to_zero) {
    fs2 s = fs2_init(16);
    memcpy(s.v, "abc", 4);
    fs2_setlen(&s, 3);

    fs2_setlen(&s, 0);

    CHECK_EQ("len", s.len, 0);
    CHECK_STR_EQ("v", s.v, "");
    fs2_free(&s);
}

Test(fs2_api, setlen_nonzero_on_empty_aborts, .signal = SIGABRT) {
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
    s.len = s.sz;   /* len >= sz — forged invariant violation, direct assignment, can't use fs2_setlen here */
    CHECK_FALSE("validate with len >= sz", fs2_validate(NULL, &s));
}

Test(fs2, validate_len_gt_strlen) {
    fs2 s = FS2LITERAL("abc");
    s.len = 100;   // direct assignment, can't use fs2_setlen here */
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
 * fs2_techfprint
 * ------------------------------------------------------------------------- */

Test(fs2, techfprint_literal) {
    char path[256];
    test_tmpname(path, sizeof path, "test_fs2_tech", NULL, "txt");

    FILE *f = fopen(path, "w");
    CHECK_NOT_NULL("fopen write", f);

    fs2  s = FS2LITERAL("payload");
    long n = fs2_techfprint(f, &s, "s");
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

    long n = fs2_techfprint(f, NULL, "s");
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
    CHECK_EQ("fs2_techfprint(NULL, ...)", fs2_techfprint(NULL, &s, "s"), 0);
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
Test(fs2_api, fromstr_null_aborts, .signal = SIGABRT) {
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
        
        fs2 tmp = _fs2_wrap(inputs[i]);
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

Test(fs2_api, fromstrf_null_aborts, .signal = SIGABRT) {
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
    CHECK_STR_EQ("src v ''", src.v, "");
    CHECK_TRUE("src canon", fs2_eq_empty(&src));
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

    CHECK_TRUE("src canon", fs2_eq_empty(&src));
    CHECK_STR_EQ("src v ''", src.v, "");

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
    CHECK_TRUE("res canon", fs2_eq_empty(&res));
    CHECK_EQ  ("len", res.len, 0);
    CHECK_STR_EQ("v ''", res.v, "");
    CHECK_FS2VALID(1, &res);

    CHECK_TRUE("src canon", fs2_eq_empty(&src));

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
    CHECK_TRUE("src canon", fs2_eq_empty(&src));
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
    CHECK_TRUE ("h canon", fs2_eq_empty(h));
    CHECK_EQ   ("h len", h->len, 0);

    fs2_free(&res);   /* frees buffer */
    fs2_free(h);      /* frees struct */
    assert_balanced();
}

Test(fs2_api, movefrom_null_aborts, .signal = SIGABRT) {
    fs2_movefrom(NULL);
}

Test(fs2_api, movefrom_local_aborts, .signal = SIGABRT) {
    __fs2_checkreset();
    FS2LOCAL(s, 16);
    memcpy(s.v, "hi", 2);
    fs2_setlen(&s, 2);

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

    CHECK_TRUE("src canon empty", fs2_eq_empty(&src));
    CHECK_STR_EQ("src v ''", src.v, "");

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

    CHECK_TRUE("src canon", fs2_eq_empty(&src));

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

    CHECK_TRUE("src canon", fs2_eq_empty(&src));

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
    CHECK_TRUE ("src canon", fs2_eq_empty(&src));

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
    CHECK_TRUE ("src canon", fs2_eq_empty(&src));

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
    CHECK_TRUE ("src canon", fs2_eq_empty(src_heap));
    CHECK_EQ   ("src len", src_heap->len, 0);

    fs2_free(&dst);
    fs2_free(src_heap);   /* frees struct, v=NULL already */
    assert_balanced();
}

/* =========================================================================
 * fs2_move — NULL/LOCAL
 * ========================================================================= */

Test(fs2_api, move_null_dst_aborts, .signal = SIGABRT) {
    fs2 src = fs2_fromstr("x");
    fs2_move(NULL, &src);
    fs2_free(&src);
}

Test(fs2_api, move_null_src_aborts, .signal = SIGABRT) {
    fs2 dst = fs2_fromstr("x");
    fs2_move(&dst, NULL);
    fs2_free(&dst);
}

Test(fs2_api, move_local_dst_aborts, .signal = SIGABRT) {
    __fs2_checkreset();
    FS2LOCAL(dst, 16);
    fs2 src = fs2_fromstr("x");
    fs2_move(&dst, &src);   /* LOCAL dst forbidden */
    fs2_free(&src);
}

Test(fs2_api, move_local_src_aborts, .signal = SIGABRT) {
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
 * fs2_move
 * ========================================================================= */

Test(fs2_api, move_empty_src_dst_safe_readable) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("old");
    fs2 src = FS2();

    fs2_move(&dst, &src);

    /* dst = safe empty, not canonical */
    CHECK_TRUE ("dst eq_empty", fs2_eq_empty(&dst));
    CHECK_TRUE ("dst STATIC", fs2_is_static(&dst));
    CHECK_EQ   ("dst sz", dst.sz, 1);
    CHECK_NOT_NULL("dst v", dst.v);
    CHECK_STR_EQ("dst v readable", dst.v, "");   /* ← не крашится */
    CHECK_FS2VALID(1, &dst);
    CHECK_FS2VALID(2, &src);

    fs2_free(&dst);
    fs2_free(&src);
    assert_balanced();
}

Test(fs2_api, move_empty_src_dst_readable_via_printf) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("old");
    fs2 src = FS2();

    fs2_move(&dst, &src);

    /* Регресс: если dst.v == NULL → segfault */
    char buf[16];
    snprintf(buf, sizeof(buf), "%s", dst.v);
    CHECK_STR_EQ("printf ok", buf, "");

    fs2_free(&dst);
    fs2_free(&src);
    assert_balanced();
}

Test(fs2_api, move_empty_src_preserves_dst_bodyalloc) {
    __fs2_checkreset();
    fs2 *dst = fs2_heap_fromstr("old");   /* ALLOC|BODYALLOC */
    fs2  src = FS2();

    fs2_move(dst, &src);

    /* dst = FS2EMPTY | BODYALLOC */
    CHECK_TRUE ("dst eq_empty", fs2_eq_empty(dst));
    CHECK_TRUE ("dst BODYALLOC", fs2_is_bodyalloc(dst));
    CHECK_EQ   ("dst sz", dst->sz, 1);
    CHECK_NOT_NULL("dst v", dst->v);
    CHECK_STR_EQ("dst v", dst->v, "");
    CHECK_FS2VALID(1, dst);

    fs2_free(dst);   /* frees body only, v="" .rodata safe */
    fs2_free(&src);
    assert_balanced();
}

Test(fs2_api, move_empty_with_buffer_preserves_buffer) {
    __fs2_checkreset();
    fs2 dst = FS2();
    fs2 src = fs2_init(16);   /* ALLOC, buffer, len=0 */

    fs2_move(&dst, &src);

    /* dst takes the buffer (fs2_init empty but has buffer) */
    CHECK_TRUE ("dst ALLOC", fs2_is_alloc(&dst));
    CHECK_TRUE ("dst has buffer", dst.v != NULL);
    CHECK_GE   ("dst sz", dst.sz, 16);
    CHECK_EQ   ("dst len", dst.len, 0);
    CHECK_STR_EQ("dst v readable", dst.v, "");
    CHECK_FS2VALID(1, &dst);

    /* src reset to FS2EMPTY */
    CHECK_TRUE("src eq_empty", fs2_eq_empty(&src));

    fs2_free(&dst);
    fs2_free(&src);
    assert_balanced();
}

Test(fs2_api, move_symmetric_safe_after) {
    __fs2_checkreset();
    fs2 dst = FS2();
    fs2 src = FS2();

    fs2_move(&dst, &src);

    /* Обе стороны readable — не крашится */
    CHECK_STR_EQ("dst.v", dst.v, "");
    CHECK_STR_EQ("src.v", src.v, "");
    CHECK_FS2VALID(1, &dst);
    CHECK_FS2VALID(2, &src);

    fs2_free(&dst);
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
    CHECK_TRUE ("c canon", fs2_eq_empty(&c));
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

    CHECK_TRUE ("res empty", fs2_eq_empty(&res));
    CHECK_STR_EQ("v ''", res.v, "");
    CHECK_EQ  ("sz", res.sz, 1);
    CHECK_FS2VALID(1, &res);

    CHECK_TRUE ("src empty (reset)", fs2_eq_empty(&src));

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

    CHECK_TRUE("src canon (reset)", fs2_eq_empty(&src));
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
    CHECK_TRUE ("movefrom returns FS2()", fs2_eq_empty(&moved));
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
Test(fs2_api, fromraw_bodyalloc_rejected, .signal = SIGABRT) {
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

Test(fs2_api, movefromstr_null_from_aborts, .signal = SIGABRT) {
    fs2_movefromstr(NULL, FS2_ALLOC);
}

Test(fs2_api, movefromstr_null_pointer_aborts, .signal = SIGABRT) {
    const char *p = NULL;
    fs2_movefromstr(&p, FS2_ALLOC);   /* *from == NULL */
}

Test(fs2_api, movefromstr_local_aborts, .signal = SIGABRT) {
    const char *p = "x";
    fs2_movefromstr(&p, FS2_LOCAL);   /* LOCAL not allowed */
}

Test(fs2_api, movefromstr_bodyalloc_aborts, .signal = SIGABRT) {
    const char *p = "x";
    fs2_movefromstr(&p, FS2_ALLOC | FS2_BODYALLOC);
}

/* =========================================================================
 * fs2_fromraw
 * ========================================================================= */

Test(fs2_api, fromraw_alloc_bodyalloc_rejected, .signal = SIGABRT) {
    char *buf = strdup("hi");
    fs2_fromraw(buf, 2, FS2_ALLOC | FS2_BODYALLOC);
    free(buf);   /* unreachable */
}

Test(fs2_api, fromraw_local_rejected, .signal = SIGABRT) {
    char buf[8] = "hi";
    fs2_fromraw(buf, 2, FS2_LOCAL);
}

Test(fs2_api, fromraw_null_buf_aborts, .signal = SIGABRT) {
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

Test(fs2_api, adoptn_null_str_aborts, .signal = SIGABRT) {
    fs2_adoptn(NULL, 5);
}

Test(fs2_api, adoptn_null_pointer_aborts, .signal = SIGABRT) {
    char *buf = NULL;
    fs2_adoptn(&buf, 5);
}

/* =========================================================================
 * fs2_adopt — via strlen
 * ========================================================================= */

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

/* =========================================================================
 * fs2_adopt — strlen-based wrapper over adoptn
 * ========================================================================= */

Test(fs2_api, adopt_basic) {
    __fs2_checkreset();
    char *buf = strdup("hello");
    const char *orig = buf;

    fs2 s = fs2_adopt(&buf);

    CHECK_STR_EQ("content", s.v, "hello");
    CHECK_EQ  ("len", s.len, 5);
    CHECK_EQ  ("sz", s.sz, 6);
    CHECK_TRUE("ALLOC", fs2_is_alloc(&s));
    CHECK_PTR_EQ("same buffer", s.v, orig);
    CHECK_PTR_EQ("from reset", buf, NULL);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, adopt_roundtrip) {
    __fs2_checkreset();
    char *buf = strdup("id=42");
    fs2 s = fs2_adopt(&buf);

    CHECK_STR_EQ("roundtrip", fs2_cstr(&s), "id=42");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, adopt_null_str_aborts, .signal = SIGABRT) {
    fs2_adopt(NULL);
}

Test(fs2_api, adopt_null_pointer_aborts, .signal = SIGABRT) {
    char *buf = NULL;
    fs2_adopt(&buf);
}

/* =========================================================================
 * fs2_heap — basic
 * ========================================================================= */

Test(fs2_api, heap_returns_valid_empty) {
    __fs2_checkreset();
    fs2 *h = fs2_heap();

    CHECK_NOT_NULL("non-NULL", h);
    CHECK_EQ  ("len", h->len, 0);
    CHECK_EQ  ("sz", h->sz, 0);
    CHECK_PTR_EQ("v NULL", h->v, NULL);
    CHECK_FS2VALID(1, h);

    fs2_free(h);
    assert_balanced();
}

Test(fs2_api, heap_flags_alloc_and_bodyalloc) {
    __fs2_checkreset();
    fs2 *h = fs2_heap();

    CHECK_TRUE ("ALLOC", fs2_is_alloc(h));
    CHECK_TRUE ("BODYALLOC", fs2_is_bodyalloc(h));
    CHECK_FALSE("not STATIC", fs2_is_static(h));
    CHECK_FALSE("not LOCAL", fs2_is_local(h));

    fs2_free(h);
    assert_balanced();
}

Test(fs2_api, heap_is_canonical_empty) {
    __fs2_checkreset();
    fs2 *h = fs2_heap();

    /* Canonical: v=NULL, sz=0, len=0, ALLOC bit set */
    CHECK_TRUE("is_canon", fs2_is_canon(h));
    CHECK_FALSE("not eq_empty", fs2_eq_empty(h));

    fs2_free(h);
    assert_balanced();
}

/* =========================================================================
 * fs2_heap — independence (each call returns unique struct)
 * ========================================================================= */

Test(fs2_api, heap_independent_instances) {
    __fs2_checkreset();
    fs2 *a = fs2_heap();
    fs2 *b = fs2_heap();

    CHECK_TRUE("different structs", a != b);

    /* Мутация a не трогает b */
    fs2_reserve(a, 10);
    memcpy(a->v, "x", 2);
    fs2_setlen(a, 1);

    CHECK_STR_EQ("a content", a->v, "x");
    CHECK_EQ  ("b still empty", b->len, 0);
    CHECK_PTR_EQ("b v NULL", b->v, NULL);

    fs2_free(a);
    fs2_free(b);
    assert_balanced();
}

/* =========================================================================
 * fs2_heap — free balances counters
 * ========================================================================= */

Test(fs2_api, heap_free_balances_body_counter) {
    __fs2_checkreset();
    fs2 *h = fs2_heap();

    /* Первая проверка: bodyalloc=1, bodyfreed=0 — mismatch, out-params пишутся */
    {
        int a = -1, f = -1, ba = -1, bf = -1;
        fs2_checkleak(&a, &f, &ba, &bf);
        CHECK_EQ("body alloc", ba, 1);
        CHECK_EQ("body freed", bf, 0);
    }

    fs2_free(h);

    /* После free — всё balanced, fs2_checkleak вернёт true.
     * Второй вызов не пишет out-params (нет mismatch), поэтому проверяем
     * через assert_balanced (он сам вызывает fs2_checkleak и пишет log при leak). */
    assert_balanced();
}

Test(fs2_api, heap_no_leak_on_immediate_free) {
    __fs2_checkreset();

    for (int i = 0; i < 100; i++) {
        fs2 *h = fs2_heap();
        fs2_free(h);
    }

    assert_balanced();
}

/* =========================================================================
 * fs2_heap — grow after allocation
 * ========================================================================= */

Test(fs2_api, heap_grows_after_alloc) {
    __fs2_checkreset();
    fs2 *h = fs2_heap();

    fs2_reserve(h, 100);

    CHECK_TRUE ("still ALLOC", fs2_is_alloc(h));
    CHECK_TRUE ("still BODYALLOC", fs2_is_bodyalloc(h));
    CHECK_TRUE ("sz grown", h->sz >= 100);
    CHECK_NOT_NULL("v allocated", h->v);
    CHECK_EQ  ("len", h->len, 0);
    CHECK_EQ  ("v[0] NUL", h->v[0], '\0');   /* was_null → clear */
    CHECK_FS2VALID(1, h);

    fs2_free(h);
    assert_balanced();
}

/* =========================================================================
 * fs2_heap — composite: heap + fill + free
 * ========================================================================= */

Test(fs2_api, heap_fill_and_free) {
    __fs2_checkreset();
    fs2 *h = fs2_heap();

    fs2_reserve(h, 6);
    memcpy(h->v, "hello", 6);
    fs2_setlen(h, 5);

    CHECK_STR_EQ("content", h->v, "hello");
    CHECK_EQ  ("len", h->len, 5);
    CHECK_FS2VALID(1, h);

    fs2_free(h);   /* frees buffer AND struct */
    assert_balanced();
}

/* =========================================================================
 * fs2_heap — used by fs2_heap_* twins
 * ========================================================================= */

Test(fs2_api, heap_twins_use_heap_base) {
    __fs2_checkreset();
    
    /* Все fs2_heap_* проходят через fs2_heap() — проверим один */
    fs2 *h = fs2_heap_fromstr("hello");
    
    CHECK_TRUE("BODYALLOC", fs2_is_bodyalloc(h));
    CHECK_TRUE("ALLOC", fs2_is_alloc(h));
    CHECK_STR_EQ("content", h->v, "hello");
    CHECK_FS2VALID(1, h);
    
    fs2_free(h);
    assert_balanced();
}

/* =========================================================================
 * fs2_heap_init — basic
 * ========================================================================= */

Test(fs2_api, heap_init_basic) {
    __fs2_checkreset();
    fs2 *h = fs2_heap_init(100);

    CHECK_NOT_NULL("struct", h);
    CHECK_NOT_NULL("v allocated", h->v);
    CHECK_TRUE ("ALLOC", fs2_is_alloc(h));
    CHECK_TRUE ("BODYALLOC", fs2_is_bodyalloc(h));
    CHECK_FALSE("not STATIC", fs2_is_static(h));
    CHECK_FALSE("not LOCAL", fs2_is_local(h));
    CHECK_EQ  ("len", h->len, 0);
    CHECK_TRUE("sz >= 100", h->sz >= 100);
    CHECK_EQ  ("v[0] NUL", h->v[0], '\0');
    CHECK_FS2VALID(1, h);

    fs2_free(h);
    assert_balanced();
}

Test(fs2_api, heap_init_min_sz) {
    __fs2_checkreset();
    fs2 *h = fs2_heap_init(0);

    /* fs2_init(0) → reserve(1) → floor to FS2_MIN_ALLOC (16) */
    CHECK_NOT_NULL("v allocated", h->v);
    CHECK_TRUE ("sz >= 16 (min floor)", h->sz >= 16);
    CHECK_EQ  ("len", h->len, 0);
    CHECK_FS2VALID(1, h);

    fs2_free(h);
    assert_balanced();
}

Test(fs2_api, heap_init_pow2_rounding) {
    __fs2_checkreset();
    fs2 *h = fs2_heap_init(100);

    /* reserve(100) → calcnewsize(POWER2, 100) = 128 */
    CHECK_EQ("sz == 128", h->sz, 128);

    fs2_free(h);
    assert_balanced();
}

/* ⚠ Ключевой: счётчики не дисбалансируются (freestr(NULL) корректно no-op) */
Test(fs2_api, heap_init_balances_counters) {
    __fs2_checkreset();
    fs2 *h = fs2_heap_init(100);

    /* Если fs2_freestr(res) инкрементит g_free_cnt на NULL v — упадёт здесь */
    fs2_free(h);
    assert_balanced();
}

Test(fs2_api, heap_init_independent) {
    __fs2_checkreset();
    fs2 *a = fs2_heap_init(10);
    fs2 *b = fs2_heap_init(20);

    CHECK_TRUE("different structs", a != b);
    CHECK_TRUE("different buffers", a->v != b->v);

    /* Мутация a не трогает b */
    fs2_reserve(a, 100);
    memcpy(a->v, "x", 2);
    fs2_setlen(a, 1);

    CHECK_STR_EQ("a content", a->v, "x");
    CHECK_EQ  ("b still empty", b->len, 0);

    fs2_free(a);
    fs2_free(b);
    assert_balanced();
}

/* =========================================================================
 * fs2_heap_init — growth after construction
 * ========================================================================= */

Test(fs2_api, heap_init_grows_after_alloc) {
    __fs2_checkreset();
    fs2 *h = fs2_heap_init(10);
    size_t orig_sz = h->sz;

    fs2_reserve(h, 1000);

    CHECK_TRUE("grew", h->sz > orig_sz);
    CHECK_TRUE("BODYALLOC preserved", fs2_is_bodyalloc(h));
    CHECK_TRUE("ALLOC", fs2_is_alloc(h));
    CHECK_FS2VALID(1, h);

    fs2_free(h);
    assert_balanced();
}

/* =========================================================================
 * fs2_heap_init — no leak on repeated alloc/free
 * ========================================================================= */

Test(fs2_api, heap_init_no_leak_loop) {
    __fs2_checkreset();

    for (int i = 0; i < 100; i++) {
        fs2 *h = fs2_heap_init(64);
        fs2_free(h);
    }

    assert_balanced();
}

/* =========================================================================
 * Composite: heap_init + fill + free
 * ========================================================================= */

Test(fs2_api, heap_init_fill_and_free) {
    __fs2_checkreset();
    fs2 *h = fs2_heap_init(16);

    memcpy(h->v, "hello", 5);
    fs2_setlen(h, 5);

    CHECK_STR_EQ("content", h->v, "hello");
    CHECK_EQ  ("len", h->len, 5);
    CHECK_FS2VALID(1, h);

    fs2_free(h);
    assert_balanced();
}

/* =========================================================================
 * fs2_heap_clone — basic non-empty
 * ========================================================================= */

Test(fs2_api, heap_clone_alloc_src) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello");

    fs2 *h = fs2_heap_clone(&src);

    CHECK_STR_EQ("content", h->v, "hello");
    CHECK_EQ  ("len", h->len, 5);
    CHECK_TRUE ("ALLOC", fs2_is_alloc(h));
    CHECK_TRUE ("BODYALLOC", fs2_is_bodyalloc(h));
    CHECK_FALSE("not STATIC", fs2_is_static(h));
    CHECK_TRUE ("independent buffer", h->v != src.v);
    CHECK_FS2VALID(1, h);

    fs2_free(&src);
    fs2_free(h);
    assert_balanced();
}

Test(fs2_api, heap_clone_static_src) {
    __fs2_checkreset();
    fs2 src = FS2LITERAL("hello");

    fs2 *h = fs2_heap_clone(&src);

    CHECK_STR_EQ("content", h->v, "hello");
    CHECK_TRUE ("ALLOC", fs2_is_alloc(h));
    CHECK_FALSE("not STATIC", fs2_is_static(h));
    CHECK_TRUE ("independent buffer", h->v != src.v);
    CHECK_FS2VALID(1, h);

    fs2_free(h);
    assert_balanced();
}

/* =========================================================================
 * heap_clone — empty source → FS2EMPTY + BODYALLOC
 * ========================================================================= */

Test(fs2_api, heap_clone_fs2empty) {
    __fs2_checkreset();
    fs2 src = FS2EMPTY;

    fs2 *h = fs2_heap_clone(&src);

    /* clone(empty) → FS2EMPTY (STATIC); move wraps with BODYALLOC */
    CHECK_EQ  ("len", h->len, 0);
    CHECK_EQ  ("sz", h->sz, 1);
    CHECK_TRUE("STATIC axis", fs2_is_static(h));
    CHECK_TRUE("BODYALLOC", fs2_is_bodyalloc(h));
    CHECK_FALSE("not ALLOC-string", fs2_is_alloc(h));
    CHECK_STR_EQ("readable", h->v, "");
    CHECK_FS2VALID(1, h);

    /* fs2_free освободит ТОЛЬКО body (v — .rodata) */
    fs2_free(h);
    assert_balanced();
}

Test(fs2_api, heap_clone_canonical) {
    __fs2_checkreset();
    fs2 src = FS2();

    fs2 *h = fs2_heap_clone(&src);

    CHECK_EQ  ("len", h->len, 0);
    CHECK_EQ  ("sz", h->sz, 1);
    CHECK_TRUE("STATIC axis", fs2_is_static(h));   /* cloned via FS2EMPTY path */
    CHECK_TRUE("BODYALLOC", fs2_is_bodyalloc(h));
    CHECK_FS2VALID(1, h);

    fs2_free(&src);
    fs2_free(h);
    assert_balanced();
}

/* =========================================================================
 * heap_clone — independence
 * ========================================================================= */

Test(fs2_api, heap_clone_independent_from_src) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("original");
    fs2 *h = fs2_heap_clone(&src);

    /* Мутируем src */
    fs2_clear(&src);
    fs2_reserve(&src, 8);
    memcpy(src.v, "changed", 7);
    fs2_setlen(&src, 7);

    CHECK_STR_EQ("clone intact", h->v, "original");
    CHECK_STR_EQ("src changed", src.v, "changed");
    CHECK_FS2VALID(1, h);
    CHECK_FS2VALID(2, &src);

    fs2_free(&src);
    fs2_free(h);
    assert_balanced();
}

Test(fs2_api, heap_clone_mutate_clone_src_intact) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("original");
    fs2 *h = fs2_heap_clone(&src);

    /* Мутируем clone */
    fs2_clear(h);
    fs2_reserve(h, 8);
    memcpy(h->v, "modified", 8);
    fs2_setlen(h, 8);

    CHECK_STR_EQ("src intact", src.v, "original");
    CHECK_STR_EQ("clone modified", h->v, "modified");
    CHECK_FS2VALID(1, h);
    CHECK_FS2VALID(2, &src);

    fs2_free(&src);
    fs2_free(h);
    assert_balanced();
}

/* =========================================================================
 * heap_clone — counters / no leak
 * ========================================================================= */

Test(fs2_api, heap_clone_balances_counters) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello");

    fs2 *h = fs2_heap_clone(&src);
    fs2_free(h);
    fs2_free(&src);

    /* Ловит bug в fs2_freestr(NULL) внутри move: было бы alloc=2, freed=1 */
    assert_balanced();
}

Test(fs2_api, heap_clone_no_leak_loop) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("payload");

    for (int i = 0; i < 100; i++) {
        fs2 *h = fs2_heap_clone(&src);
        fs2_free(h);
    }

    fs2_free(&src);
    assert_balanced();
}

/* =========================================================================
 * heap_clone — growth after clone
 * ========================================================================= */

Test(fs2_api, heap_clone_grows_after_alloc) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hi");
    fs2 *h = fs2_heap_clone(&src);
    size_t orig_sz = h->sz;

    fs2_reserve(h, 1000);

    CHECK_TRUE("grew", h->sz > orig_sz);
    CHECK_TRUE("ALLOC", fs2_is_alloc(h));
    CHECK_TRUE("BODYALLOC preserved", fs2_is_bodyalloc(h));
    CHECK_STR_EQ("content intact", h->v, "hi");
    CHECK_FS2VALID(1, h);

    fs2_free(&src);
    fs2_free(h);
    assert_balanced();
}

/* =========================================================================
 * heap_clone — NULL
 * ========================================================================= */

Test(fs2_api, heap_clone_null_aborts, .signal = SIGABRT) {
    fs2_heap_clone(NULL);
}

/* =========================================================================
 * Composite: heap_clone + extend + free
 * ========================================================================= */

Test(fs2_api, heap_clone_extend_pipeline) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("prefix");

    fs2 *h = fs2_heap_clone(&src);

    /* Extend: reserve + memcpy + setlen (core-only, test in test_fs2.c) */
    const char *suffix     = "_suffix";
    size_t      suffix_len = strlen(suffix);
    fs2_reserve(h, h->len + suffix_len + 1);
    memcpy(h->v + h->len, suffix, suffix_len);
    fs2_setlen(h, h->len + suffix_len);

    CHECK_STR_EQ("extended", h->v, "prefix_suffix");
    CHECK_TRUE ("BODYALLOC preserved", fs2_is_bodyalloc(h));
    CHECK_FS2VALID(1, h);

    /* src не тронут */
    CHECK_STR_EQ("src intact", src.v, "prefix");

    fs2_free(&src);
    fs2_free(h);
    assert_balanced();
}

/* =========================================================================
 * heap_clone — consistency with stack clone
 * ========================================================================= */

Test(fs2_api, heap_clone_matches_stack_clone_content) {
    __fs2_checkreset();
    const char *inputs[] = { "hello", "", "long string with spaces" };
    size_t n = sizeof(inputs) / sizeof(inputs[0]);

    for (size_t i = 0; i < n; i++) {
        fs2 src = fs2_fromstr(inputs[i]);
        fs2 stack_clone = fs2_clone(&src);
        fs2 *heap_clone = fs2_heap_clone(&src);

        /* Content, len, string-axis flags — идентичны */
        CHECK_EQ  ("len", stack_clone.len, heap_clone->len);
        CHECK_STR_EQ("content", stack_clone.v, heap_clone->v);
        CHECK_EQ  ("string-axis flags match",
                   (stack_clone.flags & ~FS2_BODYALLOC),
                   (heap_clone->flags  & ~FS2_BODYALLOC));
        CHECK_TRUE("heap has BODYALLOC", fs2_is_bodyalloc(heap_clone));
        CHECK_FALSE("stack no BODYALLOC", fs2_is_bodyalloc(&stack_clone));
        CHECK_FS2VALID((int)i, &stack_clone);
        CHECK_FS2VALID((int)i, heap_clone);

        fs2_free(&src);
        fs2_free(&stack_clone);
        fs2_free(heap_clone);
    }
    assert_balanced();
}

/* =========================================================================
 * fs2_heap_fromstr — basic non-empty
 * ========================================================================= */

Test(fs2_api, heap_fromstr_basic) {
    __fs2_checkreset();
    fs2 *h = fs2_heap_fromstr("hello");

    CHECK_STR_EQ("content", h->v, "hello");
    CHECK_EQ  ("len", h->len, 5);
    CHECK_TRUE ("ALLOC", fs2_is_alloc(h));
    CHECK_TRUE ("BODYALLOC", fs2_is_bodyalloc(h));
    CHECK_FALSE("not STATIC", fs2_is_static(h));
    CHECK_TRUE ("sz >= 6", h->sz >= 6);
    CHECK_FS2VALID(1, h);

    fs2_free(h);
    assert_balanced();
}

Test(fs2_api, heap_fromstr_long) {
    __fs2_checkreset();
    char big[2001];
    memset(big, 'A', 2000);
    big[2000] = '\0';

    fs2 *h = fs2_heap_fromstr(big);

    CHECK_EQ("len", h->len, 2000);
    CHECK_EQ("first", h->v[0], 'A');
    CHECK_EQ("last", h->v[1999], 'A');
    CHECK_EQ("NUL", h->v[2000], '\0');
    CHECK_FS2VALID(1, h);

    fs2_free(h);
    assert_balanced();
}

Test(fs2_api, heap_fromstr_utf8) {
    __fs2_checkreset();
    /* "привет" = 12 байт UTF-8 */
    fs2 *h = fs2_heap_fromstr("\xD0\xBF\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82");

    CHECK_EQ("len", h->len, 12);
    CHECK_EQ("first byte", (unsigned char)h->v[0], 0xD0);
    CHECK_FS2VALID(1, h);

    fs2_free(h);
    assert_balanced();
}

/* =========================================================================
 * ⚠ Edge case: empty → STATIC|BODYALLOC, не ALLOC|BODYALLOC
 * ========================================================================= */

Test(fs2_api, heap_fromstr_empty_is_static_bodyalloc) {
    __fs2_checkreset();
    fs2 *h = fs2_heap_fromstr("");

    /* fromstr("") → FS2EMPTY (STATIC); move wraps into BODYALLOC */
    CHECK_EQ  ("len", h->len, 0);
    CHECK_EQ  ("sz", h->sz, 1);
    CHECK_TRUE("STATIC axis", fs2_is_static(h));
    CHECK_TRUE("BODYALLOC", fs2_is_bodyalloc(h));
    CHECK_FALSE("not ALLOC-string", fs2_is_alloc(h));
    CHECK_STR_EQ("readable", h->v, "");
    CHECK_FS2VALID(1, h);

    /* fs2_free освободит только body, v — .rodata */
    fs2_free(h);
    assert_balanced();
}

Test(fs2_api, heap_fromstr_empty_auto_promotes_on_grow) {
    __fs2_checkreset();
    fs2 *h = fs2_heap_fromstr("");   /* STATIC | BODYALLOC */

    fs2_reserve(h, 100);              /* auto-promote FS2EMPTY → ALLOC */

    CHECK_TRUE ("ALLOC", fs2_is_alloc(h));
    CHECK_TRUE ("BODYALLOC preserved", fs2_is_bodyalloc(h));
    CHECK_FALSE("no longer STATIC", fs2_is_static(h));
    CHECK_TRUE ("sz grown", h->sz >= 100);
    CHECK_EQ   ("len", h->len, 0);
    CHECK_EQ   ("v[0] NUL", h->v[0], '\0');
    CHECK_FS2VALID(1, h);

    fs2_free(h);
    assert_balanced();
}

/* =========================================================================
 * NULL (если fromstr(NULL) silent → FS2EMPTY, иначе invfatal)
 * ========================================================================= */

#if !defined(FS2_FROMSTR_STRICT)   /* fromstr NULL silent */
Test(fs2_api, heap_fromstr_null_returns_empty) {
    __fs2_checkreset();
    fs2 *h = fs2_heap_fromstr(NULL);

    CHECK_EQ  ("len", h->len, 0);
    CHECK_TRUE("STATIC axis", fs2_is_static(h));
    CHECK_TRUE("BODYALLOC", fs2_is_bodyalloc(h));
    CHECK_FS2VALID(1, h);

    fs2_free(h);
    assert_balanced();
}
#else
Test(fs2_api, heap_fromstr_null_aborts, .signal = SIGABRT) {
    fs2_heap_fromstr(NULL);
}
#endif

/* =========================================================================
 * Independence from source
 * ========================================================================= */

Test(fs2_api, heap_fromstr_independent_from_src) {
    __fs2_checkreset();
    char src[] = "hello";
    fs2 *h = fs2_heap_fromstr(src);

    src[0] = 'X';

    CHECK_STR_EQ("content intact", h->v, "hello");
    CHECK_TRUE ("different buffer", h->v != src);
    CHECK_FS2VALID(1, h);

    fs2_free(h);
    assert_balanced();
}

Test(fs2_api, heap_fromstr_independent_instances) {
    __fs2_checkreset();
    fs2 *a = fs2_heap_fromstr("hello");
    fs2 *b = fs2_heap_fromstr("world");

    CHECK_TRUE("different structs", a != b);
    CHECK_TRUE("different buffers", a->v != b->v);

    fs2_free(a);
    fs2_free(b);
    assert_balanced();
}

/* =========================================================================
 * Counters / no leak
 * ========================================================================= */

Test(fs2_api, heap_fromstr_balances_counters) {
    __fs2_checkreset();
    fs2 *h = fs2_heap_fromstr("hello");

    /* Ловит bug в fs2_freestr(NULL) внутри move */
    fs2_free(h);
    assert_balanced();
}

Test(fs2_api, heap_fromstr_no_leak_loop) {
    __fs2_checkreset();

    for (int i = 0; i < 100; i++) {
        fs2 *h = fs2_heap_fromstr("payload");
        fs2_free(h);
    }
    assert_balanced();
}

/* =========================================================================
 * Composite: heap_fromstr + extend + free
 * ========================================================================= */

Test(fs2_api, heap_fromstr_extend_pipeline) {
    __fs2_checkreset();
    fs2 *h = fs2_heap_fromstr("prefix");

    const char *suffix     = "_suffix";
    size_t      suffix_len = strlen(suffix);
    fs2_reserve(h, h->len + suffix_len + 1);
    memcpy(h->v + h->len, suffix, suffix_len);
    fs2_setlen(h, h->len + suffix_len);

    CHECK_STR_EQ("extended", h->v, "prefix_suffix");
    CHECK_TRUE ("BODYALLOC preserved", fs2_is_bodyalloc(h));
    CHECK_FS2VALID(1, h);

    fs2_free(h);
    assert_balanced();
}

/* =========================================================================
 * Consistency with stack fromstr
 * ========================================================================= */

Test(fs2_api, heap_fromstr_matches_stack_fromstr_content) {
    __fs2_checkreset();
    const char *inputs[] = { "hello", "", "long string with spaces" };
    size_t n = sizeof(inputs) / sizeof(inputs[0]);

    for (size_t i = 0; i < n; i++) {
        fs2 stack_s = fs2_fromstr(inputs[i]);
        fs2 *heap_s = fs2_heap_fromstr(inputs[i]);

        CHECK_EQ  ("len", stack_s.len, heap_s->len);
        CHECK_STR_EQ("content", stack_s.v, heap_s->v);
        CHECK_EQ  ("string-axis flags match",
                   (stack_s.flags & ~FS2_BODYALLOC),
                   (heap_s->flags & ~FS2_BODYALLOC));
        CHECK_TRUE ("heap BODYALLOC", fs2_is_bodyalloc(heap_s));
        CHECK_FALSE("stack no BODYALLOC", fs2_is_bodyalloc(&stack_s));
        CHECK_FS2VALID((int)i, &stack_s);
        CHECK_FS2VALID((int)i, heap_s);

        fs2_free(&stack_s);
        fs2_free(heap_s);
    }
    assert_balanced();
}

/* =========================================================================
 * prepare_growth — LOCAL rejection
 * ========================================================================= */

Test(fs2_api, reserve_local_within_capacity_noop) {
    __fs2_checkreset();
    FS2LOCAL(s, 16);
    memcpy(s.v, "hi", 2);
    fs2_setlen(&s, 2); 

    fs2_reserve(&s, 10);   /* 16 >= 10 → early return, no raise */

    CHECK_EQ("sz unchanged", s.sz, 16);
    CHECK_EQ("len unchanged", s.len, 2);
    CHECK_STR_EQ("content", s.v, "hi");
    CHECK_TRUE("still LOCAL", fs2_is_local(&s));
    CHECK_FS2VALID(1, &s);
}

/* ⚠ Главный: LOCAL growth → raise, не silent no-op */
Test(fs2_api, reserve_local_overflow_aborts, .signal = SIGABRT) {
    __fs2_checkreset();
    FS2LOCAL(s, 8);

    fs2_reserve(&s, 100);   /* LOCAL cannot grow → raise */
}

Test(fs2_api, reserve_local_empty_overflow_aborts, .signal = SIGABRT) {
    __fs2_checkreset();
    FS2LOCAL(s, 8);   /* len=0, sz=8 */

    fs2_reserve(&s, 100);   /* even empty LOCAL cannot grow */
}

/* =========================================================================
 * prepare_growth — STATIC (unchanged semantics)
 * ========================================================================= */

Test(fs2_api, reserve_static_nonempty_aborts, .signal = SIGABRT) {
    fs2 s = FS2LITERAL("hello");
    fs2_reserve(&s, 100);
}

Test(fs2_api, reserve_fs2empty_grows) {
    __fs2_checkreset();
    fs2 s = FS2EMPTY;

    fs2_reserve(&s, 100);

    CHECK_TRUE("promoted", fs2_is_alloc(&s));
    CHECK_TRUE("sz grown", s.sz >= 100);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, reserve_heap_empty_grows) {
    __fs2_checkreset();
    fs2 *h = fs2_heap_fromstr("");   /* STATIC|BODYALLOC */

    fs2_reserve(h, 100);

    CHECK_TRUE ("now ALLOC", fs2_is_alloc(h));
    CHECK_TRUE ("BODYALLOC preserved", fs2_is_bodyalloc(h));
    CHECK_TRUE ("sz grown", h->sz >= 100);
    CHECK_FS2VALID(1, h);

    fs2_free(h);
    assert_balanced();
}

/* =========================================================================
 * prepare_growth — ALLOC (unchanged)
 * ========================================================================= */

Test(fs2_api, reserve_alloc_grows) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hi");

    fs2_reserve(&s, 1000);

    CHECK_TRUE("sz grown", s.sz >= 1000);
    CHECK_STR_EQ("content intact", s.v, "hi");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, reserve_alloc_empty_preserves_buffer) {
    __fs2_checkreset();
    fs2 s = fs2_init(16);
    fs2_clear(&s);
    void *orig_v = s.v;

    fs2_reserve(&s, 10);   /* 16 >= 10, early return */

    CHECK_PTR_EQ("buffer preserved", s.v, orig_v);
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * Composite: catstr в LOCAL — capacity vs overflow
 * ========================================================================= */

Test(fs2_api, local_cat_within_capacity) {
    __fs2_checkreset();
    FS2LOCAL(s, 16);
    memcpy(s.v, "hi", 2);
    fs2_setlen(&s, 2);

    test_catstr(&s, "there");   /* "hithere\0" — 8 ≤ 16 */

    CHECK_STR_EQ("content", s.v, "hithere");
    CHECK_EQ("len", s.len, 7);
    CHECK_TRUE("still LOCAL", fs2_is_local(&s));
    CHECK_FS2VALID(1, &s);
}

Test(fs2_api, local_cat_overflow_aborts, .signal = SIGABRT) {
    __fs2_checkreset();
    FS2LOCAL(s, 8);
    memcpy(s.v, "hi", 2);
    fs2_setlen(&s, 2);

    test_catstr(&s, "and a long overflow string that exceeds capacity");
    /* → fs2_reserve_span → prepare_growth → raise (LOCAL cannot grow) */
}

Test(fs2_api, static_nonempty_cat_aborts, .signal = SIGABRT) {
    fs2 s = FS2LITERAL("hi");

    test_catstr(&s, "!");   /* → prepare_growth → raise (non-empty STATIC) */
}

Test(fs2_api, fs2empty_cat_promotes) {
    __fs2_checkreset();
    fs2 s = FS2EMPTY;

    test_catstr(&s, "hello");   /* FS2EMPTY → auto-promote → ALLOC */

    CHECK_STR_EQ("content", s.v, "hello");
    CHECK_TRUE ("promoted", fs2_is_alloc(&s));
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_setlen
 * ========================================================================= */

Test(fs2_api, setlen_zero_on_canonical_ok) {
    __fs2_checkreset();
    fs2 s = FS2();          /* v=NULL, sz=0, len=0 */
    fs2_setlen(&s, 0);      /* no-op, не raise */
    CHECK_EQ("len", s.len, 0);
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, setlen_zero_on_fs2empty_ok) {
    fs2 s = FS2EMPTY;       /* v="", sz=1, len=0, STATIC */
    fs2_setlen(&s, 0);      /* no-op */
    CHECK_EQ("len", s.len, 0);
    CHECK_FS2VALID(1, &s);
}

Test(fs2_api, setlen_zero_on_literal_nonempty_aborts, .signal = SIGABRT) {
    fs2 s = FS2LITERAL("hi");   /* len=2, STATIC */
    fs2_setlen(&s, 0);           /* нельзя отрезать .rodata → raise */
}

Test(fs2_api, setlen_same_len_on_literal_noop) {
    fs2 s = FS2LITERAL("hi");   /* len=2 */
    fs2_setlen(&s, 2);           /* pos == len → no-op */
    CHECK_EQ("len", s.len, 2);
    CHECK_FS2VALID(1, &s);
}

Test(fs2_api, setlen_nonzero_on_canonical_aborts, .signal = SIGABRT) {
    __fs2_checkreset();
    fs2 s = FS2();
    fs2_setlen(&s, 5);
    fs2_free(&s);
}

Test(fs2_api, setlen_alloc_valid_pos) {
    __fs2_checkreset();
    fs2 s = fs2_init(16);
    memcpy(s.v, "hello", 5);
    fs2_setlen(&s, 5);
    CHECK_STR_EQ("content", s.v, "hello");
    CHECK_FS2VALID(1, &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, setlen_alloc_exceeds_sz_aborts, .signal = SIGABRT) {
    __fs2_checkreset();
    fs2 s = fs2_init(16);
    fs2_setlen(&s, 100);   /* 100 >= sz=16 → raise */
    fs2_free(&s);
}

/* =========================================================================
 * fs2_heap_fromstrf — smoke tests (composition of already-tested pieces)
 * ========================================================================= */

Test(fs2_api, heap_fromstrf_basic) {
    __fs2_checkreset();
    fs2 *h = fs2_heap_fromstrf("id=%d", 42);

    CHECK_STR_EQ("content", h->v, "id=42");
    CHECK_EQ  ("len", h->len, 5);
    CHECK_TRUE ("ALLOC", fs2_is_alloc(h));
    CHECK_TRUE ("BODYALLOC", fs2_is_bodyalloc(h));
    CHECK_FALSE("not STATIC", fs2_is_static(h));
    CHECK_FS2VALID(1, h);

    fs2_free(h);
    assert_balanced();
}

/* ⚠ Edge case: что возвращает fromstrf_va("")?
 *   - ALLOC empty (текущая реализация) → heap_fromstrf = ALLOC|BODYALLOC
 *   - FS2EMPTY (после fix sz==0)       → heap_fromstrf = STATIC|BODYALLOC
 * Тест принимает оба варианта, фиксируя только «BODYALLOC preserved + empty». */
Test(fs2_api, heap_fromstrf_empty_result) {
    __fs2_checkreset();
    fs2 *h = fs2_heap_fromstrf("%s", "");

    CHECK_EQ  ("len", h->len, 0);
    CHECK_TRUE("BODYALLOC preserved", fs2_is_bodyalloc(h));
    CHECK_TRUE("readable", h->v != NULL);
    CHECK_STR_EQ("readable as empty", h->v, "");
    CHECK_FS2VALID(1, h);

    fs2_free(h);   /* освободит только body если STATIC, body+buf если ALLOC */
    assert_balanced();
}

Test(fs2_api, heap_fromstrf_null_fmt_aborts, .signal = SIGABRT) {
    fs2_heap_fromstrf(NULL);
}

Test(fs2_api, heap_fromstrf_balances_counters) {
    __fs2_checkreset();
    fs2 *h = fs2_heap_fromstrf("%s", "hello");

    /* Ловит counter-bug в fs2_freestr(NULL) внутри move */
    fs2_free(h);
    assert_balanced();
}

Test(fs2_api, heap_fromstrf_grows_after_alloc) {
    __fs2_checkreset();
    fs2 *h = fs2_heap_fromstrf("%d", 42);
    size_t orig_sz = h->sz;

    fs2_reserve(h, 1000);

    CHECK_TRUE("grew", h->sz > orig_sz);
    CHECK_TRUE("BODYALLOC preserved", fs2_is_bodyalloc(h));
    CHECK_STR_EQ("content intact", h->v, "42");
    CHECK_FS2VALID(1, h);

    fs2_free(h);
    assert_balanced();
}

Test(fs2_api, heap_fromstrf_independent_instances) {
    __fs2_checkreset();
    fs2 *a = fs2_heap_fromstrf("%d", 1);
    fs2 *b = fs2_heap_fromstrf("%d", 2);

    CHECK_TRUE("different structs", a != b);
    CHECK_TRUE("different buffers", a->v != b->v);

    fs2_free(a);
    fs2_free(b);
    assert_balanced();
}

/* Contract: heap_fromstrf == heap_fromstr + same content */
Test(fs2_api, heap_fromstrf_matches_heap_fromstr) {
    __fs2_checkreset();

    fs2 *a = fs2_heap_fromstrf("%s", "hello");   /* formatting path */
    fs2 *b = fs2_heap_fromstr("hello");          /* direct path */

    CHECK_STR_EQ("same content", a->v, b->v);
    CHECK_EQ  ("same len", a->len, b->len);
    CHECK_EQ  ("same string-axis flags",
               a->flags & ~FS2_BODYALLOC, b->flags & ~FS2_BODYALLOC);
    CHECK_TRUE("both BODYALLOC",
               fs2_is_bodyalloc(a) && fs2_is_bodyalloc(b));
    CHECK_FS2VALID(1, a);
    CHECK_FS2VALID(2, b);

    fs2_free(a);
    fs2_free(b);
    assert_balanced();
}

/* =========================================================================
 * fs2_heap_movefromstr
 * ========================================================================= */

Test(fs2_api, heap_movefromstr_alloc_nonempty) {
    __fs2_checkreset();
    const char *p = strdup("hello");

    fs2 *h = fs2_heap_movefromstr(&p, FS2_ALLOC);

    CHECK_STR_EQ("content", h->v, "hello");
    CHECK_TRUE ("ALLOC", fs2_is_alloc(h));
    CHECK_TRUE ("BODYALLOC", fs2_is_bodyalloc(h));
    CHECK_PTR_EQ("from reset", p, NULL);
    CHECK_FS2VALID(1, h);

    fs2_free(h);   /* frees v + struct */
    assert_balanced();
}

Test(fs2_api, heap_movefromstr_alloc_empty) {
    __fs2_checkreset();
    const char *p = strdup("");

    fs2 *h = fs2_heap_movefromstr(&p, FS2_ALLOC);

    CHECK_EQ  ("len", h->len, 0);
    CHECK_EQ  ("sz", h->sz, 1);
    CHECK_TRUE("ALLOC", fs2_is_alloc(h));
    CHECK_TRUE("BODYALLOC", fs2_is_bodyalloc(h));
    CHECK_PTR_EQ("from reset", p, NULL);
    CHECK_FS2VALID(1, h);

    fs2_free(h);
    assert_balanced();
}

Test(fs2_api, heap_movefromstr_static_empty) {
    __fs2_checkreset();
    const char *p = "";

    fs2 *h = fs2_heap_movefromstr(&p, FS2_STATIC);

    CHECK_EQ  ("len", h->len, 0);
    CHECK_EQ  ("sz", h->sz, 1);
    CHECK_TRUE("STATIC axis", fs2_is_static(h));
    CHECK_TRUE("BODYALLOC", fs2_is_bodyalloc(h));
    CHECK_FALSE("not ALLOC-string", fs2_is_alloc(h));
    CHECK_TRUE("matches eq_empty shape", fs2_eq_empty(h));
    CHECK_PTR_EQ("from reset", p, NULL);
    CHECK_FS2VALID(1, h);

    fs2_free(h);   /* frees только body */
    assert_balanced();
}

Test(fs2_api, heap_movefromstr_null_aborts, .signal = SIGABRT) {
    fs2_heap_movefromstr(NULL, FS2_ALLOC);
}

Test(fs2_api, heap_movefromstr_null_pointer_aborts, .signal = SIGABRT) {
    const char *p = NULL;
    fs2_heap_movefromstr(&p, FS2_ALLOC);
}

Test(fs2_api, heap_movefromstr_local_aborts, .signal = SIGABRT) {
    const char *p = "x";
    fs2_heap_movefromstr(&p, FS2_LOCAL);
}

Test(fs2_api, heap_movefromstr_balances_counters) {
    __fs2_checkreset();
    const char *p = strdup("payload");

    fs2 *h = fs2_heap_movefromstr(&p, FS2_ALLOC);
    fs2_free(h);
    assert_balanced();   /* ловит counter-bug в fs2_freestr(NULL) + утечки */
}

Test(fs2_api, heap_movefromstr_matches_stack_content) {
    __fs2_checkreset();

    const char *p1 = strdup("hello");
    const char *p2 = strdup("hello");

    fs2 *h = fs2_heap_movefromstr(&p1, FS2_ALLOC);
    fs2  s = fs2_movefromstr(&p2, FS2_ALLOC);

    CHECK_STR_EQ("same content", h->v, s.v);
    CHECK_EQ  ("same len", h->len, s.len);
    CHECK_EQ  ("same string-axis flags",
               h->flags & ~FS2_BODYALLOC, s.flags & ~FS2_BODYALLOC);
    CHECK_TRUE("heap has BODYALLOC", fs2_is_bodyalloc(h));
    CHECK_FALSE("stack no BODYALLOC", fs2_is_bodyalloc(&s));
    CHECK_FS2VALID(1, h);
    CHECK_FS2VALID(2, &s);

    fs2_free(h);
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_shrink — smoke tests (composition of already-tested pieces)
 * ========================================================================= */

Test(fs2_api, shrink_static_nonempty_noop) {
    fs2 s = FS2LITERAL("hello");   /* STATIC, sz=6, len=5 */
    void *orig = s.v;

    fs2_shrink(&s);   /* sz(6) <= target(6) → return, no realloc */

    CHECK_PTR_EQ("v unchanged", s.v, orig);
    CHECK_EQ  ("sz", s.sz, 6);
    CHECK_STR_EQ("content", s.v, "hello");
    CHECK_TRUE("still STATIC", fs2_is_static(&s));
    CHECK_FS2VALID(1, &s);
}

Test(fs2_api, shrink_local_nonempty_noop) {
    __fs2_checkreset();
    FS2LOCAL(s, 16);
    memcpy(s.v, "hi", 2);
    fs2_setlen(&s, 2);
    void *orig = s.v;

    fs2_shrink(&s);   /* sz(16) > target(3) → increasesize no-op (LOCAL) */

    CHECK_PTR_EQ("v unchanged", s.v, orig);
    CHECK_EQ  ("sz", s.sz, 16);
    CHECK_EQ  ("len", s.len, 2);
    CHECK_STR_EQ("content", s.v, "hi");
    CHECK_TRUE("still LOCAL", fs2_is_local(&s));
    CHECK_FS2VALID(1, &s);
}

/* ALLOC — фактически shrinks (unchanged) */
Test(fs2_api, shrink_alloc_grows_down) {
    __fs2_checkreset();
    fs2 s = fs2_init(100);
    memcpy(s.v, "hello", 5);
    fs2_setlen(&s, 5);
    CHECK_TRUE("pre sz > 6", s.sz > 6);

    fs2_shrink(&s);

    CHECK_EQ("sz == 16 (floor)", s.sz, 6);
    CHECK_STR_EQ("content", s.v, "hello");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, shrink_empty_noop) {
    __fs2_checkreset();
    fs2 canon = FS2();          /* sz=0, target=1 */
    fs2 empty = FS2EMPTY;       /* sz=1, target=1 */

    fs2_shrink(&canon);
    fs2_shrink(&empty);

    CHECK_EQ("canonical", canon.sz, 0);
    CHECK_EQ("fs2empty",  empty.sz, 1);
    CHECK_FS2VALID(1, &canon);
    CHECK_FS2VALID(2, &empty);

    fs2_free(&canon);
}

Test(fs2_api, shrink_null_aborts, .signal = SIGABRT) {
    fs2_shrink(NULL);
}

/* =========================================================================
 * fs2_promote_empty — canonical (v==NULL) → FS2EMPTY shape
 * ========================================================================= */

Test(fs2, promote_canonical_becomes_fs2empty) {
    fs2 s = FS2();   /* v=NULL, sz=0, len=0, ALLOC */

    fs2_promote_empty(&s);

    CHECK_EQ  ("len", s.len, 0);
    CHECK_EQ  ("sz", s.sz, 1);
    CHECK_TRUE("STATIC (FS2EMPTY)", fs2_is_static(&s));
    CHECK_FALSE("not ALLOC anymore", fs2_is_alloc(&s));
    CHECK_NOT_NULL("v readable", s.v);
    CHECK_STR_EQ("v empty string", s.v, "");
    CHECK_TRUE("eq_empty shape", fs2_eq_empty(&s));
    CHECK_FS2VALID(1, &s);
}

Test(fs2, promote_canonical_with_bodyalloc) {
    __fs2_checkreset();
    fs2 *h = fs2_heap();   /* v=NULL, ALLOC|BODYALLOC */

    fs2_promote_empty(h);

    CHECK_EQ  ("len", h->len, 0);
    CHECK_EQ  ("sz", h->sz, 1);
    CHECK_TRUE("STATIC", fs2_is_static(h));
    CHECK_FALSE("not ALLOC-string", fs2_is_alloc(h));
    CHECK_TRUE ("BODYALLOC preserved", fs2_is_bodyalloc(h));
    CHECK_STR_EQ("v readable", h->v, "");
    CHECK_FS2VALID(1, h);

    /* fs2_free освободит только body (v="" .rodata) */
    fs2_free(h);
    assert_balanced();
}

/* =========================================================================
 * fs2_promote_empty — no-op when v != NULL
 * ========================================================================= */

Test(fs2, promote_fs2empty_noop) {
    fs2 s = FS2EMPTY;   /* already FS2EMPTY shape */
    void *orig_v = s.v;
    size_t orig_sz = s.sz;

    fs2_promote_empty(&s);

    CHECK_PTR_EQ("v unchanged", s.v, orig_v);
    CHECK_EQ   ("sz unchanged", s.sz, orig_sz);
    CHECK_TRUE ("STATIC", fs2_is_static(&s));
    CHECK_FS2VALID(1, &s);
}

Test(fs2, promote_literal_nonempty_noop) {
    fs2 s = FS2LITERAL("hello");   /* v="hello" */
    void *orig_v = s.v;

    fs2_promote_empty(&s);

    CHECK_PTR_EQ("v unchanged", s.v, orig_v);
    CHECK_STR_EQ("content unchanged", s.v, "hello");
    CHECK_EQ   ("len unchanged", s.len, 5);
    CHECK_TRUE ("STATIC", fs2_is_static(&s));
    CHECK_FS2VALID(1, &s);
}

Test(fs2, promote_alloc_with_content_noop) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");
    void *orig_v = s.v;

    fs2_promote_empty(&s);

    CHECK_PTR_EQ("v unchanged", s.v, orig_v);
    CHECK_STR_EQ("content", s.v, "hello");
    CHECK_EQ   ("len", s.len, 5);
    CHECK_TRUE ("ALLOC preserved", fs2_is_alloc(&s));
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* ⚠ Ключевой: empty (len=0) но с buffer — no-op, buffer preserved */
Test(fs2, promote_alloc_empty_with_buffer_noop) {
    __fs2_checkreset();
    fs2 s = fs2_init(16);   /* v=malloc(16), sz=16, len=0, ALLOC */
    void *orig_v = s.v;

    fs2_promote_empty(&s);

    CHECK_PTR_EQ("buffer preserved", s.v, orig_v);
    CHECK_GE   ("sz unchanged", s.sz, 16);
    CHECK_EQ   ("len unchanged", s.len, 0);
    CHECK_TRUE ("ALLOC preserved", fs2_is_alloc(&s));
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, promote_local_noop) {
    __fs2_checkreset();
    FS2LOCAL(s, 16);
    void *orig_v = s.v;

    fs2_promote_empty(&s);

    CHECK_PTR_EQ("v unchanged", s.v, orig_v);
    CHECK_EQ   ("sz unchanged", s.sz, 16);
    CHECK_TRUE ("LOCAL preserved", fs2_is_local(&s));
    CHECK_FS2VALID(1, &s);
}

Test(fs2, promote_local_with_content_noop) {
    __fs2_checkreset();
    FS2LOCAL(s, 16);
    memcpy(s.v, "hi", 2);
    fs2_setlen(&s, 2);
    void *orig_v = s.v;

    fs2_promote_empty(&s);

    CHECK_PTR_EQ("v unchanged", s.v, orig_v);
    CHECK_STR_EQ("content", s.v, "hi");
    CHECK_TRUE ("LOCAL preserved", fs2_is_local(&s));
    CHECK_FS2VALID(1, &s);
}

/* =========================================================================
 * fs2_promote_empty — idempotency
 * ========================================================================= */

Test(fs2, promote_idempotent) {
    fs2 s = FS2();

    fs2_promote_empty(&s);
    void *v_after_first = s.v;
    size_t sz_after_first = s.sz;
    fs2_flags flags_after_first = s.flags;

    fs2_promote_empty(&s);   /* second call */

    CHECK_PTR_EQ("v unchanged by 2nd", s.v, v_after_first);
    CHECK_EQ   ("sz unchanged by 2nd", s.sz, sz_after_first);
    CHECK_EQ   ("flags unchanged by 2nd", s.flags, flags_after_first);
    CHECK_FS2VALID(1, &s);
}

/* =========================================================================
 * fs2_promote_empty — result is writable (via reserve auto-promote)
 * ========================================================================= */

Test(fs2, promote_result_grows_on_reserve) {
    __fs2_checkreset();
    fs2 s = FS2();

    fs2_promote_empty(&s);
    CHECK_TRUE("now STATIC empty", fs2_is_static(&s));

    /* Reserve auto-promotes back to ALLOC */
    fs2_reserve(&s, 100);

    CHECK_TRUE ("now ALLOC", fs2_is_alloc(&s));
    CHECK_FALSE("no longer STATIC", fs2_is_static(&s));
    CHECK_TRUE ("sz grown", s.sz >= 100);
    CHECK_EQ   ("len", s.len, 0);
    CHECK_NOT_NULL("v allocated", s.v);
    CHECK_EQ   ("v[0] NUL", s.v[0], '\0');
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, promote_result_printf_safe) {
    fs2 s = FS2();
    fs2_promote_empty(&s);

    /* Регресс: printf/strcmp на v не крашится (v="" not NULL) */
    char buf[16];
    snprintf(buf, sizeof(buf), "%s", s.v);
    CHECK_STR_EQ("printf", buf, "");
    CHECK_EQ("strlen", strlen(s.v), 0);
    CHECK_EQ("strcmp empty", strcmp(s.v, ""), 0);

    /* fs2_free no-op для STATIC */
}

/* =========================================================================
 * fs2_exch — basic exchange
 * ========================================================================= */

Test(fs2, exch_basic_alloc_alloc) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("AAA");
    fs2 b = fs2_fromstr("BBBB");
    void *a_v = a.v;
    void *b_v = b.v;

    fs2_exch(&a, &b);

    /* Content swapped */
    CHECK_STR_EQ("a has B's content", a.v, "BBBB");
    CHECK_STR_EQ("b has A's content", b.v, "AAA");
    CHECK_EQ("a len", a.len, 4);
    CHECK_EQ("b len", b.len, 3);

    /* Buffers swapped (не просто memcpy!) */
    CHECK_PTR_EQ("a got b's buffer", a.v, b_v);
    CHECK_PTR_EQ("b got a's buffer", b.v, a_v);

    CHECK_FS2VALID(1, &a);
    CHECK_FS2VALID(2, &b);

    fs2_free(&a);
    fs2_free(&b);
    assert_balanced();
}

/* ⚠ Твой главный запрос: a <-> b <-> a */
Test(fs2, exch_double_restores) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("AAA");
    fs2 b = fs2_fromstr("BBBB");
    void *a_v  = a.v;
    void *b_v  = b.v;
    size_t a_sz = a.sz;
    size_t b_sz = b.sz;

    fs2_exch(&a, &b);   /* a=B, b=A */
    fs2_exch(&a, &b);   /* a=A, b=B — restored */

    CHECK_STR_EQ("a original",  a.v, "AAA");
    CHECK_STR_EQ("b original",  b.v, "BBBB");
    CHECK_PTR_EQ("a v restored", a.v, a_v);
    CHECK_PTR_EQ("b v restored", b.v, b_v);
    CHECK_EQ("a sz restored", a.sz, a_sz);
    CHECK_EQ("b sz restored", b.sz, b_sz);
    CHECK_FS2VALID(1, &a);
    CHECK_FS2VALID(2, &b);

    fs2_free(&a);
    fs2_free(&b);
    assert_balanced();
}

Test(fs2, exch_triple_alternates) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("A");
    fs2 b = fs2_fromstr("B");

    fs2_exch(&a, &b);   /* a=B */
    CHECK_STR_EQ("swap 1: a=B", a.v, "B");
    CHECK_STR_EQ("swap 1: b=A", b.v, "A");

    fs2_exch(&a, &b);   /* a=A */
    CHECK_STR_EQ("swap 2: a=A", a.v, "A");
    CHECK_STR_EQ("swap 2: b=B", b.v, "B");

    fs2_exch(&a, &b);   /* a=B */
    CHECK_STR_EQ("swap 3: a=B", a.v, "B");
    CHECK_STR_EQ("swap 3: b=A", b.v, "A");

    fs2_free(&a);
    fs2_free(&b);
    assert_balanced();
}

Test(fs2, exch_same_content_still_swaps_buffers) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("XYZ");
    fs2 b = fs2_fromstr("XYZ");
    void *a_v = a.v;
    void *b_v = b.v;

    fs2_exch(&a, &b);

    /* Content "identical", но буферы должны реально поменяться */
    CHECK_PTR_EQ("a got b's buffer", a.v, b_v);
    CHECK_PTR_EQ("b got a's buffer", b.v, a_v);
    CHECK_STR_EQ("a content", a.v, "XYZ");
    CHECK_STR_EQ("b content", b.v, "XYZ");
    CHECK_FS2VALID(1, &a);
    CHECK_FS2VALID(2, &b);

    fs2_free(&a);
    fs2_free(&b);
    assert_balanced();
}

/* =========================================================================
 * fs2_exch — self / NULL / LOCAL
 * ========================================================================= */

Test(fs2, exch_self_noop) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("hello");
    void *orig_v = a.v;

    fs2_exch(&a, &a);

    CHECK_PTR_EQ("v unchanged", a.v, orig_v);
    CHECK_STR_EQ("content unchanged", a.v, "hello");
    CHECK_FS2VALID(1, &a);

    fs2_free(&a);
    assert_balanced();
}

Test(fs2, exch_null_a_aborts, .signal = SIGABRT) {
    fs2 b = fs2_fromstr("x");
    fs2_exch(NULL, &b);
    fs2_free(&b);
}

Test(fs2, exch_null_b_aborts, .signal = SIGABRT) {
    fs2 a = fs2_fromstr("x");
    fs2_exch(&a, NULL);
    fs2_free(&a);
}

Test(fs2, exch_local_a_aborts, .signal = SIGABRT) {
    __fs2_checkreset();
    FS2LOCAL(a, 16);
    fs2 b = fs2_fromstr("x");
    fs2_exch(&a, &b);
    fs2_free(&b);
}

Test(fs2, exch_local_b_aborts, .signal = SIGABRT) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("x");
    FS2LOCAL(b, 16);
    fs2_exch(&a, &b);
    fs2_free(&a);
}

/* =========================================================================
 * fs2_exch — mixed kinds (STATIC <-> ALLOC)
 * ========================================================================= */

Test(fs2, exch_static_alloc) {
    __fs2_checkreset();
    fs2 a = FS2LITERAL("AAA");      /* STATIC */
    fs2 b = fs2_fromstr("BBB");     /* ALLOC */
    void *b_v = b.v;

    fs2_exch(&a, &b);

    /* a: content+flags from b */
    CHECK_STR_EQ("a content", a.v, "BBB");
    CHECK_TRUE ("a ALLOC (from b)", fs2_is_alloc(&a));
    CHECK_FALSE("a not STATIC",    fs2_is_static(&a));
    CHECK_PTR_EQ("a got b's buffer", a.v, b_v);

    /* b: content+flags from a */
    CHECK_STR_EQ("b content", b.v, "AAA");
    CHECK_TRUE ("b STATIC (from a)", fs2_is_static(&b));
    CHECK_FALSE("b not ALLOC",       fs2_is_alloc(&b));

    CHECK_FS2VALID(1, &a);
    CHECK_FS2VALID(2, &b);

    fs2_free(&a);   /* frees b's old buffer */
    /* b is STATIC — no-op */

    assert_balanced();
}

/* =========================================================================
 * fs2_exch — BODYALLOC preservation per-side
 * ========================================================================= */

Test(fs2, exch_preserves_bodyalloc_per_side) {
    __fs2_checkreset();
    fs2 *a = fs2_heap_fromstr("AAA");   /* ALLOC|BODYALLOC */
    fs2  b = fs2_fromstr("BBB");         /* ALLOC, stack */

    fs2_exch(a, &b);

    CHECK_STR_EQ("a content", a->v, "BBB");
    CHECK_TRUE ("a BODYALLOC preserved", fs2_is_bodyalloc(a));
    CHECK_STR_EQ("b content", b.v, "AAA");
    CHECK_FALSE("b no bodyalloc", fs2_is_bodyalloc(&b));

    CHECK_FS2VALID(1, a);
    CHECK_FS2VALID(2, &b);

    fs2_free(a);    /* frees b's old buffer + a's struct */
    fs2_free(&b);   /* frees a's old buffer */

    assert_balanced();
}

Test(fs2, exch_both_bodyalloc) {
    __fs2_checkreset();
    fs2 *a = fs2_heap_fromstr("AAA");
    fs2 *b = fs2_heap_fromstr("BBB");

    fs2_exch(a, b);

    CHECK_STR_EQ("a content", a->v, "BBB");
    CHECK_TRUE ("a BODYALLOC", fs2_is_bodyalloc(a));
    CHECK_STR_EQ("b content", b->v, "AAA");
    CHECK_TRUE ("b BODYALLOC", fs2_is_bodyalloc(b));

    CHECK_FS2VALID(1, a);
    CHECK_FS2VALID(2, b);

    fs2_free(a);
    fs2_free(b);
    assert_balanced();
}

/* =========================================================================
 * fs2_exch — empty cases
 * ========================================================================= */

Test(fs2, exch_fs2empty_fs2empty) {
    fs2 a = FS2EMPTY;
    fs2 b = FS2EMPTY;

    fs2_exch(&a, &b);

    CHECK_TRUE("a eq_empty", fs2_eq_empty(&a));
    CHECK_TRUE("b eq_empty", fs2_eq_empty(&b));
    CHECK_FS2VALID(1, &a);
    CHECK_FS2VALID(2, &b);
}

Test(fs2, exch_canonical_with_content) {
    __fs2_checkreset();
    fs2 a = FS2();              /* canonical: v=NULL, ALLOC */
    fs2 b = fs2_fromstr("hello");

    fs2_exch(&a, &b);

    CHECK_STR_EQ("a has hello", a.v, "hello");
    CHECK_TRUE ("a ALLOC",     fs2_is_alloc(&a));

    CHECK_TRUE ("b empty",      fs2_is_empty(&b));
    CHECK_EQ   ("b len",        b.len, 0);
    /* Per "safe empty" protocol: v должен быть readable (not NULL).
     * Если этот CHECK падает — нужно добавить fs2_promote_empty(dst)
     * внутрь fs2_move. */
    CHECK_NOT_NULL("b v readable (safe empty)", b.v);

    CHECK_FS2VALID(1, &a);
    CHECK_FS2VALID(2, &b);

    fs2_free(&a);
    fs2_free(&b);
    assert_balanced();
}

Test(fs2, exch_empty_with_content) {
    __fs2_checkreset();
    fs2 a = FS2EMPTY;           /* STATIC-empty */
    fs2 b = fs2_fromstr("hello");

    fs2_exch(&a, &b);

    CHECK_STR_EQ("a content", a.v, "hello");
    CHECK_TRUE ("a ALLOC",   fs2_is_alloc(&a));
    CHECK_TRUE ("b empty",   fs2_is_empty(&b));
    CHECK_NOT_NULL("b v readable", b.v);

    CHECK_FS2VALID(1, &a);
    CHECK_FS2VALID(2, &b);

    fs2_free(&a);
    fs2_free(&b);
    assert_balanced();
}

/* =========================================================================
 * fs2_exch — no leak
 * ========================================================================= */

Test(fs2, exch_no_leak) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("AAA");
    fs2 b = fs2_fromstr("BBBBBBBB");

    fs2_exch(&a, &b);

    fs2_free(&a);
    fs2_free(&b);

    assert_balanced();
}

/* =========================================================================
 * fs2_heaptostack — basic
 * ========================================================================= */

Test(fs2, heaptostack_with_content) {
    __fs2_checkreset();
    fs2 *h = fs2_heap_fromstr("hello");
    CHECK_TRUE("heap has BODYALLOC", fs2_is_bodyalloc(h));

    fs2 s = fs2_heaptostack(&h);

    /* Caller's pointer is now NULL */
    CHECK_PTR_EQ("h NULL", h, NULL);

    /* Content preserved */
    CHECK_STR_EQ("content", s.v, "hello");
    CHECK_EQ   ("len", s.len, 5);
    CHECK_TRUE ("ALLOC", fs2_is_alloc(&s));
    CHECK_FALSE("no BODYALLOC", fs2_is_bodyalloc(&s));
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, heaptostack_empty) {
    __fs2_checkreset();
    fs2 *h = fs2_heap();   /* canonical: v=NULL, ALLOC|BODYALLOC */

    fs2 s = fs2_heaptostack(&h);

    CHECK_PTR_EQ("h NULL", h, NULL);
    CHECK_EQ   ("len", s.len, 0);
    CHECK_FALSE("no BODYALLOC", fs2_is_bodyalloc(&s));

    /* Per protocol: canonical → FS2EMPTY */
    CHECK_TRUE ("eq_empty shape", fs2_eq_empty(&s));
    CHECK_NOT_NULL("v readable", s.v);
    CHECK_STR_EQ("v readable", s.v, "");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, heaptostack_static_bodyalloc) {
    __fs2_checkreset();
    /* heap-обёртка над STATIC: heap_movefromstr(&"hi", STATIC) */
    const char *p = "hi";
    fs2 *h = fs2_heap_movefromstr(&p, FS2_STATIC);
    CHECK_TRUE("STATIC axis", fs2_is_static(h));
    CHECK_TRUE("BODYALLOC",   fs2_is_bodyalloc(h));

    fs2 s = fs2_heaptostack(&h);

    CHECK_PTR_EQ("h NULL", h, NULL);
    CHECK_STR_EQ("content", s.v, "hi");
    CHECK_TRUE ("STATIC axis", fs2_is_static(&s));
    CHECK_FALSE("no BODYALLOC", fs2_is_bodyalloc(&s));
    CHECK_FS2VALID(1, &s);
}

/* =========================================================================
 * fs2_heaptostack — guards
 * ========================================================================= */

Test(fs2, heaptostack_null_psrc_aborts, .signal = SIGABRT) {
    fs2_heaptostack(NULL);
}

Test(fs2, heaptostack_null_inner_aborts, .signal = SIGABRT) {
    fs2 *h = NULL;
    fs2_heaptostack(&h);
}

/* ⚠ Non-BODYALLOC (stack fs2*) — только pointer на stack */
Test(fs2, heaptostack_stack_fs2_aborts, .signal = SIGABRT) {
    __fs2_checkreset();
    fs2 *h = malloc(sizeof(fs2));   /* struct on heap, но без BODYALLOC flag */
    CHECK_NOT_NULL("alloc", h);
    *h = fs2_fromstr("hi");          /* ALLOC, no BODYALLOC bit */

    fs2_heaptostack(&h);   /* ← invfatal BODYALLOC missing → abort */

    /* unreachable */
    fs2_free(h);
    free(h);
}

/* ⚠ LOCAL|BODYALLOC (forged) — pass 1st guard, fail in fs2_move */
Test(fs2, heaptostack_local_bodyalloc_aborts, .signal = SIGABRT) {
    __fs2_checkreset();
    fs2 *h = fs2_heap();          /* BODYALLOC */
    /* forge LOCAL: replace ALLOC with LOCAL */
    h->flags = (h->flags & ~FS2_ALLOC) | FS2_LOCAL;

    fs2_heaptostack(&h);   /* ← fs2_move sees LOCAL → abort */

    /* unreachable */
}

/* =========================================================================
 * fs2_heaptostack — counter balance
 * ========================================================================= */

Test(fs2, heaptostack_balances_counters) {
    __fs2_checkreset();
    fs2 *h = fs2_heap_fromstr("hello");

    fs2 s = fs2_heaptostack(&h);

    /* h struct freed, but buffer now owned by s (as stack fs2) */
    fs2_free(&s);

    assert_balanced();   /* ← ловит leak/double-free в move/freebody */
}

Test(fs2, heaptostack_caller_pointer_nullable) {
    __fs2_checkreset();
    fs2 *h = fs2_heap_fromstr("x");

    fs2 s = fs2_heaptostack(&h);

    /* fs2_free(NULL) должен быть no-op */
    fs2_free(h);         /* h == NULL → safe */
    fs2_free(&s);        /* frees content buffer */

    assert_balanced();
}

/* =========================================================================
 * fs2_heaptostack — roundtrip via stacktoheap
 * ========================================================================= */

Test(fs2, heaptostack_roundtrip_stacktoheap) {
    __fs2_checkreset();
    fs2 *original = fs2_heap_fromstr("payload");

    fs2 stack = fs2_heaptostack(&original);
    CHECK_PTR_EQ("original NULL", original, NULL);

    fs2 *again = fs2_stacktoheap(&stack);

    CHECK_STR_EQ("roundtrip content", again->v, "payload");
    CHECK_TRUE ("BODYALLOC restored", fs2_is_bodyalloc(again));
    CHECK_TRUE ("ALLOC",              fs2_is_alloc(again));
    CHECK_FS2VALID(1, again);

    /* stack after stacktoheap: reset to FS2EMPTY */
    CHECK_TRUE("stack empty", fs2_is_empty(&stack));
    CHECK_NOT_NULL("stack v readable", stack.v);

    fs2_free(again);
    fs2_free(&stack);
    assert_balanced();
}

/* =========================================================================
 * fs2_heaptostack — independent from any subsequent heap ops
 * ========================================================================= */

Test(fs2, heaptostack_result_independent) {
    __fs2_checkreset();
    fs2 *h = fs2_heap_fromstr("original");

    fs2 s = fs2_heaptostack(&h);

    /* Результат — independent value */
    fs2_clear(&s);
    memcpy(s.v, "modified", 8);
    fs2_setlen(&s, 8);

    CHECK_STR_EQ("content", s.v, "modified");
    CHECK_FS2VALID(1, &s);

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_stacktoheap — basic
 * ========================================================================= */

Test(fs2, stacktoheap_alloc) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello");
    void *src_v = src.v;

    fs2 *h = fs2_stacktoheap(&src);

    /* heap has content + BODYALLOC */
    CHECK_STR_EQ("content", h->v, "hello");
    CHECK_TRUE ("ALLOC",     fs2_is_alloc(h));
    CHECK_TRUE ("BODYALLOC", fs2_is_bodyalloc(h));
    CHECK_PTR_EQ("same buffer", h->v, src_v);   /* no copy — move */
    CHECK_FS2VALID(1, h);

    /* src reset to FS2EMPTY (safe empty) */
    CHECK_TRUE ("src empty", fs2_is_empty(&src));
    CHECK_NOT_NULL("src v readable", src.v);
    CHECK_FALSE("src no BODYALLOC", fs2_is_bodyalloc(&src));
    CHECK_FS2VALID(2, &src);

    fs2_free(h);
    fs2_free(&src);
    assert_balanced();
}

Test(fs2, stacktoheap_empty) {
    __fs2_checkreset();
    fs2 src = FS2();

    fs2 *h = fs2_stacktoheap(&src);

    CHECK_EQ  ("len", h->len, 0);
    CHECK_TRUE("BODYALLOC", fs2_is_bodyalloc(h));
    /* Per protocol: canonical → FS2EMPTY-shape */
    CHECK_TRUE("eq_empty shape", fs2_eq_empty(h));
    CHECK_NOT_NULL("v readable", h->v);
    CHECK_FS2VALID(1, h);

    CHECK_TRUE("src empty", fs2_is_empty(&src));

    fs2_free(h);
    fs2_free(&src);
    assert_balanced();
}

Test(fs2, stacktoheap_static_axis) {
    __fs2_checkreset();
    fs2 src = FS2LITERAL("hi");   /* STATIC */

    fs2 *h = fs2_stacktoheap(&src);

    /* h inherits STATIC axis from src + BODYALLOC */
    CHECK_STR_EQ("content", h->v, "hi");
    CHECK_TRUE ("STATIC axis", fs2_is_static(h));
    CHECK_TRUE ("BODYALLOC",   fs2_is_bodyalloc(h));
    CHECK_FALSE("not ALLOC-string", fs2_is_alloc(h));
    CHECK_FS2VALID(1, h);

    /* src reset to FS2EMPTY */
    CHECK_TRUE("src empty", fs2_is_empty(&src));

    /* fs2_free освободит только body (v=.rodata) */
    fs2_free(h);
    fs2_free(&src);
    assert_balanced();
}

/* =========================================================================
 * fs2_stacktoheap — guards
 * ========================================================================= */

Test(fs2, stacktoheap_null_aborts, .signal = SIGABRT) {
    fs2_stacktoheap(NULL);
}

/* ⚠ Главный: BODYALLOC src → abort (иначе alias + double-free) */
Test(fs2, stacktoheap_bodyalloc_src_aborts, .signal = SIGABRT) {
    __fs2_checkreset();
    fs2 *h = fs2_heap_fromstr("hi");   /* BODYALLOC */

    fs2_stacktoheap(h);   /* ← abort: would alias */

    /* unreachable */
    fs2_free(h);
}

/* LOCAL src → abort (via fs2_move internal guard) */
Test(fs2, stacktoheap_local_src_aborts, .signal = SIGABRT) {
    __fs2_checkreset();
    FS2LOCAL(src, 16);
    memcpy(src.v, "hi", 2);
    fs2_setlen(&src, 2);

    fs2_stacktoheap(&src);   /* ← abort in fs2_move */
}

/* =========================================================================
 * fs2_stacktoheap — roundtrip
 * ========================================================================= */

Test(fs2, stacktoheap_heaptostack_roundtrip) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("payload");

    fs2 *h = fs2_stacktoheap(&src);
    CHECK_TRUE("src empty", fs2_is_empty(&src));

    fs2 back = fs2_heaptostack(&h);
    CHECK_PTR_EQ("h NULL after heaptostack", h, NULL);

    CHECK_STR_EQ("roundtrip content", back.v, "payload");
    CHECK_FALSE("back no BODYALLOC", fs2_is_bodyalloc(&back));
    CHECK_TRUE ("back ALLOC",        fs2_is_alloc(&back));
    CHECK_FS2VALID(1, &back);

    fs2_free(&src);
    fs2_free(&back);
    assert_balanced();
}

/* =========================================================================
 * fs2_stacktoheap — counter balance
 * ========================================================================= */

Test(fs2, stacktoheap_balances_counters) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("hello");

    fs2 *h = fs2_stacktoheap(&src);
    fs2_free(h);
    fs2_free(&src);

    assert_balanced();
}

/* =========================================================================
 * fs2_stacktoheap — independence after move
 * ========================================================================= */

Test(fs2, stacktoheap_result_independent) {
    __fs2_checkreset();
    fs2 src = fs2_fromstr("original");

    fs2 *h = fs2_stacktoheap(&src);

    /* Мутируем h — src уже пуст, не задет */
    fs2_clear(h);
    fs2_reserve(h, 16);
    memcpy(h->v, "modified", 8);
    fs2_setlen(h, 8);

    CHECK_STR_EQ("h content", h->v, "modified");
    CHECK_TRUE ("src still empty", fs2_is_empty(&src));
    CHECK_FS2VALID(1, h);
    CHECK_FS2VALID(2, &src);

    fs2_free(h);
    fs2_free(&src);
    assert_balanced();
}

/* =========================================================================
 * fs2_free — exhaustive kind matrix
 * ========================================================================= */

Test(fs2, free_all_kinds_balance) {
    __fs2_checkreset();
    
    fs2 a = FS2();                              /* canonical */
    fs2 b = fs2_fromstr("hi");                  /* ALLOC */
    fs2 c = fs2_init(16);                       /* ALLOC empty+buffer */
    fs2 *d = fs2_heap();                        /* ALLOC|BODYALLOC */
    fs2 *e = fs2_heap_fromstr("hi");            /* ALLOC|BODYALLOC */
    
    fs2_free(&a);
    fs2_free(&b);
    fs2_free(&c);
    fs2_free(d);
    fs2_free(e);
    
    /* STATIC/LOCAL — no-op, no counters */
    fs2 f = FS2EMPTY;
    fs2 g = FS2LITERAL("hi");
    FS2LOCAL(h, 16);
    fs2_free(&f);
    fs2_free(&g);
    fs2_free(&h);
    
    assert_balanced();
}

/* ⚠ Idempotent только для non-BODYALLOC */
Test(fs2, free_idempotent_nonbodyalloc) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hi");
    
    fs2_free(&s);   /* v freed */
    CHECK_PTR_EQ("v NULL", s.v, NULL);
    CHECK_EQ("sz", s.sz, 0);
    CHECK_EQ("len", s.len, 0);
    /* flags остаются ALLOC, но v=NULL → freestr no-op */
    
    fs2_free(&s);   /* второй раз — no-op */
    assert_balanced();
}

/* STATIC non-empty — free no-op, content intact */
Test(fs2, free_static_nonempty_preserves_content) {
    __fs2_checkreset();
    fs2 s = FS2LITERAL("hi");
    void *orig = s.v;
    
    fs2_free(&s);
    
    CHECK_PTR_EQ("v unchanged", s.v, orig);
    CHECK_STR_EQ("content", s.v, "hi");
    CHECK_TRUE("STATIC", fs2_is_static(&s));
    CHECK_FS2VALID(1, &s);
}

/* LOCAL — free no-op, buffer intact */
Test(fs2, free_local_preserves_buffer) {
    __fs2_checkreset();
    FS2LOCAL(s, 16);
    memcpy(s.v, "hi", 2);
    fs2_setlen(&s, 2);
    void *orig = s.v;
    
    fs2_free(&s);
    
    CHECK_PTR_EQ("v unchanged", s.v, orig);
    CHECK_STR_EQ("content", s.v, "hi");
    CHECK_TRUE("LOCAL", fs2_is_local(&s));
    CHECK_FS2VALID(1, &s);
}

/* ALLOC empty+buffer — buffer freed */
Test(fs2, free_alloc_with_buffer) {
    __fs2_checkreset();
    fs2 s = fs2_init(16);   /* ALLOC, buffer, len=0 */
    
    fs2_free(&s);
    
    CHECK_PTR_EQ("v NULL", s.v, NULL);
    CHECK_EQ("sz", s.sz, 0);
    assert_balanced();
}

/* =========================================================================
 * fs2_cant_mutate — invariant guard
 *
 * Contract:
 *   NULL              → SIGABRT (contract violation)
 *   empty (any kind)  → true (skip, nothing to mutate)
 *   non-empty STATIC  → SIGABRT (cannot write .rodata)
 *   non-empty ALLOC   → false (proceed)
 *   non-empty LOCAL   → false (proceed)
 * ========================================================================= */

Test(fs2, cant_mutate_null_aborts, .signal = SIGABRT) {
    fs2_cant_mutate(NULL);
}

Test(fs2, cant_mutate_canonical_empty_skips) {
    fs2 s = FS2();   /* v=NULL, len=0, ALLOC */
    CHECK_TRUE("skip", fs2_cant_mutate(&s));
}

Test(fs2, cant_mutate_fs2empty_skips) {
    fs2 s = FS2EMPTY;
    CHECK_TRUE("skip", fs2_cant_mutate(&s));
}

Test(fs2, cant_mutate_alloc_empty_skips) {
    __fs2_checkreset();
    fs2 s = fs2_init(16);   /* v=malloc, len=0, ALLOC */
    CHECK_TRUE("skip", fs2_cant_mutate(&s));
    fs2_free(&s);
    assert_balanced();
}

Test(fs2, cant_mutate_local_empty_skips) {
    __fs2_checkreset();
    FS2LOCAL(s, 16);
    CHECK_TRUE("skip", fs2_cant_mutate(&s));
}

Test(fs2, cant_mutate_literal_empty_skips) {
    fs2 s = FS2LITERAL("");
    CHECK_TRUE("skip", fs2_cant_mutate(&s));
}

/* Non-empty STATIC → abort */
Test(fs2, cant_mutate_static_nonempty_aborts, .signal = SIGABRT) {
    fs2 s = FS2LITERAL("hello");
    fs2_cant_mutate(&s);
}

/* Non-empty ALLOC/LOCAL → false (proceed) */
Test(fs2, cant_mutate_alloc_with_content_proceeds) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");
    CHECK_FALSE("proceed", fs2_cant_mutate(&s));
    fs2_free(&s);
    assert_balanced();
}

Test(fs2, cant_mutate_local_with_content_proceeds) {
    __fs2_checkreset();
    FS2LOCAL(s, 16);
    memcpy(s.v, "hi", 2);
    fs2_setlen(&s, 2);
    CHECK_FALSE("proceed", fs2_cant_mutate(&s));
}

/* =========================================================================
 * _fs2_wrap — tolerant NVL-style wrap
 * ========================================================================= */

Test(fs2, wrap_basic) {
    fs2 s = _fs2_wrap("hello");

    CHECK_STR_EQ("content", s.v, "hello");
    CHECK_EQ  ("len", s.len, 5);
    CHECK_EQ  ("sz",  s.sz, 6);
    CHECK_TRUE ("STATIC", fs2_is_static(&s));
    CHECK_FALSE("not ALLOC", fs2_is_alloc(&s));
    CHECK_FALSE("not LOCAL", fs2_is_local(&s));
    CHECK_FS2VALID(1, &s);
    /* fs2_free — no-op для STATIC */
}

Test(fs2, wrap_aliases_input) {
    /* v must point INTO the input, not a copy */
    const char *src = "hello";
    fs2 s = _fs2_wrap(src);
    CHECK_PTR_EQ("aliases src", s.v, src);
}

Test(fs2, wrap_empty_string) {
    fs2 s = _fs2_wrap("");

    CHECK_EQ  ("len", s.len, 0);
    CHECK_EQ  ("sz",  s.sz, 1);
    CHECK_TRUE("STATIC", fs2_is_static(&s));
    CHECK_STR_EQ("readable", s.v, "");
    CHECK_FS2VALID(1, &s);
}

Test(fs2, wrap_null_returns_empty_const_shape) {
    fs2 s = _fs2_wrap(NULL);

    CHECK_EQ  ("len", s.len, 0);
    CHECK_EQ  ("sz",  s.sz, 1);
    CHECK_TRUE("STATIC", fs2_is_static(&s));
    CHECK_FALSE("not ALLOC", fs2_is_alloc(&s));
    CHECK_NOT_NULL("v readable", s.v);
    CHECK_STR_EQ("v empty", s.v, "");
    CHECK_TRUE("eq_empty shape", fs2_eq_empty(&s));
    CHECK_FS2VALID(1, &s);
}

Test(fs2, wrap_no_allocations) {
    __fs2_checkreset();
    fs2 s = _fs2_wrap("hello");   /* must NOT touch counters */
    fs2_free(&s);
    assert_balanced();           /* 0 alloc, 0 free */
}

Test(fs2, wrap_long_string) {
    char big[1001];
    memset(big, 'A', 1000);
    big[1000] = '\0';

    fs2 s = _fs2_wrap(big);
    CHECK_EQ("len", s.len, 1000);
    CHECK_EQ("sz",  s.sz, 1001);
    CHECK_EQ("first", s.v[0], 'A');
    CHECK_PTR_EQ("aliases buf", s.v, big);
    CHECK_FS2VALID(1, &s);
}

Test(fs2, wrap_non_ascii) {
    /* UTF-8 "привет" = 12 bytes */
    const char *src = "\xD0\xBF\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82";
    fs2 s = _fs2_wrap(src);
    CHECK_EQ("len", s.len, 12);
    CHECK_EQ("first byte", (unsigned char)s.v[0], 0xD0);
    CHECK_PTR_EQ("aliases", s.v, src);
    CHECK_FS2VALID(1, &s);
}

/* ══════════════════════════════════════════════════════════════════════════
 * _fs2_wrap_notnull — strict variant, NULL → SIGABRT
 * ══════════════════════════════════════════════════════════════════════════ */

Test(fs2, wrap_notnull_basic) {
    fs2 s = _fs2_wrap_notnull("hello");

    CHECK_STR_EQ("content", s.v, "hello");
    CHECK_EQ  ("len", s.len, 5);
    CHECK_EQ  ("sz",  s.sz, 6);
    CHECK_TRUE("STATIC", fs2_is_static(&s));
    CHECK_FS2VALID(1, &s);
}

Test(fs2, wrap_notnull_aliases_input) {
    const char *src = "hello";
    fs2 s = _fs2_wrap_notnull(src);
    CHECK_PTR_EQ("aliases src", s.v, src);
}

Test(fs2, wrap_notnull_empty_string) {
    /* "" is valid (non-NULL), just empty */
    fs2 s = _fs2_wrap_notnull("");

    CHECK_EQ("len", s.len, 0);
    CHECK_EQ("sz",  s.sz, 1);
    CHECK_TRUE("STATIC", fs2_is_static(&s));
    CHECK_STR_EQ("readable", s.v, "");
    CHECK_FS2VALID(1, &s);
}

Test(fs2, wrap_notnull_null_aborts, .signal = SIGABRT) {
    _fs2_wrap_notnull(NULL);
}

Test(fs2, wrap_notnull_no_allocations) {
    __fs2_checkreset();
    fs2 s = _fs2_wrap_notnull("x");
    fs2_free(&s);
    assert_balanced();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Consistency: wrap vs wrap_notnull (same shape for non-NULL)
 * ══════════════════════════════════════════════════════════════════════════ */

Test(fs2, wrap_vs_notnull_same_for_nonnull) {
    const char *inputs[] = { "hello", "", "long string here", "x" };
    size_t n = sizeof(inputs) / sizeof(inputs[0]);

    for (size_t i = 0; i < n; i++) {
        fs2 a = _fs2_wrap(inputs[i]);
        fs2 b = _fs2_wrap_notnull(inputs[i]);
        CHECK_EQ("len",   a.len,   b.len);
        CHECK_EQ("sz",    a.sz,    b.sz);
        CHECK_EQ("flags", a.flags, b.flags);
        CHECK_PTR_EQ("v", a.v,     b.v);
        CHECK_FS2VALID((int)i, &a);
        CHECK_FS2VALID((int)i, &b);
    }
}

/* ══════════════════════════════════════════════════════════════════════════
 * _fs2_wrap + fs2_cat (usage pattern)
 * ══════════════════════════════════════════════════════════════════════════ */

Test(fs2, wrap_as_cat_src) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("prefix_");

    fs2 tmp = _fs2_wrap("suffix");
    test_cat(&dst, &tmp);

    CHECK_STR_EQ("prefixed+suffix", dst.v, "prefix_suffix");
    CHECK_FS2VALID(1, &dst);
    fs2_free(&dst);
    assert_balanced();
}

Test(fs2, wrap_notnull_as_cat_src) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("a");

    fs2 tmp = _fs2_wrap_notnull("bc");
    test_cat(&dst, &tmp);

    CHECK_STR_EQ("abc", dst.v, "abc");
    CHECK_FS2VALID(1, &dst);
    fs2_free(&dst);
    assert_balanced();
}

/* =========================================================================
 * fs2_orempty
 * ========================================================================= */

Test(fs2, orempty_null_returns_empty_const) {
    const fs2 *r = fs2_orempty(NULL);
    CHECK_PTR_EQ("&fs2_empty_const", r, &fs2_empty_const);
    CHECK_TRUE ("STATIC", fs2_is_static(r));
    CHECK_STR_EQ("v readable", r->v, "");
    CHECK_FS2VALID(1, r);
}

Test(fs2, orempty_nonnull_identity) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hi");
    CHECK_PTR_EQ("identity", fs2_orempty(&s), &s);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2, orempty_fs2ptr_implicit_convert) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hi");
    fs2 *p = &s;
    CHECK_PTR_EQ("fs2* works", fs2_orempty(p), p);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2, orempty_pointer_stable) {
    CHECK_PTR_EQ("stable across calls",
                 fs2_orempty(NULL), fs2_orempty(NULL));
}

/* =========================================================================
 * fs2_nvl
 * ========================================================================= */

Test(fs2, nvl_s_nonnull_returns_s) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hi");
    fs2 def = fs2_fromstr("default");

    CHECK_PTR_EQ("s wins", fs2_nvl(&s, &def), &s);

    fs2_free(&s);
    fs2_free(&def);
    assert_balanced();
}

Test(fs2, nvl_s_null_returns_def) {
    __fs2_checkreset();
    fs2 def = fs2_fromstr("default");

    CHECK_PTR_EQ("def returned", fs2_nvl(NULL, &def), &def);

    fs2_free(&def);
    assert_balanced();
}

Test(fs2, nvl_def_null_aborts, .signal = SIGABRT) {
    fs2_nvl(NULL, NULL);
}

Test(fs2, nvl_s_null_def_static) {
    fs2 def = FS2LITERAL("fallback");
    CHECK_PTR_EQ("static def works", fs2_nvl(NULL, &def), &def);
}

/* =========================================================================
 * Usage — NVL + fs2_cat
 * ========================================================================= */

Test(fs2, orempty_cat_null_noop) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("a");

    test_cat(&dst, fs2_orempty(NULL));   /* → empty_const → no-op */

    CHECK_STR_EQ("unchanged", dst.v, "a");
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    assert_balanced();
}

Test(fs2, nvl_cat_uses_default) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("a");
    fs2 def = fs2_fromstr(" (default)");

    test_cat(&dst, fs2_nvl(NULL, &def));

    CHECK_STR_EQ("appended default", dst.v, "a (default)");
    CHECK_FS2VALID(1, &dst);

    fs2_free(&dst);
    fs2_free(&def);
    assert_balanced();
}

Test(fs2, nvl_cat_s_wins) {
    __fs2_checkreset();
    fs2 dst = fs2_fromstr("a");
    fs2 s   = fs2_fromstr(" from-s");
    fs2 def = fs2_fromstr(" from-def");

    test_cat(&dst, fs2_nvl(&s, &def));

    CHECK_STR_EQ("s wins", dst.v, "a from-s");

    fs2_free(&dst);
    fs2_free(&s);
    fs2_free(&def);
    assert_balanced();
}

