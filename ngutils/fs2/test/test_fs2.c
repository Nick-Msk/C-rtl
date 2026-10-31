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
